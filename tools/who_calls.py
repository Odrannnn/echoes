#!/usr/bin/env python3
"""Who calls this function, and what class is the argument?

    python3 tools/who_calls.py 0x80212A94 [0x80212A9C ...]
    python3 tools/who_calls.py --run 0x80212A94..0x80212ADC

The point of this tool is the *caller's parameter type*, not the accessor's own name. Retail keeps
these accessors out of line, so the call site is where the owning class is visible: a `bl` whose
preceding instructions load a value out of a `CPlayerState*` (or any known `this`) tells us the
receiver's class, and a `bl` whose result is compared against a constant fixes the member's width.

Output per call site: the caller's own retail name, its demangled class, the last few instructions
before the `bl` (the argument setup), and the first few after (how the result is used).
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
LABEL = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")


def load(path):
    """addr -> (mnemonic, operands) and addr -> label."""
    insn, label = {}, {}
    for line in pathlib.Path(path).read_text(errors="replace").splitlines():
        m = LABEL.match(line)
        if m:
            label[int(m.group(1), 16)] = m.group(2)
            continue
        m = INSN.match(line)
        if m:
            insn[int(m.group(1), 16)] = (m.group(3), m.group(4).strip())
    return insn, label


def demangle(names):
    if not names:
        return {}
    p = subprocess.run(["c++filt"], input="\n".join(names) + "\n", text=True,
                       capture_output=True)
    return dict(zip(names, p.stdout.splitlines()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("targets", nargs="*", help="retail addresses, or --run A..B")
    ap.add_argument("--run", default="", help="lo..hi inclusive-exclusive range")
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--before", type=int, default=10)
    ap.add_argument("--after", type=int, default=6)
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    a = ap.parse_args()

    insn, label = load(a.asm)

    targets = set()
    for t in a.targets:
        targets.add(int(t, 16))
    if a.run:
        lo, hi = (int(x, 16) for x in a.run.split(".."))
        targets |= {x for x in insn if lo <= x < hi}

    # symbol table: address -> retail name, for naming the callers ourselves
    sym = {}
    pat = re.compile(r"^(\S+) = (\w+):(0x[0-9A-Fa-f]+); // type:(\w+)")
    for line in pathlib.Path(a.symbols).read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m and m.group(4) == "function":
            sym[int(m.group(3), 16)] = m.group(1)

    # the index: every bl target -> call sites
    callers = collections.defaultdict(list)
    for addr, (mn, ops) in insn.items():
        if mn not in ("bl", "b", "bctrl", "blrl"):
            continue
        m = re.match(r"^([0-9a-f]+)\b", ops)
        if m:
            callers[int(m.group(1), 16)].append(addr)

    # the enclosing function of an address: the greatest function start <= it
    starts = sorted(x for x, n in label.items() if not n.startswith("lbl"))
    starts += sorted(x for x in sym if x not in label)
    starts = sorted(set(starts))
    import bisect

    def owner(addr):
        """The function containing `addr` (the greatest function start <= addr)."""
        i = bisect.bisect_right(starts, addr) - 1
        if i < 0:
            return f"fn_{addr:08X}"
        s = starts[i]
        return label.get(s) or sym.get(s) or f"fn_{s:08X}"

    rows = []
    for t in sorted(targets):
        for ca in sorted(callers.get(t, [])):
            rows.append((t, ca))

    if not rows:
        print("no `bl` found to any target")
        return 0

    caller_names = [owner(ca) for _t, ca in rows]
    dem = demangle(caller_names)
    tgt_names = [owner(t) for t in targets]

    for t, name in zip(sorted(targets), sorted([owner(t) for t in targets])):
        pass
    for (t, ca), cname in zip(rows, caller_names):
        print(f"\n=== 0x{t:08X} called from 0x{ca:08X} in {cname}  [{dem.get(cname, '?')}]")
        for k in range(1, a.before + 1):
            p = ca - 4 * k
            if p in insn:
                mn, ops = insn[p]
                print(f"    {p:08X}: {mn:8s} {ops}")
        print(f"  > {ca:08X}: bl {t:08X}")
        for k in range(1, a.after + 1):
            p = ca + 4 * k
            if p in insn:
                mn, ops = insn[p]
                print(f"    {p:08X}: {mn:8s} {ops}")
    print(f"\n{len(rows)} call sites to {len(targets)} targets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
