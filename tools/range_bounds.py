#!/usr/bin/env python3
"""List .text symbols in [s,e) from the linked retail elf; flag boundary problems."""
# Reads build/G2ME01/main.elf, so it answers this for DOL ranges; a REL unit's range lives in the
# module's own object, not the DOL. 2026-09-25: the boundary test compared hex(s) (a string) against
# a set of ints and so always printed False.
import subprocess, sys

out = subprocess.run(['build/binutils/powerpc-eabi-nm', '-n', 'build/G2ME01/main.elf'],
                     capture_output=True, text=True).stdout
syms = []
for line in out.splitlines():
    parts = line.split()
    if len(parts) == 3:
        addr, typ, name = parts
        syms.append((int(addr, 16), typ, name))
    elif len(parts) == 2:
        addr, name = parts
        syms.append((int(addr, 16), '?', name))
syms.sort()
addrs = {a for a, t, n in syms}

s, e = int(sys.argv[1], 16), int(sys.argv[2], 16)
print('start_is_symbol:', s in addrs, 'end_is_symbol:', e in addrs)
inside = [(a, t, n) for a, t, n in syms if s <= a < e]
for a, t, n in inside:
    print(f'  {a:08X} {t} {n}')
print(f'total symbols in range: {len(inside)}')
# what lies at the boundaries
for probe in (s - 4, e, e + 4):
    for a, t, n in syms:
        if a == probe:
            print(f'  boundary {probe:08X} -> {n}')
