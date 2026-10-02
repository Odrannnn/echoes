# progress-unit-cplayerdynamics

`kind: progress`, target `MetroidPrime/Player/CPlayerDynamics` (DOL unit, stays `NonMatching`).

## Result

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`, measured before and after with
`./tools/fast_try.sh MetroidPrime/Player/CPlayerDynamics`:

| | before | after |
|---|---|---|
| `matched_functions` | 11 / 62 | **23 / 62** |
| `fuzzy_match_percent` | 4.47 | 8.72 |
| `matched_code` | 580 / 27020 (2.15%) | 2144 / 27020 (7.93%) |

Whole build: `All: 33.13% fuzzy, 25.98% matched, 12.24% linked (11554 / 28465 functions)`;
`matched 11542 -> 11554`, `linked 5625 -> 5625` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS** (all seven checks, including `gate.sh`).

Twelve functions went from sub-100% to an exact byte match. Every one of them now differs from
retail only in relocated fields (`bl` displacements, `lfs` of a pooled float constant, the
`lis`/`addi` pair for a static), which is what objdiff ignores - i.e. 100% is 100%, not "close".

| function | retail | before | after | what the bytes demanded |
|---|---|---|---|---|
| `GetAverageSpeed` | 0x80189C50, 88 B | 58.55% | 100% | two calls to `TReservedAverage::GetAverage`, not one (below) |
| `GetAcceleration` | 0x80189C1C, 52 B | 95.38% | 100% | `cmpw`+`blt`, signed compare, and an explicit `else` |
| `GetUnbiasedEyeHeight` | 0x801862E8, 80 B | 74.60% | 100% | `CAABox::GetPointD()`, not `GetMaxPoint()` |
| `GetEyeHeight` | 0x80186284, 100 B | 43.80% | 100% | repeats that body instead of calling it |
| `CheckSubmerged` | 0x801864A0, 148 B | 63.19% | 100% | `IsInFluid()` first, both heights computed unconditionally |
| `GetCollisionPrimitive` | 0x80186BD0, 96 B | 53.75% | 100% | a `switch` with a separate `return` per arm |
| `GetBallMaxVelocity` | 0x80186E64, 56 B | 10.00% | 100% | `gpTweakBall->GetBallTranslationMaxSpeed(GetSurfaceRestraint())` |
| `StrafeInput` | 0x801880CC, 148 B | 5.41% | 100% | `GetControlMapper()` member calls, `kC_StrafeRight - kC_StrafeLeft` |
| `CreateTransformFromMovementDirection` | 0x80186B1C, 180 B | 20.38% | 100% | Prime 1's body, `FromColumns(right, dir, Up(), GetTranslation())` |
| `GetActualFirstPersonMaxVelocity` | 0x80186D78, 236 B | 2.37% | 100% | Prime 1's formula, `GetTweakPlayer()` three times |
| `GetActualBallMaxVelocity` | 0x80186C98, 224 B | 2.50% | 100% | same with the four `gpTweakBall` reads |
| `UpdateBombJumpStuff` | 0x80189A60, 156 B | 2.56% | 100% | Prime 1's body, verbatim |

`CPlayer::GetGravity` (0x80189B38, 228 B) and `FinishSidewaysDash` are the two I read retail for
and did not land; both are written up below. Nothing else in the unit got worse - no function
that matched before is unmatched now, and no `.s` was added.

## Files

- `src/MetroidPrime/Player/CPlayerDynamics.cpp` - the twelve bodies above, plus
  `#include "MetroidPrime/Tweaks/CTweakBall.hpp"`.
- `include/Kyoto/Math/CAABox.hpp:86-99` - `GetPointD()` defined inline (see "the `CAABox` corner"
  below). Nothing else in the tree calls it, so no other unit's `.text` moves.
- `src/MetroidPrime/PortCTweakBall.cpp:26-47` - `CTweakBall::GetMaxBallTranslationAcceleration`,
  character for character from `src/MetroidPrime/Tweaks/CTweakBall.cpp:7`. **Required by this
  change**: `GetActualBallMaxVelocity` is the first thing in the tree to *reference* that
  accessor, so `tools/link_gap.py` grew by one
  (`_ZNK10CTweakBall33GetMaxBallTranslationAccelerationEi` not in `port_link_gap_list.md`) and
  `gate.sh` failed on it until the body had a compiled home. After the addition:
  `237 MISSING symbol(s), all accounted for`, 238 -> 237.

## The findings worth keeping

**`CAABox::GetPointD()`, not `GetMaxPoint()` (why the two eye heights copy a vector).** Both
`GetUnbiasedEyeHeight` and `GetEyeHeight` spill a 12-byte `CVector3f` to the stack before taking
`.GetZ()`, and the three loads are `0x36c`, `0x370`, `0x380` - a *gap*, not a contiguous triple.
`GetMaxPoint()` would load `0x378`, `0x37c`, `0x380`. `min` is at `CPlayer+0x36c`, so the fields
are `min.x`, `min.y`, `max.z`: the `GetPointD` corner, `CVector3f(min.GetX(), min.GetY(),
max.GetZ())` by value. Prime 1 has that exact accessor and `CPlayer::GetUnbiasedEyeHeight` writing
`mFpBounds.GetPointD().GetZ()`. If a by-value `CVector3f` copy is what you are staring at, decode
*which three fields* were loaded before you assume which accessor it was.

**Two calls to a `TReservedAverage::GetAverage()`.** `GetAverageSpeed`'s two `bl`s to `fn_80189CA8`
are not redundant: the first result is only tested for its valid flag (out-param slot at `16(r1)`,
flag at `+4`), the second's value is read (slot at `8(r1)`). So the source is Prime 1's
`if (mMoveSpeedAvg.GetAverage()) { return *mMoveSpeedAvg.GetAverage(); } return mMoveSpeed;`, and
hoisting it into one `rstl::optional_object<float> local` - what the scaffold had - cannot match.
Same shape appears in `CVisorFlare.cpp:53`, which hoists it; that one is retail's own choice there.

**`>=` vs `<`, and `uint` vs `int`, are two different instructions.** `GetAcceleration` was at
95.38% with `if (mCurAcceleration >= mAccelerationTable.size())`: `cmplw` (needs SO) instead of
retail's `cmpw`, because `mCurAcceleration` is `uint` and `size()` returns `int`, so the comparison
is unsigned. `static_cast<int>` fixes the flag. Separately, retail branches `blt` *into* the
in-range read and lets `back()` fall through; that only happens with an explicit `else`, not with
an early return. Both changes were needed for 100%.

**`CPlayer::GetCollisionPrimitive` is a `switch`, and its arms are not merged.** Retail emits
`cmpwi 1/beq`, `bge`, `cmpwi 0/bge`, `b` - a comparison tree, not a jump table, and *three*
separate `bl CPhysicsActor::GetCollisionPrimitive()` call sites (one for `kMS_Unmorphed`, one
shared by `kMS_Morphing`/`kMS_Unmorphing`, one for `default`). Giving every arm its own `return`
statement reproduces both; collapsing the arms into one shared `return` gives 64 B against
retail's 96.

**`CheckSubmerged` computes both heights before the morph test.** `2.f * GetBallRadius()` and
`0.5f * GetEyeHeight()` are both live across the `mMorphBallState` load (`f31` and `f0`), so
neither can be inside the `if`. The fluid guard is `!IsInFluid()` up front, and the final compare
is `mDistanceUnderWater >= height` (`fcmpo; cror eq,gt,eq`), not `height <= ...`.

**Echoes' `GetGravity` (0x80189B38, 228 B) is not Prime 1's - two power-up tests.** It reads bit 5
of the byte at `CPlayer+0x126b` (`lbz; rlwinm. r0,r0,27,31,31`) and picks
`CPlayerState::kIT_LightSuit` (14) when it is set, `kIT_GravityBoost` (25) when it is not, then
`if (!HasPowerUp(that) && CheckSubmerged()) return GetFluidGravAccel();`, then
`if (mSidewaysDashing) return -100.f;`, else `GetNormalGravAccel()`. `GetTweakPlayer()` is called
on each returning path and the `-100.f` is a pooled constant at `-22956(r2)`. Whoever takes it
next needs a name for the `0x126b` bit; `CActor`'s `mDrawCrosshairs` is the byte at `0x1268`, not
this one, so it is a different member and I did not guess it.

## FinishSidewaysDash - a measured wall

`FinishSidewaysDash` (0x80189520, 312 B) was 13.01% from the scaffold. I recovered the whole
body from retail and Prime 1 and it came within a register allocation of matching; **I reverted
it**, because an unmatched rewrite is not progress under this item's rules. Recording it here so
the next run does not re-derive it:

```
if (mSidewaysDashing) {
  mDoneSidewaysDashing = true;                                  // stb r0,0x588
  if (mMovementState != kMS_OnGround) {                         // lwz 0x2d0; cmpwi 0; beq tail
    CVector3f velocity = GetVelocityWR();                       // field-wise copy to r1+0x28
    CVector2f planar(velocity.GetX(), velocity.GetY());        // out-of-line __ct__9CVector2fFff -> r1+0x08
    CVector3f flat(planar.GetX(), planar.GetY(), 0.f);          // r1+0x1c, Magnitude() on it
    if (flat.Magnitude() > skStrafeDistancesEchoes[GetSurfaceRestraint()]) {
      const float accel = mAccelerationChangeTimer > 0.f ? GetAcceleration() : 1.f;
      const float scale = (speed - accel * (speed - cap)) / speed;   // fnmsubs; fdivs
      SetVelocityWR(CVector3f(scale * velocity.GetX(), scale * velocity.GetY(), velocity.GetZ()));
    }
  }
}
mSidewaysDashing = false; mStrafeInputAtDash = 0.f; mDashTimer = 0.f;
```

- The cap table is `lbl_803A9FB0` (`lis r4,-32709; addi r4,r4,-24656`), 8 floats, used for
  indices 0..7 = `CPlayer::ESurfaceRestraints`:
  `{11.8f, 18.f, 15.f, 10.f, 10.f, 10.f, 10.f, 10.f}`. The *next* 8 floats at `0x803A9FD0`
  (`{11.8, 11.8, 11.8, 5, 6, 5, 5, 6}`) are Prime 1's `skStrafeDistances`, so the two tables
  are both there and only the first is indexed here.
- `mAccelerationChangeTimer` (`0x368`) is the selector, `mDashSpeedMultiplier` is not involved.
- Tried, all producing the same 344-352 B object against retail's 312 B, differing only in
  register allocation: `const`/`non-const` velocity local; `GetX()/GetY()/GetZ()` vs `operator[]`;
  named `CVector3f clamped = velocity; SetX(); SetY()`; `SetVelocityWR(CVector3f(...))` as a
  temporary; the `scale` expression spelled with the ternary inlined.
- The one remaining difference: **MWCC hoists the velocity's x/y/z into `f30`/`f29`/`f28` and keeps
  them live across the three calls, while retail spills only the `CVector3f` at `r1+0x28` and
  reloads `48(r1)`/`40(r1)`/`44(r1)` at the point of use**, with just `f31` (speed) and `f30` (cap)
  live. Retail's frame is 96 B, ours 128 B. Ours also puts `maxSpeed` in `f27` instead of reusing a
  scratch register. So the remaining gap is not the arithmetic - every `fmuls`/`fdivs`/`fsubs`/`fnmsubs`
  and every constant already agrees - it is whether MWCC scalar-replaces that one `CVector3f`
  local. Something in retail's source keeps it in memory. Worth trying next: a `CVector3f`
  *reference* local (`const CVector3f& velocity = GetVelocityWR();`, which should forbid the
  scalar replacement), or reading the components a second time through `GetVelocityWR()` instead of
  through a copy.

WALL: FinishSidewaysDash 13.0% - body recovered from retail exactly; only MWCC's scalar replacement of the velocity local differs (it hoists x/y/z into f30/f29/f28 where retail reloads them from r1+0x28), 4 spellings tried

## Not attempted (and why)

- `fn_80189CA8` (0x80189CA8, 88 B) is retail's `TReservedAverage<float, 20>::GetAverage` - its
  three lines are visible (`count == 0` -> clear the flag; else `GetAverageValue<f>(data,
  count)` -> set it). It is **unnamed in `config/G2ME01/symbols.txt`**, so our
  `_ZNK16TReservedAverageIfLi20EE10GetAverageEv` would not line up by name and the port's
  `PortReachStubs.cpp:1408` already owns that symbol's reach-stub. Left alone.
- `GetDampedClampedVelocityWR` (508 B), `UpdateStepCameraZBias` (512 B, now takes `(dt, mgr)`,
  which Prime 1's does not), `TurnInput` (600 B), `ForwardInput` (652 B), `JumpInput`,
  `ComputeMovement`, `ComputeDash`, `SidewaysDashAllowed`, `CalculatePlayerMovementDirection`,
  `CalculateLeaveMorphBallDirection`, `BombJump`, `Teleport`, `UpdateSubmerged`, `UpdateCameraBob`,
  the morph-ball transition block and the gravity-boost block are all 0-1.5% and need real
  decompiling, not a donor transplant. They are the natural next run's queue.
---

# Second run (lane 7, 2026-10-01) — 23 -> 24 / 62

Re-measured first: the clean tree already carried the first run's 23/62, so nothing here is
`STALE:`. One function went to an exact match and one went from a stub to 96.5%.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`, before and after
(`./tools/fast_try.sh MetroidPrime/Player/CPlayerDynamics`):

| | before | after |
|---|---|---|
| `matched_functions` | 23 / 62 | **24 / 62** |
| `fuzzy_match_percent` | 8.72 | 11.27 |
| `matched_code` | 2144 / 27020 (7.93%) | 2372 / 27020 (8.78%) |

Whole build: `All: 34.23% fuzzy, 27.35% matched, 12.84% linked (12098 / 28465 functions)`;
`matched 12097 -> 12098`, `linked 5849 -> 5849` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok.

| function | retail | before | after | what the bytes demanded |
|---|---|---|---|---|
| `GetGravity` | 0x80189B38, 228 B | 3.51% | **100%** | the first run's recovered body, plus the right bit of 0x126B |
| `GetDampedClampedVelocityWR` | 0x80189D00, 508 B | 4.23% | 96.54% | Echoes' own arithmetic (below), not Prime 1's donor |

Files touched: `src/MetroidPrime/Player/CPlayerDynamics.cpp` only (plus `#include
"MetroidPrime/Player/CGameState.hpp"` for `gpGameState`). No header change, no other unit's
`.text` moves, no `asm`.

## The 0x126B bit `GetGravity` branches on is `x126b_26_`

The first run's notes said the bit at `CPlayer+0x126b` was unnamed and that "whoever takes it
next needs a name for it". It still has no game-meaningful name, but the *position* is now
measured rather than guessed, which is all the body needs: compiling `if (<field>)` against
each of that byte's eight 1-bit members in turn, only `x126b_26_` emits retail's
`rlwinm. r0,r0,27,31,31`. The map is worth keeping — the rotate amount *is* the bit index, and
`27` means bit 4, which is the sixth 1-bit field of `0x126b`:

| field | `rlwinm` shift emitted | retail reads |
|---|---|---|
| `x126b_24_` | 25 | bit 0 |
| `x126b_25_` | 26 | bit 1 |
| **`x126b_26_`** | **27** | **bit 4 — this one** |
| `x126b_27_` | 28 | bit 3 |
| `mDeathFadeEnabled` | 30 | bit 5 |
| `mUseAlternateBeam` | 31 | bit 6 |
| `x126b_31_` | `clrlwi` | bit 7 |

Note the header's *names* are not in bit order — `mDeathFadeEnabled` sits at bit 5, above
`x126b_26_` at bit 4 — so the numeric suffix in a `x126b_NN_` name is the declaration order,
not the bit. Do not read a bit index off the name.

**The two power-up arms are not symmetric, and that is the whole function.** Set: ask only
`kIT_LightSuit`; if absent, return `GetTweakPlayer()->GetFluidGravAccel()`. Clear: ask
`kIT_GravityBoost`, and only if absent *and* `CheckSubmerged()`, the same fluid value. Both
arms are the same shape — `gpGameState->GetPlayerState()` writes an `rstl::rc_ptr` out-param
at `16(r1)` / `8(r1)`, the `HasPowerUp` result is copied to `r31`, `ReleaseData` runs, and only
then is `r31` tested (`clrlwi. r0,r31,24`). So a hoisted
`rstl::optional_object`-style local cannot match; the temporary has to be a full expression.
Then `mSidewaysDashing` (byte at 0x578) gives the pooled `-100.f` at `-22956(r2)`, else
`GetNormalGravAccel()`.

## `GetDampedClampedVelocityWR` — Echoes' arithmetic is not Prime 1's

Prime 1's donor (`prime-ref/src/MetroidPrime/Player/CPlayerDynamics.cpp:48`) is a decoy here.
Retail 0x80189D00 differs in four measured ways:

1. **The friction is scaled by the acceleration.** `GetAcceleration()` is called *first*, into
   `f31`, before the `TransposeRotate`, and `fmuls f29,f29,f31` multiplies the friction by it
   after both arms. Prime 1 has no such multiply.
2. **The `kSR_Air` arm replaces the friction outright**, and the replacement is
   `3.f * (CVector2f(x, y).Magnitude() / GetMass())` — `lfs f29,-22952(r2)` (3.f) loaded into
   the *friction* register before the two calls and held across them, `lfs f30,344(r30)` is
   `CPhysicsActor::mMass` (0x158), then `fdivs` then `fmuls`. It is a **separate assignment**
   (`friction = 3.f;` … `friction = friction * (planar.Magnitude() / GetMass());`), not the
   left operand of one product: written as one expression the compiler loads the 3.f *after*
   the calls and the two `lfs` order swaps. That one change moved it 90.6% -> 96.4%.
3. **The sign-preserving clamp's bound is `maxSpeed / accel`.** `fdivs f3,f1,f31` divides the
   max speed by the acceleration into `f3`, and `CMath::Limit` is then called with that.
   Prime 1 passes `maxSpeed` directly. `CMath::Limit` inlines correctly as it stands.
4. **`mOrbitState` (0x3A4) is tested first**, and only `== kOS_NoOrbit` enters the friction
   block — Prime 1 tests `mMovementState` and `GetSurfaceRestraint()` in the guard.

**`CMath::Max`/`CMath::Min` return `const T&`, so they cannot be used here.** They compile to
an out-of-line call (`Max<f>__5CMathFRCfRCf`) plus a reload through the returned pointer, which
is 8 instructions where retail has 5. The four clamps have to be written as statements with a
`float r = 0.f;` accumulator. What is *not* free is the branch direction: retail's inner test
is `fcmpo cr0,f1,f0; bge <skip the fmr>` — the branch jumps over the `fmr`, so the condition is
`v > 0.f` with the *store of v* as the fall-through, not `v < 0.f` with a jump to the zero
store. Writing `if (0.f < v) r = v;` scores 96.5% but emits `ble` where retail has `bge`.

**A 99.84% spelling exists and is WRONG — do not take it.** `if (v > 0.f) { r = 0.f; } else { r = v; }`
(inverted) emits byte-identical code to retail for the clamp and scores 99.84%, because the
`bge`/`ble` pair is symmetric under swapping which arm is the fall-through. It zeroes the
velocity component when it is *positive*, which is the opposite of the friction clamp. I
measured it, recognised it, and reverted it. A percentage that high is not evidence; the
correct spelling is 96.54% and that is what is in the tree. The remaining 3.5% is that one
branch polarity per clamp, four sites.

## `ActivateMorphBallCamera` is 99.95% and correct, but the gate rejects it

Retail 0x80184240 is two calls, no branches: `SetCameraState(2, mgr)` then
`mCameraManager->mBallCamera->SetState(kBCS_Default=0, mgr)`. That is
`SetCameraState(kCS_Two, mgr); mCameraManager->BallCamera()->SetState(CBallCamera::kBCS_Default, mgr);`
and it reaches 99.95% (the last 0.05% is a `li r4,2` immediate the compiler hoists differently).
**I reverted it**, because:

```
link_check: STRICT FAIL - regression gate: 292 undefined against a baseline of 291 (GREW)
port link gap, ... link gap not accounted for:
  gap grew: _ZN11CBallCamera8SetStateENS_16EBallCameraStateER13CStateManager is not in port_link_gap_list.md
```

`CPlayerDynamics.cpp` is in `files.cmake`, so it is compiled into the port, and the first time
anything in it *calls* `CBallCamera::SetState` the port's link has to resolve it. That symbol's
only definition is `src/MetroidPrime/Cameras/CBallCamera.cpp:650`, and that path is in
`tools/check_files_cmake.py`'s `EXCLUDED` (listing it opens 69 symbols), so the port never
compiles it. Same shape as the first run's `CTweakBall::GetMaxBallTranslationAcceleration`
problem, except there the body was small enough to host in `PortCTweakBall.cpp`; here
`CBallCamera::SetState` is a 448-byte function calling eight more unhosted camera symbols, and
hosting it is a real port item, not something to smuggle into a progress item.

Two things are worth keeping for whoever finishes it. First, **`EPlayerCameraState` is Prime
1's ordering and is wrong for Echoes**; `SetCameraState`'s own switch (0x80016428) shows 2 and
4 to be the two ball arms — 2 hands the view to the ball camera when the current camera is not
already the first-person one, 4 sets the ball camera outright. So `kCS_Ball == 1` in this header
is a Prime 1 artefact; `kCS_Cinematic == 5` (the one addition) is right. Correcting the enum
is a change to a shared header, so it is its own item. Second, `#include "MetroidPrime/CCameraManager.hpp"`
and `MetroidPrime/Cameras/CBallCamera.hpp` are both required — `mCameraManager` is an incomplete
type in `CPlayer.hpp`.

## Not attempted (and why)

- The gravity-boost trio (`StartGravityBoost` 356 B, `ApplyGravityBoost` 200 B, `EndGravityBoost`
  268 B) is Echoes-only; Prime 1 has no counterpart. `EndGravityBoost` (0x80183648) I read in
  full: dampen the velocity's Z by `GetTweakPlayer()->GetGravityBoostCancelDampening()`, zero
  `mGravityBoostDuration` (pooled 0.f at `-23120(r2)`), `SfxStop` the old `mGravityBoostSfx` if
  non-zero, `SfxStart` a new one with `GetSoundPan(kMSP_4)` and `ReturnFirstIfSingleElseSecond(864, 863)`,
  then `SetIgnoreAreaLowPass(h, true)` and `ApplySubmergedPitchBend(h)`. **It cannot be written
  yet**: `CSfxManager::SfxStop` takes its handle **by value** here, but retail's disassembly
  passes a *pointer* (`addi r3,r1,20` after storing the handle to `20(r1)`, and the callee does
  `lwz r0,0(r3)`), i.e. retail's real prototype is `SfxStop(CSfxHandle&)`. Same for
  `SetIgnoreAreaLowPass`, passed `&localHandle`. Fixing those two prototypes in
  `Kyoto/Audio/CSfxManager.hpp` would touch ~30 call sites and several units — a real item, and
  the thing that unblocks the whole gravity-boost cluster.
- `UpdateSubmerged` (232 B, 1.72%) I also read: it clears bit 2 of 0x126B, zeroes
  `mDistanceUnderWater`, and if `0x110` is set walks `fn_801C0124(this+0xEDC)` ->
  `InFluidId()` -> `mgr.GetObjectById()` -> `TCastToPtr<CScriptWater>` -> `GetWRSurfacePlane()`,
  then sets `mDistanceUnderWater` to the negated plane distance and sets bit 2 of 0x126B when
  the script water's field at `+0x44` is 2 (`subfic`/`cntlzw`, so the test is `== 2`). Blocked on
  the same unknown: what `CPlayer+0x110` is, and the `+0x1C8`/`+0x44` chain on `CScriptWater`.
- `fn_80189CA8` is still `TReservedAverage<float, 20>::GetAverage` and still unnamed in
  `symbols.txt`; unchanged from the first run.
- The morph-ball cluster (`UpdateMorphBallTransition` 1016 B, `fn_801843d0` 1680 B,
  `TransitionToMorphBallState` 1156 B, `TransitionFromMorphBallState` 1048 B, the rest) and the
  input cluster (`ComputeMovement` 2356 B, `JumpInput` 1768 B, `ComputeDash` 1408 B,
  `SetMoveState` 1052 B, `ForwardInput` 652 B, `TurnInput` 600 B) are untouched and remain the
  next run's queue. `FinishSidewaysDash` is still the measured wall the first run left, with the
  same four spellings behind it.

NEW: port-CBallCameraSetState-home | port | CBallCamera::SetState | Its only definition is
src/MetroidPrime/Cameras/CBallCamera.cpp:650, which check_files_cmake.py EXCLUDES (listing it
opens 69 symbols), so the port never compiles it. CPlayer::ActivateMorphBallCamera (0x80184240)
is 99.95% and needs it; hosting it means giving CBallCamera's 448-byte SetState and its eight
callee bodies a compiled home in the port.
NEW: sfx-handle-params-by-reference | progress | Kyoto/Audio/CSfxManager | Retail passes CSfxHandle
*pointers* to CSfxManager::SfxStop and ::SetIgnoreAreaLowPass (`addi r3,r1,N` then `lwz r0,0(r3)`
in the callee), so the by-value prototypes in CSfxManager.hpp are wrong; fixing them touches ~30
call sites and is what blocks the three gravity-boost functions in CPlayerDynamics.

---

# Third run (lane 6, 2026-10-01) — 24 -> 26 / 62

Re-measured first on the clean tree: the unit carried the second run's 24/62, so
nothing here is `STALE:`. Two functions went to an exact byte match.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`:

| | before | after |
|---|---|---|
| `matched_functions` | 24 / 62 | **26 / 62** |
| `fuzzy_match_percent` | 11.27 | 12.34 |
| `matched_code` | 2372 / 27020 (8.78%) | 3192 / 27020 (11.81%) |

Whole build: `All: 34.33% fuzzy, 27.55% matched, 12.89% linked (12154 / 28465 functions)`;
`matched 12152 -> 12154`, `linked 5860 -> 5860` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok.
No new link gap: the only new callee is `__ct__9CVector2fFff`, which the port already hosts.

| function | retail | before | after |
|---|---|---|---|
| `GetDampedClampedVelocityWR` | 0x80189D00, 508 B | 96.54% | **100%** |
| `FinishSidewaysDash` | 0x80189520, 312 B | 13.01% | **100%** |

File touched: `src/MetroidPrime/Player/CPlayerDynamics.cpp` only.

## The friction clamp: an if/else, not `r = 0.f; if (v > 0.f)`

The second run left `GetDampedClampedVelocityWR` at 96.54% and correctly refused a
99.84% spelling that inverted the polarity (`if (v > 0.f) { r = 0.f; } else { r = v; }`)
as byte-identical but semantically wrong. **The polarity was never the problem — the
*shape* was.** Retail's four clamps are:

```
fcmpo cr0,f1,f0
bge   L_then        ; v >= 0 -> the fmr
b     L_join        ; else skip it
L_then: fmr f0,f1
L_join: stfs f0,32(r1)
```

That is a two-armed `if/else` whose *then* arm is the copy, written:

```cpp
float r;
if (v < 0.f) {        // positive-side clamp
  r = 0.f;
} else {
  r = v;
}
```

The correct-polarity `if (v < 0.f) { r = 0.f; } else { r = v; }` scores **100%**. The
99.84% inverted spelling the previous run found is the *same* code with the arms swapped,
which is why it was byte-identical and why both spellings compile to retail's shape — the
distinction is only visible in what the function computes, not in the bytes. Lesson: when a
spelling is 99.x% and byte-identical to a rejected one, the two arms are the same code and
the *ordering* is the whole difference; check the semantics of both before believing either.

The negative-side clamp is the mirror, `if (0.f < v) { r = 0.f; } else { r = v; }`. Measured
matrix (10 positive spellings x 9 negative, each rebuilt): the `if/else` form wins in both
positions; `float r = 0.f; if (v > 0.f)` is 96.54%, `>=` is 94.96%, a ternary 94.96-96.54%,
`!(v < 0.f)` 93.39%.

## FinishSidewaysDash: two separate fixes, 13% -> 99.24% -> 100%

The first run recorded a wall here, attributing the whole gap to MWCC's scalar replacement of
the velocity local. That was one of **two** problems, and the other was a call order.

**1. `GetSurfaceRestraint()` is called BEFORE `Magnitude()` (85% -> 94%).** Retail:

```
801895a0: bl GetSurfaceRestraint     ; the index
801895b0: addi r3,r1,28 ; bl Magnitude__9CVector3fCFv   ; the vector
```

so the source reads `const float cap = sk[GetSurfaceRestraint()];` *before*
`const float speed = flat.Magnitude();`. Written the other way round, MWCC puts the table
base in r3 instead of r4 and the score drops 7 points.

**2. A `const CVector3f&` alias of a *value* local is what stops the promotion (94% -> 99.24%).**

Retail materialises the velocity at `r1+0x28` (three `stfs` right after loading
424/428/432(r31)) and reloads 48/40/44(r1) at the point of use; the naive spellings keep
x/y/z in f30/f29/f28, which costs three extra callee-saved spills and a **128 B frame against
retail's 96**. `GetVelocityWR()` returns `const CVector3f&`, so there is no copy in the source
— but retail clearly has one. What reproduces it:

```cpp
CVector3f v = GetVelocityWR();
const CVector3f& velocity = v;   // the alias is what pins it in memory
```

The value copy alone scores 74%; the alias alone (binding straight to `GetVelocityWR()`) 93%.
It is the **combination** — a real stack object that the rest of the body reads through a
reference — that gives 99.24% and retail's 96 B frame. Ten spellings tried here; only this
one and its const/non-const variants reach 99.24%, so the shape is pinned, not a plateau of
luck.

**3. The last 0.76% is the X/Y store order of the argument (99.24% -> 100%).** Retail stores
out.x at 16(r1) before out.y at 20(r1); a `CVector3f(a, b, c)` temporary does the opposite,
because MWCC evaluates the constructor arguments in the other order. Writing the three fields
with `SetX`/`SetY`/`SetZ` on a named default-constructed `CVector3f out` pins the order:

```cpp
CVector3f out;
out.SetX(scale * velocity.GetX());
out.SetY(scale * velocity.GetY());
out.SetZ(velocity.GetZ());
SetVelocityWR(out);
```

Stating the fields in the order X, Y, Z is what retail does; `SetZ` first gives 99.42%,
named temporaries 99.12%, scaling in place 96.94%. **The instruction-level diff was worth more
than another 20 blind spellings**: at 99.24% the two objects were the same size and had the
same instruction count, so the only thing left was visible in one hunk.

The table `skStrafeDistancesEchoes` is added to this file as a `static const float[8]` at the
top of the unit (`lbl_803A9FB0` = `{11.8f, 18.f, 15.f, 10.f, 10.f, 10.f, 10.f, 10.f}`, confirmed
by `objdump -s` on `main.elf`; the 8 floats after it at 0x803A9FD0 are Prime 1's
`skStrafeDistances`, which this function does not use). Adding a file-scope const array adds
no undefined symbol, so the port's link gap did not move.

## Tools worth having

- **Instruction-level diff of a compiled function against retail** (parse both
  `objdump -d` streams, align, print the non-equal runs). The repo has `tools/dis.sh` for retail
  and `fast_try.sh` for the score, but nothing that pairs them; the pairing is what turned
  "13%, register allocation" into three named, separately-fixable defects. It lives in
  `.tmp/opencode/` — scratch, not a `tools/` change.
- **`objdiff-cli diff -1 <retail.o> -2 <ours.o> <symbol> -o -`** returns per-instruction JSON.
  The catch: it wants the symbol and *both* paths, and the two forms that error out are
  `diff a.o b.o` and `-C .`.
- **`fast_try.sh` re-runs the whole objdiff report** (~1.5 s), so a 10x9 spelling matrix is 90
  rebuilds in about three minutes. That is cheap enough to brute-force a shape instead of
  reasoning about the compiler.

## Still open

- The gravity-boost trio is blocked on the `CSfxHandle` pass-by-reference prototype fix
  (NEW line below, already filed by the second run).
- `UpdateSubmerged` is blocked on the unknown `CPlayer+0x110` member and the
  `CScriptWater+0x1C8`/`+0x44` chain.
- `ActivateMorphBallCamera` is 99.95% and needs `CBallCamera::SetState` hosted (NEW below).
- `fn_80185814`, `fn_80185870` and `fn_801894C4` are three **92-byte destructors** at 0.00%, all
  three byte-identical to each other apart from one vtable address: `mr. r31,r3; beq end`,
  `stw <vtable>,0(r31)`, `extsh. r0,r4; ble end`, `bl Free__7CMemoryFPCv`. They are the cheapest
  un-matched functions in the unit (three at once) and I did not attempt them — naming the
  three classes they destroy is the unknown. Their vtables are at `0x803B5B3C`, `0x803B5B48`,
  `0x803B5B30` (`lbl_803B5B30` size 0xC = three entries), with a shared `lbl_803B1750`.
- `fn_80189EFC` (216 B, 0.00%) is a **static constructor**: five `bl __shl2i` with
  `r3=0, r4=1, r5=<static>`, results OR-ed into r30:r31 and stored at `-27436/-27440(r13)`,
  then five fields written into `0x803B5B30 + 0x40`. Not attempted.
- `fn_80189CA8` is still `TReservedAverage<float, 20>::GetAverage` and still unnamed in
  `symbols.txt`; unchanged from the first run.
- The morph-ball and input clusters are untouched and remain the next run's queue.

---

# Fourth run (lane 3, 2026-10-02) - 26 -> 27 / 62, plus one function to 99.18%

Re-measured first on the clean tree: the unit carried the third run's 26/62, so nothing here is
`STALE:`. One function went to an exact byte match; one more went from an empty stub to 99.18%
(three instructions out of 128) and is left in the tree, documented below.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`:

| | before | after |
|---|---|---|
| `matched_functions` | 26 / 62 | **27 / 62** |
| `fuzzy_match_percent` | 12.34 | 15.40 |
| `matched_code` | 3192 / 27020 (11.81%) | 3520 / 27020 (13.03%) |

Whole build: `All: 34.47% fuzzy, 27.74% matched, 12.89% linked (12204 / 28465 functions)`;
`matched 12203 -> 12204`, `linked 5860 -> 5860` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok.

| function | retail | before | after | what the bytes demanded |
|---|---|---|---|---|
| `CalculateLeaveMorphBallDirection` | 0x80186E9C, 328 B | 1.22% | **100%** | Prime 1's body verbatim, with `xfe8_` (0xfe8) as the copy destination and an extra `mMorphBall->InScrewAttackMode()` early-out |
| `UpdateStepCameraZBias` | 0x80189860, 512 B | 0.78% | 99.18% | Prime 1's body plus the riding-platform test and the `newBias` load order |

Files: `src/MetroidPrime/Player/CPlayerDynamics.cpp` only, plus
`include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp:105-107` (one inline accessor; **no layout
change**, nothing else in the tree calls it, so no other unit's `.text` moves). No `asm`, no new
undefined symbol in the port link (`CStateManager::GetObjectById` and `TCastToPtr<CScriptPlatform>`
are both already hosted, which is why keeping the 99.18% body is free).

## `r2` is `_SDA2_BASE_`, so every `lfs fX,-NNNN(r2)` is a table lookup, not a guess

`lfs ...,off(r2)` is *not* a mystery register: **`r2` = `_SDA2_BASE_` = 0x804223C0** (the constant
`tools/sda.py` already prints). So the value of a pooled float is `float32[0x804223C0 + off]`, and
that one lookup retires the guesswork the first two runs were doing by trial:

    0x804223C0 - 23072 = 0x3E99999A = 0.3f      (CalculateLeaveMorphBallDirection)
    0x804223C0 - 23108 = 0x3F000000 = 0.5f      (idem, and SidewaysDashAllowed's threshold)
    0x804223C0 - 23080 = 0x3C23D70A = 0.01f     (SidewaysDashAllowed)
    0x804223C0 - 23088 = 0x40A00000 = 5.f       (UpdateStepCameraZBias)
    0x804223C0 - 22956 = 0xC2C80000 = -100.f    (GetGravity, second run)
    0x804223C0 - 23120 = 0                       (0.f, used by all three)

(The second run's "3.f at -22952(r2)" is wrong: it is **3.5f**.)

## The 1-bit-field convention, measured - and it corrects the second run's table

**The `rlwinm` shift is the only reliable datum; the "bit" column of a previous note is not.**
Measured here by compiling `if (<field>) g = <distinct immediate>;` against each one-bit member of
the byte at 0x1269 (that probe is the trick - store a *different constant* per field, so the answer
survives any reordering by the register allocator; a `g[i] = field` probe does not):

| field | emitted | field | emitted |
|---|---|---|---|
| `x1269_24_` | `rlwinm ...,25,31,31` | `x1269_28_` | `...,29,31,31` |
| `mHitWallDuringMove` | `...,26,31,31` | `mInterpolatingControlDir` | `...,30,31,31` |
| `x1269_26_` | `...,27,31,31` | `x1269_30_` | `...,31,31,31` |
| `x1269_27_` | `...,28,31,31` | `x1269_31_` | `clrlwi ...,31` |

So **declaration order gives descending shifts 25..31 and then `clrlwi`** - the first declared field
is *not* the MSB, which is why the second run's bit column does not reproduce. Corrected for 0x126B:
`mDeathFadeEnabled` (shift 30) and `mUseAlternateBeam` (shift 31) are **bits 1 and 0**, not bits 5
and 6; only `x126b_26_` -> shift 27 -> bit 4 in their table is right. Its *shift* column is fine and
that is the part the `GetGravity` conclusion rests on, so nothing they landed changes.

**A read and a write of the same flag use different numbers.** A flag read with
`rlwinm rX,rY,S,31,31` is written with `rlwimi rX,rZ,(31-(S-1)),S-1,S-1` - the mask is the read
shift **minus one** (measured both ways: our `x1269_26_ = false` emits `...,5,26,26` for read-shift
27, and retail's `rlwimi r0,r3,4,27,27` is the flag it reads at shift 28). This is what proved that
`UpdateStepCameraZBias` tests *and clears the same* bit - Prime 1's single `mStepCameraZBiasDirty` -
and both are `x1269_27_` here, not two different fields.

`CScriptPlatform`'s flag byte at 0x48c follows the same rule: retail reads shift 29, which by the
measured ordering is the **fifth** declared one-bit field, **`mMotionActive`** (not
`mPassedMotionEnd`, which is sixth and reads at shift 30 - trying it first cost 6 points). It needed
a public accessor; the flag block itself is `private`.

## `CalculateLeaveMorphBallDirection`: Prime 1's body, one extra term

Prime 1's 14 lines are right, with two Echoes differences: the outer test is
`mMorphBallState != kMS_Morphed || mMorphBall->InScrewAttackMode()` (retail's `cmpwi 1; bne` +
`InScrewAttackMode; beq`, i.e. one `||`, short-circuited), and both copies of the move direction land
in `xfe8_` at 0xfe8 (`mMoveDir` is at 0xfdc) - the header has no name for that member. The four
`GetAnalogInput` calls stay in f29/f30/f31/f1 across the whole `||` chain, which is why they must be
four named locals evaluated before the compares, not inlined into the condition.

## `UpdateStepCameraZBias`: 0.78% -> 99.18%, three instructions left

Prime 1's body plus a `CStateManager&`, because Echoes also zeroes the bias while the riding
platform's motion is running: `mRidingPlatform` (0x124c, the `TUniqueId`) is looked up with
`mgr.GetObjectById`, cast with `TCastToPtr<CScriptPlatform>`, and its `mMotionActive` flag becomes a
third conjunct on the `kMS_OnGround && !IsMorphBallTransitioning()` guard. Two spellings mattered:

- **`newBias` is three statements, not one.** `float newBias = GetUnbiasedEyeHeight() +
  GetTranslation().GetZ();` emits `fadds f31,f1,f31` with `lfs f31,92(r3)` hoisted *before* the call
  (93.8%). Retail loads the translation *after* the call into f0 and adds. The spelling that
  reproduces it is a separate `const float groundZ = GetTranslation().GetZ();` then
  `newBias = groundZ + newBias;` - the named temporary stops the hoist and puts the translation on
  the left of the `fadds`. That one change is worth ~4 points.
- **`TCastToConstPtr`, not `TCastToPtr`.** `CStateManager::GetObjectById` is the `const` overload and
  returns `const CEntity*`, which `TCastToPtr<T>(CEntity*)` will not take; `TCastToConstPtr` is the
  wrapper that exists for exactly this and still calls `TCastToPtr` with a `const_cast`, so the
  emitted `bl TCastToPtr<15CScriptPlatform>` is retail's.

What is left is **one register move**: retail has `rlwinm r31,r0,29,31,31`, we have
`rlwinm r0,r0,29,31,31` + `mr r31,r0` - MWCC does the shift in place in r0 and copies, where retail
shifts straight into the flag's callee-saved register. Seven spellings tried, all 99.18% or worse:
`bool` and `int` flags; `if (T* p = ...)` initialiser-statement vs a separate declaration plus
`if (p != nullptr)`; `p != nullptr && p->IsMotionActive()` in one expression (96.8% - it introduces
`li r4,0`/`li r4,1`); a `bool&` alias of the flag; a second flag copied from the first; and the
`const CEntity*` intermediate. The body is correct and complete, so I kept it rather than reverting
an empty stub for a function that is one move from matching; it costs nothing at the gate.

WALL: UpdateStepCameraZBias 99.18% - body complete, only `rlwinm r31,r0` vs `rlwinm r0,r0`+`mr r31,r0` (the bit-test destination for the platform flag); 7 spellings tried

## `SidewaysDashAllowed` (520 B, 1.08%) - blocked on one unhosted symbol, nothing else

I read retail 0x80189658 in full against Prime 1 and the only *new* callee is unhosted. Its guard is
`bit 6 of the byte at 0x1269` then `bit 5`, i.e. **`x1269_24_` then `mHitWallDuringMove`** - and
Prime 1's order is `mSlidingOnWall || mHitWall || mOrbitState != kOS_OrbitObject`, so `x1269_24_` is
Prime 1's `mSlidingOnWall` and the existing name on bit 5 is right. The rest is Prime 1's body with
`JumpHeld`/`JumpPressed` as `CPlayer` members (defined in `src/MetroidPrime/Player/CPlayerVisor.cpp`,
in `files.cmake`), `0.01f` from `-23080(r2)`, and `CMath::SqrtF` + `Magnitude` on the stick edge.

The blocker is `fn_80012D10` (retail 260 B, `CVector3f& out, float strafe, float forward`, an
`atan`-based left-stick-edge calculation). It sits **inside `CPlayer.cpp`'s own range**
(0x8000B8E8..0x8001D0CC) but is declared nowhere in `include/` and defined nowhere in `src/`, so the
port cannot resolve a call to it - exactly the `CBallCamera::SetState` shape the second run hit.

NEW: cplayer-fn80012D10 | progress | MetroidPrime/Player/CPlayer | fn_80012D10 (260 B) is inside
CPlayer.cpp's range but is declared and defined nowhere, so the port cannot link a call to it;
implementing it (atan-based left-stick edge, out-param + two floats) also unblocks
CPlayer::SidewaysDashAllowed, retail 0x80189658, 520 B at 1.08%, which is otherwise fully recovered.

## `ApplyGravityBoost` (200 B, 2.00%) - read, one unknown link left

Full body from retail 0x80183754, so the next run does not re-derive it:

```cpp
void CPlayer::ApplyGravityBoost(float dt, CStateManager& mgr) {
  if (mGravityBoostDuration > 0.f) {                 // 0x1250
    mGravityBoostDuration -= dt;
    if (mGravityBoostDuration <= 0.f) { EndGravityBoost(mgr); return; }
    // mgr->mPlayers[GetPlayerIndex()]-><+0x18>-><+0x110> != 0 && mMorphBallState == kMS_Unmorphed
    if (mgr.mPlayers[GetPlayerIndex()]->mX->mY != 0 && mMorphBallState == kMS_Unmorphed) {
      CVector3f force(0.f, 0.f, GetTweakPlayer()->GetGravityBoostForce());
      ApplyForceOR(force, CAxisAngle::Identity());
    } else {
      EndGravityBoost(mgr);
    }
  }
}
```

`mgr+0x151C` is `CPlayer* mPlayers[4]` (so the guard reads the *other* player's field, not this
one). The unresolved link is the two-hop `+0x18 -> +0x110`: `+0x110` is the same unnamed `CPlayer`
byte `UpdateSubmerged` tests (second run), so both of those functions are blocked on naming it.
The two `CAxisAngle`/`CPhysicsActor` callees and `GetGravityBoostForce` are all already hosted, so
naming that member is the whole of this function's cost.

## Still open (unchanged from the third run unless listed above)

- The gravity-boost trio (`StartGravityBoost`, `EndGravityBoost`) is blocked on the
  `CSfxHandle` pass-by-reference prototypes (NEW line filed by the second run).
- `UpdateSubmerged` is blocked on `CPlayer+0x110` - which `ApplyGravityBoost` now also needs.
- `ActivateMorphBallCamera` is 99.95% and needs `CBallCamera::SetState` hosted (NEW, second run).
- `fn_80185814`, `fn_80185870`, `fn_801894C4` are the three 92-byte destructors. **New data for
  them, which the third run did not have:** each stores its own vtable and then `lbl_803B1750`, and
  `lbl_803B1750` is `{0, 0, fn_8000DF48}` - a 72-byte root destructor at 0x8000DF48 that only stores
  that vtable and calls `CMemory::Free`. So the chain is root(unknown) <- three classes, each with
  exactly one virtual (its own destructor) and no members to destroy. The vtables are
  `lbl_803B5B30/3C/48` = `{0, 0, <own dtor>}`, and their only construction sites are inside this
  same unit's `fn_801892a0` (0x80189400) and `fn_80184ba4` (0x80185440); `TransitionToMorphBallState`
  (0x8018576c) builds a 20-byte `{vptr, int 13, 3 floats}` from `GetPlayerGun()` fields 40/56/72 and
  copies it to `this+0x120`. Naming the root class is the only unknown - and note the constructors
  *are* in this TU, so these are almost certainly local or header classes, not remote ones.
- `fn_80189CA8` is still `TReservedAverage<float, 20>::GetAverage` and still unnamed in
  `symbols.txt`; unchanged from the first run.
- `fn_80189EFC` (216 B static constructor), the morph-ball cluster and the input cluster
  (`ComputeMovement`, `JumpInput`, `ComputeDash`, `SetMoveState`, `ForwardInput`, `TurnInput`) are
  untouched.

## Tools worth having (extending the third run's list)

- **Instruction-level differ between retail and our object, keyed on mnemonic *and operands*, with
  branch displacements normalised.** Run 3's version keyed on the mnemonic alone, which hid the
  `fadds f31,f0,f1` vs `fadds f31,f1,f0` operand swap - that one operand is worth ~4 points and
  looked like a match. `.tmp/opencode/idiff.py <symbol>`; scratch, not a `tools/` change.
- **`build/tools/objdiff-cli diff -1 <retail.o> -2 <ours.o> <sym> -o -` is not useful per function
  here**: with a symbol argument it reported only section-level `.data` diffs and no code diff, even
  at 93.8%. `fast_try.sh`'s per-function percentage plus the differ above is the working pair.
- **A one-bit-field probe must give each field its own constant.** `g[i] = field` is unreadable -
  the allocator hoists all eight `rlwinm`s and can emit them in an order that does not match the
  stores. `if (field) { g = <distinct immediate>; }` is unambiguous and costs one build.

---

# Fifth run (lane 4, 2026-10-02) — 27 -> 30 / 62, and a fresh WALL on `UpdateStepCameraZBias`

Re-measured first on the clean tree: the unit carried the fourth run's 27/62, so nothing here is
`STALE:`. Three functions went to an exact byte match, all three of them the 92-byte deleting
destructors the fourth run had left as "the cheapest unmatched functions in the unit" with the class
naming as the unknown. **The class names turned out to be irrelevant** — see below.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`:

| | before | after |
|---|---|---|
| `matched_functions` | 27 / 62 | **30 / 62** |
| `fuzzy_match_percent` | 15.40 | 16.42 |
| `matched_code` | 3520 / 27020 (13.03%) | 3796 / 27020 (14.05%) |

Whole build: `All: 34.52% fuzzy, 27.86% matched, 12.89% linked (12226 / 28465 functions)`;
`matched 12223 -> 12226`, `linked 5860 -> 5860` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok.
`./tools/probe_sources.sh` = `752 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)`.
`python3 tools/check_symbol_names.py` = `0 declared names are missing from their object`.

| function | retail | before | after |
|---|---|---|---|
| `fn_80185814` | 0x80185814, 92 B | 0.00% | **100%** |
| `fn_80185870` | 0x80185870, 92 B | 0.00% | **100%** |
| `fn_801894C4` | 0x801894C4, 92 B | 0.00% | **100%** |

Files:
- `src/MetroidPrime/Player/CPlayerDynamics.cpp` — the three destructors (66 added lines, no
  existing line changed) plus `#include "Kyoto/Alloc/CMemory.hpp"`.
- `src/MetroidPrime/PortCPlayerDynamicsVtables.cpp` — **new**, 3 host-only vtable objects.
- `files.cmake` — one line for the new file.

## The three destructors: the class naming was a red herring

The fourth run left this as "naming the root class is the only unknown", and the third before it as
"they are almost certainly local or header classes". **Neither matters.** `fn_80185814`,
`fn_80185870` and `fn_801894C4` are written here under their retail `extern "C"` names with the
vtables referenced as objects and the store written by hand, exactly as
`src/MetroidPrime/Player/CMorphBall.cpp`'s `fn_800C88C0` / `fn_800C33DC` already do. That is the
repo's own answer to the whole question, and it is the *only* answer available: retail's symbol
table names these three `fn_80185814` / `fn_80185870` / `fn_801894C4`, so a real `~X()` would emit
`__dt__<mangled>`, which retail's object does not define and objdiff has nothing to pair with.
Nothing needs to know the class's name for the bytes to match. All three went to 100% on the first
spelling.

### The body is 14 instructions and 8 of them are two stores

```
stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / stw r31,12(r1)
mr. r31,r3 ; beq end
<own vtable store>              ; lbl_803B5B3C / 5B48 / 5B30
beq +0x10                       ; UNREACHABLE - see below
<base vtable store>             ; lbl_803B1750
extsh. r0,r4 ; ble end
mr r3,r31 ; bl CMemory::Free
epilogue, mr r3,r31
```

The source, which is what the repo writes:

```cpp
extern "C" void* fn_80185814(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B5B3C;
    if (self != nullptr) {                                   // the unreachable one
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
```

**The second `if (self != nullptr)` is unreachable and must be written anyway.** Its `beq` tests the
CR0 that the opening `mr. r31,r3` already set, so the `lbl_803B1750` store can never run — but
mwcceppc emits the `beq` because the store sits inside a second null test, which is what an inlined
`Base::~Base()` looks like in source. Written flat, the `beq` is dropped and the body is 84 bytes
instead of 92. This is not a new finding: `src/MetroidPrime/Player/CMorphBall.cpp:944-948` measures
it for `fn_800C88C0`/`fn_800C33DC`, which are the identical pair of stores and sit at 100%. Copying
that file's shape is what made all three land first try. **Read the `Port*.cpp` headers before
re-deriving a `fn_` in this tree.**

Three details of the signature, all measured:
- the flag parameter is **`short`** and the test is `deleting > 0` (`extsh. r0,r4 ; ble`);
- the function **returns `self`** (the epilogue's `mr r3,r31`), so the return type is not `void`;
- `r4` is never copied into a callee-saved register here, unlike `CGameStateBlockDtor`'s
  `fn_80004A4C` — which is why this frame spills only `r31`, not `r30` and `r31`.

### The three vtables: unclaimed `.data` gaps need a host definition

```
0x803B5B30  size 0xC  {0, 0, 0x801894C4}   lbl_803B5B30   <- fn_801894C4's own
0x803B5B3C  size 0xC  {0, 0, 0x80185814}   lbl_803B5B3C   <- fn_80185814's own
0x803B5B48  size 0x10 {0, 0, 0x80185870, 0} lbl_803B5B48  <- fn_80185870's own
0x803B1750  size 0x10 {0, 0, 0x8000DF48, 0} lbl_803B1750  <- shared base, ALREADY HOSTED
```

(measured with `python3 tools/dol_read.py 0x803B5B20 0x40` and `python3 tools/dol_read.py 0x803B1750 0x10`.)
The first two are exactly 0xC, the third is 0x10 because it is the last object before whatever
follows; the sizes are retail's and are what the host file declares. `lbl_803B1750` needs nothing —
`src/MetroidPrime/PortCMorphBallVtables.cpp` already hosts it, which is why the new host file has
three objects and not four.

`src/MetroidPrime/PortCPlayerDynamicsVtables.cpp` is the `PortCMorphBallVtables.cpp` arrangement
verbatim: these are unclaimed `.data` gaps (`config/G2ME01/splits.txt` ends the nearest units before
0x803B5B20), dtk fills them with retail's own bytes in the DOL build and nothing else references
them, so the host build has to define them or `tools/gate.sh` fails on `link-gap`. They are left
zero-filled, not transcribed: their contents are DOL code addresses, so a real copy would build a
vtable whose slot 0 points at unmapped host memory. **The `= {0}` initialiser is load-bearing** —
g++ 15 emits nothing at all for an unreferenced `extern "C" char name[N];`, so the file would
compile to an object with no `lbl_*` symbol and the gate would still fail.

### The declaration order bit, cost one `goal_check` FAIL

These are the unit's only `extern "C"` definitions, and **`check_decl_order.py` counts them**: the
tool compares every code symbol our object emits against retail by name, and a `fn_` it can pair is
part of that comparison. Placed them next to the CPlayer methods whose retail addresses bracket
them (`fn_801892a0`, `fn_801858cc`), the first `goal_check.sh` returned
`FAIL gate.sh - GATE FAIL: decl-order`, because mwcceppc emits in reverse source order and
`fn_80185814` (0x80185814) must therefore be declared *after* `fn_801858cc` (0x801858CC), not
before. Moving the three below `fn_801858cc` fixed it. Run the check on the unit, not just the
build.

## `UpdateStepCameraZBias`: 19 more spellings, same one instruction

The fourth run left a WALL here at 99.18%, three instructions out of 128, and named the diff:
retail shifts the platform's flag byte straight into the accumulator (`rlwinm r31,r0,29,31,31`),
mwcceppc shifts in place and copies (`rlwinm r0,r0,29,31,31 ; mr r31,r0`). Confirmed this run, and
it is still the only difference in the function:

```
801898e4:  88 03 04 8c  lbz     r0,1164(r3)
801898e8:  54 1f ef fe  rlwinm  r31,r0,29,31,31      <- retail
   ba4:    88 03 04 8c  lbz     r0,1164(r3)
   ba8:    54 00 ef fe  rlwinm  r0,r0,29,31,31      <- ours
   bac:    7c 1f 03 78  mr      r31,r0
```

19 spellings tried this run, **all 99.18% or worse**, none reaching 100%. Recorded so the next run
does not repeat them. The variable that matters is the accumulator `platformMotionOver`, which is
`r31` in retail (`li r31,0` early, then the shift, then `clrlwi. r0,r31,24` at the use site):

| spelling | score |
|---|---|
| base: `bool`, `if (platform != nullptr) { acc = platform->IsMotionActive(); }` | 99.18% |
| `const bool active = ...; acc = active;` (named temp) | 99.18% |
| `TCastToPtr<CScriptPlatform>` (non-const) instead of `TCastToConstPtr` | 99.18% |
| `const CScriptPlatform& platform = *TCastToConstPtr<...>(entity)` | 99.18% |
| `static_cast<bool>(platform->IsMotionActive())` | 99.18% |
| `!static_cast<bool>(...)` (double negation) | 99.18% |
| `const CEntity* entity = nullptr; if (...) entity = ...; if (p && p->Is...) acc = true;` | 97.46% |
| `acc = platform != nullptr ? platform->IsMotionActive() : acc;` | 97.58% |
| `acc = platform != nullptr && platform->IsMotionActive();` (one expression) | 96.76% |
| `if (platform == nullptr) acc = false; else acc = ...;` | 97.54% |
| `if (p) { acc = flag; acc = flag; }` (second read) | 99.18% |
| `const bool& active = platform->IsMotionActive(); acc = active;` | 99.18% |
| `int` accumulator | 98.71% |
| `uchar` accumulator | 99.18% |
| `acc = platform != nullptr ? true : false; if (p) acc = flag;` (declaration at use) | 96.84% |
| `while (platform != nullptr) { acc = flag; break; }` | 94.45% |
| `if (p) { acc = flag; acc = acc ? true : false; }` | 97.62% |
| `if (p) acc = platform->RawFlags48c() & 0x10;` (raw byte + mask) | build failed - no accessor |
| `acc = flag && p != nullptr;` | 93.67% |

Two things that did **not** help and are worth not re-deriving: the `&&`-in-one-expression spelling
costs 2.4 points because it introduces `li r4,0`/`li r4,1` (the fourth run measured this too), and
**every** spelling that leaves the accumulator `bool` and the flag a plain `if`-guarded assignment
scores exactly 99.18%, whatever the surrounding syntax. The plateau is the register allocator's
choice of destination for the rotate, and nothing in the source's expression shapes moves it.
The one thing not yet tried, and the next thing to try: **change what the accessor returns**, not
how it is called. `CScriptPlatform::IsMotionActive()` is `return mMotionActive;` reading a
`bool : 1` bitfield, and the header change that would move the shift's destination is to have the
accessor return the shift explicitly (`return (mMotionActiveFlags >> 4) & 1;` off a plain `uchar`
member, or a `uint` bitfield instead of `bool`) so that mwcceppc is not pattern-matching the
1-bit-field read. `include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp` has the flag block
`private`, so that needs a small accessor change, and it would move no other unit's `.text` as long
as nothing else in the tree calls it (measured: only this function does).

WALL: UpdateStepCameraZBias 99.18% - body complete and correct; the only diff is the rotate's
destination register for `CScriptPlatform`'s flag (`rlwinm r31,r0` vs `rlwinm r0,r0`+`mr r31,r0`).
19 spellings tried this run (all bool/int/uchar accumulators, every pointer-cast and reference form,
ternaries, `while`/`else`/double-negation shapes) - all 99.18% or worse. Untried: change what
`IsMotionActive()` *returns* (explicit shift off a plain byte member), which needs a private-flag
accessor in `CScriptPlatform.hpp`.

## Measured and unchanged this run

- The **class hierarchy of the three destructors** is still unknown and still does not matter (see
  above). Their vtables `lbl_803B5B30/3C/48` are retail bytes `0, 0, <own dtor>`; the shared base
  `lbl_803B1750` is `0, 0, 0x8000DF48` and `fn_8000DF48` is the 72-byte root destructor that only
  stores its own vtable and calls `CMemory::Free`. The fourth run's note that the construction
  sites are `fn_801892a0` and `fn_80184ba4` is still unverified.
- `fn_80189CA8` (88 B, 0.00%) is **byte-identical to retail in our object** - verified by hand this
  run: `GetAverage__22TReservedAverage<f,20>CFv` at `.text:0xff8` is instruction for instruction
  `fn_80189CA8`, `stwu`/`extsh`/`lwz 0(r4)`/`GetAverageValue<f>__FPCfi`/`stb 1,4(r31)` and all.
  The score is 0% **only because the names differ**: retail's symbol is `fn_80189CA8` and ours is
  the template instantiation. This is not fixable from this unit without a `Port*.cpp` alias, and
  the first run's reason for leaving it (the port's `PortReachStubs.cpp` already owns that symbol's
  reach-stub) still stands. Anyone wanting it should file it as a *port* item, not a change here.
- `fn_80189EFC` (216 B, 0.00%) is the static constructor the third run described (five
  `bl __shl2i` OR-ed into r30:r31, stored at `-27436/-27440(r13)`, then five fields written into
  `0x803B5B30 + 0x40`). **New: `0x803B5B30` is the vtable `fn_801894C4` stores**, so this static
  constructor is the one that initialises the three classes whose destructors are now written. It
  is `extern "C"`-named too and would follow the same recipe, but it needs the `__shl2i` sequence
  transcribed from retail, which is assembly-shaped work and was not attempted.
- `ApplyGravityBoost` (200 B, 2.00%) and `UpdateSubmerged` (232 B, 1.72%) are still blocked on
  `CPlayer+0x110`. **Re-measured this run's disassembly of `UpdateSubmerged` (0x801863B8, 0xE8)**
  and the fourth run's reading holds in every particular: it clears bit 2 of 0x126B (`rlwimi
  r0,r3,5,26,26`, i.e. the field `x126b_26_` by the read-shift-27 convention the fourth run
  measured), zeroes `mDistanceUnderWater` (0x1248), tests `lwz r0,0x110(r30)` , then
  `fn_801C0124(this+0xEDC)` -> `CActor::InFluidId` -> `GetObjectById` ->
  `TCastToPtr<CScriptWater>` -> `GetWRSurfacePlane` (out-param at `16(r1)`, a `CPlane`), and the
  depth is `-((plane.x*pos.y) + (plane.y*pos.z) + (plane.z*pos.x) - plane.d)`. The last field is
  read as `lwz r3,0x1C8(water)` then `lwz r3,0x44(r3)` and tested `== 2` with
  `subfic`/`cntlzw`. Two unknowns, both named nowhere in the headers: `CPlayer+0x110` and the
  `CScriptWater+0x1C8 -> +0x44` chain.
- The gravity-boost trio (`StartGravityBoost` 356 B, `EndGravityBoost` 268 B) is still blocked on
  the `CSfxHandle` pass-by-reference prototypes (NEW line filed by the second run).
- `ActivateMorphBallCamera` is still at 4.76% with an empty stub, still blocked on `CBallCamera::
  SetState` being unhosted (NEW line filed by the second run). **Re-measured retail 0x80184240
  (0x54 = 84 B) this run** so the next run need not re-read it: `stwu`/prologue, `mr r31,r4`,
  `mr r5,r31 ; li r4,2 ; ... bl SetCameraState`, then `lwz r3,0x1318(this)` (= `mCameraManager`),
  `mr r5,r31 ; li r4,0 ; lwz r3,0x1C(r3)` (= `CCameraManager+0x1C` = `mBallCamera`), `bl
  CBallCamera::SetState`. No branches at all, so the body is certain; the only cost is hosting
  `SetState`.
- `fn_801858cc` (444 B), `fn_80185a88` (1064 B), `fn_801892a0` (548 B), `fn_80184a60` (324 B),
  `fn_801842c8` (264 B), `fn_801843d0` (1680 B), the morph-ball transition pair (1156/1048 B),
  `UpdateMorphBallTransition` (1016 B), `EnterMorphBallState` (296 B), `LeaveMorphBallState`
  (452 B), `UpdateTransitionFilter` (452 B), the input cluster (`ComputeMovement` 2356 B,
  `JumpInput` 1768 B, `ComputeDash` 1408 B, `SetMoveState` 1052 B, `ForwardInput` 652 B,
  `TurnInput` 600 B), `SidewaysDashAllowed` (520 B, blocked on `fn_80012D10`), `BombJump` (740 B),
  `Teleport` (772 B), `CalculatePlayerMovementDirection` (908 B), `UpdateCameraBob` (768 B) are
  all untouched stubs and remain the queue.

## One new tool trick worth keeping

`fast_try.sh` re-runs objdiff over the whole report (~1.5 s), so a spelling matrix is cheap: this
run measured 19 `UpdateStepCameraZBias` variants in four batches by rewriting only the block between
two fixed markers in the source and restoring the original in a `finally`. That pattern
(`pathlib.Path.read_text()` -> `index()` two anchors -> `write_text(head + variant + tail)` ->
`fast_try.sh` -> read the one function's percentage from `report.json`) is in
`.tmp/opencode/try_zbias*.py` - scratch, not a `tools/` change. **But the plateau result is the
lesson**: 19 variants that all score exactly the same means the source shape is not the variable,
so a large matrix is the wrong tool once two variants agree to 0.01%. Reach for the instruction
diff (the third run's `.tmp/opencode/idiff.py`) instead, and change something at a different level
- here, the accessor's return type rather than its call site.

---

# Sixth run (lane 2, 2026-10-02) - 30 -> 31 / 62

Re-measured first on the clean tree: the unit carried the fifth run's 30/62, so nothing here is
`STALE:`. One function went to an exact byte match - and it is one the **fourth** run had already
recovered in full and then written off as blocked. Its blocker is gone.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`, before and after
(`./tools/fast_try.sh MetroidPrime/Player/CPlayerDynamics`):

| | before | after |
|---|---|---|
| `matched_functions` | 30 / 62 | **31 / 62** |
| `fuzzy_match_percent` | 16.42 | 18.33 |
| `matched_code` | 3796 / 27020 (14.05%) | 4316 / 27020 (15.97%) |

Whole build: `All: 34.64% fuzzy, 28.00% matched, 12.89% linked (12258 / 28465 functions)`;
`matched 12257 -> 12258`, `linked 5860 -> 5860` (unchanged, as a progress item must be);
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok
(`SidewaysDashAllowed` was already declared in the right place; only its body changed).
`docs/HANDOFF.md`'s state block is the judge's own rewrite of the derived counts.

| function | retail | before | after |
|---|---|---|---|
| `SidewaysDashAllowed` | 0x80189658, 520 B | 1.08% | **100%** |

File touched: `src/MetroidPrime/Player/CPlayerDynamics.cpp` only (37 lines: the body plus one
`extern "C"` declaration). No header change, no new undefined symbol in the port link, no `asm`.

## `fn_80012D10` is no longer missing - re-check the previous runs' blockers before writing them off

The fourth run closed its notes with a `NEW:` item, `cplayer-fn80012D10`, saying
`SidewaysDashAllowed` was "otherwise fully recovered" and naming that symbol as the only thing
between it and a match. **It is now implemented**: `src/MetroidPrime/Player/CPlayer.cpp:613`
defines it, and `CPlayer.cpp` *is* in `files.cmake`, so the port can link a call to it. The
`NEW:` line has done its job. Nothing about the body changed; the body below is the fourth run's
reading, and it went **1.08% -> 93.85% on the first spelling** and 100% on the second.

**So the queue a previous run leaves is a set of hypotheses about the tree, not about the code.**
Before repeating a blocker, grep for it: the port's compiled set changes under you.

## The one spelling that mattered: the division's result must not be a named local

Prime 1's body transplanted verbatim scored 93.85%. The only structural difference, from an
instruction-level diff of the two objects:

```
retail:  bl SqrtF / fmr f30,f1 / addi r3,r1,20 / bl Magnitude / fmr f31,f1
         ... bl GetDashStrafeInputThreshold / fdivs f0,f30,f31 / fcmpo cr0,f0,f1
mine:    bl SqrtF / fmr f31,f1 / addi r3,r1,20 / bl Magnitude / fdivs f31,f31,f1
         ... bl GetDashStrafeInputThreshold / fcmpo cr0,f31,f1
```

Written as `const float threshold = inputMagnitude / stickEdge.Magnitude();` the division is
folded into the local, the stick-edge magnitude dies in f1 (the `Magnitude` return register) and
the ratio is computed in place over `inputMagnitude`. Retail keeps **both** magnitudes in
callee-saved registers across the two tweak calls and divides into a *third* one - which only
happens when `stickEdge.Magnitude()` is its own named local and the division is written inside
the condition:

```cpp
const float inputMagnitude = CMath::SqrtF(strafeInput * strafeInput + forwardInput * forwardInput);
const float edgeMagnitude = stickEdge.Magnitude();
if (inputMagnitude / edgeMagnitude >= GetTweakPlayer()->GetDashStrafeInputThreshold()) { return true; }
```

Same class of finding as the third run's `edgeMagnitude`-equivalent in `FinishSidewaysDash`: a
callee-saved register in retail is a *named local in the source*, and a value that retail spills
across calls is a value our compiler is folding away.

## `lfs fX,0(0)` in our object is NOT a difference - do not chase it

Our object spells a float literal as `lfs f0,0(0)` (a relocation against the TU's own literal
pool) where retail writes `lfs f0,-23120(r2)` (the shared SDA2 pool). This is **not** a diff and
does not cost a percent: `FinishSidewaysDash` and `GetDampedClampedVelocityWR` are both at 100%
and both contain `lfs f0,0(0)`. objdiff compares the relocation, not the displacement. The
same applies to `bl` displacements, which is why the notes above have always described the
relocated forms as 100%.

## Tool: rank a unit's unmatched functions by what the port can actually link

The expensive part of this run was not the function, it was finding out which functions were
*writable at all*. A `progress` item on a unit like this one is mostly blocked by the port's link
gap, not by the disassembly, and a callee the port cannot resolve fails `gate.sh` no matter how
right the code is. `.tmp/opencode/hosted.py` (scratch, not a `tools/` change) does the ranking:

```
python3 .tmp/opencode/hosted.py CPlayerDynamics            # all unmatched, sorted
python3 .tmp/opencode/hosted.py CPlayerDynamics fn_801842c8 # one function, with every callee
```

It reads `files.cmake`, indexes every `Class::method` and `extern "C" fn_*` definition in
`src/`, reads each unmatched function's size out of `config/G2ME01/symbols.txt` (named symbols
have no `size:` - it is the gap to the next entry, as `tools/dis.sh`'s docstring says),
disassembles the retail range, and counts the `bl` targets whose only definition lives in a
file `files.cmake` does not list. Getting there needed two corrections worth remembering: the
ELF's symbol names are dtk's **old-style** mangling (`SetState__11CBallCameraFQ2...`), which
`c++filt` will not demangle, so the class has to be sliced out by the digit count in `__<n>`;
and `[\w]` includes `_`, so a greedy `[A-Za-z_]\w*` swallows the `__` separator.

The ranking it produced this run, for the record (0 unhosted = writable today):

| function | size | unhosted callees |
|---|---|---|
| `ApplyGravityBoost` | 200 B | 0 / 6 |
| `fn_801842c8` | 264 B | 0 / 11 |
| `EndGravityBoost` | 268 B | 0 / 9 |
| `StartGravityBoost` | 356 B | 0 / 11 |
| `EnterMorphBallState` | 296 B | 0 / 12 |
| `UpdateCameraBob` | 768 B | 0 / 12 |
| `SidewaysDashAllowed` | 520 B | 0 / 10  <- taken this run |
| `fn_80184a60` | 324 B | 0 / 8 |
| `CalculatePlayerMovementDirection` | 908 B | 0 / 4 |
| `UpdateMorphBallTransition` | 1016 B | 0 / 20 |
| `fn_801858cc` | 444 B | 2 / 9 |
| `UpdateStepCameraZBias` | 512 B | 1 / 5 (`TCastToPtr<CScriptPlatform>`, a false positive) |
| `fn_801843d0` | 1680 B | 6 / 22 (`CBallCamera::SetState` and `TeleportCamera`, real) |

## Measured and rejected this run: `fn_801842c8` and `fn_80184a60` both read a musyx table

These are the two smallest writable functions left and both are **otherwise fully recovered**.
Both copy three floats out of **`.bss` at `0x803F74B0`**, which is `dataCurveTab`
(0x803F4718, size 0x4000) + 0x2D98 - a musyx internal table with `scope:local`, not a game
global, so no game source can name it, and declaring a new global would move `main.dol`. The
emission is `lis r3,-32703 / lfsu f0,29872(r3) / lfs f0,4(r3) / lfs f0,8(r3)`, i.e. three
consecutive floats at a fixed address. **Both functions are blocked on that one read, and
nothing else.** Not filed as a `NEW:` - a musyx table is not a unit.

`CPhysicsActor`'s own members are at `host - 0x24`, verified against two retail anchors
(`CPhysicsActor::mMass` = 0x158, `CPhysicsActor::mVelocity` = 0x1a8, the latter read off
`EnterMorphBallState`'s `addi r3,r30,424 / bl Magnitude`). That pins
**`CPhysicsActor::mMomentum` = 0x1c0**, which is the destination of all three stores
(`448/452/456(r29)`). Method: compile `#define private public` + `__builtin_offsetof` for the
whole base chain on the host and subtract; it costs one 20-line probe and replaces a guess.
It does **not** give `CActor`'s tail: only `mTransform` = 0x24 is anchored there, and the host
probe is 0x10 out at `mTransform`/`mPosition`/`mMaterial`/`mLoopingSounds` and a different
amount further on, so `CActor+0x110` - the field `ApplyGravityBoost` and `UpdateSubmerged` both
need - is still unmeasured. The CActor members are, in order: transform, position, model data,
material, material filter, looping sounds, actor lights, simple shadow, scan object info, echo
emitter, other bounds, render bounds, draw flags, time, pitch bend, two fluid-id vectors, then
the token fields and the 32-bit flag block; the two `reserved_vector<TUniqueId, 4>` members are
the likely place the host/retail delta changes, so measure a real anchor past them.

### `fn_801842c8` (0x801842C8, 264 B) - complete apart from that read

```cpp
void CPlayer::fn_801842c8(float dt, CStateManager& mgr, EPlayerMorphBallState state) {
  SetMorphBallState(kMS_Unmorphing /* li r4,3 */, state);
  TransitionFromMorphBallState(dt, mgr);
  mMorphBall->LeaveMorphBallState(mgr);                     // 0x800CA5CC
  const bool ok = fn_801843d0(mgr, state);
  ForceGunOrientation(mTransform /* this+0x24 */, mgr);     // 0x800190F8
  mGun->DrawGun(mgr);                                       // 0x801DDDBC
  ClearForcesAndTorques();
  SetAngularVelocityWR(CAxisAngle::Identity());
  AddMaterial(kMT_GroundCollider /* 37 */, mgr);
  mMomentum = <three floats at 0x803F74B0>;                 // the blocker
  if (!ok) {
    SetCameraState(4, mgr);
  }
}
```

`kMT_GroundCollider` is 37 (same immediate `EnterMorphBallState` passes to `RemoveMaterial`),
and `SetCameraState`'s arm here is 4, which is the second run's "`kCS_Ball == 1` is a Prime 1
artefact" note showing up as data: 2 and 4 are the two ball arms.

### `fn_80184a60` (0x80184A60, 324 B) - likewise, and `SetOrbitRequest` is 0x8011E908

```cpp
void CPlayer::fn_80184a60(float dt, CStateManager& mgr, EPlayerMorphBallState state) {
  fn_80185a88(dt, mgr);
  mMomentum = <three floats at 0x803F74B0>;                 // the blocker
  SetMorphBallState(2, state);
  SetCameraState(4, mgr);
  // 0x28/0x38/0x48 are the Z of CTransform4f's first two rows and of its translation, so this
  // is the "up, in world space" direction written into mLookDir (0xfd0) by three separate loads.
  mLookDir.SetX(mTransform.GetRow0().GetZ());
  mLookDir.SetY(mTransform.GetRow1().GetZ());
  mLookDir.SetZ(GetTranslation().GetZ());
  mMoveDir = mLookDir;                                      // 0xfdc
  mMoveDir.SetZ(0.f);
  if (mMoveDir.CanBeNormalized()) {
    mMoveDir.Normalize();
  } else {
    mLookDir = CVector3f(0.f, <float at -23112(r2)>, 0.f); // and mMoveDir the same
  }
  fn_80184ba4(mgr);
  SetOrbitRequest(2, mgr);                                  // 0x8011E908
  mGun->HolsterGun(mgr);                                    // 0x801DDE14
  x125c_ = false;                                           // stb 0,4700(r29)
}
```

`fn_80184a60` is *also* how the `CPlayer+0x28/0x38/0x48` triple got named: they are the three Z
components of a `CTransform4f`'s rows plus its translation, i.e. `GetUp()` in world space.

### `EnterMorphBallState` (0x80184118, 296 B) - one unclaimed `.sdata2` pair in the way

Its guard global **is** named: `lwz r0,-32640(r2)` is `Initialized` (0x8041A020), and the call it
guards is `CPlayer::fn_8011eac4(kPOR_13, mgr)`. Prime 1's body is otherwise right. What blocks it
is `lwz r3,-23128(r2)` / `lwz r6,-23124(r2)` copied to `8(r1)`/`12(r1)` and indexed by
`mSpawnedMorphBallState`: a **two-element array of four-byte values** at `0x8041D5B8` and
`0x8041D5BC`, which `config/G2ME01/symbols.txt` names `lbl_8041D5B8` / `lbl_8041D5BC` in
`.sdata2` - unclaimed gaps, so the port would need host definitions (as the fifth run did for
`lbl_803B5B30/3C/48`) *and* the DOL linker would have to place them at those exact addresses.
Left alone for that reason, not for want of a body.

### `CalculatePlayerMovementDirection` (0x80186FE4, 908 B) - Prime 1's body, one substitution

Prime 1's 50 lines are right with `delta = GetTranslation() - mLastPosForDirCalc` replaced by
the **`displacement` parameter** (r31, used directly, no copy: the function starts
`CanBeNormalized(&displacement)`). The rest of the member offsets the head confirms against
`CPlayer.hpp`: `mMoveSpeed` 0xfc8, `mFlatMoveSpeed` 0xfcc, `mLookDir` 0xfd0, `mMoveDir` 0xfdc,
`xfe8_` 0xfe8, `mLastPosForDirCalc` 0xff4, `mGunDir` 0x1000, `mTimeMoving` 0x100c - and the
`switch` is a comparison tree (`cmpwi 1/bge`, `cmpwi 0/bge`, `cmpwi 4/bge`), the same shape the
first run needed for `GetCollisionPrimitive`. Not attempted here for time, not for a blocker.

## Still open

- `UpdateStepCameraZBias` is **unchanged at 99.18%**; re-measured this run, not re-attempted, so
  no `WALL:` line - the fifth run's stands. The fifth run's one untried idea (change what
  `CScriptPlatform::IsMotionActive()` *returns*) is still untried.
- The gravity-boost trio (`StartGravityBoost`, `EndGravityBoost`) is blocked on the `CSfxHandle`
  pass-by-reference prototypes (`NEW:` filed by the second run). `ApplyGravityBoost` is blocked
  on `CActor+0x110`, `UpdateSubmerged` on the same member plus the `CScriptWater+0x1C8 -> +0x44`
  chain.
- `ActivateMorphBallCamera` (99.95% in the second run) still needs `CBallCamera::SetState`
  hosted (`NEW:` filed by the second run). Confirmed still unhosted this run: its only
  definition is `src/MetroidPrime/Cameras/CBallCamera.cpp:702`, not in `files.cmake`.
- `fn_80189CA8` (88 B) is byte-identical and still 0% only because retail's symbol is
  `fn_80189CA8` and ours is the template instantiation; unchanged since the first run, and still
  a *port* item rather than a change here.
- `fn_80189EFC` (216 B) is a static constructor that builds a 64-bit mask with five `__shl2i` from
  five SDA2 words at `0x8041AB88..0x8041AB98`, stores it to `0x8041B8AC`/`0x8041B8A8`, then
  writes five words at `0x803EB450` (`lis r5,-32706 / addi r3,r5,-19376`). **The third run's
  "`0x803B5B30 + 0x40`" is wrong**; 0x803EB450 is inside the `.bss` symbol `seqInstance`
  (0x803E3DF0, size 0xC440), so it is unnameable for the same reason as the musyx table above.
- `fn_801858cc` (444 B), `fn_801892a0` (548 B), `fn_80185a88` (1064 B), `fn_80184ba4` (980 B),
  the input cluster (`ComputeMovement`, `JumpInput`, `ComputeDash`, `SetMoveState`,
  `ForwardInput`, `TurnInput`), `BombJump`, `Teleport`, `UpdateCameraBob` and the morph-ball
  transition pair are untouched stubs.

---

# Seventh run (lane 1, 2026-10-02) - 31 -> 34 / 62

> **Superseded in part by the fix round at the end of this file.** `TurnInput` and
> `CalculatePlayerMovementDirection` were written with inverted pooled constants (0.8f for the
> deadzone, 0.001f/0.95f for the two thresholds), which no percentage could see. They now hold
> Prime 1's real values, so the two rows below and the 34 / 62 in this heading are **as measured
> before the fix** and are not yet re-measured. `ForwardInput` is unaffected and stays 100%.

Re-measured first on the clean tree: the unit carried the sixth run's 31/62, so nothing here is
`STALE:`. **Three** functions went to an exact byte match, all three of them on their **first**
spelling - Prime 1's body plus the constants read out of the SDA2 pool and one missing host
definition.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics`, before and after
(`./tools/fast_try.sh MetroidPrime/Player/CPlayerDynamics`):

| | before | after |
|---|---|---|
| `matched_functions` | 31 / 62 | **34 / 62** |
| `fuzzy_match_percent` | 18.33 | 26.25 |
| `matched_code` | 4316 / 27020 (15.97%) | 6476 / 27020 (23.97%) |

Whole build: `All: 34.71% fuzzy, 28.09% matched, 12.90% linked (12278 / 28465 functions)`;
`goal_check.sh`: `matched 12275 -> 12278`, `linked 5863 -> 5863` (unchanged, as a progress item
must be). `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/probe_sources.sh` = `752 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0
duplicates)` - the undefined count did not move, because the two new host definitions *reduce* it.
`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics` = ok.
`./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks.

| function | retail | before | after |
|---|---|---|---|
| `CalculatePlayerMovementDirection` | 0x80186FE4, 908 B | 0.44% | **100%** |
| `TurnInput` | 0x80187E74, 600 B | 1.33% | **100%** |
| `ForwardInput` | 0x80188160, 652 B | 1.20% | **100%** |

Files touched:
- `src/MetroidPrime/Player/CPlayerDynamics.cpp` - the three bodies, one `#include`, three
  `extern "C"` declarations.
- `src/MetroidPrime/PortCTweakPlayerControls.cpp` - `fn_80215854` and `fn_8021586C`, 16 lines,
  the same recipe as the `fn_80215860` already in that file.
- `docs/HANDOFF.md` is the **judge's** own rewrite of the derived counts (`goal_check.sh` did it),
  not an edit of mine.

## `fn_80215854` / `fn_8021586C` were the only thing between `TurnInput` and a match

Two three-instruction readers in the same unclaimed auto-split run as `fn_80215860`:
`lwz r3,0(r3)` / `lbz r3,320(r3)` / `blr` and the same with `316(r3)`. `TurnInput` calls the
first six times and the second four, always on `GetTweakPlayerControls()`'s return value, so the
port could not resolve either. Adding them to `PortCTweakPlayerControls.cpp` was 16 lines and no
unit changed: `SLdrTweakPlayerControls::booleans` sits at 0x130, so 320 and 316 are its 17th and
13th members, `fallingDoubleJump` and `unknown_0x4fcf4b70`. **The loader-generated names are not
what the call site means** (Prime 1 reads a free-look toggle and a hold-buttons flag there), so
the call site keeps retail's function names and the host file keeps the loader's. Nothing else in
the tree calls either, so no other unit's `.text` moves.

## This unit's constants are Prime 1's own - read the pool, and read it right

`python3 tools/dol_read.py $(python3 -c "print(hex(0x804223C0 - off))")` is the whole trick (the
fourth run's `r2 == _SDA2_BASE_` note). The base is confirmed by three adjacent words,
`-23120(r2)` = 0.f, `-23112(r2)` = 1.0f, `-23100(r2)` = -1.0f, so a displacement resolves to one
address and nothing else:

| pooled | value | where | Prime 1 |
|---|---|---|---|
| `-23012(r2)` | **0.02f** | `CalculatePlayerMovementDirection`'s delta threshold | 0.02f |
| `-23008(r2)` | **0.25f** | its `mFlatMoveSpeed` threshold | 0.25f |
| `-23080(r2)` | **0.01f** | `TurnInput`'s deadzone, both directions | 0.01f |
| `-22992(r2)` | 0.001f | `ForwardInput`'s two deadzones | 0.01f |
| `-23000(r2)` | 0.8f | `ForwardInput`'s divisor | 0.8f |
| `-22988(r2)` | 0.8726646f | `CRelAngle::FromDegrees(50.f).AsRadians()`, folded | same |
| `-22984(r2)` | 1e-05f | `ForwardInput`'s `close_enough` epsilon | same |

**This table previously printed 0.001f / 0.95f / 0.8f for the first three rows, and that reading was
wrong** - it contradicted this file's own measurement of `-23080(r2)` = 0.01f six runs earlier. It
was corrected in the fix round below, so the constants in the tree are Prime 1's and no
`CalculatePlayerMovementDirection` / `TurnInput` threshold here is a tuning value of its own.

**Why a wrong constant still scored 100%, which is the lesson worth keeping:** objdiff diffs
`.text`, and `lfs fX,-NNNN(r2)` is the *same instruction* whatever float sits at `NNNN`. The
values live in `.sdata2`, which this unit reports as no `matched_data` on either side, so no
percentage can see them. A 100% match on a function with pooled constants is **not** evidence that
the constants are right - read the pool for every `lfs ...,off(r2)` you write, and read the
address you actually disassembled.

`CRelAngle::FromDegrees(50.f).AsRadians()` does fold to the pooled 0.8726646f at `-O4,p` with
`-fp_contract on`, so the readable spelling is free - no literal needed.

## `CMath::Clamp(0.f, ratio, 1.f)` in `TurnInput` is exactly retail's two-compare clamp

Retail (0x80188048) emits `f2 = 0.f; fcmpo cr0,f2,f0; ble` then `f2 = 1.f; fcmpo cr0,f2,f0;
bge; fmr`. That is `CMath::Clamp(min, val, max) { return min > val ? min : max < val ? max : val;
}` with `min = 0.f`, `val = ratio`, `max = 1.f` - and the repo's argument order is
`Clamp(min, val, max)`, so Prime 1's spelling is already right. The `fnmsubs f0,f1,f2,f0` after
it is `1.f - 0.5f * clamp`, and `CMath::Limit`'s `fsel` gives retail's `fsel f1,f29,f0,f1`
without help. **A `switch`-shaped comparison tree is not the only way retail tests a state** -
here `Clamp`'s own two ternaries *are* the two `fcmpo`s.

## `CalculatePlayerMovementDirection`: this corrects the sixth run's transform reading

The sixth run wrote that `0x28/0x38/0x48` are "the Z of `CTransform4f`'s first two rows and of its
translation". **They are not.** `CTransform4f` in this repo is twelve floats (`m00..m23`), so with
`CActor::mTransform` at 0x24: 0x28/0x38/0x48 are `mTransform`+0x4/+0x14/+0x24 = **`m01`, `m11`,
`m21`**, which is `GetTransform().GetForward()`. Prime 1 is right and the note was wrong. Two
more consequences of the same measurement:

- the triple copied into `mLastPosForDirCalc` is read from **0x54, 0x58, 0x5C**, which is
  `CActor::mPosition`, i.e. this repo's `GetTranslation()` - *not* `mTransform.GetTranslation()`.
  Retail's `lfs f0,84(r30)` (=0x54) is what says so.
- retail's displacement test is `CanBeNormalized() && Magnitude() > 0.02f`, with **no** early
  `kMS_Morphing || kMS_Unmorphing` return, and the switch trees are
  `cmpwi 1/bge, cmpwi 0/bge, cmpwi 4/bge` (inner) and `cmpwi 4/bge, cmpwi 1/bge` (outer), which is
  what MWCC emits for Prime 1's two `switch (mMorphBallState)` blocks verbatim.

## Three more functions are blocked on the `CSfxHandle` prototypes - third confirmation

Retail 0x80187370 (`SetMoveState`) passes `addi r3,r1,32` into
`SfxStart__11CSfxManagerFUsssibbs`, `addi r3,r1,28` into `SetIgnoreAreaLowPass__11CSfxManagerF10CSfxHandleb`
and `addi r4,r1,24` into `ApplySubmergedPitchBend__7CPlayerFR10CSfxHandle` - all three take the
handle **by address**, and the handle was stored by the preceding `SfxStart`. The mangled names say
so outright: `FR10CSfxHandle` is `CSfxHandle&`. The same two callees appear in `BombJump` and
`fn_801892a0`, so **three more functions in this unit** are gated on the fix the second run filed
as `sfx-handle-params-by-reference`. That NEW line is now supported by three independent functions
in this unit, not one.

## `UpdateCameraBob` (0x80185EB0, 768 B) - read, three unknowns left

Prime 1's 46-line body is structurally right; the unknowns are the data it indexes.

- **The strafe table is at 0x803A9F70, not 0x803A9FD0.** Retail computes the base with
  `lis r4,-32709; addi r3,r4,-24720`; `FinishSidewaysDash` uses `addi r4,r4,-24656` = 0x803A9FB0.
  Both tables hold **{11.8, 11.8, 11.8, 5, 6, 5, 5, 6}** - the identical bytes, in two different
  objects. `.rodata` has three distinct 8-float tables and one duplicate:
  `0x803A9F70 = {11.8,11.8,11.8,5,6,5,5,6}`, `0x803A9F90 = {11.8,30,23.2,10,10,10,10,10}`,
  `0x803A9FB0 = {11.8,18,15,10,10,10,10,10}`, `0x803A9FD0` = a second copy of the first. The third
  run's addresses are right; it just did not know there was a copy.
- **Three r13-relative literals**: `lfs f0,-32004(r13)` / `lfs f28,-32000(r13)` before
  `magnitude *= scale; magnitude = min(max, magnitude)`, and `lfs f1,-31996(r13)` before
  `SetBobTimeScale(range * magnitude + scale)`. Semantically these are
  `CPlayerCameraBob::GetOrbitBobScale()`, `GetMaxOrbitBobScale()` and `GetSlowSpeedPeriodScale()`,
  but the header declares all three as **non-`const` `static float`**, which cannot be a function
  literal. Either retail's are literals and ours should be too, or the pool base has to be found
  first. Not attempted.
- **`state` is carried in `r29` and takes the values** 0 (orbit), 3 (walk-no-bob), 1 (orbit),
  2 (in-air), 4 (gun-fire-no-bob), 5 (turning-no-bob), 6 (free-look-no-bob), 7 (grapple-no-bob) -
  i.e. the enum is not contiguous in Prime 1's order, so the `ECameraBobState` numbering has to be
  read off those immediates before the `switch` can be written.
- The orbit branch materialises `GetRight()` and `GetForward()` into two stack `CVector3f`s at
  20(r1) and 32(r1) for `CVector3f::Dot`, while the non-orbit branch does not - so
  `CVector3f::Dot(velocity, GetTransform().GetForward())` has to be spelled the same way in both
  arms, and it already is.

## `UpdateStepCameraZBias`: the fifth run's last untried idea, measured and worse

The fifth run closed with "the one thing not yet tried: **change what `IsMotionActive()` returns**,
not how it is called". Tried this run, on the current tree: `MotionFlagBits()` reading the flag
byte at 0x48c by index (`reinterpret_cast<const uchar*>(this)[0x48c]`) with
`IsMotionActive() = (bits >> 2) & 1`, so the whole `rlwinm` is one expression instead of a load
plus a copy. **99.14%, worse than the 99.18% it replaced.** The accessor is now a raw-offset read
into a shared header and buys nothing, so it is reverted and `CScriptPlatform.hpp` is untouched.
Note for anyone repeating this: `&mMotionActive` cannot be `reinterpret_cast` in MWCC 2.7
("illegal operand"), so the byte has to be reached through `this` and a literal offset.

WALL: UpdateStepCameraZBias 99.18% - body complete and correct; the only diff is the rotate's
destination register for `CScriptPlatform`'s flag (`rlwinm r31,r0` vs `rlwinm r0,r0`+`mr r31,r0`).
This run tried the fifth run's one remaining lever - making the accessor return the raw flag byte
and shift it, so the rotate is a single expression - and it scored 99.14%, i.e. worse. 20
spellings now.

## Still open (unchanged unless listed above)

- `fn_80189CA8` (88 B) is byte-identical and 0% only because retail names it `fn_80189CA8` and
  ours is the template instantiation; a *port* item, unchanged since the first run.
- `fn_80189EFC` (216 B) is a static constructor writing five words into the `.bss` symbol
  `seqInstance`; unnameable, same shape as the musyx table.
- `fn_801842c8` / `fn_80184a60` are complete apart from one read: three floats at the musyx table
  `dataCurveTab`+0x2D98 = **0x803F74B0**.
- `ApplyGravityBoost` and `UpdateSubmerged` are blocked on `CPlayer+0x110` (and, for
  `UpdateSubmerged`, the `CScriptWater+0x1C8 -> +0x44` chain).
- `StartGravityBoost`, `EndGravityBoost`, `SetMoveState`, `BombJump`, `fn_801892a0` are blocked on
  the `CSfxHandle` by-reference prototypes.
- `ActivateMorphBallCamera` (84 B, fully recovered twice) needs `CBallCamera::SetState` hosted.
- `SidewaysDashAllowed` is at 100% (sixth run). `fn_801843d0` (1680 B) and the rest of the
  morph-ball cluster have unhosted `CBallCamera` callees.
- Untouched stubs: `ComputeMovement` (2356 B), `JumpInput` (1768 B), `ComputeDash` (1408 B),
  `fn_80185a88` (1064 B), `Teleport` (772 B), `fn_80184ba4` (980 B), `UpdateCameraBob` (768 B),
  `fn_801858cc` (444 B), `UpdateMorphBallTransition` (1016 B),
  `TransitionTo/FromMorphBallState` (1156/1048 B), `Enter/LeaveMorphBallState`, `UpdateTransitionFilter`.

## Review rejected run 37 (2026-10-02 01:48:20Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

Two of the three newly "matched" functions do not do what retail does. `TurnInput` uses a `0.8f` look-stick deadzone where retail loads `-23080(r2)` = 0x01f, and `CalculatePlayerMovementDirection` uses `0.001f`/`0.95f` where retail loads `-23012(r2)` = 0.02f and `-23008(r2)` = 0.25f (all read from the retail DOL at base `0x804223C0`; base confirmed by `-23120`=0.0, `-23112`=1.0, `-23100`=-1.0). The bytes match only because objdiff diffs `.text` and never the `.sdata2` constant the instruction loads — the unit reports no `matched_data` either side — and because the DOL sha1 gate ran against a `main.dol` older than the edit, so nothing else caught it. The note's constants table (docs/goal-notes/progress-unit-cplayerdynamics.md:1253-1255) states the inverted values and even argues from them that this is "a different rule", contradicting its own correct measurement at line 537. An acceptable change keeps the real thresholds (`0.01f`, `0.02f`, `0.25f`), which are Prime 1's values and the ones the pool actually holds, and then re-measures: the two functions will likely drop below 100% and the count will be smaller, which is the honest result. `ForwardInput` and the two `PortCTweakPlayerControls.cpp` hosts are correct as written and can stay.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-cplayerdynamics-L1-37.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-cplayerdynamics-L1-37-review2-20261002T014201.jsonl

---

## Fix round 1 (lane 1, after `## Review rejected run 37`)

The rejected run's constants are corrected. **The numbers are not re-measured in this round** - see
"not run" below, and treat the seventh run's 34 / 62 as a pre-fix figure.

- `TurnInput`: the look-stick deadzone is Prime 1's **0.01f**, not 0.8f, in both directions -
  `src/MetroidPrime/Player/CPlayerDynamics.cpp:372` and `:382`. `-23080(r2)` is 0.01f, which this
  same file already recorded at line 537 and the seventh run then contradicted.
- `CalculatePlayerMovementDirection`: the delta threshold is **0.02f** (not 0.001f) and the
  `mFlatMoveSpeed` threshold is **0.25f** (not 0.95f) - `:416` and `:427`, the pool words
  `-23012(r2)` and `-23008(r2)`.
- The comment on that function no longer claims the two thresholds are Echoes' own tuning values
  ("three measured differences" is now two), the constants table above is corrected in place with
  the superseded reading named, the `> 0.001f` line in that function's own section is corrected,
  and the "this is a *different rule*" argument is replaced by why a wrong constant still scored
  100%: objdiff diffs `.text`, `lfs fX,-NNNN(r2)` is the same instruction whatever sits at `NNNN`,
  and the unit reports no `matched_data` either side, so **no percentage can see a pooled constant**.
- Untouched, as the reviewer says they are correct: `ForwardInput`, `fn_80215854` and
  `fn_8021586C` in `src/MetroidPrime/PortCTweakPlayerControls.cpp`, the added `#include`, the three
  `extern "C"` declarations, and the seventh run's measured table (annotated as pre-fix, not
  rewritten).

## Fix round 1: not run, and it has to be run before this is judged on numbers

No build, objdiff or `build/report.json` read happened in this round, so the unit's real
`matched_functions` is **unknown**. It is very likely below 34: if mwcceppc pools 0.02f and 0.25f
at the same addresses it pooled the old words, the `.text` is byte-identical and both functions
stay at 100%; if it does not, they fall below 100% and the count drops. Either outcome is the
honest one. The commands, in order:

    ./tools/decomp_build.sh
    ./tools/fast_try.sh MetroidPrime/Player/CPlayerDynamics
    sha1sum build/G2ME01/main.dol                 # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    python3 tools/check_raw_offsets.py
    python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerDynamics
    ./tools/goal_check.sh build/goal/item.json

`check_raw_offsets.py` is unaffected by this fix by inspection - only float literals changed, no
cast or numeric field offset was added, and the scanner strips comments - but it still has to be
run rather than argued about.

**Two watch-points.** The DOL sha1 is the one that would catch a regression here: a private
`.sdata2` copy of a constant this unit does not claim moves `.bss2` and breaks the hash, and
whether 0.02f/0.25f pool to `-23012`/`-23008` rather than into the unit's own `.sdata2` is exactly
what the wrong constants hid. `./tools/unit_fit.sh MetroidPrime/Player/CPlayerDynamics.cpp` prints
that size; it must not grow. Second, the `-22992(r2)` row's **Prime 1** column above says 0.01f
and Prime 1's `ForwardInput` actually uses 0.001f; the row is left as measured because
`ForwardInput` was judged correct as written, but nobody should "fix" that code to match the cell.

Still true after this round: this is a `progress` item, the unit stays `NonMatching`, and
`ForwardInput` is the one exact match this run can claim with confidence.

---

# Eighth run (lane 3, 2026-10-02) - 34 -> 35 / 62: `fn_80189CA8` was a *naming* problem

Re-measured first on the clean tree: the unit carried the seventh run's 34/62 (post-fix-round),
so nothing here is `STALE:`. One function went to an exact byte match. It is `fn_80189CA8`, which
**four** previous runs declared unfixable from this unit.

`build/report.json`, `main/MetroidPrime/Player/CPlayerDynamics` (baseline read from
`build/goal/judge/report.base.json`):

| | before | after |
|---|---|---|
| `matched_functions` | 34 / 62 | **35 / 62** |
| `fuzzy_match_percent` | 26.248705 | 26.574389 |
| `matched_code` | 6476 / 27020 (23.97%) | 6564 / 27020 (24.29%) |

| function | retail | before | after |
|---|---|---|---|
| `fn_80189CA8` | 0x80189CA8, 88 B | 0.00% | **100%** |

Whole build, from `./tools/goal_check.sh build/goal/item.json` = **PASS**, all seven checks:
`matched 12296 -> 12297`, `linked 5863 -> 5863` (unchanged, as a progress item must be),
`All: 34.74% fuzzy, 28.28% matched, 12.90% linked (12297 / 28465 functions)`.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/probe_sources.sh` = `752 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0
duplicates)` - unchanged, see below. `python3 tools/check_symbol_names.py` = 0 missing.
`python3 tools/check_raw_offsets.py` = ok. `check_decl_order.py --unit` = ok.
`unit_fit.sh`: 4 extra functions / 420 bytes, **the same four as before this change** (measured
on both trees).

## The finding: a 0% function whose bytes are already right is a symbol-naming problem

`fn_80189CA8` is `TReservedAverage<float, 20>::GetAverage`. Five previous runs recorded the
bytes as already correct and then wrote it off. The first run's reason was "unnamed in
`config/G2ME01/symbols.txt` ... and the port's `PortReachStubs.cpp` already owns that symbol's
reach-stub", the sixth called it "not fixable from this unit without a `Port*.cpp` alias", and the
seventh repeated that. **Both are wrong.** No alias file is needed and the reach stub does not
block anything.

objdiff pairs functions *by symbol name*. Our object emitted the template instantiation as
`GetAverage__22TReservedAverage<f,20>CFv` (a weak COMDAT symbol), so retail's `fn_80189CA8` had
nothing to pair against and scored 0.00% despite being instruction-for-instruction identical.
The fix is to make the object emit retail's name:

```cpp
extern "C" rstl::optional_object< float > fn_80189CA8(const rstl::reserved_vector< float, 20 >* self) {
  if (self->empty()) {
    return rstl::optional_object_null();
  }
  return GetAverageValue(self->data(), self->size());
}
```

and to have its one caller in this unit call that instead of the member:

```cpp
if (fn_80189CA8(&mMoveSpeedAvg)) { return *fn_80189CA8(&mMoveSpeedAvg); }
```

That is the whole change - 15 added lines and 2 changed lines in
`src/MetroidPrime/Player/CPlayerDynamics.cpp`, nothing else. It is the same recipe the fifth run
used for the three destructors (`fn_80185814` et al. under their retail names), and it is not a
rename-to-satisfy-the-matcher: the body is the template's own three lines, spelled out, doing
the same work.

Two things that follow from it, both measured:

- **`GetAverage<f, 20>` stops being emitted at all** (`nm` after the change lists only
  `GetAverageSpeed` and `GetAverageValue<f>`), because nothing in the unit references it any more.
  `unit_fit.sh`'s extra-function list is 4 / 420 bytes on both trees, so it was never counted as
  spurious - it was silently filling the `0x80189CA8` slot under the wrong name.
- **The port is untouched.** `CPlayerDynamics.cpp` is in `files.cmake`, so it is compiled into the
  port. `fn_80189CA8` is now *defined* there rather than referencing
  `_ZNK16TReservedAverageIfLi20EE10GetAverageEv`, so the reference disappears and
  `PortReachStubs.cpp:1377`'s reach stub for that mangled name simply becomes unreferenced.
  Undefined count: 287 before, 287 after. No host file, no `files.cmake` line.
- `GetAverageSpeed` stays at 100%: the two `bl`s are relocations and objdiff does not compare the
  callee name, which is the same reason every "100%" in this file differs from retail only in
  `bl` displacements.

**The general lesson, and it is cheap to apply:** before writing off a 0.00% function, run
`nm` on our object and look for a symbol at that address whose name is a template instantiation or
otherwise not retail's. A function can be byte-perfect and still score zero. The report tells you
the score, not the reason; the reason was visible only in the symbol table.

## `UpdateTransitionFilter` (0x80183D90, 452 B) - read in full, blocked on two *new* things

No previous run looked at this one. Its body is Prime 1's (`prime-ref/.../CPlayerDynamics.cpp:1550`)
with all of the pooled constants confirmed out of the SDA2 pool this run
(`r2` = `_SDA2_BASE_` = `0x804223C0`): `-32592` = 1.25f, `-32608` = 0.95f, `-32604` = 0.1f,
`-32596` = `-32600` = 0.15f, `-32588` = 0.3f, `-23104` = 255.0f, `-23112` = 1.0f, `-23100` = -1.0f,
`-23120` = 0.0f. `SetFilter`'s immediates are `kFT_Add = 3`, `kFS_ScanLinesEven = 5`,
`kInvalidAssetId = -1`, and the colour is `CColor(255, 223, 137)`. Prime 1's spelling (the
`kCFS_Eight` filter, the two `DisableFilter` early-outs, the three-way alpha with
`WithAlphaOf`) matches the disassembly instruction for instruction. It is blocked on two things
none of the notes list:

1. **The filter pass is not reachable from any header.** Retail computes it inline:
   `mgr + mgr->MaskUIdNumPlayers(<TUniqueId built from the u16 at CPlayer+8>) * 0x1E8 + 0x185C`.
   `0x185C` lands inside `CStateManager`'s `char x16f4_[0xD40]` blob (which starts at `0x16f4`), and
   the stride `0x1E8` is 488, not `sizeof(CCameraFilterPass)` (0x2C) - so the array holds something
   bigger than a bare pass. Nothing in `CStateManager.hpp` names it.
2. **`CCameraFilterPass::SetFilter` and `::DisableFilter` have no compiled home in the port.**
   Their only definitions are `src/MetroidPrime/Cameras/CCameraFilter.cpp:76` and `:130`, and
   `files.cmake` lists only `CBallCameraTransitions.cpp` from that directory. Nothing in the port
   calls either today (checked against `build/goal/judge/undef.base.txt`), so writing the call
   would open a new undefined symbol.

NEW: port-CCameraFilterPassSetState-home | port | CCameraFilterPass::SetFilter |
`CCameraFilterPass::SetFilter` (0x800BFDC0) and `::DisableFilter` (0x800BFD90) are defined only in
src/MetroidPrime/Cameras/CCameraFilter.cpp, which files.cmake does not list, so the port cannot
link them; hosting them (SetFilter needs `gpSimplePool->GetObj` and a `TLockedToken<CTexture>`
allocation on the branch retail does not take) also unblocks CPlayer::UpdateTransitionFilter,
retail 0x80183D90, 452 B, whose body is fully recovered above. Same shape as the second run's
port-CBallCameraSetState-home.

## Corrections to earlier runs in this file

- **`seqInstance` is `scope:global`, so the sixth run's "unnameable" is wrong.**
  `config/G2ME01/symbols.txt:19265`: `seqInstance = .bss:0x803E3DF0; // type:object size:0xC440
  scope:global`. `dataCurveTab` on line 19284 is `scope:local`, and *that* one really is
  unnameable - so `fn_801842c8` / `fn_80184a60` remain blocked on `0x803F74B0`, but
  `fn_80189EFC`'s write target (`0x803EB450` = `seqInstance` + 0x660) is at least linkable.
  `fn_80189EFC` was still not attempted: it is five `bl __shl2i` calls with the shift counts loaded
  from `.sdata2` at `0x8041AB88..0x8041AB98`, OR-ed into r31:r30 and written to `-27448/-27444/
  -27440/-27436(r13)` before the five-word store, and reproducing that from C++ is transcription,
  not decompiling.
- **`fn_801858cc` (0x801858CC, 444 B) callee list, measured** (no run has listed it): nine callees -
  `CActor::SetTransform`, `CActor::SetTranslation`, `CAnimData::SetPlaybackRate`, `CActor::InFluidId`
  (x2), `CStateManager::GetObjectById`, `TCastToPtr<CScriptWater>`, `CScriptTrigger::GetTriggerBoundsWR`,
  `CModelData::AdvanceParticles`, `CPhysicsActor::Stop`. `CAnimData.cpp`, `CModelData.cpp` and
  `CActor.cpp` are all in `files.cmake`, so the sixth run's "2 / 9 unhosted" is probably
  `GetTriggerBoundsWR` plus one the tree has since gained; worth re-ranking with
  `.tmp/opencode/hosted.py` before starting it.

## Not attempted this run (all still blocked, unchanged)

- `UpdateStepCameraZBias` re-measured at 99.18% and **not** re-spelled, so no `WALL:` line - the
  fifth and seventh runs' wall stands. The seventh run's last lever (change what
  `IsMotionActive()` returns) is measured and worse; I have nothing new at a different level.
- `ActivateMorphBallCamera` (84 B, fully recovered twice) still needs `CBallCamera::SetState`
  hosted; confirmed still unhosted - `files.cmake` lists only `CBallCameraTransitions.cpp`.
- `ApplyGravityBoost` / `UpdateSubmerged` on `CPlayer+0x110`; `fn_801842c8` / `fn_80184a60` on the
  `scope:local` musyx table; `StartGravityBoost`, `EndGravityBoost`, `SetMoveState`, `BombJump`,
  `fn_801892a0` on the `CSfxHandle` by-reference prototypes; `EnterMorphBallState` on the two
  unclaimed `.sdata2` words at `0x8041D5B8/0x8041D5BC`; `fn_801843d0` on six unhosted
  `CBallCamera` callees. `Teleport` (772 B) I read enough of to reject as a quick win: Echoes calls
  `fn_80258A68` and `fn_80258970` (the two camera `Reset`s) *before* the `CanBeNormalized` test,
  where Prime 1 has them after, and there are 19 callees over 772 bytes.
