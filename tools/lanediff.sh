#!/bin/bash
# Disassemble one function from the retail-derived object and from our compile, side by
# side, with the addresses and relocation operands stripped so only real differences show.
#
#   tools/lanediff.sh <unit> [symbol]      e.g. tools/lanediff.sh Kyoto/CPakFile GetResInfo
#
# `<unit>` is the unit's path under build/G2ME01/src; the retail object is found wherever
# dtk put it (build/G2ME01/obj/... for the DOL, build/G2ME01/<Module>/obj/... for a REL).
# With no symbol, the first text symbol of the retail object is used.
#
# This is the diagnostic behind `tools/compare_unit.sh`: same idea, but one function at a
# time, which is what you want when the unit is at 99% and the question is which two
# instructions differ.
#
# Mined from a lane's private helper (/tmp/opencode/w1) and generalised: no hardcoded
# module, and the retail object is located rather than assumed.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
OBJDUMP=build/binutils/powerpc-eabi-objdump
NM=build/binutils/powerpc-eabi-nm

[ "$#" -gt 0 ] || { sed -n '2,11p' "$0"; exit 2; }
UNIT="$1"
SYMBOL="${2:-}"
[ -x "$OBJDUMP" ] || { echo "error: $OBJDUMP missing (see docs/LANE_BRIEFING.md)" >&2; exit 1; }

find_retail() {
  local base hits
  base="$(basename "$UNIT")"
  hits=$(find build/G2ME01 -path "*/obj/*" -name "$base.o" 2>/dev/null)
  [ -n "$hits" ] || { echo "error: no retail object for $UNIT" >&2; exit 1; }
  # exact unit-path match first, then the shortest path
  echo "$hits" | grep -x "build/G2ME01/obj/$UNIT.o" && return 0
  echo "$hits" | awk '{ print length, $0 }' | sort -n | head -1 | cut -d' ' -f2-
}

RETAIL="$(find_retail)"
OURS="build/G2ME01/src/$UNIT.o"
[ -f "$OURS" ] || { echo "error: $OURS not built" >&2; exit 1; }

if [ -z "$SYMBOL" ]; then
  SYMBOL=$("$NM" "$RETAIL" | awk '/ [Tt] /{print $3; exit}')
  [ -n "$SYMBOL" ] || { echo "error: no text symbol in $RETAIL" >&2; exit 1; }
fi

clean() {
  sed -e 's/^[0-9a-f]* <\(.*\)>:/<\1>:/' \
      -e 's/^ *[0-9a-f]*:\t[0-9a-f ]*\t/    /' \
      -e 's/^\t*[0-9a-f]*: \(R_PPC_[A-Z0-9]*\)/    \1/' \
      -e 's/^\( *\)\(b\|bl\|b\.[a-z]*\|beq\|bne\|blt\|bgt\|ble\|bge\|bso\|bns\)\b.*/\1\2 <target>/'
}

diff -u <("$OBJDUMP" -d -r --disassemble="$SYMBOL" "$RETAIL" 2>/dev/null | sed -n "/<$SYMBOL>:/,/^\$/p" | clean) \
        <("$OBJDUMP" -d -r --disassemble="$SYMBOL" "$OURS"   2>/dev/null | sed -n "/<$SYMBOL>:/,/^\$/p" | clean) \
  || true
