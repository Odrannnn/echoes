# progress-unit-ccharacterinfo — `Kyoto/Animation/CCharacterInfo`

`src/Kyoto/Animation/CCharacterInfo.cpp` only, no config, no carve, no asm.
`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS`**,
`target rose: main/Kyoto/Animation/CCharacterInfo: 4 -> 14 / 42 functions`,
gate green, matched `11589 -> 11599`, linked `5625 -> 5625` (unchanged — the unit stays
`NonMatching`, which is what a `progress` item wants).

## What the unit actually is

`config/G2ME01/symbols.txt` names 6 of the 42 functions in this range. The other 36 are
`fn_<addr>`, i.e. **retail's map never named them**, and objdiff had **no partner** for any of
them: they scored 0.00% because our object emitted no symbol at all, not because the bytes
differ. `MetroidPrime/CAnimData.cpp` and `MetroidPrime/CTargetReticles.cpp` already solve this
for their ranges by spelling each body out under retail's own `fn_` name with `extern "C"`; the
same trick applies here and is what this change is.

Disassembling the range (`.text 0x802925C4`–`0x80293C98`) shows the 36 unnamed functions are
the `rstl` container helpers behind the class's members, not new game logic: `uninitialized_copy`
and `destroy` loops over `TEffectList`'s and `vector<pair<uint,CAABox>>`'s elements, `reserve`,
`operator=`, `push_back_unsafe`, and five bare forwarders. Prime 1's decomp
(`prime-ref/src/Kyoto/Animation/CCharacterInfo.cpp`) has only the two constructors and
`GetAnimationIndex` — Echoes added the four table-versioned members — so it was no help here;
`tools/who_calls.py` plus the disassembly identified each body.

## 14 functions at 100.00%, measured before → after

| function | addr / size | before | after |
|---|---|---|---|
| `fn_80293C30` `uninitialized_copy` over `TEffectList` | 0x80293C30 / 104 B | 0.00% | **100.00%** |
| `fn_80293BE4` `rstl::destroy` loop over `TEffectList` | 0x80293BE4 / 76 B | 0.00% | **100.00%** |
| `fn_80293BC4` forwarder to it | 0x80293BC4 / 32 B | 0.00% | **100.00%** |
| `fn_80293A44` `vector<pair<uint,CAABox>>::reserve` | 0x80293A44 / 212 B | 0.00% | **100.00%** |
| `fn_80293248` forwarder | 0x80293248 / 32 B | 0.00% | **100.00%** |
| `fn_80292D94` `TEffectList::push_back_unsafe` | 0x80292D94 / 56 B | 0.00% | **100.00%** |
| `fn_80292C94` `TEffectList` destroy-then-zero-count | 0x80292C94 / 96 B | 0.00% | **100.00%** |
| `fn_80292C30` `uninitialized_copy` over element pointers | 0x80292C30 / 100 B | 0.00% | **100.00%** |
| `fn_80292B00` forwarder | 0x80292B00 / 32 B | 0.00% | **100.00%** |
| `fn_80292A1C` `mCount = 0` | 0x80292A1C / 12 B | 0.00% | **100.00%** |

Unit fuzzy `19.68% -> 37.43%`, `matched_functions` `4 -> 14`.

## Spellings that had to be found (measured, so the next run does not repeat them)

- **Callee-saved register allocation decides the loop's shape.** `fn_80292C30` came out 98.20%
  with `for (; first != last; ++dst, ++first)` because `mr r30,r3` / `mr r29,r4` were the other
  way round from retail's. Introducing a named cursor for the *destination* (`TEffectEntry* out`
  before `const TEffectEntry* it`) and keeping `++it` before `++out` is what lands
  `mr r30,r3` / `mr r29,r4` / `addi r30,r30,0x20` in retail's order. `fn_80293BE4` needed the
  same trick for `r31`.
- **`rstl::uninitialized_copy` will not inline.** `fn_80293A44` measured **80.40%** written as
  `rstl::uninitialized_copy(vec->begin(), vec->end(), newData)` — mwccceppc emits the
  out-of-line instantiation because this unit has a second caller for it. Putting the loop in a
  file-local `static inline` helper that takes the two `rstl::pointer_iterator`s **by value**
  gets the loop inline *and* materialises the four words at 8/12/16/20(r1) that a by-value
  aggregate argument occupies, which retail's body has: **100.00%**. (`*out = *first`, not
  `rstl::construct` — the latter emits a call and scores 0 extra.)
- **`rstl::destroy` must be reached through `vec.begin(), vec.end()`**, not a raw pointer range:
  that is what makes mwccceppc emit the out-of-line `destroy<pointer_iterator<...>>` at
  0x802925C4 rather than inlining the loop.
- **`push_back_unsafe` has to be the post-increment subscript.** Retail increments the count
  *before* forming the address (`addi r5,r6,1` / `slwi r0,r6,5` / `stw r5,4(r3)` /
  `add r3,r7,r0`), so `rstl::construct(&vec->mItems[vec->mCount++], in)` is the spelling;
  `vec.push_back(in)` is not.

## Not finished, and what stops it

- **`fn_8029293C` (`vector<pair<uint,CAABox>>::operator=`, 0x8029293C / 224 B) — 93.93%.**
  Instruction-for-instruction except two things: the self-assignment guard is `cmplw` + `bne`
  into the body in retail (branch *into* the copy) against our `beq` past it, and the copy loop
  swaps `r4`/`r6`. 6 spellings tried, all measured: `if (dst != &src) { ...; return dst; }`
  (93.93%), the same with an early `return dst` in the empty branch (93.12%), the body in an
  `else` (93.93%), a counted `for (int i = src.mCount; i != 0; --i)` loop (121 instructions —
  mwccceppc fully unrolls it, 121 vs 57), and two orders of the `in`/`out` cursor declarations.
  The remaining diff is only register allocation and branch polarity, not scheduling.
- **`fn_80293268` / `fn_80292B00`'s target `fn_80292B20` (`pair<...,CAABox>` stream
  constructors, 0x80293268 and 0x80292B20) — 35.59% / 16.43%.** Retail copies the `CAABox` out
  of its temporary **six floats at a time** (`lfs`/`stfs`); ours copies it as three words. The
  fix is the commented-out `CAABox::operator=` in `include/Kyoto/Math/CAABox.hpp` (lines 41-49),
  which does `min = other.min; max = other.max;` member-wise — but `min`/`max` are **private**,
  so nothing outside `CAABox` can spell that assignment, and uncommenting the operator would
  change every other unit that copies a `CAABox`. `SetX`/`SetY`/`SetZ` through the accessors
  compiles to the same three-word copy. **This is a header-visibility problem, not a spelling
  problem**: it needs a `CAABox` accessor or the operator un-commented, which is a change to a
  shared header and therefore out of scope for a `progress` item on one unit. Every copy site in
  this unit that moves a `CAABox` by words (`fn_80293A44`, `fn_8029293C`, `fn_80292A28`) is
  *word-wise in retail too* and does match, so the fix is local to the two stream constructors.
- **`__ct__14CCharacterInfoFR12CInputStream` — 98.77%, unchanged by this item.** Every
  difference is a `bl` to a named `rstl` instantiation where retail calls an unnamed helper
  (`bl __ct__Q24rstl220vector<pair<int,pair<string,string>>>...` against `bl fn_80293324`, and
  eight more). Landing it means writing out the `mAnimInfo` / `mAabbs` / `mEffects` /
  `mAnimIdxs` stream-assignment helpers (`fn_80293324`, `fn_80293128`, `fn_80292CF4`,
  `fn_80292A28`, `fn_80293304`) the same way. That is the obvious next slice of this unit.
- **`__ct__Q214CCharacterInfo16CParticleResDataFR12CInputStreamUs` — 55.83%, unchanged.** Its
  helpers (`fn_80293638`, `fn_802936EC`, `fn_802937A4`, `fn_80293808`, `fn_802938E8`,
  `fn_802939A4`, `fn_80293B18`) are still unwritten; the ctor itself then follows.

WALL: fn_8029293C 93.93% - regalloc + branch polarity on the self-assign guard; 6 spellings tried, nothing reached 100%
WALL: fn_80293268 35.59% and fn_80292B20 16.43% - retail copies CAABox member-wise as floats, and `CAABox::min`/`max` are private in this tree