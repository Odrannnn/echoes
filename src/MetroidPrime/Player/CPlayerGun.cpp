#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CRumbleManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGrappleArm.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"
#include "MetroidPrime/Weapons/CAuxWeapon.hpp"
#include "MetroidPrime/Weapons/CGunWeapon.hpp"
#include "MetroidPrime/Weapons/CWeaponMgr.hpp"
#include "dolphin/gx/GXEnum.h"
#include "dolphin/gx/GXFrameBuffer.h"
#include "dolphin/gx/GXManage.h"
#include "dolphin/types.h"

static const float kFactorMultiplierForBeamCombo =
    1.0f / CPlayerState::GetMissileComboChargeFactor();
static const float kChargeDtFactor = 1.0f / CPlayerState::GetMissileComboChargeFactor();
static const short skEmptyBeamSfx[] = {
    0x524,
    0x25A4,
};

// File scope, not function-local statics: MWCC only emits the registration tables as
// relocated .rodata (the shape retail links) when they are namespace-scope constants.
static const TStateMachineState< CPlayerGun >::STriggerFunction kTriggerFunctions[] = {
    {"ShouldHolster", &CPlayerGun::ShouldHolster},
    {"IsHolstered", &CPlayerGun::IsHolstered},
    {"IsNotHolstered", &CPlayerGun::IsNotHolstered},
    {"StartCharge", &CPlayerGun::StartCharge},
    {"InitiateCombo", &CPlayerGun::InitiateCombo},
    {"Discharge", &CPlayerGun::Discharge},
    {"TransitionToMorphball", &CPlayerGun::TransitionToMorphball},
    {"TransitionToPlayer", &CPlayerGun::TransitionToPlayer},
    {"AnimOver", &CPlayerGun::AnimOver},
    {"ActivateMissile", &CPlayerGun::ActivateMissile},
    {"CloseMissile", &CPlayerGun::CloseMissile},
    {"ChargeDone", &CPlayerGun::ChargeDone},
    {"ButtonRelease", &CPlayerGun::ButtonRelease},
    {"ComboOver", &CPlayerGun::ComboOver},
    {"InterruptEvent", &CPlayerGun::InterruptEvent},
    {"GunLoaded", &CPlayerGun::GunLoaded},
    {"Scanning", &CPlayerGun::Scanning},
    {"InCinematic", &CPlayerGun::InCinematic},
    {"StartFidget", &CPlayerGun::StartFidget},
    {"FidgetOver", &CPlayerGun::FidgetOver},
    {"Grappling", &CPlayerGun::Grappling},
    {"IsAlive", &CPlayerGun::IsAlive},
    {"InPhazon", &CPlayerGun::InPhazon},
};
static const TStateMachineState< CPlayerGun >::SStateFunction kStateFunctions[] = {
    {"Start", &CPlayerGun::Start},
    {"Main", &CPlayerGun::Main},
    {"InMorphball", &CPlayerGun::InMorphball},
    {"Charging", &CPlayerGun::Charging},
    {"Recoil", &CPlayerGun::Recoil},
    {"ComboActive", &CPlayerGun::ComboActive},
    {"Holstered", &CPlayerGun::Holstered},
    {"Fidgeting", &CPlayerGun::Fidgeting},
    {"MissileActive", &CPlayerGun::MissileActive},
    {"MissileClosing", &CPlayerGun::MissileClosing},
    {"EventHandler", &CPlayerGun::EventHandler},
};

static bool IsSeekerTargetInRange(const CActor& target, const CPlayer& player, float radius) {
  // TODO: Compare target aim position to player eye position. The actor virtual's signature
  // differs from the current shared declaration and must be recovered before using it here.
  return false;
}

void CPlayerGun::UpdateNormalShotCycle(float dt, CStateManager& mgr) {
  CPlayer* player = GetPlayerFromAll(mgr);
  CPlayerState* playerState = player->GetPlayerState();

  if (playerState->GetItemAmount(CPlayerState::kIT_BeamWeaponsDisabled, true) > 0) {
    return;
  }

  CPlayerState::EItemType beamAmmoTypeA = CPlayerState::kIT_Invalid;
  CPlayerState::EItemType beamAmmoTypeB = CPlayerState::kIT_Invalid;
  int outBeamAmmoCost = 0;

  bool outOfAmmo = IsOutOfAmmoToShoot(mgr);
  if (outOfAmmo && mChargePhase != kCP_Charged) {
    if (mChargePhase != kCP_NotCharging ||
        !playerState->ItemEnabled(CPlayerState::kIT_ChargeBeam)) {
      GetPlayer(mgr)->PlaySfxForPlayer(skEmptyBeamSfx[mSoundSetIndex], mSoundVolume,
                                       mgr.GetNextAreaId(), mUnderwater, 0);
    }
    ResetCharge(mgr, false);
    PlayBeamFireSfx(mgr, *player, false);
    PlayAnim(mgr, 0, 0);
  } else if (outOfAmmo ||
             GetBeamAmmoTypeAndCosts(false, mgr, beamAmmoTypeA, beamAmmoTypeB, outBeamAmmoCost)) {
    CPlayerState::EChargeStage chargeState = outOfAmmo ? CPlayerState::kCS_Normal : mChargeState;
    float chargeFactor1 = playerState->GetChargeBeamFactor();
    if (!outOfAmmo && mAbsorbedPhazonShots == gpTweakPlayerGun->GetMaxAbsorbedPhazonShots()) {
      outBeamAmmoCost = 0;
    }
    if (beamAmmoTypeA != CPlayerState::kIT_Invalid) {
      playerState->DecrementAmmoAndDisplayAlertIfOut(mgr, beamAmmoTypeA, outBeamAmmoCost);
      if (beamAmmoTypeB != CPlayerState::kIT_Invalid) {
        playerState->DecrementAmmoAndDisplayAlertIfOut(mgr, beamAmmoTypeB, outBeamAmmoCost);
      }
    }

    const bool targetHoming = mCurrentBeam->GetVelocityInfo().GetTargetHoming(int(chargeState));

    // TODO: Select the point-blank or assisted aim transform and camera translation.
    CTransform4f xf = mGunWorldXf;

    if (mAbsorbedPhazonShots == gpTweakPlayerGun->GetMaxAbsorbedPhazonShots()) {
      TCachedToken< CWeaponDescription > phazonBallToken(gpSimplePool->GetObj("PhazonBall"), true);

      TUniqueId homingTarget = targetHoming ? GetTargetId(mgr) : kInvalidUniqueId;
      mCurrentBeam->CGunWeapon::Fire(phazonBallToken, mUnderwater, dt, chargeState, xf, mgr,
                                    homingTarget, 0, 0x1c4, nullptr, nullptr, chargeFactor1,
                                    chargeFactor1);

    } else {
      // TODO: Fire the selected beam using its normal projectile token.
    }

    mgr.InformListeners(mGunWorldXf.GetTranslation(), kLNT_PlayerFire);

    // TODO: Muzzle/Phazon effects, recoil, and per-frame firing flags.
    bool resetCharge = false;
    mCooldown = mCurrentBeam->GetWeaponInfo().mCoolDown;
    if (mChargePhase == kCP_ChargeFx || mChargePhase == kCP_Charged) {
      resetCharge = true;
    }
    if (!resetCharge && mgr.IsMultiplayer()) {
      GetPlayerFromAll(mgr)->fn_8000BC44(mgr);
    }
    if (resetCharge) {
      ResetCharge(mgr, false);
    }

    if (playerState->GetItemAmount(CPlayerState::kIT_DoubleDamage, true)) {
      GetPlayer(mgr)->PlaySfxForPlayer(0x2612, mSoundVolume, mgr.GetNextAreaId(), mUnderwater, 0);
    }
  } else {
    GetPlayer(mgr)->PlaySfxForPlayer(skEmptyBeamSfx[mSoundSetIndex], mSoundVolume,
                                     mgr.GetNextAreaId(), mUnderwater, 0);
  }
}
void CPlayerGun::UpdateChargeState(float dt, CStateManager& mgr) {

  CPlayerState* playerState = GetPlayerFromAll(mgr)->GetPlayerState();

  switch (mChargePhase) {
  case kCP_Charged:
    mChargeRumbleTimer += dt;
    if (mChargeRumbleTimer >= 5.0f) {
      mChargeRumbleTimer = 0.0f;
      CRumbleManager* rumbleMgr = mgr.RumbleManager(mgr.MaskUIdNumPlayers(GetPlayerUniqueId()));
      if (mChargeRumbleHandle == -1) {
        rumbleMgr->StopRumble(mChargeRumbleHandle);
        mChargeRumbleHandle = -1;
      }
      mChargeRumbleHandle = rumbleMgr->Rumble(mgr, kRFX_PlayerGunCharge, 1.f, kRP_Three);
    }
    break;
  default:
    mChargeRumbleTimer = 0.0f;
  }

  if (mChargePhase != kCP_NotCharging) {
    switch (mChargePhase) {
    case kCP_ChargeRequested:
      if (playerState->GetChargeBeamFactor() > playerState->GetChargeAnimStart()) {
        mChargePhase = kCP_Charging;
      }
      break;
    default:
      break;
    }
    if (mChargeSfx && mSeekerChargeState != kSCS_FullyCharged) {
      CSfxManager::PitchBend(mChargeSfx, mUnderwater ? 0 : 0x2000);
    }
    if (kCP_NotCharging < mChargePhase && mChargePhase < kCP_Charged) {
      playerState->IncrementChargeBeamFactor(kChargeDtFactor * dt);
    }
  } else {
    if (playerState->GetChargeBeamFactor() > 0.0f) {
      playerState->IncrementChargeBeamFactor(-dt);
    }
  }
}

void CPlayerGun::Charging(CStateManager& mgr, int param, float dt) {
  switch (param) {
  case 0:
    PlayAnim(mgr, 1, 0);
    StopChargeSound(mgr, true);
    break;

  case 1: {
    CPlayerState* playerState = GetPlayer(mgr)->GetPlayerState();
    float factor = IsOutOfAmmoToShoot(mgr) ? 0.5f : 1.f;
    switch (mChargePhase) {
    case kCP_Charging:
      if (playerState->GetChargeBeamFactor() >= kFactorMultiplierForBeamCombo * factor) {
        mChargeEffectVisible = true;
        mChargePhase = kCP_ChargeFx;
        mChargeState = CPlayerState::kCS_Charged;
        EnableChargeFx(mgr, true);
        PlayAnim(mgr, 2, 1);
      }
      break;
    case kCP_ChargeFx:
      if (playerState->GetChargeBeamFactor() >= factor) {
        mChargePhase = kCP_Charged;
        break;
      }
    }
    break;
  }

  case 2:
    if (mInterruptEvent) {
      ResetCharge(mgr, false);
      if (mBeamChangeState == kBCS_Idle) {
        PlayAnim(mgr, 0, 0);
      } else {
        mRequestReturnToDefault = true;
      }
    }
    break;
  }
}

// Structure-first pass: bodies still marked TODO below are placeholders, not equivalent
// implementations. Bodies without a TODO are byte-matched against retail (see build/report.json).

bool CPlayerGun::ShouldHolster(CStateManager& mgr, const float& argument) {
  CPlayer* player = GetPlayer(mgr);
  CPlayerState* playerState = player->GetPlayerState();
  const CCameraManager* cameraManager = player->GetCameraManager();
  bool ret = playerState->GetCurrentVisor() == CPlayerState::kPV_Scan;
  ret |= player->GetMorphballTransitionState() == CPlayer::kMS_Morphing ||
         player->GetMorphballTransitionState() == CPlayer::kMS_Morphed;
  ret |= !playerState->IsPlayerAlive();
  ret |= cameraManager->IsInCinematicCamera();
  return ret;
}

bool CPlayerGun::IsHolstered(CStateManager& mgr, const float& argument) {
  return mGunHolsterState == kGHS_Holstered;
}

bool CPlayerGun::IsNotHolstered(CStateManager& mgr, const float& argument) {
  return mGunHolsterState == kGHS_Drawn;
}

bool CPlayerGun::StartCharge(CStateManager& mgr, const float& argument) {
  CPlayerState* playerState = GetPlayer(mgr)->GetPlayerState();
  if (!mInBigStrike && mGunHolsterState == kGHS_Drawn && mChargePhase == kCP_Charging &&
      playerState->ItemEnabled(CPlayerState::kIT_ChargeBeam)) {
    int beamAmmoCost = 0;
    CPlayerState::EItemType beamAmmoTypeA = CPlayerState::kIT_Invalid;
    CPlayerState::EItemType beamAmmoTypeB = CPlayerState::kIT_Invalid;
    if (GetBeamAmmoTypeAndCosts(false, mgr, beamAmmoTypeA, beamAmmoTypeB, beamAmmoCost)) {
      return true;
    }
    if (IsOutOfAmmoToShoot(mgr)) {
      return true;
    }
  }
  return false;
}

bool CPlayerGun::InitiateCombo(CStateManager& mgr, const float& argument) {
  // TODO: Check the selected auxiliary combo, missile/beam ammo, and Phazon override.
  return false;
}

bool CPlayerGun::Discharge(CStateManager& mgr, const float& argument) { return false; }

bool CPlayerGun::TransitionToMorphball(CStateManager& mgr, const float& argument) {
  const CPlayer::EPlayerMorphBallState state = GetPlayer(mgr)->GetMorphballTransitionState();
  return state == CPlayer::kMS_Morphed || state == CPlayer::kMS_Morphing;
}

bool CPlayerGun::TransitionToPlayer(CStateManager& mgr, const float& argument) {
  const CPlayer::EPlayerMorphBallState state = GetPlayer(mgr)->GetMorphballTransitionState();
  return state == CPlayer::kMS_Unmorphing || state == CPlayer::kMS_Unmorphed;
}

bool CPlayerGun::AnimOver(CStateManager& mgr, const float& argument) {
  CAnimData* animData = mCurrentBeam->SolidModelData()->AnimationData();
  if (mMissileCloseAnimDone) {
    mMissileCloseAnimDone = false;
    return true;
  }
  // This and MissileClosing are the unit's only users of a string literal, and they still miss by
  // one instruction each: "Whole Body" lands 0x54 bytes later in this object's .rodata than in
  // retail's, because the constructor contributes four literals retail does not have
  // ("SamusGun", "SamusGunFSM", "??(??)" and "Bomb_DGRP") on top of a 0x30-byte lead-in of
  // GetBeamAmmoTypeAndCosts' cost tables where retail has 0x10. Matching needs the constructor's
  // literals, not this function.
  return !animData->IsAnimTimeRemaining(0.001f, rstl::string_l("Whole Body"));
}

bool CPlayerGun::ActivateMissile(CStateManager& mgr, const float& argument) {
  // TODO: Check missile/seeker availability and secondary cooldown; play denial sound.
  return false;
}

bool CPlayerGun::CloseMissile(CStateManager& mgr, const float& argument) {
  return (mPressedInputFlags & 0xd) || !(mMissileExitTimer > 0.f);
}

bool CPlayerGun::ChargeDone(CStateManager& mgr, const float& argument) {
  return mChargePhase == kCP_ChargeDone || mCurrentBeam->IsChargeAnimOver();
}

bool CPlayerGun::ButtonRelease(CStateManager& mgr, const float& argument) {
  return (mReleasedInputFlags & 4) != 0;
}

bool CPlayerGun::ComboOver(CStateManager& mgr, const float& argument) {
  return mChargePhase == kCP_ComboFired && AnimOver(mgr, argument);
}

bool CPlayerGun::InterruptEvent(CStateManager& mgr, const float& argument) {
  if (!mInterruptEvent) {
    mInterruptEvent = ShouldHolster(mgr, argument);
  }
  return mInterruptEvent;
}

bool CPlayerGun::GunLoaded(CStateManager& mgr, const float& argument) {
  return mBeamChangeState == kBCS_Idle;
}

bool CPlayerGun::Scanning(CStateManager& mgr, const float& argument) {
  return GetPlayer(mgr)->GetPlayerState()->GetCurrentVisor() == CPlayerState::kPV_Scan;
}

bool CPlayerGun::InCinematic(CStateManager& mgr, const float& argument) {
  return GetPlayer(mgr)->GetCameraManager()->IsInCinematicCamera();
}

bool CPlayerGun::StartFidget(CStateManager& mgr, const float& argument) {
  if (mgr.IsMultiplayer()) {
    return false;
  }
  return mFidget.IsLoading() ? 7 : mFidget.GetState();
}

bool CPlayerGun::FidgetOver(CStateManager& mgr, const float& argument) {
  // TODO: Test input, freelook, player motion, damage, and fidget animation completion.
  return false;
}

bool CPlayerGun::Grappling(CStateManager& mgr, const float& argument) {
  return mGrappleArm->IsGrappling();
}

bool CPlayerGun::IsAlive(CStateManager& mgr, const float& argument) {
  return GetPlayer(mgr)->GetPlayerState()->IsPlayerAlive();
}

bool CPlayerGun::InPhazon(CStateManager& mgr, const float& argument) { return false; }

void CPlayerGun::Start(CStateManager& mgr, int message, float dt) {}

void CPlayerGun::Main(CStateManager& mgr, int message, float dt) {
  // TODO: Drive normal/charged firing, weapon changes, and idle animations on update.
}

void CPlayerGun::InMorphball(CStateManager& mgr, int message, float dt) {
  switch (message) {
  case 0:
    mBombDependencies.Lock();
    break;
  case 1: {
    CPlayer* player = GetPlayer(mgr);
    if (player->GetMorphballTransitionState() == CPlayer::kMS_Morphed &&
        !player->GetMorphBall()->InScrewAttackMode()) {
      FireBombs(mgr);
    }
    break;
  }
  case 2:
    mBombDependencies.Unlock();
    break;
  }
}

void CPlayerGun::Recoil(CStateManager& mgr, int message, float dt) {
  // TODO: Fire or cancel the charged shot/seeker volley and finish the recoil animation.
}

void CPlayerGun::ComboActive(CStateManager& mgr, int message, float dt) {
  // TODO: Transfer charge particles, consume combo ammunition, and handle combo fire events.
}

void CPlayerGun::Holstered(CStateManager& mgr, int message, float dt) {}

void CPlayerGun::Fidgeting(CStateManager& mgr, int message, float dt) {
  // TODO: Load, play, and unload the selected gun/grapple fidget animation.
}

void CPlayerGun::MissileActive(CStateManager& mgr, int message, float dt) {
  // TODO: Open the missile chamber, fire/reload missiles, and update seeker charging.
}

void CPlayerGun::MissileClosing(CStateManager& mgr, int message, float dt) {
  switch (message) {
  case 0:
    PlayAnim(mgr, 5, 0);
    break;
  case 1: {
    CAnimData* animData = mCurrentBeam->SolidModelData()->AnimationData();
    if (animData->GetAnimTimeRemaining(rstl::string_l("Whole Body")) < 0.8) {
      mMissileCloseAnimDone = true;
    }
    break;
  }
  case 2:
    mMissileCloseAnimDone = false;
    mMissileState = kMS_Inactive;
    mMissileMode = false;
    if (!mInterruptEvent && GetPlayer(mgr)->GetPlayerState()->ItemEnabled(CPlayerState::kIT_ChargeBeam) &&
        (mInputFlags & 4)) {
      mChargePhase = kCP_ChargeRequested;
    }
    break;
  }
}

void CPlayerGun::EventHandler(CStateManager& mgr, int message, float dt) {
  switch (message) {
  case 0:
    mInterruptEvent = false;
    if (mChargePhase != kCP_NotCharging) {
      ResetCharge(mgr, false);
    }
    break;
  case 1:
    break;
  case 2:
    break;
  }
}

void CPlayerGun::DamageRumble(const CVector3f& position, float damage) {
  mDamageAmount = damage;
  mDamageLocation = position;
}

bool CPlayerGun::IsOutOfAmmoToShoot(CStateManager& mgr) const {
  const CPlayerState* state = GetPlayer(mgr)->GetPlayerState();
  switch (mCurrentBeamId) {
  case CPlayerState::kBI_Dark:
    return state->GetItemAmount(CPlayerState::kIT_DarkAmmo, true) < 1;
  case CPlayerState::kBI_Light:
    return state->GetItemAmount(CPlayerState::kIT_LightAmmo, true) < 1;
  case CPlayerState::kBI_Annihilator:
    return state->GetItemAmount(CPlayerState::kIT_LightAmmo, true) < 1 ||
           state->GetItemAmount(CPlayerState::kIT_DarkAmmo, true) < 1;
  default:
    return false;
  }
}

bool CPlayerGun::GetBeamAmmoTypeAndCosts(bool combo, CStateManager& mgr,
                                         CPlayerState::EItemType& ammoA,
                                         CPlayerState::EItemType& ammoB, int& cost) const {
  static const int normalCosts[] = {0, 1, 1, 1};
  static const int chargedCosts[] = {0, 5, 5, 5};
  static const int comboCosts[] = {0, 30, 30, 30};
  ammoA = CPlayerState::kIT_Invalid;
  ammoB = CPlayerState::kIT_Invalid;
  cost = normalCosts[mCurrentBeamId];
  switch (mCurrentBeamId) {
  case CPlayerState::kBI_Dark:
    ammoA = CPlayerState::kIT_DarkAmmo;
    break;
  case CPlayerState::kBI_Light:
    ammoA = CPlayerState::kIT_LightAmmo;
    break;
  case CPlayerState::kBI_Annihilator:
    ammoA = CPlayerState::kIT_LightAmmo;
    ammoB = CPlayerState::kIT_DarkAmmo;
    break;
  default:
    return true;
  }
  if (combo) {
    cost = comboCosts[mCurrentBeamId];
  } else if (mChargePhase > kCP_ChargeRequested && mChargePhase <= kCP_Charged) {
    cost = chargedCosts[mCurrentBeamId];
  }
  const CPlayerState* state = GetPlayer(mgr)->GetPlayerState();
  return (ammoA == CPlayerState::kIT_Invalid || state->GetItemAmount(ammoA, true) >= cost) &&
         (ammoB == CPlayerState::kIT_Invalid || state->GetItemAmount(ammoB, true) >= cost);
}

CStateMachine* CPlayerGun::GetStateMachine() {
  return mStateMachineToken.IsLoaded() ? *mStateMachineToken : nullptr;
}

void CPlayerGun::PollStateMachine(CStateManager& mgr) {
  if (!mStateMachine.HasCurrentState() && GetStateMachine() != nullptr) {
    InitializeStateMachine(mgr);
  }
}

void CPlayerGun::ResetStateMachine(CStateManager& mgr) {
  if (mStateMachine.HasCurrentState() && strcmp("Start", mStateMachine.GetName()) == 0) {
    return;
  }
  mStateMachine.SetState(mgr, *this, rstl::string_l("Start"));
}

void CPlayerGun::InitializeStateMachine(CStateManager& mgr) {
  mStateMachine.Setup(GetStateMachine());
  mStateMachine.SetTriggerFunctions(kTriggerFunctions, ARRAY_SIZE(kTriggerFunctions));
  mStateMachine.SetStateFunctions(kStateFunctions, ARRAY_SIZE(kStateFunctions));
  ResetStateMachine(mgr);
  mStateMachineInitialized = true;
}

TUniqueId CPlayerGun::CreatePowerBomb(CStateManager& mgr) { return DropPowerBomb(mgr); }

void CPlayerGun::PlayAnim(CStateManager& mgr, int animation, bool loop) {
  mCurrentBeam->PlayAnim(NWeaponTypes::EGunAnimType(animation), loop);
  // TODO: Play the beam/multiplayer-specific open, close, and weapon-switch sound.
}

void CPlayerGun::ResetSeeker(CStateManager& mgr) {
  StopChargeSound(mgr, false);
  EnableSeekerFx(mgr, false);
  mSeekerChargeState = kSCS_NotCharging;
  mSeekerChargeFactor = 0.f;
  mCurrentSeekerTarget = kInvalidUniqueId;
  mSeekerLockTimer = 0.f;
  mAllSeekersLockedTime = 0.f;
  mSeekerTargets.clear();
}

void CPlayerGun::HandleWeaponChange(const CFinalInput& input, CStateManager& mgr) {
  if (mBeamChangeState == kBCS_Idle) {
    HandleBeamChange(input, mgr);
  }
}

CPlayerGun::CGunMorph::CGunMorph(float transformTime, float holdTime)
: mYLerp(1.f)
, mGunTransformTime(CMath::FastFSel(-transformTime, 1.f, transformTime))
, mRemTime(0.f)
, mSpeed(0.1f)
, mHoloHoldTime(fabs(holdTime))
, mRemHoldTime(2.f)
, mTransitionFactor(1.f)
, mMorphDirection(kMD_Done)
, mGunState(kGS_OutWipeDone)
, mMorphing(false)
, mWeaponChanged(false) {}

void CPlayerGun::CGunMorph::StartWipe(EMorphDir direction) {
  mRemHoldTime = mHoloHoldTime;
  if (direction == kMD_In && mGunState == kGS_InWipeDone) {
    return;
  }
  if (mMorphDirection != direction && mGunState != kGS_OutWipe) {
    mRemTime = mGunTransformTime;
    mSpeed = 1.f / mGunTransformTime;
  } else if (mGunState != kGS_InWipe) {
    mRemTime = mGunTransformTime - mRemTime;
  }
  mMorphDirection = direction;
  mGunState = mMorphDirection == kMD_In ? kGS_InWipe : kGS_OutWipe;
  mMorphing = true;
}

CPlayerGun::CGunMorph::EWipeEvent CPlayerGun::CGunMorph::Update(float inY, float outY, float dt,
                                                                const CPlayer& player) {
  const bool inCinematic = player.GetCameraManager()->IsInCinematicCamera();
  EWipeEvent ret = kWE_None;
  switch (mGunState) {
  case kGS_InWipeDone:
    mRemHoldTime -= dt;
    if ((mRemHoldTime <= 0.f || inCinematic) && mWeaponChanged) {
      StartWipe(kMD_Out);
      mWeaponChanged = false;
      mRemHoldTime = 0.f;
      ret = kWE_OutWipeStarted;
    }
    // explicitly no break

  case kGS_OutWipeDone:
  case kGS_InWipe:
  case kGS_OutWipe:
  default:
    if (mMorphing) {
      const float omt = mRemTime * mSpeed;
      const float t = 1.f - omt;
      if (mMorphDirection == kMD_In) {
        mYLerp = inY * t + outY * omt;
        mTransitionFactor = omt;
      } else {
        mYLerp = outY * t + inY * omt;
        mTransitionFactor = t;
      }
      if (mRemTime <= 0.f) {
        mMorphing = false;
        mRemTime = 0.f;
        if (mMorphDirection == kMD_In) {
          mGunState = kGS_InWipeDone;
          mTransitionFactor = 0.f;
        } else {
          mTransitionFactor = 1.f;
          mGunState = kGS_OutWipeDone;
          mMorphDirection = kMD_Done;
          ret = kWE_OutWipeFinished;
        }
      } else {
        mRemTime -= dt;
        if (inCinematic) {
          mRemTime = 0.f;
        }
      }
    }
  }
  return ret;
}

CPlayerGun::CMotionState::CMotionState(float extendDistance)
: mExtendParabolaDelayTimer(0.f)
, mFireTime(0.f)
, mCurrentExtendDistance(0.f)
, mCurrentRotation(0.f)
, mRotationT(0.f)
, mStartRotation(0.f)
, mEndRotation(0.f)
, mExtendDistance(extendDistance)
, mMotionState(kMS_Zero)
, mFireState(kFS_NotFiring)
, mExtendParabola(true) {}

void CPlayerGun::CMotionState::Update(bool firing, float dt, CTransform4f& transform,
                                      CStateManager& mgr) {
  // TODO: Recoil extension/parabola and lock-on rotation interpolation.
}

void CPlayerGun::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: Forward messages to gun/auxiliary/grapple resources and initialize registered assets.
}

void CPlayerGun::TouchModel(const CStateManager& mgr) const {
  if (mgr.IsMultiplayer()) {
    mGunMotion->GetModelData().Touch();
    mGrappleArm->TouchModel(mgr);
    // Retail walks begin() to end(), not an index: the end pointer is `data() + count` folded
    // into one compare. mwcceppc 2.7 has no range-for, so the iterator pair is written out.
    for (rstl::reserved_vector< CGunWeapon*, 4 >::const_iterator it = mSelectableBeams.begin();
         it != mSelectableBeams.end(); ++it) {
      (*it)->Touch(mgr);
    }
  } else {
    // Retail passes the const reference straight to GetPlayer(CStateManager&); the cast is
    // invisible in the object.
    if (GetPlayer(const_cast< CStateManager& >(mgr))->GetMorphballTransitionState() !=
        CPlayer::kMS_Morphed) {
      mGunMotion->GetModelData().Touch(mgr, 0);
      mCurrentBeam->Touch(mgr);
      mGrappleArm->TouchModel(mgr);
    }
    if (mLoadingBeam != nullptr) {
      mLoadingBeam->Touch(mgr);
      mLoadingBeam->TouchHolo(mgr);
    }
  }
}

void CPlayerGun::PreRender(CStateManager& mgr, const CVector3f& cameraPosition) {
  // TODO: Prepare arm/beam models, lights, rain splashes, and world shadow.
}

void CPlayerGun::AddToRenderer(const CStateManager& mgr) const {
  rstl::optional_object< CModelData >& modelData = mCurrentBeam->SolidModelData();
  if (modelData) {
    modelData->RenderParticles(mgr.GetFrustumPlanes());
  }
}

void CPlayerGun::Render(const CStateManager& mgr, const CVector3f& cameraTranslation,
                        const CModelFlags& flags) const {
  // TODO: Dispatch the selected gun renderer and draw grapple/muzzle/Phazon effects.
}

void CPlayerGun::Reset(CStateManager& mgr) {
  CPlayerGunBase::Reset(mgr);
  ResetCharge(mgr, false);
  ResetSeeker(mgr);
  PlayAnim(mgr, 0, false);
  if (mStateMachineInitialized) {
    ResetStateMachine(mgr);
  }
}

void CPlayerGun::Update(float dt, CStateManager& mgr) {
  // TODO: Coordinate loading, state machine, weapon effects, pose, aiming, lights, and timers.
}

float CPlayerGun::GetBeamVelocity() const {
  return mCurrentBeam->IsLoaded() ? mCurrentBeam->GetVelocityInfo().GetVelocity(mChargeState).GetY()
                                  : 10.f;
}

void CPlayerGun::SetAuxTargetId(TUniqueId target) {
  if (!mAuxWeapon.null()) {
    mAuxWeapon->SetTargetId(target);
  }
}

TUniqueId CPlayerGun::GetAuxTargetId() const {
  if (!mAuxWeapon.null()) {
    return mAuxWeapon->GetTargetId();
  }
  return kInvalidUniqueId;
}

void CPlayerGun::AsyncLoadSuit(CStateManager& mgr) {
  mCurrentBeam->AsyncLoadSuitArm();
}

void CPlayerGun::ProcessInput(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Update base input state, beam changes, charge requests, and weapon state machine.
}

CVector3f CPlayerGun::fn_801c6df8() const {
  if (mCurrentBeam) {
    return mCurrentBeam->GetRainSplashPosition();
  }
  return CVector3f::Zero();
}

int CPlayerGun::GetBombsAvailable(CStateManager& mgr) const {
  TUniqueId playerId = GetPlayerUniqueId();
  return 3 - mgr.GetWeaponManager()->GetNumActive(playerId, kWT_Bomb);
}

TUniqueId CPlayerGun::DropPowerBomb(CStateManager& mgr) const {
  // TODO: Create/register CPowerBomb with player damage, ownership, and bomb effect tokens.
  return kInvalidUniqueId;
}

void CPlayerGun::DropBomb(EBWeapon type, CStateManager& mgr) {
  // TODO: Create/register CBomb, apply double damage, update reload/count, and attach to platforms.
}

void CPlayerGun::FireBombs(CStateManager& mgr) {
  // TODO: Check morph state, bomb/power-bomb input, ammo, cooldown, and active power bomb.
}

void CPlayerGun::TakeDamage(bool bigStrike, bool strikeGrapple, CStateManager& mgr) {
  // TODO: Start struck gun/grapple animations, select SFX, and cancel active charge/fidget.
}

TUniqueId CPlayerGun::GetTargetId(CStateManager& mgr) {
  // TODO: Validate the player's orbit target against projectile-target material flags.
  return kInvalidUniqueId;
}

void CPlayerGun::PlayBeamFireSfx(CStateManager& mgr, CPlayer& player, bool play) {
  // TODO: Play the beam fire sound when requested and outside cinematics.
}

void CPlayerGun::ResetCharge(CStateManager& mgr, bool playAnimation) {
  if (mChargePhase != kCP_NotCharging && mChargePhase != kCP_ChargeRequested) {
    EnableChargeFx(mgr, false);
    StopChargeSound(mgr, false);
  }
  mPhazonChargeGenerator = rstl::auto_ptr< CElementGen >();
  mPhazonAbsorbFlashGenerator = rstl::auto_ptr< CElementGen >();
  GetPlayerFromAll(mgr)->GetPlayerState()->SetChargeBeamFactor(0.f);
  mRequestReturnToDefault = false;
  mChargePhase = kCP_NotCharging;
  mSeekerChargeState = kSCS_NotCharging;
  mChargeState = CPlayerState::kCS_Normal;
  mChargeRumbleTimer = 0.f;
  mAbsorbedPhazonShots = 0;
  if (mCurrentBeam != nullptr) {
    mCurrentBeam->Unk8();
  }
  if (playAnimation) {
    PlayAnim(mgr, 0, false);
  }
}

void CPlayerGun::StopChargeSound(CStateManager& mgr, bool start) {
  if (mChargeSfx) {
    CSfxManager::SfxStop(mChargeSfx);
    mChargeSfx = CSfxHandle::NullHandle();
  }
  CRumbleManager* rumble = mgr.RumbleManager(mgr.MaskUIdNumPlayers(GetPlayerUniqueId()));
  if (mChargeRumbleHandle != -1) {
    rumble->StopRumble(mChargeRumbleHandle);
    mChargeRumbleHandle = -1;
  }
  if (start) {
    // TODO: Start the SP/MP beam or seeker charge sound and charge rumble.
  }
}

void CPlayerGun::EnableChargeFx(CStateManager& mgr, bool enable) {
  mCurrentBeam->ActivateCharge(enable, false);
  SetGunLightActive(enable, mgr);
  mCurrentBeam->EnableSecondaryFx(enable ? CGunWeapon::kSFT_Charge : CGunWeapon::kSFT_CancelCharge);
  mChargeEffectVisible = enable;
  if (enable) {
    mAuxMuzzleGenerators[mCurrentBeamId] =
        rstl::auto_ptr< CElementGen >(rs_new CElementGen(mAuxMuzzleEffects[mCurrentBeamId]));
    mAuxMuzzleGenerators[mCurrentBeamId]->SetParticleEmission(true);
  } else {
    mAuxMuzzleGenerators[mCurrentBeamId] = rstl::auto_ptr< CElementGen >();
  }
  // TODO: Toggle the player's remote charge effect in multiplayer.
}

void CPlayerGun::UpdateSeeker(float dt, CStateManager& mgr) {
  // TODO: Acquire/revalidate targets, advance charge/lock timers, and launch the seeker volley.
}

void CPlayerGun::UpdateSeekerEffects(float dt) {
  // TODO: Update seeker/missile particles, opacity, scale, and muzzle positions.
}

void CPlayerGun::EnableSeekerFx(CStateManager& mgr, bool enable) {
  // TODO: Create/clear the seeker muzzle and missile secondary generators.
}

void CPlayerGun::FireSecondary(float dt, CStateManager& mgr, TUniqueId target, uint attributes,
                               const CTransform4f* transform, ushort sound) {
  // TODO: Consume missiles/combo ammunition and dispatch the auxiliary projectile.
}

void CPlayerGun::UpdateAuxWeapons(const CTransform4f& transform, CStateManager& mgr) {
  // TODO: Update combo completion and cancel the beam's secondary effect when finished.
}

void CPlayerGun::StopContinuousBeam(CStateManager& mgr, bool deactivate) {
  ReturnArmAndGunToDefault(mgr, false);
  mAuxWeapon->fn_801D6894(mgr, deactivate);
  mCurrentBeam->EnableSecondaryFx(deactivate ? CGunWeapon::kSFT_None : CGunWeapon::kSFT_CancelCharge);
}

void CPlayerGun::DoUserAnimEvent(float dt, CStateManager& mgr, const CInt32POINode& node,
                                 EUserEventType type) {
  // TODO: Handle projectile/combo animation events using their locator transforms.
}

void CPlayerGun::DoUserAnimEvents(float dt, CStateManager& mgr) {
  // TODO: Dispatch user and sound POIs with player pitch and underwater settings.
}

void CPlayerGun::SetGunLightActive(bool active, CStateManager& mgr) {
  // TODO: Activate the script light associated with this gun.
}

void CPlayerGun::UpdateGunLight(const CTransform4f& transform, CStateManager& mgr) {
  // TODO: Copy the active muzzle generator's light/color and transform to the gun light.
}

void CPlayerGun::SetBeam(CPlayerState::EBeamId beamId, CStateManager& mgr) {
  // Use the vector's inline array directly; the original beam switch keeps this base pointer in a
  // separate register from the iterator.
  CGunWeapon** beams = reinterpret_cast< CGunWeapon** >(
      reinterpret_cast< char* >(&mSelectableBeams) + sizeof(int));
  for (int i = 0; i < 4; ++i) {
    beams[i]->InitializeResources(mgr);
  }
  mNextBeamId = CPlayerState::EBeamId(beamId);
  mCurrentBeamId = CPlayerState::EBeamId(beamId);
  mCurrentBeam = beams[beamId];
  mCurrentBeam->Load(mgr, true);
  mCurrentBeam->SetRainSplashGenerator(mRainSplashGenerator.get());
  mAuxWeapon->fn_801D5DD0(beamId, mgr);
}

void CPlayerGun::InitMuzzleData(CStateManager& mgr) {
  // TODO: Choose SP/MP renderer/sounds, initialize beam/seeker/missile particle resources.
}

void CPlayerGun::InitBombData() {
  // TODO: Populate normal/power bomb effect pairs from BombSet/BombExplo/PowerBombExplo.
}

void CPlayerGun::InitBeamData() {
  CGunWeapon* beams[4] = {mPowerBeam.get(), mDarkBeam.get(), mLightBeam.get(),
                          mAnnihilatorBeam.get()};
  for (int i = 0; i < 4; ++i) {
    mSelectableBeams[i] = beams[i];
  }
  mCurrentBeam = mSelectableBeams[0];
}

void CPlayerGun::ChangeWeapon(CStateManager& mgr) {
  if (mOutgoingBeam && mOutgoingBeam != mCurrentBeam) {
    mOutgoingBeam->Unload(mgr);
  }
  mLoadingBeam = mSelectableBeams[mNextBeamId];
  mCurrentBeam->EnableFx(false);
  mCurrentBeam->ReleaseResources(mgr);
  mMuzzleEffectVisTimer = 0.f;
  mBeamLoadDelayFrames = mgr.IsMultiplayer() ? 0 : 2;
  PlayBeamFireSfx(mgr, *GetPlayerFromAll(mgr), true);
  mGunMorph.StartWipe(CGunMorph::kMD_In);
}

bool CPlayerGun::ProcessGunMorph(float dt, CStateManager& mgr) {
  // TODO: Advance the hologram wipe, update the loading beam and complete weapon changes.
  return false;
}

void CPlayerGun::UpdateBeamChange(float dt, CStateManager& mgr) {
  switch (mBeamChangeState) {
  case kBCS_Close: {
    float zero = 0.f;
    if (AnimOver(mgr, zero)) {
      ChangeWeapon(mgr);
      mBeamChangeState = kBCS_Morph;
    }
    break;
  }
  case kBCS_Morph:
    if (ProcessGunMorph(dt, mgr)) {
      mBeamChangeState = kBCS_Open;
    }
    break;
  case kBCS_Open: {
    float zero = 0.f;
    if (AnimOver(mgr, zero)) {
      mBeamChangeState = kBCS_Idle;
    }
    break;
  }
  default:
    break;
  }
}

void CPlayerGun::HandleBeamChange(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Read beam selections, validate ownership, and request the selected beam.
}

void CPlayerGun::EnterFreeLook(CStateManager& mgr) {
  mGunMotion->fn_801D6ED0(3, mgr, 0.f, false);
  mGrappleArm->SetStateFlags(CGrappleArm::kSF_FreeLook);
}

void CPlayerGun::ReturnArmAndGunToDefault(CStateManager& mgr, bool force) {
  if (force || !mInFreeLook) {
    ReturnToDefault(mgr, false);
  }
  if (!mGunMotionFidgeting) {
    mCurrentBeam->ReturnToDefault(mgr, mBeamChangeState != kBCS_Idle);
  }
  mGunMotionFidgeting = false;
}

void CPlayerGun::ReturnToDefault(CStateManager& mgr, bool bigStrikeReset) {
  mGunMotion->fn_801D6D8C();
  mGrappleArm->ReturnToDefault(mgr, 0.f, false);
}

void CPlayerGun::SetFidgetAnimBits(int type, bool holster) {
  mFidgetAnimBits = 0;
  if (holster) {
    mFidgetAnimBits = 2;
    return;
  }
  switch (mFidget.GetType()) {
  case SamusGun::kFT_Minor:
    mFidgetAnimBits = 1;
    if (type > 0 && type < 2) {
      mFidgetAnimBits |= 4;
    }
    break;
  case SamusGun::kFT_Major:
    switch (type) {
    case 4:
    case 5:
      mFidgetAnimBits = 1;
      break;
    default:
      mFidgetAnimBits = 2;
      break;
    }
    mFidgetAnimBits |= 4;
    break;
  }
}

bool CPlayerGun::IsFidgetLoaded() {
  int done = 0;
  if ((mFidgetAnimBits & 1) == 1 && mGunMotion->GunController().IsFidgetLoaded()) {
    done |= 1;
  }
  if ((mFidgetAnimBits & 2) == 2 && mCurrentBeam->IsFidgetLoaded()) {
    done |= 2;
  }
  if ((mFidgetAnimBits & 4) == 4 && mGrappleArm->GetGunController() &&
      mGrappleArm->GetGunController()->IsFidgetLoaded()) {
    done |= 4;
  }
  return done == mFidgetAnimBits;
}

void CPlayerGun::UnLoadFidget() {
  if ((mFidgetAnimBits & 1) == 1) {
    mGunMotion->GunController().UnLoadFidget();
  }
  if ((mFidgetAnimBits & 2) == 2) {
    mCurrentBeam->UnLoadFidget();
  }
  if ((mFidgetAnimBits & 4) == 4 && mGrappleArm->GetGunController()) {
    mGrappleArm->GetGunController()->UnLoadFidget();
  }
  mFidgetAnimBits = 0;
}

void CPlayerGun::AsyncLoadFidget(CStateManager& mgr) {
  const SamusGun::EFidgetType type = mFidget.GetType();
  const int animSet = mFidget.GetAnimSet();
  bool beamOnly = (mFidget.IsLoading() ? 7 : mFidget.GetState()) == CFidget::kS_HolsterBeam;
  SetFidgetAnimBits(animSet, beamOnly);

  if ((mFidgetAnimBits & 1) == 1) {
    mGunMotion->GunController().LoadFidgetAnimAsync(mgr, type, mCurrentBeamId, animSet);
  }

  if ((mFidgetAnimBits & 2) == 2) {
    mCurrentBeam->AsyncLoadFidget(mgr, beamOnly ? SamusGun::kFT_Minor : type, animSet);
  }

  if ((mFidgetAnimBits & 4) == 4) {
    if (CGunController* gc = mGrappleArm->GetGunController()) {
      gc->LoadFidgetAnimAsync(mgr, type, type != SamusGun::kFT_Minor ? mCurrentBeamId : 0, animSet);
    }
  }

  mFidget.StartLoading();
}

void CPlayerGun::EnterFidget(CStateManager& mgr) {
  const SamusGun::EFidgetType type = mFidget.GetType();
  const int animSet = mFidget.GetAnimSet();

  if ((mFidgetAnimBits & 1) == 1) {
    mGunMotion->EnterFidget(mgr, type, animSet);
    mGunMotionFidgeting = true;
  } else {
    mGunMotionFidgeting = false;
  }

  if ((mFidgetAnimBits & 2) == 2) {
    mCurrentBeam->EnterFidget(mgr, type, animSet);
  }

  if ((mFidgetAnimBits & 4) == 4) {
    mGrappleArm->EnterFidget(mgr, type, type != SamusGun::kFT_Minor ? mCurrentBeamId : 0, animSet);
  }

  UnLoadFidget();
  mFidget.DoneLoading();
}

void CPlayerGun::UpdateGunIdle(float dt, CStateManager& mgr) {
  // TODO: Advance fidget delays and choose idle/wander/holster actions.
}

void CPlayerGun::UpdateGunMotion(float dt, CStateManager& mgr) {
  // TODO: Update the gun-motion animation controller and user events.
}

void CPlayerGun::UpdateTimers(float dt) {
  if (mBombReloadTimer > 0.f) {
    mBombReloadTimer -= dt;
    if (mBombReloadTimer <= 0.f) {
      mBombCount = 3;
      mBombReloadTimer = 0.f;
    }
  }
  if (mMuzzleEffectVisTimer > 0.f) {
    mMuzzleEffectVisTimer -= dt;
  }
  if (mRapidFireDecayTimer >= 0.2f) {
    mRapidFireDecayTimer = 0.f;
    if (mRapidFireShots > 0) {
      --mRapidFireShots;
    }
  } else {
    mRapidFireDecayTimer += dt;
  }
  if ((mInputFlags & 0xd) == 0) {
    if (mTimeSinceFire < 2.f) {
      mTimeSinceFire += dt;
      if (mTimeSinceFire > 1.f) {
        mRapidFireShots = 0;
        mShotSmokeTimer = 0.f;
      }
    }
  } else {
    mTimeSinceFire = 0.f;
  }
  if (mInBigStrike) {
    if (mBigStrikeTimer <= 0.f) {
      if (mGunMotionReturningFromStrike) {
        if (!mGunMotion->GetModelData().GetAnimationData()->IsAnimTimeRemaining(
                0.001f, rstl::string("Whole Body"))) {
          mInBigStrike = false;
          mGunMotionReturningFromStrike = false;
        }
      } else {
        mBigStrikeTimer = 0.f;
        mGunMotionReturningFromStrike = true;
        mGunMotion->BasePosition(true);
      }
    } else {
      mBigStrikeTimer -= dt;
    }
  }
  if (mRapidFireShots > 5 && mShotSmokeTimer < 2.f) {
    mShotSmokeTimer += dt;
  }
  if (mGunStrikeDelayTimer > 0.f) {
    mGunStrikeDelayTimer -= dt;
  }
  if (mGunStrikeCooldownTimer > 0.f) {
    mGunStrikeCooldownTimer -= dt;
  }
}

void CPlayerGun::UpdateFreeLook(float dt, CStateManager& mgr) {
  // TODO: Enter/exit freelook according to input, movement, and strike cooldown.
}

void CPlayerGun::UpdateLeftArmTransform() {
  const CVector3f elbowOffset(-0.9f, -0.4f, 0.4f);
  CTransform4f& auxXf = mGrappleArm->AuxTransform();
  // One assignment, not two branches: the ternary yields a `const CTransform4f&`, so retail keeps a
  // single `operator=` call site after selecting the source into r4.
  auxXf = mAnimPlaying ? CTransform4f::Identity() : mElbowLocalXf;
  const CVector3f elbowPos = auxXf * elbowOffset;
  auxXf.SetTranslation(elbowPos);
  mGrappleArm->SetTransform(mTransform);
}

CTransform4f CPlayerGun::GetLocatorTransform(const CModelData& modelData, const rstl::string& name,
                                             bool dynamic) const {
  return dynamic ? modelData.GetScaledLocatorTransformDynamic(name, nullptr)
                 : modelData.GetScaledLocatorTransform(name);
}

void CPlayerGun::DrawArm(const CStateManager& mgr, const CVector3f& cameraTranslation,
                         const CModelFlags& flags) const {
  if (!mGrappleArm->IsActive()) {
    return;
  }
  const CPlayer* player =
      GetPlayer(const_cast< CStateManager& >(mgr)); // Retail passes the const ref straight through.
  // Retail copy-constructs the arm transform onto the stack before reading its forward column, so
  // the source takes it by value rather than through the reference accessor.
  // Retail keeps the arm transform as a stack copy and the player's forward column in place, and
  // multiplies them in that order; binding the player's column to a local moves it into a callee
  // register pair instead, which is a worse match.
  const CTransform4f armXf = mGrappleArm->GetTransform();
  const float dot = CVector3f::Dot(player->GetTransform().GetForward(), armXf.GetForward());
  if (player->GetGrappleState() != CPlayer::kGS_None || dot > 0.1f) {
    mGrappleArm->Render(mgr, cameraTranslation, flags, &mLights);
  }
}

void CPlayerGun::RenderGunWithHologram(const CStateManager& mgr, const CVector3f& cameraTranslation,
                                       bool drawSuitArm, const CTransform4f& elbowTransform,
                                       const CTransform4f& gunTransform,
                                       const CModelFlags& armFlags,
                                       const CModelFlags& gunFlags) const {
  // TODO: Draw the gun, transition clip planes/hologram, and suit arm.
}

void CPlayerGun::RenderGun(const CStateManager& mgr, const CVector3f& cameraTranslation,
                           bool drawSuitArm, const CTransform4f& elbowTransform,
                           const CTransform4f& gunTransform, const CModelFlags& armFlags,
                           const CModelFlags& gunFlags) const {
  // TODO: Draw the non-hologram beam and suit arm using their supplied flags/transforms.
}

CVector3f CPlayerGun::ConvertToScreenSpace(const CVector3f& position,
                                           const CGameCamera& camera) const {
  CVector3f viewPos = camera.GetTransform().TransposeRotate(
      CVector3f(position.GetX() - camera.GetTransform().Get03(),
                position.GetY() - camera.GetTransform().Get13(),
                position.GetZ() - camera.GetTransform().Get23()));
  CVector3f screenPos(viewPos);
  if (screenPos.IsNonZero()) {
    return CGraphics::GetPerspectiveProjectionMatrix().MultiplyOneOverW(screenPos);
  }
  return CVector3f(-1.f, -1.f, 1.f);
}

void CPlayerGun::BeginDarkVisorRender(const CStateManager& mgr) const {
  if (mgr.GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Dark) {
    gpRender->SetDestinationAlpha(0);
  }
}

void CPlayerGun::EndDarkVisorRender(const CStateManager& mgr) const {
  if (mgr.GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Dark) {
    gpRender->DisableDestinationAlpha();
  }
}

void CPlayerGun::DrawScreenTex() {
  // TODO: Draw the captured screen texture with the gun's transition material.
}

void CPlayerGun::CopyScreenTex() {
  GXSetTexCopySrc(320, 224, 320, 224);
  GXSetTexCopyDst(320, 224, GX_TF_RGBA8, GX_FALSE);
  GXCopyTex(CGraphics::GetDolphinSpareBuffer(), GX_FALSE);
  GXPixModeSync();
}

CPlayerGun::CPlayerGun(TUniqueId playerId, int characterIndex)
: CPlayerGunBase(rstl::string("SamusGun"), playerId, CVector3f(2.f, 2.f, 2.f), 20)
, mGunWorldXf(CTransform4f::Identity())
, mBeamLocalXf(CTransform4f::Identity())
, mElbowLocalXf(CTransform4f::Identity())
, mElbowWorldXf(CTransform4f::Identity())
, mDamageLocation(CVector3f::Zero())
, mStateMachineToken(gpSimplePool->GetObj("SamusGunFSM"))
, mGunMorph(gpTweakPlayerGun->GetGunTransformTime(), gpTweakPlayerGun->GetHoloHoldTime())
, mMotionState(gpTweakPlayerGun->GetGunExtendDistance())
, mHologramClipCube(CVector3f(-0.293292f, 0.f, -0.2481945f),
                    CVector3f(0.293292f, 1.292392f, 0.2481945f))
, mRender(&CPlayerGun::RenderGunWithHologram)
, mGrappleArm(rs_new CGrappleArm(mScale, playerId, bool(uchar(characterIndex))))
, mAuxWeapon(rs_new CAuxWeapon(playerId))
, mSelectableBeams(4, static_cast< CGunWeapon* >(nullptr))
, mBombDependencies(TToken< CDependencyGroup >(gpSimplePool->GetObj("Bomb_DGRP")), *gpSimplePool)
, mCurrentBeam(nullptr)
, mOutgoingBeam(nullptr)
, mLoadingBeam(nullptr)
, mMissileExitTimer(7.f)
, mComboTransferFactor(0.f)
, mBombReloadTimer(0.f)
, mTimeSinceFire(0.f)
, mRapidFireDecayTimer(0.f)
, mShotSmokeTimer(0.f)
, mMuzzleEffectVisTimer(0.f)
, mEnterFreeLookDelayTimer(0.f)
, mGunStrikeCooldownTimer(0.f)
, mIdleWanderDelayTimer(0.f)
, mDamageAmount(0.f)
, mBigStrikeTimer(0.f)
, mGunStrikeDelayTimer(0.f)
, mChargePhase(kCP_NotCharging)
, mSeekerChargeState(kSCS_NotCharging)
, mSeekerChargeFactor(0.f)
, mMissileState(kMS_Inactive)
, mSeekerSecondaryFx(CGunWeapon::kSFT_None)
, mMissileShotInterval(0.f)
, mBeamChangeState(kBCS_Idle)
, mCurrentBeamId(CPlayerState::kBI_Power)
, mNextBeamId(mCurrentBeamId)
, mSoundSetIndex(0)
, mFidgetAnimBits(0)
, mAnimSfxPitch(0x2000)
, mBombCount(3)
, mRapidFireShots(0)
, mGunMotionState(SamusGun::kAS_BasePosition)
, mBeamLoadDelayFrames(0)
, mAnimSfx(ushort(-1), CSfxHandle::NullHandle())
, mChargeSfx(CSfxHandle::NullHandle())
, mInvalidSfx(CSfxHandle::NullHandle())
, mChargeRumbleHandle(-1)
, mChargeRumbleTimer(0.f)
, mPowerBombId(kInvalidUniqueId)
, x7b4_(0)
, mMaxSeekerTargets(0)
, mSeekerVisor(CPlayerState::EPlayerVisor(-1))
, mCurrentSeekerTarget(kInvalidUniqueId)
, mSeekerLockTimer(0.f)
, mAllSeekersLockedTime(0.f)
, mAmbientColor(CColor::Black())
, mAbsorbedPhazonShots(0)
, mStateMachineInitialized(false)
, mComboFiring(false)
, mRequestReturnToDefault(false)
, mInterruptEvent(false)
, mFrozen(false)
, mChargeEffectVisible(false)
, mInFreeLook(false)
, mGunMotionFidgeting(false)
, mAnimPlaying(false)
, mFiring(false)
, mPointBlankWorldSurface(false)
, mGunMotionReturningFromStrike(false)
, mMissileAnimActive(false)
, mMissileCloseAnimDone(false)
, mCommonDependenciesLoaded(false)
, mBeamLoadRequested(false) {
  // TODO: Construct CGunMotion and the four beam subclasses once their layouts are recovered.
  // The current CGunMotion/CPowerBeam scaffolds cannot safely supply the target allocation sizes.
  mStateMachineToken.Lock();
  InitBeamData();
  InitBombData();
  // TODO: Fill/lock Common_DGRP tokens and prepare the single-player gun-motion materials.
}

CPlayerGun::~CPlayerGun() {
  for (rstl::vector< CToken >::iterator it = mCommonDependencies.begin();
       it != mCommonDependencies.end(); ++it) {
    it->Unlock();
  }
}
