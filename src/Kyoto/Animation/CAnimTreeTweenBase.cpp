#include "Kyoto/Animation/CAnimTreeTweenBase.hpp"
#include "Kyoto/Animation/CBoolPOINode.hpp"

// `BlendSegData` and `BlendSegStatementSet` take their optional time **by value**, so each of the
// four `VGet*` wrappers below materialises an `rstl::optional_object< CCharAnimTime >` temporary in
// its own frame. Retail copies the eight data bytes straight into it and stores the valid flag
// after the address is taken (0x802AB0D8 and 0x802AB094: `lfs f0,0(r6) / li r7,1 / addi r6,r1,8 /
// stb r7,16(r1) / stfs f0,8(r1) / stw r0,12(r1)`). With the generic `construct_impl` the temporary's
// copy goes through placement new, and MWCC guards it: `addic. r7,r1,8 / beq`, with the stores
// addressed through `r7` - 68 bytes where retail has 60, which is why both timed wrappers sat at
// 32.53%. `CCharAnimTime` is `{ float, EType }` with no user-declared special members, so the
// specialised `construct_impl` here is the same copy without the guard, and it takes both wrappers
// to 100%. Declared in this TU on purpose: `CCharAnimTime.hpp` is included almost everywhere, and
// moving it there would resize unrelated units' `.text`.
namespace rstl {
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CCharAnimTime)
} // namespace rstl

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

// `fn_802AB560` - retail has no name for this one, so dtk's `fn_<address>` is the name our object
// has to carry or objdiff, which pairs by symbol name, scores it 0% however exact the bytes are.
// `extern "C"` is what keeps the identifier out of the mangler; `RUNNING_THE_DECOMP.md`'s "name the
// function what the retail symbol says" applies.
//
// It hands the work to one child and picks the timed or the current-pose `IAnimReader` virtual from
// the optional time, so it is out of line: `BlendSegStatementSet` calls it from four places, and the
// vtable slots it selects are 0x48 (timed) and 0x44 (untimed) of `IAnimReader`.
extern "C" void fn_802AB560(const rstl::rc_ptr< CAnimTreeNode >& child, const CSegIdList& list,
                            CSegStatementSet& setOut,
                            const rstl::optional_object< CCharAnimTime >& time) {
  if (time.valid()) {
    child->VGetSegStatementSet(list, setOut, *time);
  } else {
    child->VGetSegStatementSet(list, setOut);
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

// `fn_802AB084` - `BlendSegData`'s counterpart of `fn_802AB560`, and named for the same reason. The
// slots it selects are 0x4C (timed) and 0x50 (untimed) of `IAnimReader`.
extern "C" void fn_802AB084(const rstl::rc_ptr< CAnimTreeNode >& child,
                            const CCharLayoutInfo& layout, CJointData_LinearStorage& data,
                            const rstl::optional_object< CCharAnimTime >& time) {
  if (time.valid()) {
    child->VGetSegData(layout, data, *time);
  } else {
    child->VGetSegData(layout, data);
  }
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
