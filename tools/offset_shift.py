#!/usr/bin/env python3
"""For main/ functions just under 100%, report the offset delta between our object and retail.

  tools/offset_shift.py [unit-substring]

A class laid out wrongly by a constant shows up here: every differing operand is ours minus
retail by the same amount.  A one-instruction codegen difference shows a delta of 0.
"""
import json
import re
import subprocess
import sys

OD = 'build/binutils/powerpc-eabi-objdump'


def per_fn(path):
    out = subprocess.run([OD, '-d', '-r', path], capture_output=True, text=True).stdout
    cur, res = None, {}
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <(.+)>:$', line)
        if m:
            cur = m.group(1)
            res[cur] = []
            continue
        if cur is None:
            continue
        if re.match(r'^\s+[0-9a-f]+:\t[0-9a-f ]+\t', line):
            insn = re.sub(r'^\s+[0-9a-f]+:\t[0-9a-f ]+\t', '', line).split('//')[0].rstrip()
            m2 = re.search(r'(-?\d+)\((r\d+)\)', insn)
            if m2:
                res[cur].append(int(m2.group(1)))
    return res


def main():
    only = sys.argv[1] if len(sys.argv) > 1 else ''
    rep = json.load(open('build/report.json'))
    for u in rep['units']:
        if u['metadata'].get('module_name') != 'main' or u['metadata'].get('auto_generated'):
            continue
        if only and only not in u['name']:
            continue
        short = u['name'][5:]
        retail, ours = per_fn('build/G2ME01/obj/%s.o' % short), per_fn('build/G2ME01/src/%s.o' % short)
        rows = []
        for f in u.get('functions', []):
            p = float(f.get('fuzzy_match_percent') or 0)
            if p >= 100.0 or p < 90.0:
                continue
            a, b = retail.get(f['name']), ours.get(f['name'])
            if not a or not b or len(a) != len(b):
                continue
            deltas = sorted(set(y - x for x, y in zip(a, b)))
            if deltas == [0]:
                continue
            rows.append((p, f['name'], deltas[:6], len(a)))
        if rows:
            print('== %s' % short)
            for p, n, d, c in sorted(rows):
                print('   %6.2f%% %3d insn  deltas %-22s %s' % (p, c, d, n[:64]))


if __name__ == '__main__':
    main()
