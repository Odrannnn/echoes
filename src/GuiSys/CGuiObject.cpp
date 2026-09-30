#include "GuiSys/CGuiObject.hpp"

#include "Kyoto/Math/CMatrix3f.hpp"

// Retail's `CGuiObject::RecalculateTransforms` (288 bytes) is a self-recursive function that
// mwcceppc has unrolled nine deep - r3, then r22, r30, r29, r28, r27, r26, r25, r24, r23 - before
// the tenth level becomes a real `bl` inside the `mNextSibling` loop. Reaching that needs the
// declaration to be marked `inline` (see CGuiObject.hpp) so `-inline deferred,noauto` will
// consider it, and this unit's `inline_max_size` raised above the project's 125: at 125 the
// unroll stops after three levels (RecalculateTransforms 49.36%), at 400 it is exact, and from
// 450 up both recursive functions stop changing (flat to 4000). `unroll_factor` (8/10/16) has
// no effect here; `inline_max_size` is the whole lever, and both halves are required.
#pragma inline_max_size(450)

CGuiObject::CGuiObject()
: mLocalXF(CTransform4f::Identity())
, mWorldXF(CTransform4f::Identity())
, mWorldTransformValid(false)
, mParent(nullptr)
, mChild(nullptr)
, mNextSibling(nullptr) {}

CGuiObject::~CGuiObject() {
  delete mChild;
  delete mNextSibling;
}

void CGuiObject::MoveInWorld(const CVector3f& offset) {
  if (mParent != nullptr) {
    // The original calls this conversion but discards its returned vector.
    mParent->RotateW2O(offset);
  }
  mLocalXF.AddTranslation(offset);
  RecalculateTransforms();
}

CVector3f CGuiObject::GetWorldPosition() const { return GetWorldTransform().GetTranslation(); }

CVector3f CGuiObject::GetLocalPosition() const { return mLocalXF.GetTranslation(); }

void CGuiObject::SetLocalPosition(const CVector3f& pos) {
  MoveInWorld(pos - mLocalXF.GetTranslation());
}

void CGuiObject::RotateReset() {
  const CVector3f position = mLocalXF.GetTranslation();
  mLocalXF = CTransform4f::Identity();
  mLocalXF.SetTranslation(position);
  RecalculateTransforms();
}

CVector3f CGuiObject::RotateW2O(const CVector3f& vec) const {
  const CVector3f result = GetWorldTransform().TransposeRotate(vec);
  return result;
}

CVector3f CGuiObject::RotateTranslateW2O(const CVector3f& vec) const {
  const CTransform4f& world = GetWorldTransform();
  const CVector3f translation = world.GetTranslation();
  const CVector3f result =
      world.TransposeRotate(CVector3f(vec.GetX() - translation.GetX(), vec.GetY() - translation.GetY(),
                                      vec.GetZ() - translation.GetZ()));
  return result;
}

void CGuiObject::MultiplyO2P(const CTransform4f& xf) {
  mLocalXF = xf * mLocalXF;
  RecalculateTransforms();
}

void CGuiObject::AddChildObject(CGuiObject* child, bool makeWorldLocal, bool atEnd) {
  child->mParent = this;
  if (mChild == nullptr) {
    mChild = child;
  } else if (atEnd) {
    CGuiObject* last = mChild;
    // Retail keeps the loop test at the top with the `last = next` body out of line below it;
    // the plain `while` form makes mwcceppc rotate the loop the other way (99.15% -> 96.99%).
    for (;;) {
      CGuiObject* next = last->mNextSibling;
      if (next == nullptr) {
        last->mNextSibling = child;
        break;
      }
      last = next;
    }
  } else {
    child->mNextSibling = mChild;
    mChild = child;
  }

  if (makeWorldLocal) {
    const CTransform4f& parentWorld = child->mParent->GetWorldTransform();
    CTransform4f worldLocalXf = CTransform4f::Identity();
    const CVector3f position = parentWorld.GetTranslation() * -1.f;
    const CVector3f scale(parentWorld.GetColumn(kDX).Magnitude(),
                          parentWorld.GetColumn(kDY).Magnitude(),
                          parentWorld.GetColumn(kDZ).Magnitude());
    // `column * (1.f / scale)`, not `scalar * column`: with the scalar first mwcceppc folds
    // the CVector3f temporary away and the six frame slots retail builds are lost (frame 416
    // instead of 448, 90.93% instead of 99.15%).
    const CMatrix3f tmpMtx(
      parentWorld.GetColumn(kDX) * (1.f / scale.GetX()),
      parentWorld.GetColumn(kDY) * (1.f / scale.GetY()),
      parentWorld.GetColumn(kDZ) * (1.f / scale.GetZ()));
    const CVector3f pos = tmpMtx * position;
    // Nine spelled-out `GetColumn` calls, not named column locals: retail materialises a
    // CVector3f temporary per call and reuses three frame slots, and only this form does.
    worldLocalXf = CTransform4f(
      tmpMtx.GetColumn(kDX).GetX(), tmpMtx.GetColumn(kDY).GetX(), tmpMtx.GetColumn(kDZ).GetX(), pos.GetX(),
      tmpMtx.GetColumn(kDX).GetY(), tmpMtx.GetColumn(kDY).GetY(), tmpMtx.GetColumn(kDZ).GetY(), pos.GetY(),
      tmpMtx.GetColumn(kDX).GetZ(), tmpMtx.GetColumn(kDY).GetZ(), tmpMtx.GetColumn(kDZ).GetZ(), pos.GetZ());
    child->mLocalXF = worldLocalXf * child->GetWorldTransform();
  }

  RecalculateTransforms();
}

const CGuiObject* CGuiObject::GetChildObject() const { return mChild; }

CGuiObject* CGuiObject::ChildObject() { return mChild; }

const CGuiObject* CGuiObject::GetNextSibling() const { return mNextSibling; }

CGuiObject* CGuiObject::NextSibling() { return mNextSibling; }

const CGuiObject* CGuiObject::GetParent() const { return mParent; }

CGuiObject* CGuiObject::Parent() { return mParent; }

void CGuiObject::SetO2PTransform(const CTransform4f& xf) {
  mLocalXF = xf;
  RecalculateTransforms();
}

void CGuiObject::SetO2WTransform(const CTransform4f& xf) {
  const CTransform4f& world = mParent->GetWorldTransform();
  const CTransform4f inverse = world.GetQuickInverse();
  const CTransform4f local = inverse * xf;
  SetO2PTransform(local);
}

const CTransform4f& CGuiObject::GetWorldTransform() const {
  if (!mWorldTransformValid) {
    // The positive test, not `if (mParent == nullptr) return mLocalXF;`: retail's ten inlined
    // levels branch *over* the no-parent case to reach the `mWorldTransformValid = true`
    // store, so the multiply has to be the fall-through. With the arms the other way round the
    // same ten levels are inlined but 8 of the 696 bytes per level are wrong (79.17% -> 71.62%).
    if (mParent != nullptr) {
      mWorldXF = mParent->GetWorldTransform() * mLocalXF;
      mWorldTransformValid = true;
    } else {
      return mLocalXF;
    }
  }
  return mWorldXF;
}

void CGuiObject::RecalculateTransforms() {
  mWorldTransformValid = false;
  for (CGuiObject* child = mChild; child != nullptr; child = child->mNextSibling) {
    child->RecalculateTransforms();
  }
}
