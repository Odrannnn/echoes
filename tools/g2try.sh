#!/bin/bash
# Compile one unit with the matching build's own flags and print, per instruction,
# retail's bytes beside ours. Lane g2's variant loop: objdiff's byte percentage is
# dominated by relocated words, so it hides a one-instruction mistake.
#
#   tools/g2try.sh MetroidPrime/ScriptObjects/CUnknown90 <retail-range>
#
# With no range, retail's range is read out of config/G2ME01/splits.txt.
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
TC="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
UNIT="$1"
RANGE_ARG="${2:+$2 $3}"
OUT=/tmp/opencode/g2probe
mkdir -p "$OUT"
CFLAGS=$(python3 - "$UNIT" <<'PY'
import re, sys
unit = sys.argv[1]
txt = open('build.ninja').read()
key = 'build build/G2ME01/src/%s.o: ' % unit
i = txt.index(key)
seg = txt[i:i+4000]
m = re.search(r'cflags = (.*?)\n\s*\w+ = ', seg, re.S)
flags = m.group(1)
flags = re.sub(r'\$\n\s*', ' ', flags)
print(flags.strip())
PY
)
# shellcheck disable=SC2086
eval "\"$TC/build/tools/wibo\" \"$TC/build/compilers/GC/2.7/mwcceppc.exe\" $CFLAGS -c \"src/$UNIT.cpp\" -o \"$OUT/probe.o\"" || exit 1
echo "== ours: $(build/binutils/powerpc-eabi-objdump -h "$OUT/probe.o" | awk '/ .text/{print "text="strtonum("0x"$3)}') =="
echo "== undefined =="
build/binutils/powerpc-eabi-nm -u "$OUT/probe.o"
if [ -n "$RANGE_ARG" ]; then RSTART=$(echo $RANGE_ARG | cut -d' ' -f1); REND=$(echo $RANGE_ARG | cut -d' ' -f2)
else
RSTART=$(python3 - "$UNIT" <<'PY2'
import re, sys
unit = sys.argv[1]
cur = None
for line in open('config/G2ME01/splits.txt'):
    if line.startswith('\t'):
        m = re.match(r'\t\.text\s+start:(0x[0-9A-Fa-f]+) end:(0x[0-9A-Fa-f]+)', line)
        if m and cur == unit:
            print(m.group(1)); break
    elif line.rstrip().endswith(':'):
        cur = line.rstrip()[:-1]
PY2
)
REND=$(python3 -c "print(hex(int('$RSTART',16)+320))")
fi
RETOBJ="build/G2ME01/obj/$UNIT.o"
build/binutils/powerpc-eabi-objdump -d --start-address="$RSTART" --stop-address="$REND" "$RETOBJ" \
  | sed -e '1,3d' -e 's/^ *[0-9a-f]*:\t[0-9a-f ]*\t//' -e 's/\t/ /' -e 's/<[^>]*>//' -e 's/ *$//' > "$OUT/retail.txt"
build/binutils/powerpc-eabi-objdump -d "$OUT/probe.o" | sed -e '1,3d' \
  -e 's/^ *[0-9a-f]*:\t[0-9a-f ]*\t//' -e 's/\t/ /' -e 's/<[^>]*>//' -e 's/ *$//' \
  -e '/R_PPC_/d' -e 's/^[0-9a-f ]*: //' > "$OUT/ours.txt"
diff -u --label retail --label ours "$OUT/retail.txt" "$OUT/ours.txt" | head -80
echo "retail insns: $(wc -l < "$OUT/retail.txt")   ours: $(wc -l < "$OUT/ours.txt")"
