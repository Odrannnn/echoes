#!/bin/bash
# tools/run_goal.sh - the unattended loop. All state is on disk; nothing reads a chat.
#
# One item per `opencode run`, then tools/goal_check.sh decides pass/fail. The agent never commits:
# the script does, and only after the check passes. A change that fixes its target while
# regressing something else fails, because the check looks at the whole tree. A change the check
# passes then goes to a reviewer agent on a different model (docs/goal-review-prompt.md), which can
# veto the commit but never overrule a failed check: the judge measures, the reviewer reads.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# TIP is the branch the loop advances. Single mode (no MP_GOAL_LANE) works on it directly in
# ../wt-mp2-goal. Lane mode (MP_GOAL_LANE=k, several loops at once, tools/goal_lanes.sh) works in
# ../wt-mp2-goal-L<k> on goal/lane-<k>, which is reset to TIP before every item; a passed change is
# rebased onto TIP, re-judged if TIP moved, committed and published onto TIP by a compare-and-swap
# `update-ref`, all under one flock (publish.lock) - so TIP only ever gains judged commits, one at
# a time. The queue, notes, agent transcripts and review patches are shared (SHARED); the lock,
# log, judge baselines and item.json are per worktree.
TIP="goal/decomp"
LANE="${MP_GOAL_LANE:-}"
case "$LANE" in ''|[1-9]|[1-9][0-9]) ;; *) echo "run_goal: MP_GOAL_LANE must be a small number, not '$LANE'" >&2; exit 2 ;; esac
if [ -n "$LANE" ]; then
  WT="${MP_GOAL_WT:-$REPO_ROOT/../wt-mp2-goal-L$LANE}"
  BRANCH="goal/lane-$LANE"
  SHARED="${MP_GOAL_SHARED:-$REPO_ROOT/../wt-mp2-goal/build/goal}"
  TAG="L$LANE-"
else
  WT="${MP_GOAL_WT:-$REPO_ROOT/../wt-mp2-goal}"
  BRANCH="$TIP"
  SHARED="$WT/build/goal"
  TAG=""
fi
GOAL="$WT/build/goal"   # per worktree: lock, log, judge, item.json, summary
LOCK="$GOAL/run.lock"
LOG="$GOAL/run.log"
SUMMARY="$GOAL/summary.txt"
NOTES="$SHARED/notes"
JUDGE="$GOAL/judge"     # the judge's baselines: report, port undefined count and list
AGENTLOG="$SHARED/agent"  # one JSON transcript per agent run, not interleaved into run.log
PUBLISH_LOCK="$SHARED/publish.lock"
MODE_LOCK="$SHARED/mode.lock"   # lanes hold it shared, the single loop exclusive: never both
export MP_GOAL_QUEUE_DIR="$SHARED"
AGENT_TIMEOUT="${MP_GOAL_AGENT_TIMEOUT:-60m}"
CHECK_TIMEOUT="${MP_GOAL_CHECK_TIMEOUT:-120m}"
MAX_CONSEC_FAIL="${MP_GOAL_MAX_CONSEC_FAIL:-10}"
BACKOFF_MAX="${MP_GOAL_BACKOFF_MAX:-1800}"
DISK_MIN_GB="${MP_GOAL_DISK_MIN_GB:-20}"
TMPDIR_MIN_GB="${MP_GOAL_TMP_MIN_GB:-6}"
FF_EVERY="${MP_GOAL_FF_EVERY:-10}"
# The reviewer: a second agent, in a fresh session, that reads the staged diff after the judge
# passes it and can veto the commit. It is `worker` (space-bunny at reasoning max) by the user's
# choice (2026-09-28), not the local `ornith` lane. It cannot overrule the judge - it only ever
# runs on a change the judge already passed.
REVIEWER="${MP_GOAL_REVIEWER:-worker}"
REVIEW_TIMEOUT="${MP_GOAL_REVIEW_TIMEOUT:-30m}"
REVIEW_TRIES="${MP_GOAL_REVIEW_TRIES:-3}"            # a failed review run is retried, not read as a veto
REVIEW_RETRY_WAIT="${MP_GOAL_REVIEW_RETRY_WAIT:-300}"
REVIEW_MAX_BYTES="${MP_GOAL_REVIEW_MAX_BYTES:-200000}"
MAX_NO_VERDICT="${MP_GOAL_MAX_NO_VERDICT:-3}"          # consecutive items with no verdict -> stop
# Kinds the reviewer reads. A match item is decided by the checks alone: flip_test and the sha1s
# prove the bytes. A port item's checks can pass on an empty stub, so a reader is still needed.
REVIEW_KINDS="${MP_GOAL_REVIEW_KINDS:-port}"
REVIEWDIR="$SHARED/review"  # the exact patch each review saw, kept for the reader
# 1: when no boot-progress item is queued or in review and the branch head moved, boot the head
# and queue where it stops, at the front, judged by tools/goal_verify/boot-progress.sh.
# Lane mode: lane 1 only, or every lane would boot the same head and queue the same blocker.
BOOT_BLOCKERS="${MP_GOAL_BOOT_BLOCKERS:-$( [ -z "$LANE" ] || [ "$LANE" = 1 ] && echo 1 || echo 0)}"
BOOT_VERIFY=boot-progress.sh
BOOT_HEAD="" BOOT_SUM="" BOOT_SCAN_HEAD=""

export MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort}"
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$MP_TOOLCHAIN_DIR/build/review-tools}"
# **`opencode` lives in ~/.opencode/bin, which is NOT on systemd's default PATH.** Without this the
# unit ran `timeout opencode ...`, got 127 "No such file or directory" instantly on every item, and
# Restart=on-failure turned that into an infinite loop. Exported here as well as in the unit, so the
# script also works when run by hand from a shell with a minimal PATH.
export PATH="$HOME/.opencode/bin:$MP_TOOLCHAIN/bin:/usr/local/bin:/usr/bin:/bin:$PATH"
export TMPDIR="$WT/.tmp"          # the brief's own build area, never /tmp
mkdir -p "$GOAL" "$SHARED" "$NOTES" "$JUDGE" "$AGENTLOG" "$REVIEWDIR" "$TMPDIR" "$WT"
# goal_check.sh lives in the repo and judges the worktree against the judge's baselines. Without
# these it cd'd into the repo and judged master - every "PASS" before this was a check of a tree
# the agent had not touched.
export MP_GOAL_TREE="$WT" MP_GOAL_JUDGE="$JUDGE" MP_GOAL_BASE="$JUDGE/report.base.json"

cd "$WT" || exit 2          # the tree being judged; the repo is only for commit/ff
Q() { python3 "$REPO_ROOT/tools/goal_queue.py" "$@"; }   # a function: always `Q sub ...`, never `$Q sub`
LANEARG=(); [ -n "$LANE" ] && LANEARG=(--lane "$LANE")
# The agent command. Overridable only so the self-test can drive the loop with a scripted agent
# (a good change, a bad change, an agent error) without spending a model run; the unit never sets it.
OPENCODE="${MP_GOAL_OPENCODE:-opencode}"
REVIEW_OPENCODE="${MP_GOAL_REVIEW_OPENCODE:-$OPENCODE}"
# **`opencode run` ignores an agent's `model:` line** and falls back to opencode.json's top-level
# model. Until 2026-09-28 every loop session - worker and reviewer alike - ran on
# opencode-go/mimo-v2.6-flash (read back from session_message in opencode.db), not the model the
# agent file names. So each run passes `-m` explicitly, and an agent with no entry here is refused.
model_for() {
  case "$1" in
    worker|spacebunny) echo "${MP_GOAL_MODEL:-opencode-go/space-bunny-free#max}" ;;
    ornith)            echo "lmstudio/ornith-worker" ;;
    *)                 return 1 ;;
  esac
}
model_for "$REVIEWER" >/dev/null || { echo "run_goal: no model known for reviewer '$REVIEWER'" >&2; exit 2; }

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
exec 7>"$MODE_LOCK" || exit 2
if ! flock -n $( [ -n "$LANE" ] && echo -s || echo -x ) 7; then
  say "the $( [ -n "$LANE" ] && echo single loop || echo lanes ) hold $MODE_LOCK - lanes and the single loop never run together; refusing to start"
  exit 3
fi
if [ -n "$LANE" ]; then
  # A worktree with TIP checked out would see every publish as its own tree going dirty.
  if git -C "$REPO_ROOT" worktree list --porcelain | grep -qx "branch refs/heads/$TIP"; then
    say "FATAL: $TIP is checked out in a worktree - detach it (tools/goal_lanes.sh setup does) before starting lanes"
    exit 4
  fi
  Q release-lane "$LANE" | tee -a "$LOG"   # claims a killed run of this lane left behind
fi
say "=== run_goal.sh starting; pid $$; ${LANE:+lane $LANE; }worktree $WT; agent $AGENT_TIMEOUT check $CHECK_TIMEOUT ==="

# ----------------------------------------------------------------- helpers
free_gb() { df -BG --output=avail "$1" 2>/dev/null | tail -1 | tr -dc '0-9'; }

disk_ok() {
  local r t
  r=$(free_gb "$REPO_ROOT"); t=$(free_gb "$TMPDIR")
  [ -n "$r" ] && [ -n "$t" ] || return 0            # unreadable: do not deadlock the loop
  [ "$r" -ge "$DISK_MIN_GB" ] && [ "$t" -ge "$TMPDIR_MIN_GB" ]
}

# reset_wt - a clean tree at the branch head; in lane mode the lane branch is first moved to TIP.
# BASE is the commit the item is worked against: every later reset goes back to it, not to a
# branch name another lane may have moved meanwhile.
reset_wt() {
  git -C "$WT" fetch -q origin 2>/dev/null
  if [ -n "$LANE" ]; then
    git -C "$WT" checkout -q --force -B "$BRANCH" "$TIP"
  else
    git -C "$WT" checkout -q --force "$BRANCH" 2>/dev/null
    git -C "$WT" reset -q --hard "$BRANCH"
  fi
  git -C "$WT" clean -qfd -e orig -e build -e .tmp
  BASE=$(git -C "$WT" rev-parse HEAD)
}

# clean_wt - back to BASE, dropping the attempt.
clean_wt() {
  ( cd "$WT" && git reset -q --hard "$BASE" && git clean -qfd -e orig -e build -e .tmp )
}

# publish <old> - lane mode: move TIP from <old> to this worktree's HEAD, only if TIP is still
# <old>. The caller holds PUBLISH_LOCK, so a failure means someone moved TIP by hand.
publish() {
  git -C "$REPO_ROOT" update-ref -m "goal: lane $LANE" "refs/heads/$TIP" "$(git -C "$WT" rev-parse HEAD)" "$1"
}

# record_judge - the baselines goal_check.sh measures against, taken on the branch head.
# Recorded here, by the script, into an untracked directory, with a checksum: the agent runs with
# --auto in this tree, and a baseline it could re-record is a check it could pass by editing.
record_judge() {
  local head; head=$(git -C "$WT" rev-parse HEAD)
  if [ "$(cat "$JUDGE/HEAD" 2>/dev/null)" = "$head" ] && ( cd "$JUDGE" && sha256sum --status -c sums ) 2>/dev/null; then
    return 0
  fi
  say "recording the judge's baselines at $(git -C "$WT" rev-parse --short HEAD)"
  ( cd "$WT" && ./tools/gate.sh --baseline >"$JUDGE/record-gate.log" 2>&1 ) \
    && cp -f "$WT/build/report.base.json" "$JUDGE/report.base.json" \
    || { say "gate.sh --baseline failed on the branch head - see $JUDGE/record-gate.log"; return 1; }
  ( cd "$WT" && ./tools/link_check.sh >"$JUDGE/record-link.log" 2>&1 )
  sed -n 's/.*unique undefined symbols \([0-9]*\).*/\1/p' "$JUDGE/record-link.log" | head -1 >"$JUDGE/undef.base.count"
  ( cd "$WT" && MP_UNDEF_LIST="$JUDGE/undef.base.txt" python3 tools/link_undef_refs.py >/dev/null 2>&1 )
  [ -s "$JUDGE/undef.base.count" ] && [ -s "$JUDGE/undef.base.txt" ] \
    || { say "could not record the port's undefined baseline - see $JUDGE/record-link.log"; return 1; }
  ( cd "$JUDGE" && sha256sum report.base.json undef.base.count undef.base.txt >sums )
  echo "$head" >"$JUDGE/HEAD"
  say "judge baselines: $(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["measures"]["matched_functions"])' "$JUDGE/report.base.json") matched, $(cat "$JUDGE/undef.base.count") port undefined"
}

# record_boot - the boot baseline boot-progress.sh measures against: where the head's boot stops.
# Several minutes (a port build and two boots), so only for boot-progress items and the blocker
# scan, and only once per head. The checksum lives in this process, not in the worktree, because
# the agent can write anything under build/.
record_boot() {
  local head; head=$(git -C "$WT" rev-parse HEAD)
  if [ "$BOOT_HEAD" = "$head" ] && [ "$(sha256sum <"$JUDGE/boot.base.json" 2>/dev/null)" = "$BOOT_SUM" ]; then
    return 0
  fi
  BOOT_HEAD=""
  say "recording the boot baseline at ${head:0:7} ($BOOT_VERIFY --record)"
  rm -f "$JUDGE/boot.base.json"
  ( cd "$WT" && timeout -k 10 1200 "$REPO_ROOT/tools/goal_verify/$BOOT_VERIFY" --record "$JUDGE/boot.base.json" ) \
    >"$JUDGE/record-boot.log" 2>&1
  local rc=$?
  reset_wt   # boot_probe.sh writes reach stubs into src/; the script restores them, this makes sure
  grep '^head run: ' "$JUDGE/record-boot.log" | cut -c1-400 | sed 's/^/    /' | tee -a "$LOG"
  [ "$rc" -eq 0 ] && [ -s "$JUDGE/boot.base.json" ] || { say "the head's boot could not be placed - see $JUDGE/record-boot.log"; return 1; }
  BOOT_SUM=$(sha256sum <"$JUDGE/boot.base.json"); BOOT_HEAD=$head
}

# queue_boot_blocker - keep one boot blocker in the queue: when none is queued or in review and
# the head moved since the last look, boot the head and queue where it stops, at the front. An
# item that fails three times goes to review and stops this until a person clears it. Once the
# boot reaches the game loop a "hang" is the game running; set MP_GOAL_BOOT_BLOCKERS=0 by then.
queue_boot_blocker() {
  [ "$BOOT_BLOCKERS" = 1 ] || return 0
  local head item bid btarget breason; head=$(git -C "$WT" rev-parse HEAD)
  [ "$BOOT_SCAN_HEAD" = "$head" ] && return 0
  Q has-verify "$BOOT_VERIFY" && return 0
  BOOT_SCAN_HEAD=$head
  reset_wt
  record_boot || { say "boot blocker scan: nothing queued"; return 0; }
  item=$(python3 "$REPO_ROOT/tools/goal_verify/boot_progress.py" blocker "$JUDGE/boot.base.json") \
    || { say "boot blocker scan: the head's runs left no stack to name - nothing queued"; return 0; }
  bid=$(printf '%s' "$item" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])')
  btarget=$(printf '%s' "$item" | python3 -c 'import json,sys;print(json.load(sys.stdin)["target"])')
  breason=$(printf '%s' "$item" | python3 -c 'import json,sys;print(json.load(sys.stdin)["reason"])')
  Q add "$bid" --kind port --target "$btarget" --reason "$breason" --verify "$BOOT_VERIFY" --first \
    | sed 's/^/    /' | tee -a "$LOG"
}

# port_judgeable <item-json> - can goal_check.sh decide this port item at all? Only if its target
# was in the linker's undefined list at the branch head, or the item carries a verify script.
# An inline or a wrong body is never "undefined", so without verify the item would pass on no work.
port_judgeable() {
  local target verify
  target=$(printf '%s' "$1" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("target",""))')
  verify=$(printf '%s' "$1" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("verify",""))')
  [ -n "$verify" ] && return 0
  [ -n "$target" ] && cut -f1 "$JUDGE/undef.base.txt" | grep -qF -- "$target"
}

# ingest_new <id> - queue the NEW: lines an agent wrote in its notes. The prompt promises the
# driver does this; before this function nothing did, and four found blockers sat in a note.
#
# A line may end in "| verify: boot-progress.sh" and that is the only script it may name: the
# boot judge fits any item that makes the boot get further, while the other scripts were each
# written for one item and would judge a different one wrongly.
ingest_new() {
  local f="$NOTES/$1.md" line nid nkind ntarget nreason nverify added arc
  [ -f "$f" ] || return 0
  grep -E '^[[:space:]]*NEW:' "$f" | while IFS= read -r line; do
    line=${line#*NEW:}
    nverify=()
    if [[ "$line" =~ \|[[:space:]]*verify:[[:space:]]*([^[:space:]|]+)[[:space:]]*$ ]]; then
      if [ "${BASH_REMATCH[1]}" != "$BOOT_VERIFY" ]; then
        say "ignoring the verify on a NEW: line in $1.md - only $BOOT_VERIFY may be named there"
      else
        nverify=(--verify "$BOOT_VERIFY")
      fi
      line=${line%|*}
    fi
    IFS='|' read -r nid nkind ntarget nreason <<<"$line"
    nid=$(printf '%s' "$nid" | tr -d '`[:space:]'); nkind=$(printf '%s' "$nkind" | tr -d '`[:space:]')
    ntarget=$(printf '%s' "$ntarget" | sed 's/^[[:space:]`]*//; s/[[:space:]`]*$//')
    nreason=$(printf '%s' "$nreason" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
    case "$nid" in ''|*[!A-Za-z0-9._-]*) say "ignoring a malformed NEW: line in $1.md"; continue ;; esac
    case "$nkind" in port|match) ;; *) say "ignoring NEW: $nid - kind '$nkind' is not port or match"; continue ;; esac
    [ -n "$ntarget" ] || { say "ignoring NEW: $nid - no target"; continue; }
    added=$(Q add "$nid" --kind "$nkind" --target "$ntarget" --reason "found by $1: $nreason" "${nverify[@]}" 2>&1)
    arc=$?
    printf '%s\n' "$added" | sed 's/^/    /' | tee -a "$LOG"
    # An agent may not edit tools/, so such an item can only fail three times; it is for a person.
    [ "$arc" -eq 0 ] && case "$ntarget" in tools/*)
      Q review "$nid" --why "targets $ntarget - agents may not edit tools/; for the orchestrator" \
        | sed 's/^/    /' | tee -a "$LOG" ;;
    esac
  done
}

# tree_state - one hash of everything git can see in the worktree: status, staged and unstaged
# content. The reviewer runs with --auto (it needs to read the repo), so its verdict counts only if
# this is identical before and after it ran.
tree_state() {
  ( cd "$WT" && { git status --porcelain --untracked-files=all; git diff --binary HEAD; } | sha256sum )
}

# review_text <jsonl> - the agent's text parts, in order, from an `opencode run --format json` log.
review_text() {
  python3 - "$1" <<'PY'
import json, sys
for line in open(sys.argv[1], errors="replace"):
    try:
        e = json.loads(line)
    except ValueError:
        continue
    if isinstance(e, dict) and e.get("type") == "text":
        print(e.get("part", {}).get("text", ""))
PY
}

# review_change <id> <run-n> - the reviewer's verdict on the staged diff.
#   0 PASS (REVIEW_FINDINGS set)   1 REJECT (REVIEW_REASON set)   2 no verdict (REVIEW_REASON set)
# REVIEW_PATCH is the exact diff reviewed. A diff over REVIEW_MAX_BYTES is rejected unread:
# a reviewer handed a truncated diff would be passing a change it never saw.
review_change() {
  local id=$1 n=$2 bytes try rc rlog pre text vline
  REVIEW_PATCH="$REVIEWDIR/$id-$TAG$n.patch"; REVIEW_REASON=""; REVIEW_FINDINGS=""; REVIEW_LOG=""
  git -C "$WT" diff --cached --binary >"$REVIEW_PATCH"
  bytes=$(wc -c <"$REVIEW_PATCH")
  if [ "$bytes" -gt "$REVIEW_MAX_BYTES" ]; then
    REVIEW_REASON="the staged diff is $bytes bytes, over the reviewer's $REVIEW_MAX_BYTES-byte limit. A change this large cannot be reviewed whole; do the item in a smaller change and queue the rest as NEW: items."
    return 1
  fi
  local prompt="Review one goal-loop change. Read $REPO_ROOT/docs/goal-review-prompt.md first and follow it exactly.
The item is $WT/build/goal/item.json. The staged diff to review is $REVIEW_PATCH ($bytes bytes).
You are in $WT. Read anything you need; change nothing. End with the VERDICT line."
  for try in $(seq 1 "$REVIEW_TRIES"); do
    pre=$(tree_state)
    rlog="$AGENTLOG/$id-$TAG$n-review$try-$(date -u +%Y%m%dT%H%M%S).jsonl"; REVIEW_LOG="$rlog"
    ( cd "$WT" && timeout -k 30s "$REVIEW_TIMEOUT" "$REVIEW_OPENCODE" run --standalone --agent "$REVIEWER" -m "$(model_for "$REVIEWER")" --format json --auto \
        "$prompt" ) >"$rlog" 2>&1
    rc=$?
    if [ "$(tree_state)" != "$pre" ] || ! ( cd "$JUDGE" && sha256sum --status -c sums ) 2>/dev/null; then
      say "the reviewer changed the tree or the judge's baselines - verdict void; restoring the reviewed change"
      rm -f "$JUDGE/HEAD"   # re-record before the next item, whatever it touched
      ( cd "$WT" && git reset -q --hard "$BASE" && git clean -qfd -e orig -e build -e .tmp \
          && git apply --index --binary "$REVIEW_PATCH" ) \
        || { REVIEW_REASON="could not restore the reviewed change after the reviewer modified the tree"; return 2; }
      REVIEW_REASON="the reviewer modified the tree (try $try)"
    else
      text=$(review_text "$rlog")
      # The last VERDICT: line wins; tolerate markdown decoration around it.
      vline=$(printf '%s\n' "$text" | sed 's/^[[:space:]*`>#_]*//; s/[*`_]*[[:space:]]*$//' | grep -E '^VERDICT:' | tail -1)
      case "$vline" in
        "VERDICT: PASS"|"VERDICT: PASS."|"VERDICT:PASS")
          REVIEW_FINDINGS=$(printf '%s\n' "$text" | grep -vE '^[[:space:]*`>#_]*VERDICT:' | sed '/^[[:space:]]*$/d' | tail -15 | cut -c1-200)
          return 0 ;;
        VERDICT:*REJECT*)
          REVIEW_REASON=$(printf '%s' "$vline" | sed -E 's/^VERDICT:[[:space:]]*REJECT[[:space:]:.-]*//')
          [ -n "$REVIEW_REASON" ] || REVIEW_REASON=$(printf '%s\n' "$text" | sed '/^[[:space:]]*$/d' | tail -15)
          return 1 ;;
      esac
      REVIEW_REASON="no VERDICT line (reviewer exit $rc, try $try)"
    fi
    say "review: $REVIEW_REASON - transcript $rlog"
    [ "$try" -lt "$REVIEW_TRIES" ] && sleep "$REVIEW_RETRY_WAIT"
  done
  return 2
}

# rebase_onto_tip - lane mode, PUBLISH_LOCK held, the judged change staged. If TIP moved while
# this lane worked, carry the change onto it and judge it again there: a change that passed on
# its own base can still collide with what another lane landed. 1 = does not apply or does not
# pass on the new tip; the item is released for a fresh attempt, not failed.
rebase_onto_tip() {
  local now old=$BASE patch="$GOAL/rebase.patch" rc
  now=$(git -C "$REPO_ROOT" rev-parse "$TIP") || return 1
  [ "$now" = "$BASE" ] && return 0
  say "$TIP moved (${BASE:0:7} -> ${now:0:7}) during $ID - carrying the judged change onto it"
  git -C "$WT" diff --cached --binary >"$patch"
  ( cd "$WT" && git reset -q --hard && git checkout -q --force -B "$BRANCH" "$now" \
      && git clean -qfd -e orig -e build -e .tmp ) || return 1
  BASE=$now
  record_judge || fatal "cannot record the judge's baselines at ${now:0:7}"
  [ -s "$patch" ] || return 0   # nothing to carry: the "already done" pass
  if ! ( cd "$WT" && git apply --index --3way --binary "$patch" ) >>"$LOG" 2>&1; then
    say "$ID does not apply on ${now:0:7} - releasing it for a fresh attempt"
    return 1
  fi
  say "re-judging $ID on ${now:0:7}"
  ( cd "$WT" && timeout -k 30s "$CHECK_TIMEOUT" "$REPO_ROOT/tools/goal_check.sh" "$GOAL/item.json" ) 2>&1 | tee -a "$LOG" | sed 's/^/    /'
  rc=${PIPESTATUS[0]}
  if [ "$rc" -ne 0 ]; then
    say "$ID passed on ${old:0:7} but fails on ${now:0:7} (check exit $rc) - releasing it"
    printf '\n## Lane %s: passed, then failed on the moved tip (%s)\n\nThe judged change failed goal_check.sh (exit %s) once rebased onto %s; re-do it against the current tip.\n' \
      "$LANE" "$(date -u '+%F %TZ')" "$rc" "${now:0:12}" >>"$NOTES/$ID.md"
    return 1
  fi
  ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt ) || true
  return 0
}

unlock_publish() { [ -n "$LANE" ] && exec 8>&-; return 0; }

write_summary() {
  local passed="$1" failed="$2" skipped="$3" reason="${4:-}"
  local m l ql last
  m=$(python3 -c 'import json;print(json.load(open("build/report.json"))["measures"]["matched_functions"])' 2>/dev/null || echo '?')
  l=$(python3 -c 'import json;r=json.load(open("build/report.json"));print(sum(u["measures"].get("matched_functions",0) for u in r["units"] if u.get("metadata",{}).get("complete")))' 2>/dev/null || echo '?')
  ql=$(Q list 2>/dev/null | tail -1)
  last=$(git -C "$REPO_ROOT" log -5 --format='    %h %s' "$TIP")
  cat >"$SUMMARY" <<EOF
mp2 goal summary${LANE:+ (lane $LANE)} - $(date -u '+%F %T')Z
=====================================
matched $m / 28465      linked $l
passed $passed   failed $failed   items reset/skipped $skipped
EOF
  [ -n "$reason" ] && echo "STOPPED: $reason" >>"$SUMMARY"
  cat >>"$SUMMARY" <<EOF

queue: $ql

last 5 commits on $TIP:
$last
EOF
  say "summary written: matched=$m linked=$l passed=$passed failed=$failed"
}

# ----------------------------------------------------------------- main loop
passes=0; fails=0; skipped=0; consec_fail=0; agent_errors=0; no_verdict=0; agent="worker"
item_n=0

# A summary from the first second, so `build/goal/summary.txt` always has a current
# first line rather than appearing only after 10 items.
write_summary 0 0 0
reset_wt
record_judge || fatal "cannot record the judge's baselines on the branch head"

while :; do
  # --- disk guard, before anything expensive
  if ! disk_ok; then
    say "disk guard: pausing (repo ${DISK_MIN_GB}G, TMPDIR ${TMPDIR_MIN_GB}G thresholds) - recheck in 10 min"
    sleep 600; continue
  fi

  # --- the boot blocker at the head, if none is being worked (a no-op until the head moves)
  queue_boot_blocker

  # --- anything left?
  Q has-next "${LANEARG[@]}" >/dev/null 2>&1; HN=$?
  if [ "$HN" = 3 ]; then
    # Every ready item is another lane's. It may fail back into the queue or queue NEW: items.
    say "every ready item is claimed by another lane - waiting 10 min"
    sleep 600; continue
  elif [ "$HN" != 0 ]; then
    say "queue has nothing ready - stopping"
    write_summary "$passes" "$fails" "$skipped"
    break
  fi

  ITEM=$(Q next "${LANEARG[@]}" 2>/dev/null | tail -1)
  if [ -z "$ITEM" ]; then say "next returned nothing - stopping"; write_summary "$passes" "$fails" "$skipped"; break; fi
  ID=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])')
  KIND=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["kind"])')
  item_n=$((item_n+1))
  say "--- item $item_n: $ID ($KIND)"

  # --- reset to the branch head, and measure against it. record_judge is a no-op unless the
  # head moved (a pass landed) or the baselines were disturbed; the reset comes first because
  # `gate.sh --baseline` refuses a dirty tree.
  reset_wt
  record_judge || fatal "cannot record the judge's baselines at $(git -C "$WT" rev-parse --short HEAD)"

  if [ "$KIND" = port ] && ! port_judgeable "$ITEM"; then
    say "$ID: the judge cannot decide it (target never undefined, no verify script) - to review, no agent run"
    Q review "$ID" --why "unjudgeable: target not in the port's undefined list and no verify script" | tee -a "$LOG"
    skipped=$((skipped+1))
    continue
  fi
  VERIFYP=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("verify",""))')
  if [ "$VERIFYP" = "$BOOT_VERIFY" ] && ! record_boot; then
    say "$ID: $BOOT_VERIFY has no head position to beat - to review, no agent run"
    Q review "$ID" --why "boot-progress: the head's boot could not be placed; see $JUDGE/record-boot.log" | tee -a "$LOG"
    skipped=$((skipped+1))
    continue
  fi

  # --- run the agent
  PROMPT="You are working one goal item. Read these, in this order, and follow them exactly:
1. $REPO_ROOT/docs/goal-unit-prompt.md - your instructions, the one rule, and the rules that have
   cost real time.
2. $REPO_ROOT/AGENTS.md - how the decomp works.
3. $WT/build/goal/item.json - the item itself.

Work only in $WT. Do not commit. Do not touch any other worktree. Your notes file is
$NOTES/$ID.md. If you cannot finish, write it and stop."
  if [ -f "$NOTES/$ID.md" ]; then
    PROMPT="$PROMPT

This item has been tried before. Read $NOTES/$ID.md FIRST - it is what the last run learned, and
repeating its work is the most expensive thing you can do. Append to it; do not replace it."
  fi
  printf '%s\n' "$ITEM" >"$GOAL/item.json"

  say "running agent '$agent' on $(model_for "$agent") (timeout $AGENT_TIMEOUT)"
  # **`opencode run` has NO `--dir` flag** (`opencode run --help` lists --agent, --model, --format,
  # --file, --auto, ... and no directory option). The brief's command line included one, so every
  # item died in ~4 seconds on `Unrecognized flag: --dir` - the loop counted them as agent errors
  # and fell back to a name that was equally invalid. **The working directory is set by `cd`ing
  # into the worktree, which the subshell below already does.** Run it from the worktree, not from
  # the repo, or the agent edits the wrong tree.
  #
  # `--auto` is REQUIRED unattended: without it the first tool call aborts on a permission prompt.
  # The brief scopes `--auto` to this worktree and this is that worktree.
  #
  # **`--standalone` is REQUIRED.** Without it `opencode run` is a client of the shared background
  # service, and killing the client - which is what `timeout` does - leaves the session running
  # server-side. Measured: four such ghost sessions were still editing this worktree after the
  # loop had stopped, one of them concurrently with the judge. `--standalone` gives each run a
  # private server that dies with it; `-k` makes sure it does die.
  T0=$(date +%s)
  ALOG="$AGENTLOG/$ID-$TAG$item_n-$(date -u +%Y%m%dT%H%M%S).jsonl"
  ( cd "$WT" && timeout -k 30s "$AGENT_TIMEOUT" "$OPENCODE" run --standalone --agent "$agent" -m "$(model_for "$agent")" --format json --auto \
      "$PROMPT" ) >"$ALOG" 2>&1
  ARC=$?
  T1=$(date +%s)
  ELAPSED=$((T1 - T0))
  say "agent transcript: $ALOG ($(wc -l <"$ALOG") lines)"
  ingest_new "$ID"

  # A timeout is not an agent error when the agent left a change: `port-streamnewgamestate`
  # attempt 1 reached `goal_check: PASS` at ~3486s, was killed at 3601s and reset, and the next
  # attempt inherited a note saying "finished" over a clean tree. Judge the tree instead - every
  # check and the reviewer still run, and a half-made change fails them like any other.
  if { [ "$ARC" = 124 ] || [ "$ARC" = 137 ]; } && [ -n "$(git -C "$WT" status --porcelain -- src include)" ]; then
    say "agent '$agent' ran out of time after ${ELAPSED}s and left changes under src/ or include/ - judging them"
    ARC=0
  fi

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
    clean_wt
    continue
  fi
  agent_errors=0

  # --- judge
  if ! ( cd "$JUDGE" && sha256sum --status -c sums ) 2>/dev/null; then
    say "the judge's baselines changed during the agent run - failing $ID and re-recording"
    CRC=5
    rm -f "$JUDGE/HEAD"
  elif [ "$VERIFYP" = "$BOOT_VERIFY" ] && [ "$(sha256sum <"$JUDGE/boot.base.json" 2>/dev/null)" != "$BOOT_SUM" ]; then
    say "the boot baseline changed during the agent run - failing $ID and re-recording"
    CRC=5
    BOOT_HEAD=""
  else
    say "checking (timeout $CHECK_TIMEOUT)"
    ( cd "$WT" && timeout -k 30s "$CHECK_TIMEOUT" "$REPO_ROOT/tools/goal_check.sh" "$GOAL/item.json" ) 2>&1 | tee -a "$LOG" | sed 's/^/    /'
    CRC=${PIPESTATUS[0]}
  fi

  # --- review: only a change the judge passed, and only if there is something to commit.
  # Not tools/: goal_check.sh fails any change there, and the judge's own code is not an
  # agent's to commit.
  REVIEW_NOTE=""
  KINDP=$(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin)["kind"])')
  if [ "$CRC" -eq 0 ]; then
    ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt ) || true
    if ! git -C "$WT" diff --cached --quiet && [[ " $REVIEW_KINDS " != *" $KINDP "* ]]; then
      say "judge PASS $ID - not reviewed ($KINDP items are outside MP_GOAL_REVIEW_KINDS='$REVIEW_KINDS')"
      REVIEW_NOTE="Not reviewed: $KINDP items are decided by the checks alone (MP_GOAL_REVIEW_KINDS='$REVIEW_KINDS')."
    elif ! git -C "$WT" diff --cached --quiet; then
      say "judge PASS $ID - reviewing with '$REVIEWER' (timeout $REVIEW_TIMEOUT, $REVIEW_TRIES tries)"
      review_change "$ID" "$item_n"; RV=$?
      if [ "$RV" -eq 0 ]; then
        no_verdict=0
        say "review PASS ($REVIEWER) - transcript $REVIEW_LOG"
        REVIEW_NOTE="Reviewed by $REVIEWER: PASS${REVIEW_FINDINGS:+
Reviewer findings (not blocking):
$REVIEW_FINDINGS}"
      elif [ "$RV" -eq 1 ]; then
        no_verdict=0
        say "review REJECT ($REVIEWER): $REVIEW_REASON"
        { printf '\n## Review rejected run %s (%s, reviewer %s)\n\n' "$item_n" "$(date -u '+%F %TZ')" "$REVIEWER"
          printf 'The judge passed this attempt; the reviewer rejected it:\n\n%s\n\n' "$REVIEW_REASON"
          printf 'Rejected diff: %s\nReview transcript: %s\n' "$REVIEW_PATCH" "${REVIEW_LOG:-none (rejected unread)}"
        } >>"$NOTES/$ID.md"
        CRC=6
      else
        # Fail closed: no verdict, no commit. The judged change is kept as a patch, the item goes
        # to review rather than burning a retry, and a reviewer that stays silent stops the loop -
        # every further item would spend a full agent run only to land here.
        no_verdict=$((no_verdict+1))
        say "no review verdict for $ID after $REVIEW_TRIES tries ($REVIEW_REASON) - not committing; patch kept at $REVIEW_PATCH"
        Q review "$ID" --why "judge passed but no review verdict ($REVIEW_REASON); patch at $REVIEW_PATCH" | tee -a "$LOG"
        skipped=$((skipped+1))
        clean_wt
        if [ "$no_verdict" -ge "$MAX_NO_VERDICT" ]; then
          fatal "$no_verdict items in a row got no review verdict from '$REVIEWER' - is its server up?"
        fi
        continue
      fi
    fi
  fi

  if [ "$CRC" -eq 0 ] && [ -n "$LANE" ]; then
    # Held from here to the publish (and the master fast-forward): one lane lands at a time.
    exec 8>"$PUBLISH_LOCK"; flock 8
    if ! rebase_onto_tip; then
      clean_wt; Q release "$ID" | tee -a "$LOG"; skipped=$((skipped+1))
      unlock_publish
      continue
    fi
  fi

  if [ "$CRC" -eq 0 ]; then
    say "PASS $ID - committing"
    MSG=$(git -C "$WT" diff --cached --stat | tail -1)
    committed=0
    if git -C "$WT" diff --cached --quiet; then
      # A match item whose unit was already Matching passes with nothing to change.
      say "PASS $ID with nothing to commit - it was already done at the branch head"
      committed=1
    else
    ( cd "$WT" && git commit -q -m "$KINDP: $ID

Goal item $ID ($KIND). Target: $(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("target",""))')
Reason: $(printf '%s' "$ITEM" | python3 -c 'import json,sys;print(json.load(sys.stdin).get("reason",""))')

$MSG

Verified by tools/goal_check.sh: gate.sh against the judge's baseline, matched and linked not
lower, check_symbol_names.py clean, and $( [ "$KIND" = match ] && echo "flip_test.sh kept the unit" \
  || echo "the port target resolved (undefined at the branch head and gone, or its verify script
passed), port undefined not higher, probe_sources.sh clean").
$REVIEW_NOTE

Co-Authored-By: opencode-go/space-bunny-free <no-reply@opencode.ai>" ) && committed=1
    fi
    if [ "$committed" != 1 ]; then
      say "COMMIT FAILED $ID - not marking done; resetting and counting it as a failure"
      clean_wt
      Q fail "$ID"; fails=$((fails+1)); consec_fail=$((consec_fail+1))
      unlock_publish
      continue
    fi
    if [ -n "$LANE" ] && [ "$(git -C "$WT" rev-parse HEAD)" != "$BASE" ] && ! publish "$BASE"; then
      say "PUBLISH FAILED $ID - $TIP is no longer ${BASE:0:7} though this lane held $PUBLISH_LOCK (moved by hand?); releasing it"
      clean_wt; Q release "$ID" | tee -a "$LOG"; skipped=$((skipped+1))
      unlock_publish
      continue
    fi
    say "$ID done; $TIP at $(git -C "$REPO_ROOT" rev-parse --short "$TIP")"
    Q done "$ID"
    passes=$((passes+1)); consec_fail=0
    if [ $((passes % FF_EVERY)) -eq 0 ] && [ -z "$(git -C "$REPO_ROOT" status --porcelain --untracked-files=no)" ]; then
      # Master gains tooling commits the branch lacks, and then no fast-forward is possible.
      # Take them first; the worktree is clean here, just after the commit. A conflict aborts.
      MASTER=$(git -C "$REPO_ROOT" symbolic-ref --short HEAD)
      if ! git -C "$WT" merge-base --is-ancestor "$MASTER" HEAD; then
        PREV=$(git -C "$WT" rev-parse HEAD)
        if ( cd "$WT" && git merge -q --no-edit "$MASTER" ) >/dev/null 2>&1 \
            && { [ -z "$LANE" ] || publish "$PREV"; }; then
          say "merged $MASTER into $TIP ($(git -C "$WT" rev-parse --short HEAD)) before the fast-forward"
        else
          ( cd "$WT" && git merge --abort ) >/dev/null 2>&1
          say "merging $MASTER into $TIP conflicted - aborted; the fast-forward will be skipped"
        fi
      fi
      if git -C "$REPO_ROOT" merge --ff-only "$TIP" >/dev/null 2>&1; then
        say "fast-forwarded master to $TIP"
      else
        say "skipping the fast-forward: master is not clean or not a fast-forward - noted in the summary"
      fi
    fi
    unlock_publish
    if [ $((passes % 10)) -eq 0 ]; then write_summary "$passes" "$fails" "$skipped"; fi
  else
    if [ "$CRC" -eq 6 ]; then say "FAIL $ID (the reviewer rejected it) - resetting and recording"
    else say "FAIL $ID (check exit $CRC) - resetting and recording"; fi
    clean_wt
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
