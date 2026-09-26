#!/bin/bash
# How far does the port actually get? A diagnostic, not a deliverable.
#
#   tools/boot_probe.sh [--rebuild]
#
# Why this exists. `tools/link_reach.py` says 342 undefined symbols are referenced by
# objects reachable from the program's roots, so they cannot be stubbed. That is a
# static upper bound: it is whole-object and branch-blind, so it cannot say which of
# them the game *actually calls* on the way to a frame. Every plan to close the gap
# so far has been argued from that bound.
#
# This measures instead of arguing. Linking with `--unresolved-symbols=ignore-all`
# produces a real binary whose calls into the missing 342 land at address 0, so the
# first one the game actually reaches is observable: the process dies on a jump to 0
# and the faulting address is the symbol. That converts "342 by static analysis" into
# an ordered list by first use, which is the only ordering that tells us what to
# write first.
#
# **This binary is not a port and must never be committed, shipped or reported as
# one.** Every unresolved call is a jump to address 0. It exists to be crashed.
#
# What it CAN establish, and this is worth having:
#   - whether the port reaches window creation and a first frame at all, or dies
#     earlier, and exactly where;
#   - the identity of the first genuinely-missing symbol, from the fault address;
#   - whether the 523 -> 342 stub work moved the failure later or not at all, which
#     is a real check on a claim that was made on faith.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

TOOLCHAIN="${MP_TOOLCHAIN:-$REPO_ROOT/../MetroidPrimePort/build/review-tools}"
CMAKE="$TOOLCHAIN/bin/cmake"
NINJA="$TOOLCHAIN/bin/ninja"
BUILD="${MP_BOOT_BUILD:-$REPO_ROOT/build-boot-probe}"
LOG="$BUILD/build.log"

REBUILD=0
[ "${1:-}" = "--rebuild" ] && REBUILD=1

for t in "$CMAKE" "$NINJA"; do
  [ -x "$t" ] || { echo "boot_probe: $t not found." >&2
    echo "  Set MP_TOOLCHAIN, or build the sibling port's review tools." >&2; exit 2; }
done

if [ "$REBUILD" = 1 ] || [ ! -f "$BUILD/build.log" ]; then
  echo "boot_probe: removing $BUILD"
  rm -rf "$BUILD"
fi
mkdir -p "$BUILD"

# --warn-unresolved-symbols is the entire point: without it the link fails and there
# is nothing to run. It downgrades the 342 to warnings and still produces a binary.
#
# --unresolved-symbols=ignore-all was tried first and is WRONG here: it makes ld take
# a different archive-resolution path and the final link then dies with "hidden symbol
# `crc32' in libnod.a(f0389296f42960e9-crc32.o) is referenced by DSO", which has
# nothing to do with the game. --warn-unresolved-symbols reports them and links.
#
# The flag is passed as a link flag, not baked into CMakeLists.txt, so the port's real
# build is untouched and the real link still fails on all 342.
echo "boot_probe: configuring (MP_SDK_HEADERS_ONLY=OFF, unresolved ignored)"
"$CMAKE" -S . -B "$BUILD" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DMP_SDK_HEADERS_ONLY=OFF \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--warn-unresolved-symbols -Wl,--allow-shlib-undefined" \
  > "$BUILD/configure.log" 2>&1 || {
    echo "boot_probe: configure FAILED. Tail:" >&2; tail -20 "$BUILD/configure.log" >&2; exit 1; }

echo "boot_probe: building"
"$CMAKE" --build "$BUILD" --target metroid_prime2_port > "$LOG" 2>&1
st=$?
if [ $st -ne 0 ]; then
  echo "boot_probe: BUILD FAILED (status $st). Tail:" >&2; tail -25 "$LOG" >&2; exit 1
fi

BIN="$BUILD/metroid_prime2_port"
[ -x "$BIN" ] || { echo "boot_probe: no binary at $BIN" >&2; exit 1; }
echo "boot_probe: linked $(stat -c%s "$BIN") bytes with unresolved symbols warned, not ignored"
echo "boot_probe: this is NOT a working port - every unresolved call jumps to 0."
grep -c "undefined reference to" "$LOG" 2>/dev/null | sed "s/^/boot_probe: the log names /;s/$/ unresolved symbols/"

# No display is assumed. SDL3 is told to use a dummy video driver so the run
# reaches the game's own initialisation instead of stopping in the windowing layer,
# and audio is told to use the dummy driver for the same reason.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
export MPLBACKEND="${MPLBACKEND:-Agg}"
ulimit -c 0

echo "boot_probe: running (timeout 60s)"
OUT="$BUILD/run.log"
timeout 60 "$BIN" > "$OUT" 2>&1
rc=$?
echo "boot_probe: exit status $rc"
echo "--- last 40 lines of output ---"
tail -40 "$OUT"

# A jump to an unresolved symbol lands at 0, so the faulting address *is* the
# symbol. Report it if the kernel gave us one.
if [ $rc -ge 128 ]; then
  sig=$((rc - 128))
  echo "boot_probe: died on signal $sig"
  [ "$sig" = 11 ] && echo "boot_probe: SIGSEGV is consistent with a call to an unresolved symbol (address 0)"
fi
echo "boot_probe: build log $LOG, run log $OUT"
