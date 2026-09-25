#!/usr/bin/env python3
"""Which unit (if any) claims a range, per section, from our splits.txt."""
import re, sys

SPLITS = 'config/G2ME01/splits.txt'
ranges = []  # (section, start, end, unit)
unit = None
for line in open(SPLITS):
    m = re.match(r'^(\S+):\s*$', line)
    if m:
        unit = m.group(1)
        continue
    m = re.match(r'^\s+(\S+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)', line)
    if m and unit:
        ranges.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), unit))

def owner(sec, s, e, strict=True):
    out = []
    for S, a, b, u in ranges:
        if S != sec:
            continue
        if s < b and a < e:
            if strict and a <= s and e <= b:
                return u
            out.append((hex(a), hex(b), u))
    return None if strict else out

if __name__ == '__main__':
    sec, s, e = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
    print(sec, hex(s), hex(e), '->', owner(sec, s, e) or 'UNCLAIMED',
          owner(sec, s, e, strict=False) if len(sys.argv) > 4 else '')
