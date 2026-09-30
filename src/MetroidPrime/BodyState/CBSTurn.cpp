#include "MetroidPrime/BodyState/CBSTurn.hpp"

#include "Kyoto/Animation/CPASAnimParmData.hpp"
#include "Kyoto/Animation/CPASDatabase.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/CAnimPlaybackParms.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/math.hpp"

CBSTurn::CBSTurn() : mRotateSpeed(0.f), mDest(0.f, 0.f), mTurnDir(pas::kTD_Invalid) {}

void CBSTurn::Start(CBodyController& bc, CStateManager& mgr) {
  const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
  const CVector2f lookDir = fwd.ToVec2f();
  mDest = bc.GetCommandMgr().GetFaceVector().ToVec2f();
  const float deltaAngle = CMath::Rad2Deg(CVector2f::GetAngleDiff(lookDir, mDest));
  const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
  mTurnDir = CVector2f::Dot(leftDir, mDest) > 0.f ? pas::kTD_Left : pas::kTD_Right;

  const CPASDatabase& db = bc.GetPASDatabase();
  const CPASAnimParmData parms(pas::kAS_Turn, CPASAnimParm::FromEnum(mTurnDir),
                               CPASAnimParm::FromReal32(deltaAngle),
                               CPASAnimParm::FromEnum(bc.GetLocomotionType()));
  const rstl::pair< float, int > best = db.FindBestAnimation(parms, *mgr.Random(), -1);
  bc.SetCurrentAnimation(CAnimPlaybackParms(best.second, -1, 1.f, true), false, false);

  const CPASAnimParm animAngle = db.GetAnimState(pas::kAS_Turn)->GetAnimParmData(best.second, 1);
  mRotateSpeed =
      CRelAngle::FromDegrees(mTurnDir == pas::kTD_Left ? animAngle.GetReal32Value() - deltaAngle
                                                      : deltaAngle - animAngle.GetReal32Value())
          .AsRadians();
  float playbackRate = bc.GetTimeScale();
  if (const CPatterned* patterned = TCastToPtr< CPatterned >(&bc.GetOwner())) {
    playbackRate *= patterned->GetSpeed();
  }
  if (playbackRate <= 0.f) {
    playbackRate = 1.f;
  }
  const float timeRem = bc.GetAnimTimeRemaining() / playbackRate;
  mRotateSpeed = timeRem > 0.f ? mRotateSpeed / timeRem : mRotateSpeed;
}

pas::EAnimationState CBSTurn::UpdateBody(float dt, CBodyController& bc, CStateManager& mgr) {
  const pas::EAnimationState state = GetBodyStateTransition(dt, bc);
  if (state == pas::kAS_Invalid) {
    bc.SetDeltaRotation(CQuaternion::ZRotation(CRelAngle::FromRadians(mRotateSpeed * dt)));
  }
  return state;
}

void CBSTurn::Shutdown(CBodyController&) {}

bool CBSTurn::FacingDest(CBodyController& bc) const {
  const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
  const CVector2f lookDir = fwd.ToVec2f();
  const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
  if (mTurnDir == pas::kTD_Left) {
    if (CVector2f::Dot(leftDir, mDest) < 0.f) {
      return true;
    }
  } else {
    if (CVector2f::Dot(leftDir, mDest) > 0.f) {
      return true;
    }
  }
  return false;
}

pas::EAnimationState CBSTurn::GetBodyStateTransition(float dt, CBodyController& bc) {
  CBodyStateCmdMgr& cmdMgr = bc.CommandMgr();
  if (cmdMgr.GetCmd(kBSC_Hurled)) {
    return pas::kAS_Hurled;
  }
  if (cmdMgr.GetCmd(kBSC_KnockDown)) {
    return pas::kAS_Fall;
  }
  if (cmdMgr.GetCmd(kBSC_LoopHitReaction)) {
    return pas::kAS_LoopReaction;
  }
  if (cmdMgr.GetCmd(kBSC_KnockBack)) {
    return pas::kAS_KnockBack;
  }
  if (cmdMgr.GetCmd(kBSC_Locomotion)) {
    return pas::kAS_Locomotion;
  }
  if (cmdMgr.GetCmd(kBSC_Generate)) {
    return pas::kAS_Generate;
  }
  if (cmdMgr.GetCmd(kBSC_MeleeAttack)) {
    return pas::kAS_MeleeAttack;
  }
  if (cmdMgr.GetCmd(kBSC_ProjectileAttack)) {
    return pas::kAS_ProjectileAttack;
  }
  if (cmdMgr.GetCmd(kBSC_LoopAttack)) {
    return pas::kAS_LoopAttack;
  }
  if (cmdMgr.GetCmd(kBSC_LoopReaction)) {
    return pas::kAS_LoopReaction;
  }
  if (cmdMgr.GetCmd(kBSC_Jump)) {
    return pas::kAS_Jump;
  }
  if (cmdMgr.GetCmd(kBSC_Step)) {
    return pas::kAS_Step;
  }
  if (cmdMgr.GetCmd(kBSC_Scripted)) {
    return pas::kAS_Scripted;
  }
  if (bc.IsAnimationOver() || FacingDest(bc) || cmdMgr.GetMoveVector().IsNonZero()) {
    return pas::kAS_Locomotion;
  }
  return pas::kAS_Invalid;
}

CBSFlyerTurn::CBSFlyerTurn() {}

void CBSFlyerTurn::Start(CBodyController& bc, CStateManager& mgr) {
  const CPASDatabase& db = bc.GetPASDatabase();
  if (db.GetAnimState(pas::kAS_Turn)->HasAnims()) {
    CBSTurn::Start(bc, mgr);
  } else {
    mDest = bc.GetCommandMgr().GetFaceVector().ToVec2f();

    const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
    const CVector2f lookDir = fwd.ToVec2f();
    const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
    mTurnDir = CVector2f::Dot(leftDir, mDest) > 0.f ? pas::kTD_Left : pas::kTD_Right;

    const CPASAnimParmData parms(pas::kAS_Locomotion, CPASAnimParm::FromEnum(pas::kLA_Idle),
                                 CPASAnimParm::FromEnum(bc.GetLocomotionType()));
    const rstl::pair< float, int > best = db.FindBestAnimation(parms, *mgr.Random(), -1);
    if (best.second != bc.GetCurrentAnimId()) {
      bc.SetCurrentAnimation(CAnimPlaybackParms(best.second, -1, 1.f, true), true, false);
    }
  }
}

pas::EAnimationState CBSFlyerTurn::UpdateBody(float dt, CBodyController& bc, CStateManager& mgr) {
  pas::EAnimationState st;
  if (bc.GetPASDatabase().GetAnimState(pas::kAS_Turn)->HasAnims()) {
    st = CBSTurn::UpdateBody(dt, bc, mgr);
  } else {
    st = GetBodyStateTransition(dt, bc);
    if (st == pas::kAS_Invalid) {
      CBodyStateCmdMgr& cmdMgr = bc.CommandMgr();
      CVector3f faceVec = cmdMgr.GetFaceVector();
      if (faceVec.IsNonZero()) {
        mDest = faceVec.ToVec2f();

        const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
        const CVector2f lookDir = fwd.ToVec2f();
        const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
        mTurnDir = CVector2f::Dot(leftDir, mDest) > 0.f ? pas::kTD_Left : pas::kTD_Right;
      }
      bc.FaceDirection(CVector3f(mDest, 0.f), dt);
    }
  }
  return st;
}

CBSPitchableFlyerTurn::CBSPitchableFlyerTurn() : mFaceDirection(CVector3f::Zero()) {}

void CBSPitchableFlyerTurn::Start(CBodyController& bc, CStateManager& mgr) {
  const CPASDatabase& db = bc.GetPASDatabase();
  if (db.GetAnimState(pas::kAS_Turn)->HasAnims()) {
    CBSTurn::Start(bc, mgr);
  } else {
    mDest = bc.GetCommandMgr().GetFaceVector().ToVec2f();

    const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
    const CVector2f lookDir = fwd.ToVec2f();
    const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
    mTurnDir = CVector2f::Dot(leftDir, mDest) > 0.f ? pas::kTD_Left : pas::kTD_Right;

    const CPASAnimParmData parms(pas::kAS_Locomotion, CPASAnimParm::FromEnum(pas::kLA_Idle),
                                 CPASAnimParm::FromEnum(bc.GetLocomotionType()));
    const rstl::pair< float, int > best = db.FindBestAnimation(parms, *mgr.Random(), -1);
    if (best.second != bc.GetCurrentAnimId()) {
      bc.SetCurrentAnimation(CAnimPlaybackParms(best.second, -1, 1.f, true), true, false);
    }
  }
}

pas::EAnimationState CBSPitchableFlyerTurn::UpdateBody(float dt, CBodyController& bc,
                                                        CStateManager& mgr) {
  pas::EAnimationState st;
  if (bc.GetPASDatabase().GetAnimState(pas::kAS_Turn)->HasAnims()) {
    st = CBSTurn::UpdateBody(dt, bc, mgr);
  } else {
    st = GetBodyStateTransition(dt, bc);
    if (st == pas::kAS_Invalid) {
      CBodyStateCmdMgr& cmdMgr = bc.CommandMgr();
      CVector3f faceVec = cmdMgr.GetFaceVector();
      if (faceVec.IsNonZero()) {
        mFaceDirection = faceVec;

        const CVector3f fwd = bc.GetOwner().GetTransform().GetForward();
        const CVector2f lookDir = fwd.ToVec2f();
        const CVector2f leftDir = CVector2f(lookDir[1], -lookDir[0]);
        mTurnDir = CVector2f::Dot(leftDir, mDest) > 0.f ? pas::kTD_Left : pas::kTD_Right;
      }

      if (const CPhysicsActor* actor = TCastToPtr< CPhysicsActor >(&bc.GetOwner())) {
        const CVector3f forward = actor->GetTransform().GetForward();
        const CVector3f flatForward = forward.DropZ().AsNormalized();
        const CVector3f flatFace = faceVec.DropZ();
        bc.FaceDirection3D(flatFace, flatForward, dt);

        CVector3f pitchDirection(forward.GetX(), forward.GetY(), faceVec.GetZ());
        pitchDirection.Normalize();
        if (!close_enough(flatForward, pitchDirection)) {
          const float angle = rstl::min_val(CVector3f::GetAngleDiff(faceVec, flatFace),
                                            bc.GetBodyStateInfo().GetMaximumPitch());
          pitchDirection =
              CVector3f::Slerp(flatForward, pitchDirection, CRelAngle::FromRadians(angle));
        }
        bc.FaceDirection3D(pitchDirection, forward, dt);

        const CVector3f right = actor->GetTransform().GetRight();
        bc.FaceDirection3D(right.DropZ(), right, dt);
      }
    }
  }
  return st;
}
