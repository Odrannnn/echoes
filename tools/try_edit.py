#!/usr/bin/env python3
"""Edit a function definition in a source file to each of several spellings and score it.

  tools/try_edit.py <src> <unit> <start-marker> <variants.py> [--restore]

`variants.py` defines VARIANTS = [(name, whole-function-text), ...].  For each one the file is
rewritten (everything from the line containing `start-marker` up to and including the closing
brace at column 0 replaced by the variant text), the object is rebuilt with ninja, and the unit's
per-function scores are printed.  The original is put back at the end.
"""
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
os.environ.setdefault("MP_TOOLCHAIN_DIR", os.path.join(ROOT, "..", "MetroidPrimePort"))
NINJA = os.path.join(os.environ["MP_TOOLCHAIN_DIR"], "build", "review-tools", "bin", "ninja")


def replace_fn(text, marker, body):
    i = text.index(marker)
    # back up to the start of that line
    i = text.rfind('\n', 0, i) + 1
    j = text.index('\n}\n', i) + len('\n}\n')
    return text[:i] + body.rstrip('\n') + '\n' + text[j:]


def score(unit, want):
    subprocess.run([NINJA, "build/G2ME01/src/%s.o" % unit], capture_output=True, text=True)
    subprocess.run(["./build/tools/objdiff-cli", "report", "generate", "-o", "build/report.json"],
                   capture_output=True, text=True)
    rep = json.load(open('build/report.json'))
    names = {unit, 'main/' + unit}
    for u in rep['units']:
        if u['name'] not in names:
            continue
        for f in u.get('functions', []):
            if want in f['name']:
                return u['name'], float(f.get('fuzzy_match_percent') or 0.0), int(f.get('size') or 0)
    return None


def main():
    src, unit, marker, vf = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    want = sys.argv[5]
    ns = {}
    exec(open(vf).read(), ns)
    original = open(src).read()
    try:
        for name, body in ns['VARIANTS']:
            open(src, 'w').write(replace_fn(original, marker, body))
            r = score(unit, want)
            print('%-28s %s' % (name, r if r else 'no such function'))
    finally:
        open(src, 'w').write(original)
    subprocess.run([NINJA, "build/G2ME01/src/%s.o" % unit], capture_output=True, text=True)


if __name__ == '__main__':
    main()
