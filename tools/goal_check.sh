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
[ "$ITEM" -ef "$LOGDIR/item.json" ] || cp "$ITEM" "$LOGDIR/item.json"

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
# The judge's baselines live in build/goal/judge/, untracked: the report, the port's undefined
# count and its undefined-symbol list, all recorded by run_goal.sh on the branch head.
JUDGE="${MP_GOAL_JUDGE:-${MP_GOAL_TREE:-$REPO_ROOT}/build/goal/judge}"
if [ -z "${MP_GOAL_BASE:-}" ]; then
  MP_GOAL_BASE="$JUDGE/report.base.json"
fi
BASE="$MP_GOAL_BASE"
if [ ! -f "$BASE" ]; then
  echo "goal_check: no baseline at $BASE - the brief's default is to stop the item, record a" >&2
  echo "             baseline on a clean HEAD once with tools/gate.sh --baseline, and retry." >&2
  exit 2
fi
echo "goal_check: baseline $BASE"

# ------------------------------------------------- 0. what the change touched
# **The agent must not be able to move the goalposts.** It runs with --auto in this tree, so
# nothing stops it re-recording a baseline (`link_check.sh --record` rewrites
# docs/research/port_link_baseline.txt) or editing the judge's own tools. Either would turn a
# regression into a pass, so a change that touches them fails outright, whatever else it did.
CHANGED=$(git status --porcelain --untracked-files=all | cut -c4- | sed 's/.* -> //')
BAD=$(printf '%s\n' "$CHANGED" | grep -E '^(tools/|docs/research/port_link_baseline\.txt$|build/goal/)' || true)
if [ -n "$BAD" ]; then
  note "the change touches paths the agent may not edit"
  printf '%s\n' "$BAD" | head -6 | sed 's/^/        /'
else
  ok "no judge-owned path touched"
fi
CODE_CHANGED=$(printf '%s\n' "$CHANGED" | grep -cE '^(src|include)/' || true)

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
    # **What "done" means for a port item has to be something that can fail.**
    #
    # The old check was "the target is no longer in the undefined list". For an inline target
    # (`CInputStream::ReadInt32`) that is true before any work at all - the linker never asks for
    # an inline - and it read `build-port-link/build.log` *before* `link_check.sh` rebuilt it, so
    # it judged the previous tree's link. Both are fixed here:
    #
    #   - the link runs first, so the list is this tree's;
    #   - a target counts as resolved only if it WAS in the baseline list (recorded on the
    #     branch head) and is gone now - otherwise "gone" proves nothing;
    #   - a target the linker never asked for needs the item's `verify` script, a real test
    #     under tools/goal_verify/ that the orchestrator wrote and the agent cannot edit;
    #   - either way, the change must touch src/ or include/.
    VERIFY=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("verify",""))' "$ITEM")
    [ "$CODE_CHANGED" -gt 0 ] && ok "$CODE_CHANGED path(s) changed under src/ or include/" \
                              || note "port item with no change under src/ or include/"

    ./tools/link_check.sh >"$LOGDIR/check-link.log" 2>&1
    # A port that does not compile reports "unique undefined symbols 0": the linker never ran,
    # and link_check.sh says so. Read that line, or a broken build scores as a perfect link.
    if grep -qE "LINKER NEVER RAN|compile errors [1-9]" "$LOGDIR/check-link.log"; then
      note "the port did not build - link_check's counts are vacuous"
      grep -m3 -E "error:" "$LOGDIR/check-link.log" | sed 's/^/        /'
      UNDEF_NOW=""
    else
      UNDEF_NOW=$(sed -n 's/.*unique undefined symbols \([0-9]*\).*/\1/p' "$LOGDIR/check-link.log" | head -1)
    fi
    UNDEF_LIST="$LOGDIR/undef_by_obj.txt"
    rm -f "$UNDEF_LIST"
    [ -n "$UNDEF_NOW" ] && MP_UNDEF_LIST="$UNDEF_LIST" python3 tools/link_undef_refs.py >"$LOGDIR/check-undef.log" 2>&1
    UNDEF_BASE_LIST="$JUDGE/undef.base.txt"
    UNDEF_BASE=$(cat "$JUDGE/undef.base.count" 2>/dev/null)
    judged=0; lists=0
    if [ -z "$TARGET" ]; then
      note "port item with no target symbol"
    elif [ ! -s "$UNDEF_LIST" ] || [ ! -s "$UNDEF_BASE_LIST" ]; then
      note "could not read the port's undefined-symbol list (now: $UNDEF_LIST, base: $UNDEF_BASE_LIST)"
    elif lists=1 && cut -f1 "$UNDEF_BASE_LIST" | grep -qF -- "$TARGET"; then
      judged=1
      if cut -f1 "$UNDEF_LIST" | grep -qF -- "$TARGET"; then
        note "$TARGET is still undefined"
        cut -f1 "$UNDEF_LIST" | grep -F -- "$TARGET" | head -2 | sed 's/^/        /'
      else
        ok "$TARGET was undefined at the branch head and is not now"
      fi
    fi
    if [ -n "$VERIFY" ]; then
      judged=1
      if [ ! -x "$REPO_ROOT/tools/goal_verify/$VERIFY" ]; then
        note "verify script tools/goal_verify/$VERIFY is missing"
      elif timeout -k 10 600 "$REPO_ROOT/tools/goal_verify/$VERIFY" >"$LOGDIR/check-verify.log" 2>&1; then
        ok "verify $VERIFY: $(tail -1 "$LOGDIR/check-verify.log")"
      else
        note "verify $VERIFY failed"
        tail -6 "$LOGDIR/check-verify.log" | sed 's/^/        /'
      fi
    fi
    if [ "$judged" -eq 0 ] && [ "$lists" -eq 1 ]; then
      note "unjudgeable: $TARGET was never undefined and the item has no verify script"
    fi
    if [ -n "$UNDEF_NOW" ] && [ -n "$UNDEF_BASE" ]; then
      if [ "$UNDEF_NOW" -le "$UNDEF_BASE" ]; then
        ok "port undefined $UNDEF_BASE -> $UNDEF_NOW"
      else
        note "port undefined rose $UNDEF_BASE -> $UNDEF_NOW"
      fi
    else
      [ -z "$UNDEF_NOW" ] && [ -n "$UNDEF_BASE" ] \
        || note "could not read the port's undefined count (now '$UNDEF_NOW', base '$UNDEF_BASE')"
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
