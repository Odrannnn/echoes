#!/usr/bin/env python3
"""One compiled function against a retail byte range, instruction by instruction.

  tools/bytescmp.py <obj.o> <symbol-substring> <retail_addr> <size>

Prints only the differing instructions, with a count. **This ranks register-allocation
arguments; objdiff's percentage cannot**, because the percentage is size-dominated and a
one-instruction difference in a four-instruction function reads the same as one in 156 bytes.
`tools/try_batch.py` ranks the same way over whole bodies.

Two things it does that the obvious `objdump | diff` does not:

  * it reads the **retail DOL**, not `build/G2ME01/main.elf`. dtk fills an unclaimed `.text`
    range with retail's bytes, so `tools/dis.sh` is retail for code, but the linked ELF is *our*
    build everywhere else and its `.data`/`.rodata` answers nothing. `tools/dol_read.py` is the
    same idea for raw bytes.
  * it says which bytes are **relocations**, because a relocated field is filled by the linker
    and is not evidence. It counts them as differences, so read the count with that in mind:
    a `bl` to an external symbol always differs from retail's `bl` to the same symbol, and
    objdiff ignores those fields.

**It does not reproduce objdiff's verdict.** It compares raw bytes; objdiff compares the
relocation-free bytes and reports per-function fuzzy percentages. Use it to decide *what to
try next*, and `tools/unit_fit.sh` plus `tools/flip_test.sh` to decide whether the unit is done.
"""
import re
import subprocess
import sys

OBJDUMP = "build/binutils/powerpc-eabi-objdump"
DOL = "orig/G2ME01/sys/main.dol"
# The DOL's section table, as `dtk dol info` prints it. A DOL has no section headers.
SECTIONS = {
    ".init": (0x80003100, 0x534, 0x100),
    "extab": (0x80003640, 0x10C, 0x3A22A0),
    "extabindex": (0x80003760, 0xE0, 0x3A23C0),
    ".text": (0x80003840, 0x3A1C54, 0x640),
    ".ctors": (0x803A54A0, 0x1F0, 0x3A24A0),
    ".dtors": (0x803A56A0, 0xC, 0x3A26A0),
    ".rodata": (0x803A56C0, 0xB530, 0x3A26C0),
    ".data": (0x803B0C00, 0x14E10, 0x3ADC00),
    ".sdata": (0x80417D80, 0x1104, 0x3C2A20),
    ".sdata2": (0x8041A3C0, 0x54C0, 0x3C3B40),
}


def one_function(path, needle):
    """The instructions of the *first* function whose objdump name contains `needle`."""
    out = subprocess.run([OBJDUMP, "-d", "-r", path], capture_output=True, text=True).stdout
    cur, res, found = None, [], None
    for line in out.splitlines():
        m = re.match(r"^\s*[0-9a-f]* <(.+)>:$", line)
        if m:
            # A new function starts: if the one being collected is the one asked for, keep it.
            if cur is not None and needle in cur and found is None:
                found = res
            cur, res = m.group(1), []
            continue
        if cur is None or (found is not None):
            continue
        if needle not in cur:
            continue
        m = re.match(r"^\s*([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$", line)
        if m:
            res.append((m.group(2).replace(" ", ""), m.group(3).strip()))
    if found is None and cur is not None and needle in cur:
        found = res
    return found or []


def retail(addr, n):
    d = open(DOL, "rb").read()
    for _, (va, size, off) in SECTIONS.items():
        if va <= addr < va + size:
            o = off + (addr - va)
            return d[o:o + n]
    raise SystemExit("%#x is in no loaded section of %s" % (addr, DOL))


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    obj, needle = sys.argv[1], sys.argv[2]
    addr, n = int(sys.argv[3], 16), int(sys.argv[4], 0)
    o = one_function(obj, needle)
    if not o:
        raise SystemExit("no function matching %r in %s" % (needle, obj))
    rb = retail(addr, n)
    ob = b"".join(bytes.fromhex(h) for h, _ in o)
    if len(ob) != n:
        print("SIZE: ours %d bytes (%d instrs), retail %d" % (len(ob), len(o), n))
    bad = 0
    for i, (h, text) in enumerate(o):
        if i * 4 + 4 > len(rb):
            break
        oby, rby = bytes.fromhex(h), rb[4 * i:4 * i + 4]
        if oby == rby:
            continue
        bad += 1
        print("  +%02X  ours %-9s | retail %-9s | %s" % (4 * i, oby.hex(), rby.hex(), text))
    print("%d differing instructions of %d (%d bytes ours vs %d retail)" % (bad, len(o), len(ob), n))


main()
