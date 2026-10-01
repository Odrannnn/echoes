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