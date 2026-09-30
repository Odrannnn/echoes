#include "Kyoto/Animation/CAnimTreeTimeScale.hpp"

#include "Kyoto/Animation/CBoolPOINode.hpp"
#include "Kyoto/Animation/CInt32POINode.hpp"
#include "Kyoto/Animation/CParticlePOINode.hpp"
#include "Kyoto/Animation/CSoundPOINode.hpp"

// Retail's unnamed out-of-line IVaryingAnimationTimeScale::Clone, defined at its own offset
// further down. Declared here because VClone and VGetBestUnblendedChild come first in the file.
extern "C" rstl::ownership_transfer< IVaryingAnimationTimeScale >
fn_802A8800(const rstl::object_owner< IVaryingAnimationTimeScale >& timeScale);

// Retail calls weak out-of-line IAnimReader::Clone and ::Simplified rather than inlining the
// vtable dispatch, so the calls go through functions here too. Both are one-line wrappers in
// IAnimReader.hpp with no definition anywhere in this tree, so spelling them directly is an
// undefined reference; these forward to the real virtual. File-static, unlike the same pair in
// CAnimTreeTransition.cpp: mwcceppc still emits them out of line (measured identical, 18/19), and
// file-local keeps a second global `clone_reader` out of the link when both units flip. See
// docs/goal-notes/progress-prime1-canimtreetransition.md.
static rstl::ownership_transfer< IAnimReader > clone_reader(const IAnimReader& reader) {
  return reader.VClone();
}
static rstl::optional_object< rstl::ownership_transfer< IAnimReader > > simplify_reader(
    IAnimReader& reader) {
  return reader.VSimplified();
}

SAdvancementResults CAnimTreeTimeScale::VAdvanceView(const CCharAnimTime& dt) {
  if (dt.EqualsZero() && dt > CCharAnimTime::ZeroFlat()) {
    return mChild->VAdvanceView(dt);
  }

  CCharAnimTime origAccelTime = mCurAccelTime;
  CCharAnimTime newTime = mCurAccelTime + dt;
  if (newTime < mTargetAccelTime) {
    CCharAnimTime integral = mTimeScale->TimeScaleIntegral(origAccelTime, newTime);
    SAdvancementResults res = mChild->VAdvanceView(integral);
    if (res.mRemTime.EqualsZero()) {
      mCurAccelTime = newTime;
      return SAdvancementResults(CCharAnimTime::ZeroFlat(), res.mDeltas);
    } else {
      mCurAccelTime = mTimeScale->FindUpperLimit(origAccelTime, integral - res.mRemTime);
      CCharAnimTime elapsed = mCurAccelTime - origAccelTime;
      CCharAnimTime remaining = dt - elapsed;
      return SAdvancementResults(remaining, res.mDeltas);
    }
  } else {
    CCharAnimTime newDt = mTimeScale->TimeScaleIntegral(origAccelTime, mTargetAccelTime);
    // Prime 1 spells this SAdvancementDeltas(), whose default constructor initialises from
    // sZeroVector/sNoRotation. This repo's SAdvancementDeltas() is `{}` - CVector3f() and
    // CQuaternion() are both empty - so it leaves the deltas uninitialised and mwcceppc emits it
    // as an out-of-line call. Retail copies the seven floats in place, so name the two members.
    SAdvancementResults res(CCharAnimTime(0.f),
                            SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
    if (newDt.GreaterThanZero()) {
      res = mChild->VAdvanceView(newDt);
    }
    CCharAnimTime remTime = res.mRemTime + (newTime - mTargetAccelTime);
    mCurAccelTime = mTargetAccelTime;
    return SAdvancementResults(remTime, res.mDeltas);
  }
}

CCharAnimTime CAnimTreeTimeScale::VGetTimeRemaining() const {
  CCharAnimTime timeRem = mChild->GetTimeRemaining();
  if (mTargetAccelTime == CCharAnimTime::Infinity()) {
    CCharAnimTime remaining = mTimeScale->FindUpperLimit(mCurAccelTime, timeRem) - mCurAccelTime;
    return remaining;
  }
  return GetRealLifeTime(timeRem);
}

CSteadyStateAnimInfo CAnimTreeTimeScale::VGetSteadyStateAnimInfo() const {
  CSteadyStateAnimInfo info = mChild->GetSteadyStateAnimInfo();
  CCharAnimTime originalDuration = info.GetDuration();
  if (mTargetAccelTime == CCharAnimTime::Infinity()) {
    const CCharAnimTime duration = mTimeScale->FindUpperLimit(CCharAnimTime::ZeroFlat(),
                                                              originalDuration);
    return CSteadyStateAnimInfo(info.IsLooping(), duration, info.GetOffset());
  } else {
    CCharAnimTime time = mCurAccelTime.GreaterThanZero()
                             ? mTimeScale->TimeScaleIntegral(CCharAnimTime::ZeroFlat(),
                                                             mCurAccelTime)
                             : CCharAnimTime::ZeroFlat();
    CCharAnimTime remaining = GetTimeRemaining();
    CCharAnimTime duration = mInitialTime + time + remaining;
    return CSteadyStateAnimInfo(info.IsLooping(), duration, info.GetOffset());
  }
}

rstl::ownership_transfer< IAnimReader > CAnimTreeTimeScale::VClone() const {
  return rs_new CAnimTreeTimeScale(Cast(clone_reader(*mChild)), mTimeScale->Clone(), mCurAccelTime,
                                   mTargetAccelTime, mInitialTime, mName);
}

rstl::rc_ptr< CAnimTreeNode > CAnimTreeTimeScale::VGetBestUnblendedChild() const {
  rstl::rc_ptr< CAnimTreeNode > child = mChild->GetBestUnblendedChild();
  if (child) {
    return rs_new CAnimTreeTimeScale(Cast(clone_reader(*child)), mTimeScale->Clone(), mCurAccelTime,
                                     mTargetAccelTime, mInitialTime, mName);
  }
  return child;
}

CAnimTreeEffectiveContribution CAnimTreeTimeScale::VGetContributionOfHighestInfluence() const {
  CAnimTreeEffectiveContribution contribution = mChild->GetContributionOfHighestInfluence();
  float weight = contribution.GetContributionWeight();
  rstl::string name = contribution.GetPrimitiveName();
  CSteadyStateAnimInfo info = GetSteadyStateAnimInfo();
  CCharAnimTime time = GetTimeRemaining();
  return CAnimTreeEffectiveContribution(weight, name, info, time,
                                        contribution.GetAnimDatabaseIndex());
}

uint CAnimTreeTimeScale::VGetBoolPOIList(const CCharAnimTime& time, CBoolPOINode* listOut,
                                         uint capacity, uint iterator, int additive) const {
  const CCharAnimTime useTime =
      time == CCharAnimTime::Infinity() ? mChild->GetTimeRemaining() : GetRealLifeTime(time);
  const uint ret = mChild->GetBoolPOIList(useTime, listOut, capacity, iterator, additive);
  if (mTargetAccelTime > CCharAnimTime::ZeroFlat()) {
    for (uint i = 0; i < ret; ++i) {
      CCharAnimTime realTime = GetRealLifeTime(listOut[i].GetTime());
      listOut[iterator + i].SetTime(realTime);
    }
  }
  return ret;
}

uint CAnimTreeTimeScale::VGetInt32POIList(const CCharAnimTime& time, CInt32POINode* listOut,
                                          uint capacity, uint iterator, int additive) const {
  const CCharAnimTime useTime =
      time == CCharAnimTime::Infinity() ? mChild->GetTimeRemaining() : GetRealLifeTime(time);
  const uint ret = mChild->GetInt32POIList(useTime, listOut, capacity, iterator, additive);
  if (mTargetAccelTime > CCharAnimTime::ZeroFlat()) {
    for (uint i = 0; i < ret; ++i) {
      CCharAnimTime realTime = GetRealLifeTime(listOut[i].GetTime());
      listOut[i + iterator].SetTime(realTime);
    }
  }
  return ret;
}

uint CAnimTreeTimeScale::VGetParticlePOIList(const CCharAnimTime& time, CParticlePOINode* listOut,
                                             uint capacity, uint iterator, int additive) const {
  const CCharAnimTime useTime =
      time == CCharAnimTime::Infinity() ? mChild->GetTimeRemaining() : GetRealLifeTime(time);
  const uint ret = mChild->GetParticlePOIList(useTime, listOut, capacity, iterator, additive);
  if (mTargetAccelTime > CCharAnimTime::ZeroFlat()) {
    for (uint i = 0; i < ret; ++i) {
      CCharAnimTime realTime = GetRealLifeTime(listOut[i].GetTime());
      listOut[i + iterator].SetTime(realTime);
    }
  }
  return ret;
}

uint CAnimTreeTimeScale::VGetSoundPOIList(const CCharAnimTime& time, CSoundPOINode* listOut,
                                          uint capacity, uint iterator, int additive) const {
  const CCharAnimTime useTime =
      time == CCharAnimTime::Infinity() ? mChild->GetTimeRemaining() : GetRealLifeTime(time);
  const uint ret = mChild->GetSoundPOIList(useTime, listOut, capacity, iterator, additive);
  if (mTargetAccelTime > CCharAnimTime::ZeroFlat()) {
    for (uint i = 0; i < ret; ++i) {
      CCharAnimTime realTime = GetRealLifeTime(listOut[i].GetTime());
      listOut[i + iterator].SetTime(realTime);
    }
  }
  return ret;
}

bool CAnimTreeTimeScale::VGetBoolPOIState(uint nameHash) const {
  return mChild->VGetBoolPOIState(nameHash);
}

s32 CAnimTreeTimeScale::VGetInt32POIState(uint nameHash) const {
  return mChild->VGetInt32POIState(nameHash);
}

CParticleData::EParentedMode CAnimTreeTimeScale::VGetParticlePOIState(uint nameHash) const {
  return mChild->VGetParticlePOIState(nameHash);
}

// Retail's one function here that has no name: `symbols.txt` carries the placeholder
// `fn_802A8800` for a weak out-of-line IVaryingAnimationTimeScale::Clone - a vtable dispatch to
// slot 0x14 and nothing else, and dtk's auto unit auto_03_802B2090_text *calls* it from
// fn_802B2484. `mTimeScale->Clone()` already makes mwcceppc emit those exact bytes, but under the
// mangled name `Clone__26IVaryingAnimationTimeScaleCFv`, so objdiff has nothing to pair with the
// placeholder and the function measures 0%. Naming it `fn_802A8800` - `extern "C"`, the trick the
// `fn_` carve units under src/Kyoto/Animation use - is what resolves that auto unit's reference
// and lifts the unit's fuzzy match from 99.01% to 99.68%; the three callers keep calling
// `mTimeScale->Clone()` rather than this, because calling it directly instead costs each of them
// 2-3% (measured: 18/19 -> 15/19).
extern "C" rstl::ownership_transfer< IVaryingAnimationTimeScale >
fn_802A8800(const rstl::object_owner< IVaryingAnimationTimeScale >& timeScale) {
  return timeScale->Clone();
}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > > CAnimTreeTimeScale::VSimplified() {
  rstl::optional_object< rstl::ownership_transfer< IAnimReader > > simp = simplify_reader(*mChild);
  if (simp) {
    return rstl::ownership_transfer< IAnimReader >(
        rs_new CAnimTreeTimeScale(Cast(*simp), mTimeScale->Clone(), mCurAccelTime,
                                  mTargetAccelTime, mInitialTime, mName));
  }
  if (mCurAccelTime == mTargetAccelTime) {
    return clone_reader(*mChild);
  }
  return rstl::optional_object_null();
}

void CAnimTreeTimeScale::VSetPhase(float phase) { mChild->VSetPhase(phase); }

CCharAnimTime CAnimTreeTimeScale::GetRealLifeTime(const CCharAnimTime& time) const {
  CCharAnimTime timeRem = mChild->GetTimeRemaining();
  CCharAnimTime ret(rstl::min_val(time.GetSeconds(), timeRem.GetSeconds()));
  if (mTargetAccelTime > CCharAnimTime::ZeroFlat()) {
    CCharAnimTime accelRemaining = mTargetAccelTime - mCurAccelTime;
    if (ret < accelRemaining) {
      return mTimeScale->TimeScaleIntegral(mCurAccelTime, mCurAccelTime + ret);
    } else {
      CCharAnimTime integral = mTimeScale->TimeScaleIntegral(mCurAccelTime, mTargetAccelTime);
      if (integral > ret) {
        CCharAnimTime upper = mTimeScale->FindUpperLimit(mCurAccelTime, ret);
        return upper - mCurAccelTime;
      } else {
        return integral + (ret - integral);
      }
    }
  }
  return ret;
}

rstl::string CAnimTreeTimeScale::CreatePrimitiveName(const rstl::ncrc_ptr< CAnimTreeNode >& node,
                                                     float scaleA, const CCharAnimTime& time,
                                                     float scaleB) {
  return rstl::string_l("");
}
