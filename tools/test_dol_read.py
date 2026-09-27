#!/usr/bin/env python3
"""`tools/dol_read.py` must decode every section of the DOL big-endian, because the DOL is.

    tools/test_dol_read.py [TOOL] [DOL]

A DOL is a single linked image for a big-endian PowerPC, so the format has no per-section byte
order: `.text` and `.data` are the same way round. This tool used to print `BE` for the four
code sections and `LE` for the other six, on the assumption that only code is big-endian, and
that produced a plausible wrong answer rather than a failure - `CGraphics::mViewport` at
`.data:0x803B9FE8` came out as a viewport whose width word is `0x80020000`, 2147614720 unsigned
and -2147352576 signed, where the 24 bytes are `{0, 0, 640, 480, 320.0f, 240.0f}`. A check that
cannot fail is worth nothing here, so the consequences are asserted against named retail values,
and each of the two named values has a confirmation that does not come out of this tool at all:

* `lbl_8041DEE4` is `3f 80 00 00`, which read big-endian is 1.0f, and retail's own code loads it
  as a float constant - `Carve8026FBFC.cpp:31` and `Carve80271238.cpp:252` both use it as 1.0f,
  and the second of those records a lane that once read it as -23.0f.
* `CGraphics::mViewport` is pinned by `CGraphics::SetViewport`'s `lis r11,-32708` /
  `stwu r3,-24600(r11)` pair, which carries `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` against
  `mViewport__9CGraphics` - the linker naming the symbol and the 24 bytes, not this tool.

Nothing here is a reading of the tool's own output: every expected value is either a literal
below or computed from the file's bytes with `struct` in this file, so the test cannot agree
with the tool by construction.

The per-section cases are the general one. They do not name a section to be big-endian, they
sample the first word of all ten and require the tool's answer to equal the big-endian reading
of those bytes - so reinstating the `LE` branch for any one of them fails, which is the defect
this file exists to keep fixed. `.text` and `.init` are in that list on purpose: the bug was
never that they were wrong, and the obvious repair - make `.data` big-endian and leave `.text`
little-endian - is a different wrong answer of the same shape, which this list does catch.

Exit 0 pass, 1 fail, 77 skip (no DOL). Run from the repo root; pass an explicit DOL path if the
default is not where yours is.
"""
import os
import re
import struct
import subprocess
import sys

# A number in a decode line. Deliberately not `split()`: the pre-fix tool printed a Python list
# literal (`LE  : ['0x0', '0x0', '0x80020000']`) and the fixed one prints bare space-separated
# words, and the test has to read both to be able to say what the old tool got wrong.
NUM = re.compile(r"0x[0-9a-fA-F]+|[-+]?\d+\.\d+(?:e[-+]?\d+)?|[-+]?\d+(?:e[-+]?\d+)?|[-+]?\d+")

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "dol_read.py")
DEFAULT_DOLS = ["orig/G2ME01/sys/main.dol", "build/G2ME01/main.dol"]

# (name, vaddr, size, file offset) for the ten loaded sections, as `dtk dol info` prints them.
# Written out here rather than imported from the tool, so that a wrong byte order in the tool
# cannot make the table the test checks against agree with it. The DOL's own header agrees -
# see the section-table case in main() - and it is the header that settles the byte order.
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


def run(tool, dol, addr, n):
    p = subprocess.run([sys.executable, tool, hex(addr), hex(n), dol],
                       capture_output=True, text=True)
    if p.returncode != 0:
        return None, p.stderr.strip() or "exit %d" % p.returncode
    words, floats, notes = [], [], []
    for line in p.stdout.splitlines():
        # `LE` and `BE` are the labels the pre-fix tool used, and they are accepted here on
        # purpose: this test is about the *values* the tool decodes, so running it against the
        # old tool has to fail with `0x80020000` where `0x280` belongs rather than with a
        # complaint about a word it cannot find. A test that fails on a label cannot tell a
        # relabelled-but-correct tool from a wrong one.
        for key, sink in (("u32", words), ("LE", words), ("BE", words), ("f32", floats)):
            if line.startswith(key + " ") or line.startswith(key + ":"):
                for tok in re.findall(NUM, line[len(key):].replace("[", " ")):
                    sink.append(float(tok) if key == "f32" else int(tok, 0))
        if line.startswith("warn"):
            notes.append(line)
    return (words, floats, notes, p.stdout), ""


def read_be(dol, addr, n):
    for _name, va, size, off in SECTIONS:
        if va <= addr < va + size:
            with open(dol, "rb") as f:
                f.seek(off + addr - va)
                return f.read(n)
    return None


def main():
    tool = sys.argv[1] if len(sys.argv) > 1 else TOOL
    dol = sys.argv[2] if len(sys.argv) > 2 else next(
        (p for p in DEFAULT_DOLS if os.path.exists(p)), None)
    if not dol or not os.path.exists(dol):
        print("SKIP: no main.dol; looked for %s" % ", ".join(DEFAULT_DOLS))
        return 77

    fails = []

    def want(label, got, expect):
        if got != expect:
            fails.append("%s\n      want %r\n      got  %r" % (label, expect, got))

    # 1. The named retail value with an independent confirmation: 1.0f.
    out, err = run(tool, dol, 0x8041DEE4, 4)
    if out is None:
        fails.append("dol_read.py 0x8041DEE4 0x4 failed: %s" % err)
    else:
        want("lbl_8041DEE4 (.sdata2) must decode 3f 80 00 00 as 1.0f, not 0x803f",
             (out[0], out[1]), ([0x3F800000], [1.0]))

    # 2. CGraphics::mViewport, 24 bytes = sizeof(CViewport), pinned by SetViewport's relocations.
    out, err = run(tool, dol, 0x803B9FE8, 0x18)
    if out is None:
        fails.append("dol_read.py 0x803B9FE8 0x18 failed: %s" % err)
    else:
        # As words: 0, 0, 640, 480, and the bit patterns of 320.0f and 240.0f. As floats: the
        # first two are 0, the next two are the int fields read as denormals, and the last two
        # are 320.0 and 240.0. Both are checked: the old word view was wrong and it had no float
        # view at all, and reading 0x80020000 as a width is how a viewport gets invented.
        want("CGraphics::mViewport (.data) words must be 0, 0, 640, 480, 320.0f, 240.0f",
             out[0], [0, 0, 0x280, 0x1E0, 0x43A00000, 0x43700000])
        want("CGraphics::mViewport (.data) floats must end in 320.0 and 240.0, and start at 0, 0",
             (out[1][:2], out[1][4:]), ([0.0, 0.0], [320.0, 240.0]))
        if read_be(dol, 0x803B9FE8, 0x18).hex(" ") != (
                "00 00 00 00 00 00 00 00 00 00 02 80 00 00 01 e0 43 a0 00 00 43 70 00 00"):
            fails.append("mViewport's 24 retail bytes are not what this test expects; the DOL changed")

    # 3. The general case: all ten sections, first word, big-endian. `.text` and `.init` included
    #    so a fix that only made the data sections big-endian fails here.
    for name, va, size, _off in SECTIONS:
        if size < 4:
            continue
        out, err = run(tool, dol, va, 4)
        if out is None:
            fails.append("dol_read.py %s %#x 0x4 failed: %s" % (name, va, err))
            continue
        b = read_be(dol, va, 4)
        want("%s %#x must decode big-endian" % (name, va),
             out[0], [struct.unpack(">I", b)[0]])

    # 4. A .data word holding a code pointer, which is the case the per-section sweep above
    #    cannot catch on its own: `.data`'s first word is 0x00000000, which reads the same way
    #    round. Read little-endian this one is 0xCC550080, which is not an address in this image
    #    and would pass as one; that is the whole failure mode.
    out, err = run(tool, dol, 0x803B0D64, 4)
    if out is None:
        fails.append("dol_read.py 0x803B0D64 0x4 failed: %s" % err)
    else:
        want(".data:0x803B0D64 is a pointer to .text 0x800055CC", out[0], [0x800055CC])

    # 5. The file's own section table is big-endian, so the file is. This is the root cause, and
    #    it is the one witness that no per-section assumption can survive: the eight offsets at
    #    0x1C..0x38 are 0x3a22a0 and neighbours, not 0xa0223a00.
    with open(dol, "rb") as f:
        head = f.read(0x3C)
    offs = [struct.unpack(">I", head[0x1C + 4 * i:0x20 + 4 * i])[0] for i in range(8)]
    size = os.path.getsize(dol)
    if not all(0 < o < size for o in offs):
        fails.append("DOL section file offsets do not read big-endian: %s" % [hex(o) for o in offs])

    # 6. A range that runs past its section must stop and say so, not read the next section.
    out, err = run(tool, dol, 0x803C5A00, 0x20)  # last 0x10 bytes of .data
    if out is None:
        fails.append("dol_read.py 0x803C5A00 0x20 failed: %s" % err)
    else:
        if len(out[0]) != 4:
            fails.append("a read past the end of .data returned %d words, want 4 - it read into "
                         "whatever follows .data in the file and said so" % len(out[0]))
        if not out[2]:
            fails.append("a read past the end of .data printed no warning line")

    # 7. A length that is not a whole number of words must not lose bytes quietly.
    out, err = run(tool, dol, 0x8041DEE4, 6)
    if out is None:
        fails.append("dol_read.py 0x8041DEE4 0x6 failed: %s" % err)
    else:
        if "c5 80" not in out[3]:
            fails.append("a 6-byte read dropped its last two bytes (c5 80) with no mention of them")
        if not out[2]:
            fails.append("a 6-byte read printed no warning that two bytes are not words")

    # 8. A negative length must be rejected: d[o:o-4] is a valid slice, so the old tool answered
    #    it with the four bytes *before* the address and exited 0.
    out, err = run(tool, dol, 0x8041DEE4, -4)
    if out is not None:
        fails.append("a negative length returned %d words instead of an error" % len(out[0]))

    if fails:
        print("dol_read.py decoding FAILED (%d):" % len(fails))
        for f in fails:
            print("  " + f)
        return 1
    print("ok: all 10 sections big-endian, mViewport = {0, 0, 640, 480, 320, 240}, "
          "lbl_8041DEE4 = 1.0f, 4 decoder defects covered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
