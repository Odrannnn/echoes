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

---

# Second attempt (lane 2, `wt-mp2-goal-L2`, head `fcb6470b`)

## STALE: the item's premise is already landed on this tree

`__sinit_CGameState_cpp` is **already 100.00%** at HEAD and in the judge baseline the driver
recorded for this item (`build/goal/judge/report.base.json`, written 19:02 from `fcb6470b`), and
`src/MetroidPrime/Player/CGameState.cpp:922` already carries Lane 7's `SGameModeLayer(const char*
const a, uint b)`. The unit was at **107 / 116** before I touched anything, so the item was
requeued without the function it names. Since the item's *target* is the unit, I spent the run on
the remaining eight unmatched functions instead. Measured, not recalled.

## Result: +1 matched function, `goal_check: PASS`

`__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` **97.169014% -> 100.00%**;
`MetroidPrime/Player/CGameState` **107 -> 108 / 116**; global matched **13053 -> 13054**.

One word in the mem-init list of `CWorldState::CWorldState(CBitStreamReader&, CAssetId, const
CWorldSaveGameInfo&)` (`src/MetroidPrime/Player/CGameState.cpp:576`):

    - , mAreaId(kInvalidAreaId)      -> li r7,0 + lwz r5,0(r0)/R_PPC_EMB_SDA21 + stw r5,4(r29)
    + , mAreaId(TAreaId(-1))         -> li r7,-1 + li r5,0 + stw r7,4(r29)

Everything else in the function was already retail's; those four instructions at +0x08 and
+0x40..+0x4C were the whole 2.83%.

## The rule this is worth, because it is not specific to this function

**An `extern const` named in a constructor's mem-init list is not the same code as the literal it
holds.** `kInvalidAreaId` is `extern const TAreaId` (`include/MetroidPrime/TGameTypes.hpp:15`), so
naming it emits `lwz rX,disp(r0)` + `R_PPC_EMB_SDA21`; retail *materialises* the value (`li r7,-1`)
in the prologue and reuses that register for the store, which also frees the second `li r5,0` that
retail spends zeroing the next member. Same value, same member - only the spelling moves the code,
and the score with it. Worth grepping for every `extern const` initialiser in a constructor whose
function sits just under 100%: `kInvalidAssetId` is still spelled that way at lines 570, 650, 651,
735 and 736 of this file (those functions are 84-96% for other reasons, so they are not evidence
either way).

## Negative results measured this run

* **`static inline` on `rstl::destroy`/`destroy_impl(It, It)`** (`include/rstl/construct.hpp`,
  lines 97-113) changed nothing: the object came back byte-identical and
  `reserve<vector<CWorldState>>` stayed at **71.37%**. Reverted. So the reserve gap below is not
  reachable through linkage/inline hints on `destroy`, even though `uninitialized_copy` carries
  exactly that `static` and *is* emitted out-of-line and called.
* **`reserve__Q24rstl48vector<11CWorldState,Q24rstl17rmemory_allocator>Fi`, 71.37%, ours 180 bytes
  vs retail's 172** (retail 0x80146754). The whole difference is one structural choice: ours
  **inlines** `destroy_impl`'s loop (`mr r3,r30 ; li r4,-1 ; bl __dt__11CWorldStateFv ;
  addi r30,r30,36 ; cmplw ; bne` plus the `b` into it), retail **calls** the out-of-line pair
  (`bl fn_801467A0` -> `fn_801467C0`, which this object already emits and matches). The inlined
  loop needs two more callee-saved registers, so `self`/`size` land in `r27`/`r28` instead of
  `r29`/`r30` and the save/restore becomes `stmw/lmw` instead of three `stw`/`lwz` pairs - that is
  why the register shift is a *consequence*, not a second bug. The copy step is already a call in
  both (`uninitialized_copy<pointer_iterator<CWorldState,...>>` / `fn_8014680C`), so only the
  destroy step is open. One spelling tried this run; not a wall.
* **`push_back__Q24rstl48vector<11CWorldState,...>FRC11CWorldState`, 0.00%** - the previous run's
  wall stands and I did not re-measure it beyond confirming the numbers: retail 56 bytes, our
  template instantiation 144 (the growth check), and the byte-identical `fn_801426E0` (56 bytes,
  ours at 0x90C) still scores 0 because objdiff pairs by name. Confirmed the note's claim that
  `StateForWorld` reserves first and then appends, so retail's body really is the unsafe one.
* **`LoadGameFileState__10CGameStateFPCv`, 0.00%, 488 bytes** - still the **only** retail name
  absent from `CGameState.o` (checked by comparing both objects' `nm` output: 116 retail symbols,
  203 of ours, this one missing). `CMemoryCardDriver.cpp:839,852` calls it and **no object defines
  it**; the link only succeeds because that unit is `NonMatching` and its retail object is used.
  Already filed as `progress-cgamestate-absent-functions` by Lane 7, so no duplicate `NEW:` here.
* The other four are register-allocation walls, measured with the tool below:
  `PutTo__CGameStateFR16CBitStreamWriter` 96.47% (ours saves `r23-r27`, retail saves `r22-r27` -
  one extra long-lived value, and everything after shifts), `__ct__CPersistentOptionsFR16CBitStreamReader`
  95.52%, `__ct__CGameStateFR16CBitStreamReader` 84.14% (ours 0x688 vs retail 0x684 bytes),
  `StartGameFromFrontEnd__Fv` 60.11% (frame `stwu r1,-192` vs retail's `-208`, and a loop that
  materialises `lis/addi` where retail compares against a half-constant built with `addis`).

## A measurement trap worth the same warning as `readlines(True)`

`tools/dis.sh` and `build/G2ME01/main.elf` are **our build** inside a claimed range, so comparing
our object against them is circular - I did it for `push_back` and "confirmed" a byte-exact match
that objdiff scores 0.00%, because our `push_back` is 144 bytes and retail's is 56. The retail
bytes for a claimed range are in **`build/G2ME01/obj/<unit>.o`** (the retail-derived split object
objdiff pairs against); offset = `addr - 0x80142188` for this unit. Same for `R_PPC_REL24` targets.

`tools/bytescmp.py` counts a relocated field as a difference, which buries the real one under ~20
`bl`s. `.tmp/opencode/realdiff.py` (gitignored, this run) prints the same diff with a `RELOC`/`REAL`
tag per line by intersecting the differing instructions with each side's relocations - that is what
made the `li -1` vs `lwz` pair visible at all.

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp:573-587` - `mAreaId(TAreaId(-1))` plus a comment
  recording the `extern const` rule. Nothing else; `include/rstl/construct.hpp` was reverted and
  `git status` shows one modified file.

## Verification

    $ ./tools/fast_try.sh MetroidPrime/Player/CGameState
    main/MetroidPrime/Player/CGameState: 93.03% fuzzy, 70.28% matched (108 / 116 functions)
    $ ./tools/decomp_build.sh | grep '^All:'
    All:  37.00% fuzzy, 30.44% matched, 13.43% linked (13054 / 28465 functions)
    $ sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    $ python3 tools/check_symbol_names.py
    checked 579 units; 0 declared names are missing from their object
    $ python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState
    ok: 4 unit(s) checked, none emits its functions out of retail order
    $ ./tools/goal_check.sh build/goal/item.json
    ok  counts: matched 13053 -> 13054   linked 6163 -> 6163
    ok  target rose: main/MetroidPrime/Player/CGameState: 107 -> 108 / 116 functions
    goal_check: PASS progress-cgamestate-sinit-plain-struct

Per-function diff against the judge baseline: **1 better, 0 worse, 0 added, 0 removed** - only
`__ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo` 97.169014 -> 100.0.
`unit_fit.sh` still reports the unit's pre-existing extras (203 symbols vs retail's 116, unchanged
by this diff); it is a `NonMatching` unit either way.
