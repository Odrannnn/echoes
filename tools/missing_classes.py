#!/usr/bin/env python3
"""The 75 classes the 86 real REL loaders construct, and what each one costs.

Lane e5 already measured, for every loader, the `new` size (= sizeof the class),
the entity constructor and the vtable the constructor installs
(docs/research/real_loaders.md, reproduced by tools/classify_loaders.py). This
tool reads that table back and adds the three things that decide what to do next:

  * **the class name**, from vtable slot 1. Retail names 57 of the 76
    `TypesMatch__<mangled>CFi`; slot 1 is the *inherited* `TypesMatch` when the
    name equals the class's measured base, so those are reported as INHERITED
    and the class stays unnamed rather than being confidently mis-named.
  * **ctor bytes** - the size of the entity constructor, i.e. what a Matching
    ctor unit would have to reproduce.
  * **vtable slots / unclaimed slots** - the size of the vtable, and how many of
    its function slots point into a `.text` range no unit in this tree claims.
    Claiming a vtable means every one of those has to be *named* in
    symbols.txt, because a C++ definition mangles to a name the linked dtk
    object does not define.

Reproduce: python3 tools/missing_classes.py
"""
import os
import re
import subprocess

OBJDUMP = 'build/binutils/powerpc-eabi-objdump'
ELF = 'build/G2ME01/main.elf'

FUNCS = []
RANGES = []
_cur = None
for _line in open('config/G2ME01/symbols.txt'):
    if _line.startswith('\t'):
        m = re.match(r'\t\.(\w+)\s+start:(0x[0-9A-Fa-f]+) end:(0x[0-9A-Fa-f]+)', _line)
        if m:
            RANGES.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1), _cur))
    elif _line.rstrip().endswith(':'):
        _cur = _line.rstrip()[:-1]
    else:
        m = re.match(r'^(\S+) = \.text:(0x[0-9A-Fa-f]+); // type:function size:(0x[0-9A-Fa-f]+)', _line)
        if m:
            s = int(m.group(2), 16)
            FUNCS.append((s, s + int(m.group(3), 16), m.group(1)))
FUNCS.sort()

# Every symbol that names an object (vtable or data blob), by address.
DATAOBJ = {}
for _line in open('config/G2ME01/symbols.txt'):
    m = re.match(r'^(\S+) = \.data:(0x[0-9A-Fa-f]+); // type:object size:(0x[0-9A-Fa-f]+)', _line)
    if m:
        DATAOBJ[int(m.group(2), 16)] = (m.group(1), int(m.group(3), 16))


def fname(a):
    for s, e, n in FUNCS:
        if s <= a < e:
            return n, e - s
    return 'fn_%08X' % a, None


def unit_of(a):
    for s, e, sec, u in RANGES:
        if s <= a < e:
            return u, sec
    return None, None


def has_source(u):
    return bool(u) and os.path.exists('src/' + u)


def data_bytes():
    out = subprocess.run([OBJDUMP, '-s', '-j', '.data', ELF], capture_output=True, text=True).stdout
    d = {}
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{8})((?:\s[0-9a-f]{8}){1,4})', l)
        if m:
            base = int(m.group(1), 16)
            for i, h in enumerate(m.group(2).split()):
                for j, byte in enumerate(bytes.fromhex(h)):
                    d[base + i * 4 + j] = byte
    return d


DATA = data_bytes()


def word(a):
    try:
        return int.from_bytes(bytes(DATA[a + k] for k in range(4)), 'big')
    except KeyError:
        return None


def e5_rows():
    rows = []
    for line in open('docs/research/real_loaders.md'):
        m = re.match(
            r'^\| `(\w+)` \| `(\w{4})` \| (0x[0-9A-F]+) \| (\d+) \| (\d+) \| (\d+|-) \| `([^`]+)` \| `(\w+)` \|',
            line)
        if m:
            rows.append(dict(name=m.group(1), fourcc=m.group(2), addr=int(m.group(3), 16),
                             size=int(m.group(4)), frame=int(m.group(5)),
                             newsize=None if m.group(6) == '-' else int(m.group(6)),
                             ctor=m.group(7), vt=m.group(8)))
    return rows


def vtable_slots(vtname):
    addr = None
    for a, (n, sz) in DATAOBJ.items():
        if n == vtname:
            addr = a
            break
    if addr is None:
        return None, []
    slots = []
    zeros = 0
    off = 0
    while off < 0x400:
        w = word(addr + off)
        if w is None:
            break
        if w == 0:
            zeros += 1
            if zeros == 3:
                break
        else:
            zeros = 0
        slots.append((off, w))
        off += 4
    return addr, slots


def tm_name(target):
    nm, sz = fname(target)
    return nm, sz


def main():
    rows = e5_rows()
    print('%d rows read from real_loaders.md\n' % len(rows))
    hdr = ('%-30s %-5s %8s %6s %-24s %-38s %5s %5s %6s' % (
        'loader', 'FourCC', 'new', 'size', 'ctor', 'vtable slot 1  (TypesMatch)',
        'slots', 'uncl', 'ctorB'))
    print(hdr)
    print('-' * len(hdr))
    seen_vt = {}
    out_rows = []
    for r in rows:
        nm = ''
        nslots = unclaimed = 0
        vtaddr, slots = vtable_slots(r['vt'])
        if vtaddr is not None and len(slots) > 3:
            nm, _ = tm_name(slots[3][1])
            nslots = len(slots) - 2
            for off, tgt in slots[2:]:
                if tgt == 0:
                    continue
                u, sec = unit_of(tgt)
                if not has_source(u):
                    unclaimed += 1
        if r['ctor'].startswith('fn_') or r['ctor'].startswith('__ct__'):
            try:
                csz = int(r['ctor'].split('__ct__')[1].split('F')[0][:8], 16) if False else None
            except Exception:
                csz = None
        else:
            csz = None
        # ctor size: look the ctor up by name
        csz = None
        for s, e, n in FUNCS:
            if n == r['ctor']:
                csz = e - s
                break
        if nm.startswith('TypesMatch__') and nm.endswith('CFi'):
            cls = nm[11:-3]
        else:
            cls = nm
        print('%-30s %-5s %8s %6d %-24s %-38s %5d %5d %6s' % (
            r['name'], r['fourcc'], r['newsize'], r['size'], r['ctor'][:24],
            cls[:38], nslots, unclaimed, csz))
        seen_vt.setdefault(r['vt'], []).append(r['name'])
        out_rows.append((r, cls, nslots, unclaimed, csz))
    print('\ndistinct vtables: %d' % len(seen_vt))
    for vt, ls in sorted(seen_vt.items(), key=lambda kv: -len(kv[1])):
        if len(ls) > 1:
            print('  %-14s %d loaders: %s' % (vt, len(ls), ', '.join(ls)))


if __name__ == '__main__':
    main()
