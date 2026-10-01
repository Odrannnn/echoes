#!/bin/bash
# tools/goal_verify/boot_gdb_run.sh <binary> - boot_probe's MP_PROBE_RUNNER for boot-progress.sh.
#
# Runs the port under gdb MP_BOOT_RUNS times (default 2). Each run ends in one of three ways, and
# each one leaves the main thread's stack in the log, between [boot-progress] markers:
#   - a fault: gdb stops at the signal, before the port's own handler; the first sample is it;
#   - a hang: after MP_BOOT_HANG_SECS (default 45) the inferior gets SIGINT, which gdb catches,
#     then MP_BOOT_SAMPLES-1 more a second apart (default 5 samples in all). One sample of a
#     loop is a random point in it - two head runs stopped at different lines of the allocator -
#     so boot_progress.py takes the part of the stack every sample shares as the position;
#   - an exit: no stack; boot_progress.py counts it as progress only with new markers, or with
#     the same markers and exit code 0 when the head crashed or hung.
# The main thread's position is where the boot got to, whichever thread faulted, and it is the
# same measure for a crash and a hang. The port's SIGSEGV backtrace cannot give that for a hang.
set -uo pipefail
BIN=$1
RUNS="${MP_BOOT_RUNS:-2}"
HANG="${MP_BOOT_HANG_SECS:-45}"
SAMPLES="${MP_BOOT_SAMPLES:-5}"

SAMPLE=()
for k in $(seq 1 "$SAMPLES"); do
  SAMPLE+=(-ex "echo \\n[boot-progress] sample $k\\n" -ex 'info program' -ex 'thread 1'
           -ex 'echo [boot-progress] main-thread-bt-begin\n' -ex 'bt'
           -ex 'echo [boot-progress] main-thread-bt-end\n')
  [ "$k" -lt "$SAMPLES" ] && SAMPLE+=(-ex continue)
done

for r in $(seq 1 "$RUNS"); do
  echo "[boot-progress] run $r begin"
  # -ex commands keep going after one fails, so a run that exits early just prints errors for
  # the samples it never reached.
  gdb -q -nx -batch \
    -ex 'set debuginfod enabled off' -ex 'set pagination off' -ex 'set confirm off' \
    -ex 'set print thread-events off' -ex 'set print inferior-events off' \
    -ex 'set print frame-arguments none' -ex 'set width 0' \
    -ex 'handle SIGPIPE SIGUSR1 SIGUSR2 nostop noprint pass' \
    -ex 'handle SIGINT stop print nopass' \
    -ex run "${SAMPLE[@]}" -ex kill --args "$BIN" </dev/null &
  gpid=$!
  hung=0
  for _ in $(seq 1 "$HANG"); do
    sleep 1
    kill -0 "$gpid" 2>/dev/null || break
  done
  if kill -0 "$gpid" 2>/dev/null; then
    hung=1
    for _ in $(seq 1 "$SAMPLES"); do
      pkill -INT -P "$gpid"
      sleep 1
    done
    # gdb prints the last stack and kills the inferior; give it time, then make sure.
    for _ in $(seq 1 30); do sleep 1; kill -0 "$gpid" 2>/dev/null || break; done
    kill -9 "$gpid" 2>/dev/null
  fi
  wait "$gpid" 2>/dev/null
  echo "[boot-progress] run $r end (hang=$hung)"
done
exit 0
