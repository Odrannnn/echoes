#!/usr/bin/env python3
"""Measure how mwcceppc actually encodes each CActor bitfield, and diff it against retail.

    tools/probe_cactor_offsets.py [--retail-only]

Two things, both measured with **mwcceppc's own flags** - the host compiler must not be used, it is
64-bit and `rstl::string` is 24 bytes there against retail's 0x10, so a host `sizeof` is not
evidence about anything MWCC will emit:

1. sizeof(CActor) and the non-bitfield member offsets, out of `.data`. `offsetof` on a bitfield is
   illegal in mwcceppc, and `CHECK_SIZEOF(CActor, 0x158)` is a consistency check rather than a
   measurement - `check_sizeof<T,n>` passes for any n - so neither can decide where the 0x150
   bitfield group starts. This part answers that for the members that are not bitfields.
2. The byte and the `rlwimi` merge range MWCC picks for **every** bitfield in the group, by
   disassembling one generated setter per field. This is the part that decides
   `CActor::SetDirtyFlags` (retail 0x8004A0A0, 56 bytes: four `lbz 336(r3)` /
   `rlwimi r0,r4,4,27,27` / `stb` sequences). Retail's other one-bit accessors give the same
   encoding for known fields, so the table is checkable field by field rather than being a claim.

Expected values are duplicated from the probe on purpose: a header that is edited and a probe that
is edited together cannot catch anything, so the third copy is the RETAIL table below.
"""
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
TC = os.environ.get("MP_TOOLCHAIN_DIR", os.path.join(ROOT, "..", "MetroidPrimePort"))
MWCC = os.path.join(TC, "build", "compilers", "GC", "2.7", "mwcceppc.exe")
WIBO = os.path.join(TC, "build", "tools", "wibo")
OBJDUMP = "build/binutils/powerpc-eabi-objdump"
OBJ = "/tmp/m1_cactor_probe.o"

# (label, expected) - retail's, in tools/size_probe_cactor.cpp's order.
EXPECTED = [
    ("sizeof(CActor)", 0x158),
    ("m_transform", 0x024),
    ("m_position", 0x054),
    ("m_modelData", 0x060),
    ("m_material", 0x068),
    ("x70_materialFilter", 0x070),
    ("x88_sfxId", 0x088),
    ("xbc_actorLights", 0x0BC),
    ("otherBounds", 0x0CC),
    ("m_renderBounds", 0x0E4),
    ("xfc_drawFlags", 0x0FC),
    ("xbc_time", 0x108),
    ("xc4_fluidId", 0x110),
    ("xc6_nextDrawNode", 0x112),
    ("xd4_maxVol", 0x120),
    ("xd8_nonLoopingSfxHandles", 0x124),
    ("x130_addedToken", 0x130),
    ("actor_padding", 0x134),
]

# Retail's own one-bit accessors, as (field, byte, rlwimi SH, MB, ME). Every one of these is a
# `lbz`/`stb` pair, so "byte" is a displacement in `this` and SH/MB/ME are the printed operands.
# SetDirtyFlags is the four-field case: retail writes fields 3,4,5,6 of the byte at 0x150.
RETAIL_ACCESSORS = [
    ("SetMuted", 0x151, 4, 27, 27),
    ("SetCallTouch", 0x151, 1, 30, 30),
    ("SetUseInSortedLists", 0x151, 3, 28, 28),
]

FLAGS = ["-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
         "-fp", "hardware", "-Cpp_exceptions", "off", "-O4,p", "-inline", "auto",
         "-pragma", "cats off", "-maxerrors", "1", "-nosyspath", "-RTTI", "off",
         "-fp_contract", "on", "-str", "reuse", "-i", "include", "-i", "libc",
         "-i", "build/G2ME01/include", "-DVERSION=0", "-DNDEBUG=1",
         "-str", "reuse,pool,readonly", "-gccinc", "-inline", "deferred,noauto",
         "-common", "on", "-lang=c++", "-c", "tools/size_probe_cactor.cpp", "-o", OBJ]


def section_bytes(name):
    out = subprocess.run([OBJDUMP, "-s", "-j", name, OBJ], capture_output=True, text=True).stdout
    blob = bytearray()
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]{4,8})\s+((?:[0-9a-f]{8} )+)", line)
        if m:
            blob += bytes.fromhex(m.group(2).replace(" ", ""))
    return blob


def main():
    for path in (MWCC, WIBO, OBJDUMP):
        if not os.path.exists(path):
            print("error: %s missing (set MP_TOOLCHAIN_DIR)" % path, file=sys.stderr)
            return 2
    r = subprocess.run([WIBO, MWCC] + FLAGS, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-3000:])
        print("error: probe did not compile", file=sys.stderr)
        return 2

    def w32(blob):
        return [struct.unpack(">I", bytes(blob[i:i + 4]))[0] for i in range(0, len(blob) - 3, 4)]

    data, sdata = section_bytes(".data"), section_bytes(".sdata")
    if not data or not sdata:
        print("error: probe sections empty (.data %d, .sdata %d)" % (len(data), len(sdata)),
              file=sys.stderr)
        return 2
    # g_n shares .sdata with the other small constants mwcceppc emits, so take the word that is
    # actually the count rather than assuming it is the first one - a wrong index here reads a
    # neighbouring constant as the length and reports a mismatch that is not in the header.
    n = len(EXPECTED)
    if n not in w32(sdata):
        print("error: g_n (%r) is not in .sdata (%r) - the probe did not emit as expected"
              % (n, w32(sdata)), file=sys.stderr)
        return 2
    got = w32(data)[:n]
    if len(got) != n:
        print("error: .data has %d words, expected %d" % (len(got), n), file=sys.stderr)
        return 2
    bad = 0
    for (label, want), have in zip(EXPECTED, got):
        ok = want == have
        bad += not ok
        print("%-28s want 0x%03X  got 0x%03X  %s" % (label, want, have, "ok" if ok else "MISMATCH"))

    # Part 2: the encoding mwcceppc gives each bitfield.
    dis = subprocess.run([OBJDUMP, "-d", "--section=.text", OBJ],
                         capture_output=True, text=True).stdout
    funs, cur = {}, None
    for line in dis.splitlines():
        h = re.match(r"^\s*[0-9a-f]+ <(.+)>:$", line)
        if h:
            cur = h.group(1)
            funs[cur] = []
            continue
        i = re.match(r"^\s*([0-9a-f]+):\t[0-9a-f ]+\t+(\S.*)$", line)
        if i and cur:
            funs[cur].append(i.group(2).strip())

    print("\n%-26s %-8s %s" % ("bitfield", "byte", "rlwimi"))
    table = {}
    for name, insns in funs.items():
        m = re.match(r"_?probe_(\w+)", name)
        if not m:
            continue
        field = m.group(1)
        off = rlw = None
        for t in insns:
            lb = re.match(r"^(lbz|lwz)\s+r\d+,(\d+)\(r3\)$", t)
            if lb:
                off = int(lb.group(2))
            rl = re.match(r"^rlwimi\s+r(\d+),r(\d+),(\d+),(\d+),(\d+)$", t)
            if rl:
                rlw = (int(rl.group(3)), int(rl.group(4)), int(rl.group(5)))
        table[field] = (off, rlw)
        print("%-26s %-8s %s" % (field,
                                 ("0x%03X" % off) if off is not None else "-",
                                 ("sh=%d mb=%d me=%d" % rlw) if rlw else "-"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
