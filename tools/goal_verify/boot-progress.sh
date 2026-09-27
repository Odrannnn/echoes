#!/bin/bash
# tools/goal_verify/boot-progress.sh [--record <out.json>] - the general judge for boot blockers.
#
# Passes a port item only if the boot gets further than it does at the branch head, in every run.
# "Further" is decided by boot_progress.py from where gdb finds the main thread when the boot stops
# (a fault or a hang). run_goal.sh records the head's position with --record before the agent
# runs, into build/goal/judge/boot.base.json. This script, run by goal_check.sh with the agent's
# change in the tree, compares against it. Run from the worktree root.
#
# It was measured both ways before use: see docs/RUNNING_THE_DECOMP.md.
#
# What it cannot see: a boot that gets further by skipping the code that crashed. Early returns
# and no-op bodies move the stop point too. The reviewer is told to reject those
# (docs/goal-review-prompt.md), which is why port items are reviewed.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE=build/goal/judge/boot.base.json
MODE=verify
if [ "${1:-}" = --record ]; then MODE=record; OUT=${2:?--record needs an output path}; fi
export MP_BOOT_RUNS="${MP_BOOT_RUNS:-2}" MP_BOOT_HANG_SECS="${MP_BOOT_HANG_SECS:-45}"
export MP_BOOT_SAMPLES="${MP_BOOT_SAMPLES:-5}"
export MP_PROBE_RUNNER="$HERE/boot_gdb_run.sh"
export MP_PROBE_TIMEOUT=$(( MP_BOOT_RUNS * (MP_BOOT_HANG_SECS + MP_BOOT_SAMPLES + 60) + 30 ))

if [ "$MODE" = verify ]; then
  [ -s "$BASE" ] || { echo "verify: no boot baseline at $BASE - BOOT_PROGRESS FAIL"; exit 1; }
  # The markers are half of the measure, so the change may not add, move or reword them. Comment
  # lines may quote them (the pak-pump fix does); code before a trailing // still counts.
  MARK='boot: step|Initializing renderer'
  code_marks() { sed -E 's#//.*##' | grep -vE '^[[:space:]]*(/\*|\*)' | grep -qE "$MARK"; }
  if git diff -U0 HEAD -- . ':(exclude)build*' | grep -vE '^(\+\+\+|---) ' | grep -E '^[+-]' \
       | cut -c2- | code_marks; then
    echo "verify: the change edits a boot marker line - the judge measures with those - BOOT_PROGRESS FAIL"; exit 1
  fi
  while IFS= read -r -d '' f; do
    if code_marks <"$f"; then echo "verify: a new file ($f) prints a boot marker - BOOT_PROGRESS FAIL"; exit 1; fi
  done < <(git ls-files -o --exclude-standard -z -- src include platform extern)
fi

STUBS=src/MetroidPrime/PortReachStubs.cpp
SAVE="${TMPDIR:-/tmp}/boot-progress-stubs.$$"
cp -p "$STUBS" "$SAVE" || { echo "verify: cannot save $STUBS - BOOT_PROGRESS FAIL"; exit 1; }
trap 'cp -p "$SAVE" "$STUBS"; rm -f "$SAVE"' EXIT
trap 'exit 143' TERM INT HUP  # goal_check's timeout: still restore the stubs

LOG=build-boot-probe/run.log
rm -f "$LOG"
if ! ./tools/boot_probe.sh >"${TMPDIR:-/tmp}/boot-progress-probe.$$.log" 2>&1; then
  tail -15 "${TMPDIR:-/tmp}/boot-progress-probe.$$.log"
  echo "verify: boot_probe.sh failed (the port did not build or link) - BOOT_PROGRESS FAIL"; exit 1
fi
rm -f "${TMPDIR:-/tmp}/boot-progress-probe.$$.log"
[ -s "$LOG" ] || { echo "verify: boot_probe.sh left no run log - BOOT_PROGRESS FAIL"; exit 1; }

if [ "$MODE" = record ]; then
  python3 "$HERE/boot_progress.py" record "$LOG" "$OUT"
else
  python3 "$HERE/boot_progress.py" verify "$LOG" "$BASE"
fi
