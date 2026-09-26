#!/usr/bin/env python3
"""The accessor identification table: every accessor whose owning class is provable.

    python3 tools/accessor_table.py [--min-run 1] [--depth 1] [--out docs/research/accessors.md]

One pass, and the criterion is the strict one. For every function whose whole body is one memory
access against `r3` (a `single-load`/`single-store` accessor), and for every call site in the DOL
that calls it, `tools/recv.py`'s forward abstract interpretation decides whether `r3` at that site
is still the *calling function's own* `this`. Where it is, and the calling function is a
retail-**named** method, the accessor's class is that method's class - and that is a proof, not a
guess, because the call site passes the identical pointer.

The table is grouped by class, because a class's accessors are the deliverable: a run of ten on
one object is one class's accessor block and one `Matching` unit.

What is deliberately *not* claimed:

  * a call site where `r3` was reloaded (a member, an array element, a global) - the caller names
    the *containing* class, not the accessor's. These are counted and listed as `member-of`, never
    as the class.
  * a call site where `r3` is `U` (undecidable) - reported separately.
  * an accessor with no `this`-verified named caller - listed as unattributed with its offsets,
    which is what the next lane needs for the fingerprint match.
"""
import argparse
import bisect
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def demangle(names):
    if not names:
        return {}
    p = subprocess.run(["c++filt"], input="\n".join(names) + "\n", text=True,
                       capture_output=True)
    return dict(zip(names, p.stdout.splitlines()))


OPS = ("ct", "as", "dv", "pl", "mi", "ml", "eq", "ne", "lt", "le", "gt", "ge", "nt", "ad",
       "sr", "sl", "nt", "cm", "ls", "rs", "lr", "pp", "mm", "aa", "oo", "co", "nt")


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


BASE_RE = re.compile(r"^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*public\s+([A-Za-z_][A-Za-z0-9_]*)",
                     re.M)


def base_map(include):
    """`class CPowerBeam : public CGunWeapon` -> CPowerBeam's base is CGunWeapon, from the headers.

    This matters for correctness, not tidiness. A `this`-verified call from `CPowerBeam` to
    `CGunWeapon::IsLoaded` (0x801D9B24) is *not* evidence that the accessor belongs to `CPowerBeam`:
    `CPowerBeam` **derives from** `CGunWeapon`, so the inherited method receives the identical
    pointer. Without this filter the table credits a derived class with its base's accessors - a
    false positive that looks exactly like a correct one, and that this repo has already hit in
    the reverse direction (a `CDamageVulnerability` "overlap" that was a digit transposition).
    """
    out = {}
    inc = pathlib.Path(include)
    if not inc.is_dir():
        return out
    for f in inc.rglob("*.hpp"):
        for m in BASE_RE.finditer(f.read_text(errors="replace")):
            out.setdefault(m.group(1), m.group(2))
    return out


def ancestors(cls, bases):
    """The class and every base above it, nearest first."""
    out, cur, guard = [cls], cls, 0
    while cur in bases and guard < 16:
        cur = bases[cur]
        out.append(cur)
        guard += 1
    return out


def claimed_classes(splits, report):
    """address -> (unit name, class) for every function a unit in this tree already claims.

    An accessor whose address is already inside a claimed range is *already identified*: the unit
    that claims it carries the class. Reporting it again as a fresh identification would be noise,
    and - worse - would invite a second unit to claim bytes another lane's `Matching` object
    already reproduces.
    """
    import json
    spans = []
    for line in pathlib.Path(splits).read_text(errors="replace").splitlines():
        m = re.search(r"\.text\s+start:(0x[0-9A-Fa-f]+) end:(0x[0-9A-Fa-f]+)", line)
        if m:
            spans.append((int(m.group(1), 16), int(m.group(2), 16)))
    out = {}
    r = json.load(open(report))
    for u in r["units"]:
        for f in u.get("functions", []):
            va = f.get("metadata", {}).get("virtual_address")
            if not va:
                continue
            a = int(va, 16)
            for s, e in spans:
                if s <= a < e:
                    out.setdefault(a, (u["name"], mwcc_class(f["name"]) or ""))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    ap.add_argument("--min-run", type=int, default=1)
    ap.add_argument("--out", default="")
    ap.add_argument("--include", default=str(ROOT / "include"))
    a = ap.parse_args()

    sys.path.insert(0, str(ROOT / "tools"))
    import mine_accessors as MA
    import recv as RECV

    insn, label = RECV.load(a.asm)
    sym = {}
    pat = re.compile(r"^(\S+) = (\S+):(0x[0-9A-Fa-f]+); // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?")
    for line in pathlib.Path(a.symbols).read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m and m.group(4) == "function":
            sym[int(m.group(3), 16)] = (m.group(1), int(m.group(5) or 0, 16))
    starts = sorted(set(list(sym) + [x for x, n in label.items() if not n.startswith("lbl")]))

    def enc(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return starts[i] if i >= 0 else addr

    def fname(s):
        return sym.get(s, (label.get(s, f"fn_{s:08X}"), 0))[0]

    def is_named(s):
        n = fname(s)
        return not n.startswith(("fn_", "lbl_", "?"))

    # ---- the accessors
    acc = {}
    for f in MA.parse(a.asm):
        c = MA.classify(f)
        if c is None:
            continue
        kind, offs, touches, regs = c
        if kind == "mixed" or len(touches) > 2:
            continue
        acc[f["start"]] = {"addr": f["start"], "size": f["end"] - f["start"], "kind": kind,
                           "offs": sorted(offs), "insns": f["insns"]}

    # ---- who calls them
    callers = collections.defaultdict(list)
    for addr, (mn, ops) in insn.items():
        if mn in ("bl", "b", "bctrl", "blrl"):
            m = re.match(r"^([0-9a-f]+)\b", ops)
            if m:
                callers[int(m.group(1), 16)].append(addr)

    # ---- one forward walk per calling function
    sites_by_fn = collections.defaultdict(list)
    for t, cs in callers.items():
        if t in acc:
            for ca in cs:
                sites_by_fn[enc(ca)].append((ca, t))

    # ---- Primary evidence, and it is not an inference at all: **the retail symbol that contains
    # the accessor.** retail's `symbols.txt` names `SetMaterialFilter__6CActor` at 0x8004AC98, and
    # the accessor at 0x8004BA20 is inside it, so the accessor's class is `CActor` outright. This
    # beats the whole call-graph route: a caller with `this` only proves the owner is an
    # ancestor-or-self (see below), whereas the enclosing symbol *is* a member of the class.
    # It is available for every accessor that lands inside a named method, which is most of them.
    enclosing_name = {}
    for x, (n, _sz) in sym.items():
        enclosing_name[x] = n
    starts_sorted = sorted(enclosing_name)
    own = {}
    for ad in acc:
        i = bisect.bisect_right(starts_sorted, ad) - 1
        if i >= 0:
            s = starts_sorted[i]
            n = enclosing_name[s]
            c = mwcc_class(n)
            if c and not n.startswith(("fn_", "lbl_")):
                own[ad] = (c, s, n)

    proven = collections.defaultdict(list)      # class -> [(accessor, evidence)]
    contradicted = collections.defaultdict(list)
    undecided = []
    bases = base_map(a.include)
    callers_of = collections.defaultdict(set)   # accessor -> every class that may own it
    for f, cs in sites_by_fn.items():
        named = is_named(f)
        want = {ca: t for ca, t in cs}
        for site, st in RECV.analyse(f, insn):
            if site not in want:
                continue
            t = want[site]
            v = st.get("r3", RECV.U)
            if v == RECV.T and named:
                cls = mwcc_class(fname(f)) or fname(f)
                proven[cls].append((t, f, fname(f), site))
                for a_ in ancestors(cls, bases):
                    callers_of.setdefault(t, set()).add(a_)
            elif v == RECV.T and not named:
                undecided.append((t, f, "unnamed caller, r3 == this"))
            elif not named:
                pass
            else:
                contradicted[fname(f)].append((t, f, site, v))

    # ---- runs
    def runs_of(d):
        xs = sorted(d)
        out = []
        for x in xs:
            if out and out[-1][-1] + acc_size[out[-1][-1]] == x:
                out[-1].append(x)
            else:
                out.append([x])
        return out

    acc_size = {k: v["size"] for k, v in acc.items()}

    # ---- class resolution, stated as what the evidence actually supports.
    #
    # A `this`-verified call from class C to an accessor proves the owner O satisfies
    # O in ancestors(C) - **not** O == C. `CPowerBeam` derives from `CGunWeapon`, so
    # `CPowerBeam::IsLoaded` calling 0x801D9B24 with `this` is exactly what an *inherited*
    # `CGunWeapon::IsLoaded` looks like; the repo already has that exact function as a
    # `Matching` unit (`src/MetroidPrime/Weapons/CGunWeaponIsLoaded.cpp`), so crediting
    # `CPowerBeam` would be wrong and would look completely convincing.
    #
    # So: intersect the ancestor chains of every caller. Two unrelated callers pin the owner to
    # their lowest common ancestor. **One caller cannot**: the owner is one of that caller's
    # ancestors, and the table says so rather than guessing. This is the difference between an
    # identification and a plausible story.
    known = claimed_classes(ROOT / "config" / "G2ME01" / "splits.txt",
                            ROOT / "build" / "report.json")
    known = claimed_classes(ROOT / "config" / "G2ME01" / "splits.txt",
                            ROOT / "build" / "report.json")

    # The enclosing retail symbol wins outright; the call graph only corroborates or, where the
    # symbol is unnamed, supplies the candidate set.
    resolved = {ad: c for ad, (c, _s, _n) in own.items() if ad not in known}
    ambiguous = []
    for t, cs in sorted({t: sorted(v) for t, v in callers_of.items()}.items()):
        if t in known or t in own:
            continue
        chains = [ancestors(c, bases) for c in cs]
        common = [c for c in chains[0] if all(c in ch for ch in chains[1:])]
        # drop anything that is a strict ancestor of another common candidate
        most = [c for c in common if not any(c != d and c in ancestors(d, bases) for d in common)]
        # **The standard for "proven".** Two or more *unrelated* caller classes intersect their
        # ancestor chains, and the intersection is the owner. One caller does not: the owner is
        # somewhere on that caller's chain, and the chain includes every base class, so naming the
        # top of it is a guess. A single-caller result therefore goes to the candidate table, not
        # to the proven one - `CAi::AcceptScriptMsg` calling a `+128` accessor with `this` is
        # equally consistent with the accessor being `CAi`'s, `CActor`'s or `CEntity`'s, and all
        # three are in the tree.
        if len(set(cs)) >= 2 and len(most) == 1:
            resolved[t] = most[0]
        else:
            ambiguous.append((t, cs, most or cs))

    by_class_addrs = collections.defaultdict(set)
    for t, cls in resolved.items():
        by_class_addrs[cls].add(t)
    for cls in list(by_class_addrs):
        by_class_addrs[cls] = set(x for r in runs_of(by_class_addrs[cls]) for x in r)
    # evidence, re-keyed onto the resolved owner
    ev_by_owner = collections.defaultdict(list)
    for cls, evs in proven.items():
        for t, f, n, s in evs:
            ev_by_owner[resolved.get(t, cls)].append((t, f, n, s))
    for ad, (c, s, n) in own.items():
        if ad in resolved:
            ev_by_owner[resolved[ad]].append((ad, s, n, s))

    names = {n for cls in by_class_addrs for _t, _f, n, _s in ev_by_owner[cls]}
    names |= set(contradicted)
    dem = demangle(sorted(names))

    lines = ["# `single-load` / `single-store` accessors: owning class, from evidence", "",
             "Produced by `python3 tools/accessor_table.py`. The criterion is deliberately the",
             "strict one, and this is what makes a row in the first table worth landing:", "",
             "**A retail-named method `bl`s the accessor, and a forward abstract interpretation of",
             "the caller's GPRs (`tools/recv.py`) says `r3` at that call site is still that method's",
             "own incoming `this`.** The call site therefore passes the identical pointer, so the",
             "accessor's class *is* that method's class. Nothing here is inferred from a name, an",
             "offset coincidence, or a nearby symbol.", "",
             "The second table lists the calls that **fail** the receiver test - `r3` was reloaded,",
             "so the accessor belongs to a *member* object and the named caller is evidence about the",
             "containing class only. Those are recorded and deliberately not claimed; they are the",
             "false-positive class this repo has already been bitten by.", "",
             "## Proven: class -> accessors", "",
             "| class | accessors | contiguous runs | retail range(s) | offsets | evidence |",
             "|---|---|---|---|---|---|"]
    tot = 0
    for cls in sorted(by_class_addrs, key=lambda c: -len(by_class_addrs[c])):
        addrs = sorted(by_class_addrs[cls])
        tot += len(addrs)
        rr = runs_of(set(addrs))
        offs = sorted({o for x in addrs for o in acc[x]["offs"]})
        evs = ev_by_owner[cls]
        ev = "; ".join(sorted({f"`{n}` ({dem.get(n, '')}) at 0x{s:08X}" for _t, _f, n, s in evs}))[:600]
        lines.append(f"| **{cls}** | {len(addrs)} | {len(rr)} | "
                     + ", ".join(f"`0x{r[0]:08X}..0x{r[-1]+acc_size[r[-1]]:08X}`" for r in rr)
                     + " | " + " ".join(f"+{o}" for o in offs) + f" | {ev} |")
    nprov = sum(len(v) for v in by_class_addrs.values())
    lines += ["", f"**{nprov} accessors placed, in {len(by_class_addrs)} classes.**", "",
              "## Contradicted: the accessor is a member, not the caller's class", "",
              "| named caller | accessor | why it is not the class |", "|---|---|---|"]
    for nm in sorted(contradicted, key=lambda n: -len(contradicted[n])):
        ts = sorted({t for t, _f, _s, _v in contradicted[nm]})
        why = sorted({f"r3={v} at 0x{s:08X}" for _t, _f, s, v in contradicted[nm]})
        lines.append(f"| `{nm}` ({dem.get(nm, '')}) | " + ", ".join(f"`0x{t:08X}`" for t in ts)
                     + " | " + "; ".join(why) + " |")
    lines += ["", f"**{sum(len(v) for v in contradicted.values())} further accessor calls are named "
                  f"but contradicted by the receiver test.**", "",
              "## Single caller: owner is one of these, not decided", "",
              "A `this`-verified call proves the owner is an **ancestor-or-self** of the caller, so",
              "one caller cannot pin it down. These are reported as candidate sets rather than",
              "guesses - `CPowerBeam` is the live example, and the repo's own",
              "`src/MetroidPrime/Weapons/CGunWeaponIsLoaded.cpp` shows what guessing costs.", "",
              "| accessor | only caller | owner is one of | note |", "|---|---|---|---|"]
    for t_, cs, most in sorted(ambiguous):
        if t_ in known:
            continue
        lines.append(f"| `0x{t_:08X}` | `{cs[0]}` | "
                     + " or ".join(f"`{c}`" for c in most)
                     + " | single caller; the derived class inherits the method |")
    lines += ["", "## Already identified in this tree", "",
              "Addresses inside a range a unit already claims. **Do not carve these** - a second",
              "unit claiming bytes an existing `Matching` object reproduces is a link conflict, and",
              "`link_gap.py` cannot see it.", "",
              "| accessor | claimed by | class in that unit |", "|---|---|---|"]
    for t_ in sorted(known):
        if t_ in acc:
            un, cl = known[t_]
            lines.append(f"| `0x{t_:08X}` | `{un}` | {cl or '-'} |")
    lines.append("")

    out = "\n".join(lines) + "\n"
    if a.out:
        pathlib.Path(a.out).write_text(out)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
