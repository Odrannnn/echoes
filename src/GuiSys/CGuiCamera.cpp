// mwcceppc's per-translation-unit string pool for this unit, byte for byte as retail has
// it at 0x803AE4C0 (`config/G2ME01/symbols.txt:17675`, `@stringBase0 = .rodata:0x803AE4C0;
// ... size:0x2E data:string_table`). Retail's `Create` hands `operator new` the entry at
// +0x27, which is the same "??(??)" placement string every other `rs_new` in the game uses;
// the two projection names ahead of it are interned by retail's original unit and
// referenced by nothing anywhere in the binary (no code forms 0x803AE4C0 or 0x803AE4D4).
// They cannot be interned from source - an unused `static const char* const[]` does intern
// both at the right offsets, but costs 8 bytes of `.sdata2` ahead of the three float
// constants and so moves them off their retail addresses - so the pool is spelled out here
// and `rs_new` is pointed at +0x27. Must precede every include: `CMEMORY_NEW_FILE` is
// expanded by `rs_new` wherever the header chain places one.
extern "C" const char lbl_803AE4C0[];
#define CMEMORY_NEW_FILE (lbl_803AE4C0 + 0x27)

#include "GuiSys/CGuiCamera.hpp"
#include "GuiSys/CGuiFrame.hpp"
#include "GuiSys/CGuiWidget.hpp"
#include "GuiSys/CGuiWidgetDrawParms.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CVector3f.hpp"

#include "Kyoto/Streams/CInputStream.hpp"

extern "C" const char lbl_803AE4C0[] = {
    'e', 'C', 'a', 'm', 'T', 'y', 'p', 'e', 'P', 'e', 'r', 's', 'p', 'e', 'c', 't', 'i',
    'v', 'e', '\0', 'e', 'C', 'a', 'm', 'T', 'y', 'p', 'e', 'O', 'r', 't', 'h', 'o', 'g',
    'o', 'n', 'a', 'l', '\0', '?', '?', '(', '?', '?', ')', '\0',
};

CGuiWidget* CGuiCamera::Create(CGuiFrame* frame, CInputStream& in, CSimplePool* sp, uint version) {
  CGuiWidgetParms parms = ReadWidgetHeader(frame, in);
  EProjection proj = static_cast< EProjection >(in.ReadInt32());
  CGuiCamera* camera = nullptr;
  if (proj == kProjection_Perspective) {
    const float fov = in.ReadFloat();
    const float aspect = in.ReadFloat();
    const float znear = in.ReadFloat();
    const float zfar = in.ReadFloat();
    camera = rs_new CGuiCamera(parms, fov, aspect, znear, zfar);
  } else if (proj == kProjection_Orthographic) {
    const float left = in.ReadFloat();
    const float right = in.ReadFloat();
    const float top = in.ReadFloat();
    const float bottom = in.ReadFloat();
    const float znear = in.ReadFloat();
    const float zfar = in.ReadFloat();
    camera = rs_new CGuiCamera(parms, left, right, top, bottom, znear, zfar);
  }

  frame->SetFrameCamera(camera);
  camera->ParseBaseInfo(frame, in, parms, version);
  return camera;
}

CGuiCamera::CGuiCamera(const CGuiWidgetParms& parms, float fov, float aspect, float znear,
                       float zfar)
: CGuiWidget(parms) {
  mProjection = kProjection_Perspective;
  CVector3f(1.f, 0.f, 0.f).Normalize();
  mCameraParms.mPerspective.mFov = fov;
  mCameraParms.mPerspective.mAspect = aspect;
  mCameraParms.mPerspective.mNear = znear;
  mCameraParms.mPerspective.mFar = zfar;
}

CGuiCamera::CGuiCamera(const CGuiWidgetParms& parms, float left, float right, float top,
                       float bottom, float znear, float zfar)
: CGuiWidget(parms) {
  mProjection = kProjection_Orthographic;
  mCameraParms.mOrthographic.mLeft = left;
  mCameraParms.mOrthographic.mRight = right;
  mCameraParms.mOrthographic.mTop = top;
  mCameraParms.mOrthographic.mBottom = bottom;
  mCameraParms.mOrthographic.mNear = znear;
  mCameraParms.mOrthographic.mFar = zfar;
}

void CGuiCamera::Draw(const CGuiWidgetDrawParms& parms) const {
  if (mProjection == kProjection_Perspective) {
    CGraphics::SetPerspective(mCameraParms.mPerspective.mFov, mCameraParms.mPerspective.mAspect,
                              mCameraParms.mPerspective.mNear, mCameraParms.mPerspective.mFar);
  } else {
    CGraphics::SetOrtho(mCameraParms.mOrthographic.mLeft, mCameraParms.mOrthographic.mRight,
                        mCameraParms.mOrthographic.mTop, mCameraParms.mOrthographic.mBottom,
                        mCameraParms.mOrthographic.mNear, mCameraParms.mOrthographic.mFar);
  }

  CGraphics::SetViewPointMatrix(CTransform4f::Translate(parms.GetCameraOffset()) *
                                GetWorldTransform());
  CGuiWidget::Draw(parms);
}

CVector3f CGuiCamera::ConvertToScreenSpace(const CVector3f& point) const {
  CVector3f rotated = RotateTranslateW2O(point);

  if (rotated.IsNonZero() && mProjection == kProjection_Perspective) {
    CMatrix4f xf = CGraphics::CalculatePerspectiveMatrix(
        mCameraParms.mPerspective.mFov, mCameraParms.mPerspective.mAspect,
        mCameraParms.mPerspective.mNear, mCameraParms.mPerspective.mFar);

    return xf.MultiplyOneOverW(rotated);
  }

  return CVector3f(-1.f, -1.f, 1.f);
}
