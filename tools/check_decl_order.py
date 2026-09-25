#!/usr/bin/env python3
"""Which units emit their functions in a different order than retail does?

    tools/check_decl_order.py [--unit <name>] [--all] [--list]

mwcceppc emits function definitions in **reverse source order**, and mwldeppc places an input
object's `.text` in that object's own section order. So a unit's functions must be declared
descending by retail offset, and when they are not, the module's or the DOL's bytes come out
**permuted** - a failure with no diagnostic anywhere:

  - objdiff pairs functions by name, so every one of them still scores 100%
  - `tools/unit_fit.sh` compares sizes, and a permutation does not change them
  - the link succeeds, because the total size is identical

Only `tools/flip_test.sh` catches it, and only when the unit is `Matching`. This finds the same
defect in a `NonMatching` unit, before anyone spends a lane on it: it compares the order our
object emits (its symbols, in address order) against the order retail has them (the addresses
in `build/report.json`), for every function both know by name.

A permuted unit is not broken - none of it is in the binary - but **it cannot be flipped until
it is reordered**, and the flip is the only thing that makes its functions count. So the permuted
units are a work list, kept in `docs/research/decl_order.md` with a reason each, and this tool
checks that list against the tree: a new permuted unit fails, and so does a listed one that is
no longer permuted, because that means it was reordered and the entry should go.

Expect **no** `Matching` unit in the list - those already hold the hash, so their order is right
by construction. What it finds is the units that *would* break on a flip. Measured 2026-09-25 on
`AIMannedTurret`, whose unit was permuted and had been reported 3/3 at 100% for several
sessions; see "Declare in reverse" in `docs/RUNNING_THE_DECOMP.md`.
"""
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
NM = ROOT / "build" / "binutils" / "powerpc-eabi-nm"
REPORT = ROOT / "build" / "report.json"
DOC = ROOT / "docs" / "research" / "decl_order.md"
# Function names in the report/objects are objdiff-style: `GetResInfo__8CPakFileCFUi`.


def object_for(unit_name):
    """A unit's compiled object. Both the DOL (`main/Kyoto/CPakFile`) and a REL module
    (`ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoin`) keep their objects in the one
    `build/G2ME01/src/` tree, so the unit's path after the first `/` is the object path."""
    if "/" not in unit_name:
        return None
    return ROOT / "build" / "G2ME01" / "src" / (unit_name.split("/", 1)[1] + ".o")


def our_order(obj):
    """[(address, name)] for our object, in address order."""
    if not obj.exists():
        return None
    out = subprocess.run([str(NM), "-n", "--defined-only", str(obj)],
                         capture_output=True, text=True).stdout
    pairs = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in ("t", "T"):
            pairs.append((int(parts[0], 16), parts[2]))
    return pairs


def retail_order(report):
    """{unit name: [(address, function name)]} in retail's order."""
    out = {}
    for unit in report["units"]:
        pairs = []
        for fn in unit.get("functions", []):
            meta = fn.get("metadata") or {}
            if "virtual_address" in meta:
                pairs.append((int(meta["virtual_address"]), fn["name"]))
        if pairs:
            out[unit["name"]] = sorted(pairs)
    return out


def permutation(unit, ours, retail):
    """Names both sides know, in each side's order, and whether they agree."""
    retail_addr = {name: addr for addr, name in retail}
    common = [(a, n) for a, n in ours if n in retail_addr]
    retail_common = sorted((retail_addr[n], n) for a, n in common)
    return [n for _, n in common], [n for _, n in retail_common]


def documented():
    """Unit names listed in docs/research/decl_order.md, as `- `<name>` - reason` bullets."""
    if not DOC.exists():
        return {}
    out = {}
    for line in DOC.read_text().splitlines():
        m = re.match(r"^-\s+`([\w./+-]+)`\s*(?:[-—:]\s*(.*))?$", line)
        if m:
            out[m.group(1)] = (m.group(2) or "").strip()
    return out


def main():
    args = sys.argv[1:]
    if not NM.exists():
        print("error: %s missing (see docs/LANE_BRIEFING.md)" % NM, file=sys.stderr)
        return 2
    if not REPORT.exists():
        print("error: no build/report.json - run tools/decomp_build.sh", file=sys.stderr)
        return 2
    report = json.load(open(REPORT))
    retail = retail_order(report)
    complete = {u["name"] for u in report["units"] if (u.get("metadata") or {}).get("complete")}

    only = None
    if "--unit" in args:
        only = args[args.index("--unit") + 1]
    show_all = "--all" in args
    listing = "--list" in args

    bad, checked = [], 0
    for name, ret in sorted(retail.items()):
        if only and only not in name:
            continue
        obj = object_for(name)
        if obj is None:
            continue
        ours = our_order(obj)
        if not ours:
            continue
        checked += 1
        got, want = permutation(name, ours, ret)
        if len(got) < 2:
            continue
        if got != want:
            bad.append((name, name in complete, got, want))

    if listing:
        for name, is_complete, got, want in bad:
            print("%-62s %3d fns  %s" % (name, len(got),
                                         "MATCHING but permuted" if is_complete else "would break on a flip"))
        print("total: %d of %d unit(s) with a compiled object are permuted" % (len(bad), checked))
        return 0

    if only:  # a diagnostic run: report what it found and stop
        for name, is_complete, got, want in bad:
            print("%-58s %s" % (name, "MATCHING but permuted!" if is_complete else "would break on a flip"))
            print("   first %d functions, ours vs retail:" % min(len(got), 8))
            for k in range(min(len(got), 8)):
                print("   %s %-46s %s" % ("*" if got[k] != want[k] else " ", got[k], want[k]))
            if len(got) > 8:
                print("     ... %d more" % (len(got) - 8))
        if not bad:
            print("ok: %d unit(s) checked, none emits its functions out of retail order" % checked)
        return 0

    # The documented work list, checked against the tree.
    listed = documented()
    problems = []
    for name, is_complete, got, want in bad:
        if name not in listed:
            problems.append("%-62s permuted and not in %s - add it with a reason"
                            % (name, DOC.name))
        if is_complete:
            problems.append("%-62s is Matching but permuted - that should be impossible" % name)
    for name in sorted(set(listed) - {n for n, _, _, _ in bad}):
        problems.append("%-62s listed as permuted but is not any more - delete the entry" % name)

    if problems:
        print("declaration order not accounted for:")
        for line in problems:
            print("  " + line)
        print("\npolicy and work list: %s" % DOC)
        print("fix: declare the unit's functions descending by retail offset. "
              "Nothing else reports this.")
        return 1
    print("ok: %d unit(s) checked, %d permuted, all %d accounted for in %s"
          % (checked, len(bad), len(listed), DOC.name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
