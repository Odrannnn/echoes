#!/usr/bin/env python3
"""Rank retail functions worth carving out of an unclaimed dtk auto_ range into their own unit.

    python3 tools/mine_carve.py [--report build/report.json] [--gap docs/research/port_link_gap_list.md]
                                [--module main] [--max-size 64] [--limit 60]

Three sets are intersected, because a carve only pays three times:

  * `config/G2ME01/symbols.txt` gives every retail function's name, address and size;
  * `build/report.json` says which of them are still at 0% and which unit holds them - a
    function inside `main/auto_03_*` is in a range no unit claims, which is what makes it
    carveable (a function inside a claimed unit's range is not);
  * the port's link-gap list says whether the symbol is one the port actually asks for, which
    is what turns "+1 linked" into "+1 linked the port can use".

Names are matched by demangling both sides: `symbols.txt` uses MWCC's
`Class__MethodArgs` spelling, the gap list uses Itanium mangling.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SYMBOLS = re.compile(r"^(\S+) = (\w+):(0x[0-9A-Fa-f]+); // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?")
GAP_LINE = re.compile(r"^- `(.+)`$")


def load_symbols(path):
    out = []
    for line in pathlib.Path(path).read_text(errors="replace").splitlines():
        m = SYMBOLS.match(line.strip())
        if not m or m.group(4) != "function" or not m.group(5):
            continue
        out.append((m.group(1), m.group(2), int(m.group(3), 16), int(m.group(5), 16)))
    return out


def cxxfilt(names):
    """Demangle in one process; MWCC's Class__MethodArgs demangles to the same C++ name."""
    if not names:
        return {}
    p = subprocess.run(["c++filt"], input="\n".join(names) + "\n", text=True,
                       capture_output=True)
    got = p.stdout.splitlines()
    return dict(zip(names, got))


def mwcc_split(name):
    """`SwitchToTire__10CMorphBallFv` -> `CMorphBall::SwitchToTire`.

    MWCC mangles a class member as `method__<len><Class><args>`; the Itanium form the gap
    list uses is `Class::method`. Only the comparison needs to be right, so the argument
    encoding is dropped - it is the class and the method that identify the symbol.
    """
    m = re.match(r"^([^_~][A-Za-z0-9_]*)__(\d+)([A-Za-z_][A-Za-z0-9_]*)", name)
    if not m:
        return None
    cls = m.group(3)
    n = int(m.group(2))
    if len(cls) < n:
        return None
    return f"{cls[:n]}::{m.group(1)}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default=str(ROOT / "build" / "report.json"))
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    ap.add_argument("--gap", default=str(ROOT / "docs" / "research" / "port_link_gap_list.md"))
    ap.add_argument("--module", default="main", help="only functions in this module's units")
    ap.add_argument("--in-gap-only", action="store_true")
    ap.add_argument("--max-size", type=int, default=64)
    ap.add_argument("--limit", type=int, default=60)
    a = ap.parse_args()

    report = json.load(open(a.report))

    # 1. every function the report knows, and the unit holding it
    fn_unit, fn_size, fn_pct = {}, {}, {}
    addr_of_report = {}
    for u in report["units"]:
        for f in u.get("functions", []):
            key = (u["name"], f["name"])
            fn_unit[key] = u["name"]
            fn_size[key] = int(f.get("size") or 0)
            fn_pct[key] = float(f.get("fuzzy_match_percent") or 0.0)
            va = f.get("metadata", {}).get("virtual_address")
            if va:
                addr_of_report[f["name"]] = int(va)

    # 2. the module's own symbol table, keyed by address
    syms = load_symbols(a.symbols)
    by_addr = {addr: (name, size) for name, _sec, addr, size in syms}

    # 3. the port's gap list, demangled so the two spellings meet
    gap = []
    p = pathlib.Path(a.gap)
    if p.exists():
        gap = [m.group(1) for m in
               (GAP_LINE.match(l) for l in p.read_text().splitlines()) if m]
    dem_gap = cxxfilt(gap)
    # Itanium -> "C::m"; MWCC -> "C::m" too once demangled.
    gap_dem = set()
    for s, d in dem_gap.items():
        gap_dem.add(re.sub(r"\s*\(.*\)$", "", d))

    # 4. walk the report's unmatched functions, in each module's auto_ units
    rows = []
    for (unit, fname), pct in fn_pct.items():
        if pct >= 100.0:
            continue
        if a.module and not unit.startswith(a.module + "/"):
            continue
        if not unit.split("/", 1)[1].startswith("auto_"):
            continue
        # `fn_<addr>` is the retail-unnamed spelling dtk derived from the map; a real name
        # is worth more (the port can call it), so it sorts first among equal sizes.
        m = re.match(r"^fn_([0-9A-Fa-f]{8})$", fname)
        named = m is None
        if m:
            addr = int(m.group(1), 16)
        else:
            addr = addr_of_report.get(fname)
            if addr is None:
                continue
        name, size = by_addr.get(addr, (fname, fn_size[(unit, fname)]))
        size = size or fn_size[(unit, fname)]
        if size > a.max_size:
            continue
        dem = cxxfilt([name])[name]
        dem = re.sub(r"\s*\(.*\)$", "", dem)
        keys = {name, dem}
        sp = mwcc_split(name)
        if sp:
            keys.add(sp)
        in_gap = bool(keys & gap_dem)
        rows.append((0 if in_gap else (1 if named else 2), size, addr, unit, name, dem, size))

    rows.sort()
    if a.in_gap_only:
        rows = [r for r in rows if r[0] == 0]
    print(f"{'':4s} {'size':>5s}  {'address':10s}  {'unit':34s}  demangled")
    shown = 0
    for rank, _s, addr, unit, name, dem, size in rows:
        if shown >= a.limit:
            break
        shown += 1
        tag = "GAP" if rank == 0 else "fn" if rank == 2 else "-"
        print(f"{tag:4s} {size:5d}  0x{addr:08X}  {unit:34s}  {dem}")
    total = len(rows)
    print(f"\n{total} candidates, {sum(1 for r in rows if r[0] == 0)} in the port's gap list, "
          f"{sum(1 for r in rows if r[0] == 1)} real-named.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
