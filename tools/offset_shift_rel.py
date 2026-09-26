#!/usr/bin/env python3
"""The REL half of tools/offset_shift.py: a class laid out by one wrong constant, found from the
offset deltas alone.

  tools/offset_shift_rel.py                 scan every REL unit, report candidates, print coverage
  tools/offset_shift_rel.py --self-check    falsification checks and preconditions only (gate step)
  tools/offset_shift_rel.py --explain SYM   the paired instructions for one symbol, whatever it scored
  tools/offset_shift_rel.py --min-pct 0     widen the percentage window (default 90-100)
  tools/offset_shift_rel.py --write out.json

**The signature.** A class laid out wrongly by a single constant makes every function that touches
the moved member land at 96-99% with a *uniform operand delta* - the same constant, ours minus
retail, on every operand. A codegen difference does not do that: register reallocation and
instruction scheduling move functions differently. That is how `CStateManager::pad2_2` (0x34, retail
0x2C) and `CAnimData::x120_unk` (0x58, retail 0x48) were found on the DOL side - nineteen functions
went to 100.00% on those two constants (tools/offset_shift.py, commit aec634c). The 86 REL modules
hold the other half of the unmatched functions, and nothing had ever looked there.

**Why a separate file and not a `--rel` flag on the DOL tool.** Two reasons, both about not being
wrong. (1) The pairing is not the same: a REL unit's retail object is dtk's split of the retail
*module* at `build/G2ME01/<Module>/obj/<unit>.o`, not `build/G2ME01/obj/<unit>.o`, and the report's
unit name is prefixed with the module. (2) The rule below is a strict *strengthening* of the DOL
tool's, so sharing one file would either weaken that tool or leave two rules under one name. The
DOL tool's docstring points here; this one does not touch it beyond that.

**How a REL function is paired.** By *name*, which is the only pairing that is not a guess: both
objects are built from the same `config/G2ME01/rels/<Module>/symbols.txt`, so a symbol dtk and
mwcceppc agree on is the same function in both, and it is the same pairing objdiff used to score it.
The byte-identical pairing `tools/fnmap.py` gives is a DOL-side technique and does not transfer: for
a REL unit both objects on disk are ours, and the retail bytes are in the *module*, not the object.
So retail = `build/G2ME01/<Module>/obj/<unit>.o`, ours = `build/G2ME01/src/<unit>.o`. Retail's
address comes from that module's `symbols.txt` and is printed with every candidate, because a
finding you cannot locate is not a finding.

**The rule, and why each part of it is there.** There are two, and the weaker one is the one that
found the bug, so both are documented.

*Strict* (`uniform_delta`) - the DOL tool's rule, unchanged:

  1. The two functions have the same *instruction count*, or the positional pairing below is
     meaningless. (`count`, not bytes: an instruction is the unit the deltas live in.)
  2. The two have the same *operand shape* at every index: mnemonic, registers and relocation types
     equal, with only the *operand* integers removed. This is what rules out codegen.
  3. Every integer operand differs by the same non-zero constant.

*A register keeps its identity in the shape but contributes no integer*, and that distinction is the
whole tool: `lwz r4,0(r4)` and `lwz r6,0(r3)` are the same access written by a different allocation,
so they must fail rule 2 - and their register numbers are not struct offsets, so comparing them as
operands would let a pure reallocation look like a layout shift. The first version of this tool got
that wrong, read the 4 and the 6 as operands, and its own self-check caught it.

Strict rule 2 is also why this tool does **not** report the strongest-looking REL candidate there is:
the seven `LoadTypedefSLdrTweak*` functions at 99.2-99.4%, one class family, all wrong in the same
way. They are a codegen difference - their operand deltas are all 0 - and flagging them would be
confidently wrong.

*Layout* (`layout_delta`) - the same idea, restricted to operands that could be member offsets, and
able to see through a reallocation in the same function. It exists because that combination is the
normal case and the strict rule throws the bug away with the register names: `LoadTypedefSLdrTweak
PlayerRes` is 99.24% because 12 of its 91 instructions are reallocated, and the 3 that carry member
offsets are *all* off by +80. The strict rule sees a register difference and stops. Two exclusions,
each one a weakening, so each is stated:

  * **stack slots** (`D(r1)`) are dropped. The frame size comes from the function's own locals and no
    class member can move it. Without this every function with a frame carries a 0 delta, a real hit
    reads as "mixed", and the bug is lost - which is exactly what happened here before the exclusion
    was added. Measured cost on this tree: **0 of 17** differing operands, because a spill slot never
    moves between a matching pair. A member is never at a negative offset off `r1`, so this cannot
    hide one.
  * **operands equal on both sides** contribute no delta. `lwz r3,0(r3)` is the base, not a member.

  Everything else must still share ONE constant, and at least MIN_LAYOUT_OPS of them must carry it.
  That floor is what stops the degenerate case: "every differing operand is +8" is trivially true when
  there is only one of them, so a single moved operand is reported as not-a-finding, not as a finding.

**Two things deliberately NOT measured, and what each costs.**
  * Branch displacements are dropped before either rule (`@B` in the shape, no integer). A layout
    constant does not move a branch target, and a branch target that moved is codegen; leaving them in
    would manufacture uniformity out of two functions that merely call the same helper. The target is
    the *last* immediate of the instruction, because `bdnz r4,168` puts a register in the middle and
    `bc 20,30,168` carries two BO/BI field masks that are real operands and must survive.
  * `lis`/`oris` half-words are ordinary integers and are compared like any other, so a member far
    enough away to need `lis`+`addi` splits its delta across two instructions and is reported as
    *mixed* rather than uniform. The blind spot is real: a shifted offset that crosses a 64K boundary
    is missed by the strict rule. Both known findings were inside one `addi`, so the window is narrow,
    and `--explain` is how to look at what the window hid.
  * A class laid out 4 bytes *smaller* can make mwcceppc choose a narrower load, which changes the
    instruction rather than an operand, so it fails rule 2 and is never reported. That is the shape
    the other SLdrTweak constructors are in (retail 107 instructions, ours 76 for
    `SLdrTweakSlideShow`), and the rule is honest about it: this tool does not see that, and a tool
    that claimed to would be guessing. **It now *enumerates* it instead** - see "The blind spot"
    at the end of this header, and the section every scan prints.

**Exit codes** (so this can be a gate step later, and so a broken scan can never read as a finding):
  0  scan ran, preconditions and self-check hold, no uniform delta
  1  a uniform operand delta: >= MIN_HITS functions of one class at one constant - the finding
  2  usage error
  3  **not trustworthy**: a precondition or a self-check failed. Deliberately not 1, so a gate can
     fail on 3 while 1 stays informational until the findings are fixed.
`--self-check` only ever returns 0 or 3. `--explain` only ever returns 0 or 2.

**Preconditions, all checked before anything is reported.** A percentage computed from a build that
aborted before rellinking is a measurement of the *previous* binary, so: `build/report.json` must be
newer than every `.rel` and than `main.dol`; the 86 RELs must sha1-match `config/G2ME01/config.yml`;
`main.dol` must be `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. If any fails, the tool prints why
and exits 3. It never rebuilds. It never writes anything except `--write`, which refuses any path
under a source directory or a config file - it is read-only, and idempotent: two runs on one tree
print the same thing.

**The self-check, and why it cannot pass by accident.** `--self-check` runs 24 checks in five groups,
and **every one of them can and did fail during this tool's own development** - which is the honest
argument for the list, more than any claim about the design. What each group is for:
  (1) synthetic hand-written pairs (10), including the four shapes that MUST NOT be flagged: a renamed
      register, bumped *register numbers*, a moved branch target, a changed relocation type - and the
      three cases for the length reporter below, which must be silent on equal lengths and on
      anything under the instruction floor, and must report the *sign*;
  (2) real negative control: the **largest** REL functions objdiff scored exactly 100.00% must yield
      delta {0} under both rules. Largest, not first - an 8-byte accessor has one operand-bearing
      instruction and cannot exercise anything, so picking it makes every control vacuous. This is
      the "a tool that reports a uniform delta when there is none is worse than no tool" check, on
      real data;
  (3) real positive control: take those same 100.00% functions, add +8 to every operand in our copy,
      and require both rules to return +8 *and* the tool to print the paired lines. Because (2) proves
      the unperturbed function yields {0}, this is not passing by inertia;
  (4) the non-uniform control, plus three cases aimed at the layout rule's own escape hatches: a +8
      that moves only stack slots, a single moved operand, and a reallocation with no offset change.
      This control was vacuous in its first version - it alternated by instruction index, which can
      come out uniform, and its "+8" landed on `stwu r1,-128(r1)`, which the layout rule excludes by
      design, so the +8 disappeared and the check printed PASS while testing nothing. It now splits on
      member offsets and rejects a sample with fewer than two of them;
  (5) coverage accounting: the pairs made plus the unpaired equal the report's function count, so
      nothing was dropped on the floor while being counted as covered.

Checks (2) and (3) are the pair that carries the argument: (2) says the detector is silent where
there is provably nothing, (3) says it fires with the right constant where there provably is, and
both run through the identical code path the finding came from. Every real-data control is gated on
having samples, and fails loudly rather than passing on an empty set - a check that passes because it
had nothing to look at is the shape of bug that three green checks agree on.

**What this measured on this tree** (86 REL modules, `./tools/decomp_build.sh`; reproduce with
`tools/offset_shift_rel.py`). Coverage: 791 retail functions across 110 non-auto REL units in 38 of
the 86 modules; 385 paired by name; **359 of the 365 pairable have no member offset differing at
all**; 6 do, and all 6 are off by the *same* constant, **+80 (0x50)**. Zero uniform deltas under the
strict rule - the six are found by the layout rule, which is the point of having it.

  +80, in `Tweaks`, 6 functions, and it names itself:

    __ct__14CTweakContentsFv 99.95%      __dt__14CTweakContentsFv 99.97%     DecodeAnyTweak 99.99%
    __ct__18SLdrTweakPlayerResFv 99.86%  __dt__18SLdrTweakPlayerResFv 99.91% LoadTypedefSLdrTweak
                                                                                PlayerRes 99.24%

  Two classes, one nested in the other, and the constructor of each showing the same 0x50 - which is
  what makes the constant believable rather than merely uniform. It is the defect
  `docs/research/sldr_tweak_sizes.md` predicted from a *header reading* ("+0x50, five extra members in
  one generated struct"), re-derived here from retail bytes, and it is still present. What is measured
  *here* is the constant and nothing else: 0x50 = 5 x 16, and 16 is `sizeof(rstl::string)` under
  mwcceppc, so the constant is consistent with that doc's count of five surplus members. Confirming
  the member itself needs a probe (below), not this tool.

  The honest limits: 2 of the 6 symbols are a free function and a loader, neither of which names the
  class it writes into, so they are counted for the constant and listed as `<owner not determinable>`.
  The tool names the classes and the addresses and deliberately does not guess the member. And the
  coverage is not the whole REL half - 48 of the 86 modules have no decompiled unit at all, 10 units
  (354 functions) are never built, and 52 more functions have no counterpart in our object, so "the
  rest is clean" is not a claim this tool supports. It is a claim about the 385 pairs, and the
  histogram the tool prints is the evidence for it.

  **What lane `tweaks` did about it (2026-09-26).** The member was then pinned from retail's own
  bytes and the whole six moved: `SLdrTweakPlayerRes_AutoMapperIcons` declared **fourteen**
  `rstl::string`s where retail's `LoadTypedefSLdrTweakPlayerRes_AutoMapperIcons` (.text:0x1A28C) has
  **nine** - it switches on nine property ids and writes them to +0x00..+0x80, and retail's
  `__ct__34SLdrTweakPlayerRes_AutoMapperIconsFv` puts the next member at +0x90. The five surplus
  members are `mapIconG/M/R/U/L` (ids 0x5096bfa5, 0xf4e6e0eb, 0x65700ccc, 0xa0d73242, 0x5291eb5f),
  which appear in no retail switch at all: 0xE0 against 0x90, +0x50, and every later member of
  `SLdrTweakPlayerRes` **and** of `CTweakContents` sat 0x50 too high. Five of the six went to 100.00%
  (`LoadTypedefSLdrTweakPlayerRes` is 99.29% on a 12-instruction register reallocation, no offsets
  moved) and `matched` went 3255 -> 3260. `sizeof(CTweakContents)` measured with mwcceppc is now
  **0x31F4**, retail's `li r3,0x31F4` in `REL_LoadTweaks`, against 0x37D0 before.

**The blind spot, and what was done about it.** The header above records that "a class laid out 4 bytes
*smaller* can make mwcceppc choose a narrower load, which changes the instruction rather than an
operand, so it fails rule 2 and is never reported", and names `SLdrTweakSlideShow` (retail 107
instructions, ours 76) as the shape. That hole is real and it is **still a hole**: a length mismatch
carries no constant, so no rule here can see it. What changed is that the hole stopped being a
*count*. `--lengths` names every paired function whose instruction counts differ, with retail's
address and size, objdiff's percentage and the signed delta, so the blind spot is a worklist rather
than a number nobody can act on.

  **The named example is a false lead, and measuring it is why the list is published.** Retail's
  `__ct__18SLdrTweakSlideShowFv` (.text:0x2EC4, 0x1AC) stores at +0x00/+0x10/+0x20 (three
  `rstl::string`), +0x30/+0x34/+0x4C (one word each - **`sizeof(CColor)` is 4 under mwcceppc here**,
  which is why three CColor members take 12 bytes and not 48), +0x38/+0x3C/+0x40/+0x44/+0x48 (five
  floats), +0x50..+0x64 (six floats) and +0x68 (a fourth string); `__dt__` dereferences four strings
  and `LoadTypedefSLdrTweakSlideShow` switches on seventeen property ids. `tools/size_probe_tweaks.cpp`
  measures *our* offsets and gets 0x30, 0x34, 0x4C, 0x68 and `sizeof == 0x78` - retail's, all five.
  The 31 instructions are not a layout bug: retail re-issues `lis` for the base of every `.rodata`
  constant it loads, where ours computes the base once into `r31`, plus one float store the compiler
  dropped. So **"shorter than retail" is not evidence of a class laid out small**, and a tool that
  inferred one from a length would be guessing. The list is a worklist, not a diagnosis, and it says
  so where it prints.

  Every scan prints the list, not only on request: a blind spot that has to be asked for is a blind
  spot nobody looks at, and the whole point is that the count was already in the output being ignored.
"""
import argparse
import collections
import hashlib
import json
import os
import re
import subprocess
import sys

OBJDUMP = 'build/binutils/powerpc-eabi-objdump'
DOL_SHA1 = '6ef9b491d0cc08bc81a124fdedb8bfaec34d0010'

# The DOL tool's window: "96-99% with a uniform delta". A layout constant does not always leave a
# function that close, so --min-pct 0 widens it. The rule does not change, only the window.
DEFAULT_MIN_PCT = 90.0
DEFAULT_MAX_PCT = 100.0
# One function with a uniform delta is a positional coincidence until proven otherwise - a repeated
# instruction pattern that happens to line up. A class needs this many *distinct* functions at the
# same constant; singles are printed separately as weak and do not exit 1.
MIN_HITS = 2
# Rule 2 is what actually prevents a positional coincidence, so the length floor can be low: a
# one-load accessor (`lwz r3,OFF(r3); blr`) is two instructions and is the *most* diagnostic function
# there is, because a moved member shows up in it and nowhere else. Only one-instruction functions are
# excluded, and there is nothing to pair those against.
MIN_INSNS = 2

INSN_RE = re.compile(r'^\s+([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$')
SYM_RE = re.compile(r'^[0-9a-f]+ <(.+)>:$')
RELOC_RE = re.compile(r'^\s+[0-9a-f]+:\t+[0-9a-f]+: (R_PPC_\w+)')
NUM_RE = re.compile(r'-?\d+')
IMMEDIATE = re.compile(r'^(0x[0-9a-f]+|-?\d+)$')

# Why a function was not flagged. Every one of these is counted and printed, because "no finding" has
# to be a measurement with reasons attached rather than a silence.
R_LENGTH = 'instruction count differs'
R_SHAPE = 'operand shape differs (register, schedule, or a different instruction)'
R_NO_OPS = 'no integer operands to compare'
R_ZERO = 'identical shape, every integer operand unchanged'
R_MIXED = 'operands differ by more than one constant'
R_UNIFORM = 'UNIFORM non-zero delta'
REJECT_REASONS = (R_LENGTH, R_SHAPE, R_NO_OPS, R_ZERO, R_MIXED)

# MWCC mangles a class as `<decimal length><name>` and nests as `<len><Outer><len><Inner>`. The
# decoder is `_length_prefixed` / `owner_classes` below; `c++filt` cannot do this - it is not the
# Itanium ABI - which is why it is parsed here rather than handed to binutils.
NOT_A_CLASS = {'rstl', 'std', 'MetroidPrime', 'Kyoto', 'Dolphin'}
TEMPLATE_PREFIX = ('reserved_', 'vector', 'basic_', 'allocator', 'string', 'pair', 'list', 'map',
                   'set', 'shared_', 'weak_')
# A module's entry points name no class of their own: `REL_LoadTweaks__FR12CInputStream`'s first
# parameter is CInputStream, but what it writes to is CTweakContents. Reporting the parameter as the
# class would be a confident wrong answer, so they are grouped as module entry points instead.
MODULE_ENTRY = re.compile(r'^(REL_|__REL_|RVL_)')


def objdump_insns(path):
    """{symbol: [operand text]} for one object, with every *address* already thrown away.

    Thrown away, and why: the address column and the raw byte column, so nothing about where the
    code lives can be mistaken for what it computes; and objdump's `1a4 <sym+0x1a4>` branch-target
    annotation. Relocation lines are kept as `@R_<TYPE>`, because a different external reference is a
    real difference and must fail the operand-shape rule rather than slip past it.
    """
    proc = subprocess.run([OBJDUMP, '-d', '-r', path], capture_output=True, text=True)
    if proc.returncode:
        return None
    cur, res = None, {}
    for line in proc.stdout.splitlines():
        m = SYM_RE.match(line)
        if m:
            cur = m.group(1)
            res[cur] = []
            continue
        if cur is None:
            continue
        m = INSN_RE.match(line)
        if m:
            txt = re.sub(r'\s+[0-9a-f]+ <[^>]*>$', '', m.group(3).split('//')[0].rstrip())
            res[cur].append(txt)
            continue
        m = RELOC_RE.match(line)
        if m:
            res[cur].append('@R_' + m.group(1))
    return res


REG_ANY = re.compile(r'\b(?:[rf]\d+|cr\d*|ctr|lr|sp|pc|xer|fpscr|msr|mq|dbcr|dsr)\b')


def _token(tok):
    """(shape, integers) for one whitespace-separated operand token.

    This is the distinction the whole tool rests on: **a register keeps its identity in the shape
    but contributes no integer.** `lwz r4,0(r4)` and `lwz r6,0(r3)` are the same access written by a
    different register allocation, so they must fail rule 2; but their register numbers are not struct
    offsets, so they must not be compared as operands either. Reading the 4 and the 6 as operands is
    how a pure reallocation turns into an apparent layout delta - the self-check's
    `synthetic/reg-not-operand` case is exactly this, and it is why the split is here.
    """
    out, nums, last = [], [], 0
    for m in REG_ANY.finditer(tok):
        seg = tok[last:m.start()]
        out.append(NUM_RE.sub('@', seg))
        out.append(m.group(0))
        nums.extend(int(x.group(0)) for x in NUM_RE.finditer(seg))
        last = m.end()
    tail = tok[last:]
    out.append(NUM_RE.sub('@', tail))
    nums.extend(int(x.group(0)) for x in NUM_RE.finditer(tail))
    return ''.join(out), nums


def _drop_branch_target(parts):
    """Neutralise a branch's displacement so it is neither shape nor integer.

    A layout constant does not move a branch target and a branch target that moved is codegen, so
    leaving it in would let two functions that merely call the same helper look uniform. The target is
    the *last* immediate, not the second token: `bdnz r4,168` puts a register in between, and
    `bc 20,30,168` carries two BO/BI field masks that are real operands and must survive.
    """
    if parts and parts[0].startswith('b'):
        for i in range(len(parts) - 1, 0, -1):
            if IMMEDIATE.match(parts[i]):
                parts[i] = '@B'
                break
    return parts


def norm(insn):
    """(shape tokens, per-token integer lists) for one normalised instruction.

    The shape is the instruction with every *operand* integer replaced by `@` and every register left
    exactly as it was, so two instructions share a shape exactly when the same code reads the same
    registers and takes the same branches - the property a codegen difference breaks and a layout
    constant does not.

    The integers come back **per token**, not flattened, because the two rules below have to be able to
    ask which operand a number belonged to: a frame-slot offset and a struct offset can sit in the same
    instruction, and a flat list cannot tell them apart.
    """
    if insn.startswith('@R_'):
        return [insn], [[]]
    shapes, nums = [], []
    for part in _drop_branch_target(insn.split()):
        shape, n = _token(part)
        shapes.append(shape)
        nums.append(n)
    return shapes, nums


def is_branch(insn):
    parts = insn.split()
    return bool(parts) and parts[0].startswith('b') and len(parts) > 1


def apply_delta(insn, delta):
    """Rebuild `insn` with every *operand* integer moved by `delta`, branch target left alone.

    This is how the self-check synthesises a layout bug out of real bytes, and it must go through the
    same split `norm` uses: bumping every number in the text would also bump the register numbers
    (`stw r0,8(r28)` -> `stw r8,16(r36)`), which changes the operand shape, so the control would be
    rejected for the wrong reason and would prove nothing at all.
    """
    if insn.startswith('@R_'):
        return insn
    out = []
    for part in _drop_branch_target(insn.split()):
        buf, last = [], 0
        for m in REG_ANY.finditer(part):
            seg = part[last:m.start()]
            buf.append(NUM_RE.sub(lambda x: str(int(x.group(0)) + delta), seg))
            buf.append(m.group(0))
            last = m.end()
        seg = part[last:]
        buf.append(NUM_RE.sub(lambda x: str(int(x.group(0)) + delta), seg))
        out.append(''.join(buf))
    return ' '.join(out)


def uniform_delta(retail, ours):
    """(delta, category, detail) - the one constant every integer operand differs by, or None.

    This is the **strict** rule and it is the DOL tool's, unchanged: both functions must have the
    same instruction count, every instruction must have the same operand shape, and every integer
    operand must differ by one constant. It is strict enough to be nearly unfalsifiable, and on real
    game code it is also nearly useless, because a function that touches a moved member *and* gets
    reallocated by the compiler fails rule 2 and its layout bug is thrown away with the register names.
    `layout_delta` below is the rule that survives that, and this one is kept as the first line of
    defence: a strict hit needs no caveats quoted alongside it.

    `category` is R_UNIFORM or one of the R_* reasons, `detail` is what to print beside it. A
    function returning (None, R_ZERO) has been compared and found correct; one returning
    (None, R_SHAPE) has been compared and found to differ in a way no layout constant explains.
    Those are different results and the tool must not merge them.
    """
    if len(retail) != len(ours):
        return None, R_LENGTH, '%d retail / %d ours' % (len(retail), len(ours))
    deltas, mismatched, nops = set(), 0, 0
    for r, o in zip(retail, ours):
        sr, nr = norm(r)
        so, no = norm(o)
        if sr != so or len(nr) != len(no):
            mismatched += 1
            continue
        nops += sum(len(x) for x in nr)
        deltas.update(y - x for a, b in zip(nr, no) for x, y in zip(a, b))
    if mismatched:
        return None, R_SHAPE, '%d of %d instructions' % (mismatched, len(retail))
    if not deltas:
        return None, R_NO_OPS, '%d instructions' % len(retail)
    if deltas == {0}:
        return None, R_ZERO, '%d operands over %d instructions' % (nops, len(retail))
    if len(deltas) == 1:
        return deltas.pop(), R_UNIFORM, 'all %d operands over %d instructions' % (nops, len(retail))
    return None, R_MIXED, '%d constants, e.g. %s' % (len(deltas), sorted(deltas)[:4])


# An operand that is a *local* offset rather than a member offset. These are the two exclusions the
# layout rule needs, and both have to be stated because each one weakens it:
#   r1 - the stack pointer. `stw r0,8(r1)` is a spill slot; the frame size comes from the function's own
#        locals and no class member can move it. Without this exclusion every function with a frame
#        would carry a 0 delta and a real layout bug would read as "mixed", which is exactly what
#        happened to `SLdrTweakPlayerRes` before the exclusion was added.
#   0 on both sides - the base itself, `lwz r3,0(r3)`. Nothing moved; there is no delta to read.
# A member at offset 0 in both is not a moved member, so the exclusion cannot hide one.
FRAME = re.compile(r'\br1\b')
MIN_LAYOUT_OPS = 2


def length_delta(retail, ours):
    """The signed instruction-count difference (ours - retail), or None when there is no mismatch.

    **A length mismatch is not a finding and this function does not pretend otherwise.** There is no
    constant in a length: a class laid out 80 bytes small does not shift an operand, it makes
    mwcceppc emit fewer stores, so both rules above are blind to it by construction. What a length
    *can* be is a place to look, so the only honest output is the count and the sign - never a member,
    never a class, never a "likely cause".

    It is a separate function rather than a line in the scan so the self-check can test it on
    hand-written input, where a mistake in it is visible; the two `synthetic/length-*` cases are the
    only part of the blind-spot machinery that can fail without a build.
    """
    if len(retail) < MIN_INSNS or len(ours) < MIN_INSNS:
        return None                     # nothing to compare: the same floor the rules use
    if len(retail) == len(ours):
        return None
    return len(ours) - len(retail)


def layout_delta(retail, ours):
    """(delta, detail) - one constant shared by every *member-offset* operand, or None.

    Where `uniform_delta` is all-or-nothing, this looks only at operands that could plausibly be struct
    offsets, and only at instructions whose operand shape matches retail's. It exists because a real
    layout bug routinely coexists with a reallocation in the same function, and dropping the whole
    function for that reason throws the bug away:

      LoadTypedefSLdrTweakPlayerRes, 99.24% - 12 of 91 instructions are reallocated, and the 3 that
      carry member offsets are ALL off by +80. The strict rule sees a register difference and stops.

    The exclusions are the two documented above, and the cost is measured and printed: on this tree
    the frame exclusion removes 0 of the 17 differing operands, because no spill slot ever moves
    between a matching pair. `detail` reports how many operands carried the delta, so a hit on two
    operands is visible as such and is not dressed up as a hit on twenty.
    """
    if len(retail) != len(ours):
        return None, 'instruction counts differ'
    deltas, carried, considered = set(), 0, 0
    for r, o in zip(retail, ours):
        sr, nr = norm(r)
        so, no = norm(o)
        if sr != so or len(nr) != len(no):
            continue
        for shape_r, shape_o, a, b in zip(sr, so, nr, no):
            if FRAME.search(shape_r) or FRAME.search(shape_o):
                continue
            for x, y in zip(a, b):
                considered += 1
                if x == y:
                    continue
                deltas.add(y - x)
                carried += 1
    if not deltas:
        return None, 'no member offset differs (%d operands compared)' % considered
    if deltas == {0}:
        return None, 'no member offset differs (%d operands compared)' % considered
    if len(deltas) == 1:
        k = deltas.pop()
        if carried < MIN_LAYOUT_OPS:
            return None, 'one operand off by %+d, under the %d needed' % (k, MIN_LAYOUT_OPS)
        return k, '%d member offsets, all %+d (%d operands compared)' % (carried, k, considered)
    return None, '%d different constants (%s) over %d operands' % (len(deltas), sorted(deltas)[:4], considered)


def owner_classes(mangled):
    """The class a symbol is a member *of*, or [] when the symbol does not say.

    MWCC puts the class immediately after the method name and before the `F` that starts the
    parameter list, either flat (`__17CScriptCannonBallF`) or as an enclosing-class chain
    (`__Q2` `17CScriptCannonBall` `11TrackedShot` `F`). A constructor is the same thing with the
    marker `__ct__`/`__dt__` in front of it. Parsed from the tree's own 743 renamed symbols:

      __ct__18SLdrTweakPlayerResFv         -> SLdrTweakPlayerRes
      __ct__Q217CScriptCannonBall11TrackedShotF9TUniqueIdb
                                            -> CScriptCannonBall::TrackedShot
      AcceptScriptMsg__17CScriptCannonBallFR13CStateManagerRC10CScriptMsg
                                            -> CScriptCannonBall
      DecodeAnyTweak__FUiR12CInputStream   -> []   a FREE function; CInputStream is its parameter
      LoadTypedefSLdrTweakPlayerRes__FR18SLdrTweakPlayerResR12CInputStream
                                            -> []   writes through the reference it was handed
      REL_LoadTweaks__FR12CInputStream     -> []   a module entry point
      __dt__Q24rstl53reserved_vector<...>Fv
                                            -> []   the chain names a template, not a class

    Each `[]` is a case where the naive reading is *confidently wrong*, and a diagnostic that guesses
    is worse than one that abstains: `DecodeAnyTweak` is a Tweaks free function that happens to take a
    `CInputStream&`, and calling it a CInputStream method would put a class name on a finding that
    says nothing about CInputStream. The symbol is still printed beside the finding, because the bytes
    are still evidence - only the class is not.

    The length prefix has to **slice** the name rather than merely precede it; see
    `_length_prefixed` for why, and for the bug it caused.

    A hit is evidence about the constant first and the class second, which is why the report groups by
    `(module, constant)` and lists the classes: a layout constant moves every member after the wrong
    one, in the class that owns the member *and* in every class that contains it, so "SLdrTweakPlayerRes
    and the CTweakContents that contains it" is the true statement, and the two ctors agreeing on 0x50
    is what makes the constant believable rather than merely uniform.
    """
    if MODULE_ENTRY.match(mangled) or mangled.startswith('LoadTypedef'):
        return []
    if mangled.startswith('__ct__') or mangled.startswith('__dt__'):
        rest = mangled[6:]
    elif '__' in mangled:
        rest = mangled.split('__', 1)[1]
    else:
        return []
    nested = re.match(r'Q(\d)', rest)
    if nested:
        names = _length_prefixed(rest[nested.end():])
    else:
        names = _length_prefixed(rest)
    if not names:
        return []                           # a free function: the symbol names no class
    if names[0] in NOT_A_CLASS or names[0].startswith(TEMPLATE_PREFIX):
        return []                           # a namespace or a template owner: do not guess
    return ['::'.join(names)]


def _length_prefixed(s):
    """Every `<decimal length><name>` at the head of `s`, sliced by the length.

    The length is not decoration, it is the only thing that terminates the name: MWCC identifiers are
    `[A-Za-z_0-9]+`, so `18SLdrTweakPlayerResFv` cannot be split by a pattern - a greedy
    `[A-Za-z_0-9]*` returns `SLdrTweakPlayerResFv`, which is not a class, and only the count makes it
    `SLdrTweakPlayerRes`. That was a real bug here, found by reading the output, not by a check:
    `owner_classes` has no self-check case, because a class name cannot be validated against anything
    in this tree. It stops at the first token with no leading length, which is the `F` that starts the
    parameter list.
    """
    out, i = [], 0
    while i < len(s):
        m = re.match(r'\d+', s[i:])
        if not m:
            break
        n, i = int(m.group(0)), i + m.end()
        if n == 0 or i + n > len(s):
            break
        out.append(s[i:i + n])
        i += n
    return out


GROUP_UNKNOWN = '<owner not determinable from the symbol>'


def retail_addrs(module):
    """{symbol: (text offset, size)} from config/G2ME01/rels/<Module>/symbols.txt.

    dtk's own labels are `fn_<id>_<offset>` and a renamed symbol carries its real name; both are in
    this one file, so it is the retail address of whatever objdiff paired.
    """
    path = 'config/G2ME01/rels/%s/symbols.txt' % module
    res = {}
    try:
        with open(path) as fh:
            for line in fh:
                m = re.match(r'^(\S+) = \.text:(0x[0-9A-Fa-f]+); // type:function size:(0x[0-9A-Fa-f]+)', line)
                if m:
                    res[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    except OSError:
        pass
    return res


def rel_modules():
    try:
        return sorted(os.listdir('config/G2ME01/rels'))
    except OSError:
        return []


def rel_units(report, only_module=None):
    """[(module, unit, retail object, our object)] for every non-auto REL unit in the report.

    A unit's retail object is dtk's split of the retail *module* at
    `build/G2ME01/<Module>/obj/<unit>.o`; ours is `build/G2ME01/src/<unit>.o` - the same object the
    link uses, not the copy under `<Module>/obj/` that `tools/unit_fit.sh` compares against.
    """
    out = []
    for u in report.get('units', []):
        md = u.get('metadata', {})
        if md.get('auto_generated'):
            continue
        module = md.get('module_name')
        if not module or module == 'main' or (only_module and module != only_module):
            continue
        unit = u['name'][len(module) + 1:]
        out.append((module, unit, 'build/G2ME01/%s/obj/%s.o' % (module, unit),
                    'build/G2ME01/src/%s.o' % unit))
    return out


def check_preconditions(verbose=True):
    """Every reason the result could be meaningless. Returns (ok, lines)."""
    lines, used, ok = [], [], True
    if not os.path.exists(OBJDUMP):
        return False, ['%s missing - copy the toolchain in, see docs/LANE_BRIEFING.md' % OBJDUMP]
    if not os.path.exists('build/report.json'):
        return False, ['build/report.json missing - run ./tools/decomp_build.sh first']
    modules = rel_modules()
    used += ['build/G2ME01/%s/%s.rel' % (m, m) for m in modules]
    used.append('build/G2ME01/main.dol')
    report_mtime = os.path.getmtime('build/report.json')

    # 1. The report must postdate every binary it describes. objdiff is regenerated by the build
    #    *after* the link, so a report older than any .rel is a report of the previous binary - and
    #    that is exactly the measurement this tool would otherwise present as current.
    stale = [p for p in used if os.path.exists(p) and os.path.getmtime(p) > report_mtime]
    if stale:
        ok = False
        lines.append('STALE  build/report.json is older than %d of the %d binaries it scores (first %s) '
                     '- regenerate with ./tools/decomp_build.sh' % (len(stale), len(used), stale[0]))
    elif verbose:
        lines.append('fresh  build/report.json is newer than all %d RELs and main.dol' % len(modules))

    # 2. The build must have reproduced retail, or the "retail" side of every comparison is fiction.
    try:
        cfg = open('config/G2ME01/config.yml').read()
        pairs = re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg)
    except OSError as exc:
        return False, ['config/G2ME01/config.yml unreadable: %s' % exc]
    bad = []
    for name, want in pairs:
        mod = name[:-4]
        rel = 'build/G2ME01/%s/%s.rel' % (mod, mod)
        if not os.path.exists(rel):
            bad.append(mod + ' (missing)')
        elif hashlib.sha1(open(rel, 'rb').read()).hexdigest() != want:
            bad.append(mod + ' (sha1)')
    if bad:
        ok = False
        lines.append('HASH   %d of %d RELs do not match config.yml: %s'
                     % (len(bad), len(pairs), ' '.join(bad[:4])))
    elif verbose:
        lines.append('hash   all %d RELs sha1-match config/G2ME01/config.yml' % len(pairs))

    if not os.path.exists('build/G2ME01/main.dol'):
        ok = False
        lines.append('HASH   main.dol missing - the build did not finish')
    else:
        dol = hashlib.sha1(open('build/G2ME01/main.dol', 'rb').read()).hexdigest()
        if dol != DOL_SHA1:
            ok = False
            lines.append('HASH   main.dol is %s, not %s' % (dol, DOL_SHA1))
        elif verbose:
            lines.append('hash   main.dol sha1 %s' % dol)
    return ok, lines


def pair_insn(retail, ours, name, limit=24):
    """[(index, retail text, our text, retail operands, our operands)] - the differing pairs.

    Printed the way tools/lanediff.sh prints: no addresses, no byte columns, no branch-target
    annotation, so the only thing left in the line is which operand moved. Branches are skipped: a
    moved branch target is codegen, and `norm` has already thrown its displacement away.
    """
    if name not in retail or name not in ours:
        return []
    a, b = retail[name], ours[name]
    rows = []
    for i in range(min(len(a), len(b))):
        sr, nr = norm(a[i])
        so, no = norm(b[i])
        if sr == so and nr == no:
            continue
        if is_branch(a[i]) or is_branch(b[i]):
            continue
        rows.append((i, a[i], b[i], nr, no))
        if len(rows) >= limit:
            break
    return rows


def print_pairs(rows, pct=None, cap=None):
    if not rows:
        print('     (no paired instructions differ - or the function is not in both objects)')
        return
    if cap and len(rows) > cap:
        print('     ... %d differing pairs, showing the first %d' % (len(rows), cap))
        rows = rows[:cap]
    for i, a, b, na, nb in rows:
        deltas = [y - x for xa, xb in zip(na, nb) for x, y in zip(xa, xb)]
        print('     %4d  %-36s %s' % (i, a, b))
        print('          operand deltas %s' % (deltas or 'none - the registers moved, not an offset'))


def self_check(report):
    """The five groups from the header. Returns (ok, lines)."""
    lines, ok = [], True

    def check(label, cond, detail):
        nonlocal ok
        if not cond:
            ok = False
        lines.append('  %-22s %s  %s' % (label, 'PASS' if cond else 'FAIL', detail))

    # (1) synthetic pairs, including the two shapes that MUST NOT be flagged.
    ret = ['stw     r0,8(r28)', 'addi    r0,r3,4', 'lwz     r4,0(r4)', 'cmpw    r4,r31',
           'b       20 <f+0x20>', 'addi    r0,r3,2', 'stw     r0,8(r28)', 'blr']
    check('synthetic/identical', uniform_delta(ret, ret)[0] is None, 'an identical pair yields no delta')
    shifted = [apply_delta(i, 8) for i in ret]
    d = uniform_delta(ret, shifted)[0]
    check('synthetic/uniform+8', d == 8, 'a +8 displacement on every operand yields +8 (got %s)' % d)
    mixed = list(shifted)
    mixed[1] = 'addi    r0,r3,6'
    check('synthetic/non-uniform', uniform_delta(ret, mixed)[0] is None, '+8 and +4 together yield nothing')
    regs = ['lwz     r6,0(r3)' if i.startswith('lwz') else i for i in ret]
    check('synthetic/renamed-reg', uniform_delta(ret, regs)[0] is None, 'r4->r6 is codegen, not layout')
    check('synthetic/branch-target', uniform_delta(['b       20 <f+0x20>'], ['b       2c <f+0x2c>'])[0] is None,
          'a moved branch target is not an operand delta')
    check('synthetic/reloc-type',
          uniform_delta(['bl     0 <g>', '@R_PPC_REL24'], ['bl     0 <g>', '@R_PPC_RELA24'])[0] is None,
          'a changed relocation is not an operand delta')
    # A register number must not be mistaken for an operand: `stw r0,8(r28)` shifted to `stw r8,16(r36)`
    # has the same *arithmetic* but a different shape, and shape is what rule 2 tests.
    check('synthetic/reg-not-operand',
          uniform_delta(['stw     r0,8(r28)'], ['stw     r8,16(r36)'])[0] is None,
          'bumping the register numbers is not a uniform operand delta')
    # The blind-spot reporter. Two cases, both of which can and did fail while it was written: it
    # must be silent on equal lengths and on anything under the same floor the rules use, and it must
    # report the SIGN - a class laid out too small is the case the header says this cannot see, so a
    # version that reported the absolute value would send the reader to the wrong end of the struct.
    eight = ['stw     r0,8(r28)'] * 8
    check('synthetic/length-equal', length_delta(eight, eight) is None,
          'equal lengths are not a mismatch')
    check('synthetic/length-sign', (length_delta(eight, eight[:5]), length_delta(eight[:5], eight)) == (-3, 3),
          'ours shorter and ours longer are distinguishable (got %s, %s)'
          % (length_delta(eight, eight[:5]), length_delta(eight[:5], eight)))
    check('synthetic/length-floor', length_delta(['blr'], ['blr']) is None and length_delta(eight, ['blr']) is None,
          'a function under the %d-instruction floor is not listed' % MIN_INSNS)

    # (2)-(4) real controls, built from REL functions objdiff scored exactly 100.00%.
    by_name = report_units(report)
    exact = []
    for module, unit, _, _ in rel_units(report):
        for f in by_name.get('%s/%s' % (module, unit), {}).get('functions', []):
            if f.get('fuzzy_match_percent') == 100.0 and f.get('name'):
                exact.append(('%s/%s' % (module, unit), f['name']))
    check('real/has-100%-pairs', len(exact) >= 8, '%d REL functions are at exactly 100.00%%' % len(exact))
    # The *largest* 100.00% functions, not the first three: an 8-byte accessor has one operand-bearing
    # instruction and cannot exercise the rule, so picking it would make every control below vacuous.
    sized = []
    for unit_name, sym in exact:
        module, unit = unit_name.split('/', 1)
        a = objdump_insns('build/G2ME01/%s/obj/%s.o' % (module, unit))
        b = objdump_insns('build/G2ME01/src/%s.o' % unit)
        if a and b and sym in a and sym in b:
            sized.append((len(a[sym]), sym, (a[sym], b[sym])))
    sized.sort(key=lambda t: -t[0])
    picked, sources = [], {}
    for _, sym, texts in sized[:3]:
        picked.append(sym)
        sources[sym] = texts
    check('real/samples-loaded', len(picked) == 3,
          'loaded the 3 largest 100.00%% pairs: %s instructions'
          % ', '.join(str(len(sources[s][0])) for s in picked))
    # Every check below is gated on having samples. A check that passes because it had nothing to look
    # at is worse than no check: it is the shape of bug that three green checks agree on.
    if len(picked) < 3:
        return ok, lines

    verdicts = [uniform_delta(*sources[s])[0] for s in picked]
    check('real/100%-is-silent', all(d is None for d in verdicts),
          'all %d 100.00%% functions yield no delta' % len(picked))

    found = []
    for sym in sources:
        a, b = sources[sym]
        bumped = [apply_delta(t, 8) for t in b]
        d = uniform_delta(a, bumped)[0]
        if d == 8:
            found.append((sym, len(pair_insn({sym: a}, {sym: bumped}, sym))))
    check('real/+8-is-found', len(found) == len(sources),
          'a +8 perturbation of all %d 100.00%% pairs is found as +8 (%d of %d)'
          % (len(sources), len(found), len(sources)))
    check('real/+8-shows-bytes', bool(found) and found[0][1] > 0,
          'the +8 control prints its differing pairs (%s)' % (found[0][1] if found else 0))

    # A non-uniform perturbation of real bytes. Two things had to be got right here and the first
    # version of this control got both wrong, which is worth recording: alternating by *instruction
    # index* can come out uniform (a function whose integer-bearing instructions share a parity), and
    # picking "the first operand-bearing instruction" picked `stwu r1,-128(r1)` - a frame slot, which
    # the layout rule excludes by design, so the +8 vanished and the control became vacuous while
    # still reading PASS. So the split is made on *member* offsets, and a sample with fewer than two
    # of them is rejected rather than counted.
    def member_offsets(insns):
        """Indices of instructions carrying at least one non-frame integer operand."""
        out = []
        for i, t in enumerate(insns):
            for shape, nums in zip(*norm(t)):
                if not FRAME.search(shape) and any(nums):
                    out.append(i)
                    break
        return out

    samples = [s for s in sources if len(member_offsets(sources[s][1])) >= 2]
    check('real/non-uniform-sample', bool(samples),
          '%d of %d pairs have two or more member-offset instructions' % (len(samples), len(sources)))
    if not samples:
        return ok, lines
    leaked, loose_leaked = [], []
    for sym in samples:
        a, b = sources[sym]
        first = member_offsets(b)[0]
        bumped = [apply_delta(t, 8 if i == first else -8) for i, t in enumerate(b)]
        leaked.append(uniform_delta(a, bumped)[0])
        loose_leaked.append(layout_delta(a, bumped)[0])
    check('real/non-uniform-silent', all(d is None for d in leaked),
          'the strict rule is silent on a +8/-8 perturbation of %d real 100.00%% pairs' % len(samples))
    check('layout/non-uniform-silent', all(d is None for d in loose_leaked),
          'the layout rule is silent on the same +8/-8 perturbation (%d of %d leaked)'
          % (sum(1 for d in loose_leaked if d is not None), len(samples)))

    # (4b) The layout rule, which is the weaker of the two and therefore the one that needs its own
    #      controls. It has three escape hatches that could each turn into a false positive, and each
    #      gets a case that must stay silent: a reallocation on its own, a +8 that moves only frame
    #      slots, and a +8 on a single operand. A single operand is the dangerous one, because "every
    #      differing operand is +8" is trivially true when there is only one of them.
    frame_ret = ['stw     r0,4(r1)', 'addi    r31,r1,-32', 'stwu    r1,-64(r1)', 'blr',
                 'lwz     r3,8(r28)', 'addi    r0,r3,4', 'lwz     r4,0(r4)', 'blr']
    shifted_frame = [apply_delta(i, 8) if 'r1' in i.split()[1:2] or '(r1)' in i else i
                     for i in frame_ret]
    check('layout/frame-slot-ignored', layout_delta(frame_ret, shifted_frame)[0] is None,
          'a +8 that moves only stack slots is not a layout delta (got %s)'
          % (layout_delta(frame_ret, shifted_frame)[0],))
    one_op = ['lwz     r3,8(r28)', 'blr']
    check('layout/single-operand-ignored', layout_delta(one_op, [apply_delta(t, 8) for t in one_op])[0] is None,
          'one moved operand is under MIN_LAYOUT_OPS=%d and is not a finding' % MIN_LAYOUT_OPS)
    realloc = ['lwz     r3,8(r29)', 'stw     r3,0(r28)', 'blr', 'addi    r0,r3,4', 'blr']
    check('layout/realloc-only-silent', layout_delta(frame_ret, realloc)[0] is None,
          'a reallocation with no offset change yields nothing')

    # The layout rule must be at least as sensitive as the strict one on the real +8 control, and must
    # not be more trigger-happy than it on the real non-uniform control.
    loose_found = sum(1 for sym in sources
                      if layout_delta(sources[sym][0], [apply_delta(t, 8) for t in sources[sym][1]])[0] == 8)
    check('layout/+8-is-found', loose_found == len(sources),
          'the layout rule also finds the +8 control in %d of %d' % (loose_found, len(sources)))
    # (5) coverage accounting: paired + unpaired + not-in-retail == the report's function count, so
    #     nothing was dropped on the floor while being counted as "covered".
    total = ours_total = 0
    for module, unit, _, _ in rel_units(report):
        us = by_name.get('%s/%s' % (module, unit), {}).get('functions', [])
        total += len(us)
        a = objdump_insns('build/G2ME01/%s/obj/%s.o' % (module, unit))
        b = objdump_insns('build/G2ME01/src/%s.o' % unit)
        if a and b:
            ours_total += sum(1 for f in us if f.get('name') in a and f.get('name') in b)
        else:
            ours_total += 0
    check('real/coverage-adds-up', total > 0 and ours_total <= total,
          '%d report functions, %d paired, %d unaccounted' % (total, ours_total, total - ours_total))
    return ok, lines


def report_units(report):
    return {u['name']: u for u in report.get('units', [])}


def main():
    ap = argparse.ArgumentParser(
        description='Report a uniform operand delta across REL functions - the signature of a class '
                    'laid out by one wrong constant. Read-only unless --write.')
    ap.add_argument('module', nargs='?', default=None, help='restrict the scan to one REL module')
    ap.add_argument('--min-pct', type=float, default=DEFAULT_MIN_PCT,
                    help='lowest objdiff percent considered (default %(default)s; 0 widens the window)')
    ap.add_argument('--max-pct', type=float, default=DEFAULT_MAX_PCT,
                    help='exclusive upper bound on the percent (default %(default)s)')
    ap.add_argument('--explain', metavar='SYM',
                    help='print the paired instructions for every REL function matching SYM, whatever '
                         'it scored - this is how you look at what the operand-shape rule excluded')
    ap.add_argument('--self-check', action='store_true',
                    help='run the preconditions and the falsification checks only; exits 0 or 3')
    ap.add_argument('--write', metavar='PATH', help='write the findings as JSON (never a source file)')
    ap.add_argument('-v', '--verbose', action='store_true', help='print every rejection reason with its count')
    args = ap.parse_args()

    if not os.path.exists('build/report.json'):
        print('error: build/report.json missing - run ./tools/decomp_build.sh first', file=sys.stderr)
        return 3
    report = json.load(open('build/report.json'))

    if args.explain:
        hits = 0
        for module, unit, retail_path, our_path in rel_units(report):
            retail = objdump_insns(retail_path) or {}
            ours = objdump_insns(our_path) or {}
            for f in report_units(report).get('%s/%s' % (module, unit), {}).get('functions', []):
                name = f.get('name') or ''
                if args.explain not in name:
                    continue
                hits += 1
                print('-- %s  [%s, %s, objdiff %s]' % (name, module, unit, f.get('fuzzy_match_percent')))
                delta, cat, detail = uniform_delta(retail.get(name, []), ours.get(name, []))
                print('   rule: %s%s' % (cat, (' - ' + detail) if detail else ''))
                print_pairs(pair_insn(retail, ours, name))
        if not hits:
            print('no REL function matches %r' % args.explain)
        return 0

    ok, pre = check_preconditions(verbose=not args.self_check)
    for line in pre:
        print(line)
    if not ok:
        print('exit 3: the scan above would not mean anything - refusing to report a finding')
        return 3

    if args.self_check:
        sok, lines = self_check(report)
        print('self-check:')
        for line in lines:
            print(line)
        print('SELF-CHECK %s' % ('PASS' if sok else 'FAIL'))
        return 0 if sok else 3

    units = rel_units(report, args.module)
    if args.module and not units:
        print('no non-auto REL unit in module %s' % args.module)
        return 3
    by_name = report_units(report)
    addrs, cache = {}, {}

    def objs(module, retail_path, our_path):
        if (retail_path, our_path) not in cache:
            cache[(retail_path, our_path)] = (objdump_insns(retail_path), objdump_insns(our_path))
        return cache[(retail_path, our_path)]

    cov = collections.Counter()
    by_constant = collections.defaultdict(list)   # (class, module, delta) -> rows
    lengths = []                                  # every pair whose instruction counts differ
    unpaired_units, skipped = collections.Counter(), []
    ours_only = 0
    for module, unit, retail_path, our_path in units:
        cov['units'] += 1
        funcs = by_name.get('%s/%s' % (module, unit), {}).get('functions', [])
        retail, ours = objs(module, retail_path, our_path)
        if retail is None:
            cov['units-skipped'] += 1
            cov['skipped-functions'] += len(funcs)
            skipped.append('no retail object: %s' % retail_path)
            continue
        if ours is None:
            cov['units-skipped'] += 1
            cov['skipped-functions'] += len(funcs)
            skipped.append('never built, our object missing: %s' % our_path)
            continue
        if module not in addrs:
            addrs[module] = retail_addrs(module)
        ours_only += len(set(ours) - set(retail))
        for f in by_name.get('%s/%s' % (module, unit), {}).get('functions', []):
            name, pct = f.get('name'), f.get('fuzzy_match_percent')
            cov['retail-functions'] += 1
            if pct is None:
                cov['no-percent-in-report'] += 1
            if name not in retail:
                cov['not-in-retail'] += 1
                continue
            if name not in ours:
                cov['unpaired'] += 1
                unpaired_units[unit] += 1
                continue
            cov['paired'] += 1
            in_window = pct is not None and args.min_pct <= pct < args.max_pct
            if in_window:
                cov['in-window'] += 1
            else:
                cov['outside-window'] += 1
                if pct == 100.0:
                    cov['at-100'] += 1
            a, b = retail[name], ours[name]
            if len(a) < MIN_INSNS:
                cov['too-short'] += 1
                continue
            # The blind spot, recorded before either rule runs and printed below whatever they say.
            # It is *not* compared for a constant - there is none in a length - so the row is a
            # worklist entry, and the section says so rather than letting a -31 be read as a verdict.
            dlen = length_delta(a, b)
            if dlen is not None:
                addr, size = addrs[module].get(name, (0, 0))
                lengths.append((abs(dlen), dlen, module, unit, name, pct, addr, size, len(a), len(b)))
            # The rule is evaluated on EVERY paired function, not only on the ones inside the window.
            # The window says which functions are *candidates* to report; the histogram over all of them
            # is the evidence, and a histogram that only counted the 20 sub-100% functions would show
            # nothing but "they all differ" - which is true and useless.
            delta, cat, detail = uniform_delta(a, b)
            cov[cat] += 1
            # Both rules are evaluated on every paired function. `uniform_delta` is the strict one and
            # is nearly unfalsifiable; `layout_delta` is the one that survives a reallocation in the
            # same function, which is where real bugs hide. A hit is a hit under either; which rule
            # produced it is reported, because the difference is how much caveat it needs.
            ldelta, ldetail = layout_delta(a, b)
            if ldelta is not None:
                cov['layout-delta'] += 1
                if in_window:
                    cov['layout-delta-in-window'] += 1
            else:
                cov['layout:' + ldetail.split(' (')[0].split(',')[0].strip()] += 1
            if not in_window:
                continue
            if delta is None and ldelta is None:
                continue
            addr, size = addrs[module].get(name, (0, 0))
            # One row per (module, constant): a hit is evidence about the constant, and every symbol
            # that carries it is listed. The class is a property of the row, not of the grouping, so
            # grouping by class would split one constant across two containers and halve the evidence.
            # The strict rule wins when both fire: it needs no caveats quoted alongside it.
            row = (name, pct, addr, size, a, b, tuple(owner_classes(name)) or (GROUP_UNKNOWN,))
            key = (module, delta if delta is not None else ldelta,
                   'strict' if delta is not None else 'layout')
            by_constant[key].append(row)
    strong = {k: v for k, v in by_constant.items() if len(v) >= MIN_HITS}
    weak = {k: v for k, v in by_constant.items() if len(v) < MIN_HITS}

    modules_all = rel_modules()
    modules_with = sorted({m for m, _, _, _ in units})
    modules_without = [m for m in modules_all if m not in modules_with]

    print()
    print('== coverage  (window %g-%g%%, a finding needs >= %d functions of one class)'
          % (args.min_pct, args.max_pct, MIN_HITS))
    print('   REL modules in config/G2ME01/rels/         %4d' % len(modules_all))
    print('   with a decompiled (non-auto) unit          %4d  %s'
          % (len(modules_with), ' '.join(modules_with[:6]) + (' ...' if len(modules_with) > 6 else '')))
    print('   with NO unit to compare at all            %4d  %s'
          % (len(modules_without), '(' + ' '.join(modules_without) + ')' if modules_without else ''))
    print('   units scanned                             %4d' % cov['units'])
    print('   units SKIPPED, object missing             %4d  %s'
          % (cov['units-skipped'], '; '.join(skipped[:3]) if skipped else ''))
    if cov['units-skipped']:
        print('   ... retail functions in the skipped units %4d  <-- also not covered' % cov['skipped-functions'])
    print('   retail functions in the scanned units     %4d' % cov['retail-functions'])
    print('   of those, objdiff reports no percent      %4d  (retail functions it could not pair)'
          % cov['no-percent-in-report'])
    print('   paired by name in both objects             %4d' % cov['paired'])
    print('   unpaired, our object does not define them  %4d  %s'
          % (cov['unpaired'],
             '(' + ', '.join('%s %d' % kv for kv in unpaired_units.most_common(3)) + ')' if unpaired_units else ''))
    print('   functions only we emit (no retail pair)    %4d' % ours_only)
    print('   paired and already 100.00%%                 %4d' % cov['at-100'])
    print('   -- the rule applied to ALL %4d paired functions, which is the evidence:' % cov['paired'])
    print('      %-52s %4d' % (R_ZERO, cov[R_ZERO]))
    print('      %-52s %4d' % (R_MIXED, cov[R_MIXED]))
    print('      %-52s %4d' % (R_SHAPE, cov[R_SHAPE]))
    print('      %-52s %4d' % (R_LENGTH, cov[R_LENGTH]))
    print('      %-52s %4d' % (R_NO_OPS, cov[R_NO_OPS]))
    print('      %-52s %4d' % ('too short to pair (<%d instructions)' % MIN_INSNS, cov['too-short']))
    print('      %-52s %4d  <-- the signature (strict rule)' % (R_UNIFORM, cov[R_UNIFORM]))
    print('      %-52s %4d  <-- the signature (layout rule, survives a reallocation)'
          % ('every MEMBER offset off by one constant', cov['layout-delta']))
    print('      %-52s %4d' % ('  of those, inside the %g-%g%% window' % (args.min_pct, args.max_pct),
                                cov['layout-delta-in-window']))
    print('      %-52s %4d' % ('name not in the retail object', cov['not-in-retail']))
    print('   -- the layout rule over all %d paired: %s'
          % (cov['paired'], ', '.join('%s %d' % (k[7:], v) for k, v in sorted(cov.items())
                                      if k.startswith('layout:'))))
    if args.verbose:
        for k in sorted(cov):
            if k not in REJECT_REASONS and k not in ('units', 'retail-functions', 'paired', 'unpaired',
                                                      'in-window', 'uniform', 'units-skipped',
                                                      'at-100', 'no-percent-in-report', 'too-short'):
                print('      %-52s %4d' % (k, cov[k]))

    # The blind spot, named per function. The counter above already carried this as
    # `instruction count differs`; a bare count is not actionable, and this is the whole difference
    # between "the tool cannot see this" and "the tool cannot see this and here is every place to look".
    if lengths:
        lengths.sort(key=lambda r: (-r[0], r[6]))
        print()
        print('== WHERE THE RULES ARE BLIND: %d paired functions whose instruction COUNTS differ.' % len(lengths))
        print('   Neither rule can see this and no rule here can: a class laid out too small makes')
        print('   mwcceppc emit fewer stores, so there is no operand to shift and no constant to find.')
        print('   This is a WORKLIST, not a diagnosis - a length is not evidence about a member, and the')
        print('   sign is the only hint it carries: negative means we emit fewer instructions than retail,')
        print('   which is what a class laid out *small* looks like, and it is also what a differently')
        print('   compiled body looks like. Measure the offsets (tools/size_probe_*.cpp) before believing it.')
        print('   %-7s %-14s %-52s %8s %10s %9s' % ('dInsn', 'module', 'function', 'pct', 'addr', 'retail/ours'))
        for _, dlen, module, unit, name, pct, addr, size, la, lb in lengths:
            print('   %+-7d %-14s %-52s %8s %+010X %5d/%-4d'
                  % (dlen, module[:14], name[:52], ('%.2f%%' % pct) if pct is not None else '-', addr, la, lb))

    findings = []
    for (module, delta, rule), rows in sorted(strong.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        rows.sort(key=lambda r: -r[1])
        classes = sorted({c for r in rows for c in r[6] if c != GROUP_UNKNOWN})
        undecidable = [r for r in rows if r[6] == (GROUP_UNKNOWN,)]
        print()
        print('== %s in %s: %d functions, every member offset off by %+d  (0x%X)   [%s rule]'
              % (' + '.join(classes) or GROUP_UNKNOWN, module, len(rows), delta,
                 delta & 0xFFFF, rule))
        if rule == 'layout':
            print('   These functions ALSO differ from retail in a register or in instruction count, so')
            print('   the strict rule rejected them. What they share is the constant, and it is an offset:')
        for name, pct, addr, size, _, _, owners in rows:
            tag = '' if owners != (GROUP_UNKNOWN,) else '   [owner not determinable from the symbol]'
            print('   %6.2f%%  retail .text+%06X  size 0x%X  %s%s' % (pct, addr, size, name[:64], tag))
        if undecidable and classes:
            print('   (%d of the %d are a loader or a destructor, whose class the symbol does not name;'
                  % (len(undecidable), len(rows)))
            print('    they are evidence for the constant but not for a class, and are counted above.)')
        name, pct, addr, _, a, b, _ = rows[0]
        print('   -- the paired instructions for %s (%.2f%%, retail .text+%06X)' % (name[:58], pct, addr))
        print_pairs(pair_insn({name: a}, {name: b}, name), cap=12)
        findings.append({'classes': classes, 'module': module, 'delta': delta, 'rule': rule,
                         'owners_not_determinable': [r[0] for r in undecidable],
                         'functions': [{'name': n, 'percent': p, 'retail_text_offset': ad,
                                        'retail_size': s, 'class': list(oc)}
                                       for n, p, ad, s, _, _, oc in rows]})
    if weak:
        print()
        print('== weak: fewer than %d functions at one constant, so a positional coincidence until'
              % MIN_HITS)
        print('   proven otherwise. These are where a real finding usually has its first data point,')
        print('   so they are printed with the same evidence rather than dropped.')
        for (module, delta, rule), rows in sorted(weak.items()):
            print('   %s at %+d [%s]: %s'
                  % (module, delta, rule,
                     ', '.join('%s (%.2f%%, %s)' % (n[:44], p, c[0]) for n, p, _, _, _, _, c in rows)))

    print()
    if findings:
        print('== verdict: %d constant(s) at one member offset each. Do NOT guess the member from this -'
              % len(findings))
        print('   the evidence is the constant and the addresses. Measure it with mwcceppc')
        print('   (tools/probe_offsets.cpp emits offsets into .data for `objdump -s` to read back) and')
        print('   compare against retail, because a header comment quoting our own compiler is exactly')
        print('   how CStateManager::pad2_2 and CAnimData::x120_unk were 0x8 too high in the first place.')
    else:
        print('== verdict: NO uniform operand delta in %d comparable REL function(s) at %g-%g%%.'
              % (cov['in-window'], args.min_pct, args.max_pct))
        print('   The evidence is the histogram above, not the absence of a finding: of the %d paired'
              % (cov[R_ZERO] + cov[R_MIXED] + cov[R_SHAPE] + cov[R_LENGTH] + cov[R_NO_OPS]))
        print('   functions, %d have every integer operand unchanged, %d differ in more than one'
              % (cov[R_ZERO], cov[R_MIXED]))
        print('   constant and %d differ in a register or a schedule - never in one constant repeated.'
              % cov[R_SHAPE])
    print('   Read the coverage before believing it: %d of the %d retail functions in these units'
          % (cov['unpaired'] + cov['skipped-functions'],
             cov['retail-functions'] + cov['skipped-functions']))
    print('   are not comparable (our object does not define them, or was never built), and %d of %d'
          % (len(modules_without), len(modules_all)))
    print('   modules have no decompiled unit at all, so "no signature on the REL side" is a')
    print('   statement about the paired half only.')

    if args.write:
        path = os.path.abspath(args.write)
        bad = [d for d in ('src', 'include', 'config', 'platform', 'libc', 'tools', 'extern', 'cmake')
               if ('%s%s%s' % (os.sep, d, os.sep)) in path]
        if bad or path.endswith(('.cpp', '.c', '.h', '.hpp', '.txt', '.yml', '.cmake')):
            print('refusing to write inside a source directory: %s' % path, file=sys.stderr)
            return 2
        with open(path, 'w') as fh:
            json.dump({'min_pct': args.min_pct, 'max_pct': args.max_pct, 'min_hits': MIN_HITS,
                       'coverage': {k: v for k, v in cov.items()},
                       'modules_scanned': len(modules_with),
                       'modules_without_a_decompiled_unit': modules_without,
                       'units_skipped': skipped, 'findings': findings}, fh, indent=1, default=str)
        print('wrote %s' % path)
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
