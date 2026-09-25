#!/usr/bin/env python3
"""EXPERIMENT ONLY - DO NOT LAND. Proves the 32 unnamed TypesMatch ids are gated on naming alone.

It invents the placeholder class name CUnknown<id> for every type id whose class no source in
this tree names, adds the class declaration, the TypesMatch override and the two TCastToPtr
specialisations, and renames the 94 functions in config/G2ME01/symbols.txt.  Measured result in
lane w18 (2026-09-25): main/MetroidPrime/TypesMatch 398/511 -> 492/511, all 94 new functions at
exactly 100%, project total 2800 -> 2894 matched functions, 86/86 REL module hashes unchanged,
check_symbol_names.py 0 missing, probe_sources.sh 115 files 0 failed.

The names are fiction. Revert with `git checkout src/MetroidPrime/TypesMatch.cpp
config/G2ME01/symbols.txt` (and re-run tools/decomp_build.sh) once the real class names are
known.  When they are, re-run this with the real names in CLASS below; nothing else in the
recipe changes.

Usage:  python3 research/EXPERIMENT_unnamed_typesmatch_ids.py && ./tools/decomp_build.sh MetroidPrime/TypesMatch

Two things this file encodes that cost time to find:
  * dtk rewrites config/G2ME01/symbols.txt on every build and DROPS a symbol that duplicates
    an address, so the (CEntity&) cast line must REPLACE its fn_ line, not be inserted beside
    it.  Inserting leaves the reference form unnamed and it scores 0.00%.
  * ids 40 and 46 have an unnamed parent (id 33), so only their two casts can be written; their
    TypesMatch overrides stay blocked on the parent's name.
"""
import re
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / 'src/MetroidPrime/TypesMatch.cpp'
SYM = ROOT / 'config/G2ME01/symbols.txt'
ENUM = ROOT / 'include/MetroidPrime/CEntityInfo.hpp'

# id -> real parent class, or None when the parent is itself unnamed.  From
# research/TypesMatch_mapping.txt, whose parent column is read off the call each retail override
# makes, not off a name.
PARENT = {
    10: 'CActor', 13: 'CGameCamera', 20: 'CEnergyProjectile', 24: 'CGameCamera', 27: 'CWeapon',
    31: 'CGameCamera', 33: 'CActor', 36: 'CEntity', 40: None, 42: 'CActor', 43: 'CScriptWaypoint',
    46: None, 50: 'CScriptDamageableTrigger', 52: 'CPhysicsActor', 54: 'CEntity', 63: 'CActor',
    65: 'CEntity', 67: 'CEntity', 71: 'CActor', 76: 'CEntity', 78: 'CEntity', 81: 'CActor',
    82: 'CScriptWaypoint', 83: 'CActor', 85: 'CActor', 90: 'CEntity', 91: 'CEntity', 96: 'CActor',
    100: 'CGameCamera', 101: 'CGameCamera', 137: 'CActor', 152: 'CEnergyProjectile',
}
# Stand-in parent for the two ids whose own parent (id 33) is unnamed: their casts only need a
# complete CEntity-derived class to static_cast to.
CAST_ONLY_PARENT = 'CActor'
# Retail address of each id's TypesMatch override, from research/TypesMatch_mapping.txt.
TYPESMATCH_ADDR = {
    10: 0x8009CA54, 13: 0x8009C9AC, 20: 0x8009C824, 24: 0x8009C744, 27: 0x8009C69C, 31: 0x8009C5BC,
    33: 0x8009C54C, 36: 0x8009C4A4, 40: 0x8009C3C4, 42: 0x8009C354, 43: 0x8009C31C, 46: 0x8009C274,
    50: 0x8009C194, 52: 0x8009C124, 54: 0x8009C0B4, 63: 0x8009BEBC, 65: 0x8009BE4C, 67: 0x8009BDDC,
    71: 0x8009BCFC, 76: 0x8009BBE4, 78: 0x8009BB74, 81: 0x8009BACC, 82: 0x8009BA94, 83: 0x8009BA5C,
    85: 0x8009B9EC, 90: 0x8009B8D4, 91: 0x8009B89C, 96: 0x8009B784, 100: 0x8009B6A4, 101: 0x8009B66C,
    137: 0x8009AE8C, 152: 0x8009AB44,
}
NAME = 'CUnknown{}'.format  # the placeholder


def main():
    src = SRC.read_text()
    enum = {m.group(1): int(m.group(2))
            for m in re.finditer(r'(kET_\w+) = (\d+)', ENUM.read_text())}

    # 1. the two casts per id, in place, where the file already has the "not named" comment.
    cast_addr = {}
    out = []
    for line in src.split('\n'):
        m = re.match(r'^// id (\d+): class not named by any source here'
                     r' \(fn_([0-9A-Fa-f]+), fn_([0-9A-Fa-f]+)\)', line)
        if m:
            i = int(m.group(1))
            cast_addr[i] = (int(m.group(2), 16), int(m.group(3), 16))
            out.append('CAST_TO_IMPL({}, {})'.format(NAME(i), i))
        else:
            out.append(line)
    src = '\n'.join(out)

    # 2. the class declarations, one per id that has a known parent.
    decls = '\n'.join('TYPES_MATCH_CLASS({}, {})'.format(NAME(i), PARENT[i])
                      for i in sorted(PARENT) if PARENT[i])
    for i in sorted(PARENT):
        if not PARENT[i]:
            decls += '\nTYPES_MATCH_CLASS({}, {})'.format(NAME(i), CAST_ONLY_PARENT)
    src = src.replace('\n#undef TYPES_MATCH_CLASS', '\n' + decls + '\n\n#undef TYPES_MATCH_CLASS', 1)

    # 3. the TypesMatch overrides, interleaved in the existing descending-id order.
    impls = re.findall(r'^TYPES_MATCH_IMPL\((\w+), (\w+), (\w+)\)$', src, re.M)
    pending = [(i, 'TYPES_MATCH_IMPL({}, {}, {})'.format(NAME(i), PARENT[i], i))
               for i in sorted(PARENT, reverse=True) if PARENT[i]]
    merged = []
    for cls, parent, idref in impls:
        for i, text in list(pending):
            if i > enum[idref]:
                merged.append(text)
                pending.remove((i, text))
        merged.append('TYPES_MATCH_IMPL({}, {}, {})'.format(cls, parent, idref))
    merged += [text for _, text in pending]
    old = '\n'.join('TYPES_MATCH_IMPL({}, {}, {})'.format(*t) for t in impls)
    assert old in src, 'the TypesMatch impl block moved; update this script'
    src = src.replace(old, '\n'.join(merged), 1)
    SRC.write_text(src)

    # 4. symbols.txt: name the two casts and the override at each id's retail address.
    # Every rename must REPLACE the fn_ line, never be inserted beside it: two symbols on one
    # line is a parse error for dtk ("invalid digit found in string") and the whole build dies.
    sym = SYM.read_text()

    def rename(sym, addr, newname):
        pat = re.compile(r'(?m)^fn_%X = (\.text:0x%08X;.*)$' % (addr, addr))
        sym, n = pat.subn(lambda m: newname + ' = ' + m.group(1), sym, count=1)
        assert n == 1, 'no fn_ line for %s' % hex(addr)
        return sym

    for i in sorted(PARENT, reverse=True):
        n = len(NAME(i))
        ptr, ref = cast_addr[i]
        sym = rename(sym, ptr, 'TCastToPtr<{}CUnknown{}>__FP7CEntity'.format(n, i))
        sym = rename(sym, ref, 'TCastToPtr<{}CUnknown{}>__FR7CEntity'.format(n, i))
        if PARENT[i]:
            sym = rename(sym, TYPESMATCH_ADDR[i], 'TypesMatch__{}CUnknown{}CFi'.format(n, i))
    SYM.write_text(sym)
    print('applied {} TypesMatch overrides and {} casts ({} placeholders)'
          .format(len([i for i in PARENT if PARENT[i]]), 2 * len(PARENT), len(PARENT)))


if __name__ == '__main__':
    main()
