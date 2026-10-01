#include "Kyoto/Animation/CPoseAsTransforms_Linear.hpp"

#include "Kyoto/Animation/CCharLayoutInfo.hpp"
#include "Kyoto/Animation/CJointData_LinearStorage.hpp"
#include "Kyoto/Math/CQuaternion.hpp"

static rstl::vector< CSegId >::const_iterator ConnectedPartsEnd(const CCharLayoutInfo& layout,
                                                                const CSegId& seg);

//! Retail 0x802DBE78: the 0x20-byte head of a CMatrix3f block copy, the half mwceppc's inline
//! size limit pushes out of line. It is declared in `Kyoto/Math/CMatrix3f.hpp` next to the
//! `rstl::construct_impl` specialisation that is its only caller, and the two units that hold a
//! `rstl::vector<CMatrix3f>` are the two that call it.
//!
//! It is defined here, not in `Kyoto/Math/CMatrix3f.cpp`, for two reasons that are the same
//! reason: that unit is `Matching` at 16/16, so one more function would push its object past
//! the range `config/G2ME01/splits.txt` claims and break the DOL hash, and it is the wrong
//! home for a helper the port's file list does not otherwise reach. This unit is `NonMatching`
//! with room to spare and is in `files.cmake`, so the host link resolves the symbol without
//! anyone having to add it to the port's missing-symbol list.
extern "C" void fn_802DBE78(void* self, const void* src) {
  double* head = static_cast< double* >(self);
  const double* otherHead = static_cast< const double* >(src);
  head[0] = otherHead[0];
  head[1] = otherHead[1];
  head[2] = otherHead[2];
  head[3] = otherHead[3];
}

CPoseAsTransforms_Linear::CPoseAsTransforms_Linear(int count, int withScale, int withOffsets)
: mElements(count, CElementType(CMatrix3f::Identity(), CVector3f::Zero(), CVector3f::Zero()))
, mScales(withScale == 1 ? count : 0, CVector3f::One())
, mUnscaledRotations(withScale == 1 ? count : 0, CMatrix3f::Identity())
, x30_(withOffsets == 1 ? count : 0, CVector3f::Zero())
, x40_24_(false)
, mUniformScale(0) {}

const CMatrix3f& CPoseAsTransforms_Linear::GetTransformMinusOffset(const CSegId& seg) const {
  return mElements[seg.val()].mRotation;
}

CMatrix3f CPoseAsTransforms_Linear::GetRotation(const CSegId& seg) {
  if (mScales.size() != 0) {
    return mUnscaledRotations[seg.val()];
  }
  return GetTransformMinusOffset(seg);
}

const CVector3f& CPoseAsTransforms_Linear::GetOffset(const CSegId& seg) const {
  return mElements[seg.val()].mOffset;
}

CTransform4f CPoseAsTransforms_Linear::GetTransform(const CSegId& seg) {
  const CElementType& elem = mElements[seg.val()];
  if (mScales.size() != 0) {
    return CTransform4f(mUnscaledRotations[seg.val()], elem.mOffset);
  }
  return CTransform4f(elem.mRotation, elem.mOffset);
}

void CPoseAsTransforms_Linear::BuildPose(const CCharLayoutInfo& layout,
                                         const CJointData_LinearStorage& data) {
  x40_24_ = false;
  const uchar* rotations = data.GetRotations();
  const uchar* translations = data.GetTranslations();
  const uchar* scales = data.GetScales();
  int stride = data.GetStride();
  if (mScales.size() != 0) {
    CElementType* elem = mElements.data();
    const CSegId* parent = layout.GetLinearParents().data();
    CMatrix3f* unscaled = mUnscaledRotations.data();
    CVector3f* scale = mScales.data();
    elem->mRotation = CMatrix3f::Identity();
    elem->mOffset = CVector3f::Zero();
    *unscaled = CMatrix3f::Identity();
    *scale = CVector3f::One();
    const uchar* scaleBase = scales;
    rotations += stride;
    translations += stride;
    scales += stride;
    ++elem;
    ++unscaled;
    ++parent;
    ++scale;
    int count = mElements.size();
    for (int i = 1; i < count; ++i) {
      uchar parentId = parent->val();
      const CElementType& parentElem = mElements[parentId];
      const CMatrix3f& parentRotation = mUnscaledRotations[parentId];
      *scale = *reinterpret_cast< const CVector3f* >(scales);
      *unscaled =
          parentRotation * reinterpret_cast< const CQuaternion* >(rotations)->BuildTransform();
      elem->mRotation = *unscaled * CMatrix3f::Scale(scale->GetX(), scale->GetY(), scale->GetZ());
      elem->mLocalOffset = *reinterpret_cast< const CVector3f* >(translations);
      elem->mOffset = parentElem.mOffset +
                      parentRotation * (elem->mLocalOffset * *reinterpret_cast< const CVector3f* >(
                                                                 scaleBase + stride * parentId));
      rotations += stride;
      translations += stride;
      scales += stride;
      ++parent;
      ++unscaled;
      ++scale;
      ++elem;
    }
  } else {
    CElementType* elem = mElements.data();
    const CSegId* parent = layout.GetLinearParents().data();
    elem->mRotation = CMatrix3f::Identity();
    elem->mOffset = CVector3f::Zero();
    rotations += stride;
    translations += stride;
    ++parent;
    ++elem;
    int count = mElements.size();
    for (int i = 1; i < count; ++i) {
      const CElementType& parentElem = mElements[parent->val()];
      elem->mRotation = parentElem.mRotation *
                        reinterpret_cast< const CQuaternion* >(rotations)->BuildTransform();
      elem->mLocalOffset = *reinterpret_cast< const CVector3f* >(translations);
      elem->mOffset = parentElem.mOffset + parentElem.mRotation * elem->mLocalOffset;
      rotations += stride;
      translations += stride;
      ++parent;
      ++elem;
    }
  }
}

void CPoseAsTransforms_Linear::SetRotation(const CCharLayoutInfo& layout, const CSegId& seg,
                                           const CMatrix3f& rotation) {
  x40_24_ = false;
  CMatrix3f delta = rotation * GetRotation(seg).GetTranspose();
  RotateHierarchy(layout, seg, delta, 1);
}

void CPoseAsTransforms_Linear::RotateHierarchy(const CCharLayoutInfo& layout, const CSegId& seg,
                                               const CMatrix3f& rotation, int order) {
  int id = seg.val();
  x40_24_ = false;
  CElementType& elem = mElements[id];
  if (mScales.size() == 0) {
    if (order == 1) {
      elem.mRotation = rotation * elem.mRotation;
    } else {
      elem.mRotation = elem.mRotation * rotation;
    }
    elem.mRotation = elem.mRotation.Orthonormalized();
  } else {
    CMatrix3f unscaled(CMatrix3f::Identity());
    if (order == 1) {
      unscaled = rotation * elem.mRotation;
    } else {
      unscaled = elem.mRotation * rotation;
    }
    const CVector3f& scale = mScales[id];
    elem.mRotation = unscaled * CMatrix3f::Scale(scale.GetX(), scale.GetY(), scale.GetZ());
    mUnscaledRotations[id] = unscaled;
  }

  if (layout.GetSegmentData(seg).GetNumConnectedParts() >= 2) {
    rstl::vector< CSegId >::const_iterator it =
        layout.GetSegmentData(seg).GetConnectedParts().begin();
    rstl::vector< CSegId >::const_iterator end = ConnectedPartsEnd(layout, seg);
    ++it;
    bool hasScale = mScales.size() != 0;
    const CVector3f& scale = hasScale ? mScales[id] : CVector3f::One();
    const CMatrix3f& parentRotation = hasScale ? mUnscaledRotations[id] : elem.mRotation;
    for (; it != end; ++it) {
      CElementType& child = mElements[it->val()];
      child.mOffset = elem.mOffset + parentRotation * (child.mLocalOffset * scale);
      RotateHierarchy(layout, *it, rotation, order);
    }
  }
}

static rstl::vector< CSegId >::const_iterator ConnectedPartsEnd(const CCharLayoutInfo& layout,
                                                                const CSegId& seg) {
  const CCharLayoutNode& node = layout.GetSegmentData(seg);
  return node.GetConnectedParts().data() + node.GetNumConnectedParts();
}

void CPoseAsTransforms_Linear::AllocateScale() {
  mScales.resize(mElements.size(), CVector3f::Zero());
  mUnscaledRotations.resize(mElements.size(), CMatrix3f::Identity());
}
