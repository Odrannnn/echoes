#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "Kyoto/Animation/CAnimCharacterSet.hpp"
#include "Kyoto/Animation/CPASAnimParmData.hpp"
#include "Kyoto/Animation/CPrimitive.hpp"
#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleSwoosh.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CRumbleManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Weapons/GunController/CGunController.hpp"
#include "MetroidPrime/Weapons/WeaponCommon.hpp"

static const char* const kBeamLocators[] = {"LGBeam", "LGBeam", "LGBeamLight"};
static const char* const kGrappleGear[] = {"GrappleGear", "GrappleGear", ""};
static const rstl::pair< const char*, const char* > kSuitModels[] = {
    rstl::pair< const char*, const char* >("", ""),
    rstl::pair< const char*, const char* >("LeftArm_Dark_CMDL", "LeftArm_Dark_CSKR"),
    rstl::pair< const char*, const char* >("LeftArm_Light_CMDL", "LeftArm_Light_CSKR")};
// Guessed names. Gun PAS states reuse numeric IDs from the actor PAS domain.
enum EArmPASState { kAPS_Fidget = 10, kAPS_Grapple = 11 };
static const ushort kFireSfx[] = {0x1d9, 0x258a};
static const ushort kLoopSfx[] = {0x1da, 0x2589};
static const ushort kSwooshSfx[] = {0x1df, 0x1df};

static const TStateMachineState< CGrappleArm >::STriggerFunction kTriggerFunctions[] = {
    {"HoldGun", &CGrappleArm::HoldGun},
    {"AnimOver", &CGrappleArm::AnimOver},
    {"GunChanging", &CGrappleArm::GunChanging},
    {"FidgetActive", &CGrappleArm::FidgetActive},
    {"GrappleActive", &CGrappleArm::GrappleActive}};

static const TStateMachineState< CGrappleArm >::SStateFunction kStateFunctions[] = {
    {"Start", &CGrappleArm::Start},
    {"DownAtSide", &CGrappleArm::DownAtSide},
    {"HoldingGun", &CGrappleArm::HoldingGun},
    {"WaitAnimOver", &CGrappleArm::WaitAnimOver},
    {"WeaponChange", &CGrappleArm::WeaponChange},
    {"Fidget", &CGrappleArm::Fidget},
    {"Grappling", &CGrappleArm::Grappling}};

CGrappleArm::CGrappleArm(const CVector3f& scale, TUniqueId playerId, bool multiplayer)
: CEntity(kInvalidUniqueId, CEntity::NullEntityInfo, rstl::string_l("SamusArm"), 0)
, mCurrentSuit(CPlayerState::kPS_Varia)
, mLoadedSuit(CPlayerState::kPS_Invalid)
, mArmModel(
      CModelData(CAnimRes(NWeaponTypes::get_asset_id_from_name("grappleArm"), 5, scale, -1, false)))
, mArmCharacter(gpSimplePool->GetObj("grappleArm"))
, mBeamId(CPlayerState::kBI_Power)
, mStateMachineToken(gpSimplePool->GetObj("SamusArmFSM"))
, mTransform(CTransform4f::Identity())
, mAuxTransform(CTransform4f::Identity())
, mGrappleLocatorXf(CTransform4f::Identity())
, mScale(scale)
, mGrapplePointPosition(CVector3f::Zero())
, mGunController(nullptr)
, mGrappleSegment(gpSimplePool->GetObj("grappleSegment"))
, mGrappleClaw(gpSimplePool->GetObj("grappleClaw"))
, mGrappleHitDesc(gpSimplePool->GetObj("grappleHit"))
, mGrappleMuzzle(gpSimplePool->GetObj("grappleMuzzle"))
, mGrappleSwoosh(gpSimplePool->GetObj("grappleSwoosh"))
, mSegmentGenerator(rs_new CElementGen(mGrappleSegment))
, mClawGenerator(rs_new CElementGen(mGrappleClaw))
, mHitGenerator(rs_new CElementGen(mGrappleHitDesc))
, mMuzzleGenerator(rs_new CElementGen(mGrappleMuzzle))
, mSwooshGenerator(rs_new CParticleSwoosh(mGrappleSwoosh, 0))
, mRainSplashGenerator(rs_new CRainSplashGenerator(scale, 20, 2, 0.f, 0.125f))
, mMultiplayerSegmentGenerator(multiplayer ? rs_new CElementGen(mGrappleSegment) : nullptr)
, mMultiplayerSwooshGenerator(multiplayer ? rs_new CParticleSwoosh(mGrappleSwoosh, 0) : nullptr)
, mBeamT(0.f)
, mBeamDistance(0.f)
, mAnglePhase(0.f)
, mXAmplitude(0.f)
, mZAmplitude(0.f)
, mSwingT(0.f)
, mAnimationState(kAS_Done)
, mStateFlags(kSF_Default)
, mSoundSetIndex(0)
, mAnimSfxPitch(0x2000)
, mAnimSfx(0xffff, CSfxHandle())
, mRumbleHandle(-1)
, mSoundPan(0x36)
, mPlayerId(playerId)
, mGrappleLocator(CSegId::Invalid())
, mStateMachineInitialized(false)
, mBeamActive(false)
, mGrappleHit(false)
, mDependenciesLoading(false) {
  mStateMachineToken.Lock();
  mMuzzleGenerator->SetParticleEmission(false);
  mSegmentGenerator->SetParticleEmission(false);
  if (multiplayer) {
    mMultiplayerSegmentGenerator->SetParticleEmission(false);
  }
  for (int i = 0; i < mSwooshGenerator->GetSwooshCount() - 1; ++i) {
    mSwooshGenerator->SetWarmUp();
    mSwooshGenerator->Update(0.0);
    if (multiplayer) {
      mMultiplayerSwooshGenerator->SetWarmUp();
      mMultiplayerSwooshGenerator->Update(0.0);
    }
  }

  BuildBeamDependencyList(multiplayer);
  mAnimations = mBeamDependencies[mBeamId];
  NWeaponTypes::lock_tokens(mAnimations);
  CAnimData& animData = *mArmModel->AnimationData();
  animData.SetPoseBuilt(false);
  animData.BuildPose();
  mGrappleLocator = animData.GetLocatorSegId(rstl::string_l("grapLocator_SDK"));
  for (int i = 0; i < 3; ++i) {
    mBeamLocators.push_back(animData.GetLocatorSegId(rstl::string_l(kBeamLocators[i])));
  }
  mGunController = rs_new CGunController(*mArmModel);
  if (!multiplayer) {
    mArmModel->LockTextures();
    if (!mGrappleGearModel.IsNull()) {
      mGrappleGearModel.LockTextures();
    }
  }
  mArmModel->SetRenderFullEchoModel(true);
}

void CGrappleArm::BuildBeamDependencyList(bool multiplayer) {
  const CAnimData& animData = *mArmModel->GetAnimationData();
  const CPASAnimState& state = *animData.GetPASDatabase().GetAnimState(kAPS_Fidget);
  rstl::set< CPrimitive > primitives;
  for (int i = 0; i < state.GetNumAnims(); ++i) {
    const CAnimPlaybackParms parms(state.GetAnimInfoByIndex(i)->GetAnimId(), -1, 1.f, true);
    animData.GetAnimationPrimitives(parms, primitives);
  }
  rstl::set< SObjectTag > animTags;
  for (rstl::set< CPrimitive >::const_iterator it = primitives.begin(); it != primitives.end();
       ++it) {
    animTags.insert(SObjectTag('ANIM', it->GetAnimResId()));
  }

  for (int i = 0; i < 4; ++i) {
    CModelData model(CAnimRes(NWeaponTypes::get_asset_id_from_name("grappleArm"),
                              multiplayer ? 5 : i + 1, CVector3f::One(), 0, true));
    rstl::vector< SObjectTag > tags;
    model.GetAnimationData()->CollectAnimationResources(tags);
    rstl::vector< CToken > tokens;
    tokens.reserve(tags.size());
    for (int j = 0; j < tags.size(); ++j) {
      if (animTags.find(tags[j]) != animTags.end()) {
        tokens.push_back(gpSimplePool->GetObj(tags[j]));
      }
    }
    mBeamDependencies.push_back(tokens);
  }
}

void CGrappleArm::TouchModel(const CStateManager& mgr) const {
  if (static_cast< int >(mStateFlags) != 0) {
    mArmModel->Touch();
    if (!mGrappleGearModel.IsNull()) {
      mGrappleGearModel.Touch(mgr, 0);
    }
  }
}

void CGrappleArm::PreRender(CStateManager& mgr, const CVector3f& cameraPos) {
  if (static_cast< int >(mStateFlags) != 0) {
    mArmModel->AnimationData()->PreRender();
  }
}

void CGrappleArm::Render(const CStateManager& mgr, const CVector3f& pos, const CModelFlags& flags,
                         const CActorLights* lights) const {
  if (static_cast< int >(mStateFlags) == 0) {
    return;
  }
  const CTransform4f xf = CTransform4f::Translate(pos) * mTransform * mAuxTransform;
  const int playerIndex = mgr.MaskUIdNumPlayers(mPlayerId);
  if (mRainSplashGenerator.get() && mRainSplashGenerator->IsRaining()) {
    CSkinnedModel::SetPointGeneratorFunc(mRainSplashGenerator.get(), PointGenerator);
  }
  mArmModel->Render(mgr, xf, lights, flags.UseShaderSet(playerIndex));
  if (mRainSplashGenerator.get() && mRainSplashGenerator->IsRaining()) {
    CSkinnedModel::ClearPointGeneratorFunc();
    mRainSplashGenerator->Draw(xf);
  }
  if (!mGrappleGearModel.IsNull()) {
    mGrappleGearModel.Render(mgr, xf * mGrappleLocatorXf, lights, flags);
  }
}

void CGrappleArm::RenderGrappleBeam(const CStateManager& mgr, const CVector3f& pos,
                                    bool firstPerson) const {
  if (static_cast< int >(mStateFlags) == 0) {
    return;
  }
  if (!mBeamActive) {
    return;
  }
  if (mGrappleHit) {
    mHitGenerator->Render();
  }
  mClawGenerator->Render();
  if (firstPerson) {
    mSwooshGenerator->Render();
    mSegmentGenerator->Render();
    mMuzzleGenerator->Render();
  } else {
    mMultiplayerSwooshGenerator->Render();
    mMultiplayerSegmentGenerator->Render();
  }
}

void CGrappleArm::ResetStateMachine(CStateManager& mgr) {
  if (!mStateMachine.HasCurrentState() || strcmp(mStateMachine.GetName(), "Start") != 0) {
    mStateMachine.SetState(mgr, *this, rstl::string_l("Start"));
  }
}

void CGrappleArm::TryInitializeStateMachine(CStateManager& mgr) {
  if (!mStateMachine.HasCurrentState() && GetStateMachine() != nullptr) {
    InitializeStateMachine(mgr);
  }
}

CStateMachine* CGrappleArm::GetStateMachine() {
  if (mStateMachineToken.IsLoaded()) {
    return mStateMachineToken.GetObject();
  }
  return nullptr;
}

void CGrappleArm::InitializeStateMachine(CStateManager& mgr) {
  mStateMachine.Setup(GetStateMachine());
  mStateMachine.SetTriggerFunctions(kTriggerFunctions, 5);
  mStateMachine.SetStateFunctions(kStateFunctions, 7);
  ResetStateMachine(mgr);
  mStateMachineInitialized = true;
}

void CGrappleArm::LoadBeamDependencies(CPlayerState::EBeamId beam) {
  // Keep the previous resources locked until their replacements have been locked.
  const rstl::vector< CToken > previousTokens = mAnimations;
  mAnimations = mBeamDependencies[beam];
  mBeamId = beam;
  NWeaponTypes::lock_tokens(mAnimations);
  mDependenciesLoading = true;
}

void CGrappleArm::Update(float dt, CStateManager& mgr) {
  CPlayer* player = static_cast< CPlayer* >(mgr.ObjectById(mPlayerId));
  if (!player) {
    return;
  }
  const CPlayerState& state = *player->GetPlayerState();
  if (state.GetCurrentBeam() != mBeamId) {
    LoadBeamDependencies(state.GetCurrentBeam());
  } else if (mDependenciesLoading && NWeaponTypes::are_tokens_ready(mAnimations)) {
    mDependenciesLoading = false;
  }
  if (!mgr.fn_80036F10() && mArmModel) {
    UpdateGrappleModel(mgr, state.GetCurrentSuitRaw(), false);
    if (mCurrentSuit != state.GetCurrentSuitRaw()) {
      mCurrentSuit = state.GetCurrentSuitRaw();
      CAnimData& animData = *mArmModel->AnimationData();
      const CAssetId modelId =
          mCurrentSuit == CPlayerState::kPS_Varia
              ? animData.GetCharacterInfo().GetModelId()
              : NWeaponTypes::get_asset_id_from_name(kSuitModels[mCurrentSuit].first);
      const CAssetId skinId =
          mCurrentSuit == CPlayerState::kPS_Varia
              ? animData.GetCharacterInfo().GetSkinRulesId()
              : NWeaponTypes::get_asset_id_from_name(kSuitModels[mCurrentSuit].second);
      TLockedToken< CModel > model = gpSimplePool->GetObj(SObjectTag('CMDL', modelId));
      TLockedToken< CSkinRules > skin = gpSimplePool->GetObj(SObjectTag('CSKR', skinId));
      const TToken< CSkinnedModel > skinnedModel(
          rs_new CSkinnedModel(model, skin, animData.GetModelData()->GetLayoutInfo()));
      animData.SetSkinnedModel(skinnedModel);
    }
  }
  if (mStateFlags == 0) {
    return;
  }
  if (!mStateMachineInitialized) {
    TryInitializeStateMachine(mgr);
  }
  if (!mStateMachineInitialized) {
    return;
  }

  const float speed = (mStateFlags & kSF_Grappling) &&
                              player->GetPlayerMovementState() != NPlayer::kMS_OnGround &&
                              mAnimationState != kAS_OutOfGrapple
                          ? 4.f
                          : 1.f;
  mArmModel->AdvanceAnimation(dt * speed, mgr, kInvalidAreaId, true);
  if (!mGrappleGearModel.IsNull()) {
    mGrappleLocatorXf = mArmModel->GetScaledLocatorTransformDynamic(mGrappleLocator, nullptr);
  }
  mStateMachine.Update(mgr, *this, dt);
  if (mStateFlags & kSF_Grappling) {
    UpdateSwingAction(dt, mgr);
  } else {
    UpdateArmMovement(dt, mgr);
  }
  if (mRainSplashGenerator.get()) {
    mRainSplashGenerator->Update(dt, mgr);
  }
}

void CGrappleArm::UpdateArmMovement(float dt, CStateManager& mgr) {
  DoUserAnimEvents(mgr);
  switch (mGunController->Update(dt, mgr)) {
  case 1:
    ResetAuxParams(false);
    break;
  default:
    break;
  }
}

void CGrappleArm::UpdateSwingAction(float dt, CStateManager& mgr) {
  if (mAnimationState == kAS_FireGrapple) {
    DoUserAnimEvents(mgr);
  }
  const CTransform4f beamLocator =
      mArmModel->GetScaledLocatorTransform(mBeamLocators[mCurrentSuit]);
  const bool connected = UpdateGrappleBeam(dt, beamLocator, mgr);
  if ((mSwingT > 0.175f && mSwingT < 0.3f) || (mSwingT > 0.7f && mSwingT < 0.9f)) {
    if (!CSfxManager::IsPlaying(mSwooshSfx)) {
      mSwooshSfx = GetPlayer(mgr)->PlaySfxForPlayer(kSwooshSfx[mSoundSetIndex], mSoundPan,
                                                    mgr.GetNextAreaId(), false, 0);
      if (mRumbleHandle != -1) {
        GetRumbleManager(mgr)->StopRumble(mRumbleHandle);
      }
      mRumbleHandle = GetRumbleManager(mgr)->Rumble(mgr, kRFX_PlayerGrappleSwoosh, 1.f, kRP_Three);
    }
  }
  if (!mArmModel->GetAnimationData()->IsAnimTimeRemaining(dt, rstl::string_l("Whole Body"))) {
    switch (mAnimationState) {
    case kAS_IntoGrapple:
      SetAnimState(kAS_IntoGrappleIdle);
      break;
    case kAS_FireGrapple:
      if (connected) {
        SetAnimState(kAS_ConnectGrapple);
        mGrappleHit = true;
        mHitGenerator->SetParticleEmission(true);
        GrappleBeamConnected(mgr);
        if (mRumbleHandle != -1) {
          GetRumbleManager(mgr)->StopRumble(mRumbleHandle);
        }
      }
      break;
    case kAS_ConnectGrapple:
      if (mXAmplitude == 0.f) {
        SetAnimState(kAS_Connected);
      }
      break;
    case kAS_OutOfGrapple:
      if (mRumbleHandle != -1) {
        GetRumbleManager(mgr)->StopRumble(mRumbleHandle);
      }
      SetAnimState(kAS_Done);
      break;
    default:
      break;
    }
  }
  if (mBeamActive && mGrappleHit) {
    mGrappleHit = !mHitGenerator->IsSystemDeletable();
    mHitGenerator->SetTranslation(mGrapplePointPosition);
    mHitGenerator->Update(dt);
  }
}

bool CGrappleArm::UpdateGrappleBeam(float dt, const CTransform4f& beamLocator, CStateManager& mgr) {
  bool connected = false;
  CPlayer& player = *GetPlayer(mgr);
  const CActor* target = TCastToConstPtr< CActor >(mgr.GetObjectById(player.GetOrbitTargetId()));
  mGrapplePointPosition = target ? target->GetTranslation() : mTransform.GetTranslation();
  const CVector3f gunPos = (mTransform * beamLocator).GetTranslation();
  const CVector3f beamPos = CVector3f::Lerp(gunPos, mGrapplePointPosition, mBeamT);
  switch (mAnimationState) {
  case kAS_FireGrapple:
  case kAS_Three: {
    const float distance = (mGrapplePointPosition - gunPos).Magnitude();
    mBeamT = distance > 0.f ? mBeamDistance / distance : 1.f;
    const float speed = player.GetPlayerMovementState() != NPlayer::kMS_OnGround ? 2.f : 1.f;
    mBeamDistance += speed * (dt * player.GetTweakPlayer()->GetGrappleBeamSpeed());
    if (!(mBeamT < 1.f)) {
      mBeamT = 1.f;
      connected = true;
    }
    break;
  }
  case kAS_ConnectGrapple:
    mXAmplitude -= 4.f * dt;
    mZAmplitude -= 4.f * dt;
    if (mXAmplitude < 0.f) {
      mXAmplitude = 0.f;
    }
    if (mZAmplitude < 0.f) {
      mZAmplitude = 0.f;
    }
    break;
  default:
    break;
  }
  if (mBeamActive) {
    mAnglePhase += player.GetTweakPlayer()->GetGrappleBeamAnglePhaseDelta();
    UpdateGrappleBeamFX(mgr, gunPos, beamPos, mTransform.GetRotation(), true);
    if (mgr.fn_80036F10()) {
      CPlayer& mpPlayer = *GetPlayer(mgr);
      const CVector3f wristPos =
          (mpPlayer.GetTransform() * mpPlayer.GetLocatorTransform(rstl::string_l("L_wrist")))
              .GetTranslation();
      UpdateGrappleBeamFX(mgr, wristPos, beamPos, mpPlayer.GetTransform().GetRotation(), false);
      mMultiplayerSegmentGenerator->Update(dt);
    }
    mClawGenerator->SetTranslation(beamPos);
    mMuzzleGenerator->SetGlobalTranslation(gunPos);
    mMuzzleGenerator->Update(dt);
    mClawGenerator->Update(dt);
    mSegmentGenerator->Update(dt);
  }
  return connected;
}

void CGrappleArm::UpdateGrappleBeamFX(CStateManager& mgr, const CVector3f& gunPos,
                                      const CVector3f& beamPos, const CTransform4f& rotation,
                                      bool firstPerson) {
  CElementGen& generator = firstPerson ? *mSegmentGenerator : *mMultiplayerSegmentGenerator;
  CParticleSwoosh& swoosh = firstPerson ? *mSwooshGenerator : *mMultiplayerSwooshGenerator;
  generator.SetParticleEmission(true);
  const CVector3f delta = beamPos - gunPos;
  const int segmentCount = static_cast< int >(2.f * delta.Magnitude() + 1.f);
  const CVector3f segmentDelta = delta / float(segmentCount);
  CVector3f segmentPos = gunPos;
  for (int i = 0; i < segmentCount; ++i) {
    const CVector3f wave(mXAmplitude * CMath::FastCosR(float(i) + mAnglePhase), 0.f,
                         mZAmplitude * CMath::FastSinR(float(i)));
    generator.SetTranslation(segmentPos + (i > 0 ? rotation * wave : CVector3f::Zero()));
    generator.ForceParticleCreation(1);
    segmentPos += segmentDelta;
  }
  generator.SetParticleEmission(false);

  const CVector3f swooshDelta = delta * 0.02f;
  CVector3f swooshPos = gunPos;
  float previousRotation = swoosh.GetSwooshes()[swoosh.GetSwooshCount() - 1].mInitialRot;
  for (int i = 0; i < swoosh.GetSwooshCount(); ++i) {
    const CVector3f wave(mXAmplitude * CMath::FastCosR(float(i) + mAnglePhase), 0.f,
                         mZAmplitude * CMath::FastSinR(float(i)));
    CParticleSwoosh::SSwooshData& segment = swoosh.Swooshes()[i];
    segment.mTranslation = swooshPos + (i > 0 ? rotation * wave : CVector3f::Zero());
    swooshPos += swooshDelta;
    const float initialRotation = segment.mInitialRot;
    segment.mInitialRot = previousRotation;
    previousRotation = initialRotation;
  }
}

void CGrappleArm::ResetAuxParams(bool resetGunController) {
  mAuxTransform = CTransform4f::Identity();
  if (resetGunController) {
    mGunController->Reset();
  }
}

void CGrappleArm::Activate(bool active) {
  SetAnimState(active ? kAS_IntoGrapple : kAS_OutOfGrapple);
}

void CGrappleArm::PlayGrappleAnimation(CAnimData& animData, int anim) {
  const CPASAnimParmData parms(static_cast< pas::EAnimationState >(kAPS_Grapple),
                               CPASAnimParm::FromEnum(anim));
  const int animId = animData.GetPASDatabase().FindBestAnimation(parms, -1).second;
  animData.SetAnimation(CAnimPlaybackParms(animId, -1, 1.f, true), false);
}

void CGrappleArm::SetAnimState(EArmState state) {
  if (!mArmModel) {
    mAnimationState = kAS_Done;
    mStateFlags &= ~kSF_Grappling;
    return;
  }
  if (mAnimationState == state) {
    return;
  }
  CAnimData& animData = *mArmModel->AnimationData();
  animData.EnableLooping(false);
  SetStateFlags(kSF_Grappling);
  switch (state) {
  case kAS_IntoGrapple:
    ResetAuxParams(true);
    PlayGrappleAnimation(animData, 0);
    mBeamActive = false;
    break;
  case kAS_IntoGrappleIdle:
    animData.EnableLooping(true);
    PlayGrappleAnimation(animData, 1);
    break;
  case kAS_FireGrapple:
    PlayGrappleAnimation(animData, 2);
    break;
  case kAS_ConnectGrapple:
    PlayGrappleAnimation(animData, 3);
    break;
  case kAS_Connected:
    PlayGrappleAnimation(animData, 3);
    break;
  case kAS_OutOfGrapple:
    PlayGrappleAnimation(animData, 4);
    DisconnectGrappleBeam();
    break;
  case kAS_Done:
    mStateFlags &= ~kSF_Grappling;
    break;
  default:
    break;
  }
  mAnimationState = state;
}

void CGrappleArm::GrappleBeamConnected(CStateManager& mgr) {
  if (!mGrappleLoopSfx) {
    mGrappleLoopSfx = GetPlayer(mgr)->PlaySfxForPlayer(kLoopSfx[mSoundSetIndex], mSoundPan,
                                                       mgr.GetNextAreaId(), false, 1);
  }
}

void CGrappleArm::GrappleBeamDisconnected() {
  if (mGrappleLoopSfx) {
    CSfxManager::SfxStop(mGrappleLoopSfx);
    mGrappleLoopSfx.Clear();
  }
}

void CGrappleArm::DisconnectGrappleBeam() {
  mClawGenerator->SetParticleEmission(false);
  mMuzzleGenerator->SetParticleEmission(false);
  mBeamActive = false;
  mSwingT = 0.f;
  GrappleBeamDisconnected();
}

void CGrappleArm::DoUserAnimEvents(CStateManager& mgr) {
  const int playerIndex = mgr.MaskUIdNumPlayers(mPlayerId);
  const int areaId = mgr.GetPlayer(playerIndex)->GetCurrentAreaId().Value();
  const CGameCamera& camera = *mgr.GetCameraManager(playerIndex)->GetCurrentCamera(mgr, true);
  const CVector3f origin = mTransform.GetTranslation();
  const CVector3f posToCam = camera.GetTranslation() - origin;
  int soundCount = 0;
  const CSoundPOINode* sounds = mArmModel->AnimationData()->GetSoundPOIList(soundCount);
  if (soundCount > 0) {
    for (int i = 0; i < soundCount; ++i) {
      const CSoundPOINode& sound = sounds[i];
      const int characterIndex = sound.GetCharacterIndex();
      if (sound.GetPoiType() == kPT_Sound &&
          (characterIndex == -1 ||
           characterIndex == mArmModel->AnimationData()->GetCharacterIndex())) {
        NWeaponTypes::do_sound_event(mAnimSfx, mAnimSfxPitch, false, sound.GetSoundId(),
                                     sound.GetWeight(), sound.GetFlags(), sound.GetFallOff(),
                                     sound.GetMaxDistance(), 0x14, CAudioSys::kMaxVolume, posToCam,
                                     origin, areaId, mSoundPan, mgr);
      }
    }
  }

  int intCount = 0;
  const CInt32POINode* nodes = mArmModel->AnimationData()->GetInt32POIList(intCount);
  if (intCount > 0) {
    for (int i = 0; i < intCount; ++i) {
      const CInt32POINode& node = nodes[i];
      switch (node.GetPoiType()) {
      case kPT_UserEvent:
        DoUserAnimEvent(mgr, node, static_cast< EUserEventType >(node.GetValue()));
        break;
      case kPT_SoundInt32:
        if (node.GetCharacterIndex() == -1 ||
            node.GetCharacterIndex() == mArmModel->AnimationData()->GetCharacterIndex()) {
          NWeaponTypes::do_sound_event(mAnimSfx, mAnimSfxPitch, false, node.GetValue(),
                                       node.GetWeight(), node.GetFlags(), 0.1f, 150.f, 0x14,
                                       CAudioSys::kMaxVolume, posToCam, origin, areaId,
                                       mSoundPan, mgr);
        }
        break;
      default:
        break;
      }
    }
  }
}

void CGrappleArm::DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node,
                                  EUserEventType type) {
  switch (type) {
  case kUE_Projectile: {
    if (!(mStateFlags & kSF_Grappling)) {
      return;
    }
    mBeamActive = true;
    mHitGenerator = rs_new CElementGen(mGrappleHitDesc);
    mMuzzleGenerator = rs_new CElementGen(mGrappleMuzzle);
    mBeamT = 0.f;
    mBeamDistance = 0.f;
    mAnglePhase = 0.f;
    mSwingT = 0.f;
    CTweakPlayer& tweak = *GetPlayer(mgr)->GetTweakPlayer();
    mXAmplitude = tweak.GetGrappleBeamXWaveAmplitude();
    mZAmplitude = tweak.GetGrappleBeamZWaveAmplitude();
    mHitGenerator->SetParticleEmission(false);
    mClawGenerator->SetParticleEmission(true);
    mMuzzleGenerator->SetParticleEmission(true);
    GetPlayer(mgr)->PlaySfxForPlayer(kFireSfx[mSoundSetIndex], mSoundPan,
                                     mgr.GetNextAreaId(), false, 0);
    GetRumbleManager(mgr)->Rumble(mgr, kRFX_PlayerGrappleFire, 1.f, kRP_Three);
    break;
  }
  case kUE_Delete:
  case kUE_DamageOn:
  default:
    break;
  }
}

void CGrappleArm::PointGenerator(const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                                 void* context) {
  if (context) {
    static_cast< CRainSplashGenerator* >(context)->GeneratePoints(model, workspace);
  }
}

void CGrappleArm::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_XCRT) {
    UpdateGrappleModel(mgr, mCurrentSuit, mgr.fn_80036F10());
    mSoundPan = GetPlayer(mgr)->GetSoundPan(CPlayer::kMSP_2);
    mSoundSetIndex = mgr.fn_80036F10();
  }
}

void CGrappleArm::UpdateGrappleModel(CStateManager& mgr, CPlayerState::EPlayerSuit suit,
                                     bool force) {
  const bool hasGrapple =
      GetPlayer(mgr)->GetPlayerState()->HasPowerUp(CPlayerState::kIT_GrappleBeam);
  if ((suit != mLoadedSuit && (force || hasGrapple)) ||
      (hasGrapple && mGrappleGearModel.IsNull() && suit != CPlayerState::kPS_Light)) {
    const char* name = kGrappleGear[suit];
    mGrappleGearModel =
        *name ? CModelData(CStaticRes(NWeaponTypes::get_asset_id_from_name(name), mScale))
              : CModelData();
    mLoadedSuit = suit;
  } else if (!mGrappleGearModel.IsNull() && !hasGrapple) {
    mGrappleGearModel = CModelData();
  }
}

void CGrappleArm::EnterFreeLook(CStateManager& mgr) {
  if (!mDependenciesLoading) {
    mGunController->EnterFreeLook(mgr, mBeamId, 0);
  }
}

void CGrappleArm::EnterIdle(CStateManager& mgr) { mGunController->EnterIdle(mgr); }

void CGrappleArm::EnterComboFire(CStateManager& mgr) {
  if (!mDependenciesLoading) {
    mGunController->EnterComboFire(mgr, mBeamId);
  }
}

void CGrappleArm::EnterFidget(CStateManager& mgr, int type, int gunId, int animSet) {
  SetStateFlags(kSF_Fidget);
  if (!mDependenciesLoading) {
    mGunController->EnterFidget(mgr, type, mBeamId, animSet);
  }
}

void CGrappleArm::EnterStruck(CStateManager& mgr, float angle, bool bigStrike, bool notInFreeLook) {
  if (mStateFlags & kSF_Grappling) {
    DisconnectGrappleBeam();
    mStateFlags &= ~kSF_Grappling;
  }
  mGunController->EnterStruck(mgr, angle, bigStrike, notInFreeLook);
}

void CGrappleArm::ReturnToDefault(CStateManager& mgr, float delay, bool reset) {
  if (static_cast< int >(mStateFlags) != 0) {
    SetStateFlags(kSF_Default);
    mGunController->ReturnToDefault(mgr, delay, reset);
  }
}

void CGrappleArm::SetStateFlags(uint flags) {
  uint preserved = 0;
  switch (flags) {
  case kSF_FreeLook:
    if (mStateFlags & kSF_GunChanging) {
      mStateFlags &= ~kSF_GunChanging;
    }
    break;
  case kSF_Default:
    if (mStateFlags & kSF_GunChanging) {
      preserved = kSF_GunChanging;
    }
    if (mStateFlags & kSF_Grappling) {
      preserved = kSF_Grappling;
    }
    break;
  default:
    break;
  }
  if (static_cast< int >(flags) != 0) {
    mStateFlags = flags | kSF_Default | preserved;
  } else {
    mStateFlags = 0;
  }
}

bool CGrappleArm::HoldGun(CStateManager& mgr, const float& arg) {
  return (mStateFlags & (kSF_FreeLook | kSF_ComboFire)) != 0;
}

bool CGrappleArm::AnimOver(CStateManager& mgr, const float& arg) {
  return !mArmModel->GetAnimationData()->IsAnimTimeRemaining(0.001f, rstl::string_l("Whole Body"));
}

bool CGrappleArm::GunChanging(CStateManager& mgr, const float& arg) {
  return (mStateFlags & kSF_GunChanging) != 0;
}

bool CGrappleArm::FidgetActive(CStateManager& mgr, const float& arg) {
  return (mStateFlags & kSF_Fidget) != 0;
}

bool CGrappleArm::GrappleActive(CStateManager& mgr, const float& arg) {
  return (mStateFlags & kSF_Grappling) != 0;
}

void CGrappleArm::Start(CStateManager& mgr, int msg, float dt) {}

void CGrappleArm::DownAtSide(CStateManager& mgr, int msg, float dt) {
  if (msg >= kStateMsg_Activate && msg <= kStateMsg_Update && mStateFlags == kSF_Default) {
    mStateFlags &= ~kSF_Default;
  }
}

void CGrappleArm::HoldingGun(CStateManager& mgr, int msg, float dt) {
  if (msg >= kStateMsg_Activate && msg <= kStateMsg_Activate) {
    if (mStateFlags & kSF_FreeLook) {
      EnterFreeLook(mgr);
    } else {
      EnterComboFire(mgr);
    }
  }
}

void CGrappleArm::WaitAnimOver(CStateManager& mgr, int msg, float dt) {}

void CGrappleArm::WeaponChange(CStateManager& mgr, int msg, float dt) {
  switch (msg) {
  case kStateMsg_Activate:
    EnterIdle(mgr);
  default:
    break;
  }
}

void CGrappleArm::Fidget(CStateManager& mgr, int msg, float dt) {}

void CGrappleArm::Grappling(CStateManager& mgr, int msg, float dt) {}

CPlayer* CGrappleArm::GetPlayer(CStateManager& mgr) const {
  return mgr.GetPlayer(mgr.MaskUIdNumPlayers(mPlayerId));
}

CRumbleManager* CGrappleArm::GetRumbleManager(CStateManager& mgr) const {
  return mgr.RumbleManager(mgr.MaskUIdNumPlayers(mPlayerId));
}

CGrappleArm::~CGrappleArm() {}
