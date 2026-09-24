#!/bin/bash
# Syntax-check the port's sources without a full configure.
#
#   tools/probe_sources.sh [-v]
#
# Compiles every game translation unit listed in files.cmake and every
# engine-independent platform source in the mp_platform target, mirroring
# CMakeLists.txt: C++ sources get platform/compat.h force-included, the bundled C
# sources (LZO) and glibc_compat.c do not. Per-file logs land in the directory
# named by PROBE_OUT (default: build/probe-logs).
#
# This is the sweep the port's shim queue is measured with; the full signed-char
# build with Aurora linked is cmake --build.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

OUT_DIR="${PROBE_OUT:-$REPO_ROOT/build/probe-logs}"
VERBOSE=0
[ "${1:-}" = "-v" ] && VERBOSE=1

COMMON="-fsigned-char -DTARGET_PC -DAURORA -Wno-narrowing -Wno-multichar -Wno-write-strings -Wno-trigraphs"
COMMON="$COMMON -Iplatform/include -Iextern/aurora/include -Iextern/musyx/include -Iinclude -Iinclude/LZO"
CXX_FLAGS="-std=c++20 $COMMON -include platform/compat.h"
C_FLAGS="-std=gnu11 $COMMON"
PLATFORM_CXX_FLAGS="-std=c++20 $COMMON"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/*.log "$OUT_DIR/status.txt" "$OUT_DIR/files.txt"

# Game sources: the explicit manifest.
grep -oP '^\s+\Ksrc/\S+$' files.cmake | sort -u > "$OUT_DIR/files.txt"

# Platform sources: the mp_platform target's list in CMakeLists.txt.
sed -n '/add_library(mp_platform OBJECT/,/)/p' CMakeLists.txt |
  grep -oP 'platform/\S+' | sed 's/)$//' | sort -u >> "$OUT_DIR/files.txt"

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
errors=$(grep -h "error:" "$OUT_DIR"/*.log 2>/dev/null | wc -l)
printf 'probe: %d files, %d failed, %d errors (logs in %s)\n' \
  "$total" "$failed" "$errors" "$OUT_DIR"

if [ "$failed" != 0 ]; then
  awk '$2 != 0 { print "  " $1 }' "$OUT_DIR/status.txt"
  [ "$VERBOSE" = 1 ] && grep -h "error:" "$OUT_DIR"/*.log | head -40
  exit 1
fi
