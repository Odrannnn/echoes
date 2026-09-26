#!/usr/bin/env python3
"""Pair each undefined symbol in the port's link with the object that references it.

ld.bfd prints the referencing object on one line and the symbol on the next:

    ld.bfd: CMakeFiles/mp_game.dir/src/.../CStaticAudioPlayer.cpp.o: in function `X':
    /path/src/....cpp:24:(.text+0x56): undefined reference to `SYM'

so the object has to be carried forward. That pairing is what tells a shim
author which single definition closes the most symbols, and it is not visible
from `nm` set arithmetic - which is why tools/link_check.sh is the ground
truth and link_gap.py is not.
"""
import collections
import pathlib
import re
import sys

log = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'build-port-link/build.log')
lines = log.read_text(errors='replace').split('\n')

OBJ = re.compile(r'ld\.bfd: (\S+\.(?:o|obj))(?::| )')
SYM = re.compile(r'undefined reference to `(.+?)\'')

refs = collections.defaultdict(set)
cur = None
for ln in lines:
    m = OBJ.search(ln)
    if m:
        cur = m.group(1).split('/')[-1]
        continue
    m = SYM.search(ln)
    if m and cur:
        refs[m.group(1)].add(cur)

print(f'undefined symbols: {len(refs)}')

objs = collections.Counter()
for _, v in refs.items():
    for o in v:
        objs[o] += 1
print('\ntop referencing objects (one shim per object closes this many):')
for o, n in objs.most_common(16):
    print(f'  {n:4d}  {o}')

out = pathlib.Path('/tmp/opencode/undef_by_obj.txt')
out.write_text('\n'.join(f'{s}\t{",".join(sorted(v))}' for s, v in sorted(refs.items())))
print(f'\nsaved {out}')
