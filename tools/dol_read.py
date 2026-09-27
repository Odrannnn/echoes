#!/usr/bin/env python3
"""Read a virtual-address range out of the retail DOL, by section table.

  tools/dol_read.py 0x803CE3BC 0x60 [orig/G2ME01/sys/main.dol]

`build/G2ME01/main.elf` cannot answer a question about an unclaimed `.data` address, because
that ELF is *our* build: dtk fills an unclaimed **.text** range with retail bytes, so
`tools/dis.sh` is retail for code, but `.data` is whatever our objects produced. The section
addresses are right (the linker script fixes them), the contents are not, so the retail DOL is
the only source for a `.data` question.

## Byte order: the whole DOL is big-endian, so every section in it is

A DOL is one linked image for one target, and that target is a big-endian PowerPC. There is no
per-section byte order in the format, so there is nothing to choose between `.text` and `.data`:
both are big-endian. Three witnesses, none of them a lane's recollection of what the bytes
"looked like":

* **The file's own section table is big-endian.** The eight section file offsets at `0x1C..0x38`
  are `00 3a 22 a0`, `00 3a 23 c0`, `00 3a 24 a0`, ... Read little-endian they are 0xA0223A00
  and up - terabytes of offset into a 3.9 MB file. A table that says *where the data is* cannot
  itself be in a different order than the data, and neither can the section address and size
  columns beside it.
* **`.init` starts with a PowerPC prologue.** `memset` at `0x80003100` is `94 21 ff f0`
  (`stwu r1,-16(r1)`), `7c 08 02 a6` (`mflr r0`), `90 01 00 14` (`stw r0,20(r1)`), `93 e1 00 0c`
  (`stw r31,12(r1)`) - the textbook EABI entry, read big-endian. Little-endian, the first word
  is 0xF0FF2194, which is not an instruction.
* **`.text` opens with symbols `symbols.txt` says are four bytes long.**
  `EnableMetroTRKInterrupts` at `0x80003840` is `4e 80 00 20`, one `blr`; its neighbour
  `InitMetroTRK` is `38 60 00 00` (`li r3,0`) then `4e 80 00 20` (`blr`), also four bytes.

This tool used to print `BE` for `.text`/`.init`/`extab`/`extabindex` and **`LE` for the other six
sections**, on the assumption that only code is big-endian. The assumption was the bug. It
decoded `.ctors`, `.dtors`, `.rodata`, `.data`, `.sdata` and `.sdata2` wrongly, and the `LE` line
is the one a reader is most likely to copy: `CGraphics::mViewport` at `.data:0x803B9FE8` came out
of it as a viewport whose width is `0x80020000` - 2147614720 unsigned, -2147352576 signed - where
the 24 bytes really are `{0, 0, 640, 480, 320.0f, 240.0f}`. `tools/test_dol_read.py` is the
regression test and it fails on the old decode.

The three decoder defects that are not byte order, all reported by that test and all fixed here,
because each one can be demonstrated on a real request:

* **A range that runs past its own section used to be read straight through into whatever the
  file has next**, and printed as one undifferentiated blob. `0x803C5A00` is the last `0x10`
  bytes of `.data`, and the `0x20` after it is `.sdata`, ten kilobytes away in memory. The read
  now stops at the section end and says how much it got.
* **A length that is not a multiple of four used to be truncated silently** - `0x8041DEE4 0x6`
  printed two bytes fewer than it read, and the `hex` line made that invisible.
* **A negative length used to return the bytes *before* the address asked for.** `d[o:o-4]` is
  a valid slice, so `-4` printed a plausible four bytes from the wrong place and exited 0.
"""
import struct
import sys

# One byte order for the entire image. Not a default a section can override: the format has no
# per-section byte order. The docstring above is the evidence; `tools/test_dol_read.py` is the
# thing that keeps it true.
BYTE_ORDER = ">"

# `dtk dol info orig/G2ME01/sys/main.dol` prints exactly these ten loaded sections, and it
# disagrees with the linked ELF on the file offsets - read the offsets out of the DOL, not the
# ELF. The three `.bss` sections below are listed only so that "no loaded section" can name
# which one an address is in instead of guessing.
SECTIONS = [
    (".init", 0x80003100, 0x534, 0x100),
    ("extab", 0x80003640, 0x10C, 0x3A22A0),
    ("extabindex", 0x80003760, 0xE0, 0x3A23C0),
    (".text", 0x80003840, 0x3A1C54, 0x640),
    (".ctors", 0x803A54A0, 0x1F0, 0x3A24A0),
    (".dtors", 0x803A56A0, 0xC, 0x3A26A0),
    (".rodata", 0x803A56C0, 0xB530, 0x3A26C0),
    (".data", 0x803B0C00, 0x14E10, 0x3ADC00),
    (".sdata", 0x80417D80, 0x1104, 0x3C2A20),
    (".sdata2", 0x8041A3C0, 0x54C0, 0x3C3B40),
]

BSS = [
    (".bss", 0x803C5A20, 0x52344),
    (".sbss", 0x80418EA0, 0x1508),
    (".bss2", 0x8041F880, 0x84),
]


def find(addr):
    """(name, vaddr, size, file offset) of the loaded section holding `addr`, or None."""
    for sec in SECTIONS:
        if sec[1] <= addr < sec[1] + sec[2]:
            return sec
    return None


def f32(words):
    """Format big-endian f32 words so a garbage one cannot pass for a number."""
    out = []
    for x in words:
        if x != x:
            out.append("nan")
        elif x in (float("inf"), float("-inf")):
            out.append("inf" if x > 0 else "-inf")
        else:
            out.append("%.7g" % x)
    return out


def read(addr, n, path):
    """Run the read, print it, return 0 or 2."""
    if n <= 0:
        # `d[o:o+n]` with a negative n is a valid slice ending *before* the address, so the old
        # tool answered a negative length with the wrong bytes and exit 0.
        print("dol_read: length must be positive, got %d" % n, file=sys.stderr)
        return 2

    d = open(path, "rb").read()
    hit = find(addr)
    if hit is None:
        for name, va, size in BSS:
            if va <= addr < va + size:
                print("%#x is in %s (%#x..%#x), which is not in the file" % (addr, name, va, va + size))
                return 0
        print("%#x is in no loaded section" % addr)
        for name, va, size in SECTIONS:
            print("  %-12s %#x..%#x" % (name, va, va + size))
        return 0

    name, va, size, off = hit
    o = off + (addr - va)
    got = min(n, size - (addr - va))
    b = d[o:o + got]
    if len(b) < got:  # the section table and the file disagree; say so rather than print short
        print("dol_read: %s claims %d bytes at file %#x but the file ends at %#x"
              % (name, size, off, len(d)), file=sys.stderr)
        got = len(b)

    print("%s @ %#x  (file %#x, %d bytes)" % (name, addr, o, got))
    if got != n:
        # `.data` ends at 0x803c5a10 and `.sdata` does not start until 0x80417d80, so the bytes
        # after the end of a section are a different section - or padding - and not a continuation
        # of the one asked about.
        print("warn : asked for %d, read %d - %s ends at %#x and what follows in the file is not "
              "more of it" % (n, got, name, va + size))
    print("hex :", b.hex(" "))

    words = len(b) // 4
    whole = b[:words * 4]
    if words:
        print("u32  :", " ".join("%#x" % x for x in struct.unpack(BYTE_ORDER + "I" * words, whole)))
        print("f32  :", " ".join(f32(struct.unpack(BYTE_ORDER + "f" * words, whole))))
    if len(b) != words * 4:
        print("warn : %d trailing byte(s) not shown as words: %s"
              % (len(b) - words * 4, b[words * 4:].hex(" ") or "-"))
    if addr % 4:
        print("warn : %#x is not 4-aligned, so the u32/f32 views above are a shifted reading, "
              "not words at these addresses" % addr)
    return 0


def main():
    if len(sys.argv) < 3:
        print("usage: dol_read.py <hex vaddr> <byte count> [main.dol]", file=sys.stderr)
        return 2
    try:
        # Base 0, so `0x18` and `24` both work, and base 16 for the address because a leading
        # zero there is a typo, not an octal literal.
        addr = int(sys.argv[1], 16)
        n = int(sys.argv[2], 0)
    except ValueError as e:
        # `int("08", 0)` raises, and a traceback for a mistyped argument reads as a broken tool.
        print("dol_read: %s" % e, file=sys.stderr)
        return 2
    path = sys.argv[3] if len(sys.argv) > 3 else "orig/G2ME01/sys/main.dol"
    return read(addr, n, path)


if __name__ == "__main__":
    sys.exit(main())
