#!/bin/bash
# tools/dump_fn_relocs.sh <retail-addr> [retail-addr...]
#
# The relocations of one function of retail's DOL, read out of the object `dtk dol split` made
# for it, with both the object's .text offset and the retail address shown.
#
#   tools/dump_fn_relocs.sh 0x80144140
#
# This is the tool that settles every "which symbol is this displacement" question in
# `docs/research/`, and it exists because guessing is how `cgamestate_layout.md` came to call
# `-28376(r13)` a "gpTweakGame at 0x80418EF0" when the object says `gpSimplePool` and the
# arithmetic says 0x80418EA8. **The relocations are retail's own statement of what the code
# refers to; nothing else in the tree is.**
#
# How it finds the object: by asking every `build/G2ME01/obj/*.o` which one defines the symbol,
# so it works for any function without a hard-coded file name. The object's base address comes
# from its file name for an `auto_*` split (`auto_03_80142A30_text.o` starts at 0x80142A30) and
# from `config/G2ME01/splits.txt` for a unit object. `nm` over ~1400 objects is a second or two.
#
# **A function's retail relocations only exist while no unit claims its range.** `dtk dol split`
# cuts each claimed range out of the `auto_*` objects into a *filled* `obj/<unit>.o` that
# carries **our** bytes and **our** relocations, so once a unit claims 0x80144140 the retail
# relocations of `fn_80144140` are gone from `build/G2ME01/obj/` and this tool reports the
# unit's own object instead. That is not a bug and it still prints, but the answer is then
# *ours*, so the tool says so rather than letting a reader take it as retail's. **Run it before
# adding the `splits.txt` entry, and record what it prints.**
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
NM=build/binutils/powerpc-eabi-nm
OBJDUMP=build/binutils/powerpc-eabi-objdump
[ -x "$NM" ] || { echo "error: $NM missing (see docs/LANE_BRIEFING.md)" >&2; exit 2; }
[ "$#" -ge 1 ] || { sed -n '2,23p' "$0"; exit 2; }

base_of() { # $1 = object path. The .text VMA dtk gave it.
  case "$(basename "$1")" in
    auto_*_text.o) printf '0x%s\n' "$(basename "$1" | sed -E 's/^auto_[0-9]+_([0-9A-F]+)_text\.o$/\1/')" ;;
    *)
      awk -v u="$(basename "$1" .o)" '
        $1 == u":" { seen = 1; next }
        seen && $1 == ".text" { print $3; exit }' config/G2ME01/splits.txt ;;
  esac
}

for sym in "$@"; do
  sym="$sym"
  name="$(echo "$sym" | sed 's/^0x//; s/^/fn_/')"
  obj=""
  for f in build/G2ME01/obj/*.o; do
    if "$NM" "$f" 2>/dev/null | awk -v n="$name" '$NF == n { found = 1 } END { exit !found }'; then
      obj="$f"; break
    fi
  done
  if [ -z "$obj" ]; then echo "== $name ($sym): no object in build/G2ME01/obj defines it"; continue; fi
  case "$obj" in
    build/G2ME01/obj/auto_*) ;;
    *) echo "== $name ($sym): NOTE $obj is a *unit* object, not an auto_* split, so these are"
       echo "           OUR relocations. A unit claims this range, so retail's are no longer"
       echo "           in build/G2ME01/obj - run this before adding the splits.txt entry." ;;
  esac
  base="$(base_of "$obj")"
  if [ -z "$base" ] || ! printf '%s' "$base" | grep -qE '^(0x)?[0-9A-Fa-f]+$'; then
    echo "== $name ($sym): found in $obj but its base address is unknown"; continue
  fi
  text_off="$("$OBJDUMP" -t "$obj" | awk -v n="$name" '$NF == n && $(NF-2) == ".text" { print $1; exit }')"
  [ -n "$text_off" ] || { echo "== $name ($sym): no .text symbol entry in $obj"; continue; }
  size=$(grep -E "^${name} = \.text:0x" config/G2ME01/symbols.txt | head -1 |
         sed -n 's/.*size:0x\([0-9a-fA-F]*\).*/\1/p')
  [ -n "$size" ] || size=512
  # `objdump -t` and symbols.txt both print hex without a 0x prefix, so they need the prefix
  # before `%d` - `printf '%d' 2c8` is 2028, not 0x2c8.
  printf -v base '%d' "$base"; printf -v text_off '%d' "0x$text_off"
  printf -v size '%d' "0x$size"; printf -v addr '%d' "$sym"
  echo "== $name  retail 0x$(printf '%X' $addr)..0x$(printf '%X' $(( addr + size )))  in $obj (base 0x$(printf '%X' $base))"
  # `off` is an offset into the *object*, so the window is s..s+size - not the retail address
  # range, which is a different number entirely.
  "$OBJDUMP" -r "$obj" | awk -v s="$text_off" -v a="$addr" -v e="$(( text_off + size ))" '
    /^RELOCATION/ { next }
    { off = strtonum("0x" $1) }
    $2 ~ /^R_PPC_/ && off >= s && off < e {
      printf "   +0x%03X  0x%08X  %-18s %s\n", off - s, a + (off - s), $2, $3 }'
  echo
done
