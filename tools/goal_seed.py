#!/usr/bin/env python3
"""Refill the goal queue with high-yield items computed from build/report.json.

    python3 tools/goal_seed.py                 # dry run: print the add commands it would run
    python3 tools/goal_seed.py --max 20
    python3 tools/goal_seed.py --apply         # really add them (through goal_queue.py)

The loop eats queue items; when the queue runs dry nothing regenerates it by hand. This computes
candidates from measurements instead of memory, which matters because every wrong figure in this
repo's docs came from recall.

Three kinds of candidate, in this order:

  * **REL head** (`progress` items, `module:<Name>`) - a retail REL module that links *no* object of
    our own code. Its head (accessors/RELMain/RELExit) is the standard first step and the recipe
    that passes most often, so these go first. The module list is config.yml's `modules:` block;
    the modules that already have our code come from `tools/check_module_wiring.py`, which is the
    same check that has caught a dropped `Rel(...)` block four times.
  * **Prime 1 donor** (`progress` items, a DOL unit path) - a NonMatching unit whose counterpart
    in Metroid Prime 1's decomp (../prime-ref, or $MP_PRIME_REF) is Matching, with at least
    PRIME_MIN_FNS unmatched functions whose names appear in Prime 1's symbols.txt. The item names
    them, same-size ones first, and points at the Prime 1 source. Skipped when the clone is absent.
  * **match** (`match` items, a DOL unit path) - a `main/...` unit whose overall fuzzy is already
    high, with only 1-3 functions still below 100%. One named function between a unit and Matching
    is a small, checkable item; a unit with 40 functions below 100% is a project.

The **97% wall**: a unit whose *every* remaining function is at >=97% is not seeded. That residue is
register allocation and scheduling, not a mistake an agent can find - `match-cfrustumplanes`,
`match-cmetaanimsequence`, `match-dolphincaudiogroupset` and six others in the review queue are all
this, each having cost an agent run to prove. Likewise a unit whose functions are all at 100% but
whose flip still fails is link-level (data, gap padding, weak ordering) - those are in review too.

Nothing already queued (or set aside in review-queue) is re-proposed: the id *or* the target is
checked against both files, because the two spellings drift (`match-ccharlayoutinfo` and
`match-ccharlayoutinfo-object-layout` are one unit, two queue items).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GOAL_QUEUE = ROOT / "tools/goal_queue.py"
# The tree to measure; --root moves these to a lane worktree at the branch tip, which the main
# checkout lags by up to FF_EVERY commits (a module landed there would be seeded again).
WIRING = ROOT / "tools/check_module_wiring.py"
CONFIG_YML = ROOT / "config/G2ME01/config.yml"

# A unit already Matching (or with nothing left below 100%) is not work; objdiff marks a fully
# linked unit `metadata.complete`, and one whose own code fills the object `complete_code_percent`.
FUZZY_FLOOR = 90.0  # unit-level: below this a unit is a rewrite, not a fix
REGALLOC_WALL = 97.0  # function-level: >= this in every remaining function is regalloc/scheduling
MAX_LEFT = 3  # more functions below 100% than this is a project, not an item
# Metroid Prime 1's decomp (PrimeDecomp/prime), a read-only clone beside the repo. Echoes' engine
# is a fork of it; the two trial items (cactormodelparticles +14 functions, csortedlists 11 -> 19
# of 20, 2026-09-29) against ~2 for a typical progress pass are why these are seeded.
PRIME_REF = Path(os.environ.get("MP_PRIME_REF") or (ROOT / "../prime-ref")).resolve()
PRIME_MIN_FNS = 2  # fewer shared unmatched functions than this is not worth an agent run
PRIME_LIST_MAX = 12  # functions named in one item's reason; more makes the item a project


def die(msg: str) -> "NoReturn":  # type: ignore[valid-type]
    print(f"goal_seed: {msg}", file=sys.stderr)
    raise SystemExit(2)


def fuzzy(f: dict) -> float:
    """A function's fuzzy percent. Absent means objdiff could not measure it, which is 0."""
    return float(f.get("fuzzy_match_percent") or 0)


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        die(f"{path} is missing; run ./tools/decomp_build.sh to make build/report.json")
    except json.JSONDecodeError as e:
        die(f"{path} is unreadable ({e})")


def all_modules() -> list[str]:
    """Module names from config.yml's `modules:` block (config/G2ME01/config.yml)."""
    text = CONFIG_YML.read_text(encoding="utf-8")
    lines = text.splitlines()
    out, in_modules = [], False
    for line in lines:
        if not in_modules:
            if line.rstrip() == "modules:":
                in_modules = True
            continue
        if line.strip() and not line[0].isspace():  # next top-level key ends the block
            break
        m = re.search(r"object:\s*files/RelProd/(\S+?)\.rel\s*$", line)
        if m:
            out.append(m.group(1))
    if not out:
        die(f"no modules found in {CONFIG_YML}; its `modules:` block changed")
    return out


def own_code_modules() -> set[str]:
    """Modules that already link at least one unit of our own code.

    Parses check_module_wiring.py's own summary line (`N unit(s) of our own code in M module(s):
    A, B, C`) rather than re-deriving it: that tool knows about the `source="..."` argument and
    about the shared REL/REL_Setup.cpp object, and its answer is the one the gates use.
    """
    try:
        res = subprocess.run([sys.executable, str(WIRING)], capture_output=True, text=True, cwd=ROOT)
    except OSError as e:
        die(f"could not run {WIRING.name} ({e})")
    if res.returncode > 1:
        die(f"check_module_wiring.py failed: {res.stderr.strip() or res.returncode}")
    m = re.search(r"^\d+ unit\(s\) of our own code in \d+ module\(s\): *(.*)$", res.stdout, re.M)
    if not m:
        die(f"check_module_wiring.py printed no own-code summary; got:\n{res.stdout[-500:]}")
    return {s.strip() for s in m.group(1).split(",") if s.strip()}


def queued_keys(queue_dir: Path) -> tuple[set[str], set[str]]:
    ids, targets = set(), set()
    for name in ("queue.json", "review-queue.json"):
        p = queue_dir / name
        if not p.exists():
            continue
        try:
            items = json.loads(p.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as e:
            die(f"{p} is unreadable ({e}); move it aside and re-add its items")
        if not isinstance(items, list):
            continue
        for it in items:
            ids.add(str(it.get("id", "")))
            targets.add(str(it.get("target", "")))
    return ids, targets


def match_candidates(report: dict) -> list[dict]:
    """DOL units a single agent run has a real chance of taking to Matching."""
    out = []
    for u in report.get("units", []):
        name = u.get("name") or ""
        if not name.startswith("main/"):
            continue  # REL units need the module recipe, not a flip_test
        meta = u.get("metadata") or {}
        m = u.get("measures") or {}
        if meta.get("complete") or meta.get("auto_generated"):
            continue  # already Matching, or not our code (auto sections)
        if (m.get("complete_code_percent") or 0) >= 100:
            continue  # our object is fully linked - nothing to raise
        fns = u.get("functions") or []
        if not fns:
            continue  # no functions: data/bss, not a match item
        short = [f for f in fns if fuzzy(f) < 100]
        if not short or len(short) > MAX_LEFT:
            continue  # every function at 100% is a link-level problem; >3 is a project
        worst = min(short, key=lambda f: fuzzy(f))
        if fuzzy(worst) >= REGALLOC_WALL:
            continue  # the >=97% regalloc wall
        if (m.get("fuzzy_match_percent") or 0) < FUZZY_FLOOR:
            continue  # a rewrite, not a fix
        unit = name.split("/", 1)[1]
        out.append({
            "id": f"match-{Path(unit).name.lower()}",
            "kind": "match",
            "target": unit,
            "reason": f"{len(short)} function(s) left, worst {worst.get('name')} at "
                      f"{fuzzy(worst):.2f}%; seeded by goal_seed.py",
            "sort": (len(short), -(m.get("fuzzy_match_percent") or 0)),
        })
    out.sort(key=lambda c: c["sort"])
    return out


def prime1_candidates(report: dict) -> list[dict]:
    """DOL units whose Prime 1 counterpart is Matching and shares unmatched functions by name."""
    cfg, syms = PRIME_REF / "configure.py", PRIME_REF / "config/GM8E01_00/symbols.txt"
    if not cfg.exists() or not syms.exists():
        return []
    matched = {}  # Prime 1 unit path -> matched (Matching/Equivalent), keyed also by basename
    for m in re.finditer(r'Object\(\s*(\w+)(?:\([^)]*\))?\s*,\s*"([^"]+)"', cfg.read_text()):
        if m[1].startswith(("Matching", "Equivalent")):
            matched[m[2]] = m[2]
            matched.setdefault(Path(m[2]).name, m[2])
    sizes = {}
    for m in re.finditer(r"^(\S+) = \.text:0x[0-9A-Fa-f]+; // type:function size:0x([0-9A-Fa-f]+)",
                         syms.read_text(), re.M):
        sizes[m[1]] = int(m[2], 16)
    out = []
    for u in report.get("units", []):
        name = u.get("name") or ""
        meta = u.get("metadata") or {}
        if not name.startswith("main/") or meta.get("complete") or meta.get("auto_generated"):
            continue
        src = (meta.get("source_path") or "").removeprefix("src/")
        donor = matched.get(src) or matched.get(Path(src).name)
        if not src or not donor or not (PRIME_REF / "src" / donor).exists():
            continue
        shared = [(int(f.get("size") or 0) == sizes[f["name"]], f["name"])
                  for f in u.get("functions") or []
                  if fuzzy(f) < 100 and f.get("name") in sizes]
        if len(shared) < PRIME_MIN_FNS:
            continue
        shared.sort(key=lambda t: not t[0])  # same size first: the likeliest to port unchanged
        same = sum(1 for t in shared if t[0])
        listed = ", ".join(n for _, n in shared[:PRIME_LIST_MAX])
        more = f" (and {len(shared) - PRIME_LIST_MAX} more)" if len(shared) > PRIME_LIST_MAX else ""
        unit = name.split("/", 1)[1]
        out.append({
            "id": f"progress-prime1-{Path(unit).name.lower()}",
            "kind": "progress",
            "target": unit,
            "reason": "progress item: raise the unit's matched_functions; it stays NonMatching, do "
                      "not run flip_test to decide. Re-measure first. Metroid Prime 1's decomp "
                      f"(read-only clone at {PRIME_REF}) has this unit Matching in "
                      f"{PRIME_REF}/src/{donor}; Echoes' engine is a fork of it. For each function "
                      "below, read Prime 1's implementation, adapt it to this repo's own headers "
                      "and member names (do NOT copy Prime 1 headers or change class layouts to "
                      "Prime 1's - fix only what the measured diff shows), build and measure. "
                      "Prime 1 is GC/1.3.2 and Echoes GC/2.7, so identical source may schedule "
                      "differently; tune as usual. In your notes record, per function, before%, "
                      "after% and whether Prime 1's source matched unchanged, needed small edits "
                      f"or did not help. {len(shared)} unmatched function(s) share a Prime 1 name, "
                      f"{same} with the same size (listed first): {listed}{more}. "
                      "Seeded by goal_seed.py",
            "sort": (-same, -len(shared)),
        })
    out.sort(key=lambda c: c["sort"])
    return out


def rel_head_candidates(queue_dir: Path) -> list[dict]:
    """Retail REL modules with no object of our own code linked yet."""
    mods = all_modules()
    own = own_code_modules()
    out = []
    for mod in mods:
        if mod in own:
            continue
        out.append({
            "id": f"progress-rel-head-{mod.lower()}",
            "kind": "progress",
            "target": f"module:{mod}",
            "reason": "REL module with no own-code unit yet: decompile its head "
                      "(accessors/RELMain/RELExit) as a Matching unit; see "
                      "docs/RUNNING_THE_DECOMP.md module recipe; seeded by goal_seed.py",
            "sort": (0, 0),
        })
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=None,
                    help="tree to measure (default: this checkout); run_goal.sh passes the lane worktree")
    ap.add_argument("--report", default="build/report.json",
                    help="objdiff report to read, relative to --root (default: build/report.json)")
    ap.add_argument("--queue-dir", default=None,
                    help="queue directory (default: $MP_GOAL_QUEUE_DIR, else ../wt-mp2-goal/build/goal)")
    ap.add_argument("--max", type=int, default=10, help="most items to seed in total (default: 10)")
    ap.add_argument("--only", choices=("rel-head", "prime1", "match"), default=None,
                    help="seed one kind of candidate only")
    ap.add_argument("--prime1-min-same", type=int, default=0,
                    help="Prime 1 items need at least this many same-size shared functions")
    ap.add_argument("--apply", action="store_true",
                    help="add the items through goal_queue.py; default is a dry run")
    args = ap.parse_args()

    global ROOT, WIRING, CONFIG_YML
    if args.root:
        ROOT = Path(args.root).resolve()
        WIRING = ROOT / "tools/check_module_wiring.py"
        CONFIG_YML = ROOT / "config/G2ME01/config.yml"

    report_path = Path(args.report)
    if not report_path.is_absolute():
        report_path = ROOT / args.report
    report = load_json(report_path)

    if args.queue_dir:
        queue_dir = Path(args.queue_dir)
    elif os.environ.get("MP_GOAL_QUEUE_DIR"):
        queue_dir = Path(os.environ["MP_GOAL_QUEUE_DIR"])
    else:
        wt = Path(os.environ.get("MP_GOAL_WT") or (ROOT / "../wt-mp2-goal"))
        queue_dir = wt / "build/goal"
    queue_dir = queue_dir.resolve()

    have_ids, have_targets = queued_keys(queue_dir)

    cap = max(0, args.max)
    kinds = {"rel-head": lambda: rel_head_candidates(queue_dir),
             "prime1": lambda: [c for c in prime1_candidates(report)
                                if -c["sort"][0] >= args.prime1_min_same],
             "match": lambda: match_candidates(report)}
    cands = [c for k, f in kinds.items() if args.only in (None, k) for c in f()]
    seen_ids, picked = set(), []
    for c in cands:
        if len(picked) >= cap:
            break
        if c["id"] in have_ids or c["target"] in have_targets or c["id"] in seen_ids:
            continue
        seen_ids.add(c["id"])
        picked.append(c)

    if not picked:
        print("goal_seed: nothing to seed")
        return 0

    env = dict(os.environ, MP_GOAL_QUEUE_DIR=str(queue_dir))
    for c in picked:
        cmd = [sys.executable, str(GOAL_QUEUE), "add", c["id"],
               "--kind", c["kind"], "--target", c["target"], "--reason", c["reason"]]
        if args.apply:
            res = subprocess.run(cmd, env=env, cwd=ROOT)
            if res.returncode != 0:
                die(f"goal_queue.py add {c['id']} failed with {res.returncode}")
        else:
            print(" ".join(cmd[1:]))
            print(f"    # {c['reason']}")

    if not args.apply:
        print(f"\ngoal_seed: {len(picked)} candidate(s) for {queue_dir} "
              f"(dry run; pass --apply to add them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
