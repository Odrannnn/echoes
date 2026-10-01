#include "MetroidPrime/ScriptObjects/CScriptActorRotate.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"
#include "MetroidPrime/TCastTo.hpp"

// Retail's `vector<pair<TUniqueId, CTransform4f>>::clear` (0x8010A630) is three instructions - it
// stores 0 to the count and returns, with no per-element destroy loop - and the vector destructor
// (0x80109F34) frees the block without one either. Neither member has a destructor to run, so the
// pair is trivially destructible; without this specialization `destroy(begin(), end())` emits a
// 52-byte-stride loop that retail does not have.
namespace rstl {
template <>
struct is_trivially_destructible< pair< TUniqueId, CTransform4f > > {
  enum { value = true };
};
} // namespace rstl

CScriptActorRotate::~CScriptActorRotate() {}

void CScriptActorRotate::StopRotation() { mPlaying = false; }

void CScriptActorRotate::StartRotation() { mPlaying = true; }

void CScriptActorRotate::SetCurrentTime(float time) {
  mCurrentTime = CMath::Clamp(0.f, time, mDuration);
}

void CScriptActorRotate::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  bool accepted = false;
  switch (message) {
  case kSM_Activate:
    CEntity::AcceptScriptMsg(mgr, msg);
    accepted = true;
  case kSM_XALD:
    mTargetId = FindConnectedObject(mgr, kSS_Connect, kSM_Attach);
    if ((mFlags & kF_AutoStart) != 0 && GetActive()) {
      if (TCastToPtr< CScriptActorRotate >(const_cast< CEntity* >(mgr.GetObjectById(mTargetId))) == nullptr) {
        UpdateActors(message == kSM_Next, mgr);
        break;
      }
      StartRotation();
      mCurrentTime = 0.f;
    }
    break;
  case kSM_Action:
  case kSM_Next:
    if (GetActive()) {
      if (TCastToPtr< CScriptActorRotate >(const_cast< CEntity* >(mgr.GetObjectById(mTargetId))) == nullptr) {
        UpdateActors(message == kSM_Next, mgr);
        break;
      }
      StartRotation();
      mCurrentTime = 0.f;
    }
    break;
  case kSM_Start:
    StartRotation();
    break;
  case kSM_Stop:
    StopRotation();
    break;
  case kSM_Deactivate: {
    const rstl::vector< TUniqueId > ids = FindConnectedObjects(mgr, kSS_Play, kSM_Play);
    for (int i = 0; i < ids.size(); ++i) {
      if (CScriptPlatform* plat = TCastToPtr< CScriptPlatform >(mgr.ObjectById(ids[i]))) {
        plat->SetRotateController(kInvalidUniqueId);
      }
    }
    StopRotation();
    break;
  }
  default:
    break;
  }
  if (!accepted) {
    CEntity::AcceptScriptMsg(mgr, msg);
  }
}

void CScriptActorRotate::UpdateActors(bool next, CStateManager& mgr) {
  if (mPlaying) {
    return;
  }

  mActors.clear();
  const rstl::vector< TUniqueId > ids = FindConnectedObjects(mgr, kSS_Play, kSM_Play);
  mActors.reserve(ids.size());
  for (int i = 0; i < ids.size(); ++i) {
    if (CActor* act = TCastToPtr< CActor >(mgr.ObjectById(ids[i]))) {
      mActors.push_back(
          rstl::pair< TUniqueId, CTransform4f >(act->GetUniqueId(), act->GetTransform().GetRotation()));
    }
    if (CScriptPlatform* plat = TCastToPtr< CScriptPlatform >(mgr.ObjectById(ids[i]))) {
      plat->SetRotateController(GetUniqueId());
    }
  }

  SendScriptMsgs(kSS_Play, mgr, kInvalidUniqueId, kSM_None);
  if (!mActors.empty()) {
    StartRotation();
    mCurrentTime = next ? mDuration : 0.f;
  }
}

void CScriptActorRotate::Think(float dt, CStateManager& mgr) {
  if (!mPlaying || !GetActive()) {
    return;
  }

  if ((mFlags & kF_AdvanceTime) != 0 && (mFlags & kF_ExternalTime) == 0) {
    mCurrentTime += dt;
  }

  if (TCastToPtr< CScriptActorRotate >(mgr.ObjectById(mTargetId)) != nullptr) {
    UpdateTargetRotation(mgr);
  } else {
    UpdateActorRotations(dt, mgr);
  }
}

void CScriptActorRotate::UpdateTargetRotation(CStateManager& mgr) {
  CheckEnd(mgr);
  CEntity* entity = mgr.GetObjectByIdFromListAll(mTargetId);
  if (entity == nullptr) {
    return;
  }
  CScriptActorRotate* target =
      static_cast< CScriptActorRotate* >(entity->TypesMatch(kET_ScriptActorRotate));
  if (target == nullptr) {
    return;
  }

  const CTransform4f rotation =
      CTransform4f::RotateZ(CRelAngle::FromDegrees(mZRotation.EvaluateAt(mCurrentTime))) *
      CTransform4f::RotateY(CRelAngle::FromDegrees(mYRotation.EvaluateAt(mCurrentTime))) *
      CTransform4f::RotateX(CRelAngle::FromDegrees(mXRotation.EvaluateAt(mCurrentTime)));
  target->SetActorTransforms(rotation);
}

void CScriptActorRotate::SetActorTransforms(const CTransform4f& rotation) {
  for (rstl::vector< rstl::pair< TUniqueId, CTransform4f > >::iterator it = mActors.begin();
       it != mActors.end(); ++it) {
    it->second = rotation;
  }
}

void CScriptActorRotate::UpdateActorRotations(float dt, CStateManager& mgr) {
  CheckEnd(mgr);
  // TODO: apply the sampled rotation and scale splines to each connected actor. The
  // transform composition and platform-specific path still need target verification.
}

void CScriptActorRotate::CheckEnd(CStateManager& mgr) {
  if (mCurrentTime >= mDuration) {
    SendScriptMsgs(kSS_Zero, mgr, kInvalidUniqueId, kSM_None);
    if ((mFlags & kF_Loop) != 0) {
      mCurrentTime -= mDuration;
    } else {
      StopRotation();
      mCurrentTime = mDuration;
    }
  }
}

CScriptActorRotate::CScriptActorRotate(TUniqueId uid, const rstl::string& name,
                                       const CEntityInfo& info, uint flags,
                                       const SLdrSpline& xRotation, const SLdrSpline& yRotation,
                                       const SLdrSpline& zRotation, const SLdrSpline& xScale,
                                       const SLdrSpline& yScale, const SLdrSpline& zScale,
                                       float duration)
: CEntity(uid, info, name, 0)
, mDuration(duration)
, mXRotation(xRotation)
, mYRotation(yRotation)
, mZRotation(zRotation)
, mXScale(xScale)
, mYScale(yScale)
, mZScale(zScale)
, mFlags(flags)
, mCurrentTime(0.f)
, mCurrentTransform(CTransform4f::Identity())
, mActors()
, mTargetId(kInvalidUniqueId)
, mPlaying(false) {
  if ((mFlags & kF_DurationFromSplines) != 0) {
    mDuration = mXRotation.GetMaxTime();
    float maxTime = mYRotation.GetMaxTime();
    if (maxTime > mDuration) {
      mDuration = maxTime;
    }
    maxTime = mZRotation.GetMaxTime();
    if (maxTime > mDuration) {
      mDuration = maxTime;
    }
    maxTime = mXScale.GetMaxTime();
    if (maxTime > mDuration) {
      mDuration = maxTime;
    }
    maxTime = mYScale.GetMaxTime();
    if (maxTime > mDuration) {
      mDuration = maxTime;
    }
    maxTime = mZScale.GetMaxTime();
    if (maxTime > mDuration) {
      mDuration = maxTime;
    }
  }
}
