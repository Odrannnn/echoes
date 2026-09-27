#!/usr/bin/env python3
"""Compile N spellings of one carve body and rank them against retail's bytes.

    tools/try_carve.py <variants.py> [retail_start] [retail_size]

`variants.py` is a dict of name -> the C body text, all sharing the file's preamble. For each
one the harness compiles it with a `.c` unit's flags (`tools/probe_c.sh`, i.e. `-lang=c`) and
reports differing instruction count and object size, best first.

Ranking is by **differing instructions**, not by percentage: objdiff's percentage is
size-dominated and once ranked a 352-byte variant at 6 differing instructions above a 348-byte
one at 15, which was the wrong way round. See FACTS.md.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREAMBLE = """
struct SFrameTimeHistory {
  int count;
  float values[4];
};
"""


def compile_and_measure(name, body, start, size, keep=None):
    src = os.path.join(tempfile.gettempdir(), "carve_%s.c" % name)
    obj = os.path.join(tempfile.gettempdir(), "carve_%s.o" % name)
    with open(src, "w") as f:
        f.write(PREAMBLE + "\n" + body + "\n")
    r = subprocess.run([os.path.join(ROOT, "tools/probe_c.sh"), src, obj],
                       capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0 or not os.path.exists(obj):
        return (10 ** 9, 0, "COMPILE FAILED: " + (r.stdout + r.stderr).strip().splitlines()[-1][:90]
                if (r.stdout + r.stderr).strip() else "COMPILE FAILED")
    d = subprocess.run([os.path.join(ROOT, "tools/carve_diff.sh"), start, size, obj],
                       capture_output=True, text=True, cwd=ROOT).stdout
    bad = None
    nbytes = 0
    for line in d.splitlines():
        if line.startswith("ours  :"):
            nbytes = int(line.split()[2]) * 4
        if line.startswith("differing instructions:"):
            bad = int(line.split(":")[1])
    if bad is None:
        return (10 ** 9, nbytes, "no diff output")
    return (bad, nbytes, "")


def main():
    src = open(sys.argv[1]).read()
    ns = {}
    exec(compile(src, sys.argv[1], "exec"), ns)
    variants = ns["variants"]
    start = sys.argv[2] if len(sys.argv) > 2 else "0x800069AC"
    size = sys.argv[3] if len(sys.argv) > 3 else "0x134"
    rows = []
    for name, body in variants.items():
        bad, nbytes, err = compile_and_measure(name, body, start, size)
        rows.append((bad, nbytes, name, err))
    rows.sort(key=lambda r: (r[0], abs(r[1] - int(size, 16))))
    print("%-22s %6s %8s  %s" % ("variant", "diff", "bytes", "note"))
    for bad, nbytes, name, err in rows:
        print("%-22s %6d %8d  %s" % (name, bad, nbytes, err))


main()
