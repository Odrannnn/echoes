#!/bin/bash
# Compare a unit's retail-derived object with ours, for the sections the link uses.
#
#   tools/compare_unit.sh <unit>        e.g. Kyoto/CFrameDelayedKiller
#
# `ninja` links the DOL from build/G2ME01/obj/*.o (bytes split out of the retail
# binary) and only swaps in build/G2ME01/src/*.o once a unit is marked Matching in
# configure.py. So a unit is genuinely complete when our object is link-equivalent
# to the retail one: same code, and the same data sections with the same contents.
#
# .comment differs by construction (it holds the compiler's banner and options) and
# .note.split is a dtk artifact, so both are ignored here. Everything else has to
# agree, or switching the unit to Matching changes the linked DOL.
#
# This is a *diagnostic*, not the acceptance gate. It compares more than the link
# does: trailing gap padding between splits, and weak code the retail linker drops,
# show up here even in units that link identically (most units already marked
# Matching fail it for that reason). The authoritative test is to mark the unit
# Matching in configure.py and rebuild: the project's own check
# (config/G2ME01/build.sha1) then fails loudly if the linked DOL or any REL stops
# reproducing retail. Use this script to see *what* differs, and ninja to decide.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

UNIT="${1:?usage: tools/compare_unit.sh <unit>   (e.g. Kyoto/CFrameDelayedKiller)}"
OBJ="build/G2ME01/obj/$UNIT.o"
SRC="build/G2ME01/src/$UNIT.o"
OBJDUMP="build/binutils/powerpc-eabi-objdump"
NM="build/binutils/powerpc-eabi-nm"

for f in "$OBJ" "$SRC"; do
  [ -f "$f" ] || { echo "error: missing $f (build first: ./tools/decomp_build.sh)" >&2; exit 2; }
done
for t in "$OBJDUMP" "$NM"; do
  [ -x "$t" ] || { echo "error: missing $t" >&2; exit 2; }
done

IGNORE='\.comment|\.note\.split'

sections() {
  "$OBJDUMP" -h "$1" | awk '
    /^ *[0-9]+ / {
      name = $2
      if (name ~ /^(\.comment|\.note\.split)$/) next
      printf "%-24s size=%-10s align=%s\n", name, $3, $4
    }' | sort
}

echo "== sections (retail-derived vs ours), .comment/.note.split ignored =="
diff <(sections "$OBJ") <(sections "$SRC") && echo "   sections identical"

echo
echo "== section contents =="
status=0
# objdump -s prints a banner naming the file, so compare only the hex rows.
hexdump_section() { "$OBJDUMP" -s -j "$1" "$2" 2>/dev/null | grep -E '^ [0-9a-f]{4} '; }
for section in .text .ctors .dtors .rodata .data .sdata .sdata2 .bss .sbss; do
  hexdump_section "$section" "$OBJ" > /tmp/cmp_obj_$$
  hexdump_section "$section" "$SRC" > /tmp/cmp_src_$$
  if cmp -s /tmp/cmp_obj_$$ /tmp/cmp_src_$$; then
    echo "   $section: identical"
  else
    echo "   $section: DIFFERS"
    diff /tmp/cmp_obj_$$ /tmp/cmp_src_$$ | head -6 | sed 's/^/      /'
    status=1
  fi
done
rm -f /tmp/cmp_obj_$$ /tmp/cmp_src_$$

echo
echo "== symbols (retail-derived vs ours) =="
if diff <("$NM" "$OBJ" | awk '{print $2, $3}' | sort) \
        <("$NM" "$SRC" | awk '{print $2, $3}' | sort) > /tmp/cmp_sym_$$; then
  echo "   symbol tables identical"
else
  head -20 /tmp/cmp_sym_$$ | sed 's/^/   /'
fi
rm -f /tmp/cmp_sym_$$

exit $status
