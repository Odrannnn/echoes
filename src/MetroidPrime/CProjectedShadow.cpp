#include "MetroidPrime/CProjectedShadow.hpp"

#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"

#include <dolphin/gx.h>
#include <float.h>

// The projected-shadow list head is this unnamed CStateManager helper in retail
// (0x801921A0... `fn_80036F68` at 0x80036F68); CStateManager.cpp defines it.
extern "C" void fn_80036F68(CStateManager* mgr, void* node);

CProjectedShadow::CProjectedShadow(int width, int height, uchar persistent, int projectionMode)
: mTexture(kTF_I4, width, height, 1)
, mBounds(CAABox::MakeMaxInvertedBox())
, mScale(1.f)
, mTranslation(CVector3f::Zero())
, mZDistanceAdjust(0.f)
, mOpacity(1.f)
, mEnabled(false)
, mPersistent(persistent)
, mOverrideBounds(false)
, mProjectOnActors(projectionMode == 0)
, mNextShadow(nullptr) {}

CProjectedShadow::~CProjectedShadow() { mTexture.ScheduleDeletion(); }

void CProjectedShadow::ExpandBoundsForTexture() {
  const float texelScale = 1.f / (mTexture.GetWidth() - 2);
  const CVector3f offset(3.f * mBounds.GetWidth() * texelScale,
                         3.f * mBounds.GetHeight() * texelScale, 0.f);
  mBounds = CAABox(mBounds.GetMinPoint() - offset, mBounds.GetMaxPoint() + offset);
}

// Guessed name.
void CProjectedShadow::SetBounds(const CAABox& bounds) {
  mBounds = bounds;
  mOverrideBounds = true;
}

void CProjectedShadow::RenderShadowBuffer(CStateManager& mgr, const CModelData& model,
                                          const CTransform4f& transform, int flags,
                                          const CVector3f& translation, float scale,
                                          float zDistanceAdjust) {
  const CModelData* modelPtr = &model;
  const CTransform4f* transformPtr = &transform;
  RenderShadowBuffer(mgr, 1, &modelPtr, &transformPtr, flags, translation, scale, zDistanceAdjust);
}

void CProjectedShadow::RenderShadowBuffer(CStateManager& mgr, int count,
                                          const CModelData* const* models,
                                          const CTransform4f* const* transforms, int flags,
                                          const CVector3f& translation, float scale,
                                          float zDistanceAdjust) {
  if (count < 1) {
    return;
  }

  if (!mOverrideBounds) {
    mBounds = models[0]->GetBounds(*transforms[0]);
    for (int i = 1; i < count; ++i) {
      mBounds.Include(models[i]->GetBounds(*transforms[i]));
    }
  } else {
    mOverrideBounds = false;
  }
  mScale = scale;
  mTranslation = translation;
  mZDistanceAdjust = zDistanceAdjust;
  mEnabled = true;
  ExpandBoundsForTexture();

  const CTransform4f oldView = CGraphics::GetViewMatrix();
  const float oldNear = CGraphics::GetDepthNear();
  const float oldFar = CGraphics::GetDepthFar();
  const CGraphics::CProjectionState oldProjection = CGraphics::GetProjectionState();
  const CViewport oldViewport = CGraphics::GetViewport();
  const short width = mTexture.GetWidth();
  const short height = mTexture.GetHeight();
  const int renderWidth = width * 2;
  const int renderHeight = height * 2;
  const CVector3f center = (mBounds.GetMinPoint() + mBounds.GetMaxPoint()) * 0.5f;
  const CTransform4f view =
      CTransform4f::FromColumns(CVector3f::Right(), CVector3f::Down(), CVector3f::Forward(),
                                CVector3f(center.GetX(), center.GetY(),
                                          mBounds.GetMaxPoint().GetZ()));
  CGraphics::SetViewPointMatrix(view);
  CGraphics::SetDepthRange(0.f, 1.f);
  const float halfWidth = 0.5f * mBounds.GetWidth();
  const float halfHeight = 0.5f * mBounds.GetHeight();
  CGraphics::SetOrtho(-halfWidth, halfWidth, halfHeight, -halfHeight, 0.f,
                      FLT_EPSILON + mBounds.GetDepth());
  gpRender->SetViewport(0, CGraphics::GetRenderMode().efbHeight - renderHeight, renderWidth,
                        renderHeight);
  CGX::SetNumTevStages(1);
  CGX::SetNumTexGens(1);
  CGX::SetNumChans(0);
  CGraphics::DisableAllLights();
  CGX::SetNumIndStages(0);
  CGX::SetTevDirect(GX_TEVSTAGE0);
  CGX::SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ONE);
  CGX::SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
  CGX::SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_8_8);
  CGX::SetStandardTevColorAlphaOp(GX_TEVSTAGE0);
  CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
  CGX::SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, GX_IDENTITY, false, GX_PTIDENTITY);
  CGX::SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  CGX::SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);

  for (int i = 0; i < count; ++i) {
    const CModelData& modelData = *models[i];
    const CTransform4f& xf = *transforms[i];
    CGraphics::SetModelMatrix(xf * CTransform4f::Scale(modelData.GetScale()));
    if (const CAnimData* animData = modelData.GetAnimationData()) {
      CSkinnedModel& model = modelData.PickAnimatedModel(CModelData::kWM_Normal);
      animData->SetupRender();
      const uint drawFlags =
          CSkinnedModel::kDF_Flat | CSkinnedModel::kDF_Unsorted |
          (flags == 0 ? CSkinnedModel::kDF_Sorted : static_cast< uint >(0));
      model.DolphinDrawWithFlags(&const_cast< CAnimData* >(animData)->Pose(), drawFlags,
                                 CModelFlags(CModelFlags::kT_Opaque, CColor(1.f, 1.f, 1.f, 1.f)));
    } else {
      const TLockedToken< CModel >& model = modelData.PickStaticModel(CModelData::kWM_Normal);
      model->PreDrawModel(CModelFlags(CModelFlags::kT_Opaque, CColor(1.f, 1.f, 1.f, 1.f)));
      // Retail passes 2 for a plain draw and 0 when `flags` is set (0x8019230C).
      model->DolphinDrawFlat(flags == 0 ? CModel::kDF_All : CModel::kDF_Unknown0);
    }
  }

  const bool useVideoFilter = CGraphics::GetUseVideoFilter();
  CGraphics::SetUseVideoFilter(false);
  CGX::SetZMode(true, GX_LEQUAL, true);
  GXSetTexCopySrc(0, 0, renderWidth, renderHeight);
  GXSetTexCopyDst(width, height, GX_CTF_R4, true);
  GXCopyTex(mTexture.Lock(), true);
  mTexture.UnLock();
  GXPixModeSync();
  CGraphics::SetUseVideoFilter(useVideoFilter);
  CGraphics::SetViewPointMatrix(oldView);
  CGraphics::SetProjectionState(oldProjection);
  gpRender->SetViewport(oldViewport.mLeft, oldViewport.mTop, oldViewport.mWidth,
                        oldViewport.mHeight);
  CGraphics::SetDepthRange(oldNear, oldFar);
  fn_80036F68(&mgr, this);
}

CAABox ScaleAndTranslateBounds(const CAABox& bounds, const CVector3f& translation, float scale) {
  const CVector3f extent = bounds.GetMaxPoint() - bounds.GetMinPoint();
  const CVector3f padding = (extent * scale - extent) * 0.5f;
  return CAABox(bounds.GetMinPoint() - padding + translation,
                bounds.GetMaxPoint() + padding + translation);
}

void CProjectedShadow::Render(const CStateManager& mgr) const {
  // TODO: Project the texture onto world geometry and optional actor receivers.
}
