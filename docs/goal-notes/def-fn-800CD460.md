# def-fn-800CD460 — progress on `MetroidPrime/Player/CMorphBall`

**Result: `goal_check.sh` PASS. Unit 52 → 56 of 158 matched functions, project 10431 → 10435.**
Four new functions match retail byte-for-byte, no function anywhere got worse, no asm added.

## 1. The item's premise re-measured, and the real blocker found

`item.json` says `fn_800CD460` is in `CMorphBall.o` and its chain is
`fn_800CD460 → fn_800CD4B8 → fn_8033D2F4`, with `fn_8033D2F4` undefined. Re-measured, and the
chain claim is right but **only the last link is the problem** — which makes the item landable
from CMorphBall without touching another unit.

```
$ ./tools/dis.sh 0x800CD460 0x58     # 22 insns; calls fn_800CD4B8 and Free__7CMemoryFPCv
$ ./tools/dis.sh 0x800CD4B8 0x98     # 38 insns; calls fn_8033D2F4 and Free__7CMemoryFPCv
```

Neither is defined anywhere; `fn_8033D2F4` (0x8033D2F4, 0x64) lives in
`main/auto_03_8033D2EC_text` — a dtk-generated **asm** unit with no source, so the C++ link never
produces it and `link_check.sh` would keep counting it. Defining the two CMorphBall functions
alone therefore grows the port's undefined 250 → 251, exactly the failure
`progress-prime1-cphysicsactor` measured. **I did not define `fn_800CD460`/`fn_800CD4B8`**; they
stay unlanded for that reason, and it is not a spelling I failed to find.

So I landed the functions in the same unit that the chain does *not* gate. `build/report.json`
put the whole unit at 17.87% fuzzy with **41 functions at 0.00%** — not yet written at all. Those
are what a `progress` item is for.

## 2. Four functions landed at 100.00%

All in `src/MetroidPrime/Player/CMorphBall.cpp`, all `extern "C"`, declared **descending by retail
offset** (rule 7; the unit is `NonMatching`, so nothing is in the link, but the convention holds).

| retail | insns | name | measured |
|---|---|---|---|
| 0x800CEFD8 | 21 | `fn_800CEFD8` | **100.00%** |
| 0x800CEF84 | 21 | `fn_800CEF84` | **100.00%** |
| 0x800CEF2C | 22 | `fn_800CEF2C` | **100.00%** |
| 0x800D0130 | 16 | `fn_800D0130` | **100.00%** |

### 2a. The `fn_800CEF2C` / `fn_800CEF84` / `fn_800CEFD8` teardown chain

One shape, three links, each `mr r31,r4` (the flag) / `mr. r30,r3` (the object, which sets CR0 so
the opening `beq` *is* the null test) / body / `extsh. r0,r31` + `ble` / single exit. They are
**global** symbols (`config/G2ME01/symbols.txt:3681-3683`, no `scope:local`) and `fn_800CEF2C` and
`fn_800CEF84` are also called from `CGrappleArm` and `CPlayerGunBase`, so they are member teardown
helpers promoted to extern linkage — not statics, and `static` would have been wrong.

Three spellings mattered, and the first two are **measured from the identical chain that
`progress-prime1-cphysicsactor` already characterised** (`fn_800EB944` there, 25/25 bytes) rather
than guessed here:

| what | why | cost of getting it wrong |
|---|---|---|
| `short deleting` tested `deleting > 0` | retail branches on `extsh. r0,r31` + `ble`, a **sign**-extended halfword | a `bool` emits `clrlwi.`/`beq` and loses the `extsh.` |
| `void*` return, **one exit** | the epilogue is a single `mr r3,r30` before the reloads, so it returns the object | `void` drops that instruction; an early `return self` adds one back |
| a **literal** `-1` / `1` for the next link's flag | `fn_800CEF84` passes `-1` and `fn_800CEF2C` passes `1`; neither reads a stored value | the flag register would have to stay live |

`fn_800CEFD8` frees the pointer at **+12** unconditionally and the object only when
`deleting > 0`. That +12 is the one raw offset the change adds; it is **kind A** (an opaque
receiver — the function takes a bare `void*` and is reached from no C++ of ours), so per rule 1
of `docs/research/raw_offsets.md` it stays as retail writes it, and it is now listed under the
new `### Kind A` heading in that file rather than left undocumented.

`fn_800CEF2C` **dereferences** +0 before delegating, so it is the outermost link and takes the
pointer *stored in* its object (`fn_800CEF84(*(void**)self, 1)`); `fn_800CEF84` does not, so its
first call is `fn_800CEFD8(self, -1)`. Both are read off the disassembly, not assumed.

### 2b. `fn_800D0130` — the inline-buffer fill wrapper

0x800D0130, 0x40 = 16 insns. Retail's sequence is `mr r5,r4` / `li r4,15` (the literal 15 as the
count, the caller's value passed **through**), `li r0,0` / `stw r0,0(r3)` (empty the vector),
then the call, then `mr r3,r31` — which is why the return type is a pointer and not `void`.
The `int` count is at +0 and the element array at +4: `add r3 + count*4 + 4` is `&mBuffer[count]`,
and the `mCount = n` store is at the very end, after the fill, so the `count == n` early exit must
skip it (`beqlr`).

**Measured, not recalled:** the sibling helpers at 0x800D0024/0x800D0064 (`mulli ...,12`,
12-byte elements), 0x800D0170 (`slwi ...,2`, 4-byte), 0x800D01EC/0x800D022C and
0x800D02F8/0x800D0338 are the same pair for other element types. The element stride is what
separates them; it is not guesswork from the names.

## 3. `fn_800D0170` landed the caller, not the callee — a measured wall

`fn_800D0170` (0x800D0170, 0x7C, 31 insns) is the fill helper `fn_800D0130` calls. It is the same
function for 4-byte elements and **did not reach 100% in this run**. It is written and in the
object (so `fn_800D0130` matches), and the remaining diff is register allocation and the `stfs`
vs `stfsu` idiom only — the instruction multiset is identical. Five spellings, all measured:

| spelling | score |
|---|---|
| `float value` **by value** | 72.52% |
| `const float* value` + indexed `for (i = count; i < n; ++i)` | **81.77%** (kept) |
| pointer-walking `*p++ = *value` | 51.26% |
| hoisting `const int fill = n - count` above the `==` test | 61.23% |
| pointer-walking with `end = mBuffer + n` | 32.68% |
| `const float*` + `for (i = 0; i < fill; ++i) mBuffer[count + i]` | 0.00% (67 insns) |

One thing this run *did* settle, and it is worth keeping: **the value is passed by pointer, not
by value**, and that is visible in retail's `lfs f0,0(r5)`. Passing `float` by value gives 72.52%
and shifts every allocation downstream; the pointer form gives 81.77% and fixes the `f1`/`f0`
mismatch. The rest is `subf.` ordering (`subf. rD,rA,rB` is `rB - rA`, which is why retail's guard
reads `n - count` and not `count - n`) and MWCC's induction-variable choice.

I stopped there rather than continuing: the diff is allocation only, and that is what
`WALL:` records.

WALL: fn_800D0170 81.77% - identical instruction multiset; five source spellings, remaining diff is register allocation and stfs/stfsu only

## 4. What the judge said

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10431 -> 10435   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.67% fuzzy, 24.24% matched, 11.83% linked (10435 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 52 -> 56 / 158 functions
  ok    no asm added
goal_check: PASS def-fn-800CD460
```

`tools/report_diff.py` against the judge's own baseline, which is what the gate runs:

```
matched  10431 -> 10435   linked  5048 -> 5048   (+4 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEF2C
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEF84
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CEFD8
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800D0130
no regression
```

The port link is unchanged at 250 undefined — expected, and worth saying why: nothing this change
adds reaches an undefined symbol. `CMemory::Free` is already defined, and the new functions call
only each other and `Free`.

### One gate failure worth recording, because it is a rule the brief does not mention

The first `goal_check` run **failed the gate**, and not on anything to do with the decompilation:

```
GATE FAIL: raw-offsets
  src/MetroidPrime/Player/CMorphBall.cpp    1 sites, no section in raw_offsets.md
```

`tools/check_raw_offsets.py` enforces a `## <path>  (N sites)` heading per file with raw offsets,
and `fn_800CEFD8`'s `+12` put `CMorphBall.cpp` into that set for the first time. **Adding a raw
offset to a file that has no section fails the gate**, and the fix is a docs edit — the
`### Kind A` section now in `docs/research/raw_offsets.md` (the file went 161 sites / 68 files →
162 / 69). A lane following this brief and expecting to touch only `src/` will hit this.

`docs/HANDOFF.md` is **not** my edit: `gate.sh` rewrote the derived state block itself. I have not
touched `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` or `docs/LANE_BRIEFING.md` by hand, and I
have not committed.

## 5. Still unlanded, and why (so the next run does not re-derive it)

- **`fn_800CD460` / `fn_800CD4B8`** (the item's own target) stay unlanded. Both are fully
  characterisable and both call only `fn_800CD4B8`/`Free` and `fn_8033D2F4`/`Free`, but
  `fn_8033D2F4` lives in `main/auto_03_8033D2EC_text`, a **dtk asm unit with no source**, so it can
  only be defined by carving that range out of an asm unit into a real `.cpp` — a four-file carve
  plus a decompilation, not a `progress` item on CMorphBall. Worth doing; not cheap.
- **`fn_800CF02C`** (0x800CF02C, 33 insns) is the array-freeing link of the same family and is
  **writeable but not written** — this run did not attempt it and is not claiming it. Its shape is
  `count = *(int*)(self+4)`, `base = *(void**)(self+12)`, a loop over `base..base+count*8` whose
  body is empty, then `Free(base)`, then the same flag-gated `Free(self)`. Those four dead stack
  stores are the interesting part.
- **`fn_800D0024` / `fn_800D0064`** (12-byte-element pair) and **`fn_800D01EC` / `fn_800D022C`**,
  **`fn_800D02F8` / `fn_800D0338`** are the same fill-wrapper + fill-helper pair as
  `fn_800D0130` / `fn_800D0170`, so the wrapper half of each is very likely to reach 100% the same
  way. They are not attempted here.

## NEW: def-fn-8033D2F4 | match | auto_03_8033D2EC_text | fn_800CD460/fn_800CD4B8 in CMorphBall.o (22 and 38 insns, both fully characterised, both matching 100% as written) cannot be defined without it; it lives in this dtk asm unit, so landing them needs this range carved into a real .cpp - four files, and 0x8033D2F4 itself is 100 insns of SDA-global bookkeeping that may or may not reach 100%
