#!/usr/bin/env python3
"""Read a virtual-address range out of the retail DOL, by section table.

  tools/dol_read.py 0x803CE3BC 0x60 [orig/G2ME01/sys/main.dol]

`build/G2ME01/main.elf` cannot answer a question about an unclaimed `.data` address, because
that ELF is *our* build: dtk fills an unclaimed **.text** range with retail bytes, so
`tools/dis.sh` is retail for code, but `.data` is whatever our objects produced. The section
addresses are right (the linker script fixes them), the contents are not, so the retail DOL is
the only source for a `.data` question.
"""
import struct
import sys

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


def main():
    addr = int(sys.argv[1], 16)
    n = int(sys.argv[2], 0)
    path = sys.argv[3] if len(sys.argv) > 3 else "orig/G2ME01/sys/main.dol"
    d = open(path, "rb").read()
    for name, va, size, off in SECTIONS:
        if va <= addr < va + size:
            o = off + (addr - va)
            b = d[o:o + n]
            print("%s @ %#x  (file %#x)" % (name, addr, o))
            print("hex :", b.hex(" "))
            if name not in (".text", ".init", "extab", "extabindex"):
                print("LE  :", [hex(x) for x in struct.unpack("<" + "I" * (len(b) // 4), b)])
            else:
                print("BE  :", [hex(x) for x in struct.unpack(">" + "I" * (len(b) // 4), b)])
            return
    print("%#x is in no loaded section (bss?)" % addr)


main()
