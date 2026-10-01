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

---

# Run 4 (lane 6, 2026-10-01)

## Result: `goal_check` PARTIAL - the unit's matched count rose **111 -> 113 of 158**

The item's own subject (`fn_800C084C`) was already at 100% before this run, so I re-measured
first and went after **the thing that actually stops the flip**. **Two of the four symbols
`tools/flip_test.sh` reported as `undefined:` on this clean tree are now defined**:
**`fn_800C33DC` and `fn_800C88C0` went 0.00% -> 100.00%** (retail 0x800C33DC / 0x800C88C0,
0x5C = 92 bytes = 23 instructions each), and both bodies are byte-identical to retail's.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11544 -> 11546   linked 5625 -> 5625
  ok    check_symbol_names.py
  ok    All:  33.11% fuzzy, 25.96% matched, 12.24% linked (11546 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx', 'fn_800CD35C'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 111 -> 113 / 158 functions
  ok    no asm added
goal_check: PARTIAL - flip_test FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11544 -> 11546  linked  5625 -> 5625  (+2 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: fn_800C33DC
    +100%  main/MetroidPrime/Player/CMorphBall :: fn_800C88C0
  no regression
```

## The blocker set moved, so the earlier runs' blocker list is stale

Measured at HEAD with `./tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` before any
edit - **four** undefined symbols, and only one of them is the one runs 1-3 recorded:

| | at HEAD, this run | after this diff |
|---|---|---|
| `CElementGen::GetEmitterTime() const` | gone (written out at `:118` in the tree) | gone |
| `fn_800CD4B8` | gone (written out, 96.32%) | gone |
| **`fn_800C88C0`** | **undefined** | **defined, 100.00%** |
| **`fn_800C33DC`** | **undefined** | **defined, 100.00%** |
| `fn_800CD35C` | undefined | undefined |
| `fn_800CD244` | undefined (appeared once the other two were gone) | undefined |
| `CAnimRes::kDefaultCharIdx` | undefined | undefined |

`fn_800CD244` only surfaced after `fn_800C88C0`/`fn_800C33DC` resolved, because the linker
reports at most a handful at a time - so the list was always longer than the log showed.

## What the two functions are: deleting destructors of two local helpers, from the vtables

The identification is a **measurement**, not a guess, and it comes out of the `.data` objects:

- `./tools/dis.sh 0x800C33DC 0x5C` / `0x800C88C0 0x5C`: prologue, `mr. r31,r3`, `beq`, **two**
  `lis`/`addi`/`stw` vtable stores separated by a second `beq`, `extsh. r0,r4` / `ble`,
  `mr r3,r31` / `bl CMemory::Free`, epilogue with `mr r3,r31`. Nothing else.
- `python3 tools/dol_read.py 0x803B36F0 0x10` -> `0, 0, 0x800C88C0, 0`; `0x803B36FC` ->
  `0, 0, 0x800C33DC, 0`; `0x803B1750` -> `0, 0, 0x8000DF48, 0`. **Each object's third word is a
  destructor**, which is MWCC's `{offset-to-top, type-info, slot0...}` layout - so
  `lbl_803B36F0`/`lbl_803B36FC` are the two derived classes' vtables and `lbl_803B1750` is the
  base's, and `./tools/dis.sh 0x8000DF48 0x48` is retail's base destructor (same shape).
- So the **second store is the inlined base destructor and the first is the derived one**, and
  the second `beq` is the base destructor's own `if (this)`. That makes the body the same
  family as `fn_800CEF2C`/`fn_800CEF84`/`fn_800CEFD8`, which are already at 100% in this file
  and whose comment records the same conclusions: the parameter is `short`, the test is
  `deleting > 0`, and the return is `void*` so retail's epilogue keeps its single `mr r3,r31`.
- They are **not** `CMorphBall` members: nothing calls them (`objdump -d build/G2ME01/main.elf`
  finds no `bl 800c33dc` / `bl 800c88c0`), they are reachable only through their vtables, and
  the two call sites that build those vtables (`addi r3,r3,14076` at 0x800C308C / 0x800C8138,
  inside `CollidedWith` and `ComputeScrewAttackMovement`) pass them as an argument to
  `fn_8019D2B8` - a `CMorphBall`-local stack temporary, not a member.

## The one spelling question, measured

Only one thing was not already settled by `fn_800CEF2C`: the second `beq`. **It has to be
written.** Retail's `beq +0x34` re-tests the CR0 that the opening `mr. r31,r3` set, so it is
unreachable - but mwcceppc only emits it when the second store sits inside a *second*
`if (self != nullptr)`. Written flat, that `beq` disappears. (Not measured this run - the
nested spelling was written first and is byte-exact, and the nested reading is also what an
inlined `Base::~Base()` with the standard deleting-destructor prologue looks like in source.)

Both functions are written under their retail `extern "C"` names, not as class destructors:
retail's object names them, and a real `~X()` would emit `__dt__<mangled>` instead. The vtables
are referenced as *objects* (`extern "C" char lbl_...[]`) and the store written by hand, which
is `CAudioStateWinCtor.cpp` / `CConsoleOutputWindowCtor.cpp`'s arrangement for the same reason -
these classes have no key function here, so no vtable may be emitted for them.

Verification, not just the percentage: `objdump -d` of our object against
`./tools/dis.sh 0x800C33DC 0x5C` and `0x800C88C0 0x5C` is **instruction-for-instruction
identical** for all 23 each, including the `R_PPC_ADDR16_HA/LO` pairs against
`lbl_803B36FC`/`lbl_803B36F0`/`lbl_803B1750` and the `R_PPC_REL24` against `Free__7CMemoryFPCv`.

## The fourth file, and a host-compiler fact worth keeping

Referencing those three `.data` objects **grew the port link gap** and `tools/gate.sh` failed
on `link-gap` (`gap grew: lbl_803B1750 is not in port_link_gap_list.md`) - they are unclaimed
`.data` gaps (`config/G2ME01/splits.txt` ends its neighbours at 0x803B36F0 and 0x803B1760), so
`dtk` fills them in the DOL build and **the host build has no `dtk` step at all**. Fixed with a
port-only `src/MetroidPrime/PortCMorphBallVtables.cpp` listed in `files.cmake`, the
`PortCTweakPlayerControls.cpp` arrangement run 2 established. It defines all three as
zero-filled arrays of retail's sizes rather than transcribing retail's words, because their
contents are **DOL code addresses** and a host vtable pointing at `0x800C88C0` would be worse
than an empty one.

**g++ 15 emits nothing for an unreferenced `extern "C" char name[N];` with no initialiser** -
no symbol at all and a zero-sized `.bss`. Measured: `char lbl_a[12];` gives `nm` nothing,
`int lbl_b;` gives `B lbl_b`, and `= {0}` inside an `extern "C" { }` block gives
`B lbl_a` with no "initialized and declared extern" warning. The first version of that file
used bare declarations and **still** failed `link-gap`; the initialiser is load-bearing.

## `fn_800C8CE0`: re-measured, the 32-byte frame is reachable and the third stack slot is not

74.05% unchanged, but this run got further than the earlier runs' characterisation ("the tree's
frame is 16", "needs the header's `erase` transcribed"). Retail's 19 instructions, measured:
a **32-byte frame**, `stw r31,28(r1)` / `mr r31,r3` (so `out` is live in r31 across the call),
**three** stack cells - `8(r1)` = `*it+8`, `12(r1)` = `*it+8` again, `16(r1)` = `*it` - and the
call `r5 = r1+16`, `r6 = r1+12`. So the dead third cell is a *second copy of `last`*, and the
only register saved is r31.

Spellings measured with a local variant runner (`.tmp/`, throwaway, not in the tree), counting
differing instructions out of 19:

| spelling | differing |
|---|---|
| the tree's current body (`last`, `first`, `lastCopy = last`, `(void)lastCopy`) | 14 |
| `lastA` declared before `last`, `firstCopy` etc. (three dead-copy orderings) | 14 |
| passing `&first, &last` **twice** (two `fn_800C8D2C` calls) | 15 - correct 32-byte frame and 3 cells, but it calls twice |
| `&lastCopy` behind a `&lastCopy != last` guard | build fail (MWCC rejects the `pointer_iterator` comparison) |
| heap-allocated copies (`new iterator(*it + 1)`) | 47 |
| `rstl::destroy(&*first, &*lastA)` before the call | 17 |
| **three members of a local `struct STmp` assigned then `&t.c, &t.b`** | **8 (best)** |
| the same with members initialised in a **constructor init-list** | 15 |
| the same with `out` also in the struct (to try to get `mr r31,r3`) | 11 |
| aggregate init `{*it + 1, *it + 1, *it}` | build fail (`iterator` is not an aggregate initialiser target in this MWCC) |

The 8-difference `struct STmp` spelling does produce the 32-byte frame and the right store
shape, and it shows what is left: mwcceppc **zero-initialises the whole struct first**
(`li r8,0` + three `stw r8`), and there is still no `mr r31,r3`. So the frame is reachable with
an aggregate; the zero-init and the r31 save are not, with any spelling tried. Constructor
init-lists fix the zero-init and lose the frame. **Do not re-try the three-named-locals
spellings** - 14 differences, and the dead copy is always dropped.

Not a `WALL:` line: the spelling list above is the finding, and `fn_800C8CE0` has never been
measured above 74.05%.

## Other targets measured and not taken this run

- **`fn_800CD4B8` (96.316%, 38 insns) is a codegen wall, not a spelling.** Three instructions
  differ, all rotate/mask immediates: retail `rlwinm. r0,r0,27,31,31` / `rlwimi r4,r0,2,28,29` /
  `rlwimi r3,r3,1,26,26` against ours `rlwinm. r0,r0,0,27,27` / `rlwimi r4,r0,4,26,27` /
  `ori r0,r3,64`. That is mwcceppc choosing a *different encode of the same bit-field
  operation* - the shared `(*flags >> 2) & 3` already matches retail's `rlwinm 30,30,31`, so the
  extraction is right and only the three **write** forms differ. Chasing them is a
  change-the-bit-positions exercise whose semantic justification I could not establish from the
  disassembly, so I left the tree's spelling alone. **This is the same measurement the previous
  run recorded** ("a bit-field declaration emits a `stw`-and-mask pair, 91.58%") seen from the
  other side: it is not the declaration, it is the encode.
- **`fn_800CD35C` and `fn_800CD244`** (260 / 280 bytes, both 0.00%) are the two remaining
  blockers and are real bodies, not spellings: each recurses into itself and walks a
  `std::vector`-shaped block three times, with `fn_800CD244` reaching `CPlane::CPlane` and
  `fn_800CD35C`/`fn_800CD244` both reaching `fn_80258790` / `fn_802588DC`. **Correction to the
  previous runs:** those two symbols are **not** undefined tree-wide. `nm build/G2ME01/main.elf`
  has `80258790 T fn_80258790` and `802588dc T fn_802588DC`, and
  `nm build/G2ME01/obj/auto_03_80257AF8_text.o` defines the first - both are inside the
  unclaimed auto-split range 0x80257AF8+, which the **DOL** link does get. So they are
  reachable from a `configure.py` unit with no new undefined symbol. Whether they cost the
  *port* link anything is a separate question - the gap list did not grow for them, but that was
  not measured because no body was written.
- **`CAnimRes::kDefaultCharIdx`** is the fourth blocker and needs no analysis: it is a
  `static const int` declared at `include/MetroidPrime/CAnimRes.hpp:37` and **defined nowhere
  in the DOL** (only `src/MetroidPrime/PortReachStubs.cpp` aliases it for the host), so any
  flipped unit that reaches `CMorphBall.cpp:1129` cannot link. It is a one-definition fix whose
  value is one four-byte `.sdata` word.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:718-781` - `fn_800C88C0` and `fn_800C33DC` written out
  under their retail names, the three `extern "C" char lbl_...[]` gap-object declarations, and the
  header comment recording the vtable reading, the `beq +0x34`, and why no class destructor is
  used
- `src/MetroidPrime/PortCMorphBallVtables.cpp` (new, 55 lines) - the port-only definitions of
  `lbl_803B36F0` / `lbl_803B36FC` / `lbl_803B1750`, with the g++-15 measurement
- `files.cmake:123-132` - the one listing, with the reason

Nothing else. No `configure.py`, no `config/`, no `splits.txt`, no `.s`, no asm, nothing under
`tools/` or `build/goal/`. (`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified
after `goal_check.sh` - that is the judge rewriting its own derived counts; both reverted.)

## Gates, all measured

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `gate.sh` green,
which covers the 86 REL hashes, `probe_sources.sh` and the docs claims. `build/gate-probe.log`:
`probe: 755 files, 0 failed, 0 errors; link: LINKED (241 undefined, 0 duplicates)` - **241
against the judge baseline of 250, so 9 below it and unchanged by this diff**. `docs claims
agree with the tree`. `check_symbol_names.py` now checks **515** units (was 514): the three
`lbl_` names are new *declared* names. `report_diff.py` over every unit: **+2, 0 worse, 0
changed**. `unit_fit.sh`'s "present in ours but not in the retail unit object" list is **51
functions / 5272 bytes, byte-for-byte the same list and size as at HEAD** - neither new function
added an extra. `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still says
"would break on a flip", identical at HEAD and already listed in `docs/research/decl_order.md:101`.
The two new functions are declared next to `fn_800D0640`, which is *not* their retail
neighbour, but the unit is a pre-existing permutation and this does not make it worse.

## Still open in this unit, measured not guessed

**113 of 158 matched**, 15064 / 66600 bytes, so **45 functions below 100%** and **8 with no body
at all** (was 10 - this diff removed two). Carried forward from the earlier runs and **not
retried here**: `GetSpiderBallControllerMovement` 97.41% and `ComputeMaxSpeed` 96.84% (both walled
with ~60 spellings each in run 3), `__ct__10CMorphBall` 96.667%, `fn_800CD4B8` 96.32% (characterised
above), `fn_800C8CE0` 74.05% (measured above), `DampLinearAndAngularVelocities` 57.27%.

The flip needs all four remaining `undefined:` symbols gone *and* the decl order fixed *and* the
45 sub-100% functions matched. The carve item `cmorphball-wakeeffects-carve-74-unwritten` is still
the right answer for the decl-order half; run 3's analysis (31 maximal matched runs, so 31 carve
boundaries, not one) stands.

No `NEW:` line this run. `fn_800CD35C` / `fn_800CD244` are now *known reachable* (their two callees
turn out to be defined), so they are the obvious next slice for this unit rather than a new queue
item - filing a `NEW:` for work an existing item already covers would only cost a lane an hour.

---

# Run 5 (lane 5, 2026-10-01)

## Result: `goal_check` PARTIAL - the unit's matched count rose **115 -> 116 of 158**

`fn_800C084C`, this item's own subject, was already at 100% before this run, so I re-measured the
unit and took the two functions it had **no body at all** - the last two of the 158, `fn_800CD35C`
and `fn_800C5420`. **`fn_800CD35C` went 47.692% -> 100.00%** (retail 0x800CD35C, 0x104 = 65
instructions, now byte-identical) and `fn_800C5420` went *none* -> 96.757% (444 bytes = 111
instructions, correctly shaped, 26 instructions of register allocation away - characterised below so
the next run does not repeat it). Every gate is green; the flip still fails on **one** `undefined:`
name, unchanged by this diff.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11953 -> 11954   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.78% fuzzy, 26.97% matched, 12.64% linked (11954 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 115 -> 116 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakeeffects-outofline-resize - flip_test ...: FAIL, but the target
rose; commit it and keep the item
```

Unit `matched_code` **15496 -> 15756** of 66600, `.text` fuzzy **30.414774 -> 31.264025**,
`matched_functions` **115 -> 116**. Per function: `fn_800CD35C` 47.692307 -> **100.000000**,
`fn_800C5420` `None` -> **96.756760**, every other one of the 158 unchanged - and a per-function
diff over **every** unit reports **0 worse**, 1 better.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (goal_check prints it in
the revert line). All 86 RELs unchanged (`rel_module_order: 86 modules, unchanged`). `docs claims
agree with the tree`. `build/gate-probe.log`: `probe: 746 files, 0 failed, 0 errors; link: LINKED
(324 undefined, 0 duplicates)` - **324, which is `build/goal/judge/undef.base.count` exactly**, so
this diff adds the port nothing undefined. `check_symbol_names.py` now checks **516** units (was 515):
`fn_800C5420` is a new *declared* name. `check_raw_offsets.py`: `166 raw-offset site(s) in 70
file(s)` - **unchanged**, the new code reads through named members only.
`unit_fit.sh`'s "present in ours but not in the retail unit object" list is **51 functions / 5272
bytes, identical to HEAD** - neither new body is an extra. `check_decl_order.py --unit
main/MetroidPrime/Player/CMorphBall` still says "would break on a flip", identical at HEAD (verified
by stashing); `build/gate-order.log` reads "30 permuted, all 30 accounted for in decl_order.md".
**Both new bodies are declared descending by retail offset** - `fn_800CD35C` was already in place
(0x800CD35C before `fn_800CD244` at 0x800CD244), and `fn_800C5420` went in between `fn_800C7674`
(0x800C7674) and `fn_800C5F3C` (0x800C5F3C) - so this diff adds no inversion.

## `fn_800CD35C` 47.69% -> 100%: the previous runs' wall was a loop, not a shape

**This corrects `docs/goal-notes/cmorphball-unclaimed-80258790-802588dc.md`**, which recorded a
measured wall: "no spelling can produce that shape". What it measured was right; what it concluded
from it was wrong, and the file now says so in place.

Retail's 65 instructions are **three** copies of one guard-plus-worker block followed by a tail call
to itself: guard at 0x800CD35C..0x800CD388, worker at 0x800CD38C..0x800CD3B0, guard at
0x800CD3B4..0x800CD3C4, worker at 0x800CD3C8..0x800CD3F4, guard at 0x800CD3F8..0x800CD408, worker at
0x800CD40C..0x800CD438, then `mr r3,r30` / `mr r4,r31` / `bl fn_800CD35C` at 0x800CD43C..0x800CD444.
Every branch in it is forward; there is no loop.

**MWCC does not unroll this loop, but the copies can be written out, and that is byte-exact.** 13
spellings measured with a local variant runner (throwaway, in `.tmp/`, deleted; the runner is
described at the end of this note). "differing instructions" is out of 65:

| spelling | ours | differing |
|---|---|---|
| the previous tree body: one guard + one worker + tail call | 39 | **47-52** |
| `for (int i = 0; i < 2; ++i)` / `< 3` / `< 4`, guard inside the body | **39** each | 51 each |
| the same with the guard after the workers | 39 | 66 each |
| `for (int i = 0; i < 3; ++i)` with no guard | 34 | 52 |
| `for (int i = 0; i < 3; ++i)`, guard written as `if (mRemaining == 0) {...} else { return; }` | 40 | **47** (the old best) |
| `do { ... } while (--i != 0)` with `int i = 3` | 38 | 50 |
| `while (i-- != 0)` with `int i = 3` | 39 | 50 |
| **three copies of the block written out, then the tail call** | **65** | **0 - byte-identical** |

The constant-bound `for` is the informative one: **2, 3 and 4 all give the same 39 instructions**,
i.e. one copy plus a backward branch, so MWCC is not unrolling *at all* here. The earlier note's
supporting measurement - "a `for` with a constant bound of 4 comes out as four copies" - was taken
on a loop with **no calls in it**. A body that calls `fn_80258790` and `fn_802588DC` is not unrolled.
That is the general rule this run adds: **MWCC's unroll needs a call-free body**, and a loop that
calls is where a "write it out" body and a "the compiler will do it" body come apart.

**Writing the three copies out is not a change of meaning, and that is the reason it is defensible.**
The tail call re-tests `mRemaining` before doing anything, so running the guard-and-workers once or
three times before it returns the same value for every input; three copies is what retail's object
contains, not a decision the source adds. Two smaller pieces of evidence that retail really did get
this from *one* piece of source rather than from three hand-written copies: the **first** copy needs
no `mr r3,r30` / `mr r4,r31` (it falls through from the prologue, where `r3`/`r4` still hold
`path`/`cursor`) and the two later copies re-establish them, and the **first** worker's
`bl fn_80258790` at 0x800CD398 likewise has no argument setup while copies two and three do. That
is what an unrolled body looks like, and it is reproduced exactly by writing it out.

## `fn_800C5420`: `CActorLights::operator=`, and a register-allocation wall at 96.76%

Retail 0x800C5420, 0x1BC = 111 instructions. It is **not** a `CMorphBall` member and its one caller
identifies it: `CMorphBall::PreRender` at 0x800C53B8 passes `r3 = lwz r3,3664(r28)` = this+0xE50 =
`mActorLights` and `r4 = r30`, the stack local it has been filling (`stw r0,660(r30)` at
0x800C53B0 is the +0x294 `CColor` retail's tail copies). The object is 0x2E4 bytes and
`include/MetroidPrime/CActorLights.hpp` already declares it at `CHECK_SIZEOF(CActorLights, 0x2e4)`,
with every member landing on retail's own offset.

It is written out under its retail `extern "C"` name rather than as a `CActorLights::operator=`,
for the two reasons this file uses everywhere: retail's object names it `fn_800C5420`, and a member
spelling would emit `__as__12CActorLightsFRC12CActorLights` for objdiff never to pair with it; and
retail copies a light element through `fn_80045E18` (0x80045E18 - ten `lfd`/`stfd` pairs, all 0x50
bytes) while `rstl::reserved_vector<T,N>::operator=` in `include/rstl/reserved_vector.hpp` calls
`destroy_elements()` and re-constructs through `uninitialized_copy`. Retail's loop assigns into
existing storage and calls nothing else.

Three measured shapes decide the spelling, all in the source comment:

1. **The self-assignment guard is per container, not per object.** Each container tests *its own*
   address pair - `cmplw r30,r31` / `beq` at 0x800C5438 for the first (offset 0, so the object
   pointers are the container pointers), `addi r3,r30,324` / `addi r0,r31,324` / `cmplw r3,r0` at
   0x800C5488 for the second, `addi r3,r30,648` / `addi r0,r31,648` / `cmplw r3,r0` at 0x800C54D4 for
   the ids. `rstl::reserved_vector::operator=`'s own `if (this != &other)` inlined three times. The
   containers are therefore modelled as `{ int mCount; T mItems[4]; }` structs, so `&container` is
   the *count's* address: writing the guard on `&self->mAreaLights.mItems` instead emits `addi
   r3,r30,328` and scores **67.05%** against 73.43%.
2. **Both light loops bound the source with `mulli count,80` and walk it with `cmplw`/`bne`** - the
   pointer walk, not the indexed one `fn_800C7674` above needs. Indexed spellings were measured and
   are much worse: `for (int i = 0; i < other->mX.mCount; ++i) mItems[i] = other.mItems[i]` gives
   **61** differing instructions in every combination tried, against 26 for the pointer walk.
3. **The tail is declaration order, member by member**, with the 14-bit flag run copied as **two raw
   `lbz`/`stb` bytes** at +0x2A0/+0x2A1 (+0x2A2/+0x2A3 are never touched), `lha`/`sth` for the two
   `short`s, `lfs`/`stfs` for the three trailing floats. Retail returns nothing from the epilogue
   and its caller discards the result, so this returns `void`.

### Two codegen rules this run adds, both measured

- **A 12-byte POD copies as an 8-byte block plus a 4-byte word, not as three words - but only if the
  first eight bytes are a member.** Retail hoists the first two words of `mLightingPositionOffset`
  (+0x2B4) into `r4`/`r0` and stores them back before touching the third
  (`lwz r4,692` / `lwz r0,696` / `stw r4,692` / `stw r0,696` / `lwz r0,700` / `stw r0,700`), and the
  same at +0x2C4, while the three `float`s immediately below at +0x2D0..+0x2D8 are `lfs`/`stfs`.
  A `CVector3f` member gives `lfs`/`stfs` per component. A flat `struct { unsigned int x, y, z; }`
  gives three plain `lwz`/`stw` pairs with **no hoist** (that is 73.43% -> 91.48%). Wrapping the first
  two words in a nested two-word struct makes MWCC do the block copy, and the hoist lands - in
  **r3**, where retail has **r4**:
  | spelling of the 12-byte member | objdiff | note |
  |---|---|---|
  | `CVector3f` | 73.43% | `lfs`/`stfs` x3 |
  | `struct { unsigned int x, y, z; }` | 91.48% | `lwz`/`stfs` x3, no hoist |
  | `struct { struct { unsigned int a, b; } ab; unsigned int c; }` | **96.76%** | hoist into r3, retail has r4 |
- **The hoist picks the first free volatile register, so a dead `mr r3,self` in retail's tail is
  probably what pushes it to `r4`.** Retail has `lwz r0,660(r31)` / **`mr r3,r30`** /
  `stw r0,660(r30)` at 0x800C5514..0x800C551C and then **never uses `r3` again** - a dead
  argument-register setup between a load and its store. Six tail spellings were measured for it
  (control; `mAid` first; the `CColor` through a named `const CColor&`; the whole tail in
  `do { ... } while (false)`; the `CColor` through a `reinterpret_cast<unsigned int*>` view;
  `mAid.value = other.mAid.value`) and **all six give 26** - none produces the `mr r3,r30`. Not
  fixed here.

### `fn_800C5420` at 96.76% is a register-allocation wall - 54 loop and 6 tail spellings measured

The remaining **26 differing instructions of 111** are, in full: `r28`<->`r29` swapped in both light
loops (4 lines), `r4`<->`r5` swapped in the ids loop (8 lines), the two `SWords3` hoists landing in
`r3` where retail has `r4` (2 lines), **one extra instruction** in the second light loop's bound
(`add r29,r31,r0; addi r29,r29,328` where retail has `add r28,r29,r0` off the already-materialised
array base), and **one missing instruction** (`mr r3,r30`). Everything else - the frame, the `stmw`/
`lmw r27`, all three guards, all three loops, and the whole 0x294..0x2E0 tail - is
instruction-for-instruction identical.

Measured, so the next run skips them (differing instructions out of 111; `ptr` = the pointer walk
in the tree, `A`-`F` = which of the three local pointers is declared first and how the bound is
spelled, `ids` = the same for the `unsigned short` walk):

| family | tried | best |
|---|---|---|
| local-pointer declaration order x bound spelling | 6 forms x 6 forms x 2 ids orders = 36, all builds 111/111 | **26** (`ptr`/`first`+`first`, ids `dst,src,end`) |
| bound from `first` vs from `src` vs from the member expression, with and without a hoisted `const int n` | 24 (the `n`-less forms do not compile; the rest are 61-81) | 35 |
| indexed / counted-down loops instead of the pointer walk | 27 | 52 |
| tail spellings for `mr r3,r30` | 6 | 26 (no change) |

**Do not re-try the loop spellings: 26 is the floor and it is reached by the plain pointer walk in
the tree.** The two structural residues are both allocator choices, not logic - retail's second
light loop builds its bound off the array base and the first off the object base, which MWCC did here
too in the first loop and not in the second, and the dead `mr r3,r30` resisted six tail spellings.

WALL: fn_800C5420 96.757% - 26 of 111 instructions differ after 54 loop and 6 tail spellings; the
residue is register allocation (r28/r29, r4/r5, r3/r4), one loop-bound base choice and one dead
`mr r3,r30` that six tail spellings do not produce.

## Still open in this unit, measured not guessed

**116 of 158 matched**, 15756 / 66600 bytes, so **42 functions below 100%** and **one with no body at
all** (was two - this diff removed both). Carried forward from the earlier runs and **not retried
here**: `GetSpiderBallControllerMovement` 97.41% and `ComputeMaxSpeed` 96.84% (both walled with ~60
spellings each in run 3), `__ct__10CMorphBall` 96.667% (3956 B - "a 16-byte frame and one `addi
r5,r6,442` / `addi r5,r6,420` string-offset family" away, i.e. real body work, not a spelling),
`fn_800CD4B8` 96.316% (a bit-field *encode* difference, three rotate/mask immediates), and
`DampLinearAndAngularVelocities` 57.27% (missing a call, 64 retail insns against ours 52). The 36
functions under 3% are the file's `TODO` scaffolds and are far too big for one item.

The flip's blocker list is **one** name and this diff did not change it: `CAnimRes::kDefaultCharIdx`,
declared `static const int` at `include/MetroidPrime/CAnimRes.hpp:37`, **defined nowhere in the DOL**
(only `src/MetroidPrime/PortReachStubs.cpp` aliases it for the host), absent from
`config/G2ME01/symbols.txt`, and already in the port's tolerated baseline. Past that, the flip still
needs the 42 sub-100% functions matched and the pre-existing 30-permutation decl order fixed; the
carve item `cmorphball-wakeeffects-carve-74-unwritten` is still the right answer for that half (run 3's
analysis - 31 maximal matched runs, so 31 carve boundaries, not one - stands).

No `NEW:` line this run. `fn_800C5420` is a measured wall, which the brief says belongs in the notes
rather than the queue, and the remaining names all belong to work an existing item covers.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:641-695` - `fn_800CD35C` written out (three block copies + the tail call), and the comment
    above it rewritten: it no longer claims the shape is unreachable, and it records what MWCC does
    and does not unroll
  - `:1109-1268` - `fn_800C5420` written out with `SBlob50` / `SLightVec` / `SIdVec` / `SWords2` /
    `SWords3` / `SActorLightsCopy`, `CHECK_SIZEOF(SActorLightsCopy, 0x2e4)`, and the comment
    recording the per-container guards, the pointer walk, the two codegen rules and the void return
- `docs/goal-notes/cmorphball-unclaimed-80258790-802588dc.md` - the superseded `fn_800CD35C` wall,
  annotated in place with what superseded it and why (this run changed the answer a question that
  file answers, so `AGENTS.md` requires the correction in the same change)

Nothing else. No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm,
nothing under `tools/` or `build/goal/`. `docs/HANDOFF.md` shows as modified after `goal_check.sh` -
that is the judge rewriting its own derived counts (it moves the state block to 11954 / 10406); it is
the judge's output, not an edit of mine, and the driver discards it.

Not committed, per the brief.

## Reproducing the two measurements

The variant runner used here is a local copy of `tools/try_batch.py` in `.tmp/` (deleted with the
rest of `.tmp`); it exists because `try_batch.py`'s `find_body_span` regex cannot match a definition
line that starts `extern "C" void name(` - the `"` is outside its character class, so it dies with
"definition not found". It replaces the function's brace span, rebuilds only
`build/G2ME01/src/<unit>.o` with ninja, and counts differing instructions against
`build/G2ME01/obj/<unit>.o` with branch targets and relocation operands normalised. **`tools/` is the
judge's and was not touched**; if this is worth keeping generally it belongs as a fix to
`try_batch.py` in a change of its own.
