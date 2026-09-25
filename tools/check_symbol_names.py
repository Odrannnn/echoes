#!/usr/bin/env python3
"""Report symbols.txt names that no retail-derived object defines.

A rename in symbols.txt changes the name the REL linker looks for, but the RELs resolve
against the DOL, whose objects still carry the retail name unless the unit is Matching.
So a rename that does not match the object exactly breaks all 86 REL links - which is what
happened to GetYaw__6CActorCFv (the function is not const; the object has ...Fv).

Run this before committing renames:
    python3 tools/check_symbol_names.py [unit-name-substring]
Exit status is non-zero when a name is declared that no object defines.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NM = ROOT / "build/binutils/powerpc-eabi-nm"


def symbols(path):
    if not path.exists():
        return set()
    out = subprocess.run([str(NM), str(path)], capture_output=True, text=True).stdout
    return {line.split()[2] for line in out.splitlines() if len(line.split()) >= 3 and line.split()[1] in "TWt"}


def main():
    filt = sys.argv[1] if len(sys.argv) > 1 else ""
    splits = (ROOT / "config/G2ME01/splits.txt").read_text()
    declared = {}
    for unit, body in re.findall(r"^(\S+\.cpp):\n((?:\t\.\w+\s+start:0x[0-9A-Fa-f]+ end:0x[0-9A-Fa-f]+\n)+)", splits, re.M):
        if filt and filt not in unit:
            continue
        # Per-section ranges, not one min/max span: a unit's sections are separate
        # address ranges and everything between them belongs to other units.
        text = [m for m in re.findall(r"\t\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", body)]
        if text:
            declared[unit] = [(int(a, 16), int(b, 16)) for a, b in text]

    symbols_txt = (ROOT / "config/G2ME01/symbols.txt").read_text()
    by_address = {}
    for name, addr, size in re.findall(r"^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)", symbols_txt, re.M):
        by_address[int(addr, 16)] = name

    bad = 0
    for unit, ranges in declared.items():
        obj = ROOT / "build/G2ME01/obj" / (unit[:-4] + ".o")
        defined = symbols(obj)
        if not defined:
            continue
        # Only names that look like recovered functions; retail placeholders
        # (fn_/lbl_) are expected to be absent and are not renames.
        names = {n for a, n in by_address.items()
                 if any(lo <= a < hi for lo, hi in ranges) and not n.startswith(("fn_", "lbl_"))}
        missing = sorted(names - defined)
        for name in missing:
            print(f"{unit}: {name} is declared but {obj.name} does not define it")
            bad += 1
    print(f"checked {len(declared)} units; {bad} declared names are missing from their object")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
