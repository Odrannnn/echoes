#!/usr/bin/env python3
"""Lane e5: classify the 86 real REL entity loaders.

For each loader it recovers, from the DOL disassembly only:

  * the size and the (string) class name of every `new` site,
  * the constructor called on the fresh object,
  * the vtable address that constructor installs (offset 0 of the object),
  * whether any symbol in config/G2ME01/symbols.txt covers that vtable, and
    whether that symbol is defined by a `Matching` unit in configure.py.

The last two are what decide "can this loader ever be Matching in this tree".
"""
import re
import json
import sys

OBJDUMP = 'build/binutils/powerpc-eabi-objdump'
ELF = 'build/G2ME01/main.elf'
NW = 0x802CE278
NWA = 0x802CE224

_dis = {}
_ro = {}
_nm = {}
_symtab = []


def dis():
    if not _dis:
        import subprocess
        out = subprocess.run([OBJDUMP, '-d', ELF], capture_output=True, text=True).stdout
        for l in out.splitlines():
            m = re.match(r'^([0-9a-f]{8}):\t([0-9a-f ]+)\t(.*)$', l)
            if m:
                _dis[int(m.group(1), 16)] = m.group(3).strip()
    return _dis


def rodata():
    if not _ro:
        import subprocess
        out = subprocess.run([OBJDUMP, '-s', '-j', '.rodata', ELF],
                             capture_output=True, text=True).stdout
        for l in out.splitlines():
            m = re.match(r'^\s*([0-9a-f]{8})((?:\s[0-9a-f]{8}){1,4})\s', l + ' ')
            if m:
                base = int(m.group(1), 16)
                for i, h in enumerate(m.group(2).split()):
                    for j, byte in enumerate(bytes.fromhex(h)):
                        _ro[base + i * 4 + j] = byte
    return _ro


def cstr(addr):
    out = bytearray()
    a = addr
    while a in _ro and _ro[a]:
        out.append(_ro[a])
        a += 1
    return out.decode('latin1')


def syms():
    if not _nm:
        import subprocess
        out = subprocess.run(['build/binutils/powerpc-eabi-nm', ELF],
                             capture_output=True, text=True).stdout
        for l in out.splitlines():
            p = l.split()
            if len(p) == 3:
                _nm[int(p[0], 16)] = p[2]
    return _nm


def data_words(addr, n):
    d = dis()  # noqa - keep import order
    out = []
    for i in range(n):
        a = addr + i * 4
        b = bytes(_ro.get(a + k, 0) for k in range(4))
        out.append(int.from_bytes(b, 'big'))
    return out


def load_symbol_table():
    """[(start, size, name)] for every symbol in symbols.txt, sorted by start."""
    global _symtab
    if not _symtab:
        for l in open('config/G2ME01/symbols.txt'):
            m = re.match(r'^(\S+) = \.(\w+):(0x[0-9A-Fa-f]+);', l)
            if not m:
                continue
            sz = re.search(r'size:(0x[0-9A-Fa-f]+)', l)
            _symtab.append((int(m.group(3), 16), m.group(1), m.group(2),
                            int(sz.group(1), 16) if sz else 0))
        _symtab.sort()
    return _symtab


def enclosing(addr):
    best = None
    for start, name, sec, size in load_symbol_table():
        if start == addr:
            return name, start, size
        if start <= addr and (best is None or start > best[0]):
            best = (start, name, sec, size)
    return (best[1], best[0], best[3]) if best else (None, None, 0)


def _track(vals, ins):
    m = re.match(r'^(?:lis|li)\s+(r\d+),(-?0x[0-9a-f]+|-?\d+)$', ins)
    if m:
        v = int(m.group(2), 0) & 0xFFFFFFFF
        vals[m.group(1)] = (v << 16) & 0xFFFFFFFF if m.group(0).startswith('lis') else v
        return True
    m = re.match(r'^(?:addi|ori|addis)\s+(r\d+),(r\d+),(-?0x[0-9a-f]+|-?\d+)$', ins)
    if m:
        d, a, v = m.group(1), m.group(2), int(m.group(3), 0)
        if a in vals:
            vals[d] = (vals[a] + v) & 0xFFFFFFFF
        else:
            vals.pop(d, None)
        return True
    return False


def const_walk(start, stop):
    """Track lis/li/addi/ori into registers over the window, resetting on branches."""
    d = dis()
    vals = {}
    a = start
    while a < stop:
        ins = d.get(a)
        if ins is None:
            a += 4
            continue
        if re.match(r'^(b|beq|bne|blt|bgt|ble|bge|cmp|cmpl|cmpw|cmplwi)', ins):
            vals = {}
        _track(vals, ins)
        a += 4
    return vals


def vtable_of(fn_addr, limit=0x200):
    """Disassemble a constructor; return the vtable address it stores at obj+0."""
    d = dis()
    end = None
    for start, name, sec, size in load_symbol_table():
        if start == fn_addr:
            end = start + size
    if end is None or end - fn_addr > limit:
        end = fn_addr + limit
    vals = {}
    a = fn_addr
    while a < end:
        ins = d.get(a)
        if ins:
            _track(vals, ins)
            m = re.match(r'^stw\s+(r\d+),\s*0\((r\d+)\)$', ins)
            if m and m.group(1) in vals:
                return vals[m.group(1)]
        a += 4
    return None


def analyse(name, fourcc, addr, size, frame, rsym):
    d = dis()
    s = syms()
    end = addr + size
    news = []
    calls = []
    new_sites = []
    a = addr
    while a < end:
        ins = d.get(a)
        if ins:
            m = re.match(r'^bl\s+([0-9a-f]+)', ins)
            if m:
                tgt = int(m.group(1), 16)
                nm = s.get(tgt, 'sub_%08X' % tgt)
                calls.append((a, tgt, nm))
                if tgt in (NW, NWA):
                    v = const_walk(max(addr, a - 96), a)
                    new_sites.append(dict(at=a, op=nm, size=v.get('r3'),
                                          cls=cstr(v['r4']) if 'r4' in v else None))
        a += 4
    # for each new site, the ctor is the nearest later call that installs a vtable
    for ns in new_sites:
        cands = []
        for (at, tgt, nm) in calls:
            if at <= ns['at'] or at > ns['at'] + 0x300:
                continue
            vt = vtable_of(tgt, 0x300)
            if vt and 0x803B0C00 <= vt < 0x80405A10:
                cands.append((at, tgt, nm, vt))
        ns['ctor'] = cands[0][1] if cands else None
        ns['ctorname'] = cands[0][2] if cands else None
        ns['vtable'] = cands[0][3] if cands else None
        news.append(ns)
    return dict(name=name, fourcc=fourcc, addr=addr, size=size, frame=frame,
                rsym=rsym, news=news, calls=sorted({n for _, _, n in calls}))


def main():
    rows = json.load(open('/tmp/opencode/e5_loaders.json'))
    out = []
    for row in rows:
        o = analyse(*row)
        for n in o['news']:
            vt = n.get('vtable')
            if vt:
                nm, start, size = enclosing(vt)
                n['vt_sym'] = nm
                n['vt_off'] = vt - start if start else None
                cnt = 0
                for w in data_words(vt, 0x100):
                    if 0x80003840 <= w < 0x803A54A0:
                        cnt += 1
                    else:
                        break
                n['vt_slots'] = cnt
            else:
                n['vt_sym'] = None
                n['vt_off'] = None
                n['vt_slots'] = 0
        out.append(o)
    json.dump(out, open('/tmp/opencode/e5_loaders_an.json', 'w'), indent=1)
    print('%-34s %5s %6s %5s %-52s %s' % (
        'loader', 'size', 'newsz', 'nvt', 'ctor', 'vtable'))
    for o in out:
        if not o['news']:
            print('%-34s %5d  (no new site)' % (o['name'], o['size']))
        for n in o['news']:
            print('%-34s %5d %6s %5d %-52s %s+0x%X' % (
                o['name'], o['size'], n.get('size'), n.get('vt_slots') or 0,
                str(n.get('ctorname'))[:52], str(n.get('vt_sym')), n.get('vt_off') or 0))


if __name__ == '__main__':
    main()
