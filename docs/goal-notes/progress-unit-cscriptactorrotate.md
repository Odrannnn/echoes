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