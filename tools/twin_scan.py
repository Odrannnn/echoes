#!/usr/bin/env python3
"""Unmatched functions that are byte-shape twins of an already matched one.

    python3 tools/twin_scan.py            # the totals docs/HANDOFF.md quotes
    python3 tools/twin_scan.py --list     # one line per twin: unit, function, size, twin, source

Every function over 8 bytes in the target objects (`objdiff.json` `target_path`) is hashed by its
instruction words with each relocated word masked - a `bl` target to its opcode, anything else to
its high half. Two functions with the same hash are the same instructions apart from what the
linker fills in. `build/report.json` says which are matched; `scan()` pairs each unmatched one with
a matched one of the same shape, preferring a twin that has a source file of ours to copy from.
`tools/goal_seed.py` imports `scan()` for its `twin` kind.
"""
import collections
import json
import struct
import sys
from pathlib import Path


def funcs(path):
    """name -> (size, masked instruction words) for the functions of a big-endian ELF32 object."""
    e = Path(path).read_bytes()
    shoff, = struct.unpack('>I', e[0x20:0x24])
    shentsize, shnum, _shstr = struct.unpack('>HHH', e[0x2E:0x34])
    S = [struct.unpack('>10I', e[shoff + i * shentsize:shoff + i * shentsize + 40])
         for i in range(shnum)]
    rel = collections.defaultdict(dict)
    sym = None
    for h in S:
        if h[1] == 2:
            sym = h
        if h[1] == 4:  # SHT_RELA: section h[7], offset -> relocation type
            for o in range(h[4], h[4] + h[5], 12):
                off, info, _ = struct.unpack('>IIi', e[o:o + 12])
                rel[h[7]][off] = info & 0xff
    if not sym:
        return {}
    strs = S[sym[6]]
    out = {}
    for o in range(sym[4], sym[4] + sym[5], 16):
        nm, val, size, info, _, shndx = struct.unpack('>IIIBBH', e[o:o + 16])
        if info & 15 != 2 or size <= 8 or shndx >= shnum:
            continue
        s = S[shndx]
        b = e[s[4] + val:s[4] + val + size]
        r = rel.get(shndx, {})
        ws = []
        for i in range(0, len(b) - 3, 4):
            w, = struct.unpack('>I', b[i:i + 4])
            t = r.get(val + i) or r.get(val + i + 2)
            if t == 10:  # R_PPC_REL24
                w &= 0xFC000003
            elif t:
                w &= 0xFFFF0000
            ws.append(w)
        n = e[strs[4] + nm:e.index(b'\0', strs[4] + nm)].decode()
        out[n] = (size, tuple(ws))
    return out


def scan(root):
    """(twins, rest, missing) for the tree at `root`.

    `twins` is one dict per unmatched function with a matched twin: unit, name, size, rel (in a REL
    module), auto (the unit is dtk's, no source of ours), source (the unit's own source path or
    ''), and twin_unit / twin_name / twin_source for the matched function - twin_source is '' when
    no matched twin has a source file. `rest` is (shape hash, size, unit, name) for the unmatched
    functions without one, followed by the unit's source path and whether it is dtk's, and
    `missing` counts report units whose object could not be read. A twin in a REL module also
    carries rel_example: (unit, name, source) of a matched copy that is already built from our
    own source inside some REL module, or None - the nearest thing to a worked answer, since a
    module's copy needs the module recipe as well as the body.
    """
    root = Path(root)
    od = json.loads((root / 'objdiff.json').read_text())
    rep = json.loads((root / 'build/report.json').read_text())
    tpath = {u['name']: u.get('target_path') for u in od['units']}
    matched, rel_ex, un, missing = {}, {}, [], 0
    for u in rep['units']:
        p = tpath.get(u['name'])
        try:
            F = funcs(root / p) if p else None
        except (OSError, struct.error, ValueError):
            F = None
        if F is None:
            missing += 1
            continue
        meta = u.get('metadata') or {}
        src = meta.get('source_path') or ''
        for f in u.get('functions') or []:
            x = F.get(f['name'])
            if not x:
                continue
            if (f.get('fuzzy_match_percent') or 0) >= 100:
                # A twin we have source for beats one that only matches because it is retail's,
                # and a named one beats an `fn_` carve: its source says what the code is.
                rank = (bool(src), not f['name'].startswith('fn_'))
                if x[1] not in matched or rank > matched[x[1]][3]:
                    matched[x[1]] = (u['name'], f['name'], src, rank)
                if src and not u['name'].startswith('main/'):
                    rel_ex.setdefault(x[1], (u['name'], f['name'], src))
            else:
                un.append((x[1], x[0], u, f['name'], src))
    twins, rest = [], []
    for shape, size, u, name, src in un:
        t = matched.get(shape)
        if not t:
            rest.append((hash(shape), size, u['name'], name, src,
                         bool((u.get('metadata') or {}).get('auto_generated'))))
            continue
        twins.append({
            'unit': u['name'], 'name': name, 'size': size, 'source': src,
            'rel': not u['name'].startswith('main/'),
            'auto': bool((u.get('metadata') or {}).get('auto_generated')),
            'twin_unit': t[0], 'twin_name': t[1], 'twin_source': t[2],
            'rel_example': rel_ex.get(shape),
        })
    return twins, rest, missing


def main():
    root = Path(__file__).resolve().parent.parent
    tw, rest, missing = scan(root)
    if '--list' in sys.argv[1:]:
        for t in sorted(tw, key=lambda t: (t['unit'], t['name'])):
            print(f"{t['unit']}\t{t['name']}\t{t['size']}\t{t['twin_unit']}\t{t['twin_name']}"
                  f"\t{t['twin_source'] or '-'}")
        return 0
    print('units without object', missing, 'unmatched fns >8 bytes seen', len(tw) + len(rest))
    print('unmatched with an exact matched twin:', len(tw),
          'DOL', sum(not t['rel'] for t in tw), 'REL', sum(t['rel'] for t in tw),
          'bytes', sum(t['size'] for t in tw))
    print('  of which the twin has a source file of ours:', sum(bool(t['twin_source']) for t in tw))
    g = collections.Counter(x[0] for x in rest)
    print('remaining:', len(rest), 'distinct shapes', len(g),
          '-> free copies once one is solved', sum(c - 1 for c in g.values() if c > 1))
    print('biggest groups', sorted(g.values(), reverse=True)[:10])
    for b in (32, 64, 128, 256):
        print('twins with size >', b, sum(1 for t in tw if t['size'] > b))
    print(collections.Counter(t['unit'].split('/')[0] for t in tw if t['rel']).most_common(8))
    return 0


if __name__ == '__main__':
    sys.exit(main())
