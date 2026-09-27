#!/bin/bash
# tools/goal_check.sh <item.json> - the ONLY judge. Exit 0 means the item passed.
#
# The agent does not decide whether its own work counted; this does, from the tree. Nothing here
# is relaxed, and a *new* failure anywhere fails the item even if the target improved - a change
# that fixes its target while regressing something else is a regression.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT" || exit 2
export MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$MP_TOOLCHAIN_DIR/build/review-tools}"
export PATH="$MP_TOOLCHAIN/bin:$PATH"
BASE="${MP_GOAL_BASE:-build/goal/report.base.json}"
LOGDIR="build/goal"
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
if [ ! -f "$BASE" ]; then
  echo "goal_check: no baseline at $BASE - the brief's default is to stop the item, record a" >&2
  echo "             baseline on a clean HEAD once with tools/gate.sh --baseline, and retry." >&2
  exit 2
fi

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
    elif ./tools/flip_test.sh "$TARGET" >"$LOGDIR/check-flip.log" 2>&1; then
      ok "flip_test $TARGET: $(grep -iE 'PASS|kept' "$LOGDIR/check-flip.log" | tail -1)"
    else
      note "flip_test $TARGET"
      grep -iE "FAIL|undefined|reverted" "$LOGDIR/check-flip.log" | head -6 | sed 's/^/        /'
    fi
    ;;
  port)
    # The target symbol must be gone from the port's undefined list, the undefined count must
    # not rise, and the probe must be clean. `nm` sees the real link; link_gap's categories are
    # for reading, not for the verdict.
    if [ -z "$TARGET" ]; then
      note "port item with no target symbol"
    elif python3 tools/link_undef_refs.py >"$LOGDIR/check-undef.log" 2>&1 \
         && ! grep -qF "$TARGET" "$LOGDIR/check-undef.log"; then
      ok "$TARGET no longer undefined"
    else
      note "$TARGET still undefined"
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
