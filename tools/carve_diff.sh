#!/usr/bin/env python3
"""Diff a compiled carve's .text against retail's bytes for the same range.

    tools/carve_diff.sh <retail_start> <retail_size> <ours.o> [symbol]

Prints the instruction count and byte count on each side and the first differing
instruction, so a candidate body is measured rather than eyeballed. Byte-exactness is the
acceptance test for a `Matching` carve, so "differs at" is the only verdict that matters.
"""
import re
import subprocess
import sys

BIN = "build/binutils/powerpc-eabi-objdump"
START = int(sys.argv[1], 16)
SIZE = int(sys.argv[2], 16)
OURS = sys.argv[3]
SYM = sys.argv[4] if len(sys.argv) > 4 else None

INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
LABEL = re.compile(r"^\s*([0-9a-f]+) <([^>]+)>:")


def read(path, start=None, stop=None):
    cmd = [BIN, "-d", path]
    if start is not None:
        cmd[2:2] = ["--start-address=%#x" % start, "--stop-address=%#x" % stop]
    out = []
    for line in subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines():
        m = LABEL.match(line)
        if m:
            continue
        m = INSN.match(line)
        if m:
            out.append((int(m.group(1), 16), m.group(2).replace(" ", ""),
                        m.group(3), m.group(4).strip()))
    return out


retail = read("build/G2ME01/main.elf", START, START + SIZE)
ours = read(OURS)
if SYM:
    ours = [i for i in ours if True]
print("retail: %d instructions, %d bytes" % (len(retail), len(retail) * 4))
print("ours  : %d instructions, %d bytes" % (len(ours), len(ours) * 4))
bad = 0
for n in range(max(len(retail), len(ours))):
    a = retail[n] if n < len(retail) else None
    b = ours[n] if n < len(ours) else None
    if a is None or b is None or a[1] != b[1]:
        bad += 1
        if bad <= 6:
            print("  +%-3d retail: %-34s ours: %s" % (
                n, ("%08x %-10s %s" % (a[0], a[2], a[3])) if a else "<none>",
                ("%08x %-10s %s" % (b[0], b[2], b[3])) if b else "<none>"))
print("differing instructions: %d" % bad)
print("BYTE-EXACT" if bad == 0 and len(retail) == len(ours) else "NOT byte-exact")
