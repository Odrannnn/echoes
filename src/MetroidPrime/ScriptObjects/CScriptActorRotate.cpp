#include "MetroidPrime/ScriptObjects/CScriptActorRotate.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptObjects/CScriptEffect.hpp"
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

// Retail's five max-of-splines steps in the constructor (0x8010B168: `lfs f0,36(r31) / fcmpo
// cr0,f1,f0 / bge / b / fmr f0,f1 / stfs f0,36(r31)`) are not conditional stores but selects that
// store unconditionally, and this is the only spelling measured that emits that phi. In an `if`
// MWCC folds the store to the taken arm (`ble` + `stfs f1`), and a ternary whose arms both call
// `GetMaxTime` calls it twice. Taking `t` by value rather than by reference is what stops the fold;
// the test is `t < duration` because retail's `bge` is `fcmpo cr0,f1,f0` read as
// `maxTime >= duration`.
static inline float MaxDuration(float duration, float t) {
  return t < duration ? duration : t;
}

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
  // `ids.size() > 0` and not `!ids.empty()`: retail's guard is `cmpwi r4,0 / ble` (0x8010A4E4) and
  // jumps to the loop's own condition test, so it covers the reserve and the loop together.
  if (ids.size() > 0) {
    mActors.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
      if (CActor* act = TCastToPtr< CActor >(mgr.ObjectById(ids[i]))) {
        // `push_back_unsafe`, not `push_back`: the reserve above is for every id, so retail has no
        // capacity test and no growth path at the push (0x8010A548 reads `mCount`, bumps it, and
        // stores straight into `mItems + mCount * 52`).
        mActors.push_back_unsafe(rstl::pair< TUniqueId, CTransform4f >(
            act->GetUniqueId(), act->GetTransform().GetRotation()));
      }
      if (CScriptPlatform* plat = TCastToPtr< CScriptPlatform >(mgr.ObjectById(ids[i]))) {
        plat->SetRotateController(GetUniqueId());
      }
    }
  }

  SendScriptMsgs(kSS_Play, mgr, kInvalidUniqueId, kSM_None);
  if (!mActors.empty()) {
    StartRotation();
    // The two-armed `if`, not `next ? mDuration : 0.f`: retail stores in both arms
    // (`lfs f0,36(r26) / stfs f0,452(r26) / b` then `lfs f0,0(0) / stfs f0,452(r26)`, 0x8010A5FC),
    // and the ternary sinks the second store into the merge block instead.
    if (next) {
      mCurrentTime = mDuration;
    } else {
      mCurrentTime = 0.f;
    }
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
  CScriptActorRotate* target = TCastToPtr< CScriptActorRotate >(mgr.ObjectById(mTargetId));
  if (target == nullptr) {
    return;
  }

  // Retail's three angles go through `CMath::ClampRadians`, not `CRelAngle::FromDegrees`: each has
  // the `FastFmod` shape at 0x8010A75C-0x8010A7A4 (`fmuls / fctiwz / stfd / xoris 32768 / lfd /
  // fsubs / fnmsubs / fcmpo / bge / fadds`), which is a modulo into [0, 2pi) and then one
  // conditional add, not the bare `fmuls` a degrees-to-radians conversion emits.
  //
  // The three clamped angles are named `float` locals in X, Y, Z order, and `CRelAngle` is built
  // from them only inside the composition. That is what reproduces retail's frame exactly, and the
  // reason is what each spelling asks the register allocator to do. All three values are live
  // across the three `EvaluateAt` calls, and the first two are dead only after the third, so
  // retail keeps the first two in the callee-saved `f31`/`f30` (0x8010A71C: `xsmsubadp` /
  // `xxsel`, spilled in the prologue) and the third in the volatile `f2`, for a 400-byte frame.
  // `CRelAngle` locals (address taken by `RotateX/Y/Z`) force all three into memory and give
  // 368 bytes and one register; spelled as one nested expression they are re-created per operand,
  // so the store order and the call order come out Z, Y, X (30 differing instructions). A `float`
  // local's address is never taken, so it stays in a register and the three stores are sunk to
  // the block of calls where retail has them.
  const float xr = CMath::ClampRadians(mXRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  const float yr = CMath::ClampRadians(mYRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  const float zr = CMath::ClampRadians(mZRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  const CTransform4f rotation = CTransform4f::RotateZ(CRelAngle::FromRadians(zr)) *
                                 CTransform4f::RotateY(CRelAngle::FromRadians(yr)) *
                                 CTransform4f::RotateX(CRelAngle::FromRadians(xr));
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

  // Same three `ClampRadians` samples as `UpdateTargetRotation`, before the `kF_RateScale` overwrite.
  float xr = CMath::ClampRadians(mXRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  float yr = CMath::ClampRadians(mYRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  float zr = CMath::ClampRadians(mZRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f));
  if ((mFlags & kF_RateScale) != 0) {
    // Retail does not reuse the clamped samples here: it samples each spline again and multiplies
    // the raw degree value by `dt` without clamping (0x8010AAC4-0x8010AB08 has no `FastFmod` block).
    xr = mXRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f) * dt;
    yr = mYRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f) * dt;
    zr = mZRotation.EvaluateAt(mCurrentTime) * (M_PIF / 180.f) * dt;
  }

  const CTransform4f rotation = CTransform4f::RotateZ(CRelAngle::FromRadians(zr)) *
                                 CTransform4f::RotateY(CRelAngle::FromRadians(yr)) *
                                 CTransform4f::RotateX(CRelAngle::FromRadians(xr));

  for (rstl::vector< rstl::pair< TUniqueId, CTransform4f > >::iterator it = mActors.begin();
       it != mActors.end(); ++it) {
    // Retail looks the id up twice: once for the actor and once for the platform (0x8010AB80 and
    // 0x8010ABA0), and skips only when neither resolves.
    CActor* act = TCastToPtr< CActor >(mgr.ObjectById(it->first));
    if (act == nullptr) {
      continue;
    }

    if (CScriptPlatform* plat = TCastToPtr< CScriptPlatform >(mgr.ObjectById(it->first))) {
      // Retail has four separate blocks here (0x8010ABC4, 0x8010ABF8, 0x8010AC34, 0x8010AC68), each
      // with its own product temporary, so the two flags are nested tests rather than a select.
      if ((mFlags & kF_RateScale) != 0) {
        if ((mFlags & kF_FlipOrder) != 0) {
          CTransform4f xf = plat->GetTransform() * rotation;
          xf.Orthonormalize();
          plat->SetTransformExplicitly(xf);
        } else {
          CTransform4f xf = rotation * plat->GetTransform();
          xf.Orthonormalize();
          plat->SetTransformExplicitly(xf);
        }
      } else {
        if ((mFlags & kF_FlipOrder) != 0) {
          CTransform4f xf = it->second * rotation;
          xf.Orthonormalize();
          plat->SetTransformExplicitly(xf);
        } else {
          CTransform4f xf = rotation * it->second;
          xf.Orthonormalize();
          plat->SetTransformExplicitly(xf);
        }
      }
    } else {
      if (!mPlaying) {
        mCurrentTransform = rotation;
      }
      if ((mFlags & kF_RateScale) != 0) {
        if ((mFlags & kF_FlipOrder) != 0) {
          CTransform4f xf = act->GetTransform() * mCurrentTransform;
          xf.Orthonormalize();
          xf.SetTranslation(act->GetTranslation());
          act->SetTransform(xf);
        } else {
          CTransform4f xf = mCurrentTransform * act->GetTransform();
          xf.Orthonormalize();
          xf.SetTranslation(act->GetTranslation());
          act->SetTransform(xf);
        }
      } else {
        if ((mFlags & kF_FlipOrder) != 0) {
          CTransform4f xf = it->second * mCurrentTransform;
          xf.Orthonormalize();
          xf.SetTranslation(act->GetTranslation());
          act->SetTransform(xf);
        } else {
          CTransform4f xf = mCurrentTransform * it->second;
          xf.Orthonormalize();
          xf.SetTranslation(act->GetTranslation());
          act->SetTransform(xf);
        }
      }
    }

    // The scale follows the same two-way dispatch, but the sampled scale splines override it
    // component-wise when they have knots (retail's three `GetKnots()` count tests at
    // 0x8010AE9C/0x8010AEC0/0x8010AEE4).
    CScriptEffect* effect = TCastToPtr< CScriptEffect >(act);
    CVector3f scale(1.f, 1.f, 1.f);
    if (act->HasModelData()) {
      scale = act->ModelData()->GetScale();
    } else if (effect != nullptr) {
      scale = effect->GetGlobalScale();
    }
    if (mXScale.GetKnots().size() != 0) {
      scale.SetX(mXScale.EvaluateAt(mCurrentTime));
    }
    if (mYScale.GetKnots().size() != 0) {
      scale.SetY(mYScale.EvaluateAt(mCurrentTime));
    }
    if (mZScale.GetKnots().size() != 0) {
      scale.SetZ(mZScale.EvaluateAt(mCurrentTime));
    }
    if (act->HasModelData()) {
      act->ModelData()->SetScale(scale);
    } else if (effect != nullptr) {
      effect->SetGlobalScale(scale);
    }
  }

  if (mPlaying) {
    mCurrentTransform = rotation;
  } else {
    mCurrentTransform = CTransform4f::Identity();
  }
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
    mDuration = MaxDuration(mDuration, mYRotation.GetMaxTime());
    mDuration = MaxDuration(mDuration, mZRotation.GetMaxTime());
    mDuration = MaxDuration(mDuration, mXScale.GetMaxTime());
    mDuration = MaxDuration(mDuration, mYScale.GetMaxTime());
    mDuration = MaxDuration(mDuration, mZScale.GetMaxTime());
  }
}
