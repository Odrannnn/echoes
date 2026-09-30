#include "MetroidPrime/CModelData.hpp"

#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

#include "Kyoto/Animation/CSegId.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CPlane.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CActorLights.hpp"

// Guessed name.
struct SModelDataMultipassContext {
  const CSkinnedModel& mModel;
  const CModelFlags* mFlags;
  const u64* mMasks;
  const CColor* mColors;
  const CPlane* mPlanes;
  int mCount;
};
CHECK_SIZEOF(SModelDataMultipassContext, 0x18)

// The two touch-everything helpers retail calls on the model holder `PickAnimatedModel` returns.
// They are defined in `src/MetroidPrime/CAnimData.cpp`, which owns 0x80027AE8..0x80027B68; this
// file repeats the holder shape `CModelTouchParts.cpp` and `CGunEffectTouch.cpp` each declare
// locally, because retail names no class for it.
//
// **Both shapes below are the 32-bit one and the host does not have it.** `src/Kyoto/Graphics/
// CModelTouch.cpp:53-55` records the same `CModel` members at 0/8/16/40/64 on the host, and
// `CSkinnedModel`'s `TLockedToken` is two pointers wide there, so neither the holder's +8 nor
// `CModel`'s 0x1C is where the 64-bit build keeps the model pointer or the material-set count -
// reading them there would walk a `Touch` loop over an uninitialised `int`. Under `TARGET_PC` the
// arms below name the members instead: `CSkinnedModel::GetModel()` for the token, and
// `CModel::GetMatSetCount()` (`include/Kyoto/Graphics/CModel.hpp:95`, guarded for exactly this)
// for the count, the accessor `src/MetroidPrime/Player/CGunEffectTouchAll.cpp:62` already uses.
// mwcceppc does not define `TARGET_PC`, so the `#else` arms are the ones the matching build
// compiles and the DOL is unaffected.
#ifndef TARGET_PC
struct SModelHolder {
  char x0_pad[8];
  CModel* x8_model;
  char xc_pad[4];
};

// The loop bound those helpers walk to, and the one `GetNumShaders` returns: the single `int` at
// **+0x1C** of `CModel`, i.e. `mMatSets`'s count. `mMatSets` is private and upstream exposes no
// accessor outside `#ifdef TARGET_PC`, so the one word is claimed here as
// `src/MetroidPrime/CModelTouchParts.cpp` does.
struct SShaderCount {
  char x0_pad[0x1c];
  int mNumShaders;
};

extern "C" void fn_80027B44(const SModelHolder* holder, int part);
extern "C" void fn_80027AE8(const SModelHolder* holder);
#endif // TARGET_PC

static const CAdvancementDeltas skNullAdvance(CVector3f::Zero(), CQuaternion::NoRotation());

CModelData::CModelData(const CStaticRes& res)
: mScale(res.GetScale())
, mRenderSorted(false)
, mTexturesLocked(false)
, mRenderUnsortedParts(true)
, mRenderFullEchoModel(false)
, mAmbientColor(CColor::White())
, mNormalModel(TLockedToken< CModel >(gpSimplePool->GetObj(SObjectTag('CMDL', res.GetId())))) {}

CModelData::CModelData()
: mScale(1.f, 1.f, 1.f)
, mRenderSorted(false)
, mTexturesLocked(false)
, mRenderUnsortedParts(true)
, mRenderFullEchoModel(false)
, mAmbientColor(CColor::White()) {}

CModelData::CModelData(const CAnimRes& res)
: mScale(res.GetScale())
, mRenderSorted(false)
, mTexturesLocked(false)
, mRenderUnsortedParts(true)
, mRenderFullEchoModel(false)
, mAmbientColor(CColor::White()) {
  // TODO: Create the character through CCharacterFactoryBuilder. A negative default animation
  // selects the character's default; pass the model scale to the resulting CAnimData.
}

CModelData::~CModelData() {}

// Echoes splits the two passes: `Render` only draws the sorted/solid pass and returns, while
// `RenderUnsortedParts` (below) owns the unsorted surfaces. The `kWM_Echo` arm is the Echo
// silhouette: a solid black model between two `SetDestinationAlpha` calls, with the alpha
// derived from the flags' colour.
//
// The alpha reading is the best one reached (98.43% of 572 bytes, measured; the residual is
// `rlwinm r3,r4,1,23,30` against our `1,16,30` on the doubling, a `clrlwi r0,r4,24` re-mask of
// the intermediate max that we never emit, and retail's separate `clrlwi. r4,r28,24` byte test
// of `alpha` where we reuse the previous record). What is written here is therefore a good
// reading of retail's bytes, not a verified one - see
// docs/goal-notes/progress-prime1-cmodeldata.md for the 30 spellings that were measured.
void CModelData::Render(EWhichModel which, const CTransform4f& xf, const CActorLights* lights,
                        const CModelFlags& flags) const {
  if (which == kWM_Echo) {
    uchar alpha = 0;
    if (flags.GetTrans() == CModelFlags::kT_Two) {
      const CColor& color = flags.GetColorRef();
      const uint red = color.GetRedu8();
      const uint green = color.GetGreenu8();
      const uint blue = color.GetBlueu8();
      const uint mx = rstl::max_val(rstl::max_val(red, green), blue);
      const ushort doubled = static_cast< ushort >(mx) * 2;
      alpha = static_cast< uchar >(doubled < 255 ? doubled : 255);
      if (alpha != 0) {
        gpRender->SetDestinationAlpha(mx);
      }
    }
    RenderSolid(which, xf, !mRenderFullEchoModel,
                CModelFlags(CModelFlags::kT_One, 0,
                            static_cast< CModelFlags::EFlags >(
                                CModelFlags::kF_DepthCompare | CModelFlags::kF_DepthUpdate),
                            CColor::Black()));
    if (alpha != 0) {
      gpRender->SetDestinationAlpha(0);
    }
    return;
  }
  CTransform4f scaledXf = xf;
  scaledXf *= CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ());
  gpRender->SetModelMatrix(scaledXf);
  if (lights != nullptr && which != kWM_Dark) {
    lights->ActivateLights();
  } else {
    CGraphics::DisableAllLights();
    gpRender->SetAmbientColor(mAmbientColor);
  }
  if (HasAnimation()) {
    mAnimData->Render(PickAnimatedModel(which), flags);
  } else if (mNormalModel) {
    const CModel* const model = *PickStaticModel(which);
    if (mRenderSorted) {
      model->DrawSortedParts(flags);
    } else {
      model->Draw(flags);
    }
  }
  gpRender->SetAmbientColor(CColor::White());
  CGraphics::DisableAllLights();
  mRenderSorted = false;
}

// Echoes' fourth early-out is `!mRenderUnsortedParts`, a member Prime 1 does not have: retail
// branches *over* the fallback when the bit is set (`rlwinm. r0,r0,27,31,31`, which is bit 2 of the
// flag byte, i.e. `mRenderUnsortedParts`) and falls into it when it is clear. Written as an
// `||` chain, not as a nested `if` - the nested form drops to 92.26% because it duplicates the
// fallback block. `static_cast<char>` on `GetTrans()` is what produces retail's `extsb`; the
// same spelling is already at `CActor.cpp:606`.
void CModelData::RenderUnsortedParts(EWhichModel which, const CTransform4f& xf,
                                     const CActorLights* lights, const CModelFlags& flags) const {
  if (HasAnimation() || !mNormalModel || static_cast< char >(flags.GetTrans()) > 4 ||
      !mRenderUnsortedParts) {
    mRenderSorted = false;
    return;
  }
  const CTransform4f scaledXf =
      xf * CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ());
  gpRender->SetModelMatrix(scaledXf);
  if (lights != nullptr && which != kWM_Dark) {
    lights->ActivateLights();
  } else {
    CGraphics::DisableAllLights();
    gpRender->SetAmbientColor(mAmbientColor);
  }
  (*PickStaticModel(which))->DrawUnsortedParts(flags);
  gpRender->SetAmbientColor(CColor::White());
  CGraphics::DisableAllLights();
  mRenderSorted = true;
}

void CModelData::MultipassDrawCallback(const SSkinningWorkspace& workspace,
                                       const SModelDataMultipassContext& context) {
  // TODO: For each pass, set its color and portal plane, then draw the skinned model
  // with the corresponding flags and 64-bit surface mask.
}

void CModelData::DisintegrateDraw(EWhichModel which, const CTransform4f& xf,
                                  const CTexture& texture, const CColor& color,
                                  float amount) const {
  // TODO: Submit the static or posed model through the renderer's model-input wrapper.
}

void CModelData::DisintegrateDraw(const CStateManager& mgr, const CTransform4f& xf,
                                  const CTexture& texture, const CColor& color,
                                  float amount) const {
  DisintegrateDraw(GetRenderingModel(mgr), xf, texture, color, amount);
}

void CModelData::RenderNoise(EWhichModel which, const CTransform4f& xf, const CColor& color,
                             bool additive) const {
  // TODO: Submit the scaled static or posed model to the noise-rendering path.
}

void CModelData::RenderNoise(const CStateManager& mgr, const CTransform4f& xf, const CColor& color,
                             bool additive) const {
  RenderNoise(GetRenderingModel(mgr), xf, color, additive);
}

void CModelData::RenderSolid(EWhichModel which, const CTransform4f& xf, bool unsortedOnly,
                             const CModelFlags& flags) const {
  // TODO: Flat rendering through the shared static/skinned model-input wrapper.
}

void CModelData::RenderModelMultipleTimesWithFlags(EWhichModel which, const CTransform4f& xf,
                                                   const CActorLights* lights,
                                                   const CModelFlags* flags, const u64* masks,
                                                   const CColor* colors, const CPlane* planes,
                                                   int count) const {
  // TODO: Set scaled transform and lighting, then submit the static passes or skinning callback.
}

void CModelData::Touch(const CStateManager& mgr, int shaderIdx) const {
  if (!mTexturesLocked) {
    return;
  }
  for (int which = kWM_Normal; which <= kWM_Echo; ++which) {
    Touch(static_cast< EWhichModel >(which), shaderIdx);
  }
}

void CModelData::Touch(EWhichModel which, int shaderIdx) const {
  if (!mTexturesLocked) {
    return;
  }
  if (HasAnimation()) {
#ifdef TARGET_PC
    // The holder `fn_80027B44` takes is not a thing the host has; the token it would reach
    // through the holder's +8 is the model's, and `Touch` is what the helper calls on it.
    (*PickAnimatedModel(which).GetModel())->Touch(shaderIdx);
#else
    fn_80027B44(reinterpret_cast< const SModelHolder* >(&PickAnimatedModel(which)), shaderIdx);
#endif
  } else {
    (*PickStaticModel(which))->Touch(shaderIdx);
  }
}

void CModelData::Touch() const {
  if (!mTexturesLocked) {
    return;
  }
  if (HasAnimation()) {
    for (int which = kWM_Normal; which <= kWM_Echo; ++which) {
#ifdef TARGET_PC
      // `fn_80027AE8` is that same walk over the same token, spelled out the way
      // `CGunEffectTouchAll.cpp:61-66` spells it out on the host.
      const CModel* const model = *PickAnimatedModel(static_cast< EWhichModel >(which)).GetModel();
      for (int shader = 0, n = model->GetMatSetCount(); shader < n; ++shader) {
        model->Touch(shader);
      }
#else
      fn_80027AE8(reinterpret_cast< const SModelHolder* >(
          &PickAnimatedModel(static_cast< EWhichModel >(which))));
#endif
    }
  } else {
    for (int which = kWM_Normal; which <= kWM_Echo; ++which) {
      const CModel* const model = *PickStaticModel(static_cast< EWhichModel >(which));
#ifdef TARGET_PC
      const int numShaders = model->GetMatSetCount();
#else
      const int numShaders = reinterpret_cast< const SShaderCount* >(model)->mNumShaders;
#endif
      for (int shader = 0; shader < numShaders; ++shader) {
        model->Touch(shader);
      }
    }
  }
}

void CModelData::RenderParticles(const CFrustumPlanes& planes) const {
  if (HasAnimation()) {
    mAnimData->GetParticleDB().AddToRendererClipped(planes);
  }
}

bool CModelData::IsAnimating() const { return HasAnimation() && mAnimData->IsAnimating(); }

CAdvancementDeltas CModelData::AdvanceAnimation(float dt, CStateManager& mgr, TAreaId aid,
                                                bool advTree, float cameraDistance) {
  // TODO: Delegate to CAnimData::Advance with the manager's embedded random generator.
  // CStateManager's declaration does not yet expose that recovered member.
  return skNullAdvance;
}

CAdvancementDeltas CModelData::AdvanceAnimation(float dt, CRandom16& random, bool advTree) {
  if (!HasAnimation()) {
    return skNullAdvance;
  }
  static const TAreaId areaId = kInvalidAreaId;
  return mAnimData->Advance(dt, 0.f, mScale, nullptr, random, areaId, advTree);
}

CAdvancementDeltas CModelData::AdvanceAnimationIgnoreParticles(float dt, CRandom16& random,
                                                               bool advTree) {
  if (!HasAnimation()) {
    return skNullAdvance;
  }
  return mAnimData->AdvanceIgnoreParticles(dt, random, advTree);
}

CTransform4f CModelData::GetLocatorTransform(const rstl::string& name) const {
  if (!HasAnimation()) {
    return CTransform4f::Identity();
  }
  return mAnimData->GetLocatorTransform(name, nullptr);
}

CTransform4f CModelData::GetLocatorTransform(const CSegId& id) const {
  if (!HasAnimation()) {
    return CTransform4f::Identity();
  }
  return mAnimData->GetLocatorTransform(id, nullptr);
}

CTransform4f CModelData::GetLocatorTransformDynamic(const rstl::string& name,
                                                    const CCharAnimTime* time) const {
  if (!HasAnimation()) {
    return CTransform4f::Identity();
  }
  return mAnimData->GetLocatorTransform(name, time);
}

CTransform4f CModelData::GetLocatorTransformDynamic(const CSegId& id,
                                                    const CCharAnimTime* time) const {
  if (!HasAnimation()) {
    return CTransform4f::Identity();
  }
  return mAnimData->GetLocatorTransform(id, time);
}

CTransform4f CModelData::GetScaledLocatorTransform(const rstl::string& name) const {
  CTransform4f xf = GetLocatorTransform(name);
  xf.SetTranslation(CVector3f::ByElementMultiply(mScale, xf.GetTranslation()));
  return xf;
}

CTransform4f CModelData::GetScaledLocatorTransform(const CSegId& id) const {
  CTransform4f xf = GetLocatorTransform(id);
  xf.SetTranslation(CVector3f::ByElementMultiply(mScale, xf.GetTranslation()));
  return xf;
}

CTransform4f CModelData::GetScaledLocatorTransformDynamic(const rstl::string& name,
                                                          const CCharAnimTime* time) const {
  CTransform4f xf = GetLocatorTransformDynamic(name, time);
  xf.SetTranslation(CVector3f::ByElementMultiply(mScale, xf.GetTranslation()));
  return xf;
}

CTransform4f CModelData::GetScaledLocatorTransformDynamic(const CSegId& id,
                                                          const CCharAnimTime* time) const {
  CTransform4f xf = GetLocatorTransformDynamic(id, time);
  xf.SetTranslation(CVector3f::ByElementMultiply(mScale, xf.GetTranslation()));
  return xf;
}

CAABox CModelData::GetBounds(const CTransform4f& xf) const {
  const CTransform4f scaledXf =
      xf * CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ());
  if (HasAnimation()) {
    return mAnimData->GetBoundingBox(scaledXf);
  }

  CAABox bounds = (*mNormalModel)->GetAABB();
  if (mEchoModel) {
    bounds.Include((*mEchoModel)->GetAABB());
  }
  if (mDarkModel) {
    bounds.Include((*mDarkModel)->GetAABB());
  }
  return bounds.GetTransformedAABox(scaledXf);
}

CAABox CModelData::GetBounds() const {
  if (HasAnimation()) {
    return mAnimData->GetBoundingBox(
        CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ()));
  }

  CAABox bounds = (*mNormalModel)->GetAABB();
  if (mEchoModel) {
    bounds.Include((*mEchoModel)->GetAABB());
  }
  if (mDarkModel) {
    bounds.Include((*mDarkModel)->GetAABB());
  }
  return CAABox(CVector3f::ByElementMultiply(bounds.GetMinPoint(), mScale),
                CVector3f::ByElementMultiply(bounds.GetMaxPoint(), mScale));
}

void CModelData::AdvanceParticles(const CTransform4f& xf, float dt, CStateManager& mgr) {
  if (HasAnimation()) {
    mAnimData->AdvanceParticles(xf, dt, mScale, &mgr);
  }
}

void CModelData::EnableLooping(bool enable) {
  if (HasAnimation()) {
    mAnimData->EnableLooping(enable);
  }
}

// `mAnimData.null()` rather than `!HasAnimation()`, and an `if`/return rather than a ternary:
// retail branches around the null case, and mwcceppc only produces that shape from the direct
// `null()` test. Measured, 3 spellings each (see docs/goal-notes/progress-prime1-cmodeldata.md).
float CModelData::GetAnimationDuration(int anim) const {
  if (mAnimData.null()) {
    return 0.f;
  }
  return mAnimData->GetAnimationDuration(anim);
}

// Same shape, but this one is 62.5% and is left there: retail's `rlwinm r3,r0,26,31,31` returns
// the `mLoop` bit in place with no `neg`/`or`/`srwi` bool normalisation, which only happens if
// `CAnimData::mLoop` is a `bool` bitfield. Changing that shared header to `bool` takes this to
// 100% but costs five functions in four other units (`CGSFreeLook::Update`, `CGSComboFire::Update`
// and `CGunWeapon::PlayAnim` all leave 100%, plus two more), so it is not worth one here.
bool CModelData::GetIsLoop() const {
  if (mAnimData.null()) {
    return false;
  }
  return mAnimData->GetIsLoop();
}

bool CModelData::IsDefinitelyOpaque(EWhichModel which) const {
  // TODO: Query the selected CModel's opaque-material flag after its interface is recovered.
  return false;
}

void CModelData::SetEchoModel(const rstl::pair< CAssetId, CAssetId >& assets) {
  // TODO: Validate CMDL/CSKR resources and replace the static token or animated Echo model.
}

void CModelData::SetDarkModel(const rstl::pair< CAssetId, CAssetId >& assets) {
  // TODO: Validate CMDL/CSKR resources and replace the static token or animated Dark model.
}

const TLockedToken< CModel >& CModelData::PickStaticModel(EWhichModel which) const {
  switch (which) {
  case kWM_Echo:
    if (mEchoModel) {
      return *mEchoModel;
    }
    break;
  case kWM_Dark:
    if (mDarkModel) {
      return *mDarkModel;
    }
    break;
  default:
    break;
  }
  return *mNormalModel;
}

CSkinnedModel& CModelData::PickAnimatedModel(EWhichModel which) const {
  CSkinnedModel* model = nullptr;
  switch (which) {
  case kWM_Echo:
    model = mAnimData->GetXRayModel();
    break;
  case kWM_Dark:
    model = mAnimData->GetInfraModel();
    break;
  default:
    break;
  }
  return model ? *model : **mAnimData->GetModelData();
}

CModelData::EWhichModel CModelData::GetRenderingModel(const CStateManager& mgr,
                                                      const CPlayerState& playerState) {
  switch (playerState.GetActiveVisor(mgr)) {
  case CPlayerState::kPV_Echo:
    return kWM_Echo;
  case CPlayerState::kPV_Dark:
    return kWM_Dark;
  default:
    return kWM_Normal;
  }
}

CModelData::EWhichModel CModelData::GetRenderingModel(const CStateManager& mgr) {
  return GetRenderingModel(mgr, *mgr.GetPlayerState());
}

void CModelData::Render(const CStateManager& mgr, const CTransform4f& xf,
                        const CActorLights* lights, const CModelFlags& flags) const {
  Render(GetRenderingModel(mgr), xf, lights, flags);
}

bool CModelData::IsLoaded(int shaderIdx) const {
  if (HasAnimation()) {
    // Named local: retail keeps the `CAnimData*` in a callee-saved register for all three
    // animated-model loads. Re-reading `mAnimData` after the first call costs one instruction.
    const CAnimData* const animData = mAnimData.get();
    if (!animData->GetModelData()->GetModel()->IsLoaded(shaderIdx)) {
      return false;
    }
    if (CSkinnedModel* echo = animData->GetXRayModel()) {
      if (!echo->GetModel()->IsLoaded(shaderIdx)) {
        return false;
      }
    }
    if (CSkinnedModel* dark = animData->GetInfraModel()) {
      if (!dark->GetModel()->IsLoaded(shaderIdx)) {
        return false;
      }
    }
  }
  if (mNormalModel && !(*mNormalModel)->IsLoaded(shaderIdx)) {
    return false;
  }
  if (mEchoModel && !(*mEchoModel)->IsLoaded(shaderIdx)) {
    return false;
  }
  if (mDarkModel && !(*mDarkModel)->IsLoaded(shaderIdx)) {
    return false;
  }
  return true;
}

int CModelData::GetNumShaders() const {
  if (HasAnimation()) {
    // `TLockedToken::operator*` hands back the `CModel*` rather than a reference, so the static
    // arm needs the extra dereference that the animated arm's `->GetModel()` already gives.
#ifdef TARGET_PC
    return (*mAnimData->GetModelData()->GetModel())->GetMatSetCount();
#else
    return reinterpret_cast< const SShaderCount* >(*mAnimData->GetModelData()->GetModel())
        ->mNumShaders;
#endif
  }
  if (mNormalModel) {
#ifdef TARGET_PC
    return (*(*mNormalModel))->GetMatSetCount();
#else
    return reinterpret_cast< const SShaderCount* >(*(*mNormalModel))->mNumShaders;
#endif
  }
  return 1;
}

void CModelData::LockTextures() {
  // TODO: Lock textures for every model variant and set mTexturesLocked once.
}

void CModelData::SetScale(const CVector3f& scale) {
  mScale = scale;
  if (HasAnimation()) {
    mAnimData->SetModelScale(scale);
  }
}

void CModelData::SetupWorldSpacePortalPlane(const CTransform4f& xf, const CPlane& plane) const {
  // TODO: Set the portal plane using both the scaled model matrix and its model-view matrix.
}
