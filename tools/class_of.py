#!/usr/bin/env python3
"""Class attribution for an accessor run: walk *up* the call graph to retail-named methods.

    python3 tools/class_of.py --run 0x80212A94..0x80212ADC [--depth 3] [--top 12]

Method. A `single-load` accessor is called out of line, so the accessor itself says nothing. But
the *caller* passes a `this` it got from somewhere, and a few hops up the chain that `this` came
out of a function whose own mangled name names its class. So: reverse-BFS from the accessor over
`bl` call sites, and score the *named* functions found within `--depth` hops, weighting a named
hit by proximity and how many distinct paths reach it.

What counts as evidence, in the order this tool reports it:

  * `depth 1, named`   - a retail-named method calls the accessor directly and passes its own
                          `this` (`mr r3,r3` / no re-load before the `bl`). **Strong**: the
                          accessor is that method's class's.
  * `depth 2, named`   - a named method calls an unnamed function that calls the accessor. Strong
                          if the unnamed function's own `this` chains to the named caller's
                          `this`; otherwise only probable.
  * `depth 3+, named`  - a class reaches the accessor through other classes (a manager calling
                          into a member). **Probable at best**, and the report says so.
  * `no named hit`     - nothing within `--depth`. The class is not identifiable this way; the
                          next instrument is the *offset fingerprint* against classes whose layout
                          is already known (`tools/offset_fingerprint.py`).

A `depth 1` hit is the only one this tool calls *confirmed* on its own. Everything else it labels
`probable` or `guess`, because the repo has already been bitten by a class attribution with one
`bl` behind it.
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
    got = [l for l in p.stdout.splitlines()]
    return dict(zip(names, got))


def mwcc_class(name):
    """MWCC: `method__<len><Class><args>` -> the class; also `Class__<method>` for statics."""
    m = re.match(r"^[^_~][A-Za-z0-9_]*__(\d+)([A-Za-z_][A-Za-z0-9_]*)", name)
    if m:
        n = int(m.group(1))
        c = m.group(2)
        if len(c) >= n:
            return c[:n]
    m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)__[A-Za-z]", name)
    if m:
        return m.group(1)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", default="", help="lo..hi")
    ap.add_argument("--addr", default="", help="a single retail address")
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--top", type=int, default=12)
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

    targets = set()
    if a.addr:
        targets.add(int(a.addr, 16))
    if a.run:
        lo, hi = (int(x, 16) for x in a.run.split(".."))
        targets |= {x for x in insn if lo <= x < hi}
    if not targets:
        print("no target")
        return 2

    # reverse call graph over `bl`/`b` inside functions only
    callers = collections.defaultdict(list)
    for addr, (mn, ops) in insn.items():
        if mn in ("bl", "b", "bctrl", "blrl"):
            m = re.match(r"^([0-9a-f]+)\b", ops)
            if m:
                callers[int(m.group(1), 16)].append(addr)

    starts = sorted(set(list(sym) + [x for x, n in label.items() if not n.startswith("lbl")]))

    def enclosing(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return starts[i] if i >= 0 else addr

    def fname(s):
        return sym.get(s, (label.get(s, f"fn_{s:08X}"), 0))[0]

    def is_named(s):
        n = fname(s)
        return not (n.startswith("fn_") or n.startswith("lbl_"))

    # BFS up, tracking the best (smallest) depth a node was reached at and the number of paths
    best = {}
    paths = collections.Counter()
    frontier = [(t, 0) for t in sorted(targets)]
    for t in frontier:
        best[t] = 0
    depth_of = {t: 0 for t in targets}
    q = collections.deque(sorted(targets))
    while q:
        node = q.popleft()
        d = depth_of[node]
        if d >= a.depth:
            continue
        for ca in callers.get(node, []):
            enc = enclosing(ca)
            npaths = paths[ca] + 1
            paths[ca] = npaths
            if enc not in best or d + 1 < depth_of[enc]:
                depth_of[enc] = d + 1
                best[enc] = d + 1
                if d + 1 < a.depth:
                    q.append(enc)

    # score named hits: nearer is much stronger
    hits = []
    for s, d in depth_of.items():
        if not is_named(s):
            continue
        n = fname(s)
        cls = mwcc_class(n)
        weight = 1.0 / (d ** 2) * (1.0 + 0.25 * min(paths.get(s, 1), 8))
        hits.append((weight, d, s, n, cls))
    hits.sort(reverse=True)

    seen = set()
    out = []
    for weight, d, s, n, cls in hits:
        if n in seen:
            continue
        seen.add(n)
        out.append((weight, d, s, n, cls))

    if not out:
        print(f"no retail-named function calls 0x{min(targets):08X}.. within {a.depth} hops")
        return 1

    dem = demangle([o[3] for o in out[:a.top]])
    print(f"targets 0x{min(targets):08X}..0x{max(targets):08X} ({len(targets)} functions); "
          f"reverse BFS to depth {a.depth}\n")
    for weight, d, s, n, cls in out[:a.top]:
        verdict = "CONFIRMED" if d == 1 else ("probable" if d == 2 else "guess")
        print(f"  {verdict:9s} depth {d}  score {weight:5.2f}  0x{s:08X}  "
              f"{cls or '-':20s} {n}")
        if n in dem:
            print(f"            {dem[n]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
