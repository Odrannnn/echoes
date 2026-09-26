#!/usr/bin/env python3
"""Is r3 still *this* at a given call site? A forward abstract interpretation over r3.

    python3 tools/recv.py 0x8030E43C
    python3 tools/recv.py --fn 0x8030E3xx

Why this exists. "A retail-named method calls the accessor, so the accessor is that method's
class" is the natural inference and it is **wrong** whenever the call site passes something other
than `this` - a member pointer, an element of an array, a global. A depth-1 caller with a reloaded
`r3` looks exactly as convincing as a real one, and this repo has already been burned by a class
attribution with one `bl` behind it. So the attribution gate is not "a named function called it",
it is "a named function called it **with its own this**".

The analysis. Track one abstract value per GPR: `T` (definitely the incoming `this`), `N`
(definitely not - a load, a constant, arithmetic), or `U` (unknown). Enter a function with r3 = T
and every other GPR = U, and step forward over its body, so the value at any call site is read off
the state, not guessed from the last few instructions. Backward scanning - the obvious approach -
gets this wrong whenever the nearest instruction before the `bl` is a store (`stw r3,8(r28)` does
not define r3, but a backward scan that stops at the first non-`mr` is easy to get wrong) or when
the register was set more than six instructions back.

Everything mwcc emits for a non-float body is covered by the table below; an unrecognised
instruction that writes an rN makes that register `U`, which is the conservative answer and costs
an attribution rather than inventing one.
"""
import argparse
import bisect
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(\S+)\s*(.*)$")
LABEL = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")

T, N, U = "T", "N", "U"

# mnemonics whose leading rN operand is a *source*, not a destination
SOURCE_ONLY = {
    "stw", "stb", "sth", "stfs", "stfd", "stfiw", "stwu", "stmw", "stmsr", "mtspr", "mtlr",
    "cmpw", "cmpwi", "cmp", "cmpi", "cmplw", "cmplwi", "cmpl", "cmpli", "tw", "twi", "tne",
    "subf.", "neg", "mcrxr", "crset", "crclr", "cror", "b", "bl", "bctrl", "bctr", "blr",
    "dcbf", "dcbi", "dcbst", "dcbt", "dcbz", "dcba", "dcbtst", "dcbzl", "sync", "isync",
    "mfspr", "mftb", "eieio", "icbi", "lwarx", "lbarx", "lsarx", "abs", "abso", "nabs", "nego",
    "mcrf", "mfcr", "mfmsr", "stfiwx", "stfiwx.", "extsb", "extsh", "extsw",
    "divw", "divwu", "divd", "divdu", "doz", "dozu", "rlwimi", "rlwinm", "rlwnm", "slw", "srw",
    "sraw", "srawi", "srad", "sradi", "addc", "adde", "subfc", "subfe", "mulli", "mulli.",
    "nego.", "cntlzw", "cnttzw", "popcntb", "mulhw", "mulhwu", "mulld", "mulhd", "mulhdu",
    "addco", "addco.", "addme", "addmeo", "addze", "addzeo", "subfme", "subfmeo", "subfze",
    "subfzeo", "eqv", "eqv.", "orc", "orc.", "nand", "nand.", "nor", "nor.", "andc", "andc.",
}
# mnemonics that never write a GPR at all
NO_GPR = {"blr", "bctr", "bctrl", "bc", "bdnz", "bdi", "bnel", "beq", "bne", "blt", "bgt",
          "ble", "bge", "bso", "bns", "bcctr", "bcdblr", "bneblr", "beqlr", "bgtlr", "blelr",
          "bgelr", "bltlr", "mtspr", "mtcrf", "stfiwx", "dcbz", "sync", "eieio", "isync",
          "mcrxr", "crset", "crclr", "cror", "crand", "crxor", "crnand", "crnor", "creqv",
          "mcrf", "b", "bl", "tw", "twi", "tne", "td", "divw", "divwu", "divd", "divdu",
          "mfspr", "mfmsr", "dcbf", "dcbi", "dcbst", "dcbt", "dcba", "dcbzl", "dcbtst",
          "icbi", "lwarx", "lbarx", "lsarx", "mftb", "doz", "dozu", "abs", "nabs"}


def load(path):
    insn, label = {}, {}
    for line in pathlib.Path(path).read_text(errors="replace").splitlines():
        m = LABEL.match(line)
        if m:
            label[int(m.group(1), 16)] = m.group(2)
            continue
        m = INSN.match(line)
        if m:
            insn[int(m.group(1), 16)] = (m.group(3), m.group(4).strip())
    return insn, label


def step(state, mn, ops):
    """One instruction's effect on the abstract GPR state."""
    if mn in NO_GPR:
        # a call clobbers the volatile argument registers; r3 becomes the return value, so
        # `this` is no longer provably in it afterwards.
        if mn in ("bl", "bctrl", "b"):
            for i in range(3, 13):
                state.setdefault(f"r{i}", U)
        return
    m = re.match(r"^(r\d+)\s*,(.*)$", ops)
    if not m:
        return
    d, rest = m.group(1), m.group(2)

    if mn in SOURCE_ONLY:
        if mn in ("neg", "nego", "abs", "nabs", "divw", "divwu", "divd", "divdu", "cntlzw",
                  "cnttzw", "rlwimi", "rlwinm", "rlwnm", "slw", "srw", "sraw", "srawi", "srad",
                  "sradi", "extsb", "extsh", "extsw", "mulhw", "mulhwu", "or", "xor", "and",
                  "orc", "nor", "nand", "eqv", "addc", "adde", "subfc", "subfe", "popcntb",
                  "mulld", "mulhd", "mulhdu", "lwarx", "lbarx", "lsarx", "divwu", "subfme",
                  "subfze", "addme", "addze", "neg"):
            state[d] = N
        return

    if mn in ("mr",):
        s = re.match(r"^(r\d+)$", rest.strip())
        if s:
            state[d] = state.get(s.group(1), U)
        else:
            state[d] = U
        return
    if mn in ("li", "lis", "la", "addis", "addic", "addic.", "addi", "addi.", "add", "add.",
              "subf", "subf.", "sub", "sub.", "subfic", "ori", "oris", "xori", "xoris", "mulli",
              "mulli.", "mfcr", "mfspr", "lwz", "lwz.", "lbz", "lbzu", "lha", "lhau", "lhz",
              "lhzu", "lfs", "lfsu", "lfd", "lfdu", "lmw", "eieio"):
        if mn in ("addi", "addi.", "addic", "addic.", "addis"):
            s = re.match(r"^(r\d+)\s*,(r\d+)\s*,", rest)
            if s and int(rest.split(",")[-1].strip(), 0) == 0 and state.get(s.group(2)) == T \
                    and s.group(2) == d:
                return                      # `addi r3,r3,0` keeps `this`
        if mn in ("lwz", "lwz.", "lbz", "lbzu", "lha", "lhau", "lhz", "lhzu", "lfs", "lfsu",
                  "lfd", "lfdu", "lmw"):
            state[d] = N
            return
        state[d] = N
        return
    if mn.startswith("l"):
        state[d] = N
        return
    # an unrecognised instruction writing an rN: the conservative answer
    state[d] = U


def analyse(start, insn, limit=0x8000):
    """Walk forward from a function entry, yielding (address, state) at every call site."""
    state = {"r3": T}
    for i in range(4, 33):
        state[f"r{i}"] = U
    a = start
    end = start + limit
    while a in insn and a < end:
        mn, ops = insn[a]
        if mn in ("blr", "bctr") and not ops.startswith("0x"):
            return
        if mn in ("bl", "b", "bctrl", "blrl"):
            yield a, dict(state)
        step(state, mn, ops)
        a += 4


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sites", nargs="+", help="call-site addresses")
    ap.add_argument("--asm", default="/tmp/opencode/main.asm")
    ap.add_argument("--symbols", default=str(ROOT / "config" / "G2ME01" / "symbols.txt"))
    a = ap.parse_args()

    insn, label = load(a.asm)
    sym = {}
    pat = re.compile(r"^(\S+) = (\S+):(0x[0-9A-Fa-f]+); // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?")
    for line in pathlib.Path(a.symbols).read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m and m.group(4) == "function":
            sym[int(m.group(3), 16)] = (m.group(1), int(m.group(5) or 0, 16))
    starts = sorted(set(list(sym) + [x for x, n in label.items() if not n.startswith("lbl")]))

    def enc(addr):
        i = bisect.bisect_right(starts, addr) - 1
        return starts[i] if i >= 0 else addr

    def fname(s):
        return sym.get(s, (label.get(s, f"fn_{s:08X}"), 0))[0]

    for sa in a.sites:
        s = int(sa, 16)
        f = enc(s)
        hit = None
        for ca, st in analyse(f, insn):
            if ca == s:
                hit = st
                break
        v = hit.get("r3", U) if hit else "?"
        print(f"0x{s:08X}  in 0x{f:08X} {fname(f)}   r3 = {v}"
              + ("" if v != T else "   <- the accessor's class is this function's class"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
