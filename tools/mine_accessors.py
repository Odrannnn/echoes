#!/usr/bin/env python3
"""Find `single-load` / `single-store` accessors in the main DOL and group them into runs.

    python3 tools/mine_accessors.py [--asm /tmp/opencode/main.asm] [--out docs/research/accessor_runs.md]

A `single-load`/`single-store` accessor is a *function* (a real method, it has R_PPC_REL24
relocations) whose whole body is one or two register moves plus a single memory access against
`r3` at a small object-relative offset: `stw/lwz/stb/lbz/stfs/lfs/stwu/lhz/lha/lfd`. Retail keeps
these out of line, so each is one small `Matching` unit and a run of contiguous ones is one class's
accessor block.

The output is a table: address, size, the offsets it touches, and whether the body is a load or a
store. Nothing here decides a class; `tools/who_calls.py` does that from the `bl` sites.
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
# a memory access against a register, with the displacement as written
MEM = re.compile(r"^(stw|lwz|stb|lbz|stfs|lfs|stwu|lhz|lha|lfd|lfsu|stfiw|stmw|lwarx|ld|stwu|stb|extsb|extsh)\b.*?\b([a-z]\d+),(-?(?:0x)?[0-9a-f]+)\((\w+)\)")

LOADS = {"lwz", "lbz", "lfs", "lhz", "lha", "lfd", "lfsu", "extsb", "extsh", "lwarx"}
STORES = {"stw", "stb", "stfs", "stwu", "stfiw", "stmw"}


def parse(path):
    """[(addr, size, [(mnemonic, operands), ...])] in address order, one entry per function."""
    funcs = []
    cur = None
    for line in pathlib.Path(path).read_text(errors="replace").splitlines():
        m = INSN.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        mn = m.group(3)
        ops = m.group(4).strip()
        if cur is None:
            cur = {"start": addr, "insns": [(mn, ops)]}
        else:
            cur["insns"].append((mn, ops))
        if mn in ("blr", "bctr") and not ops.startswith("0x"):
            cur["end"] = addr + 4
            funcs.append(cur)
            cur = None
    return funcs


def classify(f):
    """(kind, [(offset, width)], reg) or None. `kind` in {load, store, mixed}."""
    body = f["insns"]
    if len(body) > 12:
        return None
    # a real method: no branches, no calls
    for mn, ops in body:
        if mn.startswith("b") and mn not in ("blr", "bctr"):
            return None
        if mn in ("bl", "bctrl", "bnel", "beq", "bne", "bc", "bdnz", "bdi"):
            return None
    # A *member* accessor uses r3 as it arrived. A function that rebuilds r3 with `lis`/`addi`/
    # `ori` is reading a **global** (a small-data or absolute address), not `this` - the AI* DSP
    # entry points at 0x80381780 are three `lhz/sth` against a fixed address and look exactly like
    # an accessor run until you notice the `lis`. Excluding them is not cosmetic: they would
    # otherwise be attributed to whichever class happened to call them.
    for mn, ops in body:
        if mn in ("lis", "la", "addis") and re.search(r"\br3,", ops + " "):
            return None
        if mn in ("addi", "ori", "addic") and re.match(r"^r3,", ops):
            return None

    touches = []
    regs = set()
    for mn, ops in body:
        m = MEM.match(mn + " " + ops)
        if not m:
            continue
        reg, disp, base = m.group(2), m.group(3), m.group(4)
        if base != "r3":
            continue
        try:
            off = int(disp, 16) if disp.startswith(("0x", "-0x")) else int(disp)
        except ValueError:
            continue
        touches.append((off, mn))
        regs.add(reg)
    if not touches:
        return None
    kinds = set()
    for _off, mn in touches:
        if mn in LOADS:
            kinds.add("load")
        elif mn in STORES:
            kinds.add("store")
    if not kinds:
        return None
    kind = kinds.pop() if len(kinds) == 1 else "mixed"
    return kind, [o for o, _ in touches], touches, regs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--out", default="")
    ap.add_argument("--max-run", type=int, default=3,
                    help="only report runs of at least this many adjacent accessors")
    a = ap.parse_args()

    funcs = parse(a.asm)
    cands = []
    for f in funcs:
        c = classify(f)
        if c is None:
            continue
        kind, offs, touches, regs = c
        if kind == "mixed":
            continue
        # reject anything that is not "one memory access, maybe plus a store of the loaded value"
        if len(touches) > 2:
            continue
        cands.append({"addr": f["start"], "size": f["end"] - f["start"], "kind": kind,
                      "offs": offs, "regs": regs, "insns": f["insns"]})

    # group into runs: adjacent addresses (a run has no gap at all)
    cands.sort(key=lambda c: c["addr"])
    runs = []
    for c in cands:
        prev = runs[-1][-1] if runs else None
        if prev is not None and prev["addr"] + prev["size"] == c["addr"]:
            runs[-1].append(c)
        else:
            runs.append([c])

    big = [r for r in runs if len(r) >= a.max_run]
    lines = ["# `single-load` / `single-store` accessor runs in the main DOL", "",
             f"Detected by `tools/mine_accessors.py` from `objdump -d build/G2ME01/main.elf`.",
             f"**{len(cands)} accessor functions** in **{len(runs)} runs**; "
             f"**{sum(len(r) for r in big)} functions** in the **{len(big)} runs of >= "
             f"{a.max_run}**.", "",
             "| run | retail range | bytes | n | kind | offsets |", "|---|---|---|---|---|---|"]
    for i, r in enumerate(big):
        lo, hi = r[0]["addr"], r[-1]["addr"] + r[-1]["size"]
        offs = sorted({o for c in r for o in c["offs"]})
        kinds = ",".join(sorted({c["kind"] for c in r}))
        lines.append(f"| {i} | `0x{lo:08X}..0x{hi:08X}` | {hi - lo} | {len(r)} | {kinds} | "
                     + " ".join(f"+{o}" for o in offs) + " |")
    if a.out:
        pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    print(f"\n{len(cands)} accessor functions in {len(runs)} runs; {sum(len(r) for r in big)} "
          f"in {len(big)} runs of >= {a.max_run}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
