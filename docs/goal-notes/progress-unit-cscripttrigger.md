# progress-unit-cscripttrigger

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptTrigger`. The unit stays `NonMatching`;
`flip_test.sh` was not run and is not the acceptance test here. The judge is
`build/report.json`'s per-unit `matched_functions`.

## Result

**14 -> 16 of 44 matched functions.** `tools/goal_check.sh build/goal/item.json` prints
`PASS progress-unit-cscripttrigger`; the whole-project matched count went 11659 -> 11661 with
`linked` unchanged at 5625 and the `All:` line unmoved.

Two functions reached 100%, which is what raises the count:

| function | before | after |
| --- | --- | --- |
| `IsAI__14CScriptTriggerCFR13CStateManagerR6CActor` | 4.0% | **100%** |
| `ShouldSendScriptMsgs__14CScriptTriggerCFR6CActorR13CStateManager` | 4.8% | **100%** |

Four more rose substantially but did not reach 100%, so they do not count yet:

| function | before | after |
| --- | --- | --- |
| `RemoveInhabitantIfOutside__...F9TUniqueIdR13CStateManager` | 85.3% | 99.38% |
| `ReplaceInhabitant__...F9TUniqueId9TUniqueIdR13CStateManager` | 73.5% | 97.52% |
| `RemoveInhabitant__...F9TUniqueIdR13CStateManager` | 90.3% | 96.67% |
| unit fuzzy | 28.50% | 32.55% |

## Files

- `src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp` - five bodies rewritten, three includes
  added (`CCameraManager.hpp`, `CCollisionActor.hpp`, `Cameras/CGameCamera.hpp`,
  `Enemies/CAi.hpp`).
- `include/MetroidPrime/CCollisionActor.hpp` - one inline getter,
  `TUniqueId GetOwner() const { return mOwner; }`, needed by `IsAI`. No layout change; retail
  reads the same field at `+724`, confirmed from `CCollisionActor.o`'s constructor stores.

No `configure.py`, `splits.txt` or `files.cmake` change: nothing was carved and no new symbol was
claimed, so the four-file carve rule does not apply. `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptObjects/CScriptTrigger` says the functions are still emitted in retail order.

## What each body is, and how it was read out of retail

The unit's own retail object was disassembled with `build/binutils/powerpc-eabi-objdump -dr` and
the call targets read off the relocations; `build/binutils/powerpc-eabi-objdump -r` listed them.
`IsAI` and `ShouldSendScriptMsgs` are now byte-identical to retail including which of the four
relocations they use, so they are translations, not approximations.

`IsAI` (140 B, 4 call sites): `TCastToPtr<CAi>(CEntity&)` on the actor, then
`TCastToPtr<CCollisionActor>(CEntity&)`, then `CStateManager::GetObjectById` **const** (not
`ObjectById`) on `mOwner` at offset 0x2d4, then `TCastToPtr<CAi>(CEntity*)`. Retail returns `true`
as `li r3,1` after each hit and `false` at the end - three exits, which is what forced the early
returns rather than a single flag.

`ShouldSendScriptMsgs` (116 B, 3 call sites): `TCastToPtr<CGameCamera>(CEntity&)`, then
`CGameCamera::CameraManager(mgr)` - a **non-static member** taking `CStateManager&`, which is why
`Cameras/CGameCamera.hpp` is included - then `CCameraManager::GetCurrentCameraId(true)`, compared
against `cam->GetUniqueId()`. The `true` selector is retail's (`li r5,1`). Getting the comparison
operand order the other way round (`GetCurrentCameraId(...) != cam->GetUniqueId()`) is what took
it from 90.3% to 100%; it is the only difference between the two spellings, and the compiler
loads the camera's id into `r31` before the call in retail.

`RemoveInhabitantIfOutside`: retail loads `GetObjectById`/`TCastToPtr`, saves the actor in a
register with `mr.` (so the null test is the *taken* branch shape) and keeps a flag in `r30`
returned with `clrlwi r3,r30,24`. Writing the null test as an early `return false` with the actor
in a local - not `if (actor) { ... }` - is what produced that shape; the nested form measured
95.10% against 99.38% for the guard form. One instruction still differs at the end (retail
`clrlwi`, ours `mr r3,r30`), i.e. the flag's width.

`RemoveInhabitant`: same guard shape, one instruction shorter than retail's tail.

`ReplaceInhabitant`: retail runs `HasInhabitant(newId)` and *branches on it being false* to take
the `SetObjectId` path, with the erase path as the `else`. That is the opposite of the
`alreadyInside`-first shape, and it is worth 97.52% against 89.04%.

## Things measured, so nobody repeats them

Every number below is `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptTrigger` after a
rebuild of just that object.

- Flag type in `RemoveInhabitant`, guard form: `bool removed = false; ... = true` -> **96.67%**
  (best). `uint`/`int`/`uint8` flag returned as-is -> 94.49%. `uint`/`int` with `removed = 1` ->
  83.97%. Flag declared outside the `if` -> 77.49%. `while` loop instead of `for` -> 96.67% (same
  bytes). Flag set *before* the erase -> 86.69%. `!removed` on return -> 92.31%. `return removed
  != 0` -> 92.31%. Caching `mInhabitants.end()` in a local -> 96.49%. A `const TUniqueId uid =
  id` copy -> 93.44%. So the flag must be `bool`, set after the erase, and returned bare.
- Flag type in `RemoveInhabitantIfOutside`: `bool` -> 99.38% (best); `uint`/`int` -> 97.76%;
  assigning `removed = !BoundsOverlap(...)` before the erase -> 97.29%; `static_cast<bool>` on
  return -> 96.88%; combining the null test into one `||` -> 75.20%.
- Null-test shape in `ReplaceInhabitant`: `if (!HasInhabitant(newId)) { set } else { erase }` ->
  97.52%. Reversed -> 89.04%. `const bool alreadyInside = HasInhabitant(newId);` hoisted ->
  86.14%. A single combined `||` null test -> 75.20%. Separate early returns -> 74.68%.
  `uint` flag -> 70.94%.
- `ShouldSendScriptMsgs`: `TCastToPtr` + `GetCurrentCameraId(true) != cam->GetUniqueId()` ->
  100%. `cam->GetUniqueId() != cam->CameraManager(mgr).GetCurrentCameraId(true)` -> 90.34%. Binding
  the camera-id to a local first -> also below 100%. The cast must be to `CGameCamera*` from
  `CActor&`, not `TCastToConstPtr`.
- `RemoveInhabitant` and `RemoveInhabitantIfOutside` need the **const** `mgr.GetObjectById`, not
  `mgr.ObjectById`; retail's relocations say `GetObjectById__13CStateManagerCF9TUniqueId`. That
  forces the actor locals to be `const CActor*`, which is why `TCastToConstPtr` is used. Using
  `ObjectById` and a non-const `CActor*` is what the pre-existing source had, and it was part of
  the gap.

WALL: RemoveInhabitant__14CScriptTriggerF9TUniqueIdR13CStateManager 96.67% - flag type and shape
are pinned by the 12 spellings above; the last difference is one instruction of register
allocation (`li r6,0` in r6 vs ours in r3), not logic.

WALL: RemoveInhabitantIfOutside__14CScriptTriggerF9TUniqueIdR13CStateManager 99.38% - one
instruction (`clrlwi r3,r30,24` vs `mr r3,r30`) is the flag's width; `bool`, `uint`, `int`,
`uint8` and `static_cast<bool>` all measured, none reaches 100%.

WALL: ReplaceInhabitant__14CScriptTriggerF9TUniqueId9TUniqueIdR13CStateManager 97.52% - the
remaining difference is the stack slot the two `TUniqueId` temporaries get (retail `r1+16`/`r1+12`,
ours `r1+20`/`r1+16`); every null-test and flag spelling measured above shifts it without closing
it.

## Codegen rules learned (for the next lane, not for the queue)

- Retail's `GetObjectById` on a `const CStateManager&` is the **const** overload; a
  `TCastToPtr<T>` off its result therefore needs a `const_cast`, which is `TCastToConstPtr<T>`.
  The reverse (a non-const `mgr`) uses `ObjectById` and a plain `TCastToPtr`.
- A `bool` return from a function whose tail is a flag read out of a register compiles to
  `clrlwi rD,rS,24` when the source was an integer-typed flag, and to a bare `mr rD,rS` when the
  flag was `bool`. Both appear in retail in this same unit, so the two spellings are not
  interchangeable; the flag's declared type decides, and it is measurable.
- `GetCurrentCameraId(bool)` takes the selector in `r5`; `li r5,1` means the *player's* current
  camera, `li r5,0` the other one. `CCameraManager.cpp` already uses `false` for the shadow path.

## Still open in this unit (28 functions unmatched, measured)

`UpdateInhabitants` 1520 B at 0.26%, `Touch` 1044 B at 0.38%, `AddInhabitant` 944 B at 0.42%,
`UpdateCameraInhabitant` 664 B at 0.60%, `ClearInhabitants` 224 B at 1.79%, `SetPlayerInside`
240 B at 1.67%, `HasInhabitant` 148 B at 39.4%, `AcceptScriptMsg` 228 B at 73.3%, the
constructor at 93.8%, and 11 unnamed `fn_*` in the 0x20-0x78 range that are vtable slots and list
helpers (`fn_8007298C` .. `fn_80072BFC`, `fn_80071B2C` .. `fn_80071BE8`, `fn_80072278`,
`fn_800722B8`, `fn_8007334C`, `fn_800733E0`).

Two observations for whoever takes the big ones, both from the retail relocations:

- Prime 1's `prime-ref` donor is **not** usable for the inhabitant bookkeeping. Echoes moved
  `mPlayerInside[4]`/`mPlayerEnvironmentDamage[4]` and per-trigger `CObjectTracker::mTriggers` into
  the class; `Touch`, `UpdateInhabitants`, `AddInhabitant` and `SetPlayerInside` are all built on
  those and have no Prime 1 counterpart. Prime 1's `AcceptScriptMsg` takes
  `(EScriptObjectMessage, TUniqueId, CStateManager&)`; this one takes `(CStateManager&,
  const CScriptMsg&)` and resolves a `FindConnectedObject` target that Prime 1 has no code for.
  Only the callee lists below were reusable.
- `ClearInhabitants` (224 B) is the cheapest of the stubs left and its full call list is known
  from the relocations: `ObjectById`, `TCastToPtr<CActor>`, `SetPlayerInside`, then
  `NotifyInhabitantExited` per inhabitant, then `fn_80072278` - and `fn_80072278`/`fn_800722B8` are
  themselves an out-of-line list walk ending in `fn_8007334C` (the `rstl::list` `do_erase`).
  Clearing via `mInhabitants.clear()` would inline that walk instead of calling it, which is
  likely what the current stub gets wrong.
- `IsAI` needed `CCollisionActor::GetOwner()`, added inline. Anything else reading a
  `CCollisionActor` field directly wants the same accessor rather than a `const_cast`.

NEW: progress-unit-cscripttrigger-rest | progress | MetroidPrime/ScriptObjects/CScriptTrigger |
five bodies done (16/44 matched); the remaining 28 are the five big stubs (Touch, UpdateInhabitants,
AddInhabitant, UpdateCameraInhabitant, SetPlayerInside, ClearInhabitants) plus 11 unnamed vtable and
rstl::list helpers whose call lists are now known from the relocations - see the notes above.