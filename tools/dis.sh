#!/bin/bash
# tools/dis.sh <start_addr> <size> -- disassemble a retail byte range out of the linked ELF.
#
#   tools/dis.sh 0x8028C17C 0x7C        # CStopwatch::CSWData::Initialize
#
# Addresses and sizes are retail's and are read from `config/G2ME01/symbols.txt`; this only renders
# them, which `objdump -d` will not do for a range that starts or ends mid-symbol. Note that
# `symbols.txt`'s `size:` field is dtk's gap-to-the-next-symbol for unnamed functions, so the
# authoritative size for a named one is the difference to the next entry at or above it.
#
# For anything reached through `disp(r13)` or `disp(r2)`, resolve the displacement with
# tools/sda.py first - reading it against the wrong base gives a plausible wrong answer.
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
[ "$#" -eq 2 ] || { sed -n '2,13p' "$0"; exit 2; }
S=$1; N=$2
E=$(python3 -c "print(hex($S + $N))")
build/binutils/powerpc-eabi-objdump -d --start-address="$S" --stop-address="$E" \
  build/G2ME01/main.elf
