#include "MetroidPrime/Player/CWorldTransManager.hpp"

#include "Kyoto/Text/CGuiTextSupport.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CARAMToken.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CVector2i.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CCameraFilterPass.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "rstl/list.hpp"
#include "rstl/math.hpp"
#include "dolphin/os.h"

// Retail 0x80216D50, a one-instruction `CTweakGame` float reader - `lwz r3,0(r3)` /
// `lfs f1,48(r3)` / `blr`, i.e. the float at +0x30 of the block `*gpTweakGame` points at. It is
// a member of the same `Tweaks` accessor family as `fn_80216D38` (+0x54), and since 2026-10-02
// all four of them are defined together by the carve
// `src/MetroidPrime/Tweaks/Carve80216D2C.c` (retail 0x80216D2C..0x80216D5C, `Matching`, which
// also replaced the port-only definition `fn_80216D38` had in
// `CTweakGameHardModeDamageMultiplier.cpp`). The declaration here stays `extern "C"` because
// the name is retail's own: renaming it is a `symbols.txt` change with module-hash
// consequences, and defining it in this unit would need the `Tweaks` layout.
extern "C" float fn_80216D50(CTweakGame* tweakGame);

// Retail holds both locator names as file-scope pointers in `.sdata2`. A function-local
// `static` instead makes mwcceppc emit its dynamic-initialisation guard on every call, which
// retail does not have (it is a bare `lwz r4,off(r2)`).
static const char* const kGunLocator = "GUN_LCTR";
static const char* const kGrappleLocator = "GRAPPLE_LCTR";

struct CWorldTransManager::SModelDatas {
  CAnimRes mSamusRes;
  CModelData mSamusModelData;
  // Guessed name: a second Samus model using animation 1; pass ownership is provisional.
  CModelData mSecondPassSamusModelData;
  CModelData mBeamModelData;
  CModelData mGrappleModelData;
  CModelData mPlatformModelData;
  CModelData mBgModelData;
  rstl::optional_object< CToken > mBeamModel;
  rstl::optional_object< CToken > mGrappleModel;
  rstl::optional_object< CToken > mSuitModel;
  rstl::optional_object< CToken > mSuitSkin;
  CTransform4f mGunXf;
  CTransform4f mGrappleXf;
  rstl::vector< CLight > mLights;
  CVector2f mShakeResult;
  CVector2f mShakeDelta;
  float mRandTimeout;
  float mBlurResult;
  float mBlurDelta;
  float mDissolveStartTime;
  float mDissolveEndTime;
  float mTransCompleteTime;
  bool mDissolveStarted;

  explicit SModelDatas(const CAnimRes& samusRes);
};
NESTED_CHECK_SIZEOF(CWorldTransManager, SModelDatas, 0x2b0)

CWorldTransManager::CWorldTransManager()
: mCurTime(0.f)
, mRandom(99)
, mSfx(0x258b)
, mVolume(127)
, mPanning(64)
, mTransType(kTT_Disabled)
, mTextStartTime(0.f)
, mTextElapsedTime(0.f)
, mIntroTextFadeTimer(0.f)
, mPortalFade(0.f)
, mCameraTransform(CTransform4f::Identity())
, mTransitionFinished(true)
, mStopSoon(false)
, mGoingUp(false)
, mFadeWhite(false)
, mTextDirty(false)
, mLongShaft(false) {}

CWorldTransManager::~CWorldTransManager() {}

CWorldTransManager::SModelDatas::SModelDatas(const CAnimRes& samusRes)
: mSamusRes(samusRes)
, mSamusModelData(CModelData::CModelDataNull())
, mSecondPassSamusModelData(CModelData::CModelDataNull())
, mBeamModelData(CModelData::CModelDataNull())
, mGrappleModelData(CModelData::CModelDataNull())
, mPlatformModelData(CModelData::CModelDataNull())
, mBgModelData(CModelData::CModelDataNull())
, mGunXf(CTransform4f::Identity())
, mGrappleXf(CTransform4f::Identity())
, mShakeResult(0.f, 0.f)
, mShakeDelta(0.f, 0.f)
, mRandTimeout(0.f)
, mBlurResult(0.f)
, mBlurDelta(0.f)
, mDissolveStartTime(99999.f)
, mDissolveEndTime(99999.f)
, mTransCompleteTime(99999.f)
, mDissolveStarted(false) {
  mLights.reserve(8);
}

void CWorldTransManager::DisableTransition() {
  mTransType = kTT_Disabled;
  mModelData = nullptr;
  mTextData = nullptr;
  mSubtitleData = nullptr;
  mDarkWorldInfo = rstl::optional_object< CDarkWorldInfo >();
  mPortalTransition = nullptr;
  mGoingUp = false;
}

// `fn_8007BBB8` is `CModelData`'s copy assignment. `CModelData` declares a copy *constructor*
// (`CModelData.hpp:79`) and no `operator=`, so `x = CModelData(...)` makes mwcceppc emit a weak
// COMDAT `__as__10CModelDataFRC10CModelData` of its own, which is in neither `symbols.txt` nor the
// linked ELF. Retail calls the real one at all three sites (0x8015BA30 / 0x8015BAB8 / 0x8015BB44),
// and it is in `symbols.txt`, so naming it is both correct and the only spelling that links.
extern "C" void fn_8007BBB8(CModelData* dst, const CModelData& src);

void CWorldTransManager::TouchModels() {
  // Retail touches the portal transition *first* (0x8015B9C4), before `mModelData`.
  if (!mPortalTransition.null()) {
    mPortalTransition->TouchModels();
  }
  SModelDatas* data = mModelData.get();
  if (data != nullptr) {
    if (data->mBeamModel && data->mBeamModel->IsLoaded()) {
      fn_8007BBB8(
          &data->mBeamModelData,
          CModelData(CStaticRes(data->mBeamModel->GetTag().GetId(), data->mSamusRes.GetScale())));
      data->mBeamModel = rstl::optional_object< CToken >();
    }
    if (data->mGrappleModel && data->mGrappleModel->IsLoaded()) {
      fn_8007BBB8(
          &data->mGrappleModelData,
          CModelData(
              CStaticRes(data->mGrappleModel->GetTag().GetId(), data->mSamusRes.GetScale())));
      data->mGrappleModel = rstl::optional_object< CToken >();
    }
    // The suit reskin rebuilds Samus' *own* model from `mSamusRes` and puts it on its default
    // animation, then consumes both suit tokens. Retail tests both optionals before either
    // `IsLoaded` (0x8015BAF8-0x8015BB30), and destroys the `CModelData` last (0x8015BBEC), so it
    // has to be a named local rather than a temporary.
    if (data->mSuitModel && data->mSuitSkin && data->mSuitModel->IsLoaded() &&
        data->mSuitSkin->IsLoaded()) {
      CModelData samusData(data->mSamusRes);
      fn_8007BBB8(&data->mSamusModelData, samusData);
      const CAnimPlaybackParms parms(data->mSamusRes.GetDefaultAnim(), nullptr, nullptr, nullptr,
                                      nullptr, false);
      data->mSamusModelData.AnimationData()->SetAnimation(parms, false);
      data->mSuitModel = rstl::optional_object< CToken >();
      data->mSuitSkin = rstl::optional_object< CToken >();
    }
    // Five touches, at +0x1C (Samus), +0x14C (platform), +0x198 (background), +0xB4 (beam) and
    // +0x100 (grapple). Retail has none at +0x68, so `mSecondPassSamusModelData` is not touched.
    if (!data->mSamusModelData.IsNull())
      data->mSamusModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mPlatformModelData.IsNull())
      data->mPlatformModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mBgModelData.IsNull())
      data->mBgModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mBeamModelData.IsNull())
      data->mBeamModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mGrappleModelData.IsNull())
      data->mGrappleModelData.Touch(CModelData::kWM_Normal, 0);
  }
}

void CWorldTransManager::EnableTransition(const CAnimRes& samusRes, bool renderGrapple,
                                          CAssetId platformRes, const CVector3f& platformScale,
                                          CAssetId bgRes, const CVector3f& bgScale, bool goingUp,
                                          const CGameCameraSpline* firstPassCamera,
                                          const CGameCameraSpline* secondPassCamera,
                                          const CTransform4f& cameraTransform,
                                          rstl::optional_object< CToken > soundGroup,
                                          const CDarkWorldInfo* darkWorldInfo) {
  mStopSoon = false;
  mTransType = kTT_Enabled;
  mGoingUp = goingUp;
  mModelData = rs_new SModelDatas(samusRes);
  mTextData = nullptr;
  mSubtitleData = nullptr;
  if (firstPassCamera != nullptr) {
    mFirstPassCamera = *firstPassCamera;
  }
  if (secondPassCamera != nullptr) {
    mSecondPassCamera = *secondPassCamera;
  }
  mCameraTransform = cameraTransform;
  mRandom.SetSeed(99);

  // TODO: Set up both Samus animations, model resources, bounds and shaft lighting.
  mSoundGroup = soundGroup;
  if (mSoundGroup) {
    mSoundGroup->Lock();
  }
  if (darkWorldInfo != nullptr) {
    mDarkWorldInfo = *darkWorldInfo;
  }
  StartTransition();
  TouchModels();
}

void CWorldTransManager::StartTransition() {
  mCurTime = 0.f;
  mBgOffset = 0.f;
  mLightOffset = 0.f;
  mTransitionFinished = false;
  mTextDirty = true;
}

void CWorldTransManager::EndTransition() {
  mCharacterFactory = rstl::optional_object< TLockedToken< CCharacterFactory > >();
  DisableTransition();
}

void CWorldTransManager::Update(float dt) {
  mCurTime += dt;
  switch (mTransType) {
  case kTT_Enabled:
    UpdateEnabled(dt);
    break;
  case kTT_Text:
    UpdateText(dt);
    break;
  case kTT_Disabled:
    UpdateDisabled(dt);
    break;
  case kTT_Portal:
    UpdatePortalTransition(dt);
    break;
  }
}

void CWorldTransManager::UpdateDisabled(float dt) {
  if (mCurTime > 2.f) {
    mTransitionFinished = true;
  }
}

// `CPortalTransition` is declared but has no `.cpp` in this tree, so its four callees stay
// `extern "C"` and undefined, named in `config/G2ME01/symbols.txt` off the unit that already
// owns `__dt__17CPortalTransitionFv` - the same arrangement `fn_80216D50` below uses. This costs
// the port nothing: this file is not in `files.cmake`, so `mp_game` never compiles it.
extern "C" bool fn_80230000(CPortalTransition* transition);
extern "C" bool fn_80230D68(CPortalTransition* transition);
extern "C" void fn_80230594(CPortalTransition* transition, float dt);
extern "C" void fn_802300E8(CPortalTransition* transition);

void CWorldTransManager::UpdatePortalTransition(float dt) {
  if (mPortalTransition.null())
    return;
  if (fn_80230000(mPortalTransition.get())) {
    const float dir = fn_80230D68(mPortalTransition.get()) ? -1.f : 1.f;
    // `dt / 2.f`, not `dt * 0.5f`: both fold to the same multiply, but only the division spelling
    // puts `f31` (dt) first in retail's `fmuls f2,f31,f0` at 0x8015A244.
    const float fade = dt / 2.f;
    mPortalFade = CMath::Clamp(0.f, mPortalFade + fade * dir, 1.f);
  }
  fn_80230594(mPortalTransition.get(), dt);
  // `<=`, not `>=`: retail's `fcmpo cr0,f1,f0` + `cror eq,lt,eq` at 0x8015A298/9C is
  // "mPortalFade <= 0.f"; `>=` emits `cror eq,gt,eq` and costs the function its 100%.
  if (fn_80230D68(mPortalTransition.get()) && mPortalFade <= 0.f)
    mTransitionFinished = true;
}

void CWorldTransManager::UpdateEnabled(float dt) {
  if (!mModelData.null() && !mModelData->mSamusModelData.IsNull()) {
    // Echoes uses 4.f everywhere Prime 1 wrote 2.f in this block: the threshold, and the
    // subtrahend of the `<const> + mCurTime - <const>` expressions below, are all the same
    // literal, so retail holds it in one register for the compare, the add and the sub. The
    // add/sub pair around `mCurTime` really is in retail (0x80159e88/8c) and is kept verbatim
    // rather than simplified, because simplifying it deletes two instructions.
    if (mStopSoon && !mModelData->mDissolveStarted && mCurTime >= 4.f) {
      mModelData->mDissolveStarted = true;
      mModelData->mDissolveStartTime = mCurTime;
      mModelData->mDissolveEndTime = 4.f + mCurTime - 4.f;
      if (mSecondPassCamera) {
        mModelData->mTransCompleteTime = mCurTime + mSecondPassCamera->GetDuration();
        const CAnimPlaybackParms parms(1, -1, 1.f, true);
        mModelData->mSamusModelData.AnimationData()->SetAnimation(parms, false);
        mModelData->mSamusModelData.AnimationData()->EnableLooping(false);
      } else {
        mModelData->mTransCompleteTime = 5.f + mCurTime - 4.f;
      }
    }
    if (mCurTime > mModelData->mTransCompleteTime && mModelData->mDissolveStarted)
      mTransitionFinished = true;

    mModelData->mSamusModelData.AdvanceAnimationIgnoreParticles(dt, mRandom, true);
    mModelData->mGunXf =
        mModelData->mSamusModelData.GetScaledLocatorTransform(rstl::string_l(kGunLocator));
    mModelData->mGrappleXf =
        mModelData->mSamusModelData.GetScaledLocatorTransform(rstl::string_l(kGrappleLocator));
    mModelData->mRandTimeout -= dt;
    if (mModelData->mRandTimeout <= 0.f) {
      mModelData->mRandTimeout = mRandom.Range(0.016666668f, 0.1f);
      CVector2f randVec(mRandom.Range(-0.025f, 0.025f), mRandom.Range(-0.075f, 0.075f));
      mModelData->mShakeDelta = (randVec - mModelData->mShakeResult) / mModelData->mRandTimeout;
      const float blur = mRandom.Range(-2.f, 4.f);
      mModelData->mBlurDelta = (blur - mModelData->mBlurResult) / mModelData->mRandTimeout;
    }
    mModelData->mShakeResult += mModelData->mShakeDelta * dt;
    mModelData->mBlurResult += dt * mModelData->mBlurDelta;
  }

  // Echoes scrolls the background and the light layer at their own rates (37.5 and 18.75 units
  // per second) where Prime 1 moved the background at 50.
  float delta = 37.5f * dt;
  if (mGoingUp)
    delta = -delta;
  mBgOffset += delta;
  if (mBgOffset > mBgHeight)
    mBgOffset -= mBgHeight;
  if (mBgOffset < 0.f)
    mBgOffset += mBgHeight;
  float lightDelta = 18.75f * dt;
  if (mGoingUp)
    lightDelta = -lightDelta;
  mLightOffset += lightDelta;
  if (mLightOffset > mLightHeight)
    mLightOffset -= mLightHeight;
  if (mLightOffset < 0.f)
    mLightOffset += mLightHeight;
  UpdateLights(dt);
}

void CWorldTransManager::Draw() const {
  switch (mTransType) {
  case kTT_Enabled:
    DrawEnabled();
    break;
  case kTT_Text:
    DrawText();
    break;
  case kTT_Disabled:
    DrawDisabled();
    break;
  case kTT_Portal:
    DrawPortalTransition();
    break;
  }
}

void CWorldTransManager::UpdateLights(float) {
  if (mModelData.null())
    return;

  CColor pointColor = CColor::White();
  CColor shaftColor = CColor::White();
  if (mLongShaft) {
    shaftColor = CColor(uchar(215), uchar(220), uchar(193), uchar(225));
  }
  rstl::vector< CLight >& lights = mModelData->mLights;
  lights.clear();
  const CVector3f lightPos(0.f, 1.2f, 0.f);
  CLight point = CLight::BuildPoint(lightPos, pointColor);
  point.SetAttenuation(0.f, 0.f, 0.1f);
  CLight movingPoint = point;
  movingPoint.SetColor(shaftColor);
  movingPoint.SetPosition(
      CVector3f(lightPos.GetX(), lightPos.GetY(),
                lightPos.GetZ() + 2.f * mLightOffset - mLightHeight));
  float intensity = 1.f;
  if (!mGoingUp && mLightHeight - mLightOffset < 2.f)
    intensity = (mLightHeight - mLightOffset) / 2.f;
  else if (mGoingUp && mLightOffset < 2.f)
    intensity = mLightOffset / 2.f;

  if (intensity < 1.f) {
    CLight shaft = point;
    shaft.SetPosition(CVector3f(lightPos.GetX(), lightPos.GetY(),
                                lightPos.GetZ() + (mGoingUp ? mLightHeight : -mLightHeight)));
    shaft.SetColor(CColor::Lerp(CColor::Black(), point.GetColor(), 1.f - intensity));
    lights.push_back(shaft);
    movingPoint.SetColor(CColor::Lerp(CColor::Black(), movingPoint.GetColor(), intensity));
  }
  lights.push_back(movingPoint);
}

float CWorldTransManager::GetCameraFov(int pass) const {
  if (pass == 0 && mFirstPassCamera) {
    return const_cast< CGameCameraSpline& >(*mFirstPassCamera).GetFovByTime(mCurTime);
  }
  if (pass == 1 && mSecondPassCamera) {
    return const_cast< CGameCameraSpline& >(*mSecondPassCamera).GetFovByTime(
        mCurTime - mModelData->mDissolveStartTime);
  }
  return fn_80216D50(gpTweakGame.get());
}

CTransform4f CWorldTransManager::GetCameraTransform(int pass) const {
  // `CGameSpline`'s evaluators are non-const in retail too, and it calls them through this
  // pointer straight out of a `const CWorldTransManager`, so the const has to come off here.
  //
  // Both no-camera fallbacks name their product (`xf`) and `return` it, and both guards are
  // written `if (!camera) { ...return... }` rather than `if (camera) {...} else {...}`: retail
  // branches *over* the fallback and falls through into it, which is the shape an early return
  // inside the test produces. A bare `return A * B;` gets the product written straight into the
  // return slot; the named local is what produces retail's extra copy-construct pair.
  CGameCameraSpline* spline = nullptr;
  float time = 0.f;
  if (pass == 0) {
    if (!mFirstPassCamera) {
      const float rotationT = CMath::Clamp(0.f, mCurTime / 25.f, 100.f);
      const float translationT = CMath::Clamp(0.f, mCurTime / 10.f, 1.f);
      const CRelAngle angle = CRelAngle::FromDegrees(360.f * rotationT + 180.f - 90.f);
      const CVector2f& shake = mModelData->mShakeResult;
      CTransform4f xf =
          CTransform4f::RotateZ(angle) *
          CTransform4f::Translate(shake.GetX(), -3.5f * (1.f - translationT) + -3.5f,
                                  2.f + shake.GetY());
      return xf;
    }
    // Assigning `time` first is what makes retail's two instructions come out in its order
    // (`lfs f31,0(r30)` then `addi r31,r30,248`); the other way round is 2 instructions out.
    time = mCurTime;
    spline = const_cast< CGameCameraSpline* >(&*mFirstPassCamera);
  }
  if (pass == 1) {
    if (!mSecondPassCamera) {
      const float t = CMath::Clamp(0.f, (2.f + (mCurTime - mModelData->mDissolveStartTime)) / 5.f, 1.f);
      const CRelAngle angle = CRelAngle::FromDegrees(48.f * t + 180.f - 24.f);
      const CVector3f& scale = mModelData->mSamusRes.GetScale();
      const CVector3f v(-0.1f * scale.GetX(), -0.5f * scale.GetY(), 1.5f * scale.GetZ());
      CTransform4f xf = CTransform4f::RotateZ(angle) * CTransform4f::Translate(v);
      return xf;
    }
    spline = const_cast< CGameCameraSpline* >(&*mSecondPassCamera);
    time = mCurTime - mModelData->mDissolveStartTime;
  }

  CVector3f pos = spline->GetPositionByTime(time);
  CVector3f lookAt = spline->GetLookAtByTime(time);
  pos = mCameraTransform * pos;
  lookAt = mCameraTransform * lookAt;
  return CTransform4f::LookAt(pos, lookAt);
}
void CWorldTransManager::DrawAllModels() const {
  SModelDatas& data = *mModelData.get();
  CActorLights lights(0, CVector3f::Zero(), 4, 4);
  lights.BuildFakeLightList(data.mLights, CColor(0.1f, 0.1f, 0.1f, 1.f));
  if (!data.mBgModelData.IsNull()) {
    data.mBgModelData.Render(
        CModelData::kWM_Normal, CTransform4f::Translate(0.f, 0.f, -(2.f * mBgHeight - mBgOffset)),
        &lights, CModelFlags::Normal());
    data.mBgModelData.Render(CModelData::kWM_Normal,
                             CTransform4f::Translate(0.f, 0.f, mBgOffset - mBgHeight), &lights,
                             CModelFlags::Normal());
    data.mBgModelData.Render(CModelData::kWM_Normal,
                             CTransform4f::Translate(0.f, 0.f, mBgOffset), &lights,
                             CModelFlags::Normal());
  }
  if (!data.mPlatformModelData.IsNull())
    data.mPlatformModelData.Render(CModelData::kWM_Normal, CTransform4f::Identity(), &lights,
                                   CModelFlags::Normal());
  if (!data.mSamusModelData.IsNull()) {
    const CTransform4f& samusXf = CTransform4f::Identity();
    data.mSamusModelData.AnimationData()->PreRender();
    data.mSamusModelData.Render(CModelData::kWM_Normal, samusXf, &lights, CModelFlags::Normal());
    if (!data.mBeamModelData.IsNull())
      data.mBeamModelData.Render(CModelData::kWM_Normal, samusXf * data.mGunXf, &lights,
                                 CModelFlags::Normal());
  }
}

void CWorldTransManager::DrawFirstPass() const {
  const float fov = GetCameraFov(0);
  const float nearPlane = CCameraManager::GetDefaultFirstPersonNearClipDistance();
  const float farPlane = CCameraManager::GetDefaultFirstPersonFarClipDistance();
  gpRender->SetPerspective(fov * 0.7f, 1.42f, nearPlane, farPlane);
  CGraphics::SetViewPointMatrix(GetCameraTransform(0));
  DrawAllModels();
}

void CWorldTransManager::DrawSecondPass() const {
  const float fov = GetCameraFov(1);
  const float nearPlane = CCameraManager::GetDefaultFirstPersonNearClipDistance();
  const float farPlane = CCameraManager::GetDefaultFirstPersonFarClipDistance();
  gpRender->SetPerspective(fov * 0.7f, 1.42f, nearPlane, farPlane);
  CGraphics::SetViewPointMatrix(GetCameraTransform(1));
  DrawAllModels();
}

void CWorldTransManager::DrawEnabled() const {
  if (mModelData.null())
    return;

  gpRender->SetRequestRGBA6(true);
  const float drawTime = mCurTime;
  if (drawTime <= mModelData->mDissolveStartTime) {
    DrawFirstPass();
  } else if (drawTime > mModelData->mDissolveStartTime) {
    DrawSecondPass();
  }
  CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Multiply, CCameraFilterPass::kFS_CinemaBars,
                                CColor::Black(), nullptr, 1.f);
  const float fadeTime = mCurTime;
  float filterAlpha = 0.f;
  if (fadeTime < 0.25f)
    filterAlpha = 1.f - fadeTime / 0.25f;
  else if (fadeTime > mModelData->mTransCompleteTime)
    filterAlpha = 1.f;
  else if (fadeTime > mModelData->mTransCompleteTime - 0.25f)
    filterAlpha = 1.f - (mModelData->mTransCompleteTime - fadeTime) / 0.25f;
  if (filterAlpha > 0.f) {
    const CColor filterColor(0.f, 0.f, 0.f, filterAlpha);
    CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Blend, CCameraFilterPass::kFS_Fullscreen,
                                  filterColor, nullptr, 1.f);
  }
  CGraphics::SetIsBeginSceneClearFb(true);
}

void CWorldTransManager::DrawDisabled() const {
  const CColor color = CColor(uchar(0), uchar(0), uchar(0), uchar(3));
  CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Blend, CCameraFilterPass::kFS_Fullscreen,
                                color, nullptr, 1.f);
}

void CWorldTransManager::DrawPortalTransition() const {
  if (mPortalTransition.null())
    return;
  fn_802300E8(mPortalTransition.get());
  CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Add, CCameraFilterPass::kFS_Fullscreen,
                                CColor::Lerp(CColor::White(), CColor::Black(), mPortalFade),
                                nullptr, 1.f);
  CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Multiply, CCameraFilterPass::kFS_CinemaBars,
                                CColor::Black(), nullptr, 1.f);
  CGraphics::SetIsBeginSceneClearFb(true);
}

void CWorldTransManager::SfxStart() {
  if (!mSfxHandle && mSfx != CSfxManager::kInternalInvalidSfxId) {
    mSfxHandle = CSfxManager::SfxStart(mSfx, mVolume, mPanning, CSfxManager::kAllAreas, false, true,
                                       CSfxManager::kMedPriority);
  }
}

void CWorldTransManager::SfxStop() {
  if (mSfxHandle) {
    CSfxManager::SfxStop(mSfxHandle);
    mSfxHandle.Clear();
  }
}

void CWorldTransManager::SetSfx(ushort sfx, uchar volume, uchar panning) {
  mSfx = sfx;
  mVolume = volume;
  mPanning = panning;
}

void CWorldTransManager::EnableTransition(CAssetId fontId, CAssetId stringId, int stringIdx,
                                          bool fadeWhite, float charFadeTime, float charFadeRate,
                                          float textStartTime, float textEndDelay,
                                          float subtitleFadeInDelay, float subtitleFadeTime,
                                          const rstl::string& audioStream, uchar volume,
                                          bool displaySubtitles, bool introText) {
  mIntroText = introText;
  mIntroTextSeen = false;
  mAudioStream = audioStream;
  mStrIdx = stringIdx;
  mDisplaySubtitles = displaySubtitles;
  mTextStartTime = textStartTime;
  mTextEndDelay = textEndDelay;
  mSubtitleFadeInDelay = subtitleFadeInDelay;
  mSubtitleFadeTime = rstl::max_val(0.0001f, subtitleFadeTime);
  mVolume = volume;
  mStopSoon = false;
  mTransType = kTT_Text;
  mModelData = nullptr;
  mFadeWhite = fadeWhite;
  // TODO: Create text/subtitle support, configure typewriter effects and lock the string table.
  StartTransition();
}

void CWorldTransManager::EnableTransition(rstl::single_ptr< CPortalTransition >& transition,
                                          uchar volume) {
  if (!transition.null()) {
    mTransType = kTT_Portal;
    mPortalTransition = transition;
  }
  mStopSoon = false;
  mTextData = nullptr;
  mSubtitleData = nullptr;
  mVolume = volume;
  mPortalFade = 0.f;
  StartTransition();
  TouchModels();
}

void CWorldTransManager::UpdateText(float dt) {
  // TODO: Load/update text, subtitles and streamed audio; handle intro skipping and fades.
}

// Retail 0x80158DC4, 48 bytes: it reads two ints out of `.rodata` and builds a `CVector2i` in
// the caller's sret slot - the video resolution, (640, 480). It is in `symbols.txt`, so it stays
// `extern "C"` and undefined.
extern "C" CVector2i fn_80158DC4();

void CWorldTransManager::DrawText() const {
  gpRender->SetViewportOrtho(false, -4096.f, 4096.f);
  // Echoes positions the intro text from the video resolution; Prime 1 wrote a fixed
  // `Translate(0, 0, 448)`. The pair built at r1+280 is (176, -y), and the third component of
  // the translation is that pair's x less 176 - retail loads both operands with `lfd` and does a
  // single `fsubs` (0x80157BC8/CC), which on Gekko subtracts the single-precision halves, so it
  // folds to 0. The 32/0 selector is a named local so that its bitfield test is emitted before
  // the resolution accessor, which is retail's order.
  const float introX = mIntroText ? 32.f : 0.f;
  const CVector2f pos(176.f, -static_cast< float >(fn_80158DC4().GetY()));
  gpRender->SetModelMatrix(CTransform4f::Translate(introX, 0.f, pos.GetX() - 176.f));
  CGraphics::SetCullMode(kCM_None);
  gpRender->SetDepthReadWrite(false, false);
  gpRender->SetBlendMode_AdditiveAlpha();
  mTextData->Render();
  if (mDisplaySubtitles) {
    gpRender->SetModelMatrix(CTransform4f::Translate(0.f, 0.f, 120.f) * CTransform4f::Scale(1.f));
    mSubtitleData->Render();
  }

  float filterAlpha = 0.f;
  if (mCurTime < 1.f)
    filterAlpha = 1.f - rstl::min_val(1.f, mCurTime);
  else if (mStopSoon)
    filterAlpha = rstl::min_val(1.f, mCurTime - mStopTime);
  if (filterAlpha > 0.f) {
    const CColor filterColor =
        (mFadeWhite ? CColor::White() : CColor::Black()).WithAlphaOf(filterAlpha);
    CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Blend, CCameraFilterPass::kFS_Fullscreen,
                                  filterColor, nullptr, 1.f);
  }
  CGraphics::SetIsBeginSceneClearFb(true);
}

void CWorldTransManager::StartTextFadeOut() {
  if (!mStopSoon) {
    mStopTime = mCurTime;
  }
  mStopSoon = true;
}

void CWorldTransManager::CheckIntroTextSeen() {
  if (gpGameState->SystemOptions().FindEnvironmentVariable("SeenIntroText")->GetValue() != 0) {
    mIntroTextSeen = true;
  }
}

bool CWorldTransManager::WaitForModelsAndTextures() {
  rstl::vector< SObjectTag > tags = gpSimplePool->GetReferencedTags();
  CTexture::sCurrentFrameCount = 0x7fffffff;
  rstl::list< CARAMToken > modelData;
  CFrameDelayedKiller::StallAndFlushAllAllocations();
  for (int pass = 0; pass < 2; ++pass) {
    for (rstl::vector< SObjectTag >::iterator it = tags.begin(); it != tags.end(); ++it) {
      if (gpSimplePool->GetObj(*it).IsLoaded()) {
        if (it->GetType() == 'TXTR') {
          TToken< CTexture > texture = gpSimplePool->GetObj(*it);
          if (pass == 0) {
            texture->MakeSwappable();
            texture->LoadToARAM();
            if (texture->IsARAMTransferInProgress()) {
              while (texture->IsARAMTransferInProgress())
                CARAMToken::UpdateAllDMAs();
            }
          } else {
            texture->LoadToMRAM();
          }
        } else if (it->GetType() == 'CMDL') {
          TToken< CModel > modelToken = gpSimplePool->GetObj(*it);
          CModel* model = *modelToken;
          if (pass == 0) {
            rstl::auto_ptr< uchar > data = model->GetData();
            const uint dataSize = OSRoundUp32B(model->GetDataSize());
            CARAMToken token(data.release(), dataSize, 1);
            token.LoadToARAM();
            token.ForceSyncARAM();
            modelData.push_back(token);
          } else {
            void* data = modelData.front().ForceSyncMRAM();
            modelData.pop_front();
            model->RemapData(static_cast< uchar* >(data));
          }
        }
      }
    }
  }
  CTexture::sCurrentFrameCount = 0;
  return true;
}
