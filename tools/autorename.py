#!/usr/bin/env python3
"""Rename every byte-identical `fn_`-named function of a unit after our own symbol.

    python3 tools/autorename.py Kyoto/CPakFile

Uses tools/fnmap.py to pair the functions and tools/apply_rename.py to write the names into
config/G2ME01/symbols.txt. This is the mechanical half of porting a unit; it converted nine
functions of CPakFile from 0% to 100% in one call. Review what it prints, then build and measure.

From lane W16, 2026-09-25.
"""
import re, subprocess, sys, collections

o = sys.argv[1]
txt = subprocess.run(['python3', 'tools/fnmap.py', o], capture_output=True, text=True).stdout
pairs = []
for line in txt.splitlines():
    m = re.match(r'^(\S+)\s+\d+\s+IDENTICAL: (.+)$', line.strip())
    if m and m.group(1).startswith(('fn_', 'lbl_')):
        pairs.append((m.group(1), m.group(2).strip()))
if not pairs:
    print('no renames')
    sys.exit(0)
inp = ''.join(f'{a} = {b}\n' for a, b in pairs)
print(inp, end='')
subprocess.run(['python3', 'tools/apply_rename.py'], input=inp, text=True)
