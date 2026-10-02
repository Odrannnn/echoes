# progress-unit-cslideshow

`MetroidPrime/CSlideShow` matched_functions **10 -> 18 / 76**, unit fuzzy 20.92% -> 22.56%.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate clean, counts 11533 -> 11541,
linked 5625 unchanged, no asm, decl order ok). Unit stays `NonMatching`; not flipped.

All edits are in `src/MetroidPrime/CSlideShow.cpp` plus one return-type change in
`include/MetroidPrime/CSlideShow.hpp`. No `tools/`, `config/`, `configure.py` or
`build/goal/` (other than this file) was touched. No new `.s`/asm, no unit carved.

## Per function (before% -> after%, measured by `tools/fast_try.sh MetroidPrime/CSlideShow`)

| function | before | after | what changed |
|---|---|---|---|
| `GetStickDirection__Fffff` | 88.12% | **100%** | local `uchar direction` -> `uint flags`; case order 1,5,4,6,2,10,8,9 (donor's) instead of ascending. Kills four `clrlwi` and reorders the nine `li r3,N` bodies so the `.data` jump table matches retail byte-for-byte. |
| `AreAllDepsLoaded` | 82.19% | **100%** | `for (int i...)` index loop -> `rstl::vector<...>::const_iterator` loop. Retail walks a pointer and compares against `begin + size*8`; the index form emits `mtctr`/`bdnz`. |
| `SetTexturesLocked` | 62.19% | **100%** | same iterator change. |
| `SetDependenciesLocked` | 62.19% | **100%** | same iterator change. |
| `IsLoaded__SSlideData` | 99.14% | **100%** | `const bool loaded = !null() && IsLoaded();` -> `bool loaded = true; if (null() \|\| !IsLoaded()) loaded = false;` (donor's form). Retail pre-sets `r31=1` and clears it in either failure branch. |
| `AdvanceSlide__10CSlideShowFb` | 94.23% | **100%** | `mSlide += forward ? 1 : -1;` -> `if (forward) ++mSlide; else --mSlide;`. Retail branches and materialises ±1 in r3; the ternary folds into `li r3,-1 / beq / li r3,1 / add`. |
| `Reset__SSlideData` | 90.80% | **100%** | `mMulColor = CColor::White().WithAlphaOf(0.f);` -> two statements, `= CColor::White();` then `SetAlpha(0.f);`. Retail stores the rgb word (`stw ...68`) and the alpha byte (`stb ...71`) separately; the single-expression form makes the compiler merge them with an `rlwimi`. |
| `LoadTXTRDep` | 38.86% | **100%** | guard inverted to the donor's `if (tag != nullptr && tag->type == 'DGRP') { ... } else return false;`, plus `push_back` -> `push_back_unsafe` behind the donor's explicit `if (size() + 1 > capacity()) reserve(size() + 1)`. Retail's `push_back_unsafe` is just the size bump + placement construct with no recheck. |
| `GetGalleriesUnlocked` | 79.48% | 98.04% | local `uchar flags` -> `uint`, and the `CPersistentOptions&` local removed so each of the three `SystemOptions().FindEnvironmentVariable(...)` chains reloads `gpGameState` like retail. Not at 100%: two string-literal offsets (`+249`, `+315`, `+335` from `lbl_803AA070`) vs ours (`+0`, `+13`, `+33` from `@stringBase0`). See "Still open" below. |

Functions already at 100% before this run and still at 100%: `__ct__10CSlideShow10SSlideDataFv`,
`InitializeViewport__SSlideData`, `Draw__SSlideData`, `GetIsContinueDraw`, `SetShowControls`,
`IsControlsAnimating`, `UpdateControls`, `SetPanSfx`, `SetZoomSfx`, `IsDataLoreResearchScan`.

## Lesson worth keeping: three cheap codegen shapes in this repo

1. **`uint`/index-free locals beat `uchar`/index forms.** `GetStickDirection` and
   `GetGalleriesUnlocked` both lost ~10% purely to `clrlwi` emitted for a `uchar`
   accumulator. Retail's locals are 32-bit.
2. **`rstl::vector` iteration must go through the iterator typedef.** Retail loops are
   `it = begin; it != end; ++it` over a raw `T*` compared against `data() + size()*sizeof(T)`.
   A hand-written `for (int i = 0; i < v.size(); ++i)` compiles to a counted loop
   (`mtctr`/`bdnz`/`cmpw`) and never matches, even at identical semantics. This fixed
   three functions at once.
3. **Check whether retail stores a struct as one word plus byte updates.** `Reset`'s
   alpha was the tell: `stw` at +68 then `stb` at +71 is two statements, not one
   expression. `CColor::WithAlphaOf` merges them and costs a register-merge instruction.

## Still open (measured, not attempted further)

- **`__dt__10CSlideShowFv` 88.49%.** Retail's body is
  `gpResourceFactory->GetResLoader().RemovePakFile(gpTweakSlideShow->GetPakName())`
  plus the vptr store, then the member destructors we already emit. `GetPakName()` is
  `fn_8021437C` (size 0x28, copies `mData + 0x10`, i.e. `SLdrTweakSlideShow::pakFile`, into
  a returned `rstl::string`). **Blocked here**: that symbol is not claimed by
  `config/G2ME01/splits.txt`, so it lives in another unit's `.text` and defining it means
  editing a unit this item does not own. `CTweakSlideShow` in this tree also has no
  accessor and no `SLdrTweakSlideShow` accessors at all.
- **`__sinit_CSlideShow_cpp` 40.59%.** Retail's static init does `__ct__CVector2f(0,0)`
  (ours, matched) **and then** constructs a `rstl::string` from `lbl_803AA070 + 363`
  ("SlideShow"), registering it via `__register_global_object` with its dtor. Ours only
  does the vector. So retail has a second file-scope `rstl::string` static this tree does
  not declare. Which string it is (other than "SlideShow" at +363) is not determined;
  guessing it would add a global and could move `.data`/`.bss` for this unit.
- **`GetGalleriesUnlocked` 98.04%, one instruction class short.** The body is right; the
  three string literals sit at different offsets in retail's pool
  (`lbl_803AA070 + 249/+315/+335`) than in ours (`@stringBase0 + 0/+13/+33`). Retail's
  `+249` is `addi r4,r4,249` after the HA/LO pair; ours folds the offset into the
  ADDR16 addend. Matching needs the unit's whole string pool laid out the way retail's is,
  which depends on the literals the *other* 40 unwritten functions contribute. Not a
  spelling problem - a pool-layout problem.
- **`UpdateMusicVolume__10CSlideShowFff` 2.44%, body is still a TODO** (retail 164 B).
  Retail clamps `f1/f2` to [0,1], multiplies by a constant `0.7421875f`, reads
  `gpGameState->...+152`, and calls two `mAudio` methods. Needs the real tweak fields;
  not attempted.
- **40 retail functions this object does not emit at all** (`fn_8018C578` .. `fn_801916BC`,
  all at 0%): mostly `rstl::vector<STexture>`/`vector<CToken>`/`vector<SGalleryData>`
  helpers (assign, insert, uninitialized_copy, destroy loops, `lower_bound`) reached from
  `LoadSlide`, `BuildGalleryLists` and `OnMessage`, which are still TODO stubs. They are
  downstream of the big unwritten functions, not independently writable.
- **`__ct__10CSlideShowFv` 25.29%** and **`OnMessage` 0.28%**: the two large unwritten
  bodies. Echoes' `OnMessage` is a six-phase machine (pak load -> deps -> string table ->
  gallery lists -> audio -> per-frame) that is not the donor's five-phase one; it needs the
  `CTweakSlideShow` accessors to write at all.

WALL: __dt__10CSlideShowFv 88% - needs fn_8021437C (CTweakSlideShow::GetPakName), a symbol in another unit's unclaimed range; cannot be defined from this unit.
WALL: __sinit_CSlideShow_cpp 41% - retail initialises a second file-scope rstl::string this tree does not declare; identity unproven and guessing it would move .data/.bss.

## Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/fast_try.sh MetroidPrime/CSlideShow      # per-function percentages, no DOL relink
python3 tools/check_decl_order.py --unit MetroidPrime/CSlideShow
./tools/goal_check.sh build/goal/item.json       # the judge; -> PASS
```
## Run 2 (lane 9, 2026-10-02)

The earlier run's edits were NOT in this tree (fast_try showed 10/76 at start; report.base 13081). Re-applied
the same spellings from the notes above to `src/MetroidPrime/CSlideShow.cpp`, plus
`GetGalleriesUnlocked` return type `uchar` -> `uint` in `.cpp` and `include/MetroidPrime/CSlideShow.hpp`
(96.88% with uchar return, 98.04% with uint).

Measured: `fast_try.sh MetroidPrime/CSlideShow` 10 -> 18 / 76; `./tools/goal_check.sh build/goal/item.json`
-> PASS (matched 13081 -> 13089, linked 6187 unchanged, no asm).
Same eight functions at 100% as listed above; GetGalleriesUnlocked 98.04%. The open items above
(`__dt__`, `__sinit`, string-pool offsets, UpdateMusicVolume, ctor, OnMessage) were not re-attempted in
this run; their WALL lines from run 1 are not re-measured here.
