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
# `-lz -lpng` come FIRST, before the object list, and that is the whole fix for the
# third-party blocker. Nod's Rust `crc32fast` module is a *global* `crc32` that its
# visibility attributes mark hidden, and it sits in `libnod.a`, which CMake places
# before the system libraries. A DSO of ours needs `crc32`, ld looks for it, finds
# Nod's hidden one, and aborts with
#
#   hidden symbol `crc32' in .../libnod.a(f0389296f42960e9-crc32.o) is referenced by DSO
#
# `--allow-shlib-undefined` does NOT suppress it: that is about *undefined* DSO
# symbols, and this one is defined-but-hidden. Putting a DSO that exports `crc32`
# ahead of the archive means the DSO's own definition is found first and the archive
# member is never pulled in. Diagnosed, not guessed - `nm -A` on the archive shows the
# member is `T crc32` in `f0389296f42960e9-crc32.o`, alongside `lzma_crc32` in a
# different member, so it is Nod's own and not an LZMA symbol in disguise.
#
# The flags are passed as link flags, not baked into CMakeLists.txt, so the port's real
# build is untouched and the real link still fails on all 319.
echo "boot_probe: configuring (MP_SDK_HEADERS_ONLY=OFF, MP_BOOT_STUBS=ON)"
"$CMAKE" -S . -B "$BUILD" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DMP_SDK_HEADERS_ONLY=OFF \
  -DMP_BOOT_STUBS=ON \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--no-demangle -Wl,--allow-shlib-undefined -lz -lpng -lstdc++ -Wl,--start-group -Wl,--end-group" \
  > "$BUILD/configure.log" 2>&1 || {
    echo "boot_probe: configure FAILED. Tail:" >&2; tail -20 "$BUILD/configure.log" >&2; exit 1; }

# The third-party `crc32` blocker, part two: configure re-fetches `libnod.a`, so
# pruning the offending member has to happen here rather than by hand. `nm -A` shows
# `f0389296f42960e9-crc32.o` defines a *global* `crc32` that Nod's visibility attributes
# mark hidden, and that its own `deflate.o`/`inflate.o` reference it. Deleting the member
# makes those two resolve `crc32` from libz instead - the same function - so Nod still
# works. This mutates only the throwaway build directory's copy; the real build is
# untouched, and `orig/` and the sibling toolchain are not involved.
NOD_LIB="$BUILD/_deps/nod_prebuilt-src/lib/libnod.a"
if [ -f "$NOD_LIB" ]; then
  before=$(ar t "$NOD_LIB" | wc -l)
  ar d "$NOD_LIB" f0389296f42960e9-crc32.o 2>/dev/null
  after=$(ar t "$NOD_LIB" | wc -l)
  echo "boot_probe: pruned Nod's hidden crc32 member ($before -> $after objects in libnod.a)"
fi

echo "boot_probe: building"
"$CMAKE" --build "$BUILD" --target metroid_prime2_port > "$LOG" 2>&1
st=$?

# ---------------------------------------------------------------------------------------
# Self-heal: stub what THIS link asks for, once, and relink.
#
# Without it, every new `Matching` unit that drags in a callee breaks the probe, and
# **no gate step sees it**: `tools/gate.sh` does not run the probe, and the ordinary port
# link does not enable the diagnostic stubs, so `port link gap` and `port link dups` both
# pass. The three landed that way on 2026-09-26 - `src/MetaRender/Carve8026E7F0.cpp` pulls
# in `fn_802C15E8` and three siblings, and `src/Kyoto/Graphics/Carve802BEC1C.cpp` pulled in
# `lbl_80418AFF` - and the symptom was a probe that had not run since 18:15.
#
# The set is read from *this* log rather than from `docs/research/port_link_gap_list.md`,
# because the gap list answers "which decompilation symbols are missing" and the linker asks a
# different question. The probe's configuration resolves some symbols the port's does not
# (`MP_BOOT_STUBS=ON` adds PortReachStubs) and needs some the port's does not, so neither set
# contains the other.
#
# This is a *diagnostic* stub file, so appending to it cannot affect the port: the `reach stubs`
# gate step fails if `MP_BOOT_STUBS=ON` is ever on in a build whose undefined count anyone
# would believe. One pass only, so a genuine missing symbol is still reported rather than
# hidden.
# ---------------------------------------------------------------------------------------
# `-Wl,--no-demangle` (configure, above) is what makes this work for C++: without it ld prints
# `CFoo::Bar(int)`, which cannot be written back as a symbol, and the first version of this step
# skipped every such name. It also retires stubs the tree now defines for real (the link's
# `multiple definition` lines) - the other way the file went stale across the 2026-09-28 upstream
# merge, when 109 duplicates and 140 unstubbed C++ symbols stopped the probe linking and the goal
# loop could not place any boot. `tools/restub_reach.py` does both edits, by symbol name.
if [ $st -ne 0 ] && grep -qE "undefined reference to|multiple definition of" "$LOG" 2>/dev/null; then
  echo "boot_probe: link failed on $(grep -c "undefined reference to" "$LOG") undefined /" \
       "$(grep -c "multiple definition of" "$LOG") duplicate line(s); updating the diagnostic stubs and relinking"
  python3 tools/restub_reach.py "$LOG" src/MetroidPrime/PortReachStubs.cpp | sed 's/^/boot_probe: /'
  "$CMAKE" --build "$BUILD" --target metroid_prime2_port > "$LOG" 2>&1
  st=$?
  echo "boot_probe: relink status $st"
fi

if [ $st -ne 0 ]; then
  # **A duplicate definition is the one link failure with a known, mechanical cause, and it
  # can only appear in *this* build.** `src/MetroidPrime/PortReachStubs.cpp` is added by
  # CMakeLists.txt only under `-DMP_BOOT_STUBS=ON`, which only this script passes, and its
  # 182 `asm("_ZN...")` aliases make the linker resolve retail's name to a stub. List a decomp
  # unit that *defines* that name for real and the two collide. `gate.sh`'s `port link dups`
  # step cannot see this: it runs without the option, so the file is not compiled there and
  # `duplicate definitions 0` is a true statement about a build in which the offender is
  # absent. That is worth saying out loud, because reading that 0 as "the tree has no
  # duplicates" is how this reached the link the first time.
  dups=$(grep -c "multiple definition of" "$LOG" 2>/dev/null || echo 0)
  if [ "${dups:-0}" -gt 0 ]; then
    echo "boot_probe: $dups DUPLICATE definition(s) - a reach stub is aliased onto a symbol" >&2
    echo "  that now has a real definition. The fix is to delete the stale alias from" >&2
    echo "  src/MetroidPrime/PortReachStubs.cpp, not to remove the decomp unit:" >&2
    grep "multiple definition of" "$LOG" | sed 's/.*multiple definition of/    /' | sort -u | head -10 >&2
  fi
  echo "boot_probe: BUILD FAILED (status $st). Tail:" >&2; tail -25 "$LOG" >&2; exit 1
fi

# A stub can also be wrong *without* a link error: a strong stub for a symbol another object
# defines weak (nm V/W - a vtable keyed to an inline dtor, a COMDAT function) wins silently. The
# zero-filled `_ZTV18CErrorOutputWindow` stub did that on 2026-09-29, and frame 1 jumped to
# address 0 in `win->PreDraw()`. So after a good link, retire those too and relink once.
shadow=$(python3 tools/restub_reach.py --objdir "$BUILD/CMakeFiles" /dev/null src/MetroidPrime/PortReachStubs.cpp)
case "$shadow" in
  "retired 0,"*) ;;
  *) echo "$shadow" | sed 's/^/boot_probe: weak-shadow pass: /'
     "$CMAKE" --build "$BUILD" --target metroid_prime2_port > "$LOG" 2>&1 || {
       echo "boot_probe: relink after the weak-shadow pass FAILED. Tail:" >&2; tail -25 "$LOG" >&2; exit 1; } ;;
esac

BIN="$BUILD/metroid_prime2_port"
[ -x "$BIN" ] || { echo "boot_probe: no binary at $BIN" >&2; exit 1; }
echo "boot_probe: linked $(stat -c%s "$BIN") bytes with unresolved symbols warned, not ignored"
grep -c "undefined reference to" "$LOG" 2>/dev/null | sed "s/^/boot_probe: the log names /;s/$/ unresolved symbols/"

# ---------------------------------------------------------------------------------------
# A real display, because a window is the point.
#
# `SDL_VIDEODRIVER=dummy` does NOT work and the reason is worth recording: with it, Aurora
# tries Vulkan, then OpenGLES, then its Null backend, and **all three fail with "Failed to
# create surface"**, so the run dies in the windowing layer and never reaches the game's
# own start-up. The surface is what is missing, not a driver to select.
#
# `Xvfb` plus Mesa's `lvp_icd` (lavapipe, the software Vulkan rasteriser) gives a real
# surface on a real X server, and with it the port gets all the way to
# `Using framebuffer size 854x480 scale 1`. Neither needs a physical display.
# ---------------------------------------------------------------------------------------
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
export MPLBACKEND="${MPLBACKEND:-Agg}"
ulimit -c 0

# A DISPLAY that is *set* is not the same as a DISPLAY that *works*: this machine exports
# DISPLAY=:0 with no server on it, and SDL then fails with "x11 not available" - which looks
# like a missing driver and is actually a dead socket. So probe for a live server rather than
# trusting the variable, and start Xvfb when there is not one.
XVFB_PID=""
have_live_x11() { command -v xdpyinfo >/dev/null && xdpyinfo >/dev/null 2>&1; }
if ! have_live_x11; then
  if command -v Xvfb >/dev/null; then
    PROBE_DISPLAY=":${MP_PROBE_DISPLAY:-77}"
    Xvfb "$PROBE_DISPLAY" -screen 0 1280x720x24 >"$BUILD/xvfb.log" 2>&1 &
    XVFB_PID=$!
    sleep 3
    export DISPLAY="$PROBE_DISPLAY"
    echo "boot_probe: started Xvfb on $PROBE_DISPLAY (pid $XVFB_PID)"
  else
    export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
    echo "boot_probe: no Xvfb; falling back to $SDL_VIDEODRIVER, which cannot create a surface"
  fi
fi
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
# Prefer a real GPU. This is not a preference - it changes what the probe can see. With
# lavapipe forced, the run dies inside Aurora's surface setup *before* `main` ever calls
# `CMemorySys`, so **zero** reach-stubs are reached and the boot path's requirement list
# comes out empty. On this machine's NVIDIA card the same binary reaches 11 stubs and then
# faults in `CGameAllocator::DumpAllocations`. An empty requirement list is indistinguishable
# from a broken probe, so lavapipe is the fallback, not the default.
if [ -z "${VK_ICD_FILENAMES:-}" ] && [ -f /usr/share/vulkan/icd.d/nvidia_icd.json ]; then
  echo "boot_probe: a hardware Vulkan ICD is present; leaving it alone"
elif [ -f /usr/share/vulkan/icd.d/lvp_icd.json ]; then
  export VK_ICD_FILENAMES="${VK_ICD_FILENAMES:-/usr/share/vulkan/icd.d/lvp_icd.json}"
  export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
  echo "boot_probe: no hardware ICD; falling back to Mesa lavapipe (software Vulkan)"
fi

# The game disc, if this machine has one. The port stops with "no disc image given"
# without it, and that message is the port working correctly - it is asking for the input
# it needs rather than crashing.
if [ -z "${MP2_DISC:-}" ]; then
  for cand in "$REPO_ROOT"/../MetroidPrime2.iso \
              /run/media/*/Portable/roms/gc/Metroid*Prime*2*.iso \
              /run/media/*/*/Portable/roms/gc/Metroid*Prime*2*.iso \
              /run/media/*/roms/gc/Metroid*Prime*2*.iso \
              /run/media/*/*/roms/gc/Metroid*Prime*2*.iso; do
    [ -f "$cand" ] && { export MP2_DISC="$cand"; break; }
  done
fi
[ -n "${MP2_DISC:-}" ] && echo "boot_probe: disc image: $MP2_DISC" \
                       || echo "boot_probe: NO disc image found - the port will stop and say so"

# MP_PROBE_RUNNER runs the binary through a wrapper (the goal loop's boot-progress judge runs it
# under gdb, for a backtrace on a hang as well as on a crash); MP_PROBE_TIMEOUT widens the ceiling.
PROBE_TIMEOUT="${MP_PROBE_TIMEOUT:-120}"
RUN=("$BIN"); [ -n "${MP_PROBE_RUNNER:-}" ] && RUN=("$MP_PROBE_RUNNER" "$BIN")
echo "boot_probe: running (timeout ${PROBE_TIMEOUT}s${MP_PROBE_RUNNER:+, through $MP_PROBE_RUNNER})"
OUT="$BUILD/run.log"
timeout -k 10 "$PROBE_TIMEOUT" "${RUN[@]}" > "$OUT" 2>&1
rc=$?

# **A display that answers `xdpyinfo` is not a display Aurora can open a window on.**
# That check above (`have_live_x11`) asks X, not SDL, and the two disagree: on this machine
# `DISPLAY=:0` has a live Xwayland that `xdpyinfo` queries happily, while Aurora's SDL init
# still fails with "x11 not available" - so the Xvfb branch never ran and the probe died at
# window creation with **zero reach-stubs reached**.
#
# **Zero stubs is indistinguishable from a probe that cannot see**, which is the same trap the
# lavapipe note below describes: an empty requirement list is not a result. So: if the run
# produced no stub at all *and* Aurora reported it could not initialise SDL, retry once under a
# freshly started Xvfb. One retry, on the real display first, so the richer signal is preferred.
if [ "$(grep -c '^\[reach-stub' "$OUT" 2>/dev/null)" -eq 0 ] \
   && grep -q "Error initializing SDL" "$OUT" 2>/dev/null; then
  if [ -z "$XVFB_PID" ] && command -v Xvfb >/dev/null; then
    PROBE_DISPLAY=":${MP_PROBE_DISPLAY:-78}"
    Xvfb "$PROBE_DISPLAY" -screen 0 1280x720x24 >"$BUILD/xvfb-retry.log" 2>&1 &
    XVFB_PID=$!
    sleep 3
    export DISPLAY="$PROBE_DISPLAY"
    export SDL_VIDEODRIVER=x11
    echo "boot_probe: Aurora could not open a window on the current display and no stub was" >&2
    echo "  reached, which is not a measurement. Retrying under Xvfb on $PROBE_DISPLAY." >&2
    timeout -k 10 "$PROBE_TIMEOUT" "${RUN[@]}" > "$OUT" 2>&1
    rc=$?
  fi
fi
echo "boot_probe: exit status $rc"
echo "--- last 40 lines of output ---"
tail -40 "$OUT"

if [ $rc -ge 128 ]; then
  sig=$((rc - 128))
  echo "boot_probe: died on signal $sig"
fi

# ---------------------------------------------------------------------------------------
# WHAT THIS PROBE IS, AND WHY IT STUBS
# ---------------------------------------------------------------------------------------
#
# `-DMP_BOOT_STUBS=ON` adds `src/MetroidPrime/PortReachStubs.cpp`, generated by
# `tools/gen_link_stubs.py --reachable`, which defines the undefined symbols the boot path *does*
# reach. **This is not the port**: those are lies, and the program will run and then behave
# wrongly. `tools/check_boot_stubs.py` fails if that option is ever on in a build whose undefined
# count anyone would believe.
#
# **It replaced `--warn-unresolved-symbols`, and the reason is that the flag produced a binary
# that could not be trusted past its first unresolved call.** With the flag, each unresolved
# symbol got a PLT slot with no GOT entry and no stub - sixteen zero bytes - and the call landed
# at `plt+16`:
#
#   0x55555563ac47  call  0x555555616cf0 <__cxa_throw_bad_array_new_length@plt+16>
#   $ objdump -d .../__cxa_throw_bad_array_new_length@plt
#     00000000000c2cf0 <__cxa_throw_bad_array_new_length@plt+0x10>:
#       ...
#
# so the first crash named a hole rather than a defect, and I nearly committed it as a port bug.
# With the stubs the link is honest - no holes - and each stub **logs its own name on entry**:
#
#   [reach-stub 0007] _ZN14CGameAllocator10InitializeER10COsContext
#   [reach-stub 0008] _ZN9CGameStateC1ER11CInputStreami
#
# **That log is the point.** `link_reach.py` says which symbols are reachable; only running the
# program says which it reaches *first, and in what order* - and that order is what tells a lane
# what to write next. A non-void function stubbed as `void()` returns whatever was in the return
# register, so the port may fault later than the first stub; the log up to the fault is still
# ordered evidence, and the fault is the next thing to find.
# ---------------------------------------------------------------------------------------
echo "boot_probe: the reachability stubs are DIAGNOSTIC - this is not the port"
# Leave the virtual display behind only if this tool started it.
[ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null
echo "boot_probe: build log $LOG, run log $OUT"
