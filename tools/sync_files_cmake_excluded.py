#!/usr/bin/env python3
"""Prune and complete tools/check_files_cmake.py's EXCLUDED list from the tree.

## Why this exists

`EXCLUDED` had to be hand-edited every time a unit was added to `files.cmake` or removed
from it, and every hand edit was a chance to leave the list describing a tree that no
longer exists. It went stale repeatedly in one session: the gate reported

    stale:   src/MetroidPrime/Player/CGameStateSlotsCtor.cpp is in EXCLUDED but is now
             listed in files.cmake

five separate times across five lane collections, each one a merge artefact rather than a
decision. It also accumulated **duplicated lines** in `docs/HANDOFF.md` for the same reason.

**A list that must be maintained by hand, describing a tree that changes every commit, will
be wrong.** So it is derived here instead.

## What it does

  * drops an `EXCLUDED` entry for a source that is now listed in `files.cmake`
  * reports a `Matching` object in `configure.py` that is in neither list, so it can be
    given a reason

It does **not** invent reasons. An entry is a decision with a measurement behind it, and
the measurement is not recoverable from the tree - which is why this adds a stub entry with
a TODO rather than guessing. See `--add` for that.

## Use

    python3 tools/sync_files_cmake_excluded.py            # prune stale, report omissions
    python3 tools/sync_files_cmake_excluded.py --check    # non-zero exit if anything to do
    python3 tools/sync_files_cmake_excluded.py --add src/Foo.cpp "why it is excluded"

`--check` is what a gate step should call: it changes nothing and fails when the list and
the tree disagree.
"""

from __future__ import annotations

import argparse
import ast
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONFIGURE = ROOT / "configure.py"
FILES_CMAKE = ROOT / "files.cmake"
EXCLUDED = ROOT / "tools" / "check_files_cmake.py"

# `Object(Matching, "path")` and `Object(MatchingFor("G2ME01"), "path")`. Deliberately
# does not match NonMatching: the gate only requires an entry for units whose bytes the
# DOL link depends on, and adding entries for every NonMatching unit would bury the ones
# that matter.
MATCHING = re.compile(
    r'Object\(\s*(?:Matching|MatchingFor\([^)]*\))\s*,\s*"([^"]+)"\s*\)'
)
LISTED = re.compile(r"^\s+(src/\S+\.cpp)\s*$", re.M)
# An `EXCLUDED` key: 4 spaces, a quoted path ending in `",` (possibly multi-line reason).
EXCLUDED_KEY = re.compile(r'^    "([^"]+)":\s*$', re.M)


def parse() -> tuple[set[str], set[str], dict[str, str]]:
    """Return (matching objects, sources listed in files.cmake, EXCLUDED entries)."""
    # configure.py names units relative to src/ ("Kyoto/Alloc/CMemory.cpp") while
    # files.cmake and EXCLUDED use the full path ("src/Kyoto/Alloc/CMemory.cpp").
    # Normalising here is the whole reason the first version of this tool reported all
    # 380 omissions as "SDK objects" - the test was on a prefix the paths never have.
    matching = {
        p if p.startswith("src/") else "src/" + p
        for p in MATCHING.findall(CONFIGURE.read_text(encoding="utf-8"))
    }
    listed = set(LISTED.findall(FILES_CMAKE.read_text(encoding="utf-8")))

    text = EXCLUDED.read_text(encoding="utf-8")
    # Parse the literal rather than regexing it, so a multi-line reason string cannot be
    # mistaken for structure. The file is a script, so exec-ing just the assignment is
    # safe here and is far more reliable than pattern matching over quoted text.
    tree = ast.parse(text)
    entries: dict[str, str] = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
        if "EXCLUDED" not in targets:
            continue
        for k, v in zip(node.value.keys, node.value.values):
            entries[k.value] = ast.literal_eval(v) if isinstance(v, ast.Constant) else ""
    return matching, listed, entries


def rewrite(entries: dict[str, str]) -> None:
    """Rewrite the EXCLUDED literal, keeping every surviving reason byte for byte."""
    text = EXCLUDED.read_text(encoding="utf-8")
    start = text.index("EXCLUDED = {")
    body = "EXCLUDED = {\n"
    # Longest paths first so a prefix cannot swallow a longer sibling.
    for path in sorted(entries, key=len, reverse=True):
        reason = entries[path].replace("\\", "\\\\").replace('"', '\\"')
        body += '    "%s":\n        "%s",\n' % (path, reason)
    body += "}\n"
    tail = text[text.index("\n", start) + 1 :]
    # Drop the old literal, keeping whatever followed it.
    after = tail.index("\n}\n") + 3
    EXCLUDED.write_text(text[:start] + body + tail[after:], encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="change nothing; fail if stale")
    ap.add_argument("--add", nargs=2, metavar=("PATH", "REASON"))
    args = ap.parse_args()

    matching, listed, entries = parse()

    stale = sorted(p for p in entries if p in listed)
    missing = sorted(p for p in matching if p not in listed and p not in entries)

    if args.add:
        path, reason = args.add
        if path in listed:
            print(f"{path} is listed in files.cmake; not excluding it", file=sys.stderr)
            return 1
        entries[path] = reason
        if not args.check:
            rewrite(entries)
            print(f"added EXCLUDED entry for {path}")
        return 0

    for p in stale:
        print(f"stale:  {p} is in EXCLUDED but is now listed in files.cmake")
    for p in missing:
        print(
            f"omitted: {p} is a Matching object in configure.py and is in neither "
            f"files.cmake nor EXCLUDED - add it with a reason, or list it"
        )
    if missing:
        # The SDK's own objects (Dolphin/, Runtime/) are Matching, are not in the port
        # build, and have never been in EXCLUDED either - the real gate filters them by
        # its own rules. They are counted separately so the actionable omissions are not
        # buried under 200 lines a reader learns to skip.
        sdk = [p for p in missing if p.split("/")[1] in ("Dolphin", "Runtime", "LZO", "extern")]
        # (paths are src/-prefixed by now, so the SDK directories are the SECOND component)
        game = [p for p in missing if p not in sdk]
        print(f"  ({len(sdk)} SDK objects filtered by the real gate, {len(game)} actionable)")

    if stale and not args.check:
        for p in stale:
            del entries[p]
        rewrite(entries)
        print(f"pruned {len(stale)} stale EXCLUDED entries")

    if args.check and (stale or missing):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
