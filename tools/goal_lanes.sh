#!/bin/bash
# tools/goal_lanes.sh - run the goal loop as several parallel lanes (run_goal.sh, MP_GOAL_LANE=k).
#
#   tools/goal_lanes.sh setup N        # worktrees ../wt-mp2-goal-L1..N on goal/lane-1..N
#   tools/goal_lanes.sh install-unit   # ~/.config/systemd/user/mp2-goal@.service
#   systemctl --user start mp2-goal@{1..N}
#   tools/goal_lanes.sh status
#
# Lanes share ../wt-mp2-goal/build/goal (queue, notes, agent transcripts, review patches) and
# publish onto goal/decomp; nothing checks goal/decomp out while they run, so `setup` detaches
# ../wt-mp2-goal. To go back to the single loop: stop the lanes, `git -C ../wt-mp2-goal checkout
# goal/decomp`, start mp2-goal.service. See docs/RUNNING_THE_DECOMP.md, "Parallel lanes".
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PARENT="$(cd "$REPO_ROOT/.." && pwd)"
SINGLE="$PARENT/wt-mp2-goal"
TIP="goal/decomp"
UNIT="$HOME/.config/systemd/user/mp2-goal@.service"

lane_wt() { echo "$PARENT/wt-mp2-goal-L$1"; }

running() {
  systemctl --user is-active -q mp2-goal.service && return 0
  systemctl --user list-units --state=active --plain --no-legend 'mp2-goal@*' | grep -q .
}

cmd_setup() {
  local n=${1:-} k wt
  case "$n" in [1-9]) ;; *) echo "setup: N must be 1-9" >&2; exit 2 ;; esac
  if running; then echo "setup: stop mp2-goal.service and every mp2-goal@ lane first" >&2; exit 2; fi
  if [ "$(git -C "$SINGLE" symbolic-ref -q --short HEAD || true)" = "$TIP" ]; then
    [ -z "$(git -C "$SINGLE" status --porcelain --untracked-files=no)" ] \
      || { echo "setup: $SINGLE has uncommitted changes on $TIP; not detaching it" >&2; exit 2; }
    git -C "$SINGLE" checkout -q --detach
    echo "detached $SINGLE (it keeps the shared build/goal state; lanes publish onto $TIP)"
  fi
  for k in $(seq 1 "$n"); do
    wt=$(lane_wt "$k")
    if [ -d "$wt" ]; then echo "lane $k: $wt exists"; continue; fi
    git -C "$REPO_ROOT" worktree add -q -B "goal/lane-$k" "$wt" "$TIP"
    # orig/ is read-only and never copied; binutils and the downloaded tools are copied so the
    # lane does not fetch them again. None of these is tracked, so none can be `git add`ed.
    mkdir -p "$wt/orig" "$wt/build"
    ln -s "$REPO_ROOT/orig/G2ME01" "$wt/orig/G2ME01"
    [ -d "$SINGLE/build/binutils" ] && cp -a "$SINGLE/build/binutils" "$wt/build/"
    [ -d "$SINGLE/build/tools" ] && cp -a "$SINGLE/build/tools" "$wt/build/"
    echo "lane $k: $wt on goal/lane-$k at $(git -C "$wt" rev-parse --short HEAD)"
  done
}

cmd_install_unit() {
  mkdir -p "$(dirname "$UNIT")"
  cat >"$UNIT" <<EOF
[Unit]
Description=Metroid Prime 2 decomp goal loop, lane %i
After=default.target

[Service]
Type=simple
ExecStart=$REPO_ROOT/tools/run_goal.sh
Environment=MP_GOAL_LANE=%i
Environment=PATH=$HOME/.opencode/bin:/usr/local/bin:/usr/bin:/bin
Environment=MP_TOOLCHAIN_DIR=$PARENT/MetroidPrimePort
Restart=on-failure
RestartSec=300
StandardOutput=append:$REPO_ROOT/build/goal/run-L%i.log
StandardError=append:$REPO_ROOT/build/goal/run-L%i.log

[Install]
WantedBy=default.target
EOF
  systemctl --user daemon-reload
  echo "wrote $UNIT"
}

cmd_status() {
  local wt k
  echo "$TIP at $(git -C "$REPO_ROOT" rev-parse --short "$TIP")"
  for wt in "$PARENT"/wt-mp2-goal-L*; do
    [ -d "$wt" ] || continue
    k=${wt##*-L}
    printf 'lane %s: %s  %s  head %s\n' "$k" \
      "$(systemctl --user is-active "mp2-goal@$k" 2>/dev/null || true)" "$wt" \
      "$(git -C "$wt" rev-parse --short HEAD)"
    tail -1 "$wt/build/goal/run.log" 2>/dev/null | sed 's/^/    /' || true
  done
  MP_GOAL_QUEUE_DIR="$SINGLE/build/goal" python3 "$REPO_ROOT/tools/goal_queue.py" list | grep -E '\[lane|queued'
}

case "${1:-}" in
  setup) shift; cmd_setup "$@" ;;
  install-unit) cmd_install_unit ;;
  status) cmd_status ;;
  *) sed -n '2,12p' "$0"; exit 2 ;;
esac
