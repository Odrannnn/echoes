#!/usr/bin/env python3
"""Is this function in a vtable, and if so, which class's?

    python3 tools/vtable_of.py 0x80212A94 [0x80212A9C ...]
    python3 tools/vtable_of.py --run 0x80212A94..0x80212ADC

A virtual accessor never appears as a `bl` target, so the call-graph route
(`tools/class_of.py`) cannot see it. It *does* appear as a word in a vtable in `.data`, and a vtable
in this binary is preceded by a `typeinfo` pointer whose `.rodata` object is the mangled class name.
So: scan `.data`/`.rodata` for the address as a little-endian word, walk back to the vtable start,
read the `typeinfo` word, and read the class name string out of `.rodata`.

The typeinfo layout mwcc emits is `pClassName (char*)` then `pBaseClass (void*)`, so the *first*
word of the typeinfo object is the name pointer; a vtable's first word is the typeinfo pointer. That
gives, for each hit: the vtable's own address, the slot index, and the class name.
"""
import argparse
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def sections(elf):
    out = {}
    p = subprocess.run(["powerpc-eabi-objdump", "-h", elf], capture_output=True, text=True)
    for line in p.stdout.splitlines():
        m = re.match(r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            out[m.group(1)] = (int(m.group(3), 16), int(m.group(2), 16), int(m.group(4), 16))
    return out


def read_section(elf, name, sec):
    p = subprocess.run(["powerpc-eabi-objdump", "-s", "-j", name, elf],
                       capture_output=True, text=True)
    data = bytearray()
    for line in p.stdout.splitlines():
        m = re.match(r"^\s*([0-9a-f]+)\s+((?:[0-9a-f]{2,8}\s+){1,4})", line)
        if m:
            for grp in m.group(2).split():
                data += bytes.fromhex(grp)
    return bytes(data)


def word(data, vma, off, addr):
    """A little-endian word at `addr` (a VMA) from `data` loaded at `vma`."""
    i = addr - vma
    if i < 0 or i + 4 > len(data):
        return None
    return struct.unpack_from("<I", data, i)[0]


def cstr(data, vma, addr, limit=256):
    i = addr - vma
    if i < 0 or i >= len(data):
        return None
    e = data.find(b"\0", i, min(len(data), i + limit))
    if e < 0:
        return None
    return data[i:e].decode("latin-1", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addrs", nargs="*")
    ap.add_argument("--run", default="")
    ap.add_argument("--elf", default=str(ROOT / "build" / "G2ME01" / "main.elf"))
    a = ap.parse_args()

    targets = {int(x, 16) for x in a.addrs}
    if a.run:
        lo, hi = (int(x, 16) for x in a.run.split(".."))
        targets |= set(range(lo, hi, 4))

    sec = sections(a.elf)
    data = {n: read_section(a.elf, n, sec[n]) for n in (".data", ".rodata") if n in sec}
    vs = {n: v for n, (v, _s, _o) in sec.items() if n in data}
    dr, rr = vs.get(".data"), vs.get(".rodata")

    # the word -> addresses index over .data
    index = {}
    for i in range(0, len(data[".data"]) - 3, 4):
        w = struct.unpack_from("<I", data[".data"], i)[0]
        index.setdefault(w, []).append(dr + i)

    for t in sorted(targets):
        sites = index.get(t, [])
        if not sites:
            print(f"0x{t:08X}: not in any .data word (not virtual)")
            continue
        for s in sites:
            # walk back to the vtable start: the word before the run is the typeinfo pointer
            ti = word(data[".data"], dr, None, s - 4)
            # a vtable's first slot is virtual; the typeinfo pointer precedes it
            # confirm by walking back while the preceding word is a .text address
            start = s
            for _ in range(64):
                prev = word(data[".data"], dr, None, start - 4)
                if prev is None or not (0x80003100 <= prev < 0x803A5200):
                    break
                start -= 4
            ti = word(data[".data"], dr, None, start - 4)
            name = cstr(data[".rodata"], rr, ti) if ti and rr else None
            name2 = cstr(data[".rodata"], rr, ti + 4) if ti and rr else None
            slot = (s - start) // 4
            print(f"0x{t:08X}: vtable 0x{start:08X} slot {slot:3d}  typeinfo 0x{ti:08X}  "
                  f"name={name!r} alt={name2!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
