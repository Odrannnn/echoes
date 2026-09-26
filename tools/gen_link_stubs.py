#!/usr/bin/env python3
"""Generate a port link-stub translation unit from the reachability analysis.

Two modes, and the difference between them is the whole point:

    python3 tools/gen_link_stubs.py                 # the 181 PROVEN-unreachable
    python3 tools/gen_link_stubs.py --reachable     # the 318 REACHABLE - DIAGNOSTIC ONLY

Reads `docs/research/boot_path_stubbable.tsv` or `docs/research/boot_path_reachable.tsv`
(both written by `tools/link_reach.py`) and writes `src/MetroidPrime/PortLinkStubs.cpp`
or `src/MetroidPrime/PortReachStubs.cpp`.

**The default mode is a real, permanent part of the port.** Those symbols are referenced only by
objects unreachable from the program's roots, so a definition cannot change what the game does -
it can only let the link finish. That is a sound claim and the gate depends on it.

**`--reachable` is a DIAGNOSTIC and must never be in the port's real build.** It stubs symbols
that the boot path *does* reach, so the program will run and then behave wrongly. Its only
purpose is to make the boot path's requirements **observable in order**: each stub logs its own
mangled and demangled name on entry, so one run prints the sequence of game functions the port
actually asks for, in the order it asks for them.

That is information no static analysis in this project can produce. `tools/link_reach.py` says
which symbols are reachable; only running the program says *which it reaches first, and in what
order*, and that order is what tells a lane what to write next.

**How a definition is made without knowing the signature.** The linker resolves the *mangled*
name, which is not a legal C++ identifier, so `extern "C"` plus an `asm` label carries it
verbatim:

    extern "C" void stub_0() asm("_ZN6CActor12CreateShadowEb");
    extern "C" void stub_0() {}

The signature is deliberately the emptiest one available. That is sound **only** in the default
mode, where the symbol is unreachable. In `--reachable` mode the signature is a lie on purpose
and the log is the only trustworthy output.
"""
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REACHABLE = '--reachable' in sys.argv
TSV = ROOT / 'docs' / 'research' / ('boot_path_reachable.tsv' if REACHABLE
                                     else 'boot_path_stubbable.tsv')
OUT = ROOT / 'src' / 'MetroidPrime' / ('PortReachStubs.cpp' if REACHABLE
                                       else 'PortLinkStubs.cpp')

rows = []
for line in TSV.read_text().split('\n'):
    if not line.strip() or line.startswith('#'):
        continue
    parts = line.split('\t')
    if len(parts) >= 2:
        rows.append((parts[0], parts[1], parts[2] if len(parts) > 2 else ''))

DATA = re.compile(r'^(vtable for |typeinfo for |VTT for |construction vtable for )')
seen, uniq_f, uniq_d = set(), [], []
for m, d, _ in rows:
    if m in seen:
        continue
    seen.add(m)
    (uniq_d if DATA.match(d) else uniq_f).append((m, d))

by_class = collections.Counter(
    ('REL loader' if re.match(r'^\d*Load\w+__F', d) or d.startswith('Load') else
     'unmangled fn_/lbl_' if re.match(r'^(fn_|lbl_)', d) else
     'vtable/typeinfo' if DATA.match(d) else
     'game method')
    for _, d in uniq_f + uniq_d)

if REACHABLE:
    head = f'''/**
 * Port REACHABILITY STUBS - GENERATED, DIAGNOSTIC ONLY, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py --reachable
 *   input:     docs/research/boot_path_reachable.tsv  (from tools/link_reach.py)
 *   built by:  -DMP_BOOT_STUBS=ON, which NOTHING but tools/boot_probe.sh passes
 *
 * **THIS IS NOT PART OF THE PORT.** Every symbol here is referenced by an object the boot path
 * *does* reach, so these definitions are lies: the program will run, and then behave wrongly.
 * `link_check.sh` never sees this file, and the real link still fails on all
 * {len(uniq_f) + len(uniq_d)} of them.
 *
 * **What it is for.** `link_reach.py` can say which symbols are reachable. It cannot say which
 * one the game asks for *first*, or in what order - and the order is what tells a lane what to
 * write next. So each stub logs its own name on entry and returns. One run prints the sequence
 * the boot path actually demands:
 *
 *   [0007] _ZN13CGameAllocator10InitializeER10COsContext
 *   [0008] _ZN9CGameStateC1ER11CInputStreami
 *
 * A non-void function stubbed as `void()` returns whatever was in the return register, so the
 * port may fault later than the first stub. That is expected; the log up to the fault is still
 * ordered evidence, and the fault is itself the next thing to find.
 *
 * Breakdown: {', '.join(f'{v} {k}' for k, v in by_class.most_common())}.
 */

#include <cstdio>
#include <cstdlib>

namespace {{

unsigned g_stubSeq = 0;

}} // namespace

// One definition for all of them: the name is passed, not encoded in a symbol, so this stays
// readable and the cost is one PLT call per stub rather than {len(uniq_f)} near-identical bodies.
extern "C" void mpReachStub(const char* mangled, const char* demangled) {{
  std::fprintf(stderr, "[reach-stub %04u] %s   (%s)\\n", ++g_stubSeq, mangled, demangled);
  std::fflush(stderr);
}}

'''
else:
    head = f'''/**
 * Port link stubs - GENERATED, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py
 *   input:     docs/research/boot_path_stubbable.tsv  (from tools/link_reach.py)
 *
 * The port's link asked for 523 symbols that nothing in the tree defines. This file supplies
 * {len(uniq_f) + len(uniq_d)} of them: the ones referenced **only by objects unreachable from
 * the program's roots**, so a definition cannot change what the game does and can only let the
 * link finish.
 *
 *   {len(uniq_f)} functions, {len(uniq_d)} data objects.
 *
 * Breakdown: {', '.join(f'{v} {k}' for k, v in by_class.most_common())}.
 *
 * **None of this is decompilation and none of it is claimed to match retail.**
 * `configure.py` does not mention this file, so it cannot affect `main.dol` or any of the 86 REL
 * modules - it exists only in the port's build. What the decompilation still owes is the other
 * 342 undefined symbols, which `tools/link_reach.py` says are referenced by objects that *are*
 * reachable and so cannot be stubbed blind. `tools/gen_link_stubs.py --reachable` makes those
 * into a *diagnostic* build; this file is the sound half.
 *
 * **How a symbol is defined without its signature.** The linker resolves the mangled name and it
 * is not a legal C++ identifier, so `extern "C"` plus an `asm` label carries it verbatim. The
 * signature is the emptiest available, which is only sound because the symbol is unreachable - and
 * `tools/link_reach.py` will move it into the reachable set if that ever stops being true, which
 * is exactly when a stub here becomes a bug.
 */

'''

body = []
for i, (m, d) in enumerate(uniq_f):
    if REACHABLE:
        body.append(f'// {d}\n'
                    f'extern "C" void reachstub_{i}() asm("{m}");\n'
                    f'extern "C" void reachstub_{i}() {{ mpReachStub("{m}", "{d}"); }}\n')
    else:
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
        if REACHABLE:
            body.append(f'// {d}\n'
                        f'extern "C" char reachstub_data_{j}[64] asm("{m}") = {{}};\n')
        else:
            body.append(f'// {d}\n'
                        f'extern "C" char stub_data_{j}[64] asm("{m}") = {{}};\n')

# ---------------------------------------------------------------------------------------
# Refuse to write an empty or shrunken file. This is not defensive programming, it is a
# repair: regenerating `PortLinkStubs.cpp` from a *stale* link log wipes the 181 committed
# stubs, because they are no longer undefined and so no longer appear in the log at all.
# That happened, once, in the minute before this guard existed.
#
# The safe set is not stable - it shrinks as the stubs land, by design - so the tsv is a
# snapshot, and the committed .cpp is the durable record of what was proven safe. Regenerate
# it only immediately after a fresh `tools/link_check.sh`, and never from a log that predates
# the last collection.
# ---------------------------------------------------------------------------------------
existing = OUT.read_text() if OUT.exists() else ''
had = existing.count(' asm("')
now = len(uniq_f) + len(uniq_d)
NL = chr(10)
if now == 0:
    sys.exit('refusing to write ' + OUT.name + ': the input ' + TSV.name + ' yielded no symbols.' + NL
             + '  That file is derived from build-port-link/build.log. If the log predates the' + NL
             + '  last collection, the symbols it lists are already defined and the safe set' + NL
             + '  looks empty. Run tools/link_check.sh first.')
if had and now < had * 0.9:
    sys.exit('refusing to shrink ' + OUT.name + ' from ' + str(had) + ' to ' + str(now)
             + ' definitions.' + NL
             + '  A shrunken stub file silently un-stubs the port. If this shrink is real,' + NL
             + '  delete ' + OUT.name + ' deliberately and re-run, so it is a decision and not' + NL
             + '  a side effect of a stale log.')

OUT.write_text(head + '\n'.join(body))
print(f'wrote {OUT.relative_to(ROOT)}: {len(uniq_f)} functions, {len(uniq_d)} data objects'
      f'{"  (DIAGNOSTIC - reachable)" if REACHABLE else ""}')
for k, v in by_class.most_common():
    print(f'  {v:4d}  {k}')
