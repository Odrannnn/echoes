# progress-unit-cplayerorbit

`MetroidPrime/Player/CPlayerOrbit`, 11 functions taken to an exact match. The unit went
**10/60 -> 22/60 matched functions**, fuzzy **2.76% -> 6.36%**. Judge: `goal_check: PASS`
(gate clean, matched 11542 -> 11554, linked unchanged at 5625, no asm, decl order ok).

Only `src/MetroidPrime/Player/CPlayerOrbit.cpp` changed. Three includes were added, each
required by one of the bodies below: `CPlayerCameraBob.hpp` (the type behind `mCameraBob` must be
complete to call through it), `TCastTo.hpp`, `Tweaks/CTweakPlayer.hpp`.

Per function, retail address, before -> after, and what the spelling had to be:

| function | addr | before | after | note |
| --- | --- | --- | --- | --- |
| `PreventFallingCameraPitch` | 0x8011eb90 | 14.29% | 100% | three stores: `mJumpCameraTimer = 0.f`, `mFallCameraTimer = .01f`, `mCancelCameraPitch = true` |
| `InGrappleJumpCooldown` | 0x8011eb4c | 83.53% | 100% | see "the `if/else` that matters" below |
| `fn_8011ca08` | 0x8011ca08 | 16.52% | 100% | eye + `fn_80019360().GetForward() * distance`; `distance` is `(mOrbitPoint - eye).Magnitude()` when `mOrbitState == kOS_OrbitObject`, else `0.5f` |
| `GetHUDOrbitTargetPosition` | 0x8011ecb8 | 14.14% | 100% | `mOrbitPoint + mCameraBob->GetCameraBobTransformation().GetTranslation()` |
| `GetOrbitMaxTargetDistance` | 0x80122a40 | 7.37% | 100% | tweak distance, overridden by `GetScanMaxTargetDistance()` under the scan visor |
| `GetOrbitMaxLockDistance` | 0x801229f4 | 7.37% | 100% | same shape, `GetOrbitMaxLockDistance` / `GetScanMaxLockDistance` |
| `CalculateOrbitMinDistance` | 0x8011ec34 | 5.91% | 100% | tweak `GetOrbitMinDistance(type)` scaled by `CMath::Clamp(1.f, AbsF(mOrbitPoint.GetZ() - GetTranslation().GetZ()) / 20.f, 4.f)` |
| `UpdateOrbitFixedPosition` | 0x8011f098 | 43.02% | 100% | eye must be read into a local *first* |
| `UpdateOrbitZPosition` | 0x8011f018 | 3.12% | 100% | `switch` on `mOrbitState`, `case kOS_OrbitPoint` only |
| `UpdateOrbitZone` | 0x80121b0c | 5.88% | 100% | the `!= kPV_Scan` test must be the *outer* one |
| `OrbitPoint` | 0x8011ebe8 | 5.26% | 100% | p1 shape, but `SetOrbitPosition` takes only the distance here |
| `ValidateOrbitTargetIdAndPointer` | 0x801232e0 | 6.67% | 100% | `const TUniqueId` **by value** + `TCastToConstPtr<CActor>` |

## Things that cost time, for the next run

**The `if/else` that matters (`InGrappleJumpCooldown`).** The obvious one-expression spelling is
the same logic and scores **83.53%**, not 100%: retail materialises the result through two
separate returns, so the compiler must keep `r3` live across the whole body. Written as

```cpp
if (cond) { return true; } else { return false; }
```

it is 100%. Every "cleaner" spelling I tried scored lower: early returns with a separate
`return false` tail 75.29%, a single tail `return mJumpCameraTimer == 0.f && ...` 59.12%,
nested `if`/`else` assigning a local 59.12%, `nested-true-early` (two `return true`s, one
`return false`) 87.65%. **The `else` is load-bearing; do not tidy it away.**

**`ValidateOrbitTargetIdAndPointer` needs the parameter spelled `const TUniqueId`, by value.**
Dropping the top-level `const` costs 24 points (95.24% -> 71.19%): retail reloads the id from
`0(r4)` for the second use, and MWCC only keeps it in a register when the parameter is const.
`TCastToConstPtr<CActor>(...)` is what reproduces the `bl TCastToPtr<6CActor>__FP7CEntity` at
0x800975e4; `static_cast<const CActor*>` gives a static cast (6.67% of that part), and plain
`TCastToPtr` does not compile against the const `GetObjectById` result.

**The p1 donor is a donor, not an answer.** Three of these needed a change from
`prime-ref/src/MetroidPrime/Player/CPlayerOrbit.cpp` beyond names:

- `OrbitPoint`: p1 calls `SetOrbitPosition(distance, mgr)`; this repo's overload is
  `SetOrbitPosition(float)` (CPlayer.hpp:437), and passing `mgr` does not compile.
- `CalculateOrbitMinDistance` is p1's `CalculateOrbitZBasedDistance`, and the `static const float
  maxScale = 4.f` is optional - both spellings score 100%, so I left the literal inline.
- `UpdateOrbitZPosition` and `UpdateOrbitZone`: p1 takes a `CStateManager&` and reads the visor
  through `mgr.GetPlayerState()`; this repo's `UpdateOrbitZone()` takes nothing, so the visor read
  is `mPlayerState->GetCurrentVisor()` directly (the member is `0x1314`, matching retail's
  `lwz r3,4884(r31)`).

**Reading the retail constant pool.** The `lfs f0,-26092(r2)`-style operands resolve through the
SDA2 base, not SDA: `python3 tools/sda.py s2:-26092`. `dol_read.py` on the resolved address then
gives the value (`20.f`, `1.f`, `4.f` for `CalculateOrbitMinDistance`; `0.5f` for `fn_8011ca08`).
`tools/dol_read.py` refuses the *unresolved* displacement, which is the easy mistake here.

**Order of evaluation is a real difference, not noise.** `UpdateOrbitFixedPosition` as one
expression (`GetEyePosition() + GetTransform().Rotate(mOrbitVector)`) is 43.02%; splitting into
`const CVector3f eye = ...; const CVector3f rot = ...;` is 100%, because retail calls
`GetEyePosition` first and keeps the result in `f31/f30/f29`. Rot-first is 91.20%.

**Which member `fn_80019360` really is.** The three float loads are `4(r3)`, `20(r3)`, `36(r3)` -
`m01/m11/m21`, so it is `GetForward()`, not `GetTranslation()` (`GetRight()` and `GetUp()` both
score 99.95% and differ in one instruction).

## Measured but not matched (for the next attempt)

`fn_80123334` (104 B, 0.00%), `fn_80121908` (120 B, 0.00%), `fn_8012339C` (372 B, 0.00%) and
`fn_8011c3c0` (1608 B, 0.25%) are still pure stubs. Everything else in the unit is below 7%, and
the next-smallest unstarted bodies are `ActivateOrbitSource` (96 B, 4.17%),
`AddOrbitDisableSource` (208 B, 1.92%), `fn_801219ec`'s neighbour `CheckOrbitDisableSourceList`
(164 B, 3.41%), `UpdateOrbitPosition` (200 B, 2.00%) and `SetOrbitTargetId` (220 B, 1.82%) - all
small enough that the p1 donor is likely to apply as-is, which is how every function above landed.

`OrbitCarcass`, `RemoveOrbitDisableSource`, `CheckOrbitDisableSourceList() const`,
`UpdateOrbitModeTimer`, `UpdateOrbitPreventionTimer`, `UpdateAimTargetTimer`,
`SetOrbitRequestForTarget` and the three texture-data accessors were already matched on the branch
head and are untouched.