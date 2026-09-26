#!/usr/bin/env python3
"""Every unit and function whose score moved between two objdiff reports.

    python3 tools/j2_movers.py BASE.json NEW.json

`report_diff.py` is a ratchet: it prints regressions and fails. A *global header change* is
the case where the interesting output is the other direction too - a `Matching` unit that
moved at all is a gate failure, and a `NonMatching` unit that moved is a signal. This
prints both, so a global change is accountable rather than merely survived.
"""
import json
import sys


def load(path):
    r = json.load(open(path))
    units, fns = {}, {}
    for u in r["units"]:
        m = u.get("measures", {})
        units[u["name"]] = {
            "linked": bool(u.get("metadata", {}).get("complete")),
            "fuzzy": float(m.get("fuzzy_match_percent") or 0.0),
            "code": float(m.get("matched_code_percent") or 0.0),
            "mf": m.get("matched_functions", 0),
            "tf": m.get("total_functions", 0),
        }
        for f in u.get("functions", []):
            fns[(u["name"], f["name"])] = (float(f.get("fuzzy_match_percent") or 0.0),
                                           int(f.get("size") or 0))
    return units, fns


def main():
    bu, bf = load(sys.argv[1])
    nu, nf = load(sys.argv[2])

    print("== units whose measures moved")
    for name in sorted(set(bu) | set(nu)):
        b, n = bu.get(name), nu.get(name)
        if b == n:
            continue
        if b is None:
            print(f"  NEW      {name}: {n['fuzzy']:.2f}% fuzzy, {n['mf']}/{n['tf']} fn"
                  f"{' LINKED' if n['linked'] else ''}")
            continue
        if n is None:
            print(f"  GONE     {name}")
            continue
        d = n["fuzzy"] - b["fuzzy"]
        dc = n["code"] - b["code"]
        tag = "LINKED-WORSE" if (d < -1e-9 and n["linked"]) else ("linked" if n["linked"] else "")
        if d or dc or b["linked"] != n["linked"] or (b["mf"], b["tf"]) != (n["mf"], n["tf"]):
            print(f"  {d:+.2f}% fuzzy {dc:+.2f}% code  {b['mf']}/{b['tf']} -> {n['mf']}/{n['tf']} fn"
                  f"  {tag}  {name}")

    print("\n== functions whose score moved")
    for key in sorted(set(bf) | set(nf)):
        b, n = bf.get(key), nf.get(key)
        if b is None:
            print(f"  NEW      {n[0]:6.2f}% {n[1]:6} B  {key[0]} :: {key[1]}")
            continue
        if n is None:
            print(f"  GONE     {b[0]:6.2f}% {b[1]:6} B  {key[0]} :: {key[1]}")
            continue
        if abs(n[0] - b[0]) < 1e-9:
            continue
        unit = nu.get(key[0]) or bu.get(key[0]) or {}
        tag = "  <-- LINKED UNIT" if unit.get("linked") else ""
        print(f"  {b[0]:6.2f}% -> {n[0]:6.2f}%  ({n[0]-b[0]:+.2f}) {n[1]:6} B"
              f"  {key[0]} :: {key[1]}{tag}")


if __name__ == "__main__":
    main()
