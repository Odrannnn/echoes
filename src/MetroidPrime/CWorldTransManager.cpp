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
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/Cameras/CCameraFilterPass.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "rstl/list.hpp"
#include "rstl/math.hpp"
#include "dolphin/os.h"

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
, mAudioStream(rstl::string_l(""))
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

void CWorldTransManager::TouchModels() {
  SModelDatas* data = mModelData.get();
  if (data != nullptr) {
    if (data->mBeamModel && data->mBeamModel->IsLoaded()) {
      data->mBeamModelData = CModelData(
          CStaticRes(data->mBeamModel->GetTag().GetId(), data->mSamusRes.GetScale()));
      data->mBeamModel = rstl::optional_object< CToken >();
    }
    if (data->mGrappleModel && data->mGrappleModel->IsLoaded()) {
      data->mGrappleModelData = CModelData(
          CStaticRes(data->mGrappleModel->GetTag().GetId(), data->mSamusRes.GetScale()));
      data->mGrappleModel = rstl::optional_object< CToken >();
    }
    if (!data->mSamusModelData.IsNull())
      data->mSamusModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mSecondPassSamusModelData.IsNull())
      data->mSecondPassSamusModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mPlatformModelData.IsNull())
      data->mPlatformModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mBgModelData.IsNull())
      data->mBgModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mBeamModelData.IsNull())
      data->mBeamModelData.Touch(CModelData::kWM_Normal, 0);
    if (!data->mGrappleModelData.IsNull())
      data->mGrappleModelData.Touch(CModelData::kWM_Normal, 0);
  }
  if (!mPortalTransition.null()) {
    mPortalTransition->TouchModels();
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
  mCharacterFactory.clear();
  DisableTransition();
}

void CWorldTransManager::Update(float dt) {
  mCurTime += dt;
  switch (mTransType) {
  case kTT_Disabled:
    UpdateDisabled(dt);
    break;
  case kTT_Enabled:
    UpdateEnabled(dt);
    break;
  case kTT_Text:
    UpdateText(dt);
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

void CWorldTransManager::UpdatePortalTransition(float dt) {
  // TODO: Update portal readiness, fade, audio and completion.
}

void CWorldTransManager::UpdateEnabled(float dt) {
  if (!mModelData.null() && !mModelData->mSamusModelData.IsNull()) {
    if (mStopSoon && !mModelData->mDissolveStarted && mCurTime >= 2.f) {
      mModelData->mDissolveStarted = true;
      mModelData->mDissolveStartTime = mCurTime;
      mModelData->mDissolveEndTime = 4.f + mCurTime - 2.f;
      mModelData->mTransCompleteTime = 5.f + mCurTime - 2.f;
    }
    if (mCurTime > mModelData->mTransCompleteTime && mModelData->mDissolveStarted)
      mTransitionFinished = true;

    static const char* const kGunLocator = "GUN_LCTR";
    mModelData->mSamusModelData.AdvanceAnimationIgnoreParticles(dt, mRandom, true);
    mModelData->mGunXf =
        mModelData->mSamusModelData.GetScaledLocatorTransform(rstl::string_l(kGunLocator));
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

  float delta = 50.f * dt;
  if (mGoingUp)
    delta = -delta;
  mBgOffset += delta;
  if (mBgOffset > mBgHeight)
    mBgOffset -= mBgHeight;
  if (mBgOffset < 0.f)
    mBgOffset += mBgHeight;
  UpdateLights(dt);
}

void CWorldTransManager::Draw() const {
  switch (mTransType) {
  case kTT_Disabled:
    DrawDisabled();
    break;
  case kTT_Enabled:
    DrawEnabled();
    break;
  case kTT_Text:
    DrawText();
    break;
  case kTT_Portal:
    DrawPortalTransition();
    break;
  }
}

void CWorldTransManager::UpdateLights(float) {
  if (mModelData.null())
    return;

  rstl::vector< CLight >& lights = mModelData->mLights;
  lights.clear();
  const CVector3f lightPos(0.f, 10.f, 0.f);
  CLight spot = CLight::BuildSpot(lightPos, CVector3f::Back(), CColor::White(), 90.f);
  spot.SetAttenuation(1.f, 0.f, 0.f);
  CLight movingSpot = spot;
  movingSpot.SetPosition(lightPos + CVector3f(0.f, 0.f, 2.f * mBgOffset - mBgHeight));
  float intensity = 1.f;
  if (!mGoingUp && mBgHeight - mBgOffset < 2.f)
    intensity = (mBgHeight - mBgOffset) / 2.f;
  else if (mGoingUp && mBgOffset < 2.f)
    intensity = mBgOffset / 2.f;

  if (intensity < 1.f) {
    CLight nextSpot = spot;
    nextSpot.SetPosition(lightPos + CVector3f(0.f, 0.f, mGoingUp ? mBgHeight : -mBgHeight));
    nextSpot.SetColor(CColor::Lerp(CColor::Black(), spot.GetColor(), 1.f - intensity));
    lights.push_back(nextSpot);
    movingSpot.SetColor(CColor::Lerp(CColor::Black(), movingSpot.GetColor(), intensity));
  }
  lights.push_back(movingSpot);
}

float CWorldTransManager::GetCameraFov(int pass) const {
  // TODO: Evaluate the selected camera spline. This is a scaffold fallback.
  return 0.f;
}

CTransform4f CWorldTransManager::GetCameraTransform(int pass) const {
  // TODO: Evaluate position/orientation splines and compose the transition camera.
  return CTransform4f::Identity();
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
  // TODO: Set the first-pass camera and draw the transition models.
}

void CWorldTransManager::DrawSecondPass() const {
  // TODO: Set the second-pass camera and draw the transition models.
}

void CWorldTransManager::DrawEnabled() const {
  // TODO: Compose both passes, blur, dissolve and transition fade.
}

void CWorldTransManager::DrawDisabled() const {
  const CColor color = CColor(uchar(0), uchar(0), uchar(0), uchar(3));
  CCameraFilterPass::DrawFilter(CCameraFilterPass::kFT_Blend, CCameraFilterPass::kFS_Fullscreen,
                                color, nullptr, 1.f);
}

void CWorldTransManager::DrawPortalTransition() const {
  // TODO: Render the portal transition and its fade overlay.
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

void CWorldTransManager::DrawText() const {
  gpRender->SetViewportOrtho(false, -4096.f, 4096.f);
  gpRender->SetModelMatrix(CTransform4f::Translate(0.f, 0.f, 448.f));
  CGraphics::SetCullMode(kCM_None);
  gpRender->SetDepthReadWrite(false, false);
  gpRender->SetBlendMode_AdditiveAlpha();
  mTextData->Render();

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
  // TODO: Read the SeenIntroText environment variable into mIntroTextSeen.
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
