#include "Kyoto/Animation/CAnimTreeTweenBase.hpp"
#include "Kyoto/Animation/CBoolPOINode.hpp"

#ifndef TARGET_PC
CBoolPOINode CBoolPOINode::CopyNodeMinusStartTime(const CBoolPOINode& node,
                                                  const CCharAnimTime& startTime) {
  return CBoolPOINode(node.GetNameHash(), node.GetPoiType(), node.GetTime() - startTime,
                      node.GetIndex(), node.GetSaveState(), node.GetWeight(),
                      node.GetCharacterIndex(), node.GetFlags(), node.GetValue());
}
#endif

CAnimTreeTweenBase::CAnimTreeTweenBase(bool characterSpaceBlend,
                                       const rstl::ncrc_ptr< CAnimTreeNode >& a,
                                       const rstl::ncrc_ptr< CAnimTreeNode >& b, int flags,
                                       const rstl::string& name)
: CAnimTreeDoubleChild(a, b, name)
, mFlags(flags)
, mCharacterSpaceBlend(characterSpaceBlend)
, mCullSelector(0) {}

CAnimTreeTweenBase::~CAnimTreeTweenBase() {}

bool CAnimTreeTweenBase::VHasOffset(const CSegId& seg) const {
  return mA->VHasOffset(seg) && mB->VHasOffset(seg);
}

CVector3f CAnimTreeTweenBase::VGetOffset(const CSegId& seg) const {
  float blendWeight = GetBlendingWeight();
  if (blendWeight >= 1.0) {
    return mB->VGetOffset(seg);
  } else {
    CVector3f startOffset = mA->VGetOffset(seg);
    CVector3f endOffset = mB->VGetOffset(seg);
    return CVector3f::Lerp(startOffset, endOffset, blendWeight);
  }
}

CQuaternion CAnimTreeTweenBase::VGetRotation(const CSegId& seg) const {
  float blendWeight = GetBlendingWeight();
  if (blendWeight >= 1.0) {
    return mB->VGetRotation(seg);
  } else {
    CQuaternion startRotation = mA->VGetRotation(seg);
    CQuaternion endRotation = mB->VGetRotation(seg);
    return CQuaternion::SlerpLocal(startRotation, endRotation, blendWeight);
  }
}

// Guessed name.
void CAnimTreeTweenBase::BlendSegStatementSet(const CSegIdList& list, CSegStatementSet& setOut,
                                              rstl::optional_object< CCharAnimTime > time) const {
  // TODO: Blend rotation, translation and scale, including the recursion-depth fallback.
}

void CAnimTreeTweenBase::VGetSegStatementSet(const CSegIdList& list,
                                             CSegStatementSet& setOut) const {
  BlendSegStatementSet(list, setOut, rstl::optional_object_null());
}

void CAnimTreeTweenBase::VGetSegStatementSet(const CSegIdList& list, CSegStatementSet& setOut,
                                             const CCharAnimTime& time) const {
  BlendSegStatementSet(list, setOut, time);
}

// Guessed name.
void CAnimTreeTweenBase::BlendSegData(const CCharLayoutInfo& layout, CJointData_LinearStorage& data,
                                      rstl::optional_object< CCharAnimTime > time) const {
  // TODO: Blend packed joint data, preserving scale/offset flags and depth fallback.
}

void CAnimTreeTweenBase::VGetSegData(const CCharLayoutInfo& layout, CJointData_LinearStorage& data,
                                     const CCharAnimTime& time) const {
  BlendSegData(layout, data, time);
}

void CAnimTreeTweenBase::VGetSegData(const CCharLayoutInfo& layout,
                                     CJointData_LinearStorage& data) const {
  BlendSegData(layout, data, rstl::optional_object_null());
}

float CAnimTreeTweenBase::VGetRightChildWeight() const { return GetBlendingWeight(); }

float CAnimTreeTweenBase::GetBlendingWeight() const { return VGetBlendingWeight(); }

bool CAnimTreeTweenBase::ShouldCullTree() { return sAdvancementDepth >= 3; }

rstl::optional_object< rstl::ownership_transfer< IAnimReader > > CAnimTreeTweenBase::VSimplified() {
  if (mCullSelector == 0) {
    rstl::optional_object< rstl::ownership_transfer< IAnimReader > > a = mA->Simplified();
    rstl::optional_object< rstl::ownership_transfer< IAnimReader > > b = mB->Simplified();
    const bool simplifyA = a.valid();
    const bool simplifyB = b.valid();
    if (!simplifyA && !simplifyB) {
      return rstl::optional_object_null();
    }
    CAnimTreeTweenBase* clone = static_cast< CAnimTreeTweenBase* >(Clone().take_ownership());
    if (simplifyA) {
      clone->ReplaceLeftChild(static_cast< CAnimTreeNode* >(a->take_ownership()));
    }
    if (simplifyB) {
      clone->ReplaceRightChild(static_cast< CAnimTreeNode* >(b->take_ownership()));
    }
    return rstl::ownership_transfer< IAnimReader >(clone);
  } else {
    const rstl::ncrc_ptr< CAnimTreeNode >& child = mCullSelector == 1 ? mB : mA;
    rstl::rc_ptr< CAnimTreeNode > best = child->GetBestUnblendedChild();
    if (!best) {
      return child->Clone();
    } else {
      return best->Clone();
    }
  }
}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > >
CAnimTreeTweenBase::VReverseSimplified() {
  return CAnimTreeTweenBase::VSimplified();
}

void CAnimTreeTweenBase::VGetWeightedReaders(
    float weight, rstl::reserved_vector< rstl::pair< float, IAnimReader* >, 16 >& out) const {
  const float blendWeight = GetBlendingWeight();
  mA->VGetWeightedReaders(weight * (1.f - blendWeight), out);
  mB->VGetWeightedReaders(weight * blendWeight, out);
}

s32 CAnimTreeTweenBase::sAdvancementDepth = 0;
