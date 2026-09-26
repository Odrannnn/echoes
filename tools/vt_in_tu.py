#!/usr/bin/env python3
"""Name an accessor run's class from the vtable its own translation unit stores.

    python3 tools/vt_in_tu.py 0x80212A94 0x80212ADC
    python3 tools/vt_in_tu.py --run 0x80212A94..0x80212ADC --window 0x20000

Why this is the instrument for the hard runs. `tools/accessor_table.py` places an accessor by a
*named caller passing its own this*; that fails for any run whose whole translation unit is
unnamed, which is where the 6-to-10-accessor runs live. The other handle is the **constructor**:
a constructor stores the class's vtable pointer into the object, so a function that writes a `.data`
address into `[r3+k]` is a constructor of whatever class that vtable belongs to - and
`tools/vtable_of.py` turns the vtable address into the class name out of the `.rodata` typeinfo.
Runs of accessors sit in the same object as their own constructor, so a vtable store within
`--window` bytes of the run names the class.

The window matters and is why this is not a proof on its own: several classes' code can be within
`--window` of each other in retail's `.text`, since mwldeppc orders by *object* and an object can
hold several classes. So the output is ranked and must be read: a run whose window holds exactly
one vtable store is a strong result, a run whose window holds six is a list to check, and the
`accessor_table.py` proof still outranks all of them.
"""
import argparse
import bisect
import collections
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
LABEL = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")

# a store of an absolute/sda address into an object: `stw rX,K(r3)` where rX came from lis/addis
LIS = re.compile(r"^lis\s+(r\d+),")
ADDR = re.compile(r"^r(\d+),(-?0x[0-9a-f]+|-?\d+)$")
REL = re.compile(r"^(-?0x[0-9a-f]+|-?\d+)\((r\d+)\)$")


def load(path):
    insn, label = {}, {}
    for line in pathlib.Path(path).read_text(errors="replace").splitlines():
        m = LABEL.match(line)
        if m:
            label[int(m.group(1), 16)] = m.group(2)
            continue
        m = INSN.match(line)
        if m:
            insn[int(m.group(1), 16)] = (m.group(3), m.group(4).strip())
    return insn, label


def demangle(names):
    if not names:
        return {}
    p = subprocess.run(["c++filt"], input="\n".join(names) + "\n", text=True,
                       capture_output=True)
    return dict(zip(names, p.stdout.splitlines()))


def vtable_index(elf):
    """Every `.data` word that looks like a `.text` address -> its address (a vtable slot)."""
    p = subprocess.run(["powerpc-eabi-objdump", "-s", "-j", ".data", elf],
                       capture_output=True, text=True)
    data = bytearray()
    for line in p.stdout.splitlines():
        m = re.match(r"^\s*([0-9a-f]+)\s+((?:[0-9a-f]{2,8}\s+){1,4})", line)
        if m:
            for grp in m.group(2).split():
                data += bytes.fromhex(grp)
    # .data VMA
    h = subprocess.run(["powerpc-eabi-objdump", "-h", elf], capture_output=True, text=True)
    vma = 0x803B0C00
    for line in h.stdout.splitlines():
        m = re.match(r"^\s*\d+\s+\.data\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            vma = int(m.group(2), 16)
    idx = {}
    for i in range(0, len(data) - 3, 4):
        w = struct.unpack_from("<I", data, i)[0]
        if 0x80003100 <= w < 0x803A5200:
            idx.setdefault(vma + i, w)
    return data, vma, idx


def class_of_vtable(data, vma, idx, slot_addr):
    """Walk back to the vtable start, read the typeinfo pointer, resolve the class name string."""
    elf = str(ROOT / "build" / "G2ME01" / "main.elf")
    ro = subprocess.run(["powerpc-eabi-objdump", "-s", "-j", ".rodata", elf],
                        capture_output=True, text=True)
    rdata = bytearray()
    rvma = 0x803A56C0
    for line in ro.stdout.splitlines():
        m = re.match(r"^\s*([0-9a-f]+)\s+((?:[0-9a-f]{2,8}\s+){1,4})", line)
        if m:
            for grp in m.group(2).split():
                rdata += bytes.fromhex(grp)
    for line in subprocess.run(["powerpc-eabi-objdump", "-h", elf],
                               capture_output=True, text=True).stdout.splitlines():
        m = re.match(r"^\s*\d+\s+\.rodata\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if m:
            rvma = int(m.group(2), 16)

    start = slot_addr
    for _ in range(80):
        i = start - 4 - vma
        if i < 0:
            break
        prev = struct.unpack_from("<I", data, i)[0]
        if not (0x80003100 <= prev < 0x803A5200):
            break
        start -= 4
    i = start - 4 - vma
    if i < 0:
        return None, start
    ti = struct.unpack_from("<I", data, i)[0]
    j = ti - rvma
    if not (0 <= j < len(rdata)):
        return None, start
    e = rdata.find(b"\0", j, j + 200)
    if e < 0:
        return None, start
    return rdata[j:e].decode("latin-1", "replace"), start


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lo", nargs="?")
    ap.add_argument("hi", nargs="?")
    ap.add_argument("--run", default="")
    ap.add_argument("--window", type=lambda x: int(x, 16), default=0x20000)
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    a = ap.parse_args()

    if a.run:
        lo, hi = (int(x, 16) for x in a.run.split(".."))
    else:
        lo, hi = int(a.lo, 16), int(a.hi, 16)

    insn, label = load(a.asm)
    data, vma, idx = vtable_index(str(ROOT / "build" / "G2ME01" / "main.elf"))
    vts = {addr: w for addr, w in idx.items()}

    # Forward pass over the window, looking for a **vtable store**: an absolute address built
    # with `lis`+`addi` and written into an object. The base register is not necessarily r3 -
    # a constructor normally copies `this` into a callee-saved register first
    # (`mr r31,r3` ... `stw r0,0(r31)`), so any base is accepted. The `lis`+`lis`+`addi` shape is
    # what mwcc emits for an address above 0x80000000; the small-data form is `lwz rX,K(r13)`.
    found = []
    for pc in range(lo - a.window, hi + a.window, 4):
        if pc not in insn:
            continue
        mn, ops = insn[pc]
        if mn != "lis":
            continue
        m = re.match(r"^(r\d+),(-?0x[0-9a-f]+|-?\d+)$", ops)
        if not m:
            continue
        reg, hi16 = m.group(1), int(m.group(2), 0) & 0xFFFF
        for k in range(1, 9):
            b = pc + 4 * k
            if b not in insn:
                break
            mn2, ops2 = insn[b]
            if mn2 in ("blr", "bctr", "b"):
                break
            m2 = re.match(r"^(r\d+),(r\d+),(-?0x[0-9a-f]+|-?\d+)$", ops2)
            if not (m2 and m2.group(2) == reg):
                continue
            addr = ((hi16 << 16) + int(m2.group(3), 0)) & 0xFFFFFFFF
            if not (0x803B0C00 <= addr < 0x803C5A10):
                break
            dst = m2.group(1)
            for j2 in range(1, 4):
                c = b + 4 * j2
                if c not in insn:
                    break
                mn3, ops3 = insn[c]
                if mn3 in ("blr", "bctr", "b"):
                    break
                mm = re.match(rf"^{dst},(-?0x[0-9a-f]+|-?\d+)\((r\d+)\)$", ops3)
                if mn3 == "stw" and mm:
                    found.append((pc, addr, c, int(mm.group(1), 0)))
            break

    seen = {}
    for fpc, addr, site, off in found:
        if addr in seen:
            continue
        name, vstart = class_of_vtable(data, vma, vts, addr)
        seen[addr] = (fpc, name, vstart, site, off)

    dem = demangle([n for _a, n, _v, _s, _o in seen.values() if n])
    print(f"window 0x{lo - a.window:08X}..0x{hi + a.window:08X} around accessor run "
          f"0x{lo:08X}..0x{hi:08X}")
    if not seen:
        print("no vtable store in the window")
    for addr, (pc, name, vstart, site, off) in sorted(seen.items(),
                                              key=lambda kv: abs(kv[1][0] - lo)):
        d = dem.get(name, "") if name else ""
        print(f"  ctor-ish 0x{pc:08X} stores vtable 0x{addr:08X} (vtable base 0x{vstart:08X}) "
              f"at this+{off}: {name!r} {d}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
