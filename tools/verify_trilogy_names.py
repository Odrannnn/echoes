#!/usr/bin/env python3
"""Check Trilogy-name proposals for unnamed (fn_/lbl_) symbols against retail's argument registers.

    python3 tools/verify_trilogy_names.py docs/research/trilogy_name_proposals.tsv

For each `unnamed` row, parses the proposed Itanium-free MWCC mangling into the registers its
parameters need (r3 = this for a method; ref/pointer/int/bool/enum/TUniqueId = one GPR; float = one
FPR; by-value class = ambiguous) and compares that with the registers retail reads before writing
them, in straight-line order from the function's entry. Prints OK / FEWER (retail reads fewer
registers than the name says - an unused trailing parameter is possible) / MORE (retail reads a
register the name does not account for - the name is wrong or incomplete) / AMBIG. A register check
cannot prove a name; it can only refute one. Data labels are reported with their size and section.
"""
import re, subprocess, sys

OBJDUMP = 'build/binutils/powerpc-eabi-objdump'
ELF = 'build/G2ME01/main.elf'
BUILTIN = {'i': 'g', 'b': 'g', 's': 'g', 'c': 'g', 'l': 'g', 'f': 'f', 'd': 'f', 'v': 'v'}
GPR_CLASSES = ('9TUniqueId', '7TAreaId', '9TEditorId', '6CSegId', '7TAreaId')


def syms():
    d = {}
    for l in open('config/G2ME01/symbols.txt'):
        m = re.match(r'^(\S+) = (\.\w+):(0x[0-9A-Fa-f]+); // (.*)', l)
        if m:
            sz = re.search(r'size:(0x[0-9A-Fa-f]+)', m.group(4))
            d[int(m.group(3), 16)] = (m.group(1), m.group(2), int(sz.group(1), 16) if sz else 0)
    return d


def skipname(s, i):
    # at digit: length-prefixed name
    j = i
    while s[j].isdigit():
        j += 1
    n = int(s[i:j])
    return j + n


def ptype(s, i):
    """consume one type, return (kind, next index)."""
    kind = None
    while s[i] in 'CUS' or s[i] in 'RP':
        if s[i] in 'RP':
            _, j = ptype(s, i + 1)
            return 'g', j
        i += 1
    c = s[i]
    if c in BUILTIN:
        return BUILTIN[c], i + 1
    if c == 'x':
        return 'gg', i + 1
    if c.isdigit():
        j = skipname(s, i)
        return ('g' if s[i:j] in GPR_CLASSES else 'obj:' + s[i:j][:30]), j
    if c == 'Q':
        n = int(s[i + 1]); j = i + 2
        for _ in range(n):
            j = skipname(s, j)
        return 'obj:Q', j
    if c == 'T':
        return 'obj:T', i + 2
    raise ValueError('type at %r' % s[i:i + 10])


def regs(name):
    m = re.search(r'^(?:__\w\w__|[A-Za-z_][\w<>,]*?__)', name)
    if not m:
        return None
    i = m.end()
    cls = False
    if name[i].isdigit():
        i = skipname(name, i); cls = True
    elif name[i] == 'Q':
        n = int(name[i + 1]); i += 2
        for _ in range(n):
            i = skipname(name, i)
        cls = True
    const = False
    if name[i] == 'C':
        i += 1
    if name[i] != 'F':
        return None
    i += 1
    g = 1 if cls else 0
    f = 0; amb = []
    while i < len(name):
        k, i = ptype(name, i)
        if k == 'g': g += 1
        elif k == 'gg': g += 2
        elif k == 'f': f += 1
        elif k.startswith('obj'): amb.append(k)
    return g, f, amb, name.startswith('__dt__')


def entry_reads(addr, size):
    out = subprocess.run([OBJDUMP, '-d', '--start-address=%d' % addr, '--stop-address=%d' % (addr + size), ELF],
                         capture_output=True, text=True).stdout
    written = set(); read = set()
    for l in out.splitlines():
        m = re.match(r'\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} ){4}\s*(\w+\.?)\s*(.*)', l)
        if not m:
            continue
        op, args = m.group(1), m.group(2).split('#')[0].strip()
        ops = [a.strip() for a in args.split(',')] if args else []
        if op == 'blr':
            break
        # operand regs: r\d or f\d, including inside d(rN)
        toks = [re.findall(r'\b([rf]\d+)\b', a) for a in ops]
        dest = set(); src = []
        if op.startswith(('st', 'cmp', 'b', 'mt', 'fcmp', 'dcb', 'icb')) and not op.startswith(('stw', 'sth', 'stb')) or True:
            pass
        store = op.startswith('st') or op.startswith(('cmp', 'fcmp', 'b', 'mtctr', 'mtlr', 'dcb'))
        if store:
            for t in toks:
                src += t
            if op.startswith('stwu') or op.startswith('stbu') or op.startswith('sthu') or op.startswith('stfsu'):
                pass
        else:
            if toks:
                dest = set(toks[0])
                for t in toks[1:]:
                    src += t
        for r in src:
            if r not in written:
                read.add(r)
        written |= dest
        if op.startswith(('bl', 'bctrl')) and not op.startswith(('ble', 'blt')) and op in ('bl', 'bla', 'bctrl', 'bctrl+'):
            # call clobbers volatile arg regs; stop scanning, later reads are of results
            break
    g = max([int(r[1:]) for r in read if r[0] == 'r' and 3 <= int(r[1:]) <= 10] or [2]) - 2
    fp = max([int(r[1:]) for r in read if r[0] == 'f' and 1 <= int(r[1:]) <= 8] or [0])
    return g, fp


def main():
    S = syms()
    for l in open(sys.argv[1]).read().splitlines()[1:]:
        kind, addr, votes, total, cur, prop = l.split('\t')
        if kind != 'unnamed':
            continue
        a = int(addr, 16)
        if cur.startswith('lbl_'):
            n, sec, sz = S[a]
            print('DATA  %s %s %s size=%#x  %s' % (addr, sec, votes, sz, prop))
            continue
        n, sec, sz = S[a]
        try:
            r = regs(prop)
        except Exception as e:
            r = None
        if r is None:
            print('PARSE %s %s' % (addr, prop[:60])); continue
        eg, ef, amb, dt = r
        rg, rf = entry_reads(a, sz)
        if dt: eg += 1   # hidden delete flag in r4
        v = 'OK   '
        if amb: v = 'AMBIG'
        if rg > eg or rf > ef: v = 'MORE '
        elif rg < eg or rf < ef:
            if v == 'OK   ': v = 'FEWER'
        print('%s %s votes=%s name:g%d/f%d retail:g%d/f%d %s %s' % (v, addr, votes, eg, ef, rg, rf, ','.join(amb), prop[:70]))


main()
