#!/usr/bin/env python3
"""Generate the port's link-stub translation unit from the reachability analysis.

Reads `docs/research/boot_path_stubbable.tsv` (written by `tools/link_reach.py`)
and writes `src/MetroidPrime/PortLinkStubs.cpp`.

**What this is.** The port's link asks the linker for 523 symbols that nothing in
the tree defines. 181 of them are referenced *only* by objects that are not
reachable from the program's roots, so a definition for them cannot change what
the game does - it can only let the link finish. Those get a stub here.

**What this is not.** It is not decompilation, and a stub is not a
decompilation. Nothing in this file is claimed to match retail; `configure.py`
does not mention it, so it cannot affect `main.dol` or the 86 REL modules. The
182nd stub and the count are stated in the header so a reader never has to guess
what is real.

**How a definition is made without knowing the signature.** The linker resolves
the *mangled* name, so the name has to be spelled exactly, and it is not a legal
C++ identifier. `extern "C"` plus an `asm` label does it:

    extern "C" void stub_17() asm("_ZN6CActor12CreateShadowEb");
    extern "C" void stub_17() {}

The signature is deliberately the emptiest one available. That is safe **because
these symbols are unreachable** - a wrong signature would corrupt the stack if
the function were ever called, and the whole justification for the file is that
it is not. If one of these ever becomes reachable, the stub is a bug, and
`tools/link_reach.py` will move it into the reachable set on the next run.

Run: `python3 tools/gen_link_stubs.py`
"""
import collections
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
TSV = ROOT / 'docs' / 'research' / 'boot_path_stubbable.tsv'
OUT = ROOT / 'src' / 'MetroidPrime' / 'PortLinkStubs.cpp'

rows = []
for line in TSV.read_text().split('\n'):
    if not line.strip() or line.startswith('#'):
        continue
    parts = line.split('\t')
    if len(parts) >= 2:
        rows.append((parts[0], parts[1], parts[2] if len(parts) > 2 else ''))

DATA = re.compile(r'^(vtable for |typeinfo for |VTT for |construction vtable for )')
funcs = [(m, d) for m, d, _ in rows if not DATA.match(d)]
datas = [(m, d) for m, d, _ in rows if DATA.match(d)]

# One definition can satisfy several demangled names only if they are the same
# mangled symbol; group so each mangled name is emitted exactly once.
seen, uniq_f, uniq_d = set(), [], []
for m, d in funcs:
    if m not in seen:
        seen.add(m); uniq_f.append((m, d))
for m, d in datas:
    if m not in seen:
        seen.add(m); uniq_d.append((m, d))

by_class = collections.Counter(
    ('REL loader' if re.match(r'^\d*Load\w+__F', d) or d.startswith('Load') else
     'unmangled fn_/lbl_' if re.match(r'^(fn_|lbl_)', d) else
     'vtable/typeinfo' if DATA.match(d) else
     'game method')
    for _, d in uniq_f + uniq_d)

head = f'''/**
 * Port link stubs - GENERATED, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py
 *   input:     docs/research/boot_path_stubbable.tsv  (from tools/link_reach.py)
 *
 * The port's link asked for 523 symbols that nothing in the tree defines. This
 * file supplies {len(uniq_f) + len(uniq_d)} of them: the ones referenced **only by
 * objects unreachable from the program's roots**, so a definition cannot change
 * what the game does and can only let the link finish.
 *
 *   {len(uniq_f)} functions, {len(uniq_d)} data objects.
 *
 * Breakdown: {', '.join(f'{v} {k}' for k, v in by_class.most_common())}.
 *
 * **None of this is decompilation and none of it is claimed to match retail.**
 * `configure.py` does not mention this file, so it cannot affect `main.dol` or
 * any of the 86 REL modules - it exists only in the port's build. What the
 * decompilation still owes is the other 342 undefined symbols, which
 * `tools/link_reach.py` says are referenced by objects that *are* reachable and
 * so cannot be stubbed blind.
 *
 * **How a symbol is defined without its signature.** The linker resolves the
 * mangled name and it is not a legal identifier, so `extern "C"` plus an `asm`
 * label carries it verbatim. The signature is the emptiest available, which is
 * only safe because the symbol is unreachable - and `tools/link_reach.py` will
 * move it into the reachable set if that ever stops being true, which is exactly
 * when a stub here would become a bug.
 */

'''

body = []
for i, (m, d) in enumerate(uniq_f):
    body.append(f'// {d}\n'
                f'extern "C" void stub_{i}() asm("{m}");\n'
                f'extern "C" void stub_{i}() {{}}\n')
if uniq_d:
    body.append('\n// Data objects. A vtable or typeinfo stub is zero-filled: harmless to take the\n'
                '// address of, and a crash if used - which unreachable means it is not.\n'
                '//\n'
                '// The `= {}` is load-bearing. A tentative definition with no initialiser is\n'
                '// discarded as unused and the symbol never reaches the object file, which\n'
                '// looks exactly like the stub not working. Measured, not assumed.\n')
for j, (m, d) in enumerate(uniq_d):
    body.append(f'// {d}\n'
                f'extern "C" char stub_data_{j}[64] asm("{m}") = {{}};\n')

OUT.write_text(head + '\n'.join(body))
print(f'wrote {OUT.relative_to(ROOT)}: {len(uniq_f)} functions, {len(uniq_d)} data objects')
for k, v in by_class.most_common():
    print(f'  {v:4d}  {k}')
