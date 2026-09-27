#!/usr/bin/env python3
"""boot_progress.py - how far did the boot get, and is that further than the branch head?

  boot_progress.py record  <run.log> <out.json>   the head's position (run_goal.sh, before the agent)
  boot_progress.py verify  <run.log> <base.json>  exit 0 only if every run beats every head run
  boot_progress.py blocker <base.json>            one JSON line: a queue item for where the head dies

The log comes from boot_probe.sh with MP_PROBE_RUNNER=boot_gdb_run.sh: each run leaves the main
thread's stack at the point the boot stopped (a fault or a hang), outermost frame first here.
Only frames in this repo's sources count.

Ordering, in this order:
  1. markers - the "boot: step ..." and "Initializing renderer..." lines. Losing one the head
     printed is a regression; printing one it did not is progress. The diff may not touch them.
  2. the stacks, from main inwards: at the first frame where they differ, the same function at a
     later line is progress. The head's lines are first mapped through `git diff HEAD`, so lines
     the agent inserted above the crash do not count as movement. A hang is sampled several times
     and a run is further only if every one of its samples beats every head sample.
Everything else is undecidable, and undecidable fails: two different calls from one line, one
stack a prefix of the other, a clean exit, a death inside lines the agent rewrote.
"""
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path.cwd().resolve()
MARKER = re.compile(r"^(boot: step .*|Initializing renderer\.\.\.)\s*$")
# "set print frame-arguments none" prints "(this=..., len=...)"; argument values never hold parens.
FRAME = re.compile(r"^#(\d+)\s+(?:0x[0-9a-f]+ in )?(.+?) \([^()]*\)(?: at (.+):(\d+)| from (.+))?\s*$")
SIGNAL = re.compile(r"received signal (SIG\w+)")


def parse(text: str) -> list[dict]:
    runs, cur, in_bt = [], None, False
    for line in text.splitlines():
        if line.startswith("[boot-progress] run ") and line.endswith(" begin"):
            cur = {"markers": [], "samples": [], "signal": "", "last": "", "exited": False, "hang": False}
            in_bt = False
            continue
        if cur is None:
            continue
        m = re.match(r"^\[boot-progress\] run \d+ end \(hang=(\d)\)", line)
        if m:
            cur["hang"] = cur["hang"] or m.group(1) == "1"
            runs.append(finish(cur))
            cur = None
            continue
        if line == "[boot-progress] main-thread-bt-begin":
            in_bt = True
            cur["samples"].append((cur["last"], []))
            continue
        if line == "[boot-progress] main-thread-bt-end":
            in_bt = False
            continue
        if in_bt:
            f = FRAME.match(line)
            if f and f.group(3):
                p = (ROOT / f.group(3)).resolve()  # an absolute path stays as it is
                try:
                    rel = str(p.relative_to(ROOT))
                except ValueError:
                    continue  # a system header, glibc's "../sysdeps/...", another tree
                if rel.startswith("build") or not p.is_file():
                    continue
                cur["samples"][-1][1].append({"func": f.group(2), "file": rel, "line": int(f.group(4))})
            continue
        if MARKER.match(line) and line.strip() not in cur["markers"]:
            cur["markers"].append(line.strip())
        s = SIGNAL.search(line)
        if s:
            cur["last"] = s.group(1)
            cur["signal"] = cur["signal"] or s.group(1)
        if re.search(r"\[Inferior \d+ \(process \d+\) exited", line):
            cur["exited"] = True
    return runs


def finish(run: dict) -> dict:
    tagged = [(sig, list(reversed(f))) for sig, f in run.pop("samples") if f]  # gdb: innermost first
    del run["last"]
    if tagged and tagged[0][0] not in ("", "SIGINT"):
        run["kind"] = "crash"
        samples = [tagged[0][1]]  # the fault itself; later stops are inside the port's signal handler
    else:
        run["kind"] = ("hang" if run["hang"] or run["signal"] == "SIGINT"
                       else "exit" if run["exited"] else "unknown")
        # A hang's samples, up to any fault between them: stops after that are in the handler.
        samples = []
        for sig, f in tagged:
            if sig != "SIGINT":
                break
            samples.append(f)
    # The position: the frames every sample shares, from main inwards. For a crash that is the
    # whole stack; for a hang it drops the frames that only say where in the loop a sample landed.
    frames = samples[0] if samples else []
    for s in samples[1:]:
        n = 0
        while n < min(len(frames), len(s)) and frames[n] == s[n]:
            n += 1
        frames = frames[:n]
    run["frames"] = frames  # what the description and the blocker item name
    run["samples"] = len(samples)
    run["stacks"] = samples  # what compare() orders: every sample, not just the shared frames
    return run


def hunks(file: str) -> list[tuple[int, int, int, int]]:
    out = subprocess.run(["git", "diff", "-U0", "HEAD", "--", file], capture_output=True, text=True,
                         cwd=ROOT).stdout
    hs = []
    for m in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", out, re.M):
        a, b, c, d = int(m[1]), int(m[2] or 1), int(m[3]), int(m[4] or 1)
        hs.append((a, b, c, d))
    return hs


def map_line(file: str, line: int, cache: dict) -> tuple[int, tuple[int, int] | None]:
    """The head's line in the working tree's coordinates, and the rewritten range if it was edited."""
    if file not in cache:
        cache[file] = hunks(file)
    off = 0
    for a, b, c, d in cache[file]:
        if b == 0:  # pure insertion after old line a
            if line > a:
                off += d
            continue
        if line >= a + b:
            off += d - b
        elif line >= a:
            return c, (c, c + d)
        else:
            break
    return line + off, None


def mapped(run: dict, cache: dict) -> dict:
    def one(stack: list[dict]) -> list[dict]:
        out = []
        for f in stack:
            line, span = map_line(f["file"], f["line"], cache)
            out.append(dict(f, line=line, span=span))
        return out
    return dict(run, stacks=[one(s) for s in run["stacks"]])


def where(f: dict) -> str:
    return f"{f['func']} ({f['file']}:{f['line']})"


def order(x_stack: list[dict], y_stack: list[dict]) -> tuple[int | None, str]:
    """One head sample against one candidate sample, from main inwards."""
    for i, (x, y) in enumerate(zip(x_stack, y_stack)):
        if (x["func"], x["file"]) != (y["func"], y["file"]):
            return None, f"the stacks part at depth {i}: head in {where(x)}, now in {where(y)} - cannot order"
        if x.get("span"):
            lo, hi = x["span"]
            if lo <= y["line"] < hi:
                return None, f"the boot now stops inside lines the change rewrote ({where(y)}) - cannot order"
            return (1 if y["line"] >= hi else -1), f"{x['func']}: head stopped in rewritten lines {lo}-{hi - 1}, now line {y['line']}"
        if x["line"] != y["line"]:
            v = 1 if y["line"] > x["line"] else -1
            return v, f"{x['func']}: head at line {x['line']}, now line {y['line']}"
    if len(x_stack) == len(y_stack):
        return 0, f"the same place: {where(y_stack[-1])}"
    return None, "the same call site, one stack deeper than the other - cannot order"


def compare(base: dict, cand: dict) -> tuple[int | None, str]:
    """+1 cand is further, 0 the same place, -1 behind, None undecidable.

    Without new markers, every candidate sample has to beat every head sample. A hang's samples
    spread over the loop it is stuck in, so a candidate still in that loop lands among them and
    fails; comparing one sample each would pass or fail it by where the interrupt happened to land.
    """
    if cand["kind"] in ("exit", "unknown") or not cand["stacks"]:
        return None, f"the run ended with no stack to place ({cand['kind']})"
    if not base["stacks"]:
        return None, f"the head run has no stack to compare against ({base['kind']})"
    bm, cm = set(base["markers"]), set(cand["markers"])
    if not bm <= cm:
        return -1, f"lost boot markers the head printed: {sorted(bm - cm)}"
    if cm > bm:
        return 1, f"new boot markers: {sorted(cm - bm)}"
    worst = None
    for x in base["stacks"]:
        for y in cand["stacks"]:
            v, why = order(x, y)
            rank = {-1: 0, None: 1, 0: 2, 1: 3}[v]
            if worst is None or rank < worst[0]:
                worst = (rank, v, why)
    n = f" ({len(base['stacks'])}x{len(cand['stacks'])} sample pairs, the worst shown)"
    return worst[1], worst[2] + n


def describe(run: dict) -> str:
    inner = [where(f) for f in reversed(run["frames"])][:6]
    how = {"crash": run["signal"],
           "hang": f"hang (the frames shared by {run['samples']} sample(s) of the main thread after the timeout)"
           }.get(run["kind"], run["kind"])
    last = run["markers"][-1] if run["markers"] else "none"
    return f"{how}; last marker: {last}; main thread, innermost first: " + (" <- ".join(inner) or "no frames")


def load_log(path: str) -> list[dict]:
    runs = parse(pathlib.Path(path).read_text(errors="replace"))
    if not runs:
        sys.exit(f"boot_progress: no [boot-progress] runs in {path} - was it run through boot_gdb_run.sh?")
    return runs


def head() -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()


def main() -> int:
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "record":
        runs = load_log(sys.argv[2])
        pathlib.Path(sys.argv[3]).write_text(json.dumps({"head": head(), "runs": runs}, indent=1))
        for r in runs:
            print(f"head run: {r['kind']}: {describe(r)}")
        return 0 if any(r["frames"] for r in runs) else 1
    if cmd == "blocker":
        base = json.loads(pathlib.Path(sys.argv[2]).read_text())
        runs = [r for r in base["runs"] if r["frames"]]
        if not runs:
            return 1
        r = runs[0]
        func = r["frames"][-1]["func"]
        # The head in the id: a later blocker in the same function is a new item, not a retry that
        # would inherit this one's fail count and notes.
        slug = re.sub(r"[^a-z0-9]+", "-", func.lower()).strip("-")[:40]
        print(json.dumps({"id": f"port-boot-{slug}-{base['head'][:7]}", "target": func,
                          "reason": f"The boot stops here at {base['head'][:7]}: {describe(r)}. Make the boot "
                                    "get further by making this code behave as retail does - not by skipping, "
                                    "stubbing or returning early from it. Judged by boot-progress.sh."}))
        return 0
    if cmd == "verify":
        base = json.loads(pathlib.Path(sys.argv[3]).read_text())
        if base.get("head") != head():
            print(f"verify: the boot baseline is for {base.get('head', '?')[:7]}, the branch head is {head()[:7]} - BOOT_PROGRESS FAIL")
            return 1
        cands = load_log(sys.argv[2])
        cache: dict = {}
        bases = [mapped(b, cache) for b in base["runs"]]
        ok = True
        for i, c in enumerate(cands, 1):
            print(f"run {i}: {c['kind']}: {describe(c)}")
            for j, b in enumerate(bases, 1):
                v, why = compare(b, c)
                word = {1: "further", 0: "no further", -1: "BEHIND", None: "undecidable"}[v]
                print(f"  vs head run {j}: {word} - {why}")
                ok = ok and v == 1
        if ok:
            print(f"BOOT_PROGRESS PASS: all {len(cands)} runs got further than all {len(bases)} head runs")
            return 0
        print("BOOT_PROGRESS FAIL: every run must get further than every head run")
        return 1
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
