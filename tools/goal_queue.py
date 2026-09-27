#!/usr/bin/env python3
"""The goal queue. All state is on disk, so nothing depends on a chat session.

  build/goal/queue.json          the items still to do
  build/goal/review-queue.json   items that failed 3 times - set aside, the loop continues
  build/goal/baseline.json       matched/linked at the last accepted commit

Item fields: id, kind, target, reason, fails, deps, verify
  kind    'port'  define a missing symbol or fill an empty body on the boot path
          'match' take a unit to Matching
  deps    ids that must be done first
  verify  optional: a script under tools/goal_verify/ that goal_check.sh runs as the port
          item's acceptance test. Set by the orchestrator, never by an agent - it is how a
          behavioural port item (a wrong body, not a missing symbol) becomes judgeable at all.
  why     set on an item moved to review by `review`: the reason it was set aside

`next` returns the first item whose deps are all done, which is why the file is
kept in insertion order: it is a hand-ordered queue, not a priority heap. `add --first` puts an
item at the front; run_goal.sh does that with the boot blocker it finds itself.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

import os
ROOT = pathlib.Path(os.environ.get("MP_GOAL_TREE") or
                        pathlib.Path(__file__).resolve().parent.parent)
WT = pathlib.Path(os.environ.get("MP_GOAL_WT") or (ROOT / "../wt-mp2-goal")).resolve()
GOAL = (WT if (WT / "build/goal/queue.json").exists() else ROOT) / "build/goal"
QUEUE = GOAL / "queue.json"
REVIEW = GOAL / "review-queue.json"
MAX_FAILS = 3

KINDS = ("port", "match")


def _load(p: pathlib.Path) -> list[dict]:
    if not p.exists():
        return []
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as e:
        die(f"{p.name} is unreadable ({e}); move it aside and re-add its items")
    return data if isinstance(data, list) else []


def _save(p: pathlib.Path, items: list[dict]) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(p.suffix + ".tmp")
    tmp.write_text(json.dumps(items, indent=2) + "\n", encoding="utf-8")
    tmp.replace(p)  # atomic: a killed loop must not leave half a queue


def die(msg: str) -> "NoReturn":  # type: ignore[valid-type]
    print(f"goal_queue: {msg}", file=sys.stderr)
    raise SystemExit(2)


def _ids(items: list[dict]) -> set[str]:
    return {i["id"] for i in items}


def cmd_add(args) -> int:
    q, r = _load(QUEUE), _load(REVIEW)
    if args.id in _ids(q) or args.id in _ids(r):
        print(f"goal_queue: {args.id} is already queued; not adding it twice")
        return 0
    if args.kind not in KINDS:
        die(f"kind must be one of {KINDS}, not {args.kind!r}")
    known = _ids(q) | _ids(r)
    for d in args.deps or []:
        if d not in known and d != args.id:
            die(f"dep {d!r} of {args.id} is neither queued nor done - add it first")
    item = {
        "id": args.id, "kind": args.kind, "target": args.target,
        "reason": args.reason, "fails": 0, "deps": list(args.deps or []),
    }
    if args.verify:
        item["verify"] = args.verify
    if args.first:
        q.insert(0, item)
    else:
        q.append(item)
    _save(QUEUE, q)
    print(f"goal_queue: added {args.id} ({args.kind}){' first' if args.first else ''} - {len(q)} queued")
    return 0


def _take(pred) -> dict | None:
    for it in _load(QUEUE):
        if pred(it):
            return it
    return None


def _ready(items: list[dict]) -> list[dict]:
    """Items whose deps are all settled.

    **A dep is settled when it is no longer in the queue** - because it was either done (removed)
    or it failed 3 times and moved to review-queue. Treating only 'done' as settled deadlocks the
    loop: a dep that lands in review can never be done, so everything behind it would wait for
    ever and `has-next` would go false with work still queued. That is the failure this comment
    exists to prevent, and it is why review is a set-aside rather than a block.
    """
    pending = {i["id"] for i in items}
    return [i for i in items if all(d not in pending for d in i.get("deps", []))]


def cmd_next(args) -> int:
    ready = _ready(_load(QUEUE))
    if not ready:
        return 1  # nothing ready
    print(json.dumps(ready[0]))
    return 0


def cmd_has_next(args) -> int:
    return 0 if _ready(_load(QUEUE)) else 1


def cmd_done(args) -> int:
    q = _load(QUEUE)
    keep = [i for i in q if i["id"] != args.id]
    if len(keep) == len(q):
        print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
        return 1
    _save(QUEUE, keep)
    print(f"goal_queue: done {args.id} - {len(keep)} queued")
    return 0


def cmd_fail(args) -> int:
    q = _load(QUEUE)
    for i, it in enumerate(q):
        if it["id"] != args.id:
            continue
        it["fails"] = int(it.get("fails", 0)) + 1
        if it["fails"] >= MAX_FAILS:
            q.pop(i)
            r = _load(REVIEW)
            r.append(it)
            _save(REVIEW, r)
            _save(QUEUE, q)
            print(f"goal_queue: {args.id} failed {it['fails']}x -> review-queue; "
                  f"{len(q)} queued, {len(r)} in review")
        else:
            _save(QUEUE, q)
            print(f"goal_queue: {args.id} fail {it['fails']}/{MAX_FAILS}")
        return 0
    print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
    return 1


def cmd_review(args) -> int:
    """Set an item aside without spending three agent runs on it - for an item the judge
    cannot decide, which no amount of agent work would change."""
    q = _load(QUEUE)
    for i, it in enumerate(q):
        if it["id"] != args.id:
            continue
        q.pop(i)
        it["why"] = args.why
        r = _load(REVIEW)
        r.append(it)
        _save(REVIEW, r)
        _save(QUEUE, q)
        print(f"goal_queue: {args.id} -> review-queue ({args.why}); {len(q)} queued, {len(r)} in review")
        return 0
    print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
    return 1


def cmd_set_verify(args) -> int:
    q = _load(QUEUE)
    for it in q:
        if it["id"] == args.id:
            it["verify"] = args.script
            _save(QUEUE, q)
            print(f"goal_queue: {args.id} verify = {args.script}")
            return 0
    print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
    return 1


def cmd_has_verify(args) -> int:
    """Exit 0 if an item queued or in review is judged by this verify script."""
    return 0 if any(i.get("verify") == args.script for i in _load(QUEUE) + _load(REVIEW)) else 1


def cmd_list(args) -> int:
    q, r = _load(QUEUE), _load(REVIEW)
    if not q and not r:
        print("goal_queue: empty")
        return 0
    for it in q:
        deps = ",".join(it.get("deps", [])) or "-"
        print(f"  queue   {it['id']:34} {it['kind']:5} fails={it.get('fails',0)} "
              f"deps={deps}  {it['target']}")
    for it in r:
        why = f"  ({it['why']})" if it.get("why") else ""
        print(f"  review  {it['id']:34} {it['kind']:5} fails={it.get('fails',0)}  {it['target']}{why}")
    print(f"  {len(q)} queued, {len(r)} in review")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)

    s.add_parser("has-next").set_defaults(fn=cmd_has_next)
    s.add_parser("next").set_defaults(fn=cmd_next)
    s.add_parser("list").set_defaults(fn=cmd_list)

    a = s.add_parser("add")
    a.add_argument("id")
    a.add_argument("--kind", required=True, choices=KINDS)
    a.add_argument("--target", required=True)
    a.add_argument("--reason", default="")
    a.add_argument("--dep", dest="deps", action="append")
    a.add_argument("--verify", default="")
    a.add_argument("--first", action="store_true", help="put it at the front, not the back")
    a.set_defaults(fn=cmd_add)

    hv = s.add_parser("has-verify")
    hv.add_argument("script")
    hv.set_defaults(fn=cmd_has_verify)

    v = s.add_parser("review")
    v.add_argument("id")
    v.add_argument("--why", required=True)
    v.set_defaults(fn=cmd_review)

    sv = s.add_parser("set-verify")
    sv.add_argument("id")
    sv.add_argument("script")
    sv.set_defaults(fn=cmd_set_verify)

    d = s.add_parser("done")
    d.add_argument("id")
    d.set_defaults(fn=cmd_done)

    f = s.add_parser("fail")
    f.add_argument("id")
    f.set_defaults(fn=cmd_fail)

    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
