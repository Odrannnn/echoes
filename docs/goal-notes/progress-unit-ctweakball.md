# progress-unit-ctweakball

`src/MetroidPrime/Tweaks/CTweakBall.cpp` only. All 9 unmatched functions in the unit now match;
the unit is at 84/84 functions and 100% fuzzy / 100% matched in `build/report.json`. It stays
`NonMatching` (see "The flip does not pass" below - that is pre-existing and not from this change).

## Before / after, per function (from `build/report.json`)

| function | before | after |
| --- | --- | --- |
| `GetMaxBallTranslationAcceleration__10CTweakBallCFi` | 99.9375% | 100% |
| `GetBallTranslationFriction__10CTweakBallCFi` | 99.9375% | 100% |
| `GetBallTranslationMaxSpeed__10CTweakBallCFi` | 99.9375% | 100% |
| `GetBallForwardBrakingAcceleration__10CTweakBallCFi` | 99.9375% | 100% |
| `GetBallSlipFactor__10CTweakBallCFi` | 7.0833% | 100% |
| `GetBoostBallDamage__10CTweakBallCFv` | 75.294% | 100% |
| `GetCannonBallDamage__10CTweakBallCFv` | 75.294% | 100% |
| `GetScrewAttackDamage__10CTweakBallCFv` | 75.294% | 100% |
| `GetDeathBallDamage__10CTweakBallCFv` | 75.294% | 100% |

Unit totals: 75/84 -> 84/84 matched functions. Tree totals from `tools/decomp_build.sh`:
`All: 33.42% fuzzy, 26.45% matched, 12.64% linked (11777 / 28465 functions)`, up from
`11768 / 28465` on the clean tree. Judge: `matched 11768 -> 11777`, 0 functions worse
(checked by diffing `build/goal/judge/report.base.json` against `build/report.json` per function).

`./tools/goal_check.sh build/goal/item.json` -> **PASS progress-unit-ctweakball**.

## What each fix was, and why

### 1. The four `CDamageInfo` accessors (75.29% -> 100%)

objdiff on `GetBoostBallDamage__10CTweakBallCFv` showed our function was 68 bytes against
retail's 56, the difference being three instructions (`stw r31,12(r1)`, `mr r31,r3`,
`lwz r31,12(r1)`) around the `bl`. Retail keeps the hidden return pointer live across the
constructor call and spills it to r31; a direct `return CDamageInfo(mData->...)` is what makes
mwcceppc keep it live, and the four functions then come out 12 bytes long each.

`src/MetroidPrime/Tweaks/CTweakPlayerGun.cpp:14` already documents the fix and why, for the same
reason in the same unit family: a file-local

```cpp
static inline CDamageInfo LdrDamage(const SLdrTDamageInfo& data, bool charged = false,
                                    bool comboed = false, bool noImmunity = false,
                                    bool flag = false) {
  return CDamageInfo(data, charged, comboed, noImmunity, flag);
}
```

and `return LdrDamage(mData->...)` at each of the four call sites. The helper and a comment
referencing the requirement were added to `CTweakBall.cpp` above the first accessor.

### 2. The four surface-indexed switch accessors (99.94% -> 100%)

Only difference was one swapped pair of case bodies. Retail's jump tables for all four functions
are `[0x20,0x2c,0x38,0x44,0x50,0x68,0x5c,0x74]` relative to the function base: table entries 5 and
6 point past each other. mwcceppc lays case blocks out in source order and fills the table by case
value, so writing `case 6:` before `case 5:` reproduces it. The *values* returned are unchanged -
`case 5` still reads `forwardAccelPhazon` and `case 6` still reads `forwardAccelLava`; only the
declaration order moved, so the code is byte-identical in meaning.

**Lesson (general):** when a jump-table function sits at 99.9% with matching instruction counts,
read the retail jump table before touching the body. A permutation in the table means the source
declared its cases out of numeric order; the fix is the declaration order, not the logic.

### 3. `GetBallSlipFactor` (7.08% -> 100%)

This one was genuinely wrong, not just permuted. Retail returns **2.0 / 0.01 / 44.0**, not Prime 1's
10000 / 1000 / 2000 - the previous source was carried over from the donor and never checked
against this binary.

Measured from the linked ELF (`tools/dis.sh 0x80217354 96`), three `lfs f1,-NNNN(r2)` reads
resolve through `_SDA_BASE_ = 0x8041fd80` (`powerpc-eabi-nm build/G2ME01/main.elf`):

| `lfs` displacement | address | bytes | value |
| --- | --- | --- | --- |
| `-19108(r2)` | `0x8041b2dc` | `40000000` | 2.0f |
| `-19104(r2)` | `0x8041b2e0` | `3c23d70a` | 0.01f |
| `-19100(r2)` | `0x8041b2e4` | `42300000` | 44.0f |

and retail's table is `[0x20,0x28,0x30,0x38,0x40,0x50,0x48,0x58]`, i.e. the same 5/6 inversion, so
`case 6:` also precedes `case 5:` here. Per-surface: 0->2, 1->2, 2->0.01, 3->2, 4->44, 5->44,
6->44, 7->44, default->2.

**Lesson:** the Prime 1 donor is a donor for *shape*, not for constants. Any literal the donor
supplies still has to be read out of this binary's `.sdata2`.

## The flip does not pass - pre-existing, not from this change

`./tools/flip_test.sh MetroidPrime/Tweaks/CTweakBall.cpp` fails: every REL and the DOL come out
different, on a **clean tree** as much as on this one. Verified by stashing this change and
re-running the flip on the untouched file:

```
$ git stash push src/MetroidPrime/Tweaks/CTweakBall.cpp
$ ./tools/flip_test.sh MetroidPrime/Tweaks/CTweakBall.cpp
   FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
kept: 0 / 1   failed: 1   skipped: 0
```

Same failure both ways, so the unit was never flippable and my change did not make it worse.
`tools/unit_fit.sh` gives the likely reason:

```
   .sdata2    claimed      -   ours     16   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
```

Our object emits 16 bytes of `.sdata2` (the four float constants `2.f`/`0.01f`/`44.f` plus the
`M_PIF/180` the `CRelAngle::FromDegrees` inline needs) that `splits.txt` does not claim for this
unit. Retail keeps those constants in a neighbouring unit's claimed range, so putting our object
in the link relocates real bytes. Marking the unit `Matching` therefore needs a carve that also
moves the `.sdata2` claim, which is a four-file change (`configure.py`, `splits.txt`,
`files.cmake`, the unit's own claim) and out of scope for a `progress` item. Not attempted.

## Gates

```
python3 tools/check_decl_order.py --unit main/MetroidPrime/Tweaks/CTweakBall
  ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/Tweaks/CTweakBall.cpp
  .text 1980/1980 fits; .data 160/160 fits; no extra functions
./tools/decomp_build.sh main/MetroidPrime/Tweaks/CTweakBall
  main/MetroidPrime/Tweaks/CTweakBall: 100.00% fuzzy, 100.00% matched (84 / 84 functions)
./tools/goal_check.sh build/goal/item.json
  goal_check: PASS progress-unit-ctweakball
```

No `asm` added; the only file touched is `src/MetroidPrime/Tweaks/CTweakBall.cpp`. Not committed.

## Left for a `match` item

NEW: match-ctweakball-sdata2 | match | MetroidPrime/Tweaks/CTweakBall | the unit matches
84/84 and objdiff's `case 6` before `case 5` ordering is what it needs, but the flip fails because
our object emits 16 bytes of `.sdata2` that `splits.txt` does not claim for it; needs a carve that
moves the `.sdata2` claim into this unit (configure.py + splits.txt + files.cmake + the claim).