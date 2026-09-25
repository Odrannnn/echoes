#!/usr/bin/env python3
"""Per-function diff of two objdiff reports; exit 1 on any regression.

    python3 tools/report_diff.py BASE.json NEW.json [--allow-drop UNIT ...]

Replaces the hand-typed before/after comparison. A change can raise a unit's average while making
one function worse, and can raise the fuzzy total while lowering what is actually linked; both are
regressions here. "linked" means the unit's metadata.complete is true (Matching and source exists),
which is the only count the project's one rule accepts.

**A rename is not a loss.** objdiff pairs functions by name, so renaming a symbol in
`symbols.txt` makes the old name vanish and the new name appear - which reads as a function
deleted. A vanished function is therefore matched against the names *added* in the same unit: if
one has the same size and a score no lower, it is the same function under a new name, and it is
reported as RENAMED rather than GONE. Without this, the cheapest possible improvement - giving an
unpaired 0.00% function the name its body actually has - fails the gate.
"""
import json
import sys


def load(path):
    r = json.load(open(path))
    units, fns = {}, {}
    for u in r["units"]:
        m = u.get("measures", {})
        units[u["name"]] = (bool(u.get("metadata", {}).get("complete")),
                            m.get("matched_functions", 0), m.get("total_functions", 0))
        for f in u.get("functions", []):
            fns[(u["name"], f["name"])] = (float(f.get("fuzzy_match_percent") or 0.0),
                                           int(f.get("size") or 0))
    linked = sum(v[1] for v in units.values() if v[0])
    return r["measures"], units, fns, linked


def main():
    args = sys.argv[1:]
    allow = set()
    if "--allow-drop" in args:
        i = args.index("--allow-drop")
        allow = set(args[i + 1:])
        args = args[:i]
    base_m, base_u, base_f, base_l = load(args[0])
    new_m, new_u, new_f, new_l = load(args[1])
    bad, renamed = [], []

    # Names that appeared where there was none: the partner a rename leaves behind.
    added = [k for k in new_f if k not in base_f]

    for key, (old, old_size) in sorted(base_f.items()):
        new = new_f.get(key)
        if key[0] in allow:
            continue
        if new is None:
            partner = next((k for k in added if k[0] == key[0] and new_f[k][1] == old_size
                            and new_f[k][0] + 1e-6 >= old), None)
            if partner:
                renamed.append((key, partner, old, new_f[partner][0]))
            else:
                # A function vanishing is how a dropped rename or a lost Rel(...) block shows up.
                bad.append(f"GONE     {key[0]} :: {key[1]} (was {old:.2f}%)")
        elif new[0] + 1e-6 < old:
            bad.append(f"WORSE    {key[0]} :: {key[1]} {old:.2f}% -> {new[0]:.2f}%")
    for name, (was_linked, mf, tf) in sorted(base_u.items()):
        now = new_u.get(name)
        if name in allow:
            continue
        if now is None:
            bad.append(f"UNIT GONE {name}")
        elif was_linked and not now[0]:
            bad.append(f"UNLINKED {name} (was Matching)")

    gained = [k for k, v in new_f.items() if v[0] >= 100.0 and base_f.get(k, (0.0, 0))[0] < 100.0]
    newly_linked = [n for n, v in new_u.items() if v[0] and not base_u.get(n, (False,))[0]]

    print(f"matched  {base_m['matched_functions']} -> {new_m['matched_functions']}   "
          f"linked {base_l} -> {new_l}   (+{len(gained)} functions at 100%, "
          f"{len(newly_linked)} units newly linked)")
    for n in newly_linked:
        print(f"  LINKED   {n}")
    for k in sorted(gained):
        print(f"  +100%    {k[0]} :: {k[1]}")
    for key, partner, old, now in renamed:
        print(f"  RENAMED  {key[0]} :: {key[1]} -> {partner[1]} ({old:.2f}% -> {now:.2f}%)")
    for b in bad:
        print("  " + b)
    if new_l < base_l:
        bad.append("linked total fell")
        print(f"  LINKED TOTAL FELL {base_l} -> {new_l}")
    print("REGRESSION" if bad else "no regression")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
