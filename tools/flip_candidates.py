#!/usr/bin/env python3
"""List NonMatching units that are close to Matching, from build/report.json.

    tools/flip_candidates.py              # units with every function at 100% (flip them now)
    tools/flip_candidates.py --missing 2  # units with at most 2 functions below 100%
    tools/flip_candidates.py --missing 1 --json   # one JSON object per unit, for queueing

`linked` counts a function only when its whole unit is Matching, so 4600 functions sat at 100%
inside NonMatching units after the upstream merge (2026-09-28). The cheapest `linked` there is:

  * missing 0 - no agent needed. Run `tools/flip_test.sh $(tools/flip_candidates.py)`: the ones that
    hold are free; the ones that fail are link-level (data, gap padding, weak ordering), not code.
  * missing 1-2 - one or two named functions stand between the unit and Matching. Those make
    small, checkable goal items; the reason names the function and its percentage.

Run it again after every upstream sync: each sync moves functions to 100% without flipping units.
"""
import argparse
import json
import re
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--missing", type=int, default=0, help="max functions below 100%% (default 0)")
    ap.add_argument("--min-missing", type=int, default=0, help="min functions below 100%%")
    ap.add_argument("--json", action="store_true", help="one JSON object per unit")
    ap.add_argument("--report", default="build/report.json")
    a = ap.parse_args()

    report = json.load(open(a.report))
    cfg = open("configure.py").read()
    rows = []
    for u in report["units"]:
        fns = u.get("functions") or []
        if not fns or u.get("metadata", {}).get("complete"):
            continue
        short = [f for f in fns if f.get("fuzzy_match_percent", 0) != 100]
        if not a.min_missing <= len(short) <= a.missing:
            continue
        name = u["name"].split("/", 1)[1]
        m = re.search(r'Object\(\s*NonMatching[^,]*,\s*"(' + re.escape(name) + r'\.(?:cpp|cp|c))"', cfg)
        if not m:
            continue  # a REL unit or an entry flip_test cannot address; see its module recipe
        rows.append({
            "unit": m.group(1),
            "functions": len(fns),
            "gain": len(fns) - len(short),
            "data_percent": round(u["measures"].get("matched_data_percent", 0), 1),
            "short": [{"name": f.get("metadata", {}).get("demangled_name") or f["name"],
                       "symbol": f["name"],
                       "percent": round(f.get("fuzzy_match_percent", 0), 2)} for f in short],
        })
    rows.sort(key=lambda r: (len(r["short"]), -r["gain"]))
    for r in rows:
        print(json.dumps(r) if a.json else r["unit"])
    print(f"{len(rows)} units, {sum(r['gain'] for r in rows)} functions would link, "
          f"{sum(len(r['short']) for r in rows)} to fix", file=sys.stderr)


if __name__ == "__main__":
    main()
