#!/usr/bin/env python3
"""Rewrite the four count lines in docs/HANDOFF.md's state block from build/report.json.

This replaces an ad-hoc script that lived in /tmp and had three defects, all of them
silent, and all of them of the same kind - a rewrite that cannot tell whether it worked:

1. **It keyed on hardcoded values.** It searched for the literal line prefix
   `"linked     2551"` and replaced it with the new number. After one successful run the
   line read `linked     2554`, no longer matched its own key, and **every later run was a
   no-op for that line - forever, with no error.** The same held for the other three. So
   the block froze at whatever each line happened to hold when its key last matched, and
   `linked` drifted to 2554 while the report said 2555. Here the key is the stable
   *prefix*, so the rewrite is idempotent and re-runnable for the life of the repo.

2. **Its replacement string truncated the line mid-sentence.** The `REL units` template
   ended at `"...This line used to add a"`, and because the key matched, it kept
   overwriting the line with that fragment. The committed state block carried a dangling
   sentence for a long time and `check_docs_claims.py` could not see it, because a
   presence test cannot see a sentence that stops in the middle. Here only the numbers are
   rewritten and the prose is carried across verbatim.

3. **It lived in /tmp**, so none of this was reviewable and none of it was versioned.

Usage:  python3 tools/sync_state_block.py [--check | --dedupe]

`--check` reports drift and exits non-zero without writing, so it can be a gate step.

`--dedupe` first drops repeated key lines inside the state block's fence - the first copy of
each stays, a later copy goes with its indented continuation lines - then rewrites as usual.
It exists for the goal loop's rebase (tools/union_docs_conflicts.sh): a union merge of two
lanes that both moved the block keeps both versions of every line. The four counted lines are
re-derived either way; for the others (`port link`) the first copy is the tip's.

The module-wiring sentence (`**N units of our own code in M modules** - `A`, `B`, ...`) is
re-derived too, from tools/check_module_wiring.py - the same source check_docs_claims.py tests
it against. A union merge of two lanes that each wired a module keeps both copies of that line,
each one short by the other's module; `--dedupe` keeps the first and the rewrite puts the true
count and list in it. Without this the loop carried a passing change onto the tip and then
rejected it on the docs gate (progress-rel-head-darktrooper, 2026-09-29).
"""
from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys

REPORT = pathlib.Path("build/report.json")
HANDOFF = pathlib.Path("docs/HANDOFF.md")

# The stable part of each line. Deliberately *not* the old value.
PREFIXES = ("matched    ", "linked     ", "DOL units  ", "REL units   ")


def counts(report: dict) -> dict[str, tuple[int, int]]:
    m = report["measures"]
    units = report["units"]
    dol_units = [u for u in units if u["name"].startswith("main/")]
    dol = sum(u["measures"].get("matched_functions", 0) for u in dol_units)
    dol_total = sum(u["measures"].get("total_functions", 0) for u in dol_units)
    linked = sum(
        u["measures"].get("matched_functions", 0)
        for u in units
        if u.get("metadata", {}).get("complete")
    )
    return {
        "matched    ": (m["matched_functions"], m["total_functions"]),
        "linked     ": (linked, m["total_functions"]),
        "DOL units  ": (dol, dol_total),
        # REL is total minus DOL: the 86 modules are the complement of main/*.
        "REL units   ": (m["matched_functions"] - dol, m["total_functions"] - dol_total),
    }


def rewrite(line: str, prefix: str, matched: int, total: int) -> str:
    """Replace the numbers, keep everything from the word `functions` onward."""
    tail = line[line.index("functions"):] if "functions" in line else ""
    if not tail:
        raise SystemExit(
            f"sync_state_block: {prefix!r} line has no `functions` to anchor on: {line!r}. "
            "Refusing to guess - fix the line by hand so the prose is explicit."
        )
    return f"{prefix}{matched} / {total} {tail}"


DEDUPE_KEYS = PREFIXES + ("port link  ",)


def dedupe(lines: list[str]) -> list[str]:
    """Keep the first copy of each key line in the state block's fence; see --dedupe above."""
    try:
        head = next(i for i, ln in enumerate(lines) if ln.startswith("## The state, measured"))
        start = next(i for i in range(head, len(lines)) if lines[i].startswith("```"))
        end = next(i for i in range(start + 1, len(lines)) if lines[i].startswith("```"))
    except StopIteration:
        raise SystemExit("sync_state_block: no fenced block under '## The state, measured'")
    kept, seen, skipping = [], set(), False
    for line in lines[start + 1:end]:
        key = next((k for k in DEDUPE_KEYS if line.startswith(k)), None)
        if key is None:
            if not (skipping and line.startswith(" ")):
                skipping = False
                kept.append(line)
        elif key in seen:
            skipping = True
        else:
            seen.add(key)
            skipping = False
            kept.append(line)
    return lines[:start + 1] + kept + lines[end:]


WIRING = re.compile(r"\*\*(\d+) units of our own code in (\d+) modules\*\* - `[^`]+`(?:, `[^`]+`)*")


def wiring() -> tuple[int, int, list[str]] | None:
    """What tools/check_module_wiring.py reports, or None if it cannot be read.

    The judged tree's own copy (cwd-relative, like REPORT and HANDOFF), because that is the one
    its check_docs_claims.py runs; the goal loop calls this script from master's tree, whose copy
    can be older and count differently.
    """
    tool = pathlib.Path("tools/check_module_wiring.py")
    text = subprocess.run([sys.executable, str(tool)], capture_output=True, text=True).stdout
    m = re.search(r"(\d+) unit\(s\) of our own code in (\d+) module\(s\): (.*)", text)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2)), m.group(3).strip().split(", ")


def sync_wiring(lines: list[str], dedupe_: bool, problems: list[str]) -> list[str]:
    """Rewrite the module-wiring sentence's counts and list; with dedupe_, keep only its first copy."""
    have = [i for i, ln in enumerate(lines) if WIRING.search(ln)]
    if not have:
        return lines
    got = wiring()
    if got is None:
        problems.append("could not read tools/check_module_wiring.py output")
        return lines
    units_n, mods_n, names = got
    new = f"**{units_n} units of our own code in {mods_n} modules** - " + ", ".join(f"`{n}`" for n in names)
    drop = set(have[1:]) if dedupe_ else set()
    return [WIRING.sub(lambda _m: new, ln) for i, ln in enumerate(lines) if i not in drop]


def main() -> int:
    check = "--check" in sys.argv
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    want = counts(report)
    lines = HANDOFF.read_text(encoding="utf-8").split("\n")
    if "--dedupe" in sys.argv and not check:
        lines = dedupe(lines)
    out, drift, problems = [], [], []
    before = lines
    lines = sync_wiring(lines, "--dedupe" in sys.argv and not check, problems)
    if lines != before:
        drift.append(("wiring", "module-wiring sentence", "re-derived from check_module_wiring.py"))

    for line in lines:
        hit = next((p for p in PREFIXES if line.startswith(p)), None)
        if hit is None:
            out.append(line)
            continue
        new = rewrite(line, hit, *want[hit])
        if new != line:
            drift.append((hit, line, new))
        out.append(new)

    for prefix in PREFIXES:
        n = sum(1 for ln in lines if ln.startswith(prefix))
        if n != 1:
            problems.append(
                f"the state block's {prefix.strip()!r} line appears {n} times; "
                "it must appear exactly once."
            )

    # The defect this file exists to prevent: a line that stops mid-sentence still
    # satisfies every presence test. Check the shape, not just the content.
    for line in out:
        for prefix in PREFIXES:
            if line.startswith(prefix) and not line.rstrip().endswith((")", ".", "%")):
                problems.append(
                    f"the {prefix.strip()!r} state-block line ends mid-sentence "
                    f"({line.rstrip()[-40:]!r}); a dangling fragment is how the REL "
                    "units line lost its prose unnoticed."
                )

    if problems:
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1

    if check:
        for prefix, old, new in drift:
            print(f"  stale: {old.strip()[:60]}\n      should be {new.strip()[:60]}")
        print(f"  state block {'OK' if not drift else f'{len(drift)} line(s) stale'}")
        return 1 if drift else 0

    HANDOFF.write_text("\n".join(out), encoding="utf-8")
    for prefix, old, new in drift:
        print(f"  {prefix.strip()}: {old.split(' functions')[0].strip()}"
              f" -> {new.split(' functions')[0].strip()}")
    print(f"  state block rewritten from report.json: "
          + " ".join(f"{p.strip()} {want[p][0]}" for p in PREFIXES))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
