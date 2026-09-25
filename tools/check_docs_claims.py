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


_WORDS = {11: "eleven", 12: "twelve", 13: "thirteen", 14: "fourteen", 15: "fifteen",
          16: "sixteen", 17: "seventeen", 18: "eighteen", 19: "nineteen", 20: "twenty"}


def _num_word(n: int) -> str:
    """Spelled-out count, so the docs read as prose. Falls back to digits."""
    return _WORDS.get(n, str(n))


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

    # 4a. The port probe's file count, quoted in five files. Derived the same way
    #     tools/probe_sources.sh collects them, without compiling anything: the game
    #     manifest plus the platform sources of the two object libraries it sweeps.
    #     This was stale in three separate files at once, because nothing derived it.
    probe_files = set(re.findall(r"^\s+(src/\S+)$",
                                 (ROOT / "files.cmake").read_text(), re.M))
    # Mirror tools/probe_sources.sh's awk line for line rather than trying to match
    # the block with one regex: a target's sources can close with ')' on the last
    # source's line or on a line of its own, and a non-greedy match that allows
    # "newline then a word" stops at the *second source*, not the end of the block.
    collecting = False
    for line in (ROOT / "CMakeLists.txt").read_text().splitlines():
        if re.search(r"add_library\((mp_platform|mp_port_entry) OBJECT", line):
            collecting = True
        if collecting:
            probe_files.update(re.findall(r"(platform/\S+?)(?=\)|\s|$)", line))
            if ")" in line:
                collecting = False
    probe_files.discard("")
    probe_n = len(probe_files)
    stale_probe = sorted({int(n) for n in re.findall(r"\b(\d{3}) files\b", blob)
                          if int(n) != probe_n})
    for n in stale_probe:
        problems.append(f"stale:   {n} files  (the port probe compiles {probe_n})")
    # Deliberately *not* required to appear. This number moves with every
    # files.cmake edit, and a check that demands a doc edit each time turns a
    # ratchet into merge conflicts - which is exactly what happened when the
    # count went 133 -> 226. Any number that IS quoted is policed above; nothing
    # obliges the docs to quote one.

    # 4b. The real linker's undefined count, from tools/link_check.sh's recorded
    #     baseline. The linker is the ground truth for the port and it has moved
    #     every time anything landed, so a doc that quotes a stale one is wrong in
    #     the way that matters most: it is the number being planned against.
    baseline = ROOT / "docs/research/port_link_baseline.txt"
    if baseline.exists():
        recorded = dict(re.findall(r"^(undefined|duplicates) (\d+)$",
                                   baseline.read_text(), re.M))
        if "undefined" in recorded:
            n = recorded["undefined"]
            # Accept the number anywhere near the word, in either order: three
            # separate files phrase this differently and a check that demands one
            # exact form only enforces one file's wording.
            near = re.search(rf"\b{n}\b[^\n]{{0,24}}undefined"
                             rf"|undefined[^\n]{{0,24}}\b{n}\b", blob)
            if not near:
                problems.append(
                    f"missing: the linker's undefined count ({n}) is not quoted in the docs; "
                    f"run tools/link_check.sh --record after it moves")
            if recorded.get("duplicates", "0") != "0":
                problems.append(f"stale:   the link baseline records "
                                f"{recorded['duplicates']} duplicate definitions; "
                                f"the RELMain collision is resolved and this must be 0")
    else:
        problems.append("missing: docs/research/port_link_baseline.txt "
                        "(run tools/link_check.sh --record)")

    # 5. Claims that were measured false and must not come back. Each one cost real time: the first
    #    let a lane trust a vacuous PASS, the second let a table call a module "the working example"
    #    for several sessions while promoting it broke its hash.
    must_not_appear("configure.py refuses to run at all",
                    "configure.py accepts a Matching unit with no source and links retail instead")
    must_not_appear("an `auto_*` unit's functions count as matched by default",
                    "auto_* units have 0 of 24,456 functions matched")
    must_not_appear("**without our object linked**. The working example",
                    "AIMannedTurret does not hold its module hash when promoted")
    must_not_appear("`CPatterned`/`CAi` still do not exist",
                    "both are landed Matching units")

    # 4b2. The gap table's counts must come from the generated list and sum to its
    #      total. This paragraph has been wrong twice - it once claimed a split that
    #      summed to 896 of 1440, and it drifted again while three groups were being
    #      closed in parallel, leaving rows that summed to 499 against a list of 524.
    #      A formatted table is not a measurement, and this one is generated.
    gapdoc = (ROOT / "docs/research/port_link_gap.md")
    gaplist = (ROOT / "docs/research/port_link_gap_list.md")
    if gapdoc.exists() and gaplist.exists():
        listed = {}
        for m in re.finditer(r"^## (.+?)\n(.*?)(?=^## |\Z)", gaplist.read_text(), re.M | re.S):
            listed[re.sub(r"\s*\(\d+\)$", "", m.group(1).strip())] = \
                len(re.findall(r"^- ", m.group(2), re.M))
        # Scope to the group table. The first version matched every `| n | n |` row
        # in the file and so picked up the category-split table as well, which is
        # how a check meant to catch drift reported two false stale rows instead.
        gtext = gapdoc.read_text()
        gs = gtext.find("| group | count | what closes it |")
        ge = gtext.find("\n\n", gs) if gs >= 0 else -1
        rows = (re.findall(r"^\| ([^|]+?) \| (\d+) \|", gtext[gs:ge], re.M)
                if gs >= 0 and ge > gs else [])
        want = {re.sub(r"[`*~]", "", n).strip(): int(c) for n, c in rows}
        for name, count in sorted(listed.items()):
            if name in want and want[name] != count:
                problems.append(f"stale:   the gap table says {name} is {want[name]}, "
                                f"the generated list has {count}")
        total = sum(listed.values())
        shown = sum(int(c) for _, c in rows)
        if shown != total:
            problems.append(f"stale:   the gap table's rows sum to {shown}, "
                            f"the generated list holds {total}")

    # 4c. The research index: the count it claims, and that every row it lists
    #     exists. The index is a curated subset - generated files like
    #     port_link_gap_list.md are referenced from elsewhere and deliberately not
    #     listed - so this checks the rows, not the directory.
    handoff = docs["docs/HANDOFF.md"]
    rows = sorted(set(re.findall(r"^\| `(docs/research/[^`]+)`", handoff, re.M)))
    for r in rows:
        if not (ROOT / r).exists():
            problems.append(f"missing: the research index lists {r}, which does not exist")
    m = re.search(r"^(\w+) files carry what a later session", handoff, re.M)
    if m is None:
        problems.append("missing: HANDOFF no longer says how many files the research index carries")
    elif m.group(1).lower() != _num_word(len(rows)):
        problems.append(f"stale:   HANDOFF says {m.group(1)} files in the research index, "
                        f"it lists {len(rows)}")

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
