#!/bin/bash
# tools/goal_check.sh <item.json> - the ONLY judge. Exit 0 means the item passed.
#
# The agent does not decide whether its own work counted; this does, from the tree. Nothing here
# is relaxed, and a *new* failure anywhere fails the item even if the target improved - a change
# that fixes its target while regressing something else is a regression.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${MP_GOAL_TREE:-$REPO_ROOT}" || exit 2   # MP_GOAL_TREE: run against the worktree, not this repo
export MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$MP_TOOLCHAIN_DIR/build/review-tools}"
export PATH="$MP_TOOLCHAIN/bin:$PATH"
LOGDIR="${MP_GOAL_LOGDIR:-${MP_GOAL_TREE:-$REPO_ROOT}/build/goal}"
mkdir -p "$LOGDIR"

ITEM="${1:-}"
[ -n "$ITEM" ] || { echo "goal_check: usage: goal_check.sh <item.json>" >&2; exit 2; }
[ -f "$ITEM" ] || { echo "goal_check: no such item file: $ITEM" >&2; exit 2; }
cp "$ITEM" "$LOGDIR/item.json"

ID=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$ITEM")
KIND=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["kind"])' "$ITEM")
TARGET=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("target",""))' "$ITEM")
echo "goal_check: item $ID ($KIND) target=$TARGET"

fail=()
note() { echo "  FAIL  $*"; fail+=("$1"); }
ok()   { echo "  ok    $*"; }

# ---------------------------------------------------------------- baseline
# The baseline is a *path* the caller owns, because the tree being checked is the worktree
# ../wt-mp2-goal while this script lives in the repo. Default to the worktree's copy so a bare
# `goal_check.sh item.json` works; run_goal.sh exports MP_GOAL_BASE explicitly.
if [ -z "${MP_GOAL_BASE:-}" ]; then
  MP_GOAL_BASE="${MP_GOAL_TREE:-$REPO_ROOT}/build/goal/report.base.json"
fi
BASE="$MP_GOAL_BASE"
if [ ! -f "$BASE" ]; then
  echo "goal_check: no baseline at $BASE - the brief's default is to stop the item, record a" >&2
  echo "             baseline on a clean HEAD once with tools/gate.sh --baseline, and retry." >&2
  exit 2
fi
echo "goal_check: baseline $BASE"

# ---------------------------------------------------------------- 1. the gate
if ./tools/gate.sh "$BASE" >"$LOGDIR/check-gate.log" 2>&1; then
  ok "gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)"
else
  note "gate.sh"
  grep -E "FAIL|stale:|missing:|GONE|WORSE|UNLINKED|FELL" "$LOGDIR/check-gate.log" | head -12 | sed 's/^/        /'
fi

# ------------------------------------------------- 2. matched/linked vs baseline
# Read with the same definition the docs use: `linked` counts functions in units that are
# `complete`, which is objdiff's word for "Matching and really in the link".
python3 - "$BASE" <<'PY' >"$LOGDIR/check-counts.log" 2>&1
import json, sys
b = json.load(open(sys.argv[1])); c = json.load(open("build/report.json"))
def linked(r):
    return sum(u["measures"].get("matched_functions", 0) for u in r["units"]
               if u.get("metadata", {}).get("complete"))
bm, cm = b["measures"]["matched_functions"], c["measures"]["matched_functions"]
bl, cl = linked(b), linked(c)
print(f"matched {bm} -> {cm}   linked {bl} -> {cl}")
sys.exit(0 if (cm >= bm and cl >= bl) else 1)
PY
if [ $? -eq 0 ]; then
  ok "counts: $(cat "$LOGDIR/check-counts.log")"
else
  note "matched/linked fell: $(cat "$LOGDIR/check-counts.log")"
fi

# ---------------------------------------------------------------- 3. symbol names
if python3 tools/check_symbol_names.py >"$LOGDIR/check-names.log" 2>&1; then
  ok "check_symbol_names.py"
else
  note "check_symbol_names.py"; tail -4 "$LOGDIR/check-names.log" | sed 's/^/        /'
fi

# ---------------------------------------------------------------- 4. decomp_build All:
# The `All:` line must not fall. Quoted, not parsed out of a progress bar.
./tools/decomp_build.sh >"$LOGDIR/check-build.log" 2>&1
ALL=$(grep -oE "^All: .*" "$LOGDIR/check-build.log" | tail -1)
BASE_ALL=$(grep -oE "^All: .*" "$LOGDIR/check-gate.log" | tail -1)
if [ -z "$ALL" ]; then
  note "decomp_build.sh printed no All: line"
else
  ok "$ALL"
  [ -n "$BASE_ALL" ] && echo "        (gate's run: $BASE_ALL)"
fi

# ---------------------------------------------------------------- 5. per-kind extra
case "$KIND" in
  match)
    if [ -z "$TARGET" ]; then
      note "match item with no target unit"
    else
      # **`flip_test`'s EXIT STATUS IS NOT THE VERDICT.** Measured: it prints `SKIP <unit> - not
      # listed in configure.py`, then `kept: 0/1 failed: 0 skipped: 1`, and **exits 1.** So an
      # already-`Matching` unit - the most common possible state for a `match` item - looks exactly
      # like a failure if you trust `$?`. The verdict is the word on its own line: `PASS` means it
      # was kept as Matching, `SKIP` means there was nothing to promote, and only `FAIL` is a
      # failure. Judging on the exit code would make every already-Matching unit un-promotable.
      ./tools/flip_test.sh "$TARGET" >"$LOGDIR/check-flip.log" 2>&1
      VERDICT=$(grep -oE '^[[:space:]]*(PASS|SKIP|FAIL)' "$LOGDIR/check-flip.log" | tail -1 | tr -d '[:space:]')
      case "$VERDICT" in
        PASS) ok "flip_test $TARGET: PASS (kept as Matching)" ;;
        SKIP) ok "flip_test $TARGET: SKIP (already Matching / not in configure.py - nothing to promote)" ;;
        "")   note "flip_test $TARGET: no verdict line"
               tail -4 "$LOGDIR/check-flip.log" | sed 's/^/        /' ;;
        *)    note "flip_test $TARGET: $VERDICT"
               grep -iE "FAIL|undefined|reverted" "$LOGDIR/check-flip.log" | head -6 | sed 's/^/        /' ;;
      esac
    fi
    ;;
  port)
    # The target symbol must be gone from the port's undefined set.
    #
    # **`link_undef_refs.py` prints a SUMMARY to stdout, not the per-symbol list.** It writes the
    # actual names to `undef_by_obj.txt` (currently hardcoded to /tmp/opencode, which the brief's
    # own rules forbid for anything the loop runs - so the judge treats a missing file as a FAIL,
    # never as "the symbol is gone"). Grepping stdout would find nothing and report every port
    # target as resolved, which is a check that cannot fail.
    UNDEF_LIST="$LOGDIR/undef_by_obj.txt"
    MP_UNDEF_LIST="$UNDEF_LIST" python3 tools/link_undef_refs.py >"$LOGDIR/check-undef.log" 2>&1
    if [ ! -s "$UNDEF_LIST" ]; then
      note "could not read the port's undefined-symbol list ($UNDEF_LIST missing or empty)"
    elif [ -z "$TARGET" ]; then
      note "port item with no target symbol"
    elif grep -qF "$TARGET" "$UNDEF_LIST"; then
      note "$TARGET is still undefined"
      grep -F "$TARGET" "$UNDEF_LIST" | head -2 | cut -f1 | sed 's/^/        /'
    else
      ok "$TARGET no longer undefined"
    fi
    UNDEF_NOW=$(./tools/link_check.sh 2>/dev/null | sed -n 's/.*unique undefined symbols \([0-9]*\).*/\1/p' | head -1)
    UNDEF_BASE=$(grep -oE "^undefined [0-9]+" docs/research/port_link_baseline.txt 2>/dev/null | awk '{print $2}' | head -1)
    if [ -n "$UNDEF_NOW" ] && [ -n "$UNDEF_BASE" ]; then
      if [ "$UNDEF_NOW" -le "$UNDEF_BASE" ]; then
        ok "port undefined $UNDEF_BASE -> $UNDEF_NOW"
      else
        note "port undefined rose $UNDEF_BASE -> $UNDEF_NOW"
      fi
    else
      note "could not read the port's undefined count"
    fi
    PROBE=$(./tools/probe_sources.sh 2>&1 | tail -1)
    case "$PROBE" in
      *"0 failed"*) ok "probe: ${PROBE%% (logs*}" ;;
      *) note "probe: $PROBE" ;;
    esac
    ;;
  *)
    note "unknown kind: $KIND"
    ;;
esac

# ---------------------------------------------------------------- verdict
if [ ${#fail[@]} -eq 0 ]; then
  echo "goal_check: PASS $ID"
  exit 0
fi
echo "goal_check: FAIL $ID - ${#fail[@]} failing check(s): ${fail[*]}"
exit 1
