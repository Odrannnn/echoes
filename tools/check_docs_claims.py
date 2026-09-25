#!/usr/bin/env python3
"""Check the documentation's factual claims against the tree.

    python3 tools/check_docs_claims.py

The docs are load-bearing - a session that trusts a stale one wastes its whole budget - and the rule
saying so already existed without preventing this: through 2026-09-25 the state block was kept
current while a paragraph listed sixteen modules linking our code when three of them had no
`Rel(...)` block at all, `AIMannedTurret` was called "the working example" though promoting it breaks
its hash, and per-unit counts drifted three times. Every number below is derivable, so derive it.

Exit status is 1 if any claim in the docs disagrees with the tree. Run it before committing a change
that moves a number, and after any config merge.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ["docs/HANDOFF.md", "docs/RUNNING_THE_DECOMP.md", "docs/LANE_BRIEFING.md"]


def load_docs() -> dict:
    return {d: (ROOT / d).read_text() for d in DOCS}


def unit_counts(report: dict, name: str):
    for u in report["units"]:
        if u["name"] == name:
            m = u["measures"]
            return m.get("matched_functions", 0), m.get("total_functions", 0)
    return None


def main() -> int:
    report = json.loads((ROOT / "build/report.json").read_text())
    docs = load_docs()
    blob = "\n".join(docs.values())
    measures = report["measures"]

    dol = sum(u["measures"].get("matched_functions", 0)
              for u in report["units"] if u["name"].startswith("main/"))
    dol_total = sum(u["measures"].get("total_functions", 0)
                    for u in report["units"] if u["name"].startswith("main/"))
    rel = measures["matched_functions"] - dol
    rel_total = measures["total_functions"] - dol_total

    problems = []

    def must_appear(text: str, why: str):
        if text not in blob:
            problems.append(f"missing: {text!r}  ({why})")

    def must_not_appear(text: str, why: str):
        if text in blob:
            problems.append(f"stale:   {text!r}  ({why})")

    # 1. The state block.
    must_appear(f"matched    {measures['matched_functions']} / {measures['total_functions']} functions",
                "HANDOFF state block: total matched")
    must_appear(f"DOL units  {dol} / {dol_total} functions", "HANDOFF state block: DOL matched")
    must_appear(f"REL units   {rel} / {rel_total} functions", "HANDOFF state block: REL matched")

    # 2. Per-unit counts quoted in the prose.
    named = [
        ("main/MetroidPrime/Enemies/CAi", "CAi"),
        ("main/MetroidPrime/Enemies/CPatterned", "CPatterned"),
        ("main/MetroidPrime/TypesMatch", "TypesMatch"),
        ("main/MetroidPrime/CStateManager", "CStateManager"),
        ("main/MetroidPrime/Player/CPlayerGun", "CPlayerGun"),
        ("main/MetroidPrime/Player/CPlayerState", "CPlayerState"),
    ]
    for name, label in named:
        counts = unit_counts(report, name)
        if counts is None:
            problems.append(f"missing: unit {name} is not in the report any more")
            continue
        matched, total = counts
        if f"`{label}` {matched}/{total}" not in blob and f"`{label}` {matched} of {total}" not in blob:
            problems.append(f"missing: per-unit count for {label} ({matched}/{total} in the report)")

    # 3. The module list, which is the claim that went wrong for real.
    wiring = subprocess.run([sys.executable, str(ROOT / "tools/check_module_wiring.py")],
                            capture_output=True, text=True).stdout
    m = re.search(r"(\d+) unit\(s\) of our own code in (\d+) module\(s\): (.*)", wiring)
    if m:
        units_n, mods_n, names = m.group(1), m.group(2), m.group(3).strip()
        must_appear(f"**{units_n} units of our own code in {mods_n} modules**",
                    "HANDOFF: module wiring count")
        for mod in names.split(", "):
            if f"`{mod}`" not in blob:
                problems.append(f"missing: module {mod} links our code but is not named in the docs")
    else:
        problems.append("could not read tools/check_module_wiring.py output")

    # 4. The hashes the docs pin.
    must_appear("6ef9b491d0cc08bc81a124fdedb8bfaec34d0010", "the DOL sha1 the docs quote")

    if problems:
        print("docs claims that disagree with the tree:")
        for p in problems:
            print("  " + p)
        print("\nUpdate the docs in the same commit as the change, or correct the claim in place and "
              "say it was superseded.")
        return 1
    print("docs claims agree with the tree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
