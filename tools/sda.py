#!/usr/bin/env python3
"""Resolve a `lwz rX,<off>(r13)` / `stw rX,<off>(r13)` operand to a retail address and name.

_SDA_BASE_ for G2ME01 is 0x8041FD80 and _SDA2_BASE_ is 0x804223C0, both measured out of
build/G2ME01/main.elf. Reading one of these against the wrong base gives a plausible wrong
answer, so this is the only supported way to do it.
"""
import bisect, re, sys

SDA = 0x8041FD80
SDA2 = 0x804223C0

def load_syms(path="config/G2ME01/symbols.txt"):
    out = []
    for line in open(path):
        m = re.match(r"^(\S+) = \.(\w+):(0x[0-9A-Fa-f]+)", line)
        if m:
            out.append((int(m.group(3), 16), m.group(1), m.group(2)))
    out.sort()
    return out

def name_of(syms, addr):
    i = bisect.bisect_right([s[0] for s in syms], addr) - 1
    if i < 0:
        return None
    base, nm, sec = syms[i]
    if sec in ("text", "init", "extab", "extabindex", "rodata", "data", "sdata", "sdata2", "ctors", "dtors"):
        return None if base != addr else "%s (exact, .%s)" % (nm, sec)
    return "%s (in .%s, +0x%X)" % (nm, sec, addr - base)

if __name__ == "__main__":
    syms = load_syms()
    for a in sys.argv[1:]:
        if a.startswith("s2:"):
            base, off = SDA2, int(a[3:], 0)
        elif a.startswith("s:"):
            base, off = SDA, int(a[2:], 0)
        else:
            base, off = SDA, int(a, 0)
        addr = base + off
        print("%+d -> 0x%08X  %s" % (off, addr, name_of(syms, addr) or "<unnamed>"))
