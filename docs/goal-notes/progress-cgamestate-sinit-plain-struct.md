# progress-cgamestate-sinit-plain-struct

Lane 7, `wt-mp2-goal-L7`, head `cbcd419`. `__sinit_CGameState_cpp` is **byte-exact**: 63.35% ->
**100.00%**, and the unit's matched count rose **102 -> 103 / 116**.
`./tools/goal_check.sh build/goal/item.json` prints `PASS`. No commit.

## What the item's premise was, and what it was missing

`item.json`'s `reason` (from `progress-cgamestate-addvariable-regs`) said the plain struct gets
`__sinit_CGameState_cpp` to 19/20 instructions and that *"the one remaining gap is that retail
spends a separate `addi` to materialise the table's own address where we fold the low half into
the first `stwu`"*, and called that *"a wall, not a spelling I failed to find"*.

It is not a wall. The gap is one word of **top-level `const` on the by-value pointer parameter**:

    -  SGameModeLayer(const char* a, uint b)      -> 19 instrs / 76B / 18 of 20 bytes differ
    +  SGameModeLayer(const char* const a, uint b) -> 20 instrs / 80B /  4 of 20 bytes differ

The 4 remaining byte differences are the un-relocated `lis`/`addi` immediates in the `.o`
(`lis r3,0` + `R_PPC_ADDR16_HA` vs the linked `lis r3,-32709`); the **relocation pattern is
identical to retail's, instruction for instruction and register for register**.

## Why the `const` changes the code (measured, not inferred)

Retail's object (`build/G2ME01/obj/MetroidPrime/Player/CGameState.o`, `__sinit` at 0x80146874):

    +0x00 lis  r3,0        R_PPC_ADDR16_HA lbl_803A9208      (string pool base)
    +0x04 lis  r6,0        R_PPC_ADDR16_HA lbl_80410998      (the table)
    +0x08 addi r10,r3,0    R_PPC_ADDR16_LO lbl_803A9208
    +0x0C lis  r5,17492 ... +0x14 lis r3,17231              (hi of 'DTHM' 'SNGL' 'COIN')
    +0x18 addi r9,r10,42
    +0x1C addi r8,r6,0     R_PPC_ADDR16_LO lbl_80410998
    +0x20 addi r7,r5,18509  +0x24 addi r6,r10,440
    +0x28 addi r5,r4,18252  +0x2C addi r4,r10,448  +0x30 addi r0,r3,18766
    +0x34 .. +0x48 six plain stw off r8, then blr

Without the `const`, ours emits 19 instructions: the register holding the table's
`ADDR16_HA` dies at its `ADDR16_LO` addi, so the store-with-update pass folds the pair into
`stwu r8,0(r6)` and the remaining five words go off the write-back register. Retail keeps that
register live - **it is reused at +0x24 for `addi r6,r10,440`** - so the low half survives as a
separate `addi r8,r6,0` and all six stores are plain `stw`s. The `const` on the by-value
parameter is what changes the allocator's decision. That the fold is real and not a flag
mismatch: retail's own object has 57 `addi rA,rB,0` register copies and **none** of them is
followed by `stw ...,0(rA)` - the pass is consistently applied everywhere else in the same unit,
so `__sinit` is the exception, not the rule.

The first half of the item's premise was right and is kept: `rstl::pair`'s ctor takes
`const L&, const R&`, so each `'DTHM'` becomes an object needing an address and mwcceppc loads
the three constants from a `.sdata` literal pool with `lwz ...,0(0)` + `R_PPC_EMB_SDA21`. Retail
materialises them with `lis`+`addi` because the constant is an immediate in the constructor's
call. By value fixes that (63.35% -> 80.80%); the `const` fixes the rest (80.80% -> 100.00%).

## Spellings measured this run (all 19 instrs / 76B / 18-of-20 unless noted)

`struct SGameModeLayer { const char* first; uint second; ... }` as the element type, with:

| variant | result |
|---|---|
| `(const char*, uint)` by value, init list | 19 / 76B - the plateau the item was filed on |
| `(const char* const, uint)` by value, init list | **20 / 80B - 100.00%** |
| ctor body `{ first = a; second = b; }` | 19 / 76B |
| `char const*` / `uint` / `int` / `unsigned` variants of the parameters | 19 / 76B |
| `u32` instead of `uint` for `second` | 19 / 76B |
| `(const uint)` spellings of `'DTHM'` as `0x4454484D` | 19 / 76B |
| explicit bound `[3]`, `const` array, 2-D `[1][3]`, wrapper struct, typedef | 19 / 76B |
| class instead of struct, user `operator=`, user copy ctor, derived from `rstl::pair` | 19 / 76B |
| default argument `uint b = 0` | 19 / 76B |
| **user destructor** | 21 / 84B - adds a destructor registration |
| external linkage (no `static`) | 19 / 76B - the symbol stays `.bss`, unchanged |
| 4th duplicate element | 21 instrs |
| mem-init-list array member `mLayers{...}`, union element, aggregate-with-user-ctor | compile error |

The `rstl::pair` spelling, for the record: 17 instrs / 68B / 63.35%.

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp` **lines 796-825** - the element type of
  `sGameModeLayers` plus a comment recording the two mechanisms above. Nothing else in the tree.
  `fndiff` confirms `__sinit_CGameState_cpp` is the **only** function in the object whose bytes
  change (68 -> 80); `.sdata` shrinks 35 -> 20 bytes because the three pool constants are gone,
  and no other function moves. `docs/HANDOFF.md` is rewritten by `tools/gate.sh` (the state block
  10065 -> 10066); reverted, as the driver owns it.

## Verification

    $ ./tools/decomp_build.sh main/MetroidPrime/Player/CGameState
    All:  31.00% fuzzy, 23.30% matched, 11.78% linked (10066 / 28465 functions)
    main/MetroidPrime/Player/CGameState: 88.01% fuzzy, 61.04% matched (103 / 116 functions)
       - `__sinit_CGameState_cpp` is no longer in the sub-100% list, i.e. 100.00%

    $ sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010      (the pinned hash)
    $ python3 tools/check_symbol_names.py
    checked 504 units; 0 declared names are missing from their object
    $ python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState
    ok: 4 unit(s) checked, none emits its functions out of retail order
    $ ./tools/unit_fit.sh MetroidPrime/Player/CGameState.cpp
    no extra functions: our object defines only what the retail unit object does
    $ ./tools/goal_check.sh build/goal/item.json
    ok  counts: matched 10065 -> 10066   linked 4918 -> 4918
    ok  target rose: main/MetroidPrime/Player/CGameState: 102 -> 103 / 116 functions
    ok  no asm added
    goal_check: PASS progress-cgamestate-sinit-plain-struct

A per-function diff of `build/report.json` against `build/goal/judge/report.base.json`: **1 better,
0 worse, 0 added, 0 removed** - only `__sinit_CGameState_cpp` 63.35 -> 100.00.

## A trap that cost most of this run: `readlines(True)` is not `readlines()`

`IOBase.readlines(hint)` takes a **byte hint**. On this tree's Python 3.14.4,
`open(f).readlines(True)` == `readlines(1)` and returns **one line**, so

    original = open(path).readlines(True)
    open(path, "w").write("".join(original[:795]) + new + "".join(original[801:]))

silently rewrote a 1456-line source as a **339-byte** file. Two measurements taken that way read
"0/116 functions, 0.35% fuzzy" and looked like a catastrophic regression of the plain struct. It
was a 339-byte file that mwcceppc could not compile - and **mwcceppc then writes a 1080-byte stub
object and returns 0**, so ninja reports the build as successful and `tools/fast_try.sh` feeds
that stub to objdiff. Two defences, both in gitignored `.tmp/opencode/`:
`patch.py` uses `open(path).read().splitlines(True)` and prints the result's size, line count and
sha; `sweep2.py` and `cand.sh` refuse any object under 4096 bytes. A scratch source must also keep
the basename `CGameState.cpp`, because the static-init function is named after it
(`__sinit_<basename>_cpp`).

Also worth recording: the previous note's `probe_gs.sh` (0.32 s per compile) and `raw.py` are
still good and were verified byte-identical to ninja's object; `sweep.py`'s in-place edit/restore
loop is *not*, and `sweep2.py` replaces it with a scratch copy that never touches the tree.

## New queue items

NEW: progress-cgamestate-absent-functions | progress | MetroidPrime/Player/CGameState | three
functions objdiff scores 0.00% are **absent from the object** - `powerpc-eabi-objdump` finds no
such symbol, so they are 0% for want of a body and they are the largest remaining count in the
unit: `fn_801465EC` (0x801465EC, 264 bytes) is a vector growth - capacity test against the count
at +8, `bl allocate__Q24rstl17rmemory_allocatorFi`, then an inlined copy as an 8-byte `bdnz`
chunk loop with a byte tail, i.e. `resize`/`reserve` on `rstl::vector`;
`fn_80146338` (0x80146338, 440 bytes) is a `stmw r26` frame that zeroes r4-r7 and calls
`fn_80008CE0` when the third argument is 0, 6 `R_PPC_REL24` calls total; and
`LoadGameFileState__10CGameStateFPCv` (488 bytes), retail's memcard load. Each needs a source
body and an `extern "C"` claim in the same style as `fn_80143CD4` (see the note above
`fn_80144818` in the source for why retail's unnamed symbols have to be claimed by name).
