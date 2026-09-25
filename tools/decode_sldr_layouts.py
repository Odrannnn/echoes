#!/usr/bin/env python3
"""Decode the retail layout of every SLdr* struct from the Tweaks module's own code.

    tools/decode_sldr_layouts.py                 # human-readable
    tools/decode_sldr_layouts.py --json          # machine-readable
    tools/decode_sldr_layouts.py --only SLdrTweakAutoMapper

Why this exists
---------------
`include/MetroidPrime/ScriptLoader/SLdr*.hpp` are **generated** and they size their
members wrong, which is why `sizeof(CTweakContents)` is 0x37D0 in this tree against
retail's 0x31F4 (`docs/research/tweak_globals.md`).  Nothing in the tree records the
retail layout, and the three places it is recoverable all are here:

  * `__ct__<T>Fv`  - stores or constructs a value at a fixed offset from `this`,
                     once per member, in declaration order.  Gives **every** member's
                     offset, including members no property ever writes.
  * `__dt__<T>Fv`  - destroys a member at a fixed offset; the callee's name is the
                     member's type.  Gives the class-typed members exactly.
  * `LoadTypedef<T>(T&, CInputStream&)` - the counted-property loop.  MWCC compiles the
                     switch to a binary search, so every property tag is a `lis`/`addi`
                     pair feeding `cmpw` + `beq <block>`, and each block names the member
                     it writes.  Gives the tag -> member mapping.

Offsets are read out of the instructions; nothing is taken from a header.  The output
is the ground truth a header fix has to reproduce.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

OBJDUMP = "build/binutils/powerpc-eabi-objdump"
NM = "build/binutils/powerpc-eabi-nm"

REGS = {f"r{i}": i for i in range(32)}
REGS.update({f"f{i}": i for i in range(32)})
REGS.update({"sp": 1, "rtoc": 2})

BRANCH = ("b", "bdnz", "bdz", "beq", "bne", "blt", "bgt", "ble", "bge", "bns", "bso")


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


def reg(tok):
    tok = tok.strip()
    return REGS.get(tok)


def mem(ops, i):
    """`8(r5)` -> (5, 8); `-8(r5)` -> (5, -8); `8(r13)` -> (13, 8)."""
    m = re.match(r"^(-?(?:0x)?[0-9a-f]*)\((r\d+|r13|sp|rtoc)\)$", ops[i].strip(), re.I)
    if not m:
        return None, None
    txt = m.group(1) or "0"
    # objdump prints displacements in decimal
    return reg(m.group(2)), int(txt, 16 if txt.lower().startswith("0x") else 10)


def symbol_table(module):
    out = {}
    for path in sorted(glob.glob("build/G2ME01/%s/obj/**/*.o" % module, recursive=True)):
        txt = subprocess.run([NM, "--defined-only", path], capture_output=True,
                             text=True).stdout
        for line in txt.splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "TtWw":
                out.setdefault(p[2], path)
    return out


def disasm(path, sym):
    """Parse one function's disassembly.

    MWCC's objdump separates operands with a bare `,` and no space, so operands are
    split on commas and branch targets off whitespace.
    """
    out = subprocess.run([OBJDUMP, "-d", "-r", "--disassemble=%s" % sym, path],
                         capture_output=True, text=True).stdout
    insns, started, last = [], False, None
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            started = (m.group(2) == sym)
            last = None
            continue
        if not started:
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(\S+)\s*(.*)$", line)
        if m:
            text = m.group(4).strip()
            last = {"addr": int(m.group(1), 16), "mn": m.group(3),
                    "ops": [o.strip() for o in text.split(",")],
                    "text": text, "reloc": None}
            insns.append(last)
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+R_PPC_\S+\s+(\S+)\s*$", line)
        if m and last is not None:
            last["reloc"] = m.group(2)
    return insns


def target(ins):
    """First whitespace token of a branch operand."""
    return ins["text"].split()[0] if ins["text"] else None


def tagged_consts(insns):
    """Ordered [(addr, reg, value)] for every `lis`+`addi` materialised constant."""
    val, lis, out = {}, {}, []
    for ins in insns:
        mn, ops = ins["mn"], ins["ops"]
        if mn == "lis" and len(ops) == 2 and reg(ops[0]) is not None:
            lis[reg(ops[0])] = (s16(int(ops[1], 0)) << 16) & 0xFFFFFFFF
            val[reg(ops[0])] = lis[reg(ops[0])]
        elif mn == "addi" and len(ops) == 3 and reg(ops[1]) in val and reg(ops[0]) is not None:
            v = (val[reg(ops[1])] + s16(int(ops[2], 0))) & 0xFFFFFFFF
            val[reg(ops[0])] = v
            if reg(ops[1]) in lis:
                out.append((ins["addr"], reg(ops[0]), v))
    return out


def is_mn(ins, want):
    """MWCC emits `mr.` (record-preserving) as well as `mr`."""
    return ins["mn"] == want or ins["mn"] == want + "."


def this_reg(insns):
    """r3 at entry is `this`.  MWCC usually copies it to a callee-saved register;
    some small constructors leave it in r3 throughout."""
    # the window is wide because a big constructor saves every FPR before it
    # copies `this` out of r3
    for ins in insns[:48]:
        if is_mn(ins, "mr") and len(ins["ops"]) == 2 and reg(ins["ops"][1]) == 3:
            return reg(ins["ops"][0])
    return 3


def cases(insns, tags):
    """property tag -> address of its case body.

    The switch is a binary search: every test is `cmpw <id>, <tag>` followed within
    three instructions by `beq <body>`.  The tag is whichever operand was
    materialised by a `lis`+`addi` immediately before the test.
    """
    out = {}
    for n, ins in enumerate(insns):
        if ins["mn"] != "cmpw" or len(ins["ops"]) != 2:
            continue
        a, b = reg(ins["ops"][0]), reg(ins["ops"][1])
        best = None
        for addr, r, v in tags:
            if addr <= ins["addr"] and r in (a, b):
                if best is None or addr >= best[0]:
                    best = (addr, v)
        if best is None:
            continue
        for m in range(n + 1, min(n + 4, len(insns))):
            if insns[m]["mn"] == "beq":
                out.setdefault(best[1], int(target(insns[m]), 16))
                break
    return out


def block(insns, target):
    by_addr = {i["addr"]: n for n, i in enumerate(insns)}
    if target not in by_addr:
        return []
    out, n = [], by_addr[target]
    while n < len(insns):
        ins = insns[n]
        out.append(ins)
        if ins["mn"] in BRANCH or ins["mn"].startswith("b."):
            break
        n += 1
    return out


def decode_loadtypedef(insns):
    t = this_reg(insns)
    tags = tagged_consts(insns)
    cs = cases(insns, tags)
    rows = []
    for tag, tgt in sorted(cs.items()):
        blk = block(insns, tgt)
        # the member pointer, if the body computed one into r3
        off, kind, sym = None, None, None
        for ins in blk:
            if is_mn(ins, "mr") and len(ins["ops"]) == 2 and reg(ins["ops"][0]) == 3 and reg(ins["ops"][1]) == t:
                off = 0
            if ins["mn"] == "addi" and len(ins["ops"]) == 3 and reg(ins["ops"][0]) == 3 and reg(ins["ops"][1]) == t:
                off = int(ins["ops"][2], 0)
        for ins in blk:
            if ins["mn"] == "bl" and ins["reloc"]:
                sym = ins["reloc"]
        for ins in blk:
            if ins["mn"] == "stfs" and len(ins["ops"]) == 2:
                r, d = mem(ins["ops"], 1)
                if r == t:
                    rows.append({"tag": tag, "offset": d, "width": 4, "how": "float"})
            elif ins["mn"] in ("stw", "sth", "stb") and len(ins["ops"]) == 2:
                r, d = mem(ins["ops"], 1)
                if r == t and off is None:
                    rows.append({"tag": tag, "offset": d,
                                 "width": {"stb": 1, "sth": 2, "stw": 4}[ins["mn"]],
                                 "how": "scalar"})
        if sym and not any(x["tag"] == tag and x.get("how") for x in rows):
            rows.append({"tag": tag, "offset": off, "width": None,
                         "how": "call", "callee": sym,
                         "callees": [b["reloc"] for b in blk if b["reloc"]]})
    return t, rows


def decode_dtor(insns):
    t = this_reg(insns)
    rows = []
    for ins in insns:
        if ins["mn"] != "bl" or not ins["reloc"]:
            continue
        # walk back to the r3 setup
        n = insns.index(ins)
        off = None
        for k in range(n - 1, max(n - 6, -1), -1):
            p = insns[k]
            if is_mn(p, "mr") and len(p["ops"]) == 2 and reg(p["ops"][0]) == 3 and reg(p["ops"][1]) == t:
                off = 0
                break
            if p["mn"] == "addi" and len(p["ops"]) == 3 and reg(p["ops"][0]) == 3 and reg(p["ops"][1]) == t:
                off = int(p["ops"][2], 0)
                break
        if off is not None and "Free" not in ins["reloc"]:
            rows.append({"offset": off, "dtor": ins["reloc"]})
    return t, rows


def decode_ctor(insns):
    t = this_reg(insns)
    rows = []
    for n, ins in enumerate(insns):
        if ins["mn"] in ("stw", "sth", "stb", "stfs") and len(ins["ops"]) == 2:
            r, d = mem(ins["ops"], 1)
            if r == t:
                rows.append({"offset": d,
                             "width": {"stb": 1, "sth": 2, "stw": 4,
                                       "stfs": 4}[ins["mn"]],
                             "how": {"stw": "word", "sth": "half", "stb": "byte",
                                     "stfs": "float"}[ins["mn"]]})
        if ins["mn"] == "bl" and ins["reloc"]:
            off = None
            for k in range(n - 1, max(n - 8, -1), -1):
                p = insns[k]
                if is_mn(p, "mr") and len(p["ops"]) == 2 and reg(p["ops"][0]) == 3 and reg(p["ops"][1]) == t:
                    off = 0
                    break
                if p["mn"] == "addi" and len(p["ops"]) == 3 and reg(p["ops"][0]) == 3 and reg(p["ops"][1]) == t:
                    off = int(p["ops"][2], 0)
                    break
            if off is not None and "Free" not in ins["reloc"]:
                rows.append({"offset": off, "width": None, "how": "call",
                             "callee": ins["reloc"]})
    return t, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", default="Tweaks")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--only", default=None)
    args = ap.parse_args()

    table = symbol_table(args.module)
    syms = sorted(s for s in table if "LoadTypedef" in s or "__ct__" in s or "__dt__" in s)
    if args.only:
        syms = [s for s in syms if args.only in s]
    out = {}
    for s in syms:
        ins = disasm(table[s], s)
        if not ins:
            continue
        if s.startswith("LoadTypedef"):
            _, rows = decode_loadtypedef(ins)
            out[s] = {"kind": "load", "members": rows}
        elif s.startswith("__dt__"):
            _, rows = decode_dtor(ins)
            out[s] = {"kind": "dtor", "members": rows}
        else:
            _, rows = decode_ctor(ins)
            out[s] = {"kind": "ctor", "members": rows}
    if args.json:
        print(json.dumps(out, indent=1))
        return
    for s, d in out.items():
        print("== %s [%s]" % (s, d["kind"]))
        for m in sorted(d["members"], key=lambda x: (x.get("offset") is None, x.get("offset"))):
            extra = m.get("callee") or m.get("dtor") or m.get("how")
            off = "-" if m.get("offset") is None else "0x%04X" % m["offset"]
            print("   +%-6s w=%-4s tag=%-10s %s" % (
                off, m.get("width"),
                ("0x%08X" % m["tag"]) if "tag" in m else "", extra))


if __name__ == "__main__":
    main()
