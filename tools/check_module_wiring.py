#!/usr/bin/env python3
"""Is every REL module with sources in src/ actually wired into the build?

    python3 tools/check_module_wiring.py [--strict]

The failure this exists to catch has happened four times. A lane lands a module by adding a
`Rel("Module", [...])` block to `configure.py`; a later lane copies an older `configure.py` (or
rewrites the file) and silently drops that block. The module's sources then sit in `src/` compiled
by nothing, and nothing in the report says so - the units still appear, because `config.yml` lists
every retail module, but they score 0.00% and our code is in no link.

Measured 2026-09-25: Puffer, WallCrawler and ScriptGui were all in that state (their blocks dropped
by 33b73a3 and f599488), worth 30 matched functions once restored.

For every module directory under `config/G2ME01/rels/` it reports:
  * a unit named in the module's `splits.txt` whose source file exists in `src/` but which has no
    `Object(...)` entry in `configure.py` at all (UNWIRED), or
  * a unit whose source exists and is listed as `NonMatching` (NOT LINKED - our code compiles and is
    scored, but is not in the binary; that is legal and how the recipe works, but it is worth seeing),
  * a unit listed as `Matching` whose source file is missing (BROKEN - configure.py requires it).

Exit status is 1 if anything is UNWIRED or BROKEN, else 0.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIGURE = (ROOT / "configure.py").read_text()
RELS = ROOT / "config/G2ME01/rels"


def configured_units() -> dict:
    """unit path -> 'Matching' | 'NonMatching' | 'MatchingFor' (the last word is the state)."""
    out = {}
    # re.S: this repo writes some entries across two lines (`Object(\n NonMatching, "path")`).
    for m in re.finditer(r'Object\(\s*(Matching|NonMatching|MatchingFor)\s*(?:\([^)]*\))?\s*,\s*"([^"]+)"',
                         CONFIGURE, re.S):
        out[m.group(2)] = m.group(1)
    return out


def main() -> int:
    units = configured_units()
    unwired, not_linked, broken, wired = [], [], [], []
    for mod_dir in sorted(p for p in RELS.iterdir() if p.is_dir()):
        splits = mod_dir / "splits.txt"
        if not splits.exists():
            continue
        for line in splits.read_text().splitlines():
            s = line.strip()
            if not s.startswith(("MetroidPrime/", "REL/")) or not s.endswith(":"):
                continue
            unit = s[:-1]
            src = ROOT / "src" / unit
            state = units.get(unit)
            if state is None:
                # Not in configure.py. Only interesting if we have a source for it.
                if src.exists():
                    unwired.append((mod_dir.name, unit))
            elif state == "MatchingFor":
                wired.append((mod_dir.name, unit)) if src.exists() else broken.append((mod_dir.name, unit))
            elif state == "Matching":
                (wired if src.exists() else broken).append((mod_dir.name, unit))
            else:
                not_linked.append((mod_dir.name, unit))

    if unwired:
        print("UNWIRED - source exists in src/ but the unit is not in configure.py (our code is in no link):")
        for mod, unit in unwired:
            print(f"   {mod:26s} {unit}")
    if broken:
        print("BROKEN - listed as Matching with no source file (configure.py will refuse):")
        for mod, unit in broken:
            print(f"   {mod:26s} {unit}")
    if not_linked:
        print(f"not linked (source compiles and scores, unit is NonMatching): {len(not_linked)} unit(s)")
        for mod, unit in not_linked[:20]:
            print(f"   {mod:26s} {unit}")
    # `REL/REL_Setup.cpp` is one Matching object shared by every module, so it says nothing about
    # whether a module links *our* code; only MetroidPrime/ units do.
    own = [(m, u) for m, u in wired if u.startswith("MetroidPrime/")]
    linked_mods = sorted({m for m, _ in own})
    print(f"\n{len(own)} unit(s) of our own code in {len(linked_mods)} module(s): "
          + ", ".join(linked_mods))

    if unwired or broken:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
