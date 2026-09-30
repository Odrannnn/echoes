#!/usr/bin/env python3
"""The goal queue. All state is on disk, so nothing depends on a chat session.

  build/goal/queue.json          the items still to do
  build/goal/review-queue.json   items that failed MAX_FAILS (2) times - set aside, the loop continues
  build/goal/baseline.json       matched/linked at the last accepted commit

Item fields: id, kind, target, reason, fails, deps, verify
  kind    'port'  define a missing symbol or fill an empty body on the boot path
          'match' take a unit to Matching
          'progress' raise a unit's matched_functions without flipping it (a unit too big for one item)
  deps    ids that must be done first
  verify  optional: a script under tools/goal_verify/ that goal_check.sh runs as the port
          item's acceptance test. Set by the orchestrator, never by an agent - it is how a
          behavioural port item (a wrong body, not a missing symbol) becomes judgeable at all.
  why     set on an item moved to review by `review`: the reason it was set aside

`next` returns the first item whose deps are all done, which is why the file is
kept in insertion order: it is a hand-ordered queue, not a priority heap. `add --first` puts an
item at the front; run_goal.sh does that with the boot blocker it finds itself. `add --update` on an
item already queued replaces its reason (and with --first moves it to the front) instead of refusing,
keeping its fails and claim - how the orchestrator re-briefs and re-orders items under the lanes.

Lanes (several run_goal.sh at once, MP_GOAL_LANE): `next --lane L` also *claims* the item it
returns - `claim: {lane, at}` on the item - and skips items another lane has claimed, so two lanes
never work the same item. It also skips items whose `target` another lane's claim holds, so two
lanes never edit the same unit and discard each other's finished work on rebase. `done`, `fail` and `review` drop the claim with the attempt; `release`
drops it without counting a fail (the change could not be rebased onto the moved tip), and
`release-lane L` drops every claim of a lane that restarted. `has-next --lane L` exits 3 when
the only ready items are claimed by other lanes: wait, do not stop. Every command holds an
exclusive flock on queue.lock, so a read-modify-write can never interleave with another lane's.
Without --lane, claims are ignored: the single loop behaves exactly as before.

MP_GOAL_QUEUE_DIR names the directory holding the queue outright; the lanes set it, because each
lane's MP_GOAL_WT is its own worktree and the queue is shared.
"""
from __future__ import annotations

import argparse
import fcntl
import json
import time
import pathlib
import sys

import os
ROOT = pathlib.Path(os.environ.get("MP_GOAL_TREE") or
                        pathlib.Path(__file__).resolve().parent.parent)
WT = pathlib.Path(os.environ.get("MP_GOAL_WT") or (ROOT / "../wt-mp2-goal")).resolve()
GOAL = (pathlib.Path(os.environ["MP_GOAL_QUEUE_DIR"]).resolve() if os.environ.get("MP_GOAL_QUEUE_DIR")
        else (WT if (WT / "build/goal/queue.json").exists() else ROOT) / "build/goal")
QUEUE = GOAL / "queue.json"
REVIEW = GOAL / "review-queue.json"
# 2, not 3 (2026-09-28): the third attempt of a match item almost never passed - match-cfrustumplanes
# spent three straight hours timing out. The notes file carries what each attempt learned, and a
# set-aside item is re-queued by hand once something new is known.
MAX_FAILS = 2

KINDS = ("port", "match", "progress")


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
    if args.update and args.id in _ids(q):
        it = next(i for i in q if i["id"] == args.id)
        if args.reason:
            it["reason"] = args.reason
        if args.first:
            q.remove(it)
            q.insert(0, it)
        _save(QUEUE, q)
        print(f"goal_queue: updated {args.id}{' (moved first)' if args.first else ''}")
        return 0
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
    or it failed MAX_FAILS times and moved to review-queue. Treating only 'done' as settled deadlocks the
    loop: a dep that lands in review can never be done, so everything behind it would wait for
    ever and `has-next` would go false with work still queued. That is the failure this comment
    exists to prevent, and it is why review is a set-aside rather than a block.
    """
    pending = {i["id"] for i in items}
    return [i for i in items if all(d not in pending for d in i.get("deps", []))]


def _free(it: dict, lane: str | None, q: list | None = None) -> bool:
    """Not claimed by another lane, and its target unit not held by another lane's claim.
    Single-loop callers (lane None) ignore claims.

    The target check is what keeps lanes off each other's files: on 2026-09-30 16 of 51 items
    targeted MetroidPrime/main, five lanes edited main.cpp at once, and 12 finished attempts in
    three hours were thrown away as "does not apply" / "passed on X but fails on Y"."""
    if lane is None:
        return True
    c = it.get("claim")
    if c and str(c.get("lane")) != lane:
        return False
    t = it.get("target")
    return not t or not any(o is not it and o.get("target") == t and o.get("claim")
                            and str(o["claim"].get("lane")) != lane for o in (q or []))


def _takes(it: dict, args) -> bool:
    """Within this lane's fails band. The hard lane (--min-fails 1, a stronger model) takes only
    what the free model already failed; the free lanes (--max-fails 0) leave it the last attempt."""
    f = int(it.get("fails", 0))
    return (args.min_fails is None or f >= args.min_fails) and (args.max_fails is None or f <= args.max_fails)


def cmd_next(args) -> int:
    q = _load(QUEUE)
    ready = [i for i in _ready(q) if _free(i, args.lane, q) and _takes(i, args)]
    if not ready:
        return 1  # nothing ready
    if args.min_fails is not None:  # the hard lane: the most-failed item first (stable otherwise)
        ready.sort(key=lambda i: -int(i.get("fails", 0)))
    it = ready[0]
    if args.lane is not None:
        it["claim"] = {"lane": args.lane,
                       "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
        _save(QUEUE, q)
    print(json.dumps(it))
    return 0


def cmd_has_next(args) -> int:
    q = _load(QUEUE)
    ready = _ready(q)
    if any(_free(i, args.lane, q) and _takes(i, args) for i in ready):
        return 0
    return 3 if ready else 1  # 3: ready items exist, all claimed by other lanes or outside the band


def cmd_release(args) -> int:
    q = _load(QUEUE)
    for it in q:
        if it["id"] == args.id:
            it.pop("claim", None)
            _save(QUEUE, q)
            print(f"goal_queue: released {args.id}")
            return 0
    print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
    return 1


def cmd_release_lane(args) -> int:
    q = _load(QUEUE)
    freed = [it["id"] for it in q if str((it.get("claim") or {}).get("lane")) == args.lane]
    for it in q:
        if it["id"] in freed:
            it.pop("claim")
    if freed:
        _save(QUEUE, q)
    print(f"goal_queue: lane {args.lane} released {len(freed)} claim(s){': ' + ', '.join(freed) if freed else ''}")
    return 0


def cmd_done(args) -> int:
    q = _load(QUEUE)
    keep = [i for i in q if i["id"] != args.id]
    if len(keep) == len(q):
        print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
        return 1
    _save(QUEUE, keep)
    print(f"goal_queue: done {args.id} - {len(keep)} queued")
    return 0


def cmd_partial(args) -> int:
    """A committed partial result: the item is not done, but its attempt was not a failure.
    Its fails reset (the work is reachable) and it goes to the back, so other items get a turn."""
    q = _load(QUEUE)
    for i, it in enumerate(q):
        if it["id"] != args.id:
            continue
        q.pop(i)
        it.pop("claim", None)
        it["fails"] = 0
        it["partials"] = int(it.get("partials", 0)) + 1
        q.append(it)
        _save(QUEUE, q)
        print(f"goal_queue: {args.id} partial {it['partials']} - requeued at the back, {len(q)} queued")
        return 0
    print(f"goal_queue: {args.id} was not queued", file=sys.stderr)
    return 1


def cmd_fail(args) -> int:
    q = _load(QUEUE)
    for i, it in enumerate(q):
        if it["id"] != args.id:
            continue
        it["fails"] = int(it.get("fails", 0)) + 1
        it.pop("claim", None)
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
        it.pop("claim", None)
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
        lane = f" [lane {it['claim']['lane']}]" if it.get("claim") else ""
        print(f"  queue   {it['id']:34} {it['kind']:5} fails={it.get('fails',0)} "
              f"deps={deps}  {it['target']}{lane}")
    for it in r:
        why = f"  ({it['why']})" if it.get("why") else ""
        print(f"  review  {it['id']:34} {it['kind']:5} fails={it.get('fails',0)}  {it['target']}{why}")
    print(f"  {len(q)} queued, {len(r)} in review")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)

    for name, fn in (("has-next", cmd_has_next), ("next", cmd_next)):
        n = s.add_parser(name)
        n.add_argument("--lane", default=None, help="lane mode: claim / skip other lanes' claims")
        n.add_argument("--min-fails", type=int, default=None, help="only items failed at least N times")
        n.add_argument("--max-fails", type=int, default=None, help="only items failed at most N times")
        n.set_defaults(fn=fn)
    rl = s.add_parser("release")
    rl.add_argument("id")
    rl.set_defaults(fn=cmd_release)
    rla = s.add_parser("release-lane")
    rla.add_argument("lane")
    rla.set_defaults(fn=cmd_release_lane)
    s.add_parser("list").set_defaults(fn=cmd_list)

    a = s.add_parser("add")
    a.add_argument("id")
    a.add_argument("--kind", required=True, choices=KINDS)
    a.add_argument("--target", required=True)
    a.add_argument("--reason", default="")
    a.add_argument("--dep", dest="deps", action="append")
    a.add_argument("--verify", default="")
    a.add_argument("--first", action="store_true", help="put it at the front, not the back")
    a.add_argument("--update", action="store_true",
                   help="if already queued, replace its reason (and move it with --first)")
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

    p = s.add_parser("partial")
    p.add_argument("id")
    p.set_defaults(fn=cmd_partial)

    f = s.add_parser("fail")
    f.add_argument("id")
    f.set_defaults(fn=cmd_fail)

    args = ap.parse_args()
    GOAL.mkdir(parents=True, exist_ok=True)
    with open(GOAL / "queue.lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)  # released when the file closes
        return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
