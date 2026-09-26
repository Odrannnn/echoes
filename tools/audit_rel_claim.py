#!/usr/bin/env python3
"""audit_rel_claim.py <Module> - is every function in a Matching REL unit's claim actually ours?

A `Rel(...)` object that claims a `.text` range wider than the functions its own
source defines is filled in by dtk with *retail* bytes, and objdiff then reports
those functions at 100% and counts them in `matched_functions`. The module's
sha1 stays correct, so nothing fails - the unit just claims more than it wrote.
`src/MetroidPrime/ScriptObjects/CDarkSamusFlags.cpp` claimed 0xF840..0xF86C for
two functions and the report said 3/3 at 100.00%, because dtk put retail's
fn_10_F84C in the gap.

So: for each range in `config/G2ME01/rels/<Module>/splits.txt`, list the retail
functions `symbols.txt` places inside it, and compare that against the `.fn`
symbols dtk emitted for the corresponding object under
`build/G2ME01/<Module>/asm/`. Any retail function with no emitted counterpart in
the right object is a filled gap; any emitted symbol with no retail function at
that offset is one the unit invented.

  tools/audit_rel_claim.py DarkSamus
"""
import os
import re
import subprocess
import sys

SPLIT_FILE = re.compile(r"(?m)^(?P<path>[^\s#:][^\n:]*):$")
RANGE = re.compile(r"^\s+(?P<sec>\.[A-Za-z0-9_]+)\s+start:0x(?P<a>[0-9A-Fa-f]+) end:0x(?P<b>[0-9A-Fa-f]+)\s*$")
FUNC = re.compile(r"(?m)^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    module = sys.argv[1]
    root = os.getcwd()
    splits = os.path.join(root, "config", "G2ME01", "rels", module, "splits.txt")
    syms = os.path.join(root, "config", "G2ME01", "rels", module, "symbols.txt")
    asm = os.path.join(root, "build", "G2ME01", module, "asm")

    text = open(splits).read()
    claims = []
    path = None
    for line in text.splitlines():
        m = SPLIT_FILE.match(line)
        if m:
            path = m.group("path")
            continue
        m = RANGE.match(line)
        if m and path:
            claims.append((path, m.group("sec"), int(m.group("a"), 16), int(m.group("b"), 16)))

    retail = {}
    for name, addr, size in FUNC.findall(open(syms).read()):
        retail.setdefault(int(addr, 16), (name, int(size, 16)))

    problems = 0
    for path, sec, start, end in claims:
        if sec != ".text":
            continue
        in_range = sorted(
            (a, n, s) for a, (n, s) in retail.items() if start <= a and a + s <= end
        )
        # dtk mirrors the source path under build/G2ME01/<Module>/asm/ and drops
        # only the file's extension: MetroidPrime/ScriptObjects/CDarkSamus.cpp
        # becomes .../asm/MetroidPrime/ScriptObjects/CDarkSamus.s
        obj = os.path.join(asm, os.path.dirname(path),
                           os.path.splitext(os.path.basename(path))[0] + ".s")
        if not os.path.isfile(obj):
            print("MISSING  %s: dtk emitted no %s" % (path, obj))
            problems += 1
            continue
        emitted = {}
        cur = None
        for line in open(obj):
            f = re.match(r"^\.fn (\S+),", line)
            if f:
                cur = f.group(1)
            c = re.match(r"^/\* ([0-9A-Fa-f]+) ", line)
            if c and cur and cur not in emitted:
                emitted[cur] = int(c.group(1), 16)
        got = sorted(emitted.items(), key=lambda kv: kv[1])
        ours = [a for _, a in got if start <= a < end]
        want = [a for a, _, _ in in_range]
        gaps = [retail[a][0] for a in want if a not in ours]
        extra = [n for n, a in got if a not in retail]
        status = "ok" if not gaps and not extra and len(ours) == len(want) else "GAP"
        if status != "ok":
            problems += 1
        print("%-4s %-52s 0x%08X..0x%08X  %d/%d functions" % (
            status, path, start, end, len(ours), len(want)))
        for g in gaps:
            print("       dtk-filled, not ours: %s" % g)
        for e in extra:
            print("       emitted but not a retail function here: %s" % e)
    print("\n%d claim(s) with a problem" % problems)
    problems += dropped_by_strip_partial(asm)
    return 1 if problems else 0


def dropped_by_strip_partial(asm):
    """Did the `.plf` link drop any of our symbols?

    The `.plf` step links with `-lcf ldscript.lcf -strip_partial`, so an object whose
    symbols are neither in the module's retail FORCEACTIVE list nor referenced from
    `.data` is removed *silently*: ninja succeeds, objdiff still reports 100%, and the
    module's `.text` comes out short with every `bl` after the hole resolved low. Five
    byte-exact deleting destructors at DarkSamus 0x214D0..0x215FC vanished this way.
    Comparing the `.preplf` and `.plf` symbol tables is the cheap, direct test.
    """
    mod = os.path.basename(os.path.dirname(asm))
    nm = "build/binutils/powerpc-eabi-nm"
    tables = {}
    for ext in ("preplf", "plf"):
        path = "build/G2ME01/%s/%s.%s" % (mod, mod, ext)
        if not os.path.isfile(path):
            print("SKIP     %s: no %s (build it first)" % (mod, path))
            return 0
        names = set()
        for line in subprocess.run([nm, path], capture_output=True, text=True).stdout.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[1] in ("T", "t"):
                names.add(parts[2])
        tables[ext] = names
    lost = sorted(tables["preplf"] - tables["plf"])
    print("%s: preplf %d text symbols, plf %d, %d dropped by -strip_partial"
          % (mod, len(tables["preplf"]), len(tables["plf"]), len(lost)))
    for name in lost:
        print("       DROPPED, so the claim that defines it is not in the module: %s" % name)
    return len(lost)


if __name__ == "__main__":
    sys.exit(main())
