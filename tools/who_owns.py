#!/usr/bin/env python3
"""Bracket an accessor run by the nearest retail-*named* symbols, and by its callers' classes.

    python3 tools/who_owns.py --run 0x80212A94..0x80212ADC
    python3 tools/who_owns.py --all --max-run 3

Why bracketing is the cheapest strong evidence. Retail emits one translation unit per class
grouping and mwldeppc keeps each object's `.text` contiguous, so a class's methods sit together.
An accessor run with a *named* retail method immediately below it and another immediately above it
is in the same object as both, and those two names usually carry the class in their signature
(`CPlayerGun::ComboActive` right below a run means the run is `CPlayerGun`'s).

A run with no named symbol within +-N bytes is in an unnamed region and must be identified from
its callers instead -- which is what the caller-class column is for.
"""
import argparse
import bisect
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
LABEL = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")


def load(path):
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


def mwcc_class(name):
    """`ComboActive__10CPlayerGunFR13CStateManagerRC12CTriggerData` -> `CPlayerGun`."""
    m = re.match(r"^[^_~][A-Za-z0-9_]*__(\d+)([A-Za-z_][A-Za-z0-9_]*)", name)
    if m:
        n = int(m.group(1))
        c = m.group(2)
        if len(c) >= n:
            return c[:n]
    m = re.match(r"^(\w+?)__F", name)
    if m:
        return m.group(1)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", default="", help="lo..hi")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--max-run", type=int, default=3)
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    a = ap.parse_args()

    insn, label = load(a.asm)
    sym = {}
    pat = re.compile(r"^(\S+) = (\S+):(0x[0-9A-Fa-f]+); // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?")
    for line in pathlib.Path(a.symbols).read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m and m.group(4) == "function":
            sym[int(m.group(3), 16)] = (m.group(1), int(m.group(5) or 0, 16))

    # function starts = anything symbols.txt calls a function, plus every objdump label
    starts = sorted(set(list(sym) + [x for x, n in label.items() if not n.startswith("lbl")]))
    named = sorted(x for x in starts if x in sym and not sym[x][0].startswith("fn_")
                   and not sym[x][0].startswith("lbl_"))

    def named_name(a):
        return sym[a][0] if a in sym else label.get(a, "?")

    # the accessor runs, from mine_accessors
    sys.path.insert(0, str(ROOT / "tools"))
    import mine_accessors as MA
    funcs = MA.parse(a.asm)
    cands = []
    for f in funcs:
        c = MA.classify(f)
        if c is None:
            continue
        kind, offs, touches, regs = c
        if kind == "mixed" or len(touches) > 2:
            continue
        cands.append({"addr": f["start"], "size": f["end"] - f["start"], "kind": kind,
                      "offs": sorted(offs), "insns": f["insns"]})
    cands.sort(key=lambda c: c["addr"])
    runs = []
    for c in cands:
        if runs and runs[-1][-1]["addr"] + runs[-1][-1]["size"] == c["addr"]:
            runs[-1].append(c)
        else:
            runs.append([c])
    runs = [r for r in runs if len(r) >= a.max_run]

    # caller index
    callers = collections.defaultdict(list)
    for addr, (mn, ops) in insn.items():
        if mn in ("bl", "b", "bctrl", "blrl"):
            m = re.match(r"^([0-9a-f]+)\b", ops)
            if m:
                callers[int(m.group(1), 16)].append(addr)

    # callers of callers: an unnamed caller inside a *named* function is still class evidence
    def enclosing(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return starts[i] if i >= 0 else addr

    if a.run:
        lo, hi = (int(x, 16) for x in a.run.split(".."))
        runs = [[c for c in r if lo <= c["addr"] < hi] for r in runs]
        runs = [r for r in runs if r]

    # demangle once for the whole table
    todo = set()
    for r in runs:
        for c in r:
            for ca in callers.get(c["addr"], []):
                todo.add(enclosing(ca))
    dem = demangle([named_name(x) for x in sorted(todo)])

    for i, r in enumerate(runs):
        lo, hi = r[0]["addr"], r[-1]["addr"] + r[-1]["size"]
        j = bisect.bisect_left(named, lo) - 1
        below = [(named[k], named_name(named[k])) for k in range(max(0, j - 1), j + 1)]
        k2 = bisect.bisect_left(named, hi)
        above = [(named[k], named_name(named[k])) for k in range(k2, min(len(named), k2 + 2))]
        offs = sorted({o for c in r for o in c["offs"]})
        print(f"\n### run {i}: `0x{lo:08X}..0x{hi:08X}` {hi - lo} B, {len(r)} accessors, "
              f"offsets " + " ".join(f"+{o}" for o in offs))
        for ad, nm in below:
            d = dem.get(nm, "")
            print(f"    below  0x{ad:08X} {nm}   {d}")
        for ad, nm in above:
            d = dem.get(nm, "")
            print(f"    above  0x{ad:08X} {nm}   {d}")
        # callers
        seen = collections.Counter()
        for c in r:
            for ca in callers.get(c["addr"], []):
                enc = enclosing(ca)
                nm = named_name(enc)
                cls = mwcc_class(nm)
                d = dem.get(nm, "")
                cls2 = mwcc_class(d.split("(")[0].replace(" ", "")) or None
                tag = cls2 or cls or ""
                seen[(tag or "(unnamed caller)", f"0x{enc:08X}", nm, d)] += 1
        for (tag, ad, nm, d), n in seen.most_common(12):
            print(f"    caller x{n} {ad} {tag or '(unnamed)':24s} {nm}  {d}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
