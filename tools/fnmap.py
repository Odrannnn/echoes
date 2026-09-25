#!/usr/bin/env python3
"""Byte-identical function pairing between a unit's retail object and our compile.

    python3 tools/fnmap.py Kyoto/CPakFile

Prints every function of `build/G2ME01/obj/<unit>.o` beside the function of
`build/G2ME01/src/<unit>.o` that has the same instruction bytes, then the functions only we emit.
That pairing is the mechanical part of landing a ported unit: the code already matches, only the
name differs, so `tools/apply_rename.py` can name the retail symbol after ours and objdiff will
score it. DOL units only (REL units live under build/G2ME01/<Module>/).

From lane W16, 2026-09-25.
"""
import subprocess, sys, hashlib, re

o = sys.argv[1]
b = 'build/binutils/powerpc-eabi-objdump'
out = {}
for side, path in (('retail', f'build/G2ME01/obj/{o}.o'), ('ours', f'build/G2ME01/src/{o}.o')):
    txt = subprocess.run([b, '-d', '--section=.text', path], capture_output=True, text=True).stdout
    funcs, cur = [], None
    for line in txt.splitlines():
        m = re.match(r'^([0-9a-f]{8,16}) <(.+)>:$', line.strip())
        if m:
            cur = {'addr': int(m.group(1), 16), 'name': m.group(2), 'ins': []}
            funcs.append(cur)
        elif cur is not None and re.match(r'^\s+[0-9a-f]+:\s', line):
            parts = line.split('\t')
            cur['ins'].append(parts[1].strip() if len(parts) > 1 else '')
    for f in funcs:
        f['hash'] = hashlib.md5(''.join(f['ins']).encode()).hexdigest()[:8]
        f['size'] = 0
    for i, f in enumerate(funcs):
        f['size'] = (funcs[i + 1]['addr'] - f['addr']) if i + 1 < len(funcs) else 0
    out[side] = funcs

r = out.get('retail', [])
u = out.get('ours', [])
rh = {}
for f in r:
    rh.setdefault(f['hash'], []).append(f)
print(f'{"retail":<52} {"size":>5}  {"ours":<52} {"size":>5}')
for f in r:
    m = rh.get(f['hash'], [])
    same = [x for x in u if x['hash'] == f['hash']]
    tag = 'IDENTICAL: ' + same[0]['name'] if same else ''
    print(f'{f["name"][:52]:<52} {f["size"]:>5}  {tag}')
print('--- ours only:')
for f in u:
    if not any(x['hash'] == f['hash'] for x in r):
        print(f'  {"":<52} {"":>5}  {f["name"][:52]:<52} {f["size"]:>5}')
