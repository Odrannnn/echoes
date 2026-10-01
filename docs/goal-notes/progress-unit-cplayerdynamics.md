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
