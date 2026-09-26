#!/usr/bin/env python3
"""Which undefined symbols are *called* on the boot path, as distinct from merely referenced.

`tools/link_reach.py` answers its question per **object** and it says so: "reachable" is
an upper bound, and a stub on a reachable object is refused because a wrong stub on the
boot path is a crash. The bound is loose in one specific, measurable way, and this tool
measures it.

An object is reachable if *anything* in it is. `src/MetroidPrime/Player/CPlayerGun.cpp.o`
carries a static initialiser (it builds the two `SGunStateFunc` tables), so it is a root
under the second rule in `link_reach.py` - it runs before `main` - and **every** symbol
it references is therefore "reachable", including 22 `CPlayerGun` methods that only ever
execute once a `CPlayer` exists. 342 symbols are unstubbable for exactly this reason, and
a large share of them are not on the boot path at all.

This tool refines the granularity to the **referencing site**:

  1. pair each undefined symbol with the object that references it (ld.bfd's own
     attribution, from the link log);
  2. for that object, find the *relocation site* - the section, and the enclosing
     function when the site is in `.text`;
  3. classify the site:
       data     - the reference is a pointer in a static table or a vtable, so the
                  linker must resolve it but nothing calls it at this point
       pre-main - the enclosing function is `_GLOBAL__sub_I*`, i.e. a static
                  initialiser, which runs before `main`
       call     - the enclosing function is real code
  4. for a `call` site, also report the distance of the *object* from a root, which is
     what `link_reach.py` already computes and is reproduced here only so the two tools
     can be compared. **It is not the distance of the site.** Granularity is still the
     object, so a function deep inside a root object reads as 0 however late it runs.

Output is a TSV. What this tool deliberately does NOT do is decide that a symbol is safe
to stub: an indirect call through a vtable or a function-pointer table is invisible to a
relocation-derived graph, so the graph is a lower bound, and a symbol whose only sites are
`data` is *not* thereby proven unreachable - the table it sits in may be dispatched later.
`docs/research/boot_path.md` is the other half of the answer: it is the hand-measured
25-step map from the tree to a rendered frame, and a site is on the boot path only if that
map says so. The two together are what partition a block; neither alone is enough.

    tools/link_fn_reach.py [boot_guns.txt] [--out FILE]

Requires build-port-link/build.log, i.e. tools/link_check.sh has been run.
"""
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOG = ROOT / 'build-port-link' / 'build.log'
BUILD = ROOT / 'build-port-link'

OBJ_LINE = re.compile(r'(?:ld\.bfd: )?(\S+?\.(?:o|obj))(?:\([^)]*\))?(?::| )')
SYM = re.compile(r'undefined reference to `(.+?)\'')


def referencing_objects(text):
    """ld.bfd prints the object on the `in function' line and the symbol on the next."""
    refs = collections.defaultdict(set)
    cur = None
    for ln in text.split('\n'):
        if 'in function' in ln:
            m = OBJ_LINE.search(ln)
            if m:
                cur = m.group(1).split('/')[-1]
        m = SYM.search(ln)
        if m and cur:
            refs[m.group(1)].add(cur)
    return refs


def demangle(names):
    if not names:
        return {}
    out = subprocess.run(['c++filt'], input='\n'.join(names),
                         capture_output=True, text=True).stdout.split('\n')
    return dict(zip(names, out))


def sites_and_edges(path, wanted):
    """Every relocation of `path': the section, the enclosing function, and the callee.

    Returns (sites, defined, callers) where `sites' maps a demangled undefined symbol to
    the set of (section, function) that reference it.
    """
    import bisect
    dis = subprocess.run(['objdump', '-dr', '-C', str(path)],
                         capture_output=True, text=True).stdout
    sec_starts, sec_names, cur = collections.defaultdict(list), \
        collections.defaultdict(list), None
    for ln in dis.split('\n'):
        m = re.search(r'Disassembly of section (\S+):', ln)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'^([0-9a-f]+) <(.+)>:', ln)
        if m and cur:
            sec_starts[cur].append(int(m.group(1), 16))
            sec_names[cur].append(m.group(2))

    def fn_at(sec, off):
        st = sec_starts.get(sec)
        if not st:
            return None
        i = bisect.bisect_right(st, off) - 1
        return sec_names[sec][i] if i >= 0 else None

    rel = subprocess.run(['objdump', '-r', '-C', str(path)],
                         capture_output=True, text=True).stdout
    sites = collections.defaultdict(set)
    callers = collections.defaultdict(set)
    cur = None
    for ln in rel.split('\n'):
        m = re.match(r'RELOCATION RECORDS FOR \[(\S+)\]', ln)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'^([0-9a-f]+) R_X86_64_\S+\s+(.*)$', ln)
        if not m:
            continue
        off = int(m.group(1), 16)
        val = re.sub(r'-0x[0-9a-f]+$', '', m.group(2)).strip()
        if val in wanted:
            fn = fn_at(cur, off) if cur and cur.startswith('.text') else None
            sites[val].add((cur, fn or '<static data: relocation only, no code>'))
        elif cur and cur.startswith('.text'):
            f = fn_at(cur, off)
            if f:
                callers[f].add(val)
    defined = {m.group(2) for ln in dis.split('\n')
               if (m := re.match(r'^([0-9a-f]+) <(.+)>:', ln))}
    return sites, defined, callers


def main():
    argv = sys.argv[1:]
    out = None
    pos = []
    i = 0
    while i < len(argv):
        if argv[i] == '--out' and i + 1 < len(argv):
            out = argv[i + 1]
            i += 2
            continue
        if not argv[i].startswith('-'):
            pos.append(argv[i])
        i += 1
    args, out = pos, out
    if not LOG.exists():
        sys.exit(f'missing {LOG}; run tools/link_check.sh first')
    text = LOG.read_text(errors='replace')
    refs = referencing_objects(text)

    objs = sorted({o for v in refs.values() for o in v})
    mangled_of = demangle(sorted(refs))
    # demangled symbol -> set of objects, so the object scan can filter on it
    by_obj = collections.defaultdict(set)
    for dem, ob in mangled_of.items():
        for o in refs[dem]:
            by_obj[o].add(dem)

    all_sites, defined_of, callers_of = {}, {}, {}
    for o in objs:
        path = BUILD / 'CMakeFiles' / 'mp_game.dir' / 'src' / o
        if not path.exists():
            hits = list(BUILD.rglob(o))
            if not hits:
                sys.exit(f'stopping: no object named {o} under {BUILD}')
            path = hits[0]
        wanted = by_obj[o]
        sites, defined, callers = sites_and_edges(path, wanted)
        all_sites.update(sites)
        defined_of[o] = defined
        callers_of[o] = callers

    # Roots: an object that emits a static initialiser, plus the entry/platform objects.
    roots = {o for o, d in defined_of.items()
             if any(s.startswith('_GLOBAL__sub_I') for s in d)}
    roots |= {o for o in defined_of
              if 'mp_port_entry.dir' in str(o) or 'mp_platform.dir' in str(o)}

    # Callers are recorded by *demangled function name*, which is not unique across
    # objects, so a hop is counted at object granularity when it is unambiguous and
    # reported as `-1` when the name is defined in more than one object.
    prov = collections.defaultdict(set)
    for o, d in defined_of.items():
        for f in d:
            prov[f].add(o)
    dist = {o: 0 for o in roots}
    frontier = set(roots)
    while frontier:
        nxt = set()
        for o in frontier:
            for fn, callees in callers_of[o].items():
                for c in callees:
                    for p in prov.get(c, ()):
                        if p not in dist:
                            dist[p] = dist[o] + 1
                            nxt.add(p)
        frontier = nxt

    sel = set()
    for a in args:
        p = pathlib.Path(a)
        if p.exists():
            sel |= {ln.strip() for ln in p.read_text().split('\n') if ln.strip()}
        else:
            sel.add(a)
    if not sel:
        sel = set(refs)
    rows = []
    for dem in sorted(sel):
        if dem not in refs:
            rows.append((dem, '-', '-', '-', '-', '-', 'not in the link\'s undefined set'))
            continue
        for o in sorted(refs[dem]):
            d = dist.get(o)
            for sec, fn in sorted(all_sites.get(dem, {('-', '-')})):
                kind = ('pre-main' if fn.startswith('_GLOBAL__sub_I')
                        else 'data' if sec and not sec.startswith('.text')
                        else 'call')
                hops = '-' if kind != 'call' or d is None else str(d)
                rows.append((dem, o, sec, fn, kind, hops,
                             f'{len(refs[dem])} object(s) reference it'))
    hdr = ('symbol', 'referencing object', 'section', 'referencing function', 'site kind',
           "object's hops from a root (NOT the site's)", 'note')
    body = '\n'.join('\t'.join(r) for r in rows)
    text_out = ('# Undefined symbols by referencing site, not by object. Generated by\n'
                '# tools/link_fn_reach.py - do not hand-edit. `site kind` is data /\n'
                '# pre-main / call. A `data` site means the linker must resolve the\n'
                '# symbol, not that anything calls it. The hops column is the distance of\n'
                '# the OBJECT from a root, which is link_reach.py\'s number and is not the\n'
                '# distance of the site; a function inside a root object reads 0 however\n'
                '# late it runs. See docs/research/boot_path.md for the boot path.\n'
                + '\t'.join(hdr) + '\n' + body + '\n')
    if out:
        pathlib.Path(out).write_text(text_out)
        kinds = collections.Counter(r[4] for r in rows)
        print(f'wrote {out}: {len(rows)} row(s) over {len(sel)} symbol(s)')
        for k, n in sorted(kinds.items()):
            print(f'  {k:8s} {n}')
    else:
        print(text_out)


if __name__ == '__main__':
    main()
