#!/bin/bash
# Syntax-check the port's sources **and link the port's real executable**.
#
#   tools/probe_sources.sh [-v] [--no-link]
#
# Compiles every game translation unit listed in files.cmake and every
# engine-independent platform source in the mp_platform target, mirroring
# CMakeLists.txt: C++ sources get platform/compat.h force-included, the bundled C
# sources (LZO) and glibc_compat.c do not. Per-file logs land in the directory
# named by PROBE_OUT (default: build/probe-logs).
#
# Then it runs `tools/link_check.sh --strict`: the port's own executable, linked by the
# same objects, libraries and flags the shipping build uses, with the verdict *the link
# resolved*. This is the second half of the sweep on purpose.
#
# **Why it links.** The sweep used to compile and stop, and printed
# `probe: 648 files, 0 failed, 0 errors` on a port that `ld` could not link. That line
# read - to me, to `tools/gate.sh`, and to anyone skimming a commit - as "the port builds",
# and it was wrong: 648 translation units is not a binary. A landed vtable turned 23
# undefined references into 64 and nothing reported it. A compile-only check over a target
# whose failure mode is at the link is **a gate that cannot fail**, and that is the exact
# shape of the mistake in PROCESS_LESSONS.md 1 and 22. So the link is now in the same
# summary line as the compile, in the same exit status, and it names the symbols.
#
# `--no-link` compiles only. It exists for the case where there is no toolchain to link
# with; **the gate never passes it**, because a gate step that is skipped on purpose is
# the same hole wearing a different hat.
#
# This is the sweep the port's shim queue is measured with; the full signed-char
# build with Aurora linked is cmake --build.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

OUT_DIR="${PROBE_OUT:-$REPO_ROOT/build/probe-logs}"
VERBOSE=0
LINK=1
for arg in "$@"; do
  case "$arg" in
    -v) VERBOSE=1 ;;
    --no-link) LINK=0 ;;
    -h|--help) sed -n '2,31p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "probe: unknown argument $arg" >&2; exit 2 ;;
  esac
done

COMMON="-fsigned-char -DTARGET_PC -DAURORA -Wno-narrowing -Wno-multichar -Wno-write-strings -Wno-trigraphs"
COMMON="$COMMON -Iplatform/include -Iextern/aurora/include -Iextern/musyx-port/include -Iinclude -Iinclude/LZO"
# mp_game links musyx, whose CMake target exports MUSY_TARGET PUBLIC. Upstream's audio headers
# include musyx/musyx.h, which otherwise defaults to the Dolphin typedefs (u32 = unsigned long).
COMMON="$COMMON -DMUSY_TARGET=MUSY_TARGET_PC"
CXX_FLAGS="-std=c++20 $COMMON -include platform/compat.h"
C_FLAGS="-std=gnu11 $COMMON"
PLATFORM_CXX_FLAGS="-std=c++20 $COMMON"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/*.log "$OUT_DIR/status.txt" "$OUT_DIR/files.txt"

# Game sources: the explicit manifest.
grep -oP '^\s+\Ksrc/\S+$' files.cmake | sort -u > "$OUT_DIR/files.txt"

# Platform sources: the mp_platform and mp_port_entry target lists in CMakeLists.txt.
awk '
  /add_library\(mp_platform OBJECT|add_library\(mp_port_entry OBJECT/ { collecting = 1 }
  collecting { print }
  collecting && /\)/ { collecting = 0 }
' CMakeLists.txt | grep -oP 'platform/\S+' | sed 's/)$//' | sort -u >> "$OUT_DIR/files.txt"
sort -u -o "$OUT_DIR/files.txt" "$OUT_DIR/files.txt"

compile_one() {
  local file="$1"
  local log="$OUT_DIR/$(echo "$file" | tr '/' '_').log"
  case "$file" in
    src/LZO/*.c | platform/glibc_compat.c) gcc $C_FLAGS -fsyntax-only "$file" >"$log" 2>&1 ;;
    platform/*) g++ $PLATFORM_CXX_FLAGS -fsyntax-only "$file" >"$log" 2>&1 ;;
    *) g++ $CXX_FLAGS -fsyntax-only "$file" >"$log" 2>&1 ;;
  esac
  echo "$file $?" >> "$OUT_DIR/status.txt"
}
export -f compile_one
export CXX_FLAGS C_FLAGS PLATFORM_CXX_FLAGS OUT_DIR

xargs -a "$OUT_DIR/files.txt" -P"$(nproc)" -I{} bash -c 'compile_one "$@"' _ {}

total=$(wc -l < "$OUT_DIR/files.txt")
failed=$(awk '$2 != 0' "$OUT_DIR/status.txt" | wc -l)
# The link log lives in this directory too, and `ld` and the compiler both print "error:",
# so it is excluded by name rather than by ordering. Every compile log is a source path with
# "/" turned into "_", so it always ends in .cpp.log or .c.log and can never be this name.
LINK_LOG="$OUT_DIR/link_check.log"
compile_logs=$(ls "$OUT_DIR"/*.log 2>/dev/null | grep -v '/link_check\.log$')
# An empty `grep file...` list would read stdin and hang, and this is a gate.
errors=0
[ -z "$compile_logs" ] || errors=$(grep -h "error:" $compile_logs 2>/dev/null | wc -l)

# The link. `tools/link_check.sh --strict` is the same executable, the same objects, the
# same libraries and the same flags the shipping build uses, and its exit status says
# whether it resolved. It is also single-flight: if `tools/gate.sh` is already building
# that link in the background, this waits for that build and reuses it rather than paying
# for the port build twice, so the gate does not get slower by much more than the residual
# wait.
#
# The toolchain is resolved here rather than left to link_check.sh's default, because the
# gate names it MP_TOOLCHAIN_DIR and link_check.sh reads MP_TOOLCHAIN: a probe that cannot
# find the toolchain must say so, not quietly report a compile-only pass.
TOOLCHAIN="${MP_TOOLCHAIN:-}"
[ -n "$TOOLCHAIN" ] || TOOLCHAIN="${MP_TOOLCHAIN_DIR:+$MP_TOOLCHAIN_DIR/build/review-tools}"
[ -n "$TOOLCHAIN" ] || TOOLCHAIN="$REPO_ROOT/../MetroidPrimePort/build/review-tools"
link_rc=0
if [ "$LINK" = 0 ]; then
  # A compile-only run is a legitimate thing to ask for and a dishonest thing for a gate
  # to do, so the line says which of the two happened.
  link_state="SKIPPED (--no-link) - a compile-only result, not a build"
else
  MP_TOOLCHAIN="$TOOLCHAIN" ./tools/link_check.sh --strict >"$LINK_LOG" 2>&1
  link_rc=$?
  undef=$(sed -n 's/^link_check: unique undefined symbols \([0-9][0-9]*\).*/\1/p' "$LINK_LOG" | head -1)
  dups=$(sed -n 's/^link_check: duplicate definitions *\([0-9][0-9]*\).*/\1/p' "$LINK_LOG" | head -1)
  [ -n "$undef" ] || undef="?"
  [ -n "$dups" ] || dups="?"
  if [ "$link_rc" = 0 ]; then
    link_state="LINKED ($undef undefined, $dups duplicates)"
  elif [ "$link_rc" = 2 ]; then
    # link_check.sh could not run at all - no toolchain, a failed configure. That is not
    # the same claim as "the link failed", and conflating them would make a missing
    # toolchain read as a good link. Both fail; they are reported differently.
    link_state="LINK NOT RUN (link_check.sh could not run; see $LINK_LOG)"
  else
    link_state="NOT LINKED ($undef undefined, $dups duplicates)"
  fi
fi

# One line, both halves. A reader must not be able to get to "the port builds" from a
# line that only says the sources compiled.
printf 'probe: %d files, %d failed, %d errors; link: %s (logs in %s)\n' \
  "$total" "$failed" "$errors" "$link_state" "$OUT_DIR"

if [ "$failed" != 0 ]; then
  awk '$2 != 0 { print "  " $1 }' "$OUT_DIR/status.txt"
  [ "$VERBOSE" = 1 ] && [ -n "$compile_logs" ] && grep -h "error:" $compile_logs | head -40
  exit 1
fi

if [ "$LINK" = 1 ] && [ "$link_rc" != 0 ]; then
  # Name them. A count on its own is not something a lane can close; the list is.
  grep -E '^  UNDEF |^  DUP ' "$LINK_LOG" | head -60
  grep -qE '^  UNDEF |^  DUP ' "$LINK_LOG" || tail -12 "$LINK_LOG" | sed 's/^/  /'
  echo "  full list: $LINK_LOG"
  exit 1
fi
exit 0
