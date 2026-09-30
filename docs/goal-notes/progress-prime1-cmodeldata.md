# progress-prime1-cmodeldata — `MetroidPrime/CModelData`

`kind: progress`. The unit stays `NonMatching`; `flip_test.sh` was not run and is not the verdict
here. Every number below is from `build/report.json` measured in this worktree, never recalled.

## Result

`main/MetroidPrime/CModelData` **25 -> 31** of 49 functions matched (fuzzy 46.85% -> 51.01%,
matched code 30.41% -> 43.79%). Global `matched_functions` **9747 -> 9753**; `linked` unchanged at
4894; `All:` 21.88% -> 21.90% matched. Diff: `src/MetroidPrime/CModelData.cpp` only.

**0 functions anywhere got worse** (per-function diff of the whole report against
`build/goal/judge/report.base.json`), so the +6 is net. `tools/gate.sh` (with the DOL sha1, all 86
RELs, report diff, module wiring, docs claims, port probe, decl order, files.cmake) prints
`GATE PASS`. `python3 tools/check_symbol_names.py` -> `502 units, 0 missing names`.
`tools/unit_fit.sh` reports the same 4 pre-existing extra functions as the base tree, so this change
adds none.

## Per function: before % -> after %, and how Prime 1 was used

| function | before | after | how |
|---|---|---|---|
| `GetAnimationDuration__10CModelDataCFi` | 68.46 | **100.00** | Prime 1's shape verbatim, small edit |
| `GetBounds__10CModelDataCFRC12CTransform4f` | 96.10 | **100.00** | needed a small edit |
| `GetBounds__10CModelDataCFv` | 96.70 | **100.00** | needed a small edit |
| `Touch__10CModelDataCFQ210CModelData11EWhichModeli` | 4.17 | **100.00** | Prime 1 adapted to Echoes |
| `GetNumShaders__10CModelDataCFv` | 13.33 | **100.00** | no Prime 1 equivalent; from retail asm |
| `IsLoaded__10CModelDataCFi` | 95.58 | **100.00** | needed a small edit |
| `Touch__10CModelDataCFv` | 2.38 | **no** (98.21 best) | Prime 1 adapted; register-allocation wall |
| `GetIsLoop__10CModelDataCFv` | 33.12 | **no** (62.50 best) | Prime 1 verbatim; wall is a shared header |
| `RenderParticles__10CModelDataCFRC14CFrustumPlanes` | 90.91 | **no** | no change; needs an out-of-line callee |
| `__ct__10CModelDataFRC8CAnimRes` | 29.03 | **no** | no change; blocked on 4 missing symbols |
| `Render`, `RenderUnsortedParts`, `DisintegrateDraw`, `IsDefinitelyOpaque` | 0.70/1.22/1.28/6.36 | **no** | no change; see below |

### `GetAnimationDuration` — Prime 1's body, with `mAnimData.null()` instead of `HasAnimation()`

Prime 1:
```cpp
if (mAnimData.null()) return 0.f;
return mAnimData->GetAnimationDuration(anim);
```
That alone is not enough. `HasAnimation() ? ... : 0.f` and
`if (HasAnimation()) { return ...; } return 0.f;` both stay at **68.46%**; the ternary and the
`HasAnimation()`-then-call form both compile to `beq` forward, and retail branches *over* the
`lfs f1,0.0(r2)` with `bne`. Only spelling the test as the direct `mAnimData.null()` test gets the
`bne`. Measured, `tools/try_edit.py` on 5 spellings:

```
p1-null-then-call            100.00      if-has-call-else-zero   68.46
p1-null-then-call-bracesless 100.00      ternary-has              68.46
                                          ternary-null            100.00
```

### `GetBounds(const CTransform4f&)` and `GetBounds()` — the `Scale` overload was the whole diff

Both were already at 96%; the *only* real difference was that retail loads the three scale floats
into `f1`/`f2`/`f3` and calls `Scale__12CTransform4fFfff`, while this tree called the
`Scale(const CVector3f&)` overload. Prime 1 already writes the three-argument form; this tree did
not. Changing both call sites to
`CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ())` took both to 100% with no other
edit — the `AccumulateBounds` / `GetTransformedAABox` / per-component-multiply tails were already
right (Prime 1's early-out `if (!mXrayModel && !mInfraModel) return ...` is **not** in Echoes; the
Echoes object has no such branch, so do not copy it).

### `Touch(EWhichModel, int)` and `GetNumShaders` — Echoes-only, from retail asm

Prime 1 has no `mTexturesLocked` and no `GetNumShaders`, so these came from the disassembly
(`tools/dis.sh 0x800E5D20 0x60`, `0x800E4BF0 0x3C`). Both needed two local shapes that this tree
already uses elsewhere (`src/MetroidPrime/CModelTouchParts.cpp`, `Player/CGunEffectTouch.cpp`), and
both took 100% on the first spelling:

* `SModelHolder { char[8]; CModel* x8_model; char[4]; }` plus `extern "C"` declarations of
  `fn_80027B44` / `fn_80027AE8` (defined in `src/MetroidPrime/CAnimData.cpp`, which owns
  0x80027AE8..0x80027B68). No header edit.
* `SShaderCount { char[0x1c]; int mNumShaders; }` — the `int` at **`CModel`+0x1C**, i.e.
  `mMatSets.mCount`, reached by `reinterpret_cast` because `mMatSets` is private and upstream
  exposes `GetMatSetCount()` only under `#ifdef TARGET_PC`. Claiming the one word is what
  `CModelTouchParts.cpp` already does; no header edit.

`GetNumShaders`'s static arm needs `*(*mNormalModel)` (a `TLockedToken*`, then its `CModel*`) while
the animated arm needs only `*mAnimData->GetModelData()->GetModel()`: `TLockedToken::operator*`
returns the pointer, not a reference. A file-local helper wrapping that was tried and **not** used:
mwcceppc emitted it as an out-of-line call and both functions dropped.

### `IsLoaded` — a named local is load-bearing

Retail keeps `mAnimData` in a callee-saved register (`lwz r31,16(r3)`, then all three animated-model
loads at 280/316/324 off `r31`). Naming it as `const CAnimData* const animData = mAnimData.get();`
and re-testing the two optional models in `if (init; ...)` form gives that. Without the local the
compiler rematerialises `this->mAnimData` into `r4` after the first call and the function is one
instruction longer. Measured:

```
named-animdata  100.00   (this is what landed)
current          95.58
ptr-model        95.51
not-and-inline   93.03
```

## Measured walls (spelling lists so the next run skips them)

`WALL: CModelData::Touch__10CModelDataCFv 98.21% - 11 spellings all reach 98.21%; the block
structure and the whole loop nest are byte-identical to retail and the only difference is which
callee-saved register holds the two `which` counters (retail r30/r29, ours r31 for both), with the
model pointer and the count one register lower in consequence. Nothing in the source reaches it.`

Spellings tried, all 98.21% unless noted: current (int loop var, `const CModel* const model`),
`EWhichModel` loop var, `do/while` on the `which` counter, `while (shader < n)`, count read in the
loop test, two different variable names for the two loops, `const SShaderCount*` local (72.14%),
`if (mTexturesLocked) { ... }` instead of an early return, `for(;;) break` inner loop (53.10%),
`while (shader != numShaders)` (98.10%).

`WALL: CModelData::GetIsLoop__10CModelDataCFv 62.50% - retail returns the mLoop bit with no bool
normalisation; the only lever is CAnimData::mLoop's declared bitfield type, and changing that shared
header regresses five functions in four other units.`

The 12 bytes are `rlwinm r3,r0,26,31,31` followed by `neg r0,r3 / or r0,r0,r3 / srwi r3,r0,31`,
emitted because `CAnimData::mLoop` is `uchar mLoop : 1` and the return converts an integer to
`bool`. Retail has no normalisation, i.e. its `mLoop` is a `bool` bitfield (Prime 1 agrees:
`bool mAnimating : 1; bool mLoop : 1;`). Changing `include/MetroidPrime/CAnimData.hpp` line 225 from
`uchar` to `bool` was measured and takes `GetIsLoop` to **100.00%**, but the per-function diff of
the whole report then shows **5 regressions**:

```
-7.30  CGSFreeLook::Update__11CGSFreeLookFR9CAnimDatafR13CStateManager   100.00 -> 92.70
-6.70  CGSComboFire::Update__12CGSComboFireFR9CAnimDatafR13CStateManager   98.88 -> 92.18
-3.14  CGunWeapon::PlayAnim__10CGunWeaponFQ212NWeaponTypes12EGunAnimTypeb 100.00 -> 96.86
-2.66  CAnimData::__ct__9CAnimDataFU...                                     87.45 -> 84.78
-0.60  CGunMotion::PlayPasAnim__10CGunMotionFQ28SamusGun15EAnimationStateR13CStateManagerfb 61.54 -> 60.94
```

All five are readers of `CAnimData::GetIsLoop()`, and the header was **reverted** — one function
here is not worth two 100% functions there. Doing it properly means changing the bitfield *and*
re-tuning those five readers, which is a real job on other units and is not this item's work.

The `mAnimData.null()` vs `HasAnimation()` distinction from `GetAnimationDuration` applies here
too: with `uchar mLoop`, `if (!HasAnimation()) return false;` scores 11.25% and
`if (mAnimData.null()) return false;` scores 62.50% (ternary and `HasAnimation()`-then-loop both
11.25%). All 6 spellings measured, none reached 100% with the `uchar` bitfield.

## Not attempted, and why (all measured, none guessed)

* **`RenderParticles` (90.91%)** — the 4 bytes are `bl fn_800295BC` (an out-of-line
  `CAnimData` method that does `this+376; bl AddToRendererClipped`) against this tree's inlined
  `addi r3,r3,376; bl AddToRendererClipped`. `fn_800295BC` is inside
  `MetroidPrime/CAnimData.cpp`'s claimed range (`splits.txt` 0x80025D3C..0x8002F7A8), so calling
  it by name needs a definition in that other unit, and defining it in *this* file would add a
  function the retail `CModelData` object does not have.
* **`__ct__FRC8CAnimRes` (29.03%)** — retail's body calls four symbols this tree does not define
  under those names: `GetFactory__24CCharacterFactoryBuilderFRC8CAnimRes`,
  `GetCharInfo__17CCharacterFactoryCFi`, `__dt__9CAnimDataFv`,
  `SetModelScale__9CAnimDataFRC9CVector3f`. (`White__6CColorFv`, `__ct__6CTokenFRC6CToken`,
  `GetObj__6CTokenFv` and `__dt__6CTokenFv` do exist, in `MetaRender/Carve80271238.cpp`,
  `ScriptObjects/CRipperForwarders.cpp`, `Player/CGameStateStreamCtor.cpp` and
  `CModelDataDefaultCtor.cpp`.) `CCharacterFactory::CreateCharacter` exists but is a C++ member
  with a different mangled name than the `bl` retail emits. Four new symbol definitions is a
  separate item.
* **`IsDefinitelyOpaque` (6.36%)** — Prime 1's body is right, but both arms end in a call to retail's
  unnamed `fn_80310F14` (`CModel`'s opaque query, 0x80310F14). There is no `CModel::IsDefinitelyOpaque`
  and no definition of that symbol in the tree. Declaring it `extern "C"` and calling it would add
  an undefined symbol to the port build, which `gate.sh`'s port probe rejects; defining it in this
  file would add a function the retail unit object does not have.
* **`Render` / `RenderUnsortedParts` / `DisintegrateDraw` / `RenderNoise` / `RenderSolid` /
  `RenderModelMultipleTimesWithFlags` / `MultipassDrawCallback` / `LockTextures` /
  `SetupWorldSpacePortalPlane` / `SetEchoModel` / `SetDarkModel`** — untouched. `LockTextures` also
  wants `fn_80310E8C` (undefined in the tree, same problem as `fn_80310F14`).
* **`AdvanceAnimation(float, CStateManager&, TAreaId, bool)` (33.89%) and
  `AdvanceAnimation(float, CRandom16&, bool)` (79.81%)** — untouched; both call `CAnimData::Advance`
  / `AdvanceIgnoreParticles` with a signature this tree's `CAnimData.hpp` does not have (Prime 1's
  is `Advance(dt, ScaleCopy(), mgr, aid, advTree)`, this tree's is a 7-argument overload), so
  Prime 1's source does not transfer. Not in this item's function list either.

## Codegen rules worth keeping (not `NEW:` items)

* When retail's branch jumps *over* the fallback block rather than out to it, spell the test with
  the direct member predicate (`mAnimData.null()`), not through an inline wrapper
  (`HasAnimation()`), and use `if`/`return`, not a ternary. `HasAnimation() && x`,
  `!HasAnimation()` and `HasAnimation() ? x : y` all compile to the same short-circuit/forward-branch
  shape and none of them reproduce it.
* mwcceppc does **not** inline a file-local helper here even at `-O4,p` with
  `inline_max_size(125)`; a `static` function wrapping a two-word dereference became a real `bl`
  and cost both call sites. Inline the expression instead.
* Where retail keeps one pointer in a callee-saved register across several calls and this tree
  rematerialises the load, a named local for that pointer is the fix — but note
  `src/MetroidPrime/CModelDataModelSlots.cpp:135` records the opposite requirement for
  `PickAnimatedModel`, where a named local stops the reload retail wants. It is per-variable, not
  a general rule.

## Review rejected run 2 (2026-09-29 23:56:24Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/CModelData.cpp:29-33` and `:39-42` cast real 64-bit host objects (`CSkinnedModel&`, `CModel*`) to structs whose members sit at the retail 32-bit offsets (+8 and +0x1C), and use them at `:145`, `:157`, `:163`, `:428-432`. `src/Kyoto/Graphics/CModelTouch.cpp:53-55` records that those members are at 0/8/16/40/64 on the host, so the animated `Touch` arms would hand `fn_80027B44` a pointer read out of `TLockedToken::mLockHeld`+padding, and the static arms would take an uninitialised `int` as a shader count and loop `CModel::Touch` over it. The tree already has the host-correct answer — `CModel::GetMatSetCount()` at `include/Kyoto/Graphics/CModel.hpp:95`, guarded by `#ifdef TARGET_PC` for precisely this, already used by `CGunEffectTouchAll.cpp:62` — and a fully typed host-safe spelling for the animated arm (`*PickAnimatedModel(which).GetModel()`). Keep the offset and holder spellings on the `#else` branch so the DOL stays byte-identical, and take the accessor and the typed path under `TARGET_PC`; the four `GetMatSetCount()`/typed reads are the only change needed, and the rest of the diff (the six matched functions, the `GetIsLoop` improvement, the notes) can stand.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cmodeldata-L7-2.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cmodeldata-L7-2-review1-20260929T235042.jsonl

## Fix round 1 (2026-09-30, fix worker)

Took the reviewer's paragraph literally and changed only the five sites it names. `SModelHolder`,
`SSShaderCount` and the two `extern "C"` declarations are now inside `#ifndef TARGET_PC`, and each
read has a `TARGET_PC` arm and a `#else` arm:

* `Touch(EWhichModel, int)` animated arm: `(*PickAnimatedModel(which).GetModel())->Touch(shaderIdx)`
  under `TARGET_PC`, `fn_80027B44(reinterpret_cast< const SModelHolder* >(&PickAnimatedModel(which)),
  shaderIdx)` under `#else`.
* `Touch()` animated arm: the `fn_80027AE8` walk spelled out as
  `*PickAnimatedModel(...).GetModel()` + `GetMatSetCount()` + `model->Touch(shader)`, the shape
  `CGunEffectTouchAll.cpp:61-66` already uses on the host.
* `Touch()` static arm: `model->GetMatSetCount()` instead of the `+0x1C` read.
* `GetNumShaders()`, both arms: `GetMatSetCount()` instead of the `+0x1C` read.

The comment above the two structs now records why the 32-bit shapes are DOL-only (`CModel`'s
members are at 0/8/16/40/64 on the host, `TLockedToken` is two pointers wide there).

Verified: `main.dol` sha1 still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `decomp_build.sh` still
`All: 30.00% fuzzy, 21.90% matched, 11.74% linked (9753 / 28465 functions)` and
`main/MetroidPrime/CModelData: 43.79% matched (31 / 49)`, i.e. the judged counts are unmoved;
`link_check.sh` reports 0 compile errors (the host build *did* rebuild `CModelData.cpp.o`, so the
`TARGET_PC` arm compiles) and 250 undefined / 0 duplicates, unchanged from baseline;
`check_raw_offsets.py` ok, 152 sites in 61 files; `check_symbol_names.py` 0 missing.

---

# Run 3 (2026-09-30, lane 3)

Re-measured on this tree first: the unit was already at **31 / 49** (fuzzy 51.01%, matched code
43.79%) from run 2, so nothing here was `STALE:`. The seven functions the item names were at
`RenderUnsortedParts` 1.22, `Render` 0.70, `DisintegrateDraw` 1.28, `IsDefinitelyOpaque` 6.36,
`RenderParticles` 90.91, `GetIsLoop` 62.50, `__ct__FRC8CAnimRes` 29.03.

## Result

`main/MetroidPrime/CModelData` **31 -> 32** of 49 matched (fuzzy 51.01% -> 60.99%, matched code
43.79% -> 47.49%). Global `matched_functions` **10282 -> 10283**; `linked` unchanged at 5043;
`All: 31.27% fuzzy, 23.60% matched, 11.83% linked (10283 / 28465)`.
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`** (gate.sh green: DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86 RELs, report diff, module wiring, docs claims,
port probe, decl order, files.cmake; "target rose: 31 -> 32"; "no asm added").
`tools/unit_fit.sh MetroidPrime/CModelData.cpp` reports the same 4 pre-existing extra template
destructors as the base tree and no new ones. `check_symbol_names.py` -> `505 units, 0 missing`.

Diff: `src/MetroidPrime/CModelData.cpp` and `src/Kyoto/Graphics/CModelPortStub.cpp`.

## Per function: before % -> after %

| function | before | after | how |
|---|---|---|---|
| `RenderUnsortedParts__10CModelDataCFQ210CModelData11EWhichModel...` | 1.22 | **100.00** | Prime 1's shape + Echoes' 4th early-out; **first spelling won** |
| `Render__10CModelDataCFQ210CModelData11EWhichModel...` | 0.70 | 98.43 | 30 spellings measured; Echoes-only, from retail asm |
| the other 16 unmatched | unchanged | unchanged | untouched - see the previous runs' sections |
| `Touch__Fv`, `GetIsLoop`, `RenderParticles`, `__ct__FRC8CAnimRes`, `IsDefinitelyOpaque` | 98.21 / 62.50 / 90.91 / 29.03 / 6.36 | same | untouched; the previous runs' blockers re-confirmed below |

`Render` contributes **0** to the matched count (98.43% is not 100%). It is kept because it is
real decompilation replacing a TODO stub, and the header comment says so.

## `RenderUnsortedParts` - 100.00% on the first spelling

Prime 1's body plus one Echoes-only condition. Measured with `tools/try_edit.py`, five spellings:

```
A-or-4cond                    100.00      <- landed
B-or-4cond-rendersorted        99.939026   (mRenderSorted instead of mRenderUnsortedParts)
C-nested-if-last               92.256096
D-pos-flag-last                99.939026
E-lights-shape-darkfirst       89.45122
```

Three findings, each measured:

* **The fourth early-out is `!mRenderUnsortedParts`** - a member Prime 1 does not have. Retail
  (`0x800E64D8`) loads the flag byte at `+0x14` and branches *over* the fallback when the bit is
  set. mwcceppc's bitfield test is `rlwinm. rD,rS,SH,31,31` with **SH = 25 + (0-based bitfield
  index)**, confirmed against three sites in this tree and in retail:
  `SH=25` -> bit 0 -> `mRenderSorted` (`Render` at `0x800E67B0`),
  `SH=26` -> bit 1 -> `mTexturesLocked` (our own `Touch__Fv`, 98.21%),
  `SH=27` -> bit 2 -> `mRenderUnsortedParts` (`RenderUnsortedParts` at `0x800E64DC`).
  `B`/`D` show the bit really is that one and not `mRenderSorted`.
* **The four tests must be one `||` chain**, not a nested `if` for the flag alone: `C` duplicates
  the fallback block and drops to 92.26%.
* **`static_cast<char>(flags.GetTrans()) > 4`** is what produces retail's `extsb`; the same
  spelling is already 100% at `CActor.cpp:606`. The lights test is
  `lights != nullptr && which != kWM_Dark` - the Echoes-specific `kWM_Dark` replaces Prime 1's
  `kWM_ThermalHot` (`which != kWM_Dark` at `0x800E6548`, identical shape in `Render` at
  `0x800E6734`). Everything else is Prime 1 verbatim: the three-argument
  `CTransform4f::Scale(GetX(), GetY(), GetZ())`, the `gpRender->SetModelMatrix` virtual,
  `(*PickStaticModel(which))->DrawUnsortedParts(flags)` (retail's `lwz r3,8(r3)`, i.e.
  `TLockedToken::operator*`), and the `mRenderSorted = true` epilogue.

## `Render` - 98.43%, a register-allocation wall this run

Retail (`0x800E65D4`, 572 bytes) is **not** Prime 1's `Render`: Echoes splits the passes, so
`Render` returns after the `kWM_Echo` arm instead of falling through, and it ends with
`mRenderSorted = false` (Prime 1 also has that, but there it is a plain assignment).

Read off the bytes:

* `cmpwi r30,2 / bne` at `0x800E65E8` - `if (which == kWM_Echo)` takes a wholly separate arm
  that **returns** at `0x800E67FC`. So `Render` draws only the sorted/solid pass; the unsorted
  surfaces are `RenderUnsortedParts`' job. That is consistent with the new `mRenderUnsortedParts`
  flag.
* Inside it, `lbz r0,4(r27); cmpwi r0,2` (`flags.GetTrans() == CModelFlags::kT_Two`) guards the
  alpha; then `RenderSolid(which, xf, !mRenderFullEchoModel, CModelFlags(kT_One, 0,
  kF_DepthCompare|kF_DepthUpdate, CColor::Black()))` - built on the stack at `r1+8` with
  `x0_` **left uninitialised**, which is exactly this header's four-argument `CModelFlags`
  ctor. `!mRenderFullEchoModel` is `lbz; rlwinm ...,28,31,31; cntlzw; srwi ...,5`.
* `gpRender->SetDestinationAlpha` is `IRenderer` **vtable slot 74** (`lwz r12,296(r12)`). Slot
  `item index + 1` in `include/MetaRender/IRenderer.hpp` reproduces the three slots this file
  uses - 0x40 `SetModelMatrix` (item 15), 0xC8 `SetAmbientColor` (item 49), 0xDC
  `DrawModelDisintegrate` (item 54) - and `CPlayerGun::BeginDarkVisorRender`
  (`gpRender->SetDestinationAlpha(0)`, 100.00%) pins slot 74. Useful for the next run.
* Normal path is Prime 1's, with `scaledXf = xf; scaledXf *= Scale(...)` (the
  `__ct__` + `*=` + `__as__` triple at `0x800E66DC`-`0x800E6710`), `mAnimData->Render(model,
  flags)` - Echoes' two-argument `CAnimData::Render`, not Prime 1's four-argument one - and
  `if (mRenderSorted) DrawSortedParts else Draw`.

The alpha block (`0x800E6610`-`0x800E6670`) is `max(r,g,b)` of the flags' colour bytes, doubled,
capped at 255, and used only as an `!= 0` guard around the two `SetDestinationAlpha` calls. Its
*shape* is `rstl::max_val(rstl::max_val(r,g,b))` + a cap; 30 spellings were measured and the
best is 98.43%. The whole rest of the function matches instruction for instruction.

Residual, from `tools/bytescmp.py` (ours 564 bytes / 141 instructions, retail 572 / 143):

```
+54  ours cmplw r4,r3   | retail clrlwi r0,r4,24   ; a re-mask of the intermediate max we never emit
+58  ours bge           | retail cmplw r0,r3
+5C  ours mr r4,r3      | retail bge
+60  ours rlwinm r3,r4,1,16,30 | retail mr r4,r3
+64  ours li r0,255     | retail rlwinm r3,r4,1,23,30
+68  ours cmplwi r3,255 | retail li r0,255
+6C  ours bgt           | retail cmplwi r3,255
+70  ours mr r0,r3      | retail bge
+74  ours clrlwi. r28,r0,24 | retail mr r0,r3
+78  ours beq           | retail clrlwi r28,r0,24
+7C  ours lwz r3,gpRender | retail clrlwi. r4,r28,24
```

All three differences are MWCC range-propagation, not algorithm: retail's three are what you get
when the compiler cannot prove the intermediate is already a byte. Measured mask-bit ladder for
the doubling (`rlwinm rD,rS,1,MB,30`): `uint` -> `slwi` (MB 0), `ushort` -> MB 16, `uchar` ->
MB 24; retail has **MB 23**, which nothing in the type ladder produced.

`WALL: CModelData::Render__10CModelDataCFQ210CModelData11EWhichModelRC12CTransform4fPC12CActorLightsRC11CModelFlags 98.43% - 30 spellings of the alpha block measured (max via ternary / CMath::Max / rstl::max_val; doubled as int/uint/ushort/short/uchar/schar/uchar-shift/uint-shift; capped by ternary / CMath::Min / rstl::min_val; the SetDestinationAlpha argument as mx / alpha / 255), all between 88.47 and 98.43; the whole function outside the alpha block is byte-identical and the residual is three MWCC range-propagation choices on the doubling's mask bit, the intermediate's re-mask and alpha's byte test.`

Spellings and scores, so the next run does not repeat them (each is one `tools/try_edit.py`
build; the `echo()`/`d()` helpers are in the git-untracked `.tmp/opencode/v_render*.py`, gone
with the tree):

```
R1 min 2*max, arg max        95.86014     T1..T6 uint/uchar/rstl variants      96.76923
R2 min 2*max, arg alpha      95.86014     U1 rstl::max_val + rstl::min_val    97.97203
R3 min 2*max, arg 255        95.86014     U2 named cap  96.76923  U3 two steps 97.04895
R4 no min (2*max)            92.888115    U4 CMath::Min/Max                    88.46853
R5 min, int locals           95.79021     U5 reversed ternary 96.69930  U6 one-line 96.69930
S1/S3 int + uchar locals     96.52447 / 96.76923   X4 static_cast<uchar>(mx)*2 98.04196
X1 uchar nested 97.02797  X3 <<1 96.37763  X5 nested cast 97.97203  X6 CColor by value 96.37763
Y1 ushort doubled            98.426575    Y3 98.04196   Y4 97.97203   Y2/Y4 (ushort variants) 98.426575
Z1 uchar doubled 98.426575   Z2 schar 98.426575   Z3 short 96.22378   Z4/Z5 uchar max 96.92308/97.34266
A2 uchar <<1 98.426575  A3 ushort <<1 98.426575  A4 98.426575  A5 schar 98.426575  A6 98.426575
A1 uchar mx+mx               95.94405
```

The one instruction that has resisted every spelling is `SetDestinationAlpha`'s argument: retail
passes **r4 = the max component**, not `alpha` and not 255, and every spelling that changes it
scores identically (objdiff ignores the argument register's *value*, only the encoded
instruction). So that detail is still unverified.

## Two wall re-checks, both confirmed, neither re-tried

* **`GetIsLoop` (62.50%)** - re-read `include/MetroidPrime/CAnimData.hpp`: `mLoop` is still
  `uchar mLoop : 1`. Retail's `rlwinm r3,r0,26,31,31` is a `bool` bitfield read with no `bool`
  normalisation, and run 2's measurement stands (the `bool` change costs 5 functions in 4 other
  units). **Not re-tried**; a wall, not a spelling problem. Note this run's SH ladder
  (`SH = 25 + bitfield index`) independently confirms retail's SH=26 here is bit 1 of
  `CAnimData`'s flag byte at `+0x2AC` - i.e. the second bitfield, which is what `mAnimating`/`mLoop`
  ordering predicts.
* **`RenderParticles` (90.91%)** - `fn_800295BC` is 0x800295BC..0x800295CC, **inside**
  `MetroidPrime/CAnimData.cpp`'s claimed range (`splits.txt` 0x80025D3C..0x8002F7A8), so it is
  *not* unclaimed: defining a `CAnimData` method there would add a function to that unit that
  retail's object does not have under a new name, and renaming `fn_800295BC` in
  `config/G2ME01/symbols.txt` is a config change this item did not need to make. The 4-byte
  difference stands as run 2 recorded it. **Not re-tried.**

## Also measured this run, deliberately not landed

* **`DisintegrateDraw` (1.28%, 312 bytes)** - fully read (`0x800E62A0`), and it is blocked on a
  signature this tree does not have. Retail's call is `gpRender` vtable slot 55 - which *is*
  `DrawModelDisintegrate` - but with a **pointer to a 16-byte context struct** as the first
  argument, `{ const CModel*, CSkinnedModel*, void*, void* }` (static arm: model at word 0;
  animated arm: 0, the `CSkinnedModel*`, 0, `mAnimData + 0x2B0`). `IRenderer`'s
  `DrawModelDisintegrate` takes `const CModel&`, so the call cannot be written without changing
  that declaration, which other callers use. Measured, not guessed.
* **`IsDefinitelyOpaque` (6.36%)** and **`LockTextures` (1.92%)** - both blocked on the *same*
  carve, which the previous runs did not name precisely. `fn_80310F14` (`CModel::IsDefinitelyOpaque`,
  9 instructions, read) and `fn_80310E8C` (`CModel::LockTextures`, 0x7C bytes, a loop over
  `mMatSets` calling the already-named `UnlockTextures__Q26CModel7SShaderFv`) sit in the
  **unclaimed** gap `0x80310E8C..0x80310F38` between `Kyoto/Animation/DolphinCVirtualBone.cpp`
  and `Kyoto/Graphics/DolphinCModel.cpp` in `config/G2ME01/splits.txt`. A carve of that gap
  would unblock both plus two functions of its own; it needs four coordinated files
  (`configure.py`, `splits.txt`, `files.cmake`, the new source) and was out of budget here.

## Codegen facts worth keeping

* **MWCC bitfield test/insert ladder**, for the flag byte at a struct's `+0x14`: bitfield *i*
  (0-based in declaration order) is register bit `24 + i` in big-endian, tested with
  `rlwinm. rD,rS,SH,31,31` where **SH = 25 + i**, and written with
  `rlwimi rA,rS,SH,24+i,24+i` where **SH = 7 - i**. Verified against
  `__ct__10CModelDataFv` (100%), our own `Touch__Fv`, and retail's `Render` /
  `RenderUnsortedParts` / `GetIsLoop`. This replaces hand-decoding `rlwimi`, which is a
  dead end (its operand roles do not decode the way the manual reads).
* **`rstl::max_val` / `rstl::min_val` (`include/rstl/math.hpp`) are not interchangeable with
  `CMath::Max/Min` here**: `CMath::Max(a,b)` is `a > b ? a : b` and produced 88.47% here where
  `rstl::max_val` produced 97.97%; `rstl::math.hpp` had to be included.
* **`EFlags` needs an explicit cast**: `-enum int` means `kF_DepthCompare | kF_DepthUpdate` is
  an `int` and will not bind to `CModelFlags(ETrans, uchar, EFlags, const CColor&)`.
* **The port build grows its undefined count when you add a call.** Adding the three calls to
  `CModel::Draw` / `DrawSortedParts` / `DrawUnsortedParts` took `link_check.sh` from 250 to 253
  and `goal_check.sh` failed on `probe link-gap`. Fixed by defining the three empty host bodies
  in `src/Kyoto/Graphics/CModelPortStub.cpp`, next to the existing `~CModel` and
  `CModel::FrameDone`, with the same "delete when `DolphinCModel.cpp` is listed" note. That file
  is not in `configure.py`, so mwcceppc never sees it and the DOL is untouched (sha1 held).
  `link_check.sh` is back to **250 undefined, 0 duplicates, unchanged from baseline**.

## NEW: items

```
NEW: progress-prime1-cmodeldata-l8 | progress | MetroidPrime/CModelData | carve the unclaimed gap 0x80310E8C..0x80310F38 (fn_80310E8C CModel::LockTextures, fn_80310F14 CModel::IsDefinitelyOpaque) - the only thing blocking IsDefinitelyOpaque and LockTextures in this unit, and it needs four coordinated files
NEW: progress-prime1-cmodeldata-l9 | match | MetroidPrime/CModelData | IRenderer::DrawModelDisintegrate is declared with a const CModel& but retail's DisintegrateDraw passes a 16-byte {CModel*,CSkinnedModel*,void*,void*} context; correcting the declaration would unblock DisintegrateDraw, RenderNoise and RenderSolid
```

## Not filed, and why

* `Render`'s 98.43% is a wall measured this run, so it is a `WALL:` line and not a `NEW:` item.
* `GetIsLoop`'s blocker is the shared `CAnimData.hpp` bitfield and costs 5 functions elsewhere -
  that is a lesson, not a lane-sized item on this unit.
* `__ct__FRC8CAnimRes` still needs four new symbols
  (`GetFactory__24CCharacterFactoryBuilderFRC8CAnimRes`, `GetCharInfo__17CCharacterFactoryCFi`,
  `__dt__9CAnimDataFv`, `SetModelScale__9CAnimDataFRC9CVector3f`); run 2's measurement stands and
  I did not re-measure it, so I am not filing a duplicate `NEW:` for it.
