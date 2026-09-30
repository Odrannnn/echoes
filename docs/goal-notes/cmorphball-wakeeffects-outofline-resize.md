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
