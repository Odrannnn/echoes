#!/usr/bin/env python3
"""Rewrite README.md's progress badges from build/report.json.

  python3 tools/update_readme_progress.py           # rewrite README.md in place
  python3 tools/update_readme_progress.py --check   # exit 1 if README.md is stale

The README's badges used to be decomp.dev shields for PrimeDecomp/echoes, which show
upstream's numbers, not this tree's. These are static shields.io badges carrying the
figures of the last local build, so run ./tools/decomp_build.sh first. Only G2ME01 is
measured here; the other versions' cells say so rather than show a number we did not
measure. Everything between the BEGIN/END markers is generated.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
REPORT = ROOT / "build" / "report.json"
BEGIN = "<!-- BEGIN progress (tools/update_readme_progress.py) -->"
END = "<!-- END progress -->"


def badge(label, pct):
    text = f"{pct:.2f}%25"
    return f"https://img.shields.io/badge/{label}-{text}-blue"


def render(report):
    m = report["measures"]
    cats = {c["id"]: c["measures"] for c in report.get("categories", [])}
    dol, rel = cats["dol"], cats["modules"]
    lines = [
        BEGIN,
        f"[Code Progress]: {badge('Code', m['matched_code_percent'])}",
        f"[Data Progress]: {badge('Data', m['matched_data_percent'])}",
        f"[DOL Progress]: {badge('DOL', dol['matched_code_percent'])}",
        f"[RELs Progress]: {badge('RELs', rel['matched_code_percent'])}",
        "[progress]: #progress",
        END,
    ]
    return "\n".join(lines)


def table(report):
    m = report["measures"]
    cats = {c["id"]: c["measures"] for c in report.get("categories", [])}
    rows = [("Everything", m)] + [(n, cats[k]) for n, k in
                                  (("DOL (main.dol)", "dol"), ("RELs (86 modules)", "modules"),
                                   ("Game code", "game"), ("SDK", "sdk")) if k in cats]
    out = [BEGIN.replace("progress", "progress-table", 1),
           "| Part | Code | Data | Functions | Fully linked code |",
           "|------|------|------|-----------|-------------------|"]
    for name, x in rows:
        out.append(f"| {name} | {x['matched_code_percent']:.2f}% | {x['matched_data_percent']:.2f}% "
                   f"| {x['matched_functions']} / {x['total_functions']} "
                   f"({x['matched_functions_percent']:.2f}%) | {x['complete_code_percent']:.2f}% |")
    out.append(END.replace("progress", "progress-table", 1))
    return "\n".join(out)


def main():
    report = json.loads(REPORT.read_text())
    old = README.read_text()
    new = old
    for block in (render(report), table(report)):
        b, e = block.splitlines()[0], block.splitlines()[-1]
        pat = re.compile(re.escape(b) + r".*?" + re.escape(e), re.S)
        if not pat.search(new):
            sys.exit(f"README.md has no {b!r} ... {e!r} block")
        new = pat.sub(lambda _: block, new)
    if "--check" in sys.argv:
        if new != old:
            print("README.md progress is stale; run python3 tools/update_readme_progress.py")
            return 1
        print("README.md progress agrees with build/report.json")
        return 0
    if new != old:
        README.write_text(new)
        print("README.md progress updated")
    else:
        print("README.md progress already current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
