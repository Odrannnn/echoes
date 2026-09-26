#!/usr/bin/env python3
"""Check every member offset of CGameState against the expected values, measured with mwcceppc.

    tools/probe_gs_offsets.py

The header claims a map of 27 `this` offsets for `CGameState::CGameState(CInputStream&, int)`
(retail `fn_80144140`, 0x80144140). A claim in a header is a memory, and this is what turns it
into a measurement: the probe is compiled with **mwcceppc's own flags** (never the host
compiler, which is 64-bit and has `rstl::string` at 24 bytes against retail's 0x10), read back
out of `.data` and compared against the numbers the header and
`docs/research/cgamestate_layout.md` state.

The expected list is duplicated here on purpose: a header that is edited and a probe that is
edited together cannot catch anything, so the third copy is this file's.

    tools/size_probe_gs.cpp is the probe; the flags come out of build.ninja.
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

# (probe array index, label, expected) - in tools/size_probe_gs.cpp's order.
EXPECTED = [
    ("sizeof(CGameState)", 0x2F0),
    ("+0x00 x00_unk", 0x000),
    ("+0x04 x04_unk", 0x004),
    ("+0x08 x08_reserve", 0x008),
    ("+0x18 x18_playerStates", 0x018),
    ("+0x1C x01c_players", 0x01C),
    ("+0x3C x3c_worldState", 0x03C),
    ("+0x40 x40_refCount", 0x040),
    ("+0x44 x44_unk", 0x044),
    ("+0x48 x48_time", 0x048),
    ("+0x50 x50_unk", 0x050),
    ("+0x54 x54", 0x054),
    ("+0x80 gameOptions", 0x080),
    ("+0xC4 hintOptions", 0x0C4),
    ("+0xDC persistentOptions", 0x0DC),
    ("+0x108 cardSerial", 0x108),
    ("+0x110 x110", 0x110),
    ("+0x144 x144", 0x144),
    ("+0x178 x178", 0x178),
    ("+0x188 x188", 0x188),
    ("+0x198 x198_ptrSet", 0x198),
    ("+0x19C x19c_ptr", 0x19C),
    ("+0x1A0 x1a0", 0x1A0),
    ("+0x1F4 x1f4", 0x1F4),
    ("+0x204 x204", 0x204),
    ("+0x2EC x2ec_flags", 0x2EC),
    ("sizeof(CGameOptions)", 0x044),
    ("sizeof(CHintOptions)", 0x018),
    ("sizeof(CPersistentOptions)", 0x02C),
    ("sizeof(SGameStateBlock)", 0x010),
    ("sizeof(SGameStateSlots)", 0x034),
    ("sizeof(SGameStateCardOpts)", 0x02C),
    ("sizeof(SGameStateWorlds)", 0x054),
    ("sizeof(SGameStateMemcard)", 0x0E8),
    ("SGameStateSlots::x04_blk", 0x004),
    ("SGameStateWorlds::x10_count", 0x010),
    ("SGameStateWorlds::x14_rec", 0x014),
    ("SGameStateMemcard::x00_size", 0x000),
    ("SGameStateMemcard::x04_buf", 0x004),
    ("SGameStateMemcard::x50_size", 0x050),
    ("SGameStateMemcard::x54_buf", 0x054),
    ("SGameStateMemcard::xa0_unk", 0x0A0),
    ("SGameStateMemcard::xa4_unk", 0x0A4),
    ("SGameStateMemcard::xe4_flag", 0x0E4),
]

FLAGS = ["-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int",
         "-fp", "hardware", "-Cpp_exceptions", "off", "-O4,p", "-inline", "auto",
         "-pragma", "cats off", "-maxerrors", "1", "-nosyspath", "-RTTI", "off",
         "-fp_contract", "on", "-str", "reuse", "-i", "include", "-i", "libc",
         "-i", "build/G2ME01/include", "-DVERSION_G2ME01", "-DNDEBUG=1",
         "-str", "reuse,pool,readonly", "-gccinc", "-inline", "deferred,noauto",
         "-common", "on", "-lang=c++", "-c", "tools/size_probe_gs.cpp",
         "-o", "/tmp/m1_gs_probe.o"]


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
    # `g_probe` lands in `.data` and mwcceppc puts the `g_n` count in `.sdata`, so both have to
    # be read: `.data` is exactly the array's words and `.sdata` is exactly the count. Reading
    # only `.data` and treating its tail as the count silently "passes" with garbage, which is
    # what an earlier version of this tool did.
    def section_bytes(name):
        out = subprocess.run([OBJDUMP, "-s", "-j", name, "/tmp/m1_gs_probe.o"],
                             capture_output=True, text=True).stdout
        blob = bytearray()
        for line in out.splitlines():
            m = re.match(r"^\s*([0-9a-f]{4,8})\s+((?:[0-9a-f]{8} )+)", line)
            if m:
                blob += bytes.fromhex(m.group(2).replace(" ", ""))
        return blob

    def words_of(blob):
        return [struct.unpack(">I", bytes(blob[i:i + 4]))[0] for i in range(0, len(blob) - 3, 4)]

    data = section_bytes(".data")
    sdata = section_bytes(".sdata")
    if not data or not sdata:
        print("error: probe sections are empty (.data %d, .sdata %d) - it did not emit g_probe"
              % (len(data), len(sdata)), file=sys.stderr)
        return 2
    n = words_of(sdata)[0]
    got = words_of(data)[:n]
    if len(got) != len(EXPECTED) or n != len(EXPECTED):
        print("error: probe returned %d words (g_n = %d), expected %d"
              % (len(got), n, len(EXPECTED)), file=sys.stderr)
        return 2
    bad = 0
    for (label, want), have in zip(EXPECTED, got):
        ok = want == have
        bad += not ok
        print("%-32s want 0x%03X  got 0x%03X  %s" % (label, want, have, "ok" if ok else "MISMATCH"))
    if bad:
        print("FAIL: %d of %d offsets wrong" % (bad, len(EXPECTED)))
    else:
        print("ok: %d offsets" % len(EXPECTED))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
