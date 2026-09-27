#!/bin/bash
# tools/goal_verify/port-pak-byteorder.sh - run from the tree being judged. Exit 0 = the port's
# CInputStream reads a retail (big-endian) stream correctly on this little-endian host.
#
# The port target of this item is an *inline* (`CInputStream::ReadInt32`), so it is never in the
# linker's undefined list and "the target is no longer undefined" is true before any work is done.
# That check could not fail; this one can. Measured on the branch head before the fix:
# `version = 0x05000300`, `BE_TEST FAIL (5)`. With the fix: `BE_TEST PASS`.
#
# It compiles the tree's real header and the real src/Kyoto/Streams/CInputStream.cpp with
# -DTARGET_PC, exactly as the port build does, so it tests the code that ships, not a copy.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/goal-verify-be.$$"
trap 'rm -f "$OUT"' EXIT
if ! g++ -std=c++17 -w -DTARGET_PC -Iinclude -o "$OUT" \
       "$HERE/be_input_stream.cpp" src/Kyoto/Streams/CInputStream.cpp; then
  echo "verify: the byte-order test does not compile against this tree"
  exit 1
fi
"$OUT"
