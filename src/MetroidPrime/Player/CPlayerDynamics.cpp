#include "MetroidPrime/Player/CPlayer.hpp"

#include "Collision/CCollidableSphere.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

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

float CPlayer::GetAverageSpeed() const {
  // Retail 0x80189C50 calls `TReservedAverage::GetAverage` twice: once to test the flag and
  // again to read the value, so the test and the read must be two separate calls.
  if (mMoveSpeedAvg.GetAverage()) {
    return *mMoveSpeedAvg.GetAverage();
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
  // result is tested. The branch is on bit 4 of the byte at 0x126B, which is `x126b_26_`:
  // measured by compiling this body against all eight of that byte's 1-bit fields, and only
  // `x126b_26_` emits retail's `rlwinm. r0,r0,27,31,31`. The bit's own name is still unknown.
  if (x126b_26_) {
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

bool CPlayer::SidewaysDashAllowed(float strafeInput, float forwardInput,
                                  const CFinalInput& input) const {
  // TODO: Recover the remaining target behavior.
  return false;
}

// Retail lbl_803A9FB0: 8 floats indexed by `CPlayer::ESurfaceRestraints`.
static const float skStrafeDistancesEchoes[] = {11.8f, 18.f, 15.f, 10.f,
                                                10.f,   10.f, 10.f, 10.f};

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
  // TODO: Recover the remaining target behavior.
}

void CPlayer::ComputeMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Recover the remaining target behavior.
}

float CPlayer::ForwardInput(const CFinalInput& input, float turnInput) const {
  // TODO: Recover the remaining target behavior.
  return 0.f;
}

float CPlayer::StrafeInput(const CFinalInput& input) const {
  if (IsMorphBallTransitioning() || mOrbitState == kOS_NoOrbit) {
    return 0.f;
  }
  return GetControlMapper().GetAnalogInput(CControlMapper::kC_StrafeRight, input) -
         GetControlMapper().GetAnalogInput(CControlMapper::kC_StrafeLeft, input);
}

float CPlayer::TurnInput(const CFinalInput& input) const {
  // TODO: Recover the remaining target behavior.
  return 0.f;
}

float CPlayer::JumpInput(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return 0.f;
}

void CPlayer::SetMoveState(NPlayer::EPlayerMovementState state, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::CalculatePlayerMovementDirection(float dt, const CVector3f& displacement) {
  // TODO: Recover the remaining target behavior.
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
  // TODO: Recover the remaining target behavior.
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
  // TODO: Recover the remaining target behavior.
  return 0.f;
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
  // TODO: Recover the remaining target behavior.
}

void CPlayer::fn_80184294(EPlayerMorphBallState state) {
  mScrewAttackTransitionPending = true;
  mScrewAttackTransitionState = state;
  mMorphBall->SetScrewAttackActive(false);
}

void CPlayer::ActivateMorphBallCamera(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
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
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::ApplyGravityBoost(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::EndGravityBoost(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}
