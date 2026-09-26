#!/usr/bin/env python3
"""Per-unit *and* per-function score movement between two objdiff reports.

    python3 tools/unit_movement.py BASE.json NEW.json

`tools/report_diff.py` is the gate: it fails on a regression and prints the offending
functions. This is its per-unit counterpart, for the question a unit-movement report has to
answer - **which units moved, in which direction** - including movement the gate is silent about:
a change can leave `matched` and `linked` untouched and still move a `NonMatching` unit's
average, and a unit's `complete` flag can flip with no function name changing at all. Prints
every unit whose `complete` flag, matched/total function counts or matched/total code bytes
moved, then every function whose percentage or size moved, then additions and vanishings.
Units that did not move are counted, not listed.

    python3 tools/gate.sh --baseline     # on a clean tree, first - see the note below
    # ...make the change, rebuild, generate build/report.json...
    python3 tools/unit_movement.py build/report.base.json build/report.json

The baseline has to be recorded **before** the first edit, not after the last: this project's
`CHECK_OFFSETOF` and `NESTED_CHECK_SIZEOF` cannot guard a class whose members are all `private:`
(mwcceppc 2.7 refuses to name a private member outside its class), so for those classes a
per-function movement report over a recorded baseline is the *only* instrument. Deleting
`CGameGlobalObjects`'s four-byte member shows up here and nowhere else. Written for lane j4;
see `docs/research/paks.md`'s third correction and `docs/RUNNING_THE_DECOMP.md`,
"`CHECK_SIZEOF` is a consistency check, not a measurement".
"""
import json
import sys


def load(path):
    r = json.load(open(path))
    units, fns = {}, {}
    for u in r["units"]:
        m = u.get("measures", {}) or {}
        units[u["name"]] = {
            "complete": bool(u.get("metadata", {}).get("complete")),
            "mf": m.get("matched_functions") or 0,
            "tf": m.get("total_functions") or 0,
            "mc": int(m.get("matched_code") or 0),
            "tc": int(m.get("total_code") or 0),
        }
        for f in u.get("functions", []):
            fns[(u["name"], f["name"])] = (float(f.get("fuzzy_match_percent") or 0.0),
                                           int(f.get("size") or 0))
    return units, fns


def main():
    bu, bf = load(sys.argv[1])
    nu, nf = load(sys.argv[2])
    moved, added_units = [], []
    for n in sorted(set(bu) | set(nu)):
        b, nw = bu.get(n), nu.get(n)
        if b is None:
            added_units.append(n)
            continue
        if nw is None:
            moved.append((n, b, None))
            continue
        if (b["complete"], b["mf"], b["tf"], b["mc"], b["tc"]) != \
           (nw["complete"], nw["mf"], nw["tf"], nw["mc"], nw["tc"]):
            moved.append((n, b, nw))
    print("=== UNITS: %d moved, %d added, %d total in baseline"
          % (len(moved), len(added_units), len(bu)))
    if added_units:
        for n in added_units:
            print("  NEW    %s  %s" % (n, nu[n]))
    for n, b, nw in moved:
        if nw is None:
            print("  GONE   %s  was complete=%s mf=%s/%s code=%s/%s" % (n, b["complete"], b["mf"], b["tf"], b["mc"], b["tc"]))
            continue
        d = []
        if b["complete"] != nw["complete"]:
            d.append("complete %s->%s" % (b["complete"], nw["complete"]))
        if (b["mf"], b["tf"]) != (nw["mf"], nw["tf"]):
            d.append("matched %s->%s of %s->%s" % (b["mf"], nw["mf"], b["tf"], nw["tf"]))
        if (b["mc"], b["tc"]) != (nw["mc"], nw["tc"]):
            d.append("code %s->%s of %s->%s" % (b["mc"], nw["mc"], b["tc"], nw["tc"]))
        print("  MOVED  %-70s %s   [baseline complete=%s]" % (n, "; ".join(d), b["complete"]))

    gone = set(bf) - set(nf)
    added = set(nf) - set(bf)
    changed = []
    for k in sorted(set(bf) & set(nf)):
        if abs(bf[k][0] - nf[k][0]) > 1e-9 or bf[k][1] != nf[k][1]:
            changed.append(k)
    print()
    print("=== FUNCTIONS: %d changed, %d added, %d vanished" % (len(changed), len(added), len(gone)))
    for k in sorted(changed):
        o, ns = bf[k], nf[k]
        dirn = "WORSE" if ns[0] < o[0] - 1e-9 else ("BETTER" if ns[0] > o[0] + 1e-9 else "same-pct")
        extra = "" if ns[1] == o[1] else "  size %s->%s" % (o[1], ns[1])
        print("  %-6s %-72s %6.2f -> %6.2f%s" % (dirn, k[0] + " :: " + k[1], o[0], ns[0], extra))
    for k in sorted(added):
        print("  ADDED  %-72s %6.2f (size %s)" % (k[0] + " :: " + k[1], nf[k][0], nf[k][1]))
    for k in sorted(gone):
        print("  GONE   %-72s was %6.2f (size %s)" % (k[0] + " :: " + k[1], bf[k][0], bf[k][1]))


if __name__ == "__main__":
    main()
