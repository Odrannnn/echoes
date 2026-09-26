#!/usr/bin/env python3
"""Show the differing instructions between the retail object and ours for one function.

  tools/dol_fd.py <unit> <symbol-substring> [<symbol-substring> ...]

For every matching function name, disassemble both objects, strip addresses / branch
targets / relocation addends, and print only the lines that differ, with a count.
"""
import difflib
import re
import subprocess
import sys

OBJDUMP = "build/binutils/powerpc-eabi-objdump"
NM = "build/binutils/powerpc-eabi-nm"


def funcs(path):
    out = subprocess.run([OBJDUMP, "-d", "-r", path], capture_output=True, text=True).stdout
    res = {}
    cur = None
    for line in out.splitlines():
        m = re.match(r'^[0-9a-f]+ <(.+)>:$', line)
        if m:
            cur = m.group(1)
            res[cur] = []
            continue
        if cur is None:
            continue
        if re.match(r'^\s+[0-9a-f]+:\t[0-9a-f ]+\t', line):
            txt = re.sub(r'^\s+[0-9a-f]+:\t[0-9a-f ]+\t', '', line)
            # strip the `NNN <sym+0xNN>` annotation objdump puts on branch targets
            txt = re.sub(r'\s+[0-9a-f]+ <[^>]*>\s*$', '', txt)
            txt = re.sub(r'\b\d+\s*\(', 'OFFSET(', txt)
            res[cur].append(txt.rstrip())
    return res


def main():
    unit, syms = sys.argv[1], sys.argv[2:]
    ours = "build/G2ME01/src/%s.o" % unit
    retail = "build/G2ME01/obj/%s.o" % unit
    a, b = funcs(retail), funcs(ours)
    for pat in syms:
        hits = [k for k in a if pat in k and k in b]
        if not hits:
            print("== %s: no paired function matching %r (retail %d, ours %d)" % (unit, pat, len(a), len(b)))
            continue
        for h in hits:
            ra, rb = a[h], b[h]
            diff = [l for l in difflib.unified_diff(ra, rb, 'retail', 'ours', lineterm='', n=2)][2:]
            print("== %s (%d retail insns, %d ours, %d differing lines)" % (h, len(ra), len(rb), len(diff)))
            for l in diff:
                print("   " + l)


if __name__ == '__main__':
    main()
