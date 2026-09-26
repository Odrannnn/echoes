#!/usr/bin/env python3
"""Class attribution for every accessor run, in one pass over the DOL.

    python3 tools/class_sweep.py [--depth 3] [--min-run 3] [--out docs/research/accessor_classes.md]

One process, one parse. The reverse-call-graph BFS is done once from *all* accessor addresses
together, so a retail-named function that calls any accessor in the DOL is found once, and then
each run is scored by the named functions that reach it. That is the difference between this and
`tools/class_of.py` called per run: 186 runs x 1s is three minutes, this is one.

The scoring is deliberately conservative and the verdict labels are the point:

  * `CONFIRMED` - a retail-named method calls the accessor within `--depth` hops. Depth 1 is a
    direct `bl`; the named function's own `this` is then the receiver.
  * `probable`  - reached at depth 2 (one unnamed hop).
  * `guess`     - depth >= 3, or reached only through a function that is itself only *called* by
                  the named one.
  * `none`      - nothing named within `--depth`. These are the ones that need the offset
                  fingerprint or a hand read; the table lists them with their offsets so they can
                  be matched against known layouts.
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



def this_regs_of(start, insn):
    """The registers a function's incoming `this` (r3) has been copied into, in its prologue.

    mwcceppc spills `this` to a callee-saved register (`mr r30,r3`) or to the stack
    (`stw r3,N(r1)`) before the first call. Both are evidence that the register still holds `this`.
    A register that is *written* before the copy is not one; the prologue is what counts, so this
    scans only the first 24 instructions, which is long enough for every prologue mwcc emits here.
    """
    out = set()
    for k in range(24):
        a = start + 4 * k
        mn_ops = insn.get(a)
        if mn_ops is None:
            break
        mn, ops = mn_ops
        if mn == "blr":
            break
        m = re.match(r"^mr\s+(r\d+),r3$", ops)
        if m:
            out.add(m.group(1))
            continue
        m = re.match(r"^stw\s+r3,(-?\d+)\(r1\)$", ops)
        if m:
            out.add("stack:" + m.group(1))
    return out


def receiver(insn, site, this_regs):
    """Is r3 at `site` still this function's own `this`? Returns 'this' / 'other' / 'unknown'."""
    if site is None:
        return "unknown"
    for k in range(1, 7):
        a = site - 4 * k
        mn_ops = insn.get(a)
        if mn_ops is None:
            return "unknown"
        mn, ops = mn_ops
        if mn == "mr":
            m = re.match(r"^r3,(r\d+)$", ops)
            if m:
                return "this" if m.group(1) in this_regs else "other"
        if mn in ("lwz", "lbz", "lfs", "lha", "lhz", "addi", "lis", "li", "lfd", "mr", "or",
                  "load_short", "load_byte", "la"):
            # r3 was recomputed: not this function's own incoming this
            if re.search(r"\br3,", ops) or ops.startswith("r3,"):
                return "other"
        if mn in ("stw", "stb", "stfs", "addi", "blr", "b"):
            pass
    return "this"   # nothing touched r3 in the last 6 instructions


OPS = ("ct", "dt", "as", "dv", "pl", "mi", "ml", "eq", "ne", "lt", "le", "gt", "ge", "nt", "ad",
       "sr", "sl", "cm", "ls", "rs", "lr", "pp", "mm", "aa", "oo", "co")


def mwcc_class(name):
    """MWCC's mangling -> the class a member belongs to, or None for a free function.

    Three shapes, and the third silently produces garbage if you skip it:
      * `ComboActive__10CPlayerGunFR13CStateManager` - length-prefixed member, class at the head;
      * `__ct__18CDolphinControllerFv` - a constructor, where the length prefix names the class
        (`CDolphinController`), not the method (`ct`). Reading the method as the class gives a row
        called `__ct__18CDolphinControllerFv`, which is not a class;
      * `__dv__FRC9CVector2fRCf` - a **free** operator, which has no `this` at all, and the class
        is the first *argument* type. `__as__Q24rstl55vector<...>` is the same shape. Treating a
        free function's method name as a class yields rows called `__dv`, which is exactly what
        this returned before it was fixed.
    """
    m = re.match(r"^__([a-z]{2})__(\d+)([A-Za-z_][A-Za-z0-9_]*)", name)
    if m:
        n = int(m.group(2))
        c = m.group(3)
        if len(c) >= n:
            return c[:n]
    m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)__(\d+)([A-Za-z_][A-Za-z0-9_]*)", name)
    if m:
        n = int(m.group(2))
        c = m.group(3)
        if len(c) >= n:
            return c[:n]
    m = re.match(r"^__([a-z]{2})__((?:FR|F)?.*)$", name)     # free operator: class is arg 1
    if m and m.group(1) in OPS:
        for _pfx, c in re.findall(r"[RCKPF](\d+)([A-Za-z_][A-Za-z0-9_]*)", m.group(2)):
            if c[0].isupper():
                return c
        return None
    m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)__[A-Za-z]", name)
    if m:
        return m.group(1)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--min-run", type=int, default=3)
    ap.add_argument("--top", type=int, default=3)
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    ap.add_argument("--out", default="")
    a = ap.parse_args()

    sys.path.insert(0, str(ROOT / "tools"))
    import mine_accessors as MA
    import recv as RECV

    insn, label = load(a.asm)
    sym = {}
    pat = re.compile(r"^(\S+) = (\S+):(0x[0-9A-Fa-f]+); // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?")
    for line in pathlib.Path(a.symbols).read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m and m.group(4) == "function":
            sym[int(m.group(3), 16)] = (m.group(1), int(m.group(5) or 0, 16))

    # ---- the runs
    cands = []
    for f in MA.parse(a.asm):
        c = MA.classify(f)
        if c is None:
            continue
        kind, offs, touches, regs = c
        if kind == "mixed" or len(touches) > 2:
            continue
        cands.append({"addr": f["start"], "size": f["end"] - f["start"], "kind": kind,
                      "offs": sorted(offs)})
    cands.sort(key=lambda c: c["addr"])
    runs = []
    for c in cands:
        if runs and runs[-1][-1]["addr"] + runs[-1][-1]["size"] == c["addr"]:
            runs[-1].append(c)
        else:
            runs.append([c])
    runs = [r for r in runs if len(r) >= a.min_run]
    run_of = {}
    for i, r in enumerate(runs):
        for c in r:
            run_of[c["addr"]] = i
    acc_set = set(run_of)

    # ---- reverse call graph
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

    enc_cache = {}

    def enc(addr):
        v = enc_cache.get(addr)
        if v is None:
            v = enc_cache[addr] = enclosing(addr)
        return v

    # ---- one BFS from every accessor address
    seed = [c["addr"] for r in runs for c in r]
    depth_of = {t: 0 for t in seed}
    npaths = collections.Counter(seed)
    q = collections.deque(seed)
    while q:
        node = q.popleft()
        d = depth_of[node]
        if d >= a.depth:
            continue
        for ca in callers.get(node, []):
            e = enc(ca)
            npaths[e] += 1
            nd = d + 1
            if e not in depth_of or nd < depth_of[e]:
                depth_of[e] = nd
                if nd < a.depth:
                    q.append(e)

    def fname(s):
        return sym.get(s, (label.get(s, f"fn_{s:08X}"), 0))[0]

    # every retail-named function anywhere in the reverse cone of any accessor
    allhits = []
    for s, d in depth_of.items():
        n = fname(s)
        if n in ("?", "") or n.startswith(("fn_", "lbl_")):
            continue
        allhits.append((d, s, n))
    named_of = {s: n for _d, s, n in allhits}

    # The forward direction, so a named function is credited only to the runs it actually
    # reaches. A reverse cone is ambiguous - it can be entered from either end - and only the
    # forward walk knows which accessor is downstream of the named function.
    # callees[fn] = [(callee address, the `bl` site), ...] - the site is needed for the receiver
    # test, which is what separates a real class attribution from a convincing-looking one.
    callees = collections.defaultdict(list)
    for tgt, sites in callers.items():
        for ca in sites:
            callees[enc(ca)].append((tgt, ca))

    reach = collections.defaultdict(set)
    fwd_depth = {}
    d_of = {s: 0 for s in named_of}
    q = collections.deque(d_of)
    while q:
        node = q.popleft()
        d = d_of[node]
        if node in named_of:
            fwd_depth.setdefault(node, d)
        if d >= a.depth:
            continue
        # r3's abstract value at every call site in this function, from the forward walk in
        # tools/recv.py. This is the attribution gate, not a heuristic.
        r3at = {ca: st.get("r3", RECV.U) for ca, st in RECV.analyse(node, insn)}
        for tgt, site in callees.get(node, ()):
            # only a call *of an accessor* is evidence; anything else is just a hop, and the
            # walk must continue through it even when it is unnamed - that is the whole point
            # of a multi-hop search.
            if tgt not in acc_set:
                e0 = enc(tgt)
                if e0 != node and (e0 not in d_of or d + 1 < d_of[e0]):
                    d_of[e0] = d + 1
                    if d + 1 < a.depth:
                        q.append(e0)
                continue
            rid = run_of.get(tgt)
            if rid is not None:
                # The receiver test. A named function calling an accessor proves the accessor's
                # class only if the call site's r3 is *this* function's own `this`. If r3 was
                # reloaded from memory the accessor belongs to a member object, and the named
                # function is evidence about the *containing* class, not the accessor's. That
                # distinction is the whole point: the repo's `CDamageVulnerability` "overlap" was
                # a digit transposition in .bss, and a depth-1 call with a reloaded r3 looks just
                # as convincing as a real one.
                recv = r3at.get(site, RECV.U)
                reach[rid].add((node, recv))
                continue
            e = enc(tgt)
            if e == node:
                continue
            nd = d + 1
            if e not in d_of or nd < d_of[e]:
                d_of[e] = nd
                if nd < a.depth:
                    q.append(e)

    todo = set()
    for _rid, ss in reach.items():
        todo |= {s for s, _r in ss}
    dem = demangle([named_of.get(s, "?") for s in sorted(todo)])

    rows = []
    for i, r in enumerate(runs):
        lo, hi = r[0]["addr"], r[-1]["addr"] + r[-1]["size"]
        offs = sorted({o for c in r for o in c["offs"]})
        hits = []
        for s, recv in reach.get(i, ()):
            n = named_of.get(s, "?")
            # hops from the named function down to the accessor: 1 == a direct `bl`
            hops = fwd_depth.get(s, 99) + 1
            # a `this`-receiver is required for CONFIRMED; a reloaded r3 downgrades to `other`
            w = 1.0 / (hops ** 2) * (1.0 if recv == "this" else (0.4 if recv == "unknown" else 0.05))
            hits.append((w, hops, s, n, recv))
        hits.sort(reverse=True)
        rows.append((i, lo, hi, len(r), offs, hits[:a.top]))

    lines = ["# Accessor-run class attribution", "",
             f"`python3 tools/class_sweep.py --depth {a.depth} --min-run {a.min_run}`, one parse of",
             "`objdump -d build/G2ME01/main.elf`. `CONFIRMED` = a retail-**named** method reaches the",
             "run within the depth; `probable` = depth 2; `guess` = depth >= 3.", ""]
    n_conf = sum(1 for _i, _lo, _hi, _n, _o, h in rows if h and h[0][4] == "this" and h[0][1] == 1)
    n_prob = sum(1 for _i, _lo, _hi, _n, _o, h in rows if h and h[0][4] == "this" and h[0][1] == 2)
    n_guess = sum(1 for _i, _lo, _hi, _n, _o, h in rows if h and h[0][4] == "this" and h[0][1] >= 3)
    n_contra = sum(1 for _i, _lo, _hi, _n, _o, h in rows if h and h[0][4] != "this"
                   and h[0][1] < 99)
    n_none = sum(1 for _i, _lo, _hi, _n, _o, h in rows
                 if (not h) or h[0][1] >= 99)
    lines += [f"**{len(runs)} runs of >= {a.min_run}: {n_conf} CONFIRMED, {n_prob} probable, "
              f"{n_guess} guess, {n_contra} contradicted (r3 reloaded, so the accessor belongs to a "
              f"member), {n_none} with no named caller.**", "",
              "| run | retail range | B | n | offsets | verdict | class | evidence |",
              "|---|---|---|---|---|---|---|---|"]
    for i, lo, hi, n, offs, hits in rows:
        if not hits:
            verdict, cls, ev = "none", "-", "no retail-named caller within depth"
        else:
            w, d, s, nm, recv = hits[0]
            if d >= 99:
                verdict, cls, ev = "none", "-", "not reached forward from any named function"
            else:
                if recv != "this":
                    verdict = "CONTRADICTED"
                elif d == 1:
                    verdict = "CONFIRMED"
                elif d == 2:
                    verdict = "probable"
                else:
                    verdict = "guess"
                cls = mwcc_class(nm) or "-"
                ev = (f"`{nm}` ({dem.get(nm, '')}) at 0x{s:08X}, {d} hop(s) forward, "
                      f"r3 receiver = {recv}")
            cls = mwcc_class(nm) or "-"
            ev = f"`{nm}` ({dem.get(nm, '')}) at 0x{s:08X}, depth {d}"
        lines.append(f"| {i} | `0x{lo:08X}..0x{hi:08X}` | {hi - lo} | {n} | "
                     + " ".join(f"+{o}" for o in offs) + f" | {verdict} | {cls} | {ev} |")
    out = "\n".join(lines) + "\n"
    if a.out:
        pathlib.Path(a.out).write_text(out)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
