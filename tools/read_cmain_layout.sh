#!/usr/bin/env python3
"""Decode tools/sizeprobe_cmain.cpp's .data words, in order, with names.

    tools/read_cmain_layout.sh [obj]

Reads the object `tools/probe_cc.sh` produced and prints one `name 0xNN` line per word.
mwcceppc, not the host, is the compiler that decides these numbers.
"""
import re
import struct
import subprocess
import sys

OBJ = sys.argv[1] if len(sys.argv) > 1 else "/tmp/opencode/cmainhdr/sizeprobe.o"
NAMES = ["sizeof(CMain)", "sizeof(SFrameTimeHistory)", "osContext", "x4_unk1",
         "memorySys", "xc_unk2", "x10_unk", "x18_frameTimeHistory",
         "x2c_frameTimeHistory", "x40_frameTimeTotal", "x44_frameTimeTotal",
         "frameTimeMinimum", "x4c", "x50", "gameGlobalObjects", "restartMode",
         "x5c", "frameTimes", "frameTimeIdx", "x164_"]
RETAIL = {"sizeof(CMain)": 0x98, "sizeof(SFrameTimeHistory)": 0x14, "osContext": 0x0,
          "x4_unk1": 0x4, "memorySys": 0x8, "xc_unk2": 0xC, "x10_unk": 0x10,
          "x18_frameTimeHistory": 0x18, "x2c_frameTimeHistory": 0x2C,
          "x40_frameTimeTotal": 0x40, "x44_frameTimeTotal": 0x44,
          "frameTimeMinimum": 0x48, "x4c": 0x4C, "x50": 0x50,
          "gameGlobalObjects": 0x54, "restartMode": 0x58, "x5c": 0x5C,
          "frameTimes": 0x60, "frameTimeIdx": 0x8C, "x164_": 0x94}

out = subprocess.run(["build/binutils/powerpc-eabi-objdump", "-s", "-j", ".rodata", OBJ],
                     capture_output=True, text=True).stdout
raw = b""
for line in out.splitlines():
    m = re.match(r"^\s+[0-9a-f]+\s+((?:[0-9a-f]{2,8}\s+)+)", line)
    if m:
        raw += bytes.fromhex(m.group(1).replace(" ", ""))
words = struct.unpack(">%dI" % (len(raw) // 4), raw)
bad = 0
for name, w in zip(NAMES, words):
    want = RETAIL.get(name)
    mark = ""
    if want is not None:
        mark = "  OK" if w == want else "  <-- retail 0x%X" % want
        if w != want:
            bad += 1
    print("%-18s 0x%02X%s" % (name, w, mark))
print("%d word(s), %d disagree with retail" % (len(words), bad))
