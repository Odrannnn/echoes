# cmorphball-wakeeffects-outofline-resize (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-09-30. **Result: `goal_check` PARTIAL** - the unit's matched count rose
**83 -> 84 of 158**, every other check green, and the flip still fails on the same two
*unwritten* functions of the unit (`CElementGen::GetEmitterTime() const`, `fn_800CD4B8`),
unchanged by this diff. The item as filed is **done**: `fn_800C084C` went **0.00% -> 100.00%**
and the call site in `InitializeWakeEffects` now resolves to it.

## What I changed

One file, `src/MetroidPrime/Player/CMorphBall.cpp`, three hunks.

1. **`:64-94` - the element type of `sWakeEffectForMaterial` is now an enum, not `int`.**
   New `EWakeEffectIndex` (`kWEI_None = -1`, `kWEI_Phazon = 0`, `kWEI_Dirt = 2`,
   `kWEI_Organic = 3`, `kWEI_Sand = 4`), `SWakeEffectIndices =
   rstl::reserved_vector<EWakeEffectIndex, 64>`, the vector retyped, and a new
   `static const EWakeEffectIndex kNoWakeEffect(kWEI_None)`.

2. **`:385-442` - `fn_800C084C` written out** under that `extern "C"` name, between
   `fn_800CEF2C` and `CMorphBall::DeleteBallShadow` (the file's descending-by-retail-offset
   order; retail has 0x800C084C between 0x800CEF2C and 0x800C07E0's neighbours).

3. **`:463-469` - `InitializeWakeEffects` calls `fn_800C084C(&sWakeEffectForMaterial, 64,
   &kNoWakeEffect)`** instead of `sWakeEffectForMaterial.resize(64, -1)`, and the four
   material writes use the enumerators.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm. Nothing
under `tools/` or `build/goal/` was edited. (`docs/HANDOFF.md` shows as modified; that is
`MP_GATE_DOCS_WRITE=1` inside `tools/gate.sh` rewriting the derived counts, and the driver
discards it.)

## The element type is a measurement, not a guess - this is the load-bearing finding

`fn_800C084C` is retail's out-of-line `reserved_vector<..., 64>::resize`, and the only thing
that distinguishes it from the four `fn_800D0xxx` fills already in this file is **what the
element type is**. Retail's version fills through `rstl::construct`: one `stw` per trip, and
it keeps `construct`'s placement-new null test (`cmplwi r6,0` / `beq` at 0x800C08A4). The
`fn_800D0xxx` fills do neither - they are eight-wide unrolled with no null test, because their
elements are on the `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` list in
`include/rstl/construct.hpp` and so take the assignment specialisation.

**`int` is on that list.** So `rstl::reserved_vector<int, 64>` cannot produce 0x800C084C at
all - measured, it emits the weak `resize__Q24rstl21reserved_vector<i,64>FiRCi` in exactly
`fn_800D0170`'s 8-unrolled shape, 0% on 0x800C084C's 29 instructions. An **enum is not on the
list**, so it takes the `new (dest) T(src)` path and the loop keeps retail's shape.

I verified the element is 4 bytes and read as a word three independent ways: the stride
(`slwi rX,2`), the fill's `lwz r0,0(r5)` of the value, and `CMorphBall::CollidedWith` at
0x800C28BC (`addi r21,r3,4` for `data()`, then `slwi r0,r0,2` / `lwzx r6,r21,r0` /
`cmpwi r6,0` - a 4-byte element tested against zero). The values written
(`stw` at +0x20 / +0x24 / +0x60 / +0x48 of the object, i.e. `data()[7] / [8] / [23] / [17]`)
are kMT_Phazon / kMT_Dirt / kMT_Organic / kMT_Sand - the same four the tree already wrote, so
retail's semantics are preserved, not reshaped to fit a percentage.

## Two codegen rules this established (both measured, both non-obvious)

- **The `count > n` test must come first.** Written fill-first
  (`if (mCount <= n) { fill } else { destroy }`) mwcceppc emits `bgt` into a *fall-through*
  fill and lays the shrink walk out after it - 2 of 29 instructions wrong. Retail's `ble`
  skips forward over the shrink walk to the fill, so `destroy` is the fall-through. Same
  finding as `GetGravityAcceleration` in `cmorphball-three-tweak-bodies`: **MWCC lays out an
  `if/else` so the *first source branch* is the one that falls through only when the test is
  the negation** - put the uncommon case first and the layout inverts.
- **The destroy walk's register pair is `begin` in r5, `end` in r6, and only
  `rstl::destroy(self->begin() + n, self->end())` produces it.** Spellings measured, all
  putting `begin` in r6 and swapping the pair (2 of 29 each): `destroy(data() + n,
  data() + count)`, `destroy(begin() + n, end())` with *named* `SVal* const` locals, and a
  hand-written `for (it = begin(); it != end(); ++it) destroy(&*it)`. `begin()`/`end()` build
  both iterators from the same `data() + mCount` expression, which is what fixes the
  allocation. Full body after this: **byte-identical** to retail's 29 instructions
  (`build/binutils/powerpc-eabi-objdump` diff of mine against `./tools/dis.sh 0x800C084C 0x74`
  is empty apart from the address column).

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  11286 -> 11287   linked 5507 -> 5507   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: fn_800C084C
no regression

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11286 -> 11287   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.46% fuzzy, 25.10% matched, 11.94% linked (11287 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CElementGen::GetEmitterTime() const',
            'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 83 -> 84 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakeeffects-outofline-resize - flip_test ...: FAIL, but the
target rose; commit it and keep the item
```

Per function, `build/report.json`: `fn_800C084C` (116 B) **0.0 -> 100.0**;
`InitializeWakeEffects__10CMorphBallFv` (532 B) **99.729324 -> 99.729324** (unchanged - the
`bl` target reloc is fixed but the string-pool `addi r4,r31,0x17a` vs our `0x2aa` still
stands, which is the `cmorphball-loadmorphballmodel` item, not this one). Unit `.text` fuzzy
24.883904 -> 25.06.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. All 86 RELs
unchanged (`hashes vs config.yml ok`). `build/gate-probe.log`: `probe: 751 files, 0 failed,
0 errors; link: LINKED (244 undefined, 0 duplicates)` - **244 against the judge baseline of
244, unchanged**, and 0 duplicates. `docs claims agree with the tree`. `check_symbol_names.py`
now checks **514** units, up from 505: `fn_800C084C` is a new *declared* name in the object.

`check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` says "would break on a flip",
**identical at HEAD** (verified by stashing the edit) - pre-existing, and this unit is already
listed as a known permutation in `docs/research/decl_order.md:101` ("48/158, upstream's
order"). `unit_fit.sh`'s "present in ours but not in the retail unit object" list went
**49 -> 48**: the weak `resize__Q24rstl21reserved_vector<i,64>FiRCi` is gone, replaced by the
correctly-named `fn_800C084C`. That is the one extra function this change removed.

## Still open in this unit, measured not guessed

- **74 of the unit's 158 functions still have no body.** That, not anything in this diff, is
  what stops the flip: mwldeppc `undefined:` for `CElementGen::GetEmitterTime() const` and
  `fn_800CD4B8`. A carve is the only route and that is `configure.py` + `splits.txt` +
  `files.cmake` + the source's own claim, four files, which is a different item.
- **The `.rodata` string pool** still blocks `InitializeWakeEffects` (99.73%), `CreateBallShadow`
  (99.97), `UpdateMorphBallTransitionFlash` and `UpdateIceBreakEffect` (99.99 each) on one
  `addi` offset in `rs_new`'s `"\?\?(\?\?)"`. Tracked by `cmorphball-loadmorphballmodel`,
  already filed; unchanged here.

NEW: cmorphball-wakeeffects-carve-74-unwritten | match | MetroidPrime/Player/CMorphBall |
74 of the unit's 158 functions have no body and that, not any single function, is what stops
the flip: mwldeppc reports `undefined: 'CElementGen::GetEmitterTime() const'` and
`'fn_800CD4B8'`, and every lane that improves one function at a time keeps paying the same
flip_test cost. This unit needs a carve (configure.py + config/G2ME01/splits.txt +
files.cmake + the source's own claim, four files in one change) to split the unwritten 74
into their own unit, after which the remaining 84 could plausibly reach Matching. The unit
is already a known permutation (docs/research/decl_order.md:101), so the carve must also
re-order the retained half descending by retail offset or its bytes will come out permuted
with objdiff still at 100% - only flip_test catches that.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:64-94` - `EWakeEffectIndex`, `SWakeEffectIndices`, the retyped
    `sWakeEffectForMaterial`, `kNoWakeEffect`, and the comment recording why the element is
    a class and not `int`
  - `:385-442` - `fn_800C084C`, out of line, with its disassembly and the two codegen rules
  - `:463-469` - the `InitializeWakeEffects` call site

Not committed, per the brief.

---

# Run 2 (lane 6, 2026-10-01)

## Result: `goal_check` PARTIAL - the unit's matched count rose **100 -> 101 of 158**

The item's own subject (`fn_800C084C`, the out-of-line `resize`) was already at 100% when
this run started, so I re-measured first and took the next cheapest real body in the unit.
**`CMorphBall::IsMovementAllowed` went 3.78% -> 100.00%** (retail 0x800CE7D0, 148 B). Every
gate is green; the flip still fails on the same two **unwritten** functions, unchanged by
this diff. This is the `NEW:` line the `cmorphball-wakeeffects-carve-74-unwritten` run filed
as `cmorphball-writemovementallowed`; it is done, so that item is now stale.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11415 -> 11416   linked 5523 -> 5523
  ok    check_symbol_names.py
  ok    All:  32.80% fuzzy, 25.55% matched, 11.98% linked (11416 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CElementGen::GetEmitterTime() const',
                                       'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 100 -> 101 / 158 functions
  ok    no asm added
goal_check: PARTIAL - flip_test FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py <report at HEAD> build/report.json
  matched 11415 -> 11416  linked 5523 -> 5523  (+1 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: IsMovementAllowed__10CMorphBallCFv
  no regression
```

Unit `matched_code` **12156 -> 12304** of 66600, `.text` fuzzy **28.628408 -> 28.842222**.
I built the whole tree at HEAD and diffed all 158 per-function percentages plus
`report_diff.py` over every unit: **+1, 0 worse, 0 changed**.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `hashes vs
config.yml ok` (all 86 RELs); `docs claims ok`; `check_symbol_names.py` still 514 units,
0 missing. `build/gate-probe.log`: `probe: 752 files, 0 failed, 0 errors; link: LINKED
(250 undefined, 0 duplicates)` - 250 against the judge baseline of 250, unchanged.

## What the function is

Retail 0x800CE7D0, 0x94 = 37 instructions, three callees and one float test:

- `CPlayer::GetTweakPlayerControls` (0x8000BF7C) then **`fn_80215860`** (0x80215860) - a free
  function in `build/G2ME01/obj/auto_03_80215424_text.o`, three instructions:
  `lwz r3,0(r3)` / `lbz r3,319(r3)` / `blr`. It dereferences the tweak control's `mData`
  and reads one `bool`. 319 = **0x13F**, and `SLdrTweakPlayerControls`'s `booleans` sub-struct
  starts at **0x130** - `rstl::string` is 4 bytes, then 75 `int` = 300 - so 0x13F is its 16th
  member, `unknown_0x5282c47e`. (Derived from the struct's own field list in the header; no
  probe needed.)
- the pair `lbz r0,1521(r3)` / `lbz r0,1522(r3)` on the CPlayer pointer = **0x5F1 / 0x5F2**,
  which `include/MetroidPrime/Player/CPlayer.hpp` already names `mInFreeLook` and
  `mLookButtonHeld`. Both are private, so two inline accessors were added.
- `CPlayer::IsMorphBallTransitioning` (0x80019DF8).
- the tail is `lfs f1,6240(r31)` = **0x1860**. I measured that with a throwaway
  `tools/probe_cc.sh` layout probe (deleted): it reports `mDisableControlCooldown` at 0x1860,
  which is also what the *name* wants - a control-disable cooldown that has run out is what
  re-allows movement.

## The three things that were load-bearing (all measured)

1. **The float tail is `!(x > 0.f)`, not `x > 0.f` and not `x != 0.f`.** Retail's
   `fcmpo cr0,f1,f0` / `mfcr r0` / `rlwinm r0,r0,2,31,31` / `cntlzw r0,r0` / `srwi r3,r0,5`
   is the *value* form of the conversion, and only the `!`-of-a-positive-compare spelling
   produces it. `CScriptPickup::IsVisible` is at 100% in this tree and is the same shape -
   **its body is `return !(x170 > 0.0f);`**, and that is where I read the spelling off.
   Measured on this function: `!= 0.f` 80.14%, `< 0.f` 83.65%, `>= 0.f` 82.30%,
   `!(x > 0.f)` 83.11% (before the structural fix below), `x > 0.f` 85.14% - and only the
   `!` form has retail's five-instruction value tail, which is what the other four lack.
2. **The outer test must be written negated.** Retail's `bne` at 0x800CE7F4 branches *into*
   the free-look pair, so the source is the negation of the polarity MWCC would choose for
   the positive spelling. `A || (!B && !C)` (the obvious reading) emits a fall-through
   `return false` retail does not have: **88.78%, 20 of 37 instructions wrong**. With the
   tail fixed, `!A && (B || C)` is **byte-exact** apart from relocations. Same finding as
   `fn_800C084C` and `GetGravityAcceleration` in this unit: **MWCC lays out an `if/else` so
   the first source branch falls through only when the test is the negation.**
3. **The `B || C` polarity is also the semantics.** CPlayer+0x5F1/0x5F2 are "free look
   engaged" and "look button down", so movement is blocked when *either* is set. A spelling
   with `!B || !C` also reaches 99.86% (4 differing instructions) and is *semantically
   inverted*; I kept the form the bytes and the field names agree on rather than the one
   that merely scored.

The 4 residual `bytescmp` differences are all relocations - three `bl` (retail 0x8000BF7C /
0x80215860 / 0x80019DF8) and one SDA `lfs f0,-28856(r2)` for the 0.0f constant - which objdiff
normalises. `tools/bytescmp.py` on `CScriptPickup::IsVisible`, which is at 100%, reports 14
differing instructions for the same reason, so its count is not a verdict.

## `fn_80215860` needs a `Port*.cpp`, and that is the whole fourth file

Calling it added a 251st undefined symbol to the **port** link (`build/gate-probe.log`:
`NOT LINKED (251 undefined)`, `NEW fn_80215860`) and `gate.sh` failed on `probe link-gap`.
Measured first: at HEAD the port link is `LINKED (250 undefined, 0 duplicates)`, which is
the judge's baseline (`build/goal/judge/undef.base.count` = 250) - so exactly one symbol was
mine, and the other 11 names in the `NEW` list are pre-existing baseline drift, not mine.

Retail's 0x80215860 is inside an **unclaimed auto-split range**: the nearest
`config/G2ME01/splits.txt` entries are `MetroidPrime/Tweaks/CTweakPlayerGun.cpp` ending
0x80215424 and `CTweakGuiColors.cpp` starting 0x80215878, so no unit owns it and no
`configure.py` entry can. `src/MetroidPrime/PortCTweakPlayerControls.cpp` therefore defines
it, listed in **`files.cmake`** (the port-only list; `configure.py` is the DOL list and was
not touched). This is the arrangement `PortCTweakBall.cpp`, `PortTweakGlobals.cpp` and
`PortMwccNew.cpp` already use, and for the same reason: `tools/` is the judge's, so
`tools/check_files_cmake.py`'s `EXCLUDED` list is not a lane's to edit. The two files must
never be compiled together. The body is retail's own three instructions, character for
character, and the port link is back to **752 files, 250 undefined, 0 duplicates**.

`CTweakPlayerControls::mData` is private, so `GetData()` was added as an inline reader -
`CHECK_SIZEOF(CTweakPlayerControls, 0x4)` and every other member offset are unchanged, and
the DOL report is byte-identical apart from the one new function.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:1720-1748` - `IsMovementAllowed` written out, with
  the three callees, the two code-generation rules and the 0x1860 measurement recorded
- `include/MetroidPrime/Player/CPlayer.hpp:304-308` - `GetInFreeLook()` / `GetLookButtonHeld()`
- `include/MetroidPrime/Tweaks/CTweakPlayerControls.hpp:17-22` - `GetData()`
- `src/MetroidPrime/PortCTweakPlayerControls.cpp` (new, 38 lines) - host `fn_80215860`
- `files.cmake:122-126` - the one listing, with the reason

No `configure.py`, no `config/`, no `splits.txt`, no `.s`, no asm, nothing under `tools/` or
`build/goal/`. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified after
`goal_check.sh` - that is the judge rewriting its own derived counts, and I reverted both.

## Still open in this unit, measured not guessed

Unchanged by this diff and re-measured on the clean tree first: **101 of 158 matched**,
**12304 / 66600 bytes**, so **57 functions below 100%** covering 54296 bytes, and **10 with
no body at all** (was 11 - this diff removed one). The flip is still blocked by the same two
of them, `CElementGen::GetEmitterTime() const` (8 B) and `fn_800CD4B8` (152 B), both
`undefined:` at link time. `tools/check_decl_order.py --unit main/MetroidPrime/Player/
CMorphBall` still says "would break on a flip" (146 of 158), identical at HEAD and already
listed in `docs/research/decl_order.md:101`; that is the real blocker after the unwritten
bytes, and the previous run's carve analysis (31 maximal matched runs, so 31 carve
boundaries, not one) still stands.

Walls carried forward unchanged from `cmorphball-wakeeffects-carve-74-unwritten`, all
measured there and **not retried by this run**: `ComputeMaxSpeed` 96.84%, `GetRenderBounds`
96.48%, `fn_800C8CE0` 74.05%, `GetSpiderBallControllerMovement` 94.51% (and the
`DampLinearAndAngularVelocities` 57.27% call-order note). The `.rodata` string pool still
blocks `InitializeWakeEffects` / `CreateBallShadow` / `UpdateIceBreakEffect` /
`UpdateMorphBallTransitionFlash` - **and this run measured the pool gap, which the earlier
runs only described**: retail's pool (0x238..0x528 of the unclaimed 0x803A84B8) is
`... SamusBallFrozenCMDL, SamusMultiBallANCS, PhazonWake ... RainWake_DGRP, "??(??)",
TXTR_BallFade, Locomotion, BallLight, SlowBlueTailSwoosh_MP ... ScrewAttackJumpFlash, then
three more "??(??)" at 0x2D8/0x2E0/0x2E8`, while ours is `... SamusBallFrozenCMDL,
PhazonWake ... RainWake_DGRP, SamusMultiBallANCS, SlowBlueTailSwoosh_MP ...`, with
`"??(??)"` only at the end and **`Locomotion` and `BallLight` missing entirely**. So the
offset delta is not one constant: `SamusMultiBallANCS` is declared in the wrong place
(it is a `GetMorphBallModel` argument in the constructor, retail has it before the wake
strings), and two strings retail has are not in our source at all. That is a real
`NEW:`-shaped target, see below.

NEW: cmorphball-wakepool-order | match | MetroidPrime/Player/CMorphBall | the unit's
`.rodata` string pool is out of order in a way that pins 4 functions at 99.73-99.99%
(`InitializeWakeEffects`, `CreateBallShadow`, `UpdateIceBreakEffect`,
`UpdateMorphBallTransitionFlash`, each one `addi` off). Measured this run by dumping both
pools: retail has `SamusMultiBallANCS` immediately after `SamusBallFrozenCMDL` and before
`PhazonWake` (ours has it after `RainWake_DGRP`), and retail has `??(??)`, `Locomotion` and
`BallLight` between `RainWake_DGRP` and `SlowBlueTailSwoosh_MP` (ours has none of the three
there). The four affected functions are in `LoadMorphBallModel`'s neighbourhood, so this is
`cmorphball-loadmorphballmodel`'s `.rodata` item, not a new one - but the two missing
strings name something no current code references, so identifying them is its own piece of
work. It is 4 functions of 57, the largest single cluster left that is not an unwritten body.

---

# Run 3 (lane 6, 2026-10-01)

## Result: `goal_check` PARTIAL - the unit's matched count rose **107 -> 108 of 158**

The item's own subject (`fn_800C084C`) was already at 100% before this run, so I re-measured
and took the two nearest functions in the unit. **`CMorphBall::GetRenderBounds` went
96.479% -> 100.00%** (retail 0x800C22F8, 0x180 = 96 instructions, now **byte-exact**), and
**`GetSpiderBallControllerMovement` went 94.506% -> 97.407%** (its remaining 3 instructions are
a measured wall, below). The flip still fails on the same two **unwritten** functions,
unchanged by this diff.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11459 -> 11460   linked 5587 -> 5587
  ok    check_symbol_names.py
  ok    All:  32.85% fuzzy, 25.73% matched, 12.17% linked (11460 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'fn_800CD4B8', 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 107 -> 108 / 158 functions
  ok    no asm added
goal_check: PARTIAL - flip_test FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11459 -> 11460  linked  5587 -> 5587  (+1 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: GetRenderBounds__10CMorphBallCFRC13CStateManager
  no regression
```

Unit `matched_code` 14288 -> 14512 of 66600, `.text` fuzzy 28.956938 -> 28.99.
Per function: `GetRenderBounds` 96.479 -> **100.000**,
`GetSpiderBallControllerMovement` 94.506 -> 97.407. I built the whole tree at HEAD and diffed
all 158 per-function percentages plus `report_diff.py` over every unit: **+1, 0 worse, 0
changed**. `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`hashes vs config.yml ok` (all 86 RELs); `docs claims ok`; `check_symbol_names.py` 514 units,
0 missing. `build/gate-probe.log`: `probe: 752 files, 0 failed, 0 errors; link: LINKED
(250 undefined, 0 duplicates)` - 250 against the judge baseline of 250, unchanged, and
`nm` on the rebuilt object shows **no new defined or undefined symbol** versus HEAD. No
`configure.py`, no `config/`, no `splits.txt`, no `.s`, no asm, nothing under `tools/` or
`build/goal/`.

## The load-bearing finding: a non-`const` `float` local defeats mwcceppc's `x - 0.0f` fold

This is the general rule this run established, and it is what took `GetRenderBounds` to 100%.
The tree's body read `CMath::AbsF(...GetAlpha() - 0.f) < 1e-05f` and mwcceppc emitted **no
`fsubs` at all** - it folds `x - 0.0f` away completely. Measured with a throwaway
`tools/probe_cc.sh` probe (deleted, in `.tmp/`, not in the tree): `extern "C" float t(float a)
{ return a - 0.0f; }` compiles to a bare `blr` - the argument is simply returned.

**A non-`const` `float` local defeats the fold.** The compiler must load the local, and the
load of a float whose value is 0 is exactly the `lfs f1,0(0)` retail has:

| spelling | differing instrs of 96 |
|---|---|
| `- 0.f` (literal) | 8 |
| `- 0.f` with a **non-`const`** `float zero = 0.f` | **3** |

Do **not** reach for `volatile` for this (measured: 50 differing, and it grows the frame by 4
and shifts every slot). A plain non-`const` local is enough and changes nothing else.

The corollary is worth stating because it is the same fact seen from the other side: this is
**not** a "MWCC is missing an optimisation" wall. It is the compiler correctly applying
`x - 0.0f == x` (IEEE-754 makes it exact, and the `frsp` round-trip cannot change that), and
retail's own body is only reachable because the value reached the subtract as a *variable*.
So the previous run's `fn_800C084C` finding ("an enum is not on the trivially-constructible
list, so it takes the placement-new path") and this one are the same shape: **MWCC's
value-based simplifications are what decide the shape, and the way to get a different shape is
to make the value opaque to the simplifier** - by type in one case, by storage in this one.

## The second finding: retail's two `AccumulateBounds` arguments share one base

`GetRenderBounds`'s other 3 instructions were the argument registers. Retail keeps the
optional's address in r31 and passes `mr r4,r31` then `addi r4,r31,12`, so the second argument
is **the same object at +12**, not an independently materialised temporary. Measured:

| spelling | differing instrs |
|---|---|
| `trailBounds->GetMinPoint()` / `->GetMaxPoint()` (the tree's) | 3 |
| two named `const CVector3f&` locals | 3 |
| `const CVector3f* mn = &...GetMinPoint(); *(mn + 3)` | 1 |
| **`const CAABox& box = *trailBounds; box.GetMinPoint(); box.GetMaxPoint()`** | **0** |

Naming the two references is *not* enough - they are two separate temporaries as far as the
register allocator is concerned. Binding the **`CAABox&` itself** and reading both points off
it is what produces the single shared base, and it is the only spelling measured at zero.
(The `+3` variant's 1 remaining difference is instructive: `CVector3f` is 12 bytes, so `+3` is
`+36`, and retail's `+12` is a stride that is not `CVector3f`-derived - it comes from the
box's own layout, which is another reason the `CAABox&` form is the right one.)

## `GetSpiderBallControllerMovement`: a corrected comment, 94.51% -> 97.41%, and a wall

This one is **a correction, not a new body**, and the correction is worth more than the two
points. The comment above the function claimed retail's register pair was
`fmr f1,f31` / `fmr f2,f30`. **`tools/dis.sh 0x800CC758 0x144` says the opposite**:
`fmr f1,f30` / `fmr f2,f31` at 0x800CC80C/0x800CC810. The body had been written to match the
*comment*, so `atan2`'s two arguments were transposed; the `fmr`s were the only instructions
that changed. Corrected in the source and the claim corrected in place, as
`AGENTS.md` requires.

The old comment also claimed "the final `return` shares retail's `fneg` tail with the `-55`
arm". **That was never true** - measured, the three-statement spelling emits *two* `fneg`s and
scores 9 differing instructions. Retail's tail is one `fneg` reached both by the `-55` branch
and by falling out of the `145` test, i.e. mwcceppc tail-merged the two `return -magnitude`
paths. About sixty spellings of the tail were measured with `tools/try_batch.py` (nested
`if`/`else`/`else if` in both orders, ternaries nested and combined, `bool` temporaries, a
`const float neg = -magnitude` hoist, a named `float result`, the outer `if` written as an
explicit `else`). The best is the `else` form in the tree now at **3 differing instructions**,
and the residue is that one un-merged `fneg` plus the polarity of the `-55` test: mwcceppc
emits `bge` (branch into the inner block) where retail has `blt` (branch out to the shared
`fneg`), i.e. it keeps making the *first* source branch the fall-through. The `else` form is
**not** a boundary change - the four arms mean exactly what the three-statement form means.

WALL: GetSpiderBallControllerMovement 97.407% - 3 differing instructions after ~60 tail
spellings; the residue is one `fneg` mwcceppc will not tail-merge plus the `-55` branch
polarity (`bge` where retail has `blt`), i.e. pure layout/polarity, not logic.

## `ComputeMaxSpeed` is a wall too - ~60 spellings, 3 differing instructions, and worse than the tree

Retail's tail is `lfs f0,95.0` / `fcmpo cr0,f2,f0` / `bge` / `b` / `fmr f1,f2` / `b` /
`fmr f1,f0` / `b`: 95.0 in **f0** with the result in f1, and the `min` written so the *product*
is the fall-through. The tree had `rstl::min_val(maxSpeed, 95.f)`, i.e. `lfs f1` /
`fcmpo cr0,f1,f2`. This is the same "make the value opaque" lever as `GetRenderBounds`, and it
does not work here: a non-`const` `float cap = 95.f` gives the identical 4 differing
instructions, because the register choice is driven by the **comparison operand order**, not by
whether the constant is a load.

Measured, all with `tools/try_batch.py` (this is the list, so the next run does not repeat it):

| spelling | differing instrs of 38 |
|---|---|
| `min_val(maxSpeed, 95.f)` (the tree's) | 5 |
| `min_val(95.f, maxSpeed)` | 4 |
| non-`const` `cap`, `min_val(cap, maxSpeed)` | 4 |
| non-`const` `cap`, `min_val(maxSpeed, cap)` | 5 |
| non-`const` `cap`, `result = maxSpeed; if (result < cap) result = cap;` | **3 (best)** |
| the same with the compare on `maxSpeed` rather than `result` | 3 |
| the same with the seed and compare swapped / assigned into `maxSpeed` | 3 |
| `if (maxSpeed < cap) maxSpeed = cap;` alone | 3 |
| non-`const` `cap`, `if (maxSpeed < cap) return cap; return maxSpeed;` | 5 |
| non-`const` `cap`, `result = cap; if (maxSpeed >= cap) result = maxSpeed;` | 6 |
| `if (angle >= ...)`, `cror`-producing `>=` forms, `const` locals hoisted first | 5-8 |
| `float floorVal = 0.01f` made non-`const` too (to move 95.0 into f0) | 6-7 |
| ternaries (`<` and `>`), nested ternaries, `min_val` on the whole `max_val` expression | 4-12 |

WALL: ComputeMaxSpeed 96.842% - 3 differing instructions is the floor over ~60 spellings; the
residue is the `fmr f1,f2` / `b` tail and 95.0 landing in f1 instead of f0, and **every**
spelling that reaches 3 scores *lower* in objdiff (the best measured is 91.58%, i.e. below the
tree's 96.84%), because 3 differing instructions of scheduling cost more bytes than the 5 they
replace. The tree's existing body is the best objdiff spelling and was left alone. The earlier
runs' "ComputeMaxSpeed 96.84%" wall stands; this run adds the spellings above and the
measurement that **the scoring metric and the instruction count disagree here**, so do not
chase 3.

## `fn_800C8CE0` (74.05%) and `fn_800CD244`/`fn_800CD35C`: re-measured, still not reachable

- `fn_800C8CE0` (retail 0x800C8CE0, 0x4C = 19 insns) is retail's
  `rstl::vector<TUniqueId,float>::erase(iterator)`. Its three stack slots and the **32-byte
  frame** are what the tree is missing (the tree's frame is 16). It is `rstl::vector::erase`'s
  own `iterator`-by-address signature, and getting the spill pattern right needs the header's
  `erase(iterator, iterator)` to be transcribed with the same temporaries - real work, not a
  spelling, and it was not reached this run.
- `fn_800CD244` (0x118 = 70 insns) and `fn_800CD35C` (0x104 = 65 insns) are the pair
  `FindClosestSpiderBallWaypoint` calls. `fn_800CD244` reaches `CPlane::CPlane` (defined) and
  `fn_80258790` / `fn_802588DC` (**undefined tree-wide**) and recurses into itself;
  `fn_800CD35C` is its sibling. They are not reachable from this unit without adding undefined
  symbols, which `gate.sh`'s `probe link-gap` fails on. Same situation the previous runs
  recorded for `fn_800CD460` / `fn_800CD4B8`.

## The two unwritten functions that block the flip, re-measured

`tools/flip_test.sh` still fails on `undefined: 'fn_800CD4B8'` and
`undefined: 'CAnimRes::kDefaultCharIdx'` - the same two as the previous runs, and both are
unwritten/undefined rather than anything in this diff. **`CAnimRes::kDefaultCharIdx` is already
in the port's tolerated baseline** (`docs/research/port_link_baseline.txt:7`, one of the 250),
so it is not new; `fn_800CD4B8` (retail 0x800CD4B8, 0x98 = 38 insns) calls `fn_8033D2F4`,
**also undefined tree-wide**, and is reached from three units.

`check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still says "would break on a
flip", and `unit_fit.sh`'s "present in ours but not in the retail unit object" list is **51
functions / 5272 bytes, identical at HEAD** (verified by stashing) - both pre-existing, both
untouched here, and the first is already listed in `docs/research/decl_order.md:101`.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:1668-1696` - the `GetSpiderBallControllerMovement` header comment, **corrected in place**
    (the `fmr` pair was transposed; the `-55`/`fneg` merge claim was never true) and the
    measured spellings recorded
  - `:1697-1728` - that body: `atan2`'s arguments in retail's order, and the `else` form of the
    tail with a comment saying why the `else` is not a boundary change
  - `:1200-1237` - `GetRenderBounds` written out, with the non-`const`-local rule and the
    shared-`CAABox&` finding recorded inline
- nothing else. No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`,
  no asm, nothing under `tools/` or `build/goal/`.

Not committed, per the brief.

## Still open in this unit, measured not guessed

**108 of 158 matched**, 14512 / 66600 bytes, so **50 functions below 100%** and **10 with no
body at all** (unchanged by this run). Carried forward from earlier runs and **not retried
here**: `fn_800C8CE0` 74.05% (now characterised above), `DampLinearAndAngularVelocities` 57.27%
(64 retail insns against ours 52 - it is missing a call, not a spelling),
`GetSpiderBallControllerMovement` 97.41% and `ComputeMaxSpeed` 96.84% (both walled above with
their spellings), and `__ct__10CMorphBall` 96.667% (3956 B - re-measured this run and it is a
**16-byte frame and one `addi r5,r6,442` / `addi r5,r6,420` string-offset family** away, i.e. a
real body, not a spelling).

No new `NEW:` line this run. The carve item `cmorphball-wakeeffects-carve-74-unwritten` is
still the right answer for the flip and is unclaimed work; this run found no *new* target whose
success would raise a count, so filing one would only cost a lane an hour.
