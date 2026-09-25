#!/usr/bin/env python3
"""Batch body-variant runner for one function of one unit - the tool for the last 1%.

    tools/try_batch.py <src> <unit> <fn-pattern> <variants.py>

  <src>         the source file holding the function, e.g. src/Kyoto/CPakFile.cpp
  <unit>        the unit's path under build/G2ME01/src, e.g. Kyoto/CPakFile
  <fn-pattern>  a substring of the *mangled* name objdiff uses, e.g. GetResInfo__8CPakFile
  <variants.py> a python file defining VARIANTS = [(name, body), ...]; `body` replaces
                the function's braces and nothing else.

For each variant it writes the body, rebuilds only that one object with ninja, and
diffs our disassembly against the retail-derived object's, counting *differing
instructions* rather than bytes - so a 4-byte register choice shows up as 2, not as
a byte percentage. `*** MATCH ***` means the two disassemblies are identical once
branch targets and relocation operands are normalised; only then is the variant worth
keeping. The source file is always restored, including on failure.

Why count instructions: objdiff's percentage is dominated by size, so a variant that
fixes the logic but loses a scheduling decision can score *lower*. This ranks variants
by what actually has to change, which is the number that goes to zero.

Mined from a lane's private helper (/tmp/opencode/w14) and generalised: the unit
argument replaces its hardcoded root, and the retail object is found wherever dtk put
it, so it works for REL modules as well as the DOL.
"""
import difflib
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
os.environ.setdefault("MP_TOOLCHAIN_DIR", os.path.join(ROOT, "..", "MetroidPrimePort"))
NINJA = os.path.join(os.environ["MP_TOOLCHAIN_DIR"], "build", "review-tools", "bin", "ninja")
OBJDUMP = "build/binutils/powerpc-eabi-objdump"


def die(msg):
    print("error: %s" % msg, file=sys.stderr)
    raise SystemExit(1)


def find_retail_obj(unit):
    """dtk's object for this unit: build/G2ME01/obj/<unit>.o (DOL) or <Module>/obj/<unit>.o (REL)."""
    base = os.path.basename(unit)
    hits = [p for p in glob.glob("build/G2ME01/**/obj/**/*.o", recursive=True)
            if os.path.basename(p) == base + ".o"]
    if not hits:
        die("no retail object for %s (looked in build/G2ME01/**/obj/%s.o)" % (unit, base))
    # Prefer the one whose stem matches the unit path exactly.
    exact = [p for p in hits if p == "build/G2ME01/obj/%s.o" % unit or p.endswith("/obj/%s.o" % unit)]
    return (exact or sorted(hits, key=len))[0]


def disasm(path):
    out = subprocess.run([OBJDUMP, "-d", path], capture_output=True, text=True).stdout
    funcs, cur = {}, None
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]{8}) <(.+)>:", line)
        if m:
            cur = m.group(2)
            funcs[cur] = []
        elif cur and re.match(r"^\s+[0-9a-f]+:", line):
            t = line.split("\t", 2)
            if len(t) == 3:
                funcs[cur].append(t[2].strip())
    return funcs


def norm(t):
    """Hide the differences that cannot come from the source: branch targets and
    relocation operands on `bl`/`b`, which depend on where the linker put things."""
    p = t.split()
    if not p:
        return t
    if p[0].startswith("b") or p[0] in ("beq", "bne", "blt", "bgt", "ble", "bge", "bso", "bns"):
        return p[0]
    return t


def find_body_span(text, fnpat):
    plain = fnpat.split("__")[0]
    m = re.search(r"^[A-Za-z_][\w:<>\*& \t]*\b" + re.escape(plain) + r"\s*\(", text, re.M)
    if not m:
        die("definition of %r not found in the source" % plain)
    i = text.index("{", m.start())
    k, depth = i, 0
    while True:
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                break
        k += 1
    return i, k


def main():
    if len(sys.argv) != 5:
        die(__doc__.strip().splitlines()[0])
    src, unit, fnpat, varfile = sys.argv[1:5]
    if not os.path.exists(OBJDUMP):
        die("%s missing - copy build/binutils from another tree (see docs/LANE_BRIEFING.md)" % OBJDUMP)
    ours = "build/G2ME01/src/%s.o" % unit
    retail = find_retail_obj(unit)
    original = open(src).read()
    open_at, close_at = find_body_span(original, fnpat)

    ref = disasm(retail)
    keys = [k for k in ref if fnpat in k]
    if not keys:
        die("no function matching %r in %s" % (fnpat, retail))

    variants = {}
    exec(open(varfile).read(), variants)
    best = (10 ** 6, None)
    try:
        for name, body in variants["VARIANTS"]:
            open(src, "w").write(original[:open_at + 1] + "\n" + body + "\n" + original[close_at:])
            r = subprocess.run([NINJA, ours], capture_output=True, text=True)
            if r.returncode:
                print("%-24s BUILD FAIL" % name)
                print("\n".join(r.stderr.splitlines()[-4:]))
                continue
            got = disasm(ours)
            total = 0
            for key in keys:
                a = [norm(x) for x in ref.get(key, [])]
                b = [norm(x) for x in got.get(key, [])]
                for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
                    if tag != "equal":
                        total += max(i2 - i1, j2 - j1)
            if total == 0:
                print("%-24s *** MATCH ***" % name)
            else:
                print("%-24s %d differing instrs" % (name, total))
                if len(keys) == 1:
                    a = [norm(x) for x in ref[keys[0]]]
                    b = [norm(x) for x in got.get(keys[0], [])]
                    for line in difflib.unified_diff(a, b, "retail", "ours", lineterm="", n=1):
                        if line.startswith(("+", "-", "@")) and not line.startswith(("+++", "---")):
                            print("   " + line)
            if total < best[0]:
                best = (total, name)
    finally:
        open(src, "w").write(original)
    print("best: %s (%d differing instrs)" % (best[1], best[0]))


if __name__ == "__main__":
    main()
