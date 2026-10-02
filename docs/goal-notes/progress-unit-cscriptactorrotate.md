# progress-unit-cscriptactorrotate

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptActorRotate` (DOL unit, stays
`NonMatching`). **`4/16 -> 10/16` functions matched**, `11731 -> 11737` matched functions overall.
`./tools/goal_check.sh build/goal/item.json` -> `PASS`.

## Files touched

- `src/MetroidPrime/ScriptObjects/CScriptActorRotate.cpp`
- `include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp` (one inline setter, 3 lines)
- `include/rstl/pair.hpp` (one `construct_impl` overload + forward declaration, 13 lines)

No `asm`, no judge-owned path, no `configure.py`/`config/` change, unit not promoted.

## Per function (before -> after)

All before-values re-measured on this clean tree with `./tools/fast_try.sh
MetroidPrime/ScriptObjects/CScriptActorRotate` before any edit; after-values from the same command.

| function | before | after | what changed |
| --- | --- | --- | --- |
| `SetCurrentTime(float)` | 25.0% | **100%** | body is `CMath::Clamp(0.f, time, mDuration)` |
| `CheckEnd(CStateManager&)` | 97.0% | **100%** | `if (mCurrentTime >= mDuration) { ... }` instead of early `return` on `<` |
| `Think(float, CStateManager&)` | 83.7% | **100%** | `TCastToPtr<CScriptActorRotate>(mgr.ObjectById(mTargetId))`, no `TypesMatch` |
| `vector<pair<TUniqueId,CTransform4f>>::clear()` | 0.0% | **100%** | `is_trivially_destructible` specialization |
| `vector<pair<TUniqueId,CTransform4f>>::~vector()` | 42.2% | **100%** | same specialization |
| `reserve(int)` | 50.4% | 87.0% | same + `construct_impl` assignment form |
| `AcceptScriptMsg` | 54.8% | **100%** | fall-through `kSM_Activate`->`kSM_XALD`, `TCastToPtr`+`GetObjectById`, `kSM_Start`/`kSM_Stop` cases ordered **before** `kSM_Deactivate`, `SetRotateController` loop on deactivate |
| `UpdateActors(bool, CStateManager&)` | 63.7% | 83.7% | indexed loop, `act->GetUniqueId()`, `GetTransform().GetRotation()`, `SetRotateController` on each connected platform |
| `UpdateTargetRotation` | 45.6% | 45.6% (not kept) | tried `ClampRadians`+`FromRadians` spellings; best was 30 differing instrs, see below |
| ctor | 90.7% | 90.7% | not improved, see below |
| `UpdateActorRotations` | 1.9% | 1.9% | not attempted (1640 B, still a stub) |

## The four spellings that reached 100%, and why

Measured with `tools/try_batch.py` (instruction-level diff vs the retail object), then kept.

- **`SetCurrentTime`**: `mCurrentTime = CMath::Clamp(0.f, time, mDuration);` -> `*** MATCH ***`.
  Retail 0x8010A1CC is `fcmpo f0,f1 / ble / b / fcmpo f2,f1 / bge / fmr / fmr / stfs`, i.e. one
  clamped select. `CMath::Clamp` is `min > val ? min : max < val ? max : val` and it is the only
  spelling tried that emits the `bge`/`ble` pair plus both `fmr`s; the hand-written if/else chain
  and the explicit ternaries all scored 4-8 differing instrs.
- **`CheckEnd`**: the whole body under `if (mCurrentTime >= mDuration) { ... }`. Retail 0x8010AFC8
  uses `fcmpo cr0,f1,f0 / cror eq,gt,eq / bne`; only `>=` produces that `cror`+`bne` pair.
  `if (mCurrentTime < mDuration) return;` gives `blt` (1 diff); `!(a > b)` and `!(a < b)` give 2.
- **`Think`**: `TCastToPtr<CScriptActorRotate>(mgr.ObjectById(mTargetId))`. Retail calls
  `ObjectById` then the out-of-line `TCastToPtr<18CScriptActorRotate>__FP7CEntity`
  (0x80099EB0). The old `GetObjectByIdFromListAll` + virtual `TypesMatch` inlined a 5-instruction
  vtable dispatch that retail does not have. `GetObjectByIdFromListAll` scores the same as
  `ObjectById` here (objdiff ignores the `bl` target), so `ObjectById` was kept as retail's.
- **`AcceptScriptMsg`**: **`kSM_Start`/`kSM_Stop` must be spelled before `kSM_Deactivate`** -
  with the source order reversed, 30 instrs differ; in this order, `*** MATCH ***`. Retail's
  binary-search tree on the message puts `kSM_Start`(0x53545254)/`kSM_Stop`(0x53544F50) at
  0x8010A3A8/0x8010A3B0, immediately before the deactivate block at 0x8010A3B8. The cast test
  must be written **null-first with a `break`** (`if (cast == nullptr) { UpdateActors(...); break; }
  StartRotation(); mCurrentTime = 0.f;`), which is the only spelling that emits retail's
  `cmplwi r3,0 / bne`; the `!= nullptr` if/else and the ternary both cost 30 instrs.

## Shared-header findings (these are general, not item-specific)

- **`rstl::pair<TUniqueId, CTransform4f>` is trivially destructible in retail.** Retail's
  `clear()` (0x8010A630) is 3 instructions - `li r0,0 / stw r0,4(r3) / blr` - with no
  52-byte-stride destroy loop, and the vector destructor (0x80109F34) frees without one. The
  generic `destroy(begin, end())` in `include/rstl/vector.hpp` emits the loop unless
  `is_trivially_destructible<pair<TUniqueId, CTransform4f>>` is specialized. Adding it (in the
  unit's own .cpp, so nothing else moves) took `clear` 0->100%, the destructor 42->100% and
  `reserve` 50->87%.
- **That pair copies as one block, not member-wise.** Retail's `UpdateActors` builds the pair in
  a stack temporary and then does `bl fn_800E88FC` (the out-of-line `CTransform4f` copy, 48
  bytes) followed by a single trailing `stw` (0x8010A568/0x8010A570). Placement-new of the
  pair's copy constructor emits the 48 bytes member-wise, splitting the 2-byte `TUniqueId` store
  out. A `construct_impl` overload for `pair<T, CTransform4f>` that assigns instead of placement-
  news reproduces it. This is the same mechanism already documented for `pair<uint,uint>` and
  `pair<int,float>` in `include/rstl/pair.hpp`, so the change is in keeping with that file.

## What is still open, with the evidence

- **`UpdateTargetRotation` (500 B, 45.6%) - a wall on the frame, not the logic.** The logic is
  settled: `CheckEnd`, then `ObjectById` + `TCastToPtr<CScriptActorRotate>`, then three
  `CRelAngle::FromRadians(CMath::ClampRadians(spline.EvaluateAt(mCurrentTime) * (M_PIF / 180.f)))`
  and `RotateZ(z) * RotateY(y) * RotateX(x)`, then `target->SetActorTransforms(rotation)`. The
  `ClampRadians` spelling is confirmed by retail's per-angle integer-truncation block
  (0x8010A758-0x8010A7A8: `fmuls / fctiwz / stfd / xoris 32768 / lfd / fsubs / fnmsubs /
  fcmpo / bge / fadds`), which is `CMath::ClampRadians`'s `FastFmod` shape, not the plain
  `deg * (M_PIF / 180.f)` that `CRelAngle::FromDegrees` spells. Best spelling scored **30
  differing instrs out of ~125**; the rest is a frame difference - retail allocates 400 bytes
  and spills `f31`/`f30` (it keeps two of the three angles in callee-saved registers across the
  `EvaluateAt` calls) while every spelling tried allocates 320-368 and keeps one value in `f2`.
  Tried and their scores: `xyz-rev` (three named `CRelAngle` locals, product in one expression)
  31; `clamp-cast` (everything inline) 30; `xf-locals-named` (three named `CTransform4f`) 79;
  `xyz-locals` 53; `plain` (`FromDegrees`, the old code) 81. Not a spelling search that is
  obviously close to done.
- **Constructor (460 B, 90.7%).** Only the max-of-splines block differs. Retail 0x8010B16C is
  `lfs f0,36(r31) / fcmpo cr0,f1,f0 / bge +8 / b +8 / fmr f0,f1 / stfs f0,36(r31)`, i.e. an
  unconditional store of a select, **not** a conditional store. 20 differing instrs = 5 blocks x
  4. Tried, all 20: `mDuration = maxTime >= mDuration ? maxTime : mDuration`, `>` variant,
  `mDuration <= maxTime` variant, `CMath::Max` both argument orders, `if (>=)`/`if (>)` with an
  explicit empty `else` storing `mDuration`, a `float m` local assigned unconditionally, and
  `!=(mDuration < maxTime)`. Every one MWCC accepts folds the store to the taken arm only
  (`ble` + `stfs f1`, 6 instrs). The retail form wants a *phi* the front end will not build.
  Worth another run if someone wants it: the shapes not yet tried are a `volatile`-free
  by-reference helper, or spelling the whole thing as a `CMath::Max` on a `float&`.
- **`UpdateActorRotations` (1640 B, 1.9%) is still a stub** calling only `CheckEnd`. Retail
  0x8010A960 is 1232 bytes of stack frame and reuses the same `ClampRadians` shape for six
  splines, then walks `mActors` composing `GetRotation()` with `CTransform4f::operator*` and
  branches on `mFlags` bits 25 and 29 (`0x8010ABB4/0x8010ABBC`). One item's worth of work on its
  own; nothing here is a prerequisite for it.
- **`LoadActorRotate` (540 B, 0.0%) is not in our source at all** - it is the free
  `SLdrEditorProperties` loader, and the report lists it with no `fuzzy_match_percent` because our
  object does not define it.

## Note for the reviewer

The `CScriptPlatform.hpp` addition is a single inline setter,
`void SetRotateController(TUniqueId controller) { x450_ = controller; }`, added because both
`UpdateActors` (0x8010A5A4: `sth r0,1104(r3)`) and `AcceptScriptMsg`'s deactivate path
(0x8010A424: `sth r0,1104(r3)`) write this->`m_uid` into offset 0x450 of every connected
`CScriptPlatform`, and offset 0x450 is that class's existing `x450_` member. The old code had a
`// TODO: record this controller on any connected script platform.` where the store belongs, so
this fills in the TODO with retail's behaviour rather than adding new behaviour. No class layout
changed (`CHECK_SIZEOF(CScriptPlatform, 0x490)` still holds and the DOL sha1 gate passes).

---

# Run 2 (lane 4, 2026-10-02) - `10/16 -> 13/16`

The previous run's work is in HEAD (`1379b9b4`); this run re-measured the clean tree first and got
the same numbers the notes recorded, then took the three functions closest to done.

**`10/16 -> 13/16`**, `12265 -> 12268` matched functions overall, linked unchanged at 5863.
`./tools/goal_check.sh build/goal/item.json` -> **PASS**, with no `WORSE` line in
`build/gate-diff.log` (checked by grep).

## Files touched

- `src/MetroidPrime/ScriptObjects/CScriptActorRotate.cpp` (the four hunks below)
- `include/rstl/pair.hpp` (one `construct_impl` body + comment)

No `asm`, no judge-owned path, no `configure.py`/`config/` change, unit not promoted.

## Per function (before -> after), all re-measured on this tree

| function | before | after | what changed |
| --- | --- | --- | --- |
| `reserve(int)` | 87.0% | **100%** | the pair's `construct_impl` now *is* the 52-byte block copy (see below) |
| ctor (460 B) | 90.7% | **100%** | `MaxDuration` helper: `t < duration ? duration : t`, by value |
| `UpdateActors(bool, CStateManager&)` | 83.7% | **100%** | `push_back_unsafe`, `ids.size() > 0` guard, two-armed `if` for `mCurrentTime` |
| `UpdateTargetRotation` | 45.6% | **87.1%** | `ObjectById` + `TCastToPtr`, and `ClampRadians` + `FromRadians` instead of `FromDegrees` |
| `UpdateActorRotations` | 1.9% | 1.9% | not attempted (still a stub) |
| `LoadActorRotate` | absent | absent | not attempted (still not in our source) |

## The three that reached 100%, and the mechanism

The common cause of two of the three is a codegen fact worth having in general:

**MWCC folds `x = cond ? t : x` into a conditional store; retail's MWCC did not.** Retail wants
`cmp / b<cc> / b / fmr / stfs` - a phi and one unconditional store. Every spelling in the previous
run's list folded to `ble + stfs f1` (4 instructions) instead. Two spellings stop the fold, and both
were needed here:

- **Passing the running value through a by-value helper** (`static inline float MaxDuration(float
  duration, float t) { return t < duration ? duration : t; }`, then `mDuration =
  MaxDuration(mDuration, spline.GetMaxTime());`). By *reference* the fold still happens; by value
  with both arms spelled out it does not. `t < duration`, not `t >= duration`: retail's `bge` is
  `fcmpo cr0,f1,f0` read as `maxTime >= duration`, so the source test has to be written as the
  mirror image to get the same branch.
- **Writing the two arms as two statements** (`if (next) { mCurrentTime = mDuration; } else
  { mCurrentTime = 0.f; }`). The ternary sinks the second store into the merge block; retail stores
  in both arms (0x8010A5FC).

Measured, ctor (460 B, before 90.7%): `t >= d ? t : d` **86.1**; `t > d ? t : d` **90.4**;
`@T < mDuration ? mDuration : @T` (unhoisted) **91.3** but calls `GetMaxTime` twice; hoisted
`{ const float t = @T; mDuration = t >= mDuration ? t : mDuration; }` **94.8**; hoisted
`{ const float t = @T; mDuration = t < mDuration ? mDuration : t; }` **100**; `<=` variant **95.4**;
hoisted `if (t >= mDuration)` **86.3**; `CMath::Max` either order **80.8**/**81.0**; `FastMax`
**76.7**; `if/else` with a self-assigning else **77.6**. So the previous run's list was right that
everything folds and wrong that nothing can: the missing ingredient is the by-value call.

Measured, `UpdateActors` (436 B, before 83.7%; the new `construct_impl` alone moved it to 83.1):
`push_back_unsafe` only **97.2**; `push_back_unsafe` + `!ids.empty()` **99.0**; + `ids.size() > 0` +
ternary tail **99.0**; + `ids.size() > 0` + two-armed `if` tail **100**; `push_back` + guard +
two-armed `if` **84.9**. Two details that are retail's and not arbitrary: the guard is `size() > 0`
(`cmpwi r4,0 / ble`, 0x8010A4E4 - `!empty()` gives `beq`), and there is no capacity test at the push
at all, because the reserve covers every id.

**The pair's copy is a 52-byte block copy split at 48, and neither `operator=` nor a placement
`new` emits it.** The previous run's `construct_impl` assigned the pair, which gave
`lhz / addi r3,r30,4 / addi r4,r29,4 / sth / bl __as__12CTransform4fFRC12CTransform4f`; retail
(0x8010B278) is `mr r3,dst / mr r4,src / bl fn_800E88FC` - the *copy constructor*, called on the
pair's own base - then `lwz r0,48(src) / stw r0,48(dst)`. The two halves together are the whole
52-byte object representation, so the body now spells them out. Also measured: the generic
placement-`new` path (specialization deleted) drops `reserve` to 51.4% and `UpdateActors` to 81.9%,
because it outlines the whole copy loop.

## `UpdateTargetRotation`: 45.6% -> 87.1%, and where it stops

Two of the three things the previous run left as "settled logic" were not in the source at all, and
putting them in took the function from 45.6% to 87.1%:

- the cast is `TCastToPtr<CScriptActorRotate>(mgr.ObjectById(mTargetId))`, not
  `GetObjectByIdFromListAll` + `TypesMatch` (retail calls the out-of-line
  `TCastToPtr<18CScriptActorRotate>__FP7CEntity` and has no vtable dispatch). This is the same
  spelling `Think` already used, and the previous run's own reviewer flagged the mismatch.
- the angles are `CRelAngle::FromRadians(CMath::ClampRadians(deg * (M_PIF / 180.f)))`. The evidence
  is retail's per-angle block at 0x8010A75C-0x8010A7A4: `fmuls f4,f2,f1 / stw r0,320(r1) / lfd f3 /
  fmuls f2,f4,f0 / fctiwz / stfd / lwz / xoris 32768 / stw / lfd / fsubs / fnmsubs / fcmpo / bge /
  fadds`, and the two SDA21 constants it loads are `lbl_8041BB7C` = 0x3e22f983 = 1/(2pi) and
  `lbl_8041BB74` = 0.0f (read out of the DOL with `tools/dol_read.py 0x8041BB74 0x1c`). That is
  `FastFmod`'s shape; `FromDegrees` emits one `fmuls` and cannot produce it. The previous run had
  identified this shape but did not land the spelling.

What is left is only the order MWCC evaluates the three `EvaluateAt` calls in, and where it puts the
two live angles. Retail calls `mXRotation` first, then `mYRotation`, then `mZRotation`; ours calls
Z, Y, X, because a left-associated `RotateZ * RotateY * RotateX` evaluates its leaves in source
order. And retail keeps the first two angles in `f31`/`f30` across the calls
(`stfd f31,384(r1) / xsmsubadp / stfd f30,368(r1) / xxsel`), giving a 400-byte frame; ours keeps
`f2` and allocates 368. Seven spellings measured **in this run**, all worse than the one kept:
three named `CTransform4f` multiplied `z * y * x` **34.96**; three named `CRelAngle` then the
expression **39.52**; one expression `RotateX * RotateY * RotateZ` **36.53**; `RotateZ * (RotateY *
RotateX)` **34.81**; three named degree floats then the expression **78.12**. (The previous run's
five were measured against the old body, so they are not comparable to these and are not repeated.)

WALL: CScriptActorRotate::UpdateTargetRotation 87.1% - every call and the whole arithmetic now match
retail, and what is left is MWCC's evaluation order for the three EvaluateAt calls plus its choice of
f31/f30 over f2 (a 400- vs 368-byte frame); seven spellings measured this run, none closer.

## Still open, with the evidence

- **`UpdateActorRotations` (1640 B, 1.9%) is still a stub** calling only `CheckEnd`. Retail
  0x8010A960 is 1640 bytes. Not attempted this run: it is a from-scratch function and nothing
  measured here is a prerequisite for it. Prime 1's donor has the shape at
  `prime-ref/src/MetroidPrime/ScriptObjects/CScriptActorRotate.cpp:89-103` (`timeOffset *
  mRotation.GetX/Y/Z` into `RotateX * RotateY * RotateZ`, then `it->second * xf`, then the
  translation add, then `UpdatePlatformRiders`), and Echoes' version additionally reuses
  `ClampRadians` for six splines and branches on `mFlags` bits 25 and 29 - the same six-spline
  max-of-`GetMaxTime` block the constructor now matches, which is worth reusing.
- **`LoadActorRotate` (540 B) is not in our source.** Retail 0x80109F88: a 512-byte frame, three
  `CMayaSpline` constructions, an `SLdrSpline`-shaped property block, and a binary search over
  property tags (`lwz r3,8(r25) / lhz r30,0(r3)` then `cmpw r4,r31 / beq / bge` against four
  `lis`/`addi` tag constants) inside a loop. The report lists it with no `fuzzy_match_percent`
  because our object does not define it. Writing it is the same kind of work as
  `UpdateActorRotations`.
- **This unit cannot be flipped as it stands, and that is pre-existing, not from this run.** On a
  clean HEAD (measured by checking the two files out and re-running both) `check_decl_order.py`
  already says "would break on a flip" and `unit_fit.sh` already reports "6 function(s) present in
  ours but not in the retail unit object, 1148 bytes total". Both are unchanged by this run: no
  function was added or moved, and the 6 extras are the same 6. Fixing either is its own item.

## Note for the reviewer

Three hunks in the `.cpp` look like deliberate contortions, so the evidence is in the comment at
each one: `MaxDuration` reproduces retail's unconditional store of a select in the ctor;
`push_back_unsafe` and `ids.size() > 0` reproduce retail's absent capacity test and its `ble` guard;
the two-armed `if` reproduces retail's duplicated store. None of them drops work - the pair's
`construct_impl` copies all 52 bytes in retail's two pieces, and no initialisation was removed to
buy a percentage. The `pair.hpp` addition is a redeclaration of `fn_800E88FC` (already declared in
`Kyoto/Math/CTransform4f.hpp`) plus a two-line body for one pair type that only this unit
instantiates; no class layout changed, and the DOL sha1 and all 86 REL hashes still hold
(`gate.sh` inside `goal_check.sh`).
