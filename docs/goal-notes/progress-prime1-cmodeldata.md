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
