#include "MetroidPrime/ScriptObjects/CScriptColorModulate.hpp"

#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "math.h"
#include "rstl/math.hpp"

CScriptColorModulate::CScriptColorModulate(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CColor& colorA,
    const CColor& colorB, EBlendMode blendMode, float timeA2B, float timeB2A, bool doReverse,
    bool resetTargetWhenDone, bool depthCompare, bool depthUpdate, bool depthBackwards,
    bool autoStart, bool updateTime, bool loopForever, bool externalTime,
    bool copyModelColorToColorA, const SLdrSpline& controlSpline)
: CEntity(uid, info, name, 0)
, mParent(kInvalidUniqueId)
, mFadeState(kFS_AtoB)
, mCurTime(0.f)
, mColorA(colorA)
, mColorB(colorB)
, mBlendMode(blendMode)
, mTimeA2B(timeA2B)
, mTimeB2A(timeB2A)
, mControlSpline(controlSpline)
, mDoReverse(doReverse)
, mResetTargetWhenDone(resetTargetWhenDone)
, mDepthCompare(depthCompare)
, mDepthUpdate(depthUpdate)
, mDepthBackwards(depthBackwards)
, mReversing(false)
, mEnable(false)
, mDieOnEnd(false)
, mIsFadeOutHelper(false)
, mUpdateTime(updateTime)
, mAutoStart(autoStart)
, mLoopForever(loopForever)
, mExternalTime(externalTime)
, mCopyModelColorToColorA(copyModelColorToColorA) {}

TUniqueId CScriptColorModulate::FadeInHelper(CStateManager& mgr, TUniqueId obj, float fadeTime) {
  const CEntity* entity = mgr.GetObjectById(obj);
  const TAreaId area = entity ? entity->GetCurrentAreaId() : mgr.GetNextAreaId();
  const CActor* actor = TCastToConstPtr< CActor >(entity);
  const CModelFlags flags = actor ? actor->GetModelFlags() : CModelFlags::Normal();
  const uint depthFlags = flags.GetOtherFlags();
  const TUniqueId uid = mgr.AllocateUniqueId();
  CScriptColorModulate* mod = rs_new CScriptColorModulate(
      uid, rstl::string(), CEntityInfo(area, NullConnectionList, true), CColor(1.f, 1.f, 1.f, 0.f),
      CColor::White(), kBM_Alpha, fadeTime, 0.f, false, true,
      (depthFlags & CModelFlags::kF_DepthCompare) != 0,
      (depthFlags & CModelFlags::kF_DepthUpdate) != 0,
      (depthFlags & CModelFlags::kF_DepthGreater) != 0, true, true, false, false, false,
      SLdrSpline());
  mod->mParent = obj;
  mod->mEnable = true;
  mod->mDieOnEnd = true;
  mgr.AddObject(mod);
  mod->Think(0.f, mgr);
  return uid;
}

TUniqueId CScriptColorModulate::FadeOutHelper(CStateManager& mgr, TUniqueId obj, float fadeTime) {
  const CEntity* entity = mgr.GetObjectById(obj);
  const TAreaId area = entity ? entity->GetCurrentAreaId() : mgr.GetNextAreaId();
  const CActor* actor = TCastToConstPtr< CActor >(entity);
  const CModelFlags flags = actor ? actor->GetModelFlags() : CModelFlags::Normal();
  const uint depthFlags = flags.GetOtherFlags();
  const TUniqueId uid = mgr.AllocateUniqueId();
  CScriptColorModulate* mod = rs_new CScriptColorModulate(
      uid, rstl::string(), CEntityInfo(area, NullConnectionList, true), CColor::White(),
      CColor(1.f, 1.f, 1.f, 0.f), kBM_Alpha, fadeTime, 0.f, false, true,
      (depthFlags & CModelFlags::kF_DepthCompare) != 0,
      (depthFlags & CModelFlags::kF_DepthUpdate) != 0,
      (depthFlags & CModelFlags::kF_DepthGreater) != 0, true, true, true, false, false,
      SLdrSpline());
  mod->mParent = obj;
  mod->mEnable = true;
  mod->mDieOnEnd = true;
  mod->mIsFadeOutHelper = true;
  mgr.AddObject(mod);
  mod->Think(0.f, mgr);
  return uid;
}

void CScriptColorModulate::SetTargetFlags(CStateManager& mgr, const CModelFlags& flags) {
  for (rstl::vector< SConnection >::const_iterator it = GetConnectionList().begin();
       it != GetConnectionList().end(); ++it) {
    if (it->state != kSS_Play || it->msg != kSM_Activate) {
      continue;
    }
    const CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
    for (CStateManager::TIdList::const_iterator id = ids.first; id != ids.second; ++id) {
      if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(id->second))) {
        actor->SetModelFlags(flags);
      }
    }
  }
  if (mParent != kInvalidUniqueId) {
    if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(mParent))) {
      actor->SetModelFlags(flags);
    }
  }
}

void CScriptColorModulate::End(CStateManager& mgr) {
  bool done = false;
  if (mControlSpline.GetKnots().empty()) {
    if (mDoReverse && !mReversing) {
      mReversing = true;
      mFadeState = mFadeState == kFS_AtoB ? kFS_BtoA : kFS_AtoB;
    } else {
      done = true;
    }
  } else {
    if (mLoopForever) {
      mCurTime -= mControlSpline.GetMaxTime();
      return;
    }
    done = true;
  }
  mCurTime = 0.f;
  if (!done) {
    return;
  }
  mEnable = false;
  mReversing = false;
  if (mResetTargetWhenDone) {
    CModelFlags flags = CModelFlags::Normal().DepthCompareUpdate(mDepthCompare, mDepthUpdate);
    if (mDepthBackwards) {
      flags = CModelFlags(flags, flags.GetOtherFlags() | CModelFlags::kF_DepthGreater |
                                     CModelFlags::kF_Unknown200);
    }
    SetTargetFlags(mgr, flags);
  }
  if (mIsFadeOutHelper) {
    mgr.SendScriptMsg(
        CScriptMsg(GetUniqueId(), kInvalidUniqueId, mParent, kSM_Deactivate, kSS_InvalidState));
  }
  SendScriptMsgs(kSS_MaxReached, mgr, kInvalidUniqueId, kSM_None);
  if (mDieOnEnd) {
    mgr.DeleteObjectRequest(GetUniqueId());
  }
}

CModelFlags CScriptColorModulate::CalculateFlags(const CColor& color) const {
  CModelFlags::ETrans trans;
  switch (mBlendMode) {
  case kBM_Alpha:
    trans = !mDepthBackwards && color == CColor::White() ? CModelFlags::kT_Opaque
                                                         : CModelFlags::kT_Blend;
    break;
  case kBM_Additive:
    trans = CModelFlags::kT_Additive;
    break;
  case kBM_Additive2:
    trans = CModelFlags::kT_Additive2;
    break;
  case kBM_Opaque:
    trans =
        !mDepthBackwards && color == CColor::White() ? CModelFlags::kT_Opaque : CModelFlags::kT_One;
    break;
  case kBM_OpaqueAdd:
    trans = CModelFlags::kT_Two;
    break;
  default:
    return CModelFlags::Normal();
  }
  CModelFlags flags = CModelFlags(trans, color).DepthCompareUpdate(mDepthCompare, mDepthUpdate);
  if (mDepthBackwards) {
    flags = CModelFlags(flags, flags.GetOtherFlags() | CModelFlags::kF_DepthGreater |
                                   CModelFlags::kF_Unknown200);
  }
  return flags;
}

void CScriptColorModulate::Think(float dt, CStateManager& mgr) {
  if (!GetActive() || !mEnable) {
    return;
  }
  // The outer `mEnable` test is redundant - the guard above already returned - and it is
  // deliberate. Retail carries a second, provably untaken test of mEnable here (asm
  // .../CScriptColorModulate.s:805, a `beq` whose condition is the `extrwi` from :801), and
  // this nesting is the only spelling of the guard that makes MWCC emit it. With the inner
  // test written as a plain `if (mUpdateTime && !mExternalTime)` Think scores 99.31 and is
  // 4 bytes short; with the redundant `if (mEnable)` around it, Think matches retail exactly.
  if (mEnable) {
    if (mUpdateTime && !mExternalTime) {
      mCurTime += dt;
    }
  }
  if (mControlSpline.GetKnots().empty()) {
    switch (mFadeState) {
    case kFS_AtoB: {
      const float t = close_enough(mTimeA2B, 0.f) ? 1.f : rstl::min_val(1.f, mCurTime / mTimeA2B);
      // Named local on purpose: passing `CColor::Lerp(...)` straight into CalculateFlags makes
      // MWCC forward the temporary's own address, but retail materialises the Lerp result into a
      // second stack slot and passes that (asm :852 `lwz 0x10(r1)` / :856 `stw 0x1c(r1)`, then
      // `addi r5, r1, 0x1c`). The spline arm below has always had the local and already matched.
      const CColor color = CColor::Lerp(mColorA, mColorB, t);
      SetTargetFlags(mgr, CalculateFlags(color));
      if (mCurTime > mTimeA2B) {
        End(mgr);
      }
      break;
    }
    case kFS_BtoA: {
      const float t = close_enough(mTimeB2A, 0.f) ? 1.f : rstl::min_val(1.f, mCurTime / mTimeB2A);
      const CColor color = CColor::Lerp(mColorB, mColorA, t);
      SetTargetFlags(mgr, CalculateFlags(color));
      if (mCurTime > mTimeB2A) {
        End(mgr);
      }
      break;
    }
    }
    return;
  }
  const CColor color = CColor::Lerp(mColorA, mColorB, mControlSpline.EvaluateAt(mCurTime));
  SetTargetFlags(mgr, CalculateFlags(color));
  if (mCurTime >= mControlSpline.GetMaxTime()) {
    End(mgr);
  }
}

void CScriptColorModulate::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  CEntity::AcceptScriptMsg(mgr, msg);
  if (!GetActive()) {
    return;
  }
  switch (message) {
  case kSM_Increment:
  case kSM_Decrement: {
    CopyTargetColor(mgr);
    if (mReversing) {
      mFadeState = mFadeState == kFS_AtoB ? kFS_BtoA : kFS_AtoB;
      mReversing = false;
    } else {
      const bool forward = message == kSM_Increment;
      if (mEnable) {
        if (mFadeState == kFS_AtoB) {
          mCurTime = 0.f;
        } else if (forward) {
          mCurTime = mTimeA2B - mTimeA2B * (mCurTime / mTimeB2A);
        } else {
          mCurTime = mTimeB2A - mTimeB2A * (mCurTime / mTimeA2B);
        }
      } else {
        SetTargetFlags(mgr, CalculateFlags(forward ? mColorA : mColorB));
      }
      mEnable = true;
      mFadeState = forward ? kFS_AtoB : kFS_BtoA;
    }
    if ((mFadeState == kFS_AtoB && mTimeA2B == 0.f) ||
        (mFadeState == kFS_BtoA && mTimeB2A == 0.f)) {
      Think(0.f, mgr);
    }
    break;
  }
  case kSM_Start:
    CopyTargetColor(mgr);
    mEnable = true;
    break;
  case kSM_Stop:
    mEnable = false;
    break;
  case kSM_Reset:
    mCurTime = 0.f;
    break;
  case kSM_XALD:
    mEnable = mAutoStart;
    if (mExternalTime) {
      mEnable = true;
    }
    mCurTime = 0.f;
    break;
  }
}

// Guessed name
void CScriptColorModulate::SetExternalTime(float time) {
  if (mExternalTime) {
    if (!mControlSpline.GetKnots().empty()) {
      mCurTime = time;
      return;
    }
    float duration = mTimeA2B;
    if (mFadeState == kFS_BtoA) {
      duration = mTimeB2A;
    }
    mCurTime = fmod(time, duration);
  }
}

// Guessed name
void CScriptColorModulate::CopyTargetColor(CStateManager& mgr) {
  if (mCopyModelColorToColorA) {
    const TUniqueId target = FindConnectedObject(mgr, kSS_Play, kSM_Activate);
    if (target != kInvalidUniqueId) {
      if (const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(target))) {
        mColorA = actor->GetModelFlags().GetColor();
      }
    }
  }
}

CScriptColorModulate::~CScriptColorModulate() {}
