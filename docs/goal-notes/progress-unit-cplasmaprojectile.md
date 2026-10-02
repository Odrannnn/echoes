# progress-unit-cplasmaprojectile — `MetroidPrime/Weapons/CPlasmaProjectile` 20/21 → **21/21**

**Result: the unit is at 21/21 functions matched and 100.00% fuzzy / 100.00% matched code**
(`build/report.json`: `total_code 10768`, `matched_code 10768`, `matched_functions 21/21`).
Whole DOL: **matched 12448 → 12449 / 28465**. `tools/goal_check.sh build/goal/item.json` → **PASS**.
Nothing is `Matching` yet — this is a `progress` item and `flip_test.sh` was deliberately not run to decide it.

No Prime 1 donor was needed: the item's `reason` was written when three functions were short; two of
them (`MakeBillboardEffect`, `UpdatePlayerEffects`) had already landed upstream, so only one remained.

## Re-measure first (what the clean tree actually said)

```
$ ./tools/decomp_build.sh MetroidPrime/Weapons/CPlasmaProjectile
main/MetroidPrime/Weapons/CPlasmaProjectile: 98.37% fuzzy, 98.37% matched (20 / 21 functions)
   fn_801197E8                                            0.00%  176 bytes
```

Not 18/21. The queue's reason is a starting point, not a measurement — re-measuring it first is what
left one function to work on instead of three. `report.json` had **no** `fuzzy_match_percent` key for
`fn_801197E8` at all: objdiff had nothing to pair, which is a different failure from a wrong byte.

## `fn_801197E8` — 0.00% → 100.00% (176 bytes, the last function in the unit)

`config/G2ME01/symbols.txt:4876`:

```
fn_801197E8 = .text:0x801197E8; // type:function size:0xB0
```

**It is `rstl::vector<TUniqueId>::operator=`**, dtk having recovered no name for it. Identified from
the two ends, not from the name:

* `tools/dis.sh 0x80119740 0xA8` — `DeletePlasmaLights__17CPlasmaProjectileFR13CStateManager`
  ends at 0x801197E7 with `bl fn_801197E8`, `addi r3,r1,12`, `li r4,-1`,
  `bl __dt__Q24rstl45vector<9TUniqueId,Q24rstl17rmemory_allocator>Fv`: the call is
  `mLights = rstl::vector<TUniqueId>()` written out.
* `tools/dis.sh 0x801197E8 0xB0` — self-assign guard on the two pointers (`cmplw r30,r31`), then
  `bl fn_800E9A38` (12 bytes at 0x800E9A38: `li r0,0 / stw r0,4(r3) / blr` — i.e. `clear()`),
  the `mCount == 0` → `CMemory::Free` → zero three fields arm, and a `lhz`/`sth` **2-byte** copy
  walk bounded by `src.mItems + 2*src.mCount`. `TUniqueId` is 16-bit in this tree, which is what
  makes the element width two bytes and pins the identity.

### The bytes were already right. Only the symbol name was wrong.

`build/G2ME01/src/MetroidPrime/Weapons/CPlasmaProjectile.o` (before the change) defined the function
**at 0x250, size 0xB0, instruction for instruction identical to retail** — under the name
`__as__Q24rstl45vector<9TUniqueId,Q24rstl17rmemory_allocator>FRCQ24rstl45vector<9TUniqueId,Q24rstl17rmemory_allocator>`,
as a weak COMDAT emitted right after `DeletePlasmaLights` (its only user). Verified by disassembling
both objects over the same range and diffing the listings; the only differences were the relocation
records, which are relocation records.

**objdiff pairs functions by name**, so a byte-perfect function under a name retail does not have is
unscoreable. That is the whole 176-byte shortfall: `matched_code` 10592 → 10768 is exactly `0xB0`.
dtk named 99 `__as__` instantiations in `symbols.txt`, including the sibling
`__as__Q24rstl45vector<9TEditorId,...>` at 0x8005424C — this one alone came out as `fn_`.

### The fix, and the convention it follows

`src/MetroidPrime/Weapons/CPlasmaProjectile.cpp` now spells the function out under retail's own name
and `DeletePlasmaLights` calls it in place of the assignment (lines 591-616 and 624). This is the
repo's existing convention for a retail-unnamed symbol, from
`build/goal/notes/cmorphball-wakeeffects-outofline-resize.md` (`fn_800C5420` = `CActorLights::operator=`,
"a member spelling would emit `__as__12CActorLightsFRC12CActorLights` for objdiff never to pair with it")
and `src/MetroidPrime/Player/CGameStateBlockDtor.cpp` (`extern "C" SGameStateBlock* fn_80004A4C(...)`).
The body is `include/rstl/vector.hpp`'s `operator=` spelled out; it is not a stub and it does the work.

Measured after: the object defines `T fn_801197E8` at 0x2A4, **0xB0 bytes, byte-identical**, and the
weak `__as__` copy is gone. 21/21.

### Decl order and the slot

`fn_801197E8` (retail +0x250) sits between `DeletePlasmaLights` (+0x1A8) and `CreatePlasmaLights`
(+0x300), so it is **defined between those two**, which is where it is in the file now.
`python3 tools/check_decl_order.py --unit MetroidPrime/Weapons/CPlasmaProjectile` → `none emits its
functions out of retail order` (gate's `decl order` step also ok). In the object it lands at 0x2A4,
not 0x250, because the `__dt__<vector<TUniqueId>>` COMDAT now takes 0x250; both are COMDAT-dropped
at link time, so the linked order is still retail's. Not measured against a flip — see below.

## Measurements

| | before | after |
|---|---|---|
| unit matched_functions | 20 / 21 | **21 / 21** |
| unit fuzzy / matched code % | 98.37 / 98.37 | **100.00 / 100.00** |
| unit `matched_code` | 10592 / 10768 | **10768 / 10768** |
| DOL matched_functions | 12448 | **12449** |
| DOL linked | 5863 | 5863 |

`fn_801197E8` 0.00% → 100.00%. Every other function in the unit: unchanged at 100.00%.
`unit_fit.sh`: extras 18 → **17** functions (1568 bytes, all pre-existing COMDAT copies;
`reserve`/`__dt__`/`__ct__`/token destructors) — the `__as__` became a claimed function.

Gate, all green (`build/goal/check-gate.log`):

```
configure ok / ninja + build.sha1 ok / hashes vs config.yml ok / report ok
per-function diff  matched 12448 -> 12449  linked 5863 -> 5863  (+1 functions at 100%, 0 units newly linked)
module wiring ok / dol_read ok / docs claims ok / gs offsets ok / raw offsets ok
decl order ok / files.cmake ok / module order ok / port probe ok / port link gap ok / reach stubs ok
GATE PASS  b52a79f6+2 changed
sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

`goal_check.sh`:

```
ok  no judge-owned path touched
ok  gate.sh        ok  counts: matched 12448 -> 12449   linked 5863 -> 5863
ok  check_symbol_names.py    ok  All: 35.19% fuzzy, 28.92% matched, 12.90% linked (12449 / 28465 functions)
ok  target rose: main/MetroidPrime/Weapons/CPlasmaProjectile: 20 -> 21 / 21 functions
ok  no asm added
goal_check: PASS progress-unit-cplasmaprojectile
```

## Two facts worth keeping (measured, not recalled)

1. **objdiff does not penalise a relocation whose target symbol is absent from the target**, as long
   as the instruction bytes match. Our `fn_801197E8` calls `clear__Q24rstl45vector<9TUniqueId,...>`
   where retail calls `fn_800E9A38` — the same three instructions, a different name, and the function
   still scores **100.00%**. That is consistent with `DeletePlasmaLights` having scored 100% all along
   while its `bl` targeted `__as__...`, which retail does not have either. So a `bl` to a COMDAT is
   not a hidden deduction waiting to happen.
2. **A byte-perfect function under a wrong name is invisible to every tool here** except
   `report_diff.py`'s byte count. `unit_fit.sh` listed the 176-byte `__as__` as *extra* (a COMDAT
   copy the linker drops) and the unit still could not flip. When a unit sits at `total_code - N`
   with `matched_code` short by exactly the size of one function objdiff reports no percentage for,
   read `nm` on `build/G2ME01/src/...o` before rewriting the body. The bytes were never the problem.

## Not done / follow-ups

- **The unit is still `NonMatching` and `flip_test.sh` was not run**, per this item's instruction
  ("it stays NonMatching, do not run flip_test to decide"). Its `.text` is now 10768/10768 with every
  function named and matched, and the remaining 17 `unit_fit` extras are COMDAT copies mwldeppc drops
  (the same shape as CAi, which flips carrying 224 bytes of them). **A `match` item on
  `MetroidPrime/Weapons/CPlasmaProjectile` is the natural next step** and is the one thing that would
  turn these 21 functions into linked ones — that is measured plausibility, not a flip I ran.
- `NEW:` none. No new blocker and no function reachable-but-unwritten was found.
- `WALL:` none. Nothing sat below 100% across several spellings.
- `docs/HANDOFF.md`'s state block was rewritten by the judge itself (`goal_check.sh` runs
  `check_docs_claims.py --write`), 12448 → 12449 and DOL units 10900 → 10901. Not hand-edited.