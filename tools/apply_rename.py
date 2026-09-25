#!/usr/bin/env python3
"""Rename symbols in config/G2ME01/symbols.txt from `old=new` pairs on stdin.

    printf 'fn_80096EBC = AcceptScriptMsg__3CAiFR13CStateManagerRC10CScriptMsg\n' | python3 tools/apply_rename.py

Exact-name replacement only, each name once, and it prints any name it could not find rather than
failing silently. A rename must REPLACE its line, never be inserted beside it: two symbols on one
line is a dtk parse error, and a dropped rename leaves the address unnamed and scoring 0.00%, which
reads exactly like wrong code.

From lane W16, 2026-09-25.
"""
import re, sys

path = 'config/G2ME01/symbols.txt'
lines = open(path).read().splitlines(keepends=True)
renames = dict(l.strip().split('=', 1) for l in sys.stdin if l.strip() and '=' in l)
renames = {k.strip(): v.strip() for k, v in renames.items()}
seen = set()
for i, line in enumerate(lines):
    m = re.match(r'^(\S+) = ', line)
    if m and m.group(1) in renames:
        new = renames[m.group(1)]
        lines[i] = line.replace(m.group(1) + ' = ', new + ' = ', 1)
        seen.add(m.group(1))
missing = set(renames) - seen
open(path, 'w').writelines(lines)
print(f'renamed {len(seen)}/{len(renames)}', 'MISSING: ' + ','.join(sorted(missing)) if missing else '')
