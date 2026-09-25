#!/usr/bin/env python3
"""Lane e5: for each of the 86 real loaders, is the entity class it constructs
present in this tree?

A loader can only ever be a `Matching` DOL unit if the class it `new`s has both
its constructor and its vtable defined by a unit in this tree, because the
loader stores the vtable pointer and tail-calls the constructor at fixed DOL
addresses.  So the test is mechanical:

  * the constructor address (recovered from the `bl` after the `__nw__` call)
    must lie inside a range `config/G2ME01/splits.txt` gives to a unit that
    compiles a source file in this tree, and
  * likewise the vtable address the constructor installs at object+0.

`config/G2ME01/splits.txt` ranges not claimed by any unit are filled from
retail by dtk, so "no unit" means "the class has no unit here".
"""
import json
import re
import sys

sys.path.insert(0, 'tools')
import analyze_loaders as A  # noqa: E402


def load_ranges():
    cur = None
    out = []
    for line in open('config/G2ME01/splits.txt'):
        if line.startswith('\t'):
            m = re.match(r'\t\.(\w+)\s+start:(0x[0-9A-Fa-f]+) end:(0x[0-9A-Fa-f]+)', line)
            if m:
                out.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1), cur))
        elif line.rstrip().endswith(':'):
            cur = line.rstrip()[:-1]
    return out


def has_source(unit):
    import os
    if unit is None:
        return False
    if unit.startswith('auto_') or '/auto' in unit:
        return False
    p = os.path.join('src', unit)
    return os.path.exists(p)


def owner(ranges, addr):
    best = None
    for s, e, sec, unit in ranges:
        if s <= addr < e and (best is None or s > best[0]):
            best = (s, e, sec, unit)
    return best


def main():
    ranges = load_ranges()
    rows = json.load(open('/tmp/opencode/e5_loaders.json'))
    unblocked, blocked = [], []
    for name, fourcc, addr, size, frame, rsym in rows:
        o = A.analyse(name, fourcc, addr, size, frame, rsym)
        rec = dict(name=name, fourcc=fourcc, addr=addr, size=size, frame=frame,
                   rsym=rsym)
        if not o['news']:
            rec['note'] = 'no new site (delegates)'
            blocked.append(rec)
            print('%-34s %5d  %s' % (name, size, 'NO NEW SITE - delegates to another loader'))
            continue
        n = o['news'][0]
        ctor_owner = owner(ranges, n['ctor']) if n['ctor'] else None
        vt_owner = owner(ranges, n['vtable']) if n['vtable'] else None
        ok = (ctor_owner and has_source(ctor_owner[3]) and
              vt_owner and has_source(vt_owner[3]))
        rec.update(newsize=n.get('size'), ctor=n['ctor'], ctorname=n['ctorname'],
                   vtable=n['vtable'], vtsym=n['vtable'] and A.enclosing(n['vtable'])[0],
                   ctor_unit=ctor_owner and ctor_owner[3],
                   vt_unit=vt_owner and vt_owner[3])
        (unblocked if ok else blocked).append(rec)
        print('%-34s %5d %6s ctor=%-46s %-46s %s' % (
            name, size, n.get('size'),
            (str(n['ctorname']) or '')[:46],
            str(ctor_owner[3] if ctor_owner else None)[:46],
            'UNBLOCKED' if ok else ('vt unclaimed' if not vt_owner else
                                    'vt in ' + str(vt_owner[3]))))
    print()
    print('UNBLOCKED %d / %d' % (len(unblocked), len(rows)))
    json.dump(dict(unblocked=unblocked, blocked=blocked),
              open('/tmp/opencode/e5_loaders_blocked.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
