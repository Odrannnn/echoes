import re, sys
mod = sys.argv[1]
txt = open('build/G2ME01/%s/ldscript.lcf' % mod).read()
m = re.search(r'FORCEACTIVE\s*\{(.*?)\}', txt, re.S)
active = set(l.strip() for l in m.group(1).splitlines() if l.strip())
fns = [(n, int(a, 16), int(sz, 16)) for n, a, sz in re.findall(
    r'(?m)^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)',
    open('config/G2ME01/rels/%s/symbols.txt' % mod).read())]
byaddr = dict((a, (n, s)) for n, a, s in fns)
print("%s: %d module functions, %d FORCEACTIVE entries, %d of them module functions"
      % (mod, len(fns), len(active), len([1 for n, a, s in fns if n in active])))

claims = []
path = None
for line in open('config/G2ME01/rels/%s/splits.txt' % mod):
    mm = re.match(r'^(\S.*):$', line)
    if mm:
        path = mm.group(1); continue
    mm = re.match(r'^\s+\.text\s+start:0x([0-9A-F]+) end:0x([0-9A-F]+)', line)
    if mm and path:
        claims.append((path, int(mm.group(1), 16), int(mm.group(2), 16)))
for path, a, b in claims:
    names = [byaddr[x][0] for x in sorted(byaddr) if a <= x and x + byaddr[x][1] <= b]
    bad = [n for n in names if n not in active]
    print("  %-52s 0x%06X..0x%06X %2d fns, %2d NOT active %s"
          % (path, a, b, len(names), len(bad), bad if bad else ''))
