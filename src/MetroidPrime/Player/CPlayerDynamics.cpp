#include "MetroidPrime/Player/CPlayer.hpp"

#include "Collision/CCollidableSphere.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CFluidPlaneCPU.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerCameraBob.hpp"
#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Math/CVector3f.hpp"

#include <float.h>

// Retail's two unnamed control-tweak flag readers, hosted in
// `src/MetroidPrime/PortCTweakPlayerControls.cpp` next to `fn_80215860`.
extern "C" bool fn_80215854(const CTweakPlayerControls* self);
extern "C" bool fn_80215860(const CTweakPlayerControls* self);
extern "C" bool fn_8021586C(const CTweakPlayerControls* self);

// NonMatching scaffold. Definitions are in reverse target order for deferred inlining.

CVector3f CPlayer::GetDampedClampedVelocityWR() const {
  // Retail 0x80189D00 is not Prime 1's version: the friction is scaled by the acceleration and,
  // on kSR_Air, by the planar speed / mass, and the sign-preserving clamp at the end uses
  // `maxSpeed / accel` as its bound (fdivs into f3) rather than `maxSpeed` itself.
  const float accel = GetAcceleration();
  CVector3f localVelocity = GetTransform().TransposeRotate(GetVelocityWR());
  if (mOrbitState == kOS_NoOrbit) {
    float friction = GetTweakPlayer()->GetPlayerTranslationFriction(GetSurfaceRestraint());
    if (GetSurfaceRestraint() == kSR_Air) {
      // The kSR_Air case replaces the friction outright: 3.f, times the planar speed divided
      // by the mass (lfs f30,0x158 - `GetMass`). Retail loads the 3.f into the friction register
      // *before* the two calls and keeps it there across them, so it is a separate assignment
      // rather than the left operand of one product.
      friction = 3.f;
      const CVector2f planar(localVelocity.GetX(), localVelocity.GetY());
      friction = friction * (planar.Magnitude() / GetMass());
    }
    friction *= accel;
    // `CMath::Max`/`CMath::Min` return `const T&`, so they compile to an out-of-line call and a
    // reload through the returned pointer, and a ternary would test the other way round.
    // Retail's shape is a statement: compute, then conditionally overwrite with 0.f.
    if (localVelocity.GetY() > 0.f) {
      const float v = localVelocity.GetY() - friction;
      float r;
      if (v < 0.f) {
        r = 0.f;
      } else {
        r = v;
      }
      localVelocity.SetY(r);
    } else {
      const float v = localVelocity.GetY() + friction;
      float r;
      if (0.f < v) {
        r = 0.f;
      } else {
        r = v;
      }
      localVelocity.SetY(r);
    }
    if (localVelocity.GetX() > 0.f) {
      const float v = localVelocity.GetX() - friction;
      float r;
      if (v < 0.f) {
        r = 0.f;
      } else {
        r = v;
      }
      localVelocity.SetX(r);
    } else {
      const float v = localVelocity.GetX() + friction;
      float r;
      if (0.f < v) {
        r = 0.f;
      } else {
        r = v;
      }
      localVelocity.SetX(r);
    }
  }
  const float maxSpeed = GetTweakPlayer()->GetPlayerTranslationMaxSpeed(GetSurfaceRestraint());
  localVelocity.SetY(CMath::Limit(localVelocity.GetY(), maxSpeed / accel));
  if (mMovementState == NPlayer::kMS_OnGround) {
    localVelocity.SetZ(0.f);
  }
  return GetTransform().Rotate(localVelocity);
}

// Retail 0x80189CA8 is `TReservedAverage<float, 20>::GetAverage` - the body is the template's own
// three lines and it lands byte-for-byte in this object - but `config/G2ME01/symbols.txt` has no
// name for it, so retail calls it `fn_80189CA8` while the template instantiation is emitted as
// `GetAverage__22TReservedAverage<f,20>CFv` and objdiff has nothing to pair it against. It is
// written here under retail's own name so the pair exists; `CPlayer::GetAverageSpeed`, below, is
// its only caller in this unit and calls it instead of the member, which is what keeps the
// template instantiation out of the object (an unreferenced instantiation would be an extra
// function the retail object does not define).
extern "C" rstl::optional_object< float > fn_80189CA8(const rstl::reserved_vector< float, 20 >* self) {
  if (self->empty()) {
    return rstl::optional_object_null();
  }
  return GetAverageValue(self->data(), self->size());
}

float CPlayer::GetAverageSpeed() const {
  // Retail 0x80189C50 calls `TReservedAverage::GetAverage` twice: once to test the flag and
  // again to read the value, so the test and the read must be two separate calls.
  if (fn_80189CA8(&mMoveSpeedAvg)) {
    return *fn_80189CA8(&mMoveSpeedAvg);
  }
  return mMoveSpeed;
}

float CPlayer::GetAcceleration() const {
  // Retail 0x80189C1C branches with `blt` into the in-range read, so the comparison is written
  // the other way round from `>=`: `>=` would need `cmplw` to test the SO bit.
  if (static_cast< int >(mCurAcceleration) >= mAccelerationTable.size()) {
    return mAccelerationTable.back();
  } else {
    return mAccelerationTable[mCurAcceleration];
  }
}

float CPlayer::GetGravity() const {
  // Retail 0x80189B38 splits on one bit of the byte at 0x126B: set, it asks only for
  // kIT_LightSuit; clear, it asks for kIT_GravityBoost *and* CheckSubmerged(). Both power-up
  // tests are the same shape - the `rstl::rc_ptr` temporary out-param is released before the
  // result is tested. The branch is on bit 4 of the byte at 0x126B, which is `mInLava`:
  // measured by compiling this body against all eight of that byte's 1-bit fields, and only that
  // field emits retail's `rlwinm. r0,r0,27,31,31`. `UpdateSubmerged` (0x801863B8) is what named
  // it - the same bit, written from `CFluidPlane::GetFluidType() == 2`.
  if (mInLava) {
    if (!gpGameState->GetPlayerState()->HasPowerUp(CPlayerState::kIT_LightSuit)) {
      return GetTweakPlayer()->GetFluidGravAccel();
    }
  } else {
    if (!gpGameState->GetPlayerState()->HasPowerUp(CPlayerState::kIT_GravityBoost) &&
        CheckSubmerged()) {
      return GetTweakPlayer()->GetFluidGravAccel();
    }
  }
  if (mSidewaysDashing) {
    return -100.f;
  }
  return GetTweakPlayer()->GetNormalGravAccel();
}

float CPlayer::GetWeight() const { return GetMass() * -GetGravity(); }

void CPlayer::UpdateBombJumpStuff() {
  if (mBombJumpCount == 0) {
    return;
  }
  if (--mBombJumpCheckDelayFrames > 0) {
    return;
  }
  CVector3f flatVelocity = GetVelocityWR();
  flatVelocity.SetZ(0.f);
  if (mMovementState == NPlayer::kMS_OnGround ||
      (flatVelocity.CanBeNormalized() && flatVelocity.Magnitude() > 6.f)) {
    mBombJumpCount = 0;
  }
}

void CPlayer::UpdateStepCameraZBias(float dt, CStateManager& mgr) {
  // Retail 0x80189860: Prime 1's body plus a `CStateManager&`, because Echoes also zeroes the
  // bias while the riding platform's motion is active. `x1269_27_` is the one flag both read
  // (`rlwinm.,28` up front) and cleared (`rlwimi ...,27,27`) here, i.e. Prime 1's
  // `mStepCameraZBiasDirty`; the header has no name for it.
  float newBias = GetUnbiasedEyeHeight();
  const float groundZ = GetTranslation().GetZ();
  newBias = groundZ + newBias;
  bool platformMotionOver = false;
  if (mRidingPlatform != kInvalidUniqueId) {
    const CEntity* entity = mgr.GetObjectById(mRidingPlatform);
    const CScriptPlatform* platform = TCastToConstPtr< CScriptPlatform >(entity);
    if (platform != nullptr) {
      platformMotionOver = platform->IsMotionActive();
    }
  }
  if (mMovementState == NPlayer::kMS_OnGround && !IsMorphBallTransitioning() &&
      !platformMotionOver) {
    const float oldBias = newBias;
    if (!x1269_27_) {
      const float delta = newBias - mStepCameraZBias;
      const float verticalStep = dt * GetVelocityWR().GetZ();
      float newDelta = 5.f * dt;
      if (delta > 0.f) {
        if (delta > verticalStep && delta > newDelta) {
          if (delta > GetStepUpHeight()) {
            newDelta += delta - GetStepUpHeight();
          }
          newBias = mStepCameraZBias + newDelta;
        }
      } else if (delta < verticalStep && delta < -newDelta) {
        if (delta < -GetStepDownHeight()) {
          newDelta += -delta - GetStepDownHeight();
        }
        newBias = mStepCameraZBias - newDelta;
      }
    }
    SetEyeZBias(newBias - oldBias);
  } else {
    SetEyeZBias(0.f);
  }
  mStepCameraZBias = newBias;
  x1269_27_ = false;
}

// Retail 0x80012D10, `CPlayer::CalculateLeftStickEdgePosition`; `extern "C"` and defined in
// `src/MetroidPrime/Player/CPlayer.cpp:613` because Echoes' DOL carries no symbol there.
extern "C" CVector3f fn_80012D10(const CPlayer* player, float strafeInput, float forwardInput);

bool CPlayer::SidewaysDashAllowed(float strafeInput, float forwardInput,
                                  const CFinalInput& input) const {
  // Retail 0x80189658: Prime 1's body, with `x1269_24_` (shift 25) and `mHitWallDuringMove`
  // (shift 26) standing in for Prime 1's `mSlidingOnWall` / `mHitWall` - both are read out of
  // the one byte at 0x1269, which is why retail loads it once. The two arms differ in more than
  // the conditions: the hold arm compares `|strafe| >= |forward|` with the strafe absolute value
  // in the register the forward one came from (`fabs f30,f31`), the press arm keeps both in f0/f1,
  // and only the hold arm scales by the tweak threshold rather than the 0.01f at -23080(r2).
  if (x1269_24_ || mHitWallDuringMove || mOrbitState != kOS_OrbitObject) {
    return false;
  }
  if (GetTweakPlayer()->GetDashOnButtonRelease()) {
    if (mOrbitState != kOS_NoOrbit && GetTweakPlayer()->GetDashEnabled() &&
        mStartingJumpTimeout > 0.f && !JumpHeld(input) &&
        mDashButtonHoldTime < GetTweakPlayer()->GetDashButtonHoldCancelTime() &&
        CMath::AbsF(strafeInput) >= CMath::AbsF(forwardInput) &&
        CMath::AbsF(strafeInput) > GetTweakPlayer()->GetDashStrafeInputThreshold()) {
      return true;
    }
  } else if (mOrbitState != kOS_NoOrbit && GetTweakPlayer()->GetDashEnabled() &&
             JumpPressed(input) && mStartingJumpTimeout > 0.f &&
             CMath::AbsF(strafeInput) >= CMath::AbsF(forwardInput) &&
             CMath::AbsF(strafeInput) > 0.01f) {
    const CVector3f stickEdge = fn_80012D10(this, strafeInput, forwardInput);
    const float inputMagnitude =
        CMath::SqrtF(strafeInput * strafeInput + forwardInput * forwardInput);
    // `edgeMagnitude` has to be its own named local: retail keeps it in f31 *across* the two
    // tweak calls and divides into a third register (f0), which only happens when the division
    // is written inside the condition instead of being folded into the `inputMagnitude` local.
    const float edgeMagnitude = stickEdge.Magnitude();
    if (inputMagnitude / edgeMagnitude >= GetTweakPlayer()->GetDashStrafeInputThreshold()) {
      return true;
    }
  }
  return false;
}

// The four 8-float tables this unit indexes by `CPlayer::ESurfaceRestraints`, read out of retail's
// image with `python3 tools/dol_read.py 0x803A9F70 0x80`:
//
//   0x803A9F70  skStrafeDistances        {11.8, 11.8, 11.8, 5, 6, 5, 5, 6}
//   0x803A9F90  skDashStrafeDistances    {11.8, 30,   22.6, 10, 10, 10, 10, 10}
//   0x803A9FB0  skStrafeDistancesEchoes  {11.8, 18,   15,   10, 10, 10, 10, 10}
//   0x803A9FD0  skOrbitForwardDistances  {11.8, 11.8, 11.8, 5, 6, 5, 5, 6}
//
// **The declaration order *is* the link order.** This unit has a `.text` and a `.ctors` claim in
// `config/G2ME01/splits.txt` and no `.rodata` claim, so the linker places the section by matching
// its bytes against retail's image, whole: the concatenation of these four tables
// (`ABCD`, 0x80 bytes) has exactly one byte-match in the DOL, at retail's 0x803A9F70, and the two
// tables `ComputeDash` indexes by one shared base register - `lis r6,-32709; addi r31,r6,-24720`
// then `lfsx` at `r31+0`, `r31+32` and `r31+96` - land on exactly the three addresses above.
static const float skStrafeDistances[] = {11.8f, 11.8f, 11.8f, 5.f, 6.f, 5.f, 5.f, 6.f};
static const float skDashStrafeDistances[] = {11.8f, 30.f, 22.6f, 10.f, 10.f, 10.f, 10.f, 10.f};
static const float skStrafeDistancesEchoes[] = {11.8f, 18.f, 15.f, 10.f,
                                                10.f,   10.f, 10.f, 10.f};
static const float skOrbitForwardDistances[] = {11.8f, 11.8f, 11.8f, 5.f, 6.f, 5.f, 5.f, 6.f};

void CPlayer::FinishSidewaysDash() {
  if (mSidewaysDashing) {
    mDoneSidewaysDashing = true;
    if (mMovementState != NPlayer::kMS_OnGround) {
      CVector3f v = GetVelocityWR();
      const CVector3f& velocity = v;
      CVector2f planar(velocity[0], velocity[1]);
      CVector3f flat(planar.GetX(), planar.GetY(), 0.f);
      const float cap = skStrafeDistancesEchoes[GetSurfaceRestraint()];
      const float speed = flat.Magnitude();
      if (speed > cap) {
        const float accel = mAccelerationChangeTimer > 0.f ? GetAcceleration() : 1.f;
        const float scale = (speed - accel * (speed - cap)) / speed;
        CVector3f out;
        out.SetX(scale * velocity.GetX());
        out.SetY(scale * velocity.GetY());
        out.SetZ(velocity.GetZ());
        SetVelocityWR(out);
      }
    }
  }
  mSidewaysDashing = false;
  mStrafeInputAtDash = 0.f;
  mDashTimer = 0.f;
}

// `fn_801894C4`, `fn_80185814` and `fn_80185870` are the deleting destructors of the three
// classes `CPlayer`'s morph-ball transitions build on its stack (retail 0x80185814, 0x80185870,
// 0x801894C4 - three identical 92-byte bodies, `stwu r1,-16(r1) / mflr / stw r31 / mr. r31,r3 /
// beq end / <own vtable store> / beq +0x10 / <base vtable store> / extsh. r0,r4 ; ble end /
// mr r3,r31 ; bl CMemory::Free / epilogue`, differing only in the vtable address).
//
// **The second store is unreachable but must be written.** Retail's `beq` after the first store
// tests the CR0 that the opening `mr. r31,r3` set, so the `lbl_803B1750` store can never run;
// mwcceppc emits it anyway when the store sits inside a second `if (self != nullptr)`, which is
// what an inlined `Base::~Base()` looks like in source. Written flat (measured) the `beq` is
// dropped and the body is 84 bytes. `src/MetroidPrime/Player/CMorphBall.cpp`'s `fn_800C88C0` and
// `fn_800C33DC` are this same pair of stores and are already at 100%.
//
// **Written under their retail `extern "C"` names rather than as `~X()`** for the reason every
// other `fn_` in this unit gives: retail's object names them, and a real destructor emits
// `__dt__<mangled>`, a symbol retail's object does not define, so objdiff has nothing to pair with.
// The vtables are referenced as objects and the store written by hand, because the classes have
// no key function here and so no vtable may be emitted for them.
extern "C" char lbl_803B1750[];
extern "C" char lbl_803B5B30[];

extern "C" void* fn_801894C4(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B5B30;
    if (self != nullptr) {
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

void CPlayer::fn_801892a0(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::ComputeDash(const CFinalInput& input, float dt, CStateManager& mgr) {
  // Retail 0x80188D20. Prime 1's body with five measured differences, all visible in retail's
  // code: the "start the dash" block is a call to `fn_801892a0` (0x801892A0) rather than inline
  // assignments and sfx; the jump test is `CPlayer::JumpHeld` (0x8022B8F4) rather than
  // `ControlMapper::GetDigitalInput(kC_JumpOrBoost, ...)`; `x1269_24_` (shift 25) and
  // `mHitWallDuringMove` (shift 26) stand in for Prime 1's `mSlidingOnWall` / `mHitWall`; the sfx
  // handle is `x1184_`; and the acceleration carries Echoes' 270.f factor (`lfs f2,-22968(r2)`,
  // which `tools/sda.py` resolves to 0x8041CA08 = 270.f - resolve every pooled float with it).
  //
  // Two spellings below look redundant and are not: retail hoists one reciprocal and multiplies
  // (`fdivs f5,f0,f26` then three `fmuls f4,f5,f4`), which this repo's `CVector3f::operator/`
  // does not do - it divides three times - so the reciprocal is written out. And `restraint` is
  // never read: retail calls `GetSurfaceRestraint` at 0x80188EFC, before the `mNoStrafeDashBlend`
  // test, and calls it again inside that arm. The dead call is retail's, so it is kept.
  //
  // 98.11%, not 100%: four register-allocation differences remain (see the notes).
  const float strafeInput = StrafeInput(input);
  const float forwardInput = ForwardInput(input, TurnInput(input));
  CVector3f orbitPoint = mOrbitPoint;
  orbitPoint.SetZ(GetTranslation().GetZ());
  const CVector3f orbitToPlayer = GetTranslation() - orbitPoint;
  if (!orbitToPlayer.CanBeNormalized()) {
    return;
  }
  CVector3f useOrbitToPlayer = orbitToPlayer;
  float strafeVelocity = dt * skStrafeDistances[GetSurfaceRestraint()];
  if (JumpHeld(input)) {
    mDashButtonHoldTime += dt;
  }
  if (!mSidewaysDashing) {
    if (SidewaysDashAllowed(strafeInput, forwardInput, input)) {
      fn_801892a0(strafeInput, mgr);
    }
    strafeVelocity *= strafeInput;
  } else {
    mDashTimer += dt;
    if (mMovementState == NPlayer::kMS_OnGround || mDashTimer >= mDashDuration || x1269_24_ ||
        mHitWallDuringMove || mOrbitState != kOS_OrbitObject) {
      FinishSidewaysDash();
      strafeVelocity *= strafeInput;
      CSfxManager::RemoveEmitter(x1184_);
    } else {
      const ESurfaceRestraints restraint = GetSurfaceRestraint();
      if (mNoStrafeDashBlend) {
        strafeVelocity =
            dt * (mDashSpeedMultiplier * skDashStrafeDistances[GetSurfaceRestraint()]);
      } else {
        float blend = CMath::Limit(mDashTimer / mStrafeDashBlendDuration, 1.f);
        blend = 1.f - blend;
        const float dashDifference = skDashStrafeDistances[GetSurfaceRestraint()] -
                                     skStrafeDistances[GetSurfaceRestraint()];
        strafeVelocity = dt * (mDashSpeedMultiplier *
                               (dashDifference * blend +
                                skStrafeDistances[GetSurfaceRestraint()]));
      }
      if (mStrafeInputAtDash < 0.f) {
        strafeVelocity = -strafeVelocity;
      }
    }
  }

  const float angle = strafeVelocity / orbitToPlayer.Magnitude();
  float maxAngle = M_PIF * 2.f / 3.f;
  if (mSidewaysDashing) {
    maxAngle = M_PIF;
  }
  const float limitedAngle = CMath::Limit(angle, maxAngle * dt);
  const CQuaternion rotation = CQuaternion::AxisAngle(
      CUnitVector3f(0.f, 0.f, 1.f, CUnitVector3f::kN_Yes), CRelAngle::FromRadians(limitedAngle));
  useOrbitToPlayer = rotation.Transform(orbitToPlayer);
  orbitPoint += useOrbitToPlayer;
  if (!JumpHeld(input)) {
    mDashButtonHoldTime = 0.f;
  }

  strafeVelocity = dt * (forwardInput * skOrbitForwardDistances[GetSurfaceRestraint()]);
  orbitPoint += strafeVelocity * -useOrbitToPlayer.AsNormalized();
  const CVector2f flatVelocity(GetVelocityWR().GetX(), GetVelocityWR().GetY());
  const float flatVelocityY = flatVelocity.GetY();
  const float invDt = 1.f / dt;
  CVector3f newVelocity = (orbitPoint - GetTranslation()) * invDt;
  newVelocity.SetZ(GetVelocityWR().GetZ());
  CVector3f velocityDelta = newVelocity - CVector3f(flatVelocity.GetX(), flatVelocityY, 0.f);
  velocityDelta.SetZ(0.f);
  const float deltaMagnitude = velocityDelta.Magnitude();
  if (deltaMagnitude > FLT_EPSILON) {
    const float acceleration = 270.f * GetAcceleration();
    const float accelerationBlend = CMath::Limit(deltaMagnitude / (acceleration * dt), 1.f);
    const CVector3f dir = (1.f / deltaMagnitude) * velocityDelta;
    newVelocity = GetVelocityWR() + accelerationBlend * ((acceleration * dt) * dir);
    if (!x1269_24_) {
      SetVelocityWR(newVelocity);
    }
  }
}

void CPlayer::ComputeMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Recover the remaining target behavior.
}

float CPlayer::ForwardInput(const CFinalInput& input, float turnInput) const {
  // Retail 0x80188160. Prime 1's body, with two of its names replaced: `CheckPostGrapple()` is
  // this repo's `InGrappleJumpCooldown()`, and `GetMoveDuringFreeLook()` is retail's unnamed
  // `fn_80215860`. Prime 1's `close_enough(a, b)` is `CMath::AbsF(a - b) < 1e-05f`, which is
  // exactly the `fsubs` against a pooled 0.f and the 1e-05f threshold retail compares against.
  float forward = GetControlMapper().GetAnalogInput(CControlMapper::kC_Forward, input);
  float backward = GetControlMapper().GetAnalogInput(CControlMapper::kC_Backward, input);
  if (mMorphBallState != kMS_Unmorphed || InGrappleJumpCooldown()) {
    backward = 0.f;
  }
  if (!(forward < 0.001f)) {
    forward = CMath::Limit(forward / 0.8f, 1.f);
    if (CMath::AbsF(atan2f(CMath::AbsF(turnInput), forward)) <
        CRelAngle::FromDegrees(50.f).AsRadians()) {
      const CVector3f stick(CMath::AbsF(turnInput), forward, 0.f);
      if (stick.CanBeNormalized()) {
        forward = stick.Magnitude();
      }
    }
  }
  if (!(backward < 0.001f)) {
    backward = CMath::Limit(backward / 0.8f, 1.f);
    if (CMath::AbsF(atan2f(CMath::AbsF(turnInput), backward)) <
        CRelAngle::FromDegrees(50.f).AsRadians()) {
      const CVector3f stick(CMath::AbsF(turnInput), backward, 0.f);
      if (stick.CanBeNormalized()) {
        backward = stick.Magnitude();
      }
    }
  }
  if (!fn_80215860(GetTweakPlayerControls())) {
    CVector3f flatVelocity = GetVelocityWR();
    flatVelocity.SetZ(0.f);
    if (mInFreeLook || mLookButtonHeld) {
      if (mMovementState == NPlayer::kMS_OnGround ||
          CMath::IsEpsilon(flatVelocity.Magnitude(), 0.f, 1e-05f)) {
        return 0.f;
      }
    }
  }
  return CMath::Limit(forward - backward * GetTweakPlayer()->GetBackwardsForceMultiplier(), 1.f);
}

float CPlayer::StrafeInput(const CFinalInput& input) const {
  if (IsMorphBallTransitioning() || mOrbitState == kOS_NoOrbit) {
    return 0.f;
  }
  return GetControlMapper().GetAnalogInput(CControlMapper::kC_StrafeRight, input) -
         GetControlMapper().GetAnalogInput(CControlMapper::kC_StrafeLeft, input);
}

float CPlayer::TurnInput(const CFinalInput& input) const {
  // Retail 0x80187E74. Prime 1's body verbatim, with the two tweak-control flags taken from
  // retail's own unnamed readers rather than `gpTweakPlayer`: `fn_8021586C` gates the whole
  // free-look block and `fn_80215854` the look-button hold test, and each of them is re-`d`
  // rather than hoisted, so the source has to spell `!H || (H && mLookButtonHeld)` twice over.
  float left = GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnLeft, input);
  float right = GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnRight, input);
  if (fn_8021586C(GetTweakPlayerControls())) {
    if (!fn_80215854(GetTweakPlayerControls()) ||
        (fn_80215854(GetTweakPlayerControls()) && mLookButtonHeld)) {
      if (left < 0.01f && right < 0.01f) {
        left = GetControlMapper().GetAnalogInput(CControlMapper::kC_LookLeft, input);
        right = GetControlMapper().GetAnalogInput(CControlMapper::kC_LookRight, input);
      }
    }
  } else if (!fn_80215854(GetTweakPlayerControls()) ||
             (fn_80215854(GetTweakPlayerControls()) && mLookButtonHeld)) {
    const float lookLeft = GetControlMapper().GetAnalogInput(CControlMapper::kC_LookLeft, input);
    const float lookRight =
        GetControlMapper().GetAnalogInput(CControlMapper::kC_LookRight, input);
    if (lookLeft > 0.01f || lookRight > 0.01f) {
      return 0.f;
    }
  }
  if (mOrbitState == kOS_OrbitObject || mOrbitState == kOS_Grapple) {
    return 0.f;
  }
  if (IsMorphBallTransitioning()) {
    return 0.f;
  }
  float turn = left - right;
  if (mOrbitModeTimer > 0.f) {
    turn *= 1.f - 0.5f * CMath::Clamp(0.f, mOrbitModeTimer / GetTweakPlayer()->GetOrbitModeTimer(),
                                     1.f);
  }
  return CMath::Limit(turn, 1.f);
}

float CPlayer::JumpInput(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return 0.f;
}

void CPlayer::SetMoveState(NPlayer::EPlayerMovementState state, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::CalculatePlayerMovementDirection(float dt, const CVector3f& displacement) {
  // Retail 0x80186FE4. Prime 1's body, with two measured differences: there is no early
  // `kMS_Morphing || kMS_Unmorphing` return, and the delta is the `displacement` argument itself
  // (r31, used in place - the function opens with `displacement.CanBeNormalized()`). The two
  // thresholds are Prime 1's own, read out of the pool: 0.02f (`-23012(r2)`) and 0.25f
  // (`-23008(r2)`). `GetTranslation()` here is `CActor::mPosition` (0x54), which is what retail's
  // `lfs f0,84(r30)` triple reads.
  if (displacement.CanBeNormalized() && displacement.Magnitude() > 0.02f) {
    mTimeMoving += dt;
    mMoveSpeed = CMath::AbsF(displacement.Magnitude() / dt);
    mLookDir = displacement.AsNormalized();
    CVector3f flatDelta = displacement;
    flatDelta.SetZ(0.f);
    if (flatDelta.CanBeNormalized()) {
      mFlatMoveSpeed = CMath::AbsF(flatDelta.Magnitude() / dt);
      flatDelta.Normalize();
      switch (mMorphBallState) {
      case kMS_Morphed:
        if (mFlatMoveSpeed > 0.25f) {
          mMoveDir = flatDelta;
        }
        mGunDir = mMoveDir;
        mLastPosForDirCalc = GetTranslation();
        break;
      case kMS_Unmorphed:
      case kMS_Morphing:
      case kMS_Unmorphing:
        mLookDir = GetTransform().GetForward();
        mMoveDir = mLookDir;
        mMoveDir.SetZ(0.f);
        if (mMoveDir.CanBeNormalized()) {
          mMoveDir.Normalize();
        }
        mGunDir = mMoveDir;
        mLastPosForDirCalc = GetTranslation();
        break;
      }
    } else {
      if (mMorphBallState != kMS_Morphed) {
        mLookDir = GetTransform().GetForward();
        mMoveDir = mLookDir;
        mMoveDir.SetZ(0.f);
        if (mMoveDir.CanBeNormalized()) {
          mMoveDir.Normalize();
        }
        mGunDir = mMoveDir;
        mLastPosForDirCalc = GetTranslation();
      }
      mFlatMoveSpeed = 0.f;
    }
  } else {
    mTimeMoving = 0.f;
    switch (mMorphBallState) {
    case kMS_Morphed:
    case kMS_Morphing:
    case kMS_Unmorphing:
      mLookDir = mMoveDir;
      break;
    default:
      mLookDir = GetTransform().GetForward();
      mMoveDir = mLookDir;
      mMoveDir.SetZ(0.f);
      if (mMoveDir.CanBeNormalized()) {
        mMoveDir.Normalize();
      }
      mGunDir = mMoveDir;
      mLastPosForDirCalc = GetTranslation();
      break;
    }
    mMoveSpeed = 0.f;
    mFlatMoveSpeed = 0.f;
  }
  mMoveDir.SetZ(0.f);
  if (mMoveDir.CanBeNormalized()) {
    mLookDir.Normalize();
  }
}

void CPlayer::CalculateLeaveMorphBallDirection(const CFinalInput& input) {
  // Retail 0x80186E9C. `xfe8_` (0xfe8) is the destination of both copies of
  // `mMoveDir` (0xfdc) below; the header has no name for it yet.
  if (mMorphBallState != kMS_Morphed || mMorphBall->InScrewAttackMode()) {
    xfe8_ = mMoveDir;
    return;
  }
  const float forward = GetControlMapper().GetAnalogInput(CControlMapper::kC_Forward, input);
  const float backward = GetControlMapper().GetAnalogInput(CControlMapper::kC_Backward, input);
  const float left = GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnLeft, input);
  const float right = GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnRight, input);
  if (forward > 0.3f || backward > 0.3f || left > 0.3f || right > 0.3f) {
    if (GetVelocityWR().Magnitude() > 0.5f) {
      xfe8_ = mMoveDir;
    }
  }
}

float CPlayer::GetBallMaxVelocity() const {
  return gpTweakBall->GetBallTranslationMaxSpeed(GetSurfaceRestraint());
}

float CPlayer::GetActualFirstPersonMaxVelocity(float dt) const {
  const float friction = GetTweakPlayer()->GetPlayerTranslationFriction(GetSurfaceRestraint());
  const float frictionForce = friction * GetMass();
  const float maxSpeed = GetTweakPlayer()->GetPlayerTranslationMaxSpeed(GetSurfaceRestraint());
  const float acceleration = GetTweakPlayer()->GetMaxTranslationalAcceleration(GetSurfaceRestraint());
  return -(frictionForce * maxSpeed / (acceleration * dt) - maxSpeed - friction);
}

float CPlayer::GetActualBallMaxVelocity(float dt) const {
  const float friction = gpTweakBall->GetBallTranslationFriction(GetSurfaceRestraint());
  const float frictionForce = friction * GetMass();
  const float maxSpeed = gpTweakBall->GetBallTranslationMaxSpeed(GetSurfaceRestraint());
  const float acceleration = gpTweakBall->GetMaxBallTranslationAcceleration(GetSurfaceRestraint());
  return -(frictionForce * maxSpeed / (acceleration * dt) - maxSpeed - friction);
}

void CPlayer::CollidedWith(const TUniqueId& id, const CCollisionInfoList& list,
                           CStateManager& mgr) {
  if (mMorphBallState != kMS_Unmorphed) {
    mMorphBall->CollidedWith(id, list, mgr);
  }
}

CTransform4f CPlayer::GetPrimitiveTransform() const {
  return CPhysicsActor::GetPrimitiveTransform();
}

const CCollidableSphere* CPlayer::GetCollidableSphere() const {
  return &mMorphBall->GetCollidableSphere();
}

const CCollisionPrimitive* CPlayer::GetCollisionPrimitive() const {
  // Retail 0x80186BD0 tests the four enumerators separately (`== 1`, `>= 4`, `== 0`) and sends
  // every arm except `kMS_Morphed` to `CPhysicsActor`, so this is a `switch`, not an `if`.
  switch (mMorphBallState) {
    case kMS_Morphed:
      return GetCollidableSphere();
    case kMS_Unmorphed:
      return CPhysicsActor::GetCollisionPrimitive();
    case kMS_Morphing:
    case kMS_Unmorphing:
      return CPhysicsActor::GetCollisionPrimitive();
    default:
      return CPhysicsActor::GetCollisionPrimitive();
  }
}

CTransform4f CPlayer::CreateTransformFromMovementDirection() const {
  CVector3f direction = mMoveDir;
  if (direction.CanBeNormalized()) {
    direction.Normalize();
  } else {
    direction = CVector3f(0.f, 1.f, 0.f);
  }
  const CVector3f right(direction.GetY(), -direction.GetX(), 0.f);
  return CTransform4f::FromColumns(right, direction, CVector3f::Up(), GetTranslation());
}

void CPlayer::BombJump(const CVector3f& position, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::Teleport(const CTransform4f& xf, CStateManager& mgr, bool resetBallCam) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::CheckSubmerged() const {
  // Retail 0x801864A0 returns false up front when `CActor::IsInFluid` is false, then computes
  // *both* heights before the morph test - `2.f * GetBallRadius` and `0.5f * GetEyeHeight` are
  // both live across the `mMorphBallState` load, so neither is inside the `if`.
  if (!IsInFluid()) {
    return false;
  }
  const float ballHeight = 2.f * GetTweakPlayer()->GetBallRadius();
  const float eyeHeight = 0.5f * GetEyeHeight();
  float height = eyeHeight;
  if (mMorphBallState == kMS_Morphed) {
    height = ballHeight;
  }
  return mDistanceUnderWater >= height;
}

void CPlayer::UpdateSubmerged(const CStateManager& mgr) {
  // Retail 0x801863B8. Echoes drops Prime 1's `GetTriggerBoundsWR().GetMaxPoint().GetZ()` plane
  // and its `kFT_Lava || kFT_ThickLava` test in favour of `CScriptWater::GetWRSurfacePlane()`
  // plus a single `fluidType == 2` compare, and it drops the trailing `CheckSubmerged()` call
  // entirely - `mDistanceUnderWater` is the only thing this leaves behind.
  //
  // `mKnockBackManager.fn_801C0124()` is retail's own call at 0x801863FC: `addi r3,r30,3804` is
  // `&mKnockBackManager` (0xEDC, MWCC `offsetof`) and the callee writes three of its fields
  // (0x801C0124: `+0x64`, `+0x68` = `lfs f0,-21572(r2)`, `+0x6C` = `kInvalidUniqueId`). The
  // class's private members `x64_`/`x68_`/`x6c_` line up with that exactly, so the call goes
  // through the class; its implementation lives in the unclaimed `auto_03_801BEDD0_text` range,
  // which is why the port link now reports it missing (`docs/research/port_link_gap_list.md`).
  mInLava = false;
  mDistanceUnderWater = 0.f;
  if (!IsInFluid()) {
    return;
  }
  mKnockBackManager.fn_801C0124();
  const CScriptWater* water =
      TCastToConstPtr< CScriptWater >(mgr.GetObjectById(InFluidId()));
  if (water != nullptr) {
    // `-GetHeight`: retail computes `n.y*pos.y`, folds `n.x*pos.x` and then `n.z*pos.z` into it
    // with `fmadds`, subtracts the constant and negates the result - i.e. the *negative* of
    // `CPlane::GetHeight`, which is this header's spelling. Depth below the surface.
    mDistanceUnderWater = -water->GetWRSurfacePlane().GetHeight(GetTranslation());
    // `subfic r3,r3,2` / `cntlzw` is `== 2`, and 2 is `CFluidPlane`'s lava fluid type (the enum
    // itself is still unnamed in `CFluidPlane.hpp`, so the value is written out).
    mInLava = water->GetFluidPlane()->GetFluidType() == 2;
  }
}

float CPlayer::GetStepDownHeight() const {
  if (mMovementState == NPlayer::kMS_Jump) {
    return -1.f;
  }
  if (mMovementState == NPlayer::kMS_ApplyJump) {
    return 0.1f;
  }
  return CPhysicsActor::GetStepDownHeight();
}

float CPlayer::GetStepUpHeight() const {
  if (mMovementState == NPlayer::kMS_Jump || mMovementState == NPlayer::kMS_ApplyJump) {
    return 0.3f;
  }
  return CPhysicsActor::GetStepUpHeight();
}

float CPlayer::GetUnbiasedEyeHeight() const {
  return mFpBounds.GetPointD().GetZ() - GetTweakPlayer()->GetEyeOffset();
}

float CPlayer::GetEyeHeight() const {
  // Retail 0x80186284 repeats `GetUnbiasedEyeHeight`'s body instead of calling it, and its frame
  // also spills `r31` for `mEyeZBias`.
  return mEyeZBias + (mFpBounds.GetPointD().GetZ() - GetTweakPlayer()->GetEyeOffset());
}

CVector3f CPlayer::GetEyePosition() const {
  return GetTranslation() + CVector3f(0.f, 0.f, GetEyeHeight());
}

CVector3f CPlayer::GetBallPosition() const {
  return GetTranslation() + CVector3f(0.f, 0.f, GetTweakPlayer()->GetBallRadius());
}

void CPlayer::SetEyeZBias(float bias) { mEyeZBias = bias; }

float CPlayer::UpdateCameraBob(float dt, CStateManager& mgr) {
  CPlayerCameraBob::ECameraBobState state;
  const CVector3f velocity = GetVelocityWR();
  float magnitude;
  if (mOrbitState == kOS_NoOrbit) {
    state = CPlayerCameraBob::kCBS_Walk;
    const float forwardSpeed = CVector3f::Dot(velocity, GetTransform().GetForward());
    magnitude = CMath::AbsF(forwardSpeed / GetActualFirstPersonMaxVelocity(dt));
    if (magnitude < 0.01f) {
      state = CPlayerCameraBob::kCBS_WalkNoBob;
      magnitude = 0.f;
    }
  } else {
    state = CPlayerCameraBob::kCBS_Orbit;
    const float rightSpeed = CVector3f::Dot(velocity, GetTransform().GetRight());
    const float forwardSpeed = CVector3f::Dot(velocity, GetTransform().GetForward());
    const float maxSpeed = GetActualFirstPersonMaxVelocity(dt);
    const float strafeSpeed = skStrafeDistances[GetSurfaceRestraint()];
    const float maxMagnitude = CMath::SqrtF(strafeSpeed * strafeSpeed + maxSpeed * maxSpeed);
    magnitude = CMath::SqrtF(rightSpeed * rightSpeed + forwardSpeed * forwardSpeed) / maxMagnitude;
    magnitude *= CPlayerCameraBob::GetOrbitBobScale();
    magnitude = rstl::min_val(CPlayerCameraBob::GetMaxOrbitBobScale(), magnitude);
    if (magnitude < 0.01f) {
      magnitude = 0.f;
    }
  }
  if (mMovementState != NPlayer::kMS_OnGround) {
    state = CPlayerCameraBob::kCBS_InAir;
    magnitude = 0.f;
  } else if (magnitude < 0.01f) {
    if (static_cast< int >(mGun->GetFiredWeaponFlags()) != 0) {
      state = CPlayerCameraBob::kCBS_GunFireNoBob;
      magnitude = 0.f;
    } else if (CMath::AbsF(GetAngularVelocityOR().GetAngle()) > 0.1f) {
      state = CPlayerCameraBob::kCBS_TurningNoBob;
      magnitude = 0.f;
    }
  }
  if (mInFreeLook || mLookButtonHeld) {
    state = CPlayerCameraBob::kCBS_FreeLookNoBob;
    magnitude = 0.f;
  }
  if (mOrbitState == kOS_Grapple) {
    state = CPlayerCameraBob::kCBS_GrappleNoBob;
    magnitude = 0.f;
  }
  if (mScanState == kSS_ScanComplete) {
    magnitude = 0.f;
  }
  if (mDoneSidewaysDashing) {
    state = CPlayerCameraBob::kCBS_FreeLookNoBob;
    magnitude *= 0.1f;
    if (mMovementState == NPlayer::kMS_OnGround) {
      mDoneSidewaysDashing = false;
    }
  }
  if (mCameraManager->IsInCinematicCamera()) {
    magnitude = 0.f;
  }
  magnitude *= mCameraManager->GetCameraBobMagnitude();
  mCameraBob->SetPlayerVelocity(velocity);
  mCameraBob->SetState(state, mgr);
  mCameraBob->SetBobMagnitude(magnitude);
  const float slowSpeedPeriodScale = CPlayerCameraBob::GetSlowSpeedPeriodScale();
  const float timeScaleRange = 1.f - slowSpeedPeriodScale;
  mCameraBob->SetBobTimeScale(timeScaleRange * magnitude + slowSpeedPeriodScale);
  mCameraBob->Update(dt, mgr, *this);
  return magnitude;
}

void CPlayer::fn_80185a88(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::fn_801858cc(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return false;
}

extern "C" char lbl_803B1750[];
extern "C" char lbl_803B5B3C[];
extern "C" char lbl_803B5B48[];

extern "C" void* fn_80185870(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B5B48;
    if (self != nullptr) {
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void* fn_80185814(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B5B3C;
    if (self != nullptr) {
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

void CPlayer::TransitionToMorphBallState(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::TransitionFromMorphBallState(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::fn_80184ba4(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::fn_80184a60(float dt, CStateManager& mgr, EPlayerMorphBallState state) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::fn_801843d0(CStateManager& mgr, EPlayerMorphBallState state) {
  // TODO: Recover the remaining target behavior.
  return false;
}

void CPlayer::fn_801842c8(float dt, CStateManager& mgr, EPlayerMorphBallState state) {
  // Retail 0x801842C8: nine calls and one three-float store, in this order.
  //  - `SetMorphBallState` gets the literal 3 = `kMS_Unmorphing` in r4 and the caller's `state`
  //    in r5, so it is the two-argument overload (`EPlayerMorphBallState spawnedState`).
  //  - `fn_801843d0` returns the bool that decides the trailing `SetCameraState`, and it is kept
  //    in r31 across the four calls in between, so it is read into a local.
  //  - `ClearForcesAndTorques` then `SetAngularVelocityWR(CAxisAngle::Identity())`: the
  //    `Identity()` result is passed straight through r4 without a store, so the call is written
  //    as the inline-returning reference this header declares.
  //  - the three `stfs` at 0x1C0/0x1C4/0x1C8 copy `CVector3f::Zero()`: `lis r3,-32703` +
  //    `lfsu f0,29872(r3)` is 0x804174B0, which is `sZeroVector__9CVector3f` in
  //    `config/G2ME01/symbols.txt`, and 0x1C0 is `CPhysicsActor::mMomentum` (MWCC `offsetof`), so
  //    this is `SetMomentumWR(CVector3f::Zero())`.
  //  - the final `SetCameraState` runs only when `fn_801843d0` returned false (`bne` skips it).
  SetMorphBallState(kMS_Unmorphing, state);
  TransitionFromMorphBallState(dt, mgr);
  mMorphBall->LeaveMorphBallState(mgr);
  const bool transitionDone = fn_801843d0(mgr, state);
  ForceGunOrientation(GetTransform(), mgr);
  mGun->DrawGun(mgr);
  ClearForcesAndTorques();
  SetAngularVelocityWR(CAxisAngle::Identity());
  AddMaterial(kMT_GroundCollider, mgr);
  SetMomentumWR(CVector3f::Zero());
  if (!transitionDone) {
    SetCameraState(kCS_Spawned, mgr);
  }
}

void CPlayer::fn_80184294(EPlayerMorphBallState state) {
  mScrewAttackTransitionPending = true;
  mScrewAttackTransitionState = state;
  mMorphBall->SetScrewAttackActive(false);
}

void CPlayer::ActivateMorphBallCamera(CStateManager& mgr) {
  // Retail 0x80184240. Two calls and nothing else: `SetCameraState` with the literal 2
  // (`li r4,2` - `kCS_Two`, the header's placeholder name for value 2) and then, off
  // `mCameraManager` (0x1318) and its `mBallCamera` (+0x1C), `CBallCamera::SetState` with 0
  // (`li r4,0` = `kBCS_Default`). `CBallCamera::SetState` is defined in
  // `src/MetroidPrime/Cameras/CBallCamera.cpp:702`, so the second call resolves; this function
  // only has to make the call.
  SetCameraState(kCS_Two, mgr);
  mCameraManager->BallCamera()->SetState(CBallCamera::kBCS_Default, mgr);
}

void CPlayer::EnterMorphBallState(CStateManager& mgr, EPlayerMorphBallState state) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::LeaveMorphBallState(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateTransitionFilter(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateMorphBallTransition(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::IsGravityBoostActive() const { return mGravityBoostDuration > 0.f; }

void CPlayer::StartGravityBoost(CStateManager& mgr) {
  // Retail 0x8018381C. The guard is a two-part disjunction read off the branch targets:
  // `clrlwi. r0,r3,24` / `bne` on `GetGravityBoostMultipleAllowed`'s bool jumps straight into the
  // body, and when it is false `lbz r0,4700(r31)` (0x125C = `mGravityBoostActive`) / `bne` leaves.
  // Inside, the four exits are, in order: `IsGravityBoostActive()` (0x80183980), the camera's
  // fluid list through `CStateManager::mCameraManagers[GetPlayerIndex()]->mFpCamera` (0x151C /
  // 0x18 / 0x110, the same chain `ApplyGravityBoost` and `CPlayerGunBase::Update` use),
  // `mMorphBallState` (0x38C), and **this** actor's own `mFluidIds` count (0x110) - so the boost
  // only starts with the player in a fluid.
  if (GetTweakPlayer()->GetGravityBoostMultipleAllowed() || !mGravityBoostActive) {
    if (!IsGravityBoostActive() && mgr.CameraManager(GetPlayerIndex())
                                      ->FirstPersonCamera()
                                      ->IsInFluid()
        && mMorphBallState == kMS_Unmorphed) {
      // The last gate is written as an if/else rather than as a fourth `&&` term, because retail
      // rotates it: `lwz r0,272(r31)` / `cmpwi` / `bne +0x8` / `b <exit>`, i.e. the exit branch is
      // skipped over rather than branched to.
      if (IsInFluid()) {
        mGravityBoostDuration = GetTweakPlayer()->GetGravityBoostTime();
        // The z component is stored twice (`stfs f1,28(r1)` then `stfs f0,28(r1)`), which is the
        // copy-then-assign shape; `-23116(r2)` is 0.1f (tools/sda.py `s2:-23116`). The multiply
        // is written constant-first because that is the order MWCC keeps the two z stores from
        // being coalesced into one, as retail has them.
        CVector3f velocity = GetVelocityWR();
        velocity.SetZ(0.1f * velocity.GetZ());
        SetVelocityWR(velocity);
        // `li r9,1` here against `li r9,0` in `EndGravityBoost`: the start sound loops, the cancel
        // sound does not. 291 / 782 are the single- / multiplayer ids.
        mGravityBoostSfx = CSfxManager::SfxStart(mgr.ReturnFirstIfSingleElseSecond(291, 782), 127,
                                                 GetSoundPan(kMSP_4), GetCurrentAreaId().Value(), true,
                                                 true, CSfxManager::kMedPriority);
        CSfxManager::SetIgnoreAreaLowPass(mGravityBoostSfx, true);
        CSfxHandle handle = mGravityBoostSfx;
        ApplySubmergedPitchBend(handle);
        mGravityBoostActive = true;
      } else {
        return;
      }
    }
  }
}

// Guessed name
void CPlayer::ApplyGravityBoost(float dt, CStateManager& mgr) {
  // Retail 0x80183754. Three conditions end the boost and one applies it, and the polarity of
  // each is read off the branch targets rather than assumed:
  //   - `lfs f0,4688(r3)` / `fcmpo cr0,f0,f2` / `ble` with `f2 = 0.f` (`-23120(r2)`) returns while
  //     the duration has already run out without ending the boost;
  //   - the duration is decremented and **re-read** (`lfs f0,4688(r30)` at 0x80183788) before the
  //     second test, so the source re-reads the member rather than reusing the register. The
  //     first test is a bare `ble` on the `fcmpo` (no `cror`), so the guard is the positive
  //     `> 0.f` with the body inside it; the second is `cror eq,lt,eq` + `beq`, which is the
  //     `<= 0.f` form.
  if (mGravityBoostDuration > 0.f) {
    mGravityBoostDuration -= dt;
    if (mGravityBoostDuration <= 0.f || !mgr.CameraManager(GetPlayerIndex())
                                           ->FirstPersonCamera()
                                           ->IsInFluid()
          || mMorphBallState != kMS_Unmorphed) {
        EndGravityBoost(mgr);
    } else {
      // Retail builds the force vector on the stack (`stfs f1,16(r1)` / `stfs f0,8(r1)` /
      // `stfs f0,12(r1)`, the third store first) and passes `CAxisAngle::Identity()`'s address
      // in r5.
      ApplyForceOR(CVector3f(0.f, 0.f, GetTweakPlayer()->GetGravityBoostForce()),
                   CAxisAngle::Identity());
    }
  }
}

void CPlayer::EndGravityBoost(CStateManager& mgr) {
  // Retail 0x80183648, in order:
  //  - the velocity is copied to the stack (`stfs` at 24/28/32(r1)) *before* the tweak lookups,
  //    and only its z component is reloaded and scaled afterwards (`lfs f0,32(r1)` /
  //    `fmuls f0,f0,f1` / `stfs f0,32(r1)`), so the scaled value goes back into the copy that
  //    `SetVelocityWR` is handed by reference;
  //  - `mGravityBoostDuration` is cleared with `lfs f2,-23120(r2)` = 0.f;
  //  - the running boost sfx is stopped only when its handle is non-zero (`cmplwi r0,0`), and the
  //    handle is passed **by value** through a 20(r1) temporary;
  //  - `li r4,864` / `li r5,863` are the single-player / multiplayer ids handed to
  //    `CStateManager::ReturnFirstIfSingleElseSecond`, `li r5,127` is the volume, `lwz r7,4(r29)`
  //    is `CEntity::mAreaId` (0x4, measured) passed as the area, `li r8,1` / `li r9,0` are
  //    useAcoustics / looped, and `lha r10,-16604(r2)` is `CSfxManager::kMedPriority` (127,
  //    read as a halfword, verified with tools/sda.py `s2:-16604`);
  //  - the returned handle is written to `x1258_` (0x1258) and then re-loaded **twice**, once
  //    for the by-value `SetIgnoreAreaLowPass` argument and once for the by-reference
  //    `ApplySubmergedPitchBend` argument - which is what two separate statements over the same
  //    member emit, so it is written as two.
  CVector3f velocity = GetVelocityWR();
  const float dampening = GetTweakPlayer()->GetGravityBoostCancelDampening();
  velocity.SetZ(velocity.GetZ() * dampening);
  SetVelocityWR(velocity);
  mGravityBoostDuration = 0.f;
  if (mGravityBoostSfx) {
    CSfxManager::SfxStop(mGravityBoostSfx);
  }
  x1258_ = CSfxManager::SfxStart(mgr.ReturnFirstIfSingleElseSecond(864, 863), 127,
                                  GetSoundPan(kMSP_4), GetCurrentAreaId().Value(), true, false,
                                  CSfxManager::kMedPriority);
  CSfxManager::SetIgnoreAreaLowPass(x1258_, true);
  CSfxHandle handle = x1258_;
  ApplySubmergedPitchBend(handle);
}
