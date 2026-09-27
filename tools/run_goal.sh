#!/bin/bash
# tools/run_goal.sh - the unattended loop. All state is on disk; nothing reads a chat.
#
# One item per `opencode run`, then tools/goal_check.sh decides pass/fail. The agent never commits:
# the script does, and only after the check passes. A change that fixes its target while
# regressing something else fails, because the check looks at the whole tree.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WT="${MP_GOAL_WT:-$REPO_ROOT/../wt-mp2-goal}"
BRANCH="goal/decomp"
GOAL="$WT/build/goal"   # beside the queue and the baseline, in the worktree
LOCK="$GOAL/run.lock"
LOG="$GOAL/run.log"
SUMMARY="$GOAL/summary.txt"
NOTES="$GOAL/notes"
AGENT_TIMEOUT="${MP_GOAL_AGENT_TIMEOUT:-60m}"
CHECK_TIMEOUT="${MP_GOAL_CHECK_TIMEOUT:-120m}"
MAX_CONSEC_FAIL="${MP_GOAL_MAX_CONSEC_FAIL:-10}"
BACKOFF_MAX="${MP_GOAL_BACKOFF_MAX:-1800}"
DISK_MIN_GB="${MP_GOAL_DISK_MIN_GB:-20}"
TMPDIR_MIN_GB="${MP_GOAL_TMP_MIN_GB:-6}"
FF_EVERY="${MP_GOAL_FF_EVERY:-10}"

export MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort}"
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$MP_TOOLCHAIN_DIR/build/review-tools}"
# **`opencode` lives in ~/.opencode/bin, which is NOT on systemd's default PATH.** Without this the
# unit ran `timeout opencode ...`, got 127 "No such file or directory" instantly on every item, and
# Restart=on-failure turned that into an infinite loop. Exported here as well as in the unit, so the
# script also works when run by hand from a shell with a minimal PATH.
export PATH="$HOME/.opencode/bin:$MP_TOOLCHAIN/bin:/usr/local/bin:/usr/bin:/bin:$PATH"
export TMPDIR="$WT/.tmp"          # the brief's own build area, never /tmp
mkdir -p "$GOAL" "$NOTES" "$TMPDIR" "$WT"

cd "$WT" || exit 2          # the tree being judged; the repo is only for commit/ff
Q() { python3 "$REPO_ROOT/tools/goal_queue.py" "$@"; }   # a function: always `Q sub ...`, never `$Q sub`

say() { echo "[$(date -u '+%F %T')Z] $*" | tee -a "$LOG"; }

# fatal <reason> - stop the loop rather than spin, and put the reason where the next reader looks.
# This exists because the loop's only output for a broken environment was an exit code, and
# Restart=on-failure turned that into an endless silent restart. A loop that cannot work must say
# so and stop; that is the whole difference between a failure and a spin.
fatal() {
  say "FATAL: $*"
  say "stopping - this is an environment fault, not a hard item. Fix it and start the service again."
  write_summary "${passes:-0}" "${fails:-0}" "${skipped:-0}" "STOPPED: $*"
  exit 4
}

# ----------------------------------------------------------------- lock
exec 9>"$LOCK" || exit 2
if ! flock -n 9; then
  say "another instance holds $LOCK - refusing to start"
  exit 3
fi
say "=== run_goal.sh starting; pid $$; worktree $WT; agent $AGENT_TIMEOUT check $CHECK_TIMEOUT ==="

# ----------------------------------------------------------------- helpers
free_gb() { df -BG --output=avail "$1" 2>/dev/null | tail -1 | tr -dc '0-9'; }

disk_ok() {
  local r t
  r=$(free_gb "$REPO_ROOT"); t=$(free_gb "$TMPDIR")
  [ -n "$r" ] && [ -n "$t" ] || return 0            # unreadable: do not deadlock the loop
  [ "$r" -ge "$DISK_MIN_GB" ] && [ "$t" -ge "$TMPDIR_MIN_GB" ]
}

reset_wt() {
  git -C "$WT" fetch -q origin 2>/dev/null
  git -C "$WT" checkout -q --force "$BRANCH" 2>/dev/null
  git -C "$WT" reset -q --hard "$BRANCH"
  git -C "$WT" clean -qfd -e orig -e build -e .tmp
}

write_summary() {
  local passed="$1" failed="$2" skipped="$3" reason="${4:-}"
  local m l ql last
  m=$(python3 -c 'import json;print(json.load(open("build/report.json"))["measures"]["matched_functions"])' 2>/dev/null || echo '?')
  l=$(python3 -c 'import json;r=json.load(open("build/report.json"));print(sum(u["measures"].get("matched_functions",0) for u in r["units"] if u.get("metadata",{}).get("complete")))' 2>/dev/null || echo '?')
  ql=$(Q list 2>/dev/null | tail -1)
  last=$(git -C "$REPO_ROOT" log -5 --format='    %h %s')
  cat >"$SUMMARY" <<EOF
mp2 goal summary - $(date -u '+%F %T')Z
=====================================
matched $m / 28465      linked $l
passed $passed   failed $failed   items reset/skipped $skipped
EOF
  [ -n "$reason" ] && echo "STOPPED: $reason" >>"$SUMMARY"
  cat >>"$SUMMARY" <<EOF

queue: $ql

last 5 commits on $BRANCH:
$last
EOF
  say "summary written: matched=$m linked=$l passed=$passed failed=$failed"
}

# ----------------------------------------------------------------- main loop
passes=0; fails=0; skipped=0; consec_fail=0; agent_errors=0; agent="worker"
item_n=0

# A summary from the first second, so `build/goal/summary.txt` always has a current
# first line rather than appearing only after 10 items.
write_summary 0 0 0

while :; do
  # --- disk guard, before anything expensive
  if ! disk_ok; then
    say "disk guard: pausing (repo ${DISK_MIN_GB}G, TMPDIR ${TMPDIR_MIN_GB}G thresholds) - recheck in 10 min"
    sleep 600; continue
  fi

  # --- anything left?
  if ! Q has-next >/dev/null 2>&1; then
    say "queue has nothing ready - stopping"
    write_summary "$passes" "$fails" "$skipped"
    break
  fi

  ITEM=$(Q next 2>/dev/null | tail -1)
  if [ -z "$ITEM" ]; then say "next returned nothing - stopping"; write_summary "$passes" "$fails" "$skipped"; break; fi
  ID=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])')
  KIND=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["kind"])')
  item_n=$((item_n+1))
  say "--- item $item_n: $ID ($KIND)"

  # --- reset to the branch head
  reset_wt

  # --- run the agent
  PROMPT="You are working one goal item. Read these, in this order, and follow them exactly:
1. $REPO_ROOT/docs/goal-unit-prompt.md - your instructions, the one rule, and the rules that have
   cost real time.
2. $REPO_ROOT/AGENTS.md - how the decomp works.
3. $WT/build/goal/item.json - the item itself.

Work only in $WT. Do not commit. Do not touch any other worktree. If you cannot finish, write
$NOTES/$ID.md and stop."
  printf '%s\n' "$ITEM" >"$GOAL/item.json"

  say "running agent '$agent' (timeout $AGENT_TIMEOUT)"
  # **`opencode run` has NO `--dir` flag** (`opencode run --help` lists --agent, --model, --format,
  # --file, --auto, ... and no directory option). The brief's command line included one, so every
  # item died in ~4 seconds on `Unrecognized flag: --dir` - the loop counted them as agent errors
  # and fell back to a name that was equally invalid. **The working directory is set by `cd`ing
  # into the worktree, which the subshell below already does.** Run it from the worktree, not from
  # the repo, or the agent edits the wrong tree.
  #
  # `--auto` is REQUIRED unattended: without it the first tool call aborts on a permission prompt.
  # The brief scopes `--auto` to this worktree and this is that worktree.
  T0=$(date +%s)
  ( cd "$WT" && timeout "$AGENT_TIMEOUT" opencode run --agent "$agent" --format json --auto \
      "$PROMPT" ) >>"$LOG" 2>&1
  ARC=$?
  T1=$(date +%s)
  ELAPSED=$((T1 - T0))

  if [ $ARC -ne 0 ]; then
    agent_errors=$((agent_errors+1))
    say "agent '$agent' exited $ARC after ${ELAPSED}s (agent_errors=$agent_errors)"

    # --- CIRCUIT BREAKER: an instant 127 is a broken environment, not a hard item.
    # `opencode` missing from PATH gives exactly this: 127, sub-second, every time. The old loop
    # treated it as a retryable agent error, so the same item was re-selected forever with
    # fails=0 and Restart=on-failure restarted the whole thing. **If the agent cannot even start,
    # no amount of retrying will help, and the honest move is to stop and say so.**
    if [ "$ARC" = 127 ] && [ "$ELAPSED" -lt 5 ]; then
      fatal "agent '$agent' cannot execute (exit 127 in ${ELAPSED}s) - opencode is not on PATH for this process"
    fi
    if [ "$ARC" = 127 ]; then
      fatal "agent '$agent' exited 127 - check the agent name against 'opencode run --help'"
    fi

    # An agent error still counts against the item. The 3-strikes rule has to cover agent
    # failures too, or an item the agent cannot even start on sits at fails=0 forever and the
    # loop re-selects it immediately - which is precisely the spin that happened.
    Q fail "$ID"
    consec_fail=$((consec_fail+1))
    if [ "$agent" = worker ] && [ "$agent_errors" -ge 3 ]; then
      say "falling back to --agent spacebunny (the same model, a fresh session) after 3 agent errors"
      agent=spacebunny; agent_errors=0
    elif [ "$agent" = spacebunny ] && [ "$agent_errors" -ge 3 ]; then
      say "spacebunny also failing 3x - back to worker, waiting an hour"
      agent=worker; agent_errors=0; sleep 3600
    fi
    if [ "$consec_fail" -ge "$MAX_CONSEC_FAIL" ]; then
      fatal "$consec_fail consecutive agent failures - stopping rather than spinning"
    fi
    say "resetting the worktree after an agent error"
    ( cd "$WT" && git reset -q --hard "$BRANCH" && git clean -qfd -e orig -e build -e .tmp )
    continue
  fi
  agent_errors=0

  # --- judge
  say "checking (timeout $CHECK_TIMEOUT)"
  ( cd "$WT" && timeout "$CHECK_TIMEOUT" "$REPO_ROOT/tools/goal_check.sh" "$GOAL/item.json" ) 2>&1 | tee -a "$LOG" | sed 's/^/    /'
  CRC=${PIPESTATUS[0]}

  if [ "$CRC" -eq 0 ]; then
    say "PASS $ID - committing"
    ( cd "$WT" && git add -A -- src include config docs tools ) || true
    KINDP=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["kind"])')
    MSG=$(git -C "$WT" diff --cached --stat | tail -1)
    ( cd "$WT" && git commit -q -m "$KINDP: $ID

Goal item $ID ($KIND). Target: $(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("target",""))')
Reason: $(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("reason",""))')

$MSG

Verified by tools/goal_check.sh: gate.sh against build/goal/report.base.json, matched and linked
not lower, check_symbol_names.py clean, and flip_test.sh kept the unit.
Co-Authored-By: opencode-go/space-bunny-free <no-reply@opencode.ai>" ) && say "committed $ID" || say "COMMIT FAILED $ID - not marking done"
    Q done "$ID"
    passes=$((passes+1)); consec_fail=0
    # refresh the baseline so the next item is measured against what we just landed
    ( cd "$WT" && ./tools/gate.sh --baseline >/dev/null 2>&1 ) && cp -f "$WT/$BASE" "$BASE" 2>/dev/null \
      || ( cd "$WT" && git add -f build/goal/report.base.json 2>/dev/null; git commit -q -m "goal: refresh the check baseline" 2>/dev/null )
    if [ $((passes % FF_EVERY)) -eq 0 ] && [ -z "$(git -C "$REPO_ROOT" status --porcelain --untracked-files=no)" ]; then
      if git -C "$REPO_ROOT" merge --ff-only "$BRANCH" >/dev/null 2>&1; then
        say "fast-forwarded master to $BRANCH"
      else
        say "skipping the fast-forward: master is not clean or not a fast-forward - noted in the summary"
      fi
    fi
    if [ $((passes % 10)) -eq 0 ]; then write_summary "$passes" "$fails" "$skipped"; fi
  else
    say "FAIL $ID (check exit $CRC) - resetting and recording"
    ( cd "$WT" && git reset -q --hard "$BRANCH" && git clean -qfd -e orig -e build -e .tmp )
    Q fail "$ID"
    fails=$((fails+1)); consec_fail=$((consec_fail+1))
    if [ "$consec_fail" -ge "$MAX_CONSEC_FAIL" ]; then
      say "$consec_fail consecutive failures across both models - stopping"
      write_summary "$passes" "$fails" "$skipped"
      break
    fi
    BACK=$((60 * consec_fail)); [ "$BACK" -gt "$BACKOFF_MAX" ] && BACK=$BACKOFF_MAX
    say "backing off ${BACK}s"
    sleep "$BACK"
  fi
done

say "=== run_goal.sh finished: passed=$passes failed=$fails ==="
exit 0
