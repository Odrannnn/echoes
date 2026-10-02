#include "Kyoto/Animation/CAnimTreeTransition.hpp"

#include "Kyoto/Math/CloseEnough.hpp"

// rstl::max_val in this repo takes and returns by value, which forces both operands into
// temporaries here; retail compares the two in place and picks a reference. Prime 1's rstl
// spells max_val the same way as below.
template < typename T >
inline const T& max_val_in_place(const T& a, const T& b) {
  return (a < b) ? b : a;
}

// Retail calls a weak out-of-line IAnimReader::Clone / Simplified rather than inlining the vtable
// dispatch, so the calls go through functions here too. IAnimReader's own definitions cannot be
// moved out of line in this tree: IAnimReader.cpp is NonMatching, so its object is not in the link.
// See docs/goal-notes/progress-prime1-canimtreetransition.md.
rstl::ownership_transfer< IAnimReader > clone_reader(const IAnimReader& reader) {
  return reader.VClone();
}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > >
simplified_reader(IAnimReader& reader) {
  return reader.VSimplified();
}

// The "Loop" POI hash, cached in a function-local static behind its guard. Retail keeps this out
// of line and dtk records it only as the unnamed fn_802AA708, so it is named the way the rest of
// this tree names retail functions with no recoverable C++ name (see
// src/MetroidPrime/CModelDataModelSlots.cpp): the extern "C" linkage is what gives the symbol the
// unmangled name, so the address-based name is also the one objdiff pairs. `extern "C"` rather
// than `static` - a static function is still mangled (`fn_802AA708__Fv`) and pairs with nothing.
extern "C" uint fn_802AA708() {
  static uint hash = CPOINode::GetHashForString("Loop");
  return hash;
}

static inline bool loop_state(const rstl::ncrc_ptr< CAnimTreeNode >& a, uint hash) {
  return a->VGetBoolPOIState(hash);
}

CAnimTreeTransition::CAnimTreeTransition(const bool characterSpaceBlend,
                                         const rstl::ncrc_ptr< CAnimTreeNode >& a,
                                         const rstl::ncrc_ptr< CAnimTreeNode >& b,
                                         const CCharAnimTime& duration, bool runA, int flags,
                                         const rstl::string& name)
: CAnimTreeTweenBase(characterSpaceBlend, a, b, flags, name)
, mTransDur(duration)
, mTimeInTrans(0.f)
, mRunA(runA)
, mLoopA(loop_state(a, fn_802AA708()))
, mInitialized(false) {}

CAnimTreeTransition::CAnimTreeTransition(const bool characterSpaceBlend,
                                         const rstl::ncrc_ptr< CAnimTreeNode >& a,
                                         const rstl::ncrc_ptr< CAnimTreeNode >& b,
                                         const CCharAnimTime& duration,
                                         const CCharAnimTime& timeInTrans, bool runA, bool loopA,
                                         int flags, const rstl::string& name, bool initialized)
: CAnimTreeTweenBase(characterSpaceBlend, a, b, flags, name)
, mTransDur(duration)
, mTimeInTrans(timeInTrans)
, mRunA(runA)
, mLoopA(loopA)
, mInitialized(initialized) {}

CAnimTreeTransition::~CAnimTreeTransition() {}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > >
CAnimTreeTransition::VSimplified() {
  if (close_enough(GetBlendingWeight(), 1.f)) {
    rstl::optional_object< rstl::ownership_transfer< IAnimReader > > simp = simplified_reader(*mB);
    if (simp) {
      return simp;
    }
    return clone_reader(*mB);
  }
  return CAnimTreeTweenBase::VSimplified();
}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > >
CAnimTreeTransition::VReverseSimplified() {
  if (close_enough(GetBlendingWeight(), 0.f)) {
    return clone_reader(*mA);
  }
  return CAnimTreeTweenBase::VReverseSimplified();
}

rstl::pair< CCharAnimTime, SAdvancementDeltas >
CAnimTreeTransition::AdvanceViewForTransitionalPeriod(const CCharAnimTime& time) {
  IncAdvancementDepth();
  CDoubleChildAdvancementResult res = AdvanceViewBothChildren(time, mRunA, mLoopA);
  DecAdvancementDepth();
  const CCharAnimTime& trueAdvancement = res.GetTrueAdvancement();
  if (trueAdvancement.EqualsZero()) {
    return rstl::pair< CCharAnimTime, SAdvancementDeltas >(
        CCharAnimTime::ZeroFlat(),
        SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  float oldWeight = GetBlendingWeight();
  mTimeInTrans += trueAdvancement;
  float newWeight = GetBlendingWeight();
  if (ShouldCullTree()) {
    if (newWeight < 0.5f) {
      mCullSelector = 1;
    } else {
      mCullSelector = 2;
    }
  }
  const SAdvancementDeltas& leftDeltas = res.GetLeftAdvancementDeltas();
  const SAdvancementDeltas& rightDeltas = res.GetRightAdvancementDeltas();
  if (GetBlendRoot() & kBlendRoot_Offset) {
    return rstl::pair< CCharAnimTime, SAdvancementDeltas >(
        trueAdvancement,
        SAdvancementDeltas::Interpolate(leftDeltas, rightDeltas, oldWeight, newWeight));
  }
  return rstl::pair< CCharAnimTime, SAdvancementDeltas >(trueAdvancement, rightDeltas);
}

SAdvancementResults CAnimTreeTransition::VAdvanceView(const CCharAnimTime& time) {
  if (time.EqualsZero()) {
    IncAdvancementDepth();
    mB->VAdvanceView(time);
    if (mRunA) {
      mA->VAdvanceView(time);
    }
    DecAdvancementDepth();
    if (ShouldCullTree()) {
      mCullSelector = 1;
    }
    return SAdvancementResults(CCharAnimTime::ZeroFlat(),
                               SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (!mInitialized) {
    mInitialized = true;
  }
  if (mTimeInTrans + time < mTransDur) {
    rstl::pair< CCharAnimTime, SAdvancementDeltas > res = AdvanceViewForTransitionalPeriod(time);
    return SAdvancementResults(time - res.first, res.second);
  }
  CCharAnimTime transTimeRem = mTransDur - mTimeInTrans;
  rstl::pair< CCharAnimTime, SAdvancementDeltas > res(
      CCharAnimTime(0.f), SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  if (transTimeRem.GreaterThanZero()) {
    res = AdvanceViewForTransitionalPeriod(transTimeRem);
    if (res.first != transTimeRem) {
      return SAdvancementResults(res.first, res.second);
    }
  }
  CCharAnimTime remainder = time - transTimeRem;
  return SAdvancementResults(remainder, res.second);
}

rstl::ownership_transfer< IAnimReader > CAnimTreeTransition::VClone() const {
  return rs_new CAnimTreeTransition(CharacterSpaceBlend(), Cast(clone_reader(*mA)),
                                    Cast(clone_reader(*mB)), mTransDur, mTimeInTrans, mRunA, mLoopA,
                                    GetBlendRoot(), mName, mInitialized);
}

float CAnimTreeTransition::VGetBlendingWeight() const {
  if (mTransDur.GreaterThanZero()) {
    return (1.f / mTransDur.GetSeconds()) * mTimeInTrans.GetSeconds();
  }
  return 1.f;
}

CCharAnimTime CAnimTreeTransition::VGetTimeRemaining() const {
  return max_val_in_place(mB->VGetTimeRemaining(), mTransDur - mTimeInTrans);
}

CSteadyStateAnimInfo CAnimTreeTransition::VGetSteadyStateAnimInfo() const {
  CSteadyStateAnimInfo info = mB->VGetSteadyStateAnimInfo();
  return CSteadyStateAnimInfo(
      info.IsLooping(), rstl::max_val< const CCharAnimTime& >(mTransDur, info.GetDuration()),
      info.GetOffset());
}

rstl::string CAnimTreeTransition::CreatePrimitiveName(const rstl::ncrc_ptr< CAnimTreeNode >& a,
                                                      const rstl::ncrc_ptr< CAnimTreeNode >& b,
                                                      float duration) {
  return rstl::string_l("");
}

void CAnimTreeTransition::SetBlendingWeight(float weight) {
  rstl::rc_ptr< CAnimTreeNode > right = GetRightChild();
  static_cast< CAnimTreeTweenBase* >(right.GetPtr())->SetBlendingWeight(weight);
}

rstl::rc_ptr< CAnimTreeNode > CAnimTreeTransition::VGetBestUnblendedChild() const {
  rstl::rc_ptr< CAnimTreeNode > right = GetRightChild();
  rstl::rc_ptr< CAnimTreeNode > child = right->GetBestUnblendedChild();
  if (!child) {
    return right;
  }
  return child;
}

const int CAnimTreeTweenBase::kBlendRoot_Offset = 1;

const int CAnimTreeTweenBase::kBlendRoot_Rotation = 2;
