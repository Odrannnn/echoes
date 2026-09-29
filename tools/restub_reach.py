#!/usr/bin/env python3
"""Bring src/MetroidPrime/PortReachStubs.cpp in line with one failed boot-probe link.

`tools/boot_probe.sh` calls this when its link fails, then relinks once. The link must have been
run with `-Wl,--no-demangle`: GNU ld otherwise prints C++ names demangled
(`CFoo::Bar(int)`), which cannot be written back as a symbol, and the old shell self-heal
skipped every one of them. That, plus never retiring duplicates, is how the file went stale
across the 2026-09-28 upstream merge (109 duplicates, 140 unstubbed; `docs/research/boot_probe.md`).

Two edits, both by symbol name, never by line number:

  - **retire** every stub whose symbol the link reports as `multiple definition` - the tree now
    defines it for real. Both stub shapes are recognised: the `reachstub_N() asm("sym")` pair
    with its `// demangled` comment line, and the one-line `extern "C" void sym(void) {...}`
    auto-stub. A duplicate that is not a stub is reported and left alone.
  - **append** a stub for every `undefined reference` not already stubbed. Functions call
    `mpReachStub`, so each logs its own name on entry. Data symbols get zeroed storage (0x400
    bytes, or retail's size if larger), so a read sees 0 instead of code bytes and a write does
    not hit a read-only page. **Data or code is asked, not guessed from the spelling** - the old
    self-heal stubbed `lbl_80418BA8` and `lbl_803A56C0` as functions (docs/HANDOFF.md, "The
    stub-a-data-symbol-as-a-function bug"): an unmangled name takes its `type:` from
    `config/G2ME01/symbols.txt`; a mangled one is data when it demangles without a parameter list
    (vtables, static members); an unmangled name retail does not list is data only if `lbl_`.

Run: `python3 tools/restub_reach.py <link-log> [stub-file]`. Prints `retired N, added M`; exits 0
whether or not it changed anything (the caller decides from the relink).
"""
import datetime
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def demangle(syms):
    if not syms:
        return []
    out = subprocess.run(['c++filt'], input='\n'.join(syms), capture_output=True, text=True).stdout
    return out.split('\n')[:len(syms)]


def retail_types():
    """name -> (type, size) from config/G2ME01/symbols.txt; only unmangled names can match."""
    out = {}
    for line in (ROOT / 'config/G2ME01/symbols.txt').read_text(errors='replace').splitlines():
        m = re.match(r'(\S+) = [^;]*; // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?', line)
        if m:
            out[m.group(1)] = (m.group(2), int(m.group(3), 16) if m.group(3) else 0)
    return out


def main() -> int:
    log = pathlib.Path(sys.argv[1]).read_text(errors='replace')
    path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'src/MetroidPrime/PortReachStubs.cpp'
    lines = path.read_text().split('\n')

    dups = set(re.findall(r"multiple definition of `([^']+)'", log))
    kill = set()
    retired = []
    for i, line in enumerate(lines):
        m = re.match(r'extern "C" void (reachstub_\d+)\(\) asm\("([^"]+)"\);$', line)
        if m and m.group(2) in dups:
            kill.add(i)
            if i > 0 and lines[i - 1].startswith('// '):
                kill.add(i - 1)
            if i + 1 < len(lines) and lines[i + 1].startswith(f'extern "C" void {m.group(1)}() {{'):
                kill.add(i + 1)
                if i + 2 < len(lines) and lines[i + 2] == '':
                    kill.add(i + 2)
            retired.append(m.group(2))
            continue
        m = re.match(r'extern "C" void ([A-Za-z_$][A-Za-z0-9_$]*)\(void\) \{', line)
        if m and m.group(1) in dups:
            kill.add(i)
            retired.append(m.group(1))
    lines = [l for i, l in enumerate(lines) if i not in kill]
    for sym in sorted(dups - set(retired)):
        print(f'restub_reach: duplicate `{sym}` is not a stub - left for a human', file=sys.stderr)

    text = '\n'.join(lines)
    have = set(re.findall(r'asm\("([^"]+)"\)', text))
    have |= set(re.findall(r'^extern "C" void ([A-Za-z_$][A-Za-z0-9_$]*)\(void\) \{', text, re.M))
    undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", log)) - have)
    bad = [s for s in undef if not re.fullmatch(r'[A-Za-z_$.][A-Za-z0-9_$.]*', s)]
    for s in bad:
        print(f'restub_reach: `{s}` is not a symbol name (was the link run with '
              f'-Wl,--no-demangle?) - skipped', file=sys.stderr)
    undef = [s for s in undef if s not in bad]

    n = max([int(x) for x in re.findall(r'reach(?:stub|data)_(\d+)', text)] or [0]) + 1
    types = retail_types()
    out = []
    for sym, dem in zip(undef, demangle(undef)):
        kind, size = types.get(sym, (None, 0))
        if kind is not None:
            data = kind == 'object'
        else:
            data = sym.startswith('lbl_') or (sym.startswith('_Z') and '(' not in dem)
        out.append(f'// {dem}')
        if data:
            size = f'{max(0x400, (size + 31) & ~31):#x}'
            out.append(f'extern "C" __attribute__((aligned(32))) char reachdata_{n}[{size}] asm("{sym}");')
            out.append(f'__attribute__((aligned(32))) char reachdata_{n}[{size}] = {{}};')
        else:
            out.append(f'extern "C" void reachstub_{n}() asm("{sym}");')
            out.append(f'extern "C" void reachstub_{n}() {{ mpReachStub("{sym}", "{dem}"); }}')
        out.append('')
        n += 1

    body = text.rstrip('\n') + '\n'
    if retired:
        print(f'restub_reach: retired {len(retired)} stub(s) the tree now defines: '
              + ', '.join(retired[:8]) + (' ...' if len(retired) > 8 else ''))
    if out:
        stamp = datetime.datetime.now().isoformat(timespec='seconds')
        body += '\n'.join(['', f'// --- appended by tools/restub_reach.py on {stamp} ---',
                           '// Unresolved symbols one boot-probe link asked for. Diagnostic only;'
                           ' see the file header.'] + out)
    if retired or out:
        path.write_text(body)
    print(f'retired {len(retired)}, added {len(undef)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
