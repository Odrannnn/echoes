#include "MetroidPrime/Enemies/CPatterned.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/PathFinding/CPathFindSearch.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"

void CPatterned::Start(CStateManager&, EStateMsg, float) {}

void CPatterned::Patrol(CStateManager& mgr, EStateMsg msg, float dt) {
  mWaypointNavigation.Patrol(mgr, msg, dt, *this);
}

void CPatterned::Dead(CStateManager& mgr, EStateMsg msg, float) {
  switch (msg) {
  case kStateMsg_Update:
    mBodyController->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_Die));
    if (!mFadeToDeath) {
      const CBodyStateInfo& bodyState = mBodyController->GetBodyStateInfo();
      if (bodyState.GetCurrentState()->IsDead()) {
        mFadeToDeath = true;
        mAlphaDelta = -1.f / GetFadeOnDeathTime();
        RemoveMaterial(kMT_Character, kMT_Unknown59, kMT_Target, kMT_Orbit, mgr);
        AddMaterial(kMT_NoPlatformCollision, mgr);
      }
    }
    break;
  default:
    break;
  }
}

float CPatterned::GetFadeOnDeathTime() const { return mFadeOnDeathTime; }

void CPatterned::PathFind(CStateManager& mgr, EStateMsg msg, float) {
  if (GetSearchPath()) {
    switch (msg) {
    case kStateMsg_Activate:
      fn_801524fc(mgr);
      break;
    case kStateMsg_Update:
      if (!GetSearchPath()->IsOver()) {
        if (mVerticalMovement || mOnGround) {
          mPathOverCount += 1;
          mPathOverCount &= 3;
        }
        CVector3f position = GetTranslation() + 0.3f * CVector3f::Up();
        mReflectedDestPos = position - (mDestPos - position);
        ApproachDest(mgr);
        CVector3f point = position + GetModelData()->GetScale().GetY() * GetTransform().GetForward();
        GetSearchPath()->GetSplinePointWithLookahead(
            point, position, skActorApproachDistance * GetModelData()->GetScale().GetY());
        SetDestPos(point);
        if (GetSearchPath()->SegmentOver(position)) {
          GetSearchPath()->Advance();
        }
      }
      break;
    default:
      break;
    }
  }
}

void CPatterned::fn_801524fc(CStateManager& mgr) {
  if (GetSearchPath()->Search(GetTranslation(), mDestPos) == CPathFindSearch::kR_Success) {
    mReflectedDestPos = GetTranslation();
    SetDestPos(GetSearchPath()->GetPoint());
    mInPosition = false;
    ApproachDest(mgr);
  }
}

bool CPatterned::OffLine(CStateManager&, const CTriggerData& data) const {
  CVector3f curLine = GetTranslation() - mReflectedDestPos;
  CVector3f pathLine = mDestPos - mReflectedDestPos;
  float distance = 0.f;
  if (CVector3f::Dot(pathLine, curLine) <= 0.f) {
    distance = curLine.MagSquared();
  } else {
    pathLine.Normalize();
    curLine -= CVector3f::Dot(pathLine, curLine) * pathLine;
    distance = curLine.MagSquared();
    const CVector3f delta = GetTranslation() - mDestPos;
    if (CVector3f::Dot(pathLine, delta) > 0.f) {
      distance = delta.MagSquared();
    }
  }
  const float arg = data.GetFloat();
  return distance > arg * arg;
}

bool CPatterned::InRange(CStateManager& mgr, const CTriggerData&) const {
  const float distance = (mgr.GetPlayer(0)->GetTranslation() - GetTranslation()).MagSquared();
  const float range = 0.5f * (mMinAttackRange + mMaxAttackRange);
  return distance < range * range;
}

bool CPatterned::TooClose(CStateManager& mgr, const CTriggerData&) const {
  return (mgr.GetPlayer(0)->GetTranslation() - GetTranslation()).MagSquared() <
         mMinAttackRange * mMinAttackRange;
}

bool CPatterned::InMaxRange(CStateManager& mgr, const CTriggerData&) const {
  return (mgr.GetPlayer(0)->GetTranslation() - GetTranslation()).MagSquared() <
         mMaxAttackRange * mMaxAttackRange;
}

bool CPatterned::InDetectionRange(CStateManager& mgr, const CTriggerData&) const {
  const float heightRange = mDetectionHeightRange;
  const float range = mDetectionRange;
  const float heightRangeSq = heightRange * heightRange;
  const float rangeSq = range * range;
  const CVector3f translation = GetTranslation();
  for (int i = 0; i < static_cast< uint >(mgr.GetNumPlayers()); ++i) {
    const CVector3f delta = mgr.GetPlayer(i)->GetTranslation() - translation;
    if (delta.MagSquared() < rangeSq) {
      if (heightRange > 0.f) {
        if (delta.GetZ() * delta.GetZ() < heightRangeSq) {
          return true;
        }
      } else {
        return true;
      }
    }
  }
  return false;
}

bool CPatterned::Leash(CStateManager&, const CTriggerData&) const {
  bool result = mCurPlayerLeashTime > mPlayerLeashTime;
  if (result) {
    const float distance = (mLatestLeashPosition - GetTranslation()).MagSquared();
    result = result && distance > mLeashRadius * mLeashRadius;
  }
  return result;
}

bool CPatterned::SpotPlayer(CStateManager& mgr, const CTriggerData&) const {
  const CVector3f eye = GetGunEyePos();
  const CVector3f forward = GetTransform().GetForward();
  for (int i = 0; i < static_cast< uint >(mgr.GetNumPlayers()); ++i) {
    const CVector3f delta = mgr.GetPlayer(i)->GetAimPosition(mgr, 0.f) - eye;
    const float forwardDistance = CVector3f::Dot(delta, forward);
    if (forwardDistance > 0.f) {
      const float magSquared = delta.MagSquared();
      if (forwardDistance * forwardDistance > magSquared * mDetectionAngle) {
        return true;
      }
    }
  }
  return false;
}

bool CPatterned::IsOnScreen(const CStateManager& mgr) const {
  const CVector3f center = GetBoundingBox().GetCenterPoint();
  const CFirstPersonCamera* camera =
      const_cast< CCameraManager* >(mgr.GetCameraManager(0))->FirstPersonCamera();
  const CVector3f screen = camera->ConvertToScreenSpace(center);
  return screen.GetZ() > 0.f && screen.GetX() * screen.GetX() < 1.f &&
         screen.GetY() * screen.GetY() < 1.f;
}

bool CPatterned::PlayerSpot(CStateManager&, const CTriggerData&) const {
  // TODO: Check the first player's morph state, screen projection and visibility ray.
  return false;
}

bool CPatterned::Landed(CStateManager&, const CTriggerData&) const {
  bool result = false;
  const bool onGround = mOnGround;
  if (onGround) {
    if (!mPrevOnGround) {
      result = true;
    }
  }
  mPrevOnGround = onGround;
  return result;
}

bool CPatterned::PathOver(CStateManager&, const CTriggerData&) const {
  // GetSearchPath() is a non-const virtual in retail too; the trigger functions are const.
  if (const_cast< CPatterned* >(this)->GetSearchPath() && (mVerticalMovement || mOnGround)) {
    bool result = false;
    if (!const_cast< CPatterned* >(this)->GetSearchPath()->IsShagged()) {
      if (const_cast< CPatterned* >(this)->GetSearchPath()->IsOver()) {
        result = true;
      }
    }
    return result;
  }
  return false;
}

bool CPatterned::PathFound(CStateManager&, const CTriggerData&) const {
  // GetSearchPath() is a non-const virtual in retail too; the trigger functions are const.
  bool result = false;
  if (const_cast< CPatterned* >(this)->GetSearchPath()) {
    if (!const_cast< CPatterned* >(this)->GetSearchPath()->IsShagged()) {
      result = true;
    }
  }
  return result;
}

bool CPatterned::PathShagged(CStateManager&, const CTriggerData&) const {
  // GetSearchPath() is a non-const virtual in retail too; the trigger functions are const.
  if (const_cast< CPatterned* >(this)->GetSearchPath()) {
    if (const_cast< CPatterned* >(this)->GetSearchPath()->IsShagged()) {
      return true;
    }
    if (const_cast< CPatterned* >(this)->GetSearchPath()->GetCurrentWaypoint() > 0 &&
        mPathOverCount == 0) {
      const CVector3f original = GetTranslation() + 0.3f * CVector3f::Up();
      CVector3f point = original;
      const_cast< CPatterned* >(this)->GetSearchPath()->GetSplinePoint(point, GetTranslation());
      if ((point - original).MagSquared() >
          4.f * skActorApproachDistance * skActorApproachDistance) {
        return true;
      }
    }
  }
  return false;
}

bool CPatterned::NoPathNodes(CStateManager&, const CTriggerData&) const {
  // GetSearchPath() is a non-const virtual in retail too; the trigger functions are const.
  if (const_cast< CPatterned* >(this)->GetSearchPath()) {
    return const_cast< CPatterned* >(this)->GetSearchPath()->OnPath(GetTranslation()) !=
           CPathFindSearch::kR_Success;
  }
  return true;
}

bool CPatterned::Attacked(CStateManager&, const CTriggerData&) const {
  return mHitByPlayerProjectile;
}

bool CPatterned::HasPatrolPath(CStateManager& mgr, const CTriggerData&) const {
  return GetConnectedObject(mgr, kSS_Patrol, kSM_Follow) != kInvalidUniqueId;
}

bool CPatterned::InPosition(CStateManager&, const CTriggerData&) const { return mInPosition; }

bool CPatterned::AnimOver(CStateManager& mgr, const CTriggerData& data) const {
  return GetAnimOver(mgr, data);
}

bool CPatterned::GetAnimOver(CStateManager&, const CTriggerData&) const {
  return mAnimationState.IsOver();
}

bool CPatterned::Stuck(CStateManager&, const CTriggerData&) const {
  return mPredictedLeashTime > 0.2f;
}

bool CPatterned::Delay(CStateManager&, const CTriggerData& data) const {
  return mStateMachine->GetTime() > data.GetFloat();
}

bool CPatterned::RandomDelay(CStateManager&, const CTriggerData& data) const {
  const TStateMachineState< CPatterned >& state =
      static_cast< const TStateMachineState< CPatterned >& >(*mStateMachine);
  return state.GetTime() > data.GetFloat() * state.GetRandom();
}

bool CPatterned::FixedDelay(CStateManager&, const CTriggerData&) const {
  const TStateMachineState< CPatterned >& state =
      static_cast< const TStateMachineState< CPatterned >& >(*mStateMachine);
  return state.GetTime() > state.GetDelay();
}

bool CPatterned::CodeTrigger(CStateManager&, const CTriggerData&) const {
  return static_cast< const TStateMachineState< CPatterned >& >(*mStateMachine).GetCodeTrigger();
}

bool CPatterned::Random(CStateManager&, const CTriggerData& data) const {
  return static_cast< const TStateMachineState< CPatterned >& >(*mStateMachine).GetRandom() <
         data.GetFloat();
}

bool CPatterned::FixedRandom(CStateManager&, const CTriggerData&) const {
  const TStateMachineState< CPatterned >& state =
      static_cast< const TStateMachineState< CPatterned >& >(*mStateMachine);
  return state.GetRandom() < state.GetFixedRandom();
}

void CPatterned::SetAttackTarget(CStateManager&, TUniqueId) {}

void CPatterned::ApproachDest(CStateManager&) {
  // TODO: Choose locomotion/step commands using the destination segment and body type.
}

TUniqueId CPatterned::GetConnectedObject(CStateManager& mgr, EScriptObjectState state,
                                         EScriptObjectMessage msg) const {
  rstl::reserved_vector< TUniqueId, 8 > ids;
  const rstl::vector< SConnection >& connections = GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = connections.begin();
       it != connections.end(); ++it) {
    if (it->state == state && it->msg == msg) {
      const TUniqueId id = mgr.GetIdForScript(it->objId);
      if (const CEntity* entity = mgr.GetObjectById(id)) {
        if (entity->GetActive()) {
          ids.push_back(id);
          if (ids.capacity() - ids.size() <= 0) {
            break;
          }
        }
      }
    }
  }
  if (ids.size() != 0) {
    return ids[mgr.Random()->Next() % ids.size()];
  }
  return kInvalidUniqueId;
}

pas::EStepDirection CPatterned::FindBestStepDirection(const CVector3f& direction) const {
  const CVector3f local = GetTransform().TransposeRotate(direction);
  const float angle = CVector3f::GetAngleDiff(local, CVector3f::Forward());
  if (angle < CMath::Deg2Rad(45.f)) {
    return pas::kSD_Forward;
  }
  if (angle > CMath::Deg2Rad(135.f)) {
    return pas::kSD_Backward;
  }
  return CVector3f::Dot(local, CVector3f::Right()) > 0.f ? pas::kSD_Right : pas::kSD_Left;
}

void CPatterned::RotateToPoint(const CVector3f& position, float dt, float turnSpeed) {
  if (dt <= 0.f || mBodyController->GetTimeScale() == 0.f) {
    return;
  }
  CVector3f dir = GetTransform().GetForward();
  dir.SetZ(0.f);
  if (!dir.CanBeNormalized()) {
    return;
  }
  dir.Normalize();
  CVector3f to = position - GetTranslation();
  to.SetZ(0.f);
  if (!to.CanBeNormalized()) {
    return;
  }
  to.Normalize();
  const CRelAngle max =
      CRelAngle::FromRadians(dt * turnSpeed * mBodyController->GetTimeScale());
  const CQuaternion rotation = CQuaternion::ShortestRotationArcClamped(dir, to, max);
  RotateInOneFrameOR(rotation, dt);
}

void CPatterned::ApplyScreenShake(CStateManager&, const CVector3f&, TUniqueId) {
  // TODO: Resolve the shaker (or Footstep/Attach connection), copy its shake and set its position.
}
