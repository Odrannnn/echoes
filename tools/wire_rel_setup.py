#!/usr/bin/env python3
"""wire_rel_setup.py <Module>... - claim a REL module's REL_Setup tail for the shared "REL" lib.

Every module ends in the same five functions (`_unresolved`, `_epilog`, `_prolog`,
`ModuleDestructors`, `ModuleConstructors`), and the shared "REL" lib in configure.py already
compiles them. A module links them only once its `splits.txt` claims their range, and it hashes
only once `symbols.txt` names what they reference:

  - `ModuleDestructors` / `ModuleConstructors`, the two 0x4C functions after `_prolog`;
  - `RELExit` / `RELMain`, the second `bl` of `_epilog` / `_prolog`. Unnamed (`fn_55_104`),
    the reference stays unresolved and the module grows 48 bytes of relocations; named
    without `scope:global` (AtomicAlpha), the same. Both need `scope:global`.

The second names are read from dtk's disassembly of the claimed range, so this builds once
between the two passes. It does not judge the result: run `./tools/decomp_build.sh` and check
the module's sha1 against config/G2ME01/config.yml, and revert the module if it moved.

  tools/wire_rel_setup.py SandBoss Blogg
"""
import re
import subprocess
import sys


def claim(m):
    out = subprocess.run(["python3", "tools/scaffold_rel_module.py", m],
                         capture_output=True, text=True, check=True).stdout
    blk = re.search(r"\nREL/REL_Setup\.cpp:\n(\t.*\n)+", out).group(0)
    d = f"config/G2ME01/rels/{m}/"
    if "REL/REL_Setup.cpp:" in open(d + "splits.txt").read():
        sys.exit(f"{m}: REL_Setup is already claimed")
    open(d + "splits.txt", "a").write(blk)
    lines = open(d + "symbols.txt").read().split("\n")
    i = next(k for k, l in enumerate(lines) if l.startswith("_prolog = "))
    for k, name in ((i + 1, "ModuleDestructors"), (i + 2, "ModuleConstructors")):
        if lines[k].startswith(name + " = "):
            continue
        assert re.match(r"fn_\d+_[0-9A-F]+ = \.text:.*size:0x4C$", lines[k]), lines[k]
        lines[k] = name + lines[k][lines[k].index(" = "):] + " scope:global"
    open(d + "symbols.txt", "w").write("\n".join(lines))


def name_entry_points(m):
    asm = open(f"build/G2ME01/{m}/asm/REL/REL_Setup.s").read()
    want = {}
    for fn, name in (("_epilog", "RELExit"), ("_prolog", "RELMain")):
        body = asm.split(f".fn {fn}, global")[1].split(".endfn")[0]
        want[re.findall(r"\bbl (\S+)", body)[1]] = name
    p = f"config/G2ME01/rels/{m}/symbols.txt"
    lines = open(p).read().split("\n")
    for k, l in enumerate(lines):
        n = l.split(" = ")[0]
        if n in want:
            l = want[n] + l[len(n):]
            lines[k] = l if "scope:global" in l else l + " scope:global"
    open(p, "w").write("\n".join(lines))


mods = sys.argv[1:]
if not mods:
    sys.exit(__doc__)
for m in mods:
    claim(m)
subprocess.run(["./tools/decomp_build.sh"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
for m in mods:
    name_entry_points(m)
print("claimed and named:", " ".join(mods), "- now build and check each module's sha1")
