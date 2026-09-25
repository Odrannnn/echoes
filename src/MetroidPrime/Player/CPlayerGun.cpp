#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CMath.hpp"
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
#include "MetroidPrime/Weapons/CGunWeapon.hpp"
#include "MetroidPrime/Weapons/CWeaponMgr.hpp"
#include "dolphin/types.h"

#include <string.h>

extern "C" bool fn_800C08D4(const CMorphBall*);
extern "C" void fn_801CA0F8(CPlayerGun*);
extern "C" void fn_800E5C78(CPlayerGunUnk570*);
extern "C" void fn_800E5D80(CPlayerGunUnk570*, CStateManager&, bool);
extern "C" void fn_801C5990(CGrappleArm*, CStateManager&);
extern "C" void fn_801D9F90(CGunWeapon*, CStateManager&);
extern "C" void fn_801D9F5C(CGunWeapon*, CStateManager&);

static const float kFactorMultiplierForBeamCombo =
    1.0f / CPlayerState::GetMissileComboChargeFactor();
static const float kChargeDtFactor = 1.0f / CPlayerState::GetMissileComboChargeFactor();
static SGunTriggerFunc sTriggerFuncs[] = {
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

static SGunStateFunc sStateFuncs[] = {
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

static const ushort kUnkSfxTable[2][2] = {
    {0xca, 0xcb},
    {0x25cd, 0x25ce},
};

static const ushort kSomeSfxForCharge[] = {
    0x524,
    0x25A4,
};

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
  if (outOfAmmo && m_chargePhase != kCP_AnimAndSfx) {
    if (m_chargePhase != kCP_NotCharging ||
        !playerState->ItemEnabled(CPlayerState::kIT_ChargeBeam)) {
      GetPlayer(mgr)->PlaySfxForPlayer(kSomeSfxForCharge[m_0x77c], m_0x3ac, mgr.GetNextAreaId().Value(),
                                       m_isUnderwater, 0);
    }
    ResetCharge(mgr, false);
    fn_801cdca0(mgr, player, false);
    PlayAnim(mgr, 0, 0);
    //
  } else if (outOfAmmo ||
             GetBeamAmmoTypeAndCosts(false, mgr, beamAmmoTypeA, beamAmmoTypeB, outBeamAmmoCost)) {
    CPlayerState::EChargeStage chargeState = outOfAmmo ? CPlayerState::kCS_Normal : m_chargeState;
    float chargeFactor1 = playerState->GetChargeBeamFactor();
    if (!outOfAmmo && m_absorbedPhazonShots == gpTweakPlayerGun->GetMaxAbsorbedPhazonShots()) {
      outBeamAmmoCost = 0;
    }
    if (beamAmmoTypeA != CPlayerState::kIT_Invalid) {
      playerState->DecrementAmmoAndDisplayAlertIfOut(mgr, beamAmmoTypeA, outBeamAmmoCost);
      if (beamAmmoTypeB != CPlayerState::kIT_Invalid) {
        playerState->DecrementAmmoAndDisplayAlertIfOut(mgr, beamAmmoTypeB, outBeamAmmoCost);
      }
    }

    // bool chargeEffectVisible = false;
    // if (showChargeFx && x32c_chargePhase == kCP_NotCharging) {
    //   chargeEffectVisible = true;
    // }
    // x832_25_chargeEffectVisible = chargeEffectVisible;
    // x30c_rapidFireShots += 1;

    const bool targetHoming = m_currentBeam->GetVelocityInfo().GetTargetHoming(int(chargeState));

    CTransform4f xf = m_gunWorldXf;
    // CTransform4f xf(x833_29_pointBlankWorldSurface ? x448_elbowWorldXf
    //                                                : GetGunMotionTransform() * x418_beamLocalXf);
    // if (!x833_29_pointBlankWorldSurface && x364_gunStrikeCoolTimer <= 0.f) {
    //   const CVector3f fwd = xf.GetForward();
    //   xf = x478_assistAimXf;
    //   xf.SetTranslation(fwd);
    // }

    // xf.AddTranslation(mgr.GetCameraManager()->GetGlobalCameraTranslation(mgr));
    // x38c_muzzleEffectVisTimer = 0.0625f;

    // TUniqueId homingTarget = targetHoming ? GetTargetId(mgr) : kInvalidUniqueId;
    // x72c_currentBeam->Fire(x834_27_underwater, dt, CPlayerState::EChargeStage(x330_chargeState),
    //                        xf, mgr, homingTarget, x340_chargeBeamFactor, x340_chargeBeamFactor);

    // mgr.InformListeners(GetGunMotionTransform().GetTranslation(), kLNT_PlayerFire);

    if (m_absorbedPhazonShots == gpTweakPlayerGun->GetMaxAbsorbedPhazonShots()) {
      // fun!
      CToken phazonBallToken = gpSimplePool->GetObj("PhazonBall");

      TUniqueId homingTarget = targetHoming ? GetTargetId(mgr) : kInvalidUniqueId;
      m_currentBeam->Fire(phazonBallToken, m_isUnderwater, dt, chargeState, xf, mgr, homingTarget,
                          0, 0x1c4, nullptr, nullptr, chargeFactor1, chargeFactor1);

    } else {
      // more fun!
    }

    mgr.fn_8003C4B8(m_gunWorldXf.GetTranslation(), 0); // something with object lists

    bool resetCharge = false;
    m_cooldown = m_currentBeam->GetWeaponInfo().m_coolDown;
    if (m_chargePhase == kCP_Phase_3 || m_chargePhase == kCP_AnimAndSfx) {
      resetCharge = true;
    }
    if (!resetCharge && mgr.fn_80036F10()) {
      GetPlayerFromAll(mgr)->fn_8000BC44(mgr);
    }
    if (resetCharge) {
      ResetCharge(mgr, false);
    }

    if (playerState->GetItemAmount(CPlayerState::kIT_DoubleDamage, true)) {
      GetPlayer(mgr)->PlaySfxForPlayer(0x2612, m_0x3ac, mgr.GetNextAreaId().Value(), m_isUnderwater, 0);
    }
    // TODO
  } else {
    // can't shoot
    GetPlayer(mgr)->PlaySfxForPlayer(kSomeSfxForCharge[m_0x77c], m_0x3ac, mgr.GetNextAreaId().Value(),
                                     m_isUnderwater, 0);
  }
}
void CPlayerGun::UpdateChargeState(float dt, CStateManager& mgr) {

  CPlayerState* playerState = GetPlayerFromAll(mgr)->GetPlayerState();

  switch (m_chargePhase) {
  case kCP_AnimAndSfx:
    m_maybeChargeAnim += dt;
    if (m_maybeChargeAnim >= 5.0f) {
      m_maybeChargeAnim = 0.0f;
      CRumbleManager* rumbleMgr = mgr.RumbleManager(mgr.MaskUIdNumPlayers(GetPlayerUniqueId()));
      if (m_chargeRumbleHandle == -1) {
        rumbleMgr->StopRumble(m_chargeRumbleHandle);
        m_chargeRumbleHandle = -1;
      }
      m_chargeRumbleHandle = rumbleMgr->Rumble(mgr, kRFX_PlayerGunCharge, 1.f, kRP_Three);
    }
    break;
  default:
    m_maybeChargeAnim = 0.0f;
  }

  if (m_chargePhase != kCP_NotCharging) {
    switch (m_chargePhase) {
    case kCP_Phase_1:
      if (playerState->GetChargeBeamFactor() > playerState->GetChargeAnimStart()) {
        m_chargePhase = kCP_Phase_2;
      }
      break;
    default:
      break;
    }
    if (m_chargeSfx && m_seekerChargeState != kSCS_FullyCharged) {
      CSfxManager::PitchBend(m_chargeSfx, m_isUnderwater ? 0 : 0x2000);
    }
    if (0 < m_chargePhase && m_chargePhase < 4) {
      playerState->IncrementChargeBeamFactor(kChargeDtFactor * dt);
    }
  } else {
    if (playerState->GetChargeBeamFactor() > 0.0f) {
      playerState->IncrementChargeBeamFactor(-dt);
    }
  }
}

void CPlayerGun::Charging(CStateManager& mgr, EStateMsg param, float arg) {
  switch (param) {
  case 0:
    PlayAnim(mgr, 1, 0);
    StopChargeSound(mgr, true);
    break;

  case 1: {
    CPlayerState* playerState = GetPlayer(mgr)->GetPlayerState();
    float factor = IsOutOfAmmoToShoot(mgr) ? 0.5f : 1.f;
    switch (m_chargePhase) {
    case kCP_Phase_2:
      if (playerState->GetChargeBeamFactor() >= kFactorMultiplierForBeamCombo * factor) {
        m_0x810_b5 = true;
        m_chargePhase = kCP_Phase_3;
        m_chargeState = CPlayerState::kCS_Charged;
        EnableChargeFx(mgr, true);
        PlayAnim(mgr, 2, 1);
      }
      break;
    case kCP_Phase_3:
      if (playerState->GetChargeBeamFactor() >= factor) {
        m_chargePhase = kCP_AnimAndSfx;
        break;
      }
    }
    break;
  }

  case 2:
    if (m_0x810_b3) {
      ResetCharge(mgr, false);
      if (m_0x770 == 0) {
        PlayAnim(mgr, 0, 0);
      } else {
        m_0x810_b2 = true;
      }
    }
    break;
  }
}

CVector3f CPlayerGun::GetCurrentBeamUnkVector() const {
  if (m_currentBeam) {
    return m_currentBeam->GetUnkVector();
  }
  return CVector3f::Zero();
}

int CPlayerGun::GetNumBombsAvailable(CStateManager& mgr) const {
  TUniqueId playerId = GetPlayerUniqueId();
  return 3 - mgr.GetWeaponManager()->GetNumActive(playerId, kWT_Bomb);
}

void CPlayerGun::EventHandler(CStateManager& mgr, EStateMsg msg, float arg) {
  switch (msg) {
  case 0:
    m_0x810_b3 = false;
    if (m_chargePhase != kCP_NotCharging) {
      ResetCharge(mgr, false);
    }
    break;
  case 1:
    break;
  case 2:
    break;
  }
}

void CPlayerGun::Holstered(CStateManager& mgr, EStateMsg msg, float arg) {}

void CPlayerGun::InMorphball(CStateManager& mgr, EStateMsg msg, float arg) {
  switch (msg) {
  case 0:
    m_0x624.fn_80320A04();
    break;
  case 1: {
    CPlayer* player = GetPlayer(mgr);
    if (player->GetMorphballTransitionState() == CPlayer::kMS_Morphed &&
        !fn_800C08D4(player->GetMorphBall())) {
      fn_801CA734(mgr);
    }
    break;
  }
  case 2:
    m_0x624.fn_80320978();
    break;
  }
}

void CPlayerGun::Start(CStateManager& mgr, EStateMsg msg, float arg) {}

bool CPlayerGun::IsAlive(CStateManager& mgr, const CTriggerData& data) {
  return GetPlayer(mgr)->GetPlayerState()->IsPlayerAlive();
}

bool CPlayerGun::Grappling(CStateManager& mgr, const CTriggerData& data) { return m_grappleArm->IsGrappling(); }

bool CPlayerGun::StartFidget(CStateManager& mgr, const CTriggerData& data) {
  if (mgr.fn_80036F10()) {
    return false;
  }
  return m_0x56c_b0 ? 7 : m_0x560;
}

bool CPlayerGun::InCinematic(CStateManager& mgr, const CTriggerData& data) {
  return GetPlayer(mgr)->GetCameraManager()->IsInCinematicCamera();
}

bool CPlayerGun::Scanning(CStateManager& mgr, const CTriggerData& data) {
  return GetPlayer(mgr)->GetPlayerState()->GetCurrentVisor() == CPlayerState::kPV_Scan;
}

bool CPlayerGun::GunLoaded(CStateManager& mgr, const CTriggerData& data) { return m_0x770 == 0; }

bool CPlayerGun::InterruptEvent(CStateManager& mgr, const CTriggerData& data) {
  if (!m_0x810_b3) {
    m_0x810_b3 = ShouldHolster(mgr, data);
  }
  return m_0x810_b3;
}

bool CPlayerGun::ComboOver(CStateManager& mgr, const CTriggerData& data) { return m_chargePhase == 9 && AnimOver(mgr, data); }

bool CPlayerGun::AnimOver(CStateManager& mgr, const CTriggerData& data) {
  CAnimData* animData = m_currentBeam->SolidModelData()->AnimationData();
  if (m_0x810_b13) {
    m_0x810_b13 = false;
    return true;
  }
  return !animData->IsAnimTimeRemaining(0.001f, rstl::string_l("Whole Body"));
}

bool CPlayerGun::TransitionToPlayer(CStateManager& mgr, const CTriggerData& data) {
  CPlayer::EPlayerMorphBallState state = GetPlayer(mgr)->GetMorphballTransitionState();
  return state == CPlayer::kMS_Unmorphing || state == CPlayer::kMS_Unmorphed;
}

bool CPlayerGun::TransitionToMorphball(CStateManager& mgr, const CTriggerData& data) {
  CPlayer::EPlayerMorphBallState state = GetPlayer(mgr)->GetMorphballTransitionState();
  return state == CPlayer::kMS_Morphed || state == CPlayer::kMS_Morphing;
}

bool CPlayerGun::Discharge(CStateManager& mgr, const CTriggerData& data) { return false; }

bool CPlayerGun::IsNotHolstered(CStateManager& mgr, const CTriggerData& data) { return m_gunHolsterState == 2; }

bool CPlayerGun::IsHolstered(CStateManager& mgr, const CTriggerData& data) { return m_gunHolsterState == 0; }

bool CPlayerGun::ShouldHolster(CStateManager& mgr, const CTriggerData& data) {
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

bool CPlayerGun::ButtonRelease(CStateManager& mgr, const CTriggerData& data) { return (m_0x394 >> 2) & 1; }

bool CPlayerGun::ChargeDone(CStateManager& mgr, const CTriggerData& data) {
  return m_chargePhase == 10 || m_currentBeam->IsChargeAnimOver();
}

bool CPlayerGun::CloseMissile(CStateManager& mgr, const CTriggerData& data) {
  return (m_0x398 & 0xd) || !(m_0x660[0] > 0.f);
}

void CPlayerGun::SetUnk470(float f, const CVector3f& v) {
  m_0x688 = f;
  m_0x470 = v;
}

float CPlayerGun::GetBeamVelocity() const {
  if (m_currentBeam->IsLoaded()) {
    return m_currentBeam->GetVelocityInfo().GetVelocity(m_chargeState).GetY();
  }
  return 10.f;
}

TUniqueId CPlayerGun::GetUnk578Id() {
  if (m_0x578) {
    return m_0x578->fn_801D6924();
  }
  return kInvalidUniqueId;
}

void CPlayerGun::SetUnk578Id(TUniqueId id) {
  if (m_0x578) {
    m_0x578->fn_801D6930(id);
  }
}
void CPlayerGun::fn_801C97DC() { fn_801CA0F8(this); }

void CPlayerGun::RenderBeamParticles(const CStateManager& mgr) {
  rstl::optional_object< CModelData >& modelData = m_currentBeam->SolidModelData();
  if (modelData) {
    modelData->RenderParticles(mgr.GetFrustumPlanes());
  }
}

void CPlayerGun::fn_801CE800(const CFinalInput& input, CStateManager& mgr) {
  if (m_0x770 == 0) {
    fn_801CE5C0(input, mgr);
  }
}

void CPlayerGun::fn_801D0700() { m_currentBeam->fn_801D8EC0(); }

void CPlayerGun::fn_801D0CD0(CStateManager& mgr) {
  if (mgr.GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Dark) {
    gpRender->UnkI();
  }
}

void CPlayerGun::fn_801D0D10(CStateManager& mgr) {
  if (mgr.GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Dark) {
    gpRender->UnkH(0);
  }
}

CStateMachine* CPlayerGun::GetStateMachine() {
  return m_stateMachineToken.IsLoaded() ? *m_stateMachineToken : nullptr;
}

void CPlayerGun::UpdateStateMachine(CStateManager& mgr) {
  if (!m_stateMachine.HasState() && GetStateMachine()) {
    InitStateMachine(mgr);
  }
}

void CPlayerGun::InitStateMachine(CStateManager& mgr) {
  m_stateMachine.SetStateMachine(GetStateMachine());
  m_stateMachine.SetTriggerFuncs(sTriggerFuncs, ARRAY_SIZE(sTriggerFuncs));
  m_stateMachine.SetStateFuncs(sStateFuncs, ARRAY_SIZE(sStateFuncs));
  ResetStateMachine(mgr);
  m_0x810_b0 = true;
}

void CPlayerGun::ResetStateMachine(CStateManager& mgr) {
  if (m_stateMachine.HasState() && strcmp("Start", m_stateMachine.GetCurrentStateName()) == 0) {
    return;
  }
  m_stateMachine.SetState(mgr, this, rstl::string_l("Start"));
}

CPlayerGun::CGunMorph::EMorphEvent CPlayerGun::CGunMorph::Update(float inY, float outY, float dt,
                                                                 const CPlayer& player) {
  bool inCinematic = player.GetCameraManager()->IsInCinematicCamera();
  EMorphEvent ret = kME_None;

  switch (x20_gunState) {
  case kGS_InWipeDone:
    x14_remHoldTime -= dt;
    if ((x14_remHoldTime <= 0.f || inCinematic) && x24_25_weaponChanged) {
      StartWipe(kD_Out);
      x24_25_weaponChanged = false;
      x14_remHoldTime = 0.f;
      ret = kME_InWipeDone;
    }
    // explicitly no break

  case kGS_OutWipeDone:
  case kGS_InWipe:
  case kGS_OutWipe:
  default:
    if (x24_24_morphing) {
      float omt = x8_remTime * xc_speed;
      float t = 1.f - omt;
      if (x1c_dir == kD_In) {
        x0_yLerp = inY * t + outY * omt;
        x18_transitionFactor = omt;
      } else {
        x0_yLerp = outY * t + inY * omt;
        x18_transitionFactor = t;
      }

      if (x8_remTime <= 0.f) {
        x24_24_morphing = false;
        x8_remTime = 0.f;
        if (x1c_dir == kD_In) {
          x20_gunState = kGS_InWipeDone;
          x18_transitionFactor = 0.f;
        } else {
          x18_transitionFactor = 1.f;
          x20_gunState = kGS_OutWipeDone;
          x1c_dir = kD_Done;
          ret = kME_OutWipeDone;
        }
      } else {
        x8_remTime -= dt;
        if (inCinematic) {
          x8_remTime = 0.f;
        }
      }
    }
  }

  return ret;
}

void CPlayerGun::CGunMorph::StartWipe(CPlayerGun::CGunMorph::EDir dir) {
  x14_remHoldTime = x10_holoHoldTime;
  if (dir == kD_In && x20_gunState == kGS_InWipeDone)
    return;

  if (x1c_dir != dir && x20_gunState != kGS_OutWipe) {
    x8_remTime = x4_gunTransformTime;
    xc_speed = 1.f / x4_gunTransformTime;
  } else if (x20_gunState != kGS_InWipe) {
    x8_remTime = x4_gunTransformTime - x8_remTime;
  }

  x1c_dir = dir;
  x20_gunState = x1c_dir == kD_In ? kGS_InWipe : kGS_OutWipe;
  x24_24_morphing = true;
}

CPlayerGun::CGunMorph::CGunMorph(float gunTransformTime, float holoHoldTime)
: x0_yLerp(1.f)
, x4_gunTransformTime(CMath::FastFSel(-gunTransformTime, 1.f, gunTransformTime))
, x8_remTime(0.f)
, xc_speed(0.1f)
, x10_holoHoldTime(fabs(holoHoldTime))
, x14_remHoldTime(2.f)
, x18_transitionFactor(1.f)
, x1c_dir(kD_Done)
, x20_gunState(kGS_OutWipeDone)
, x24_24_morphing(false)
, x24_25_weaponChanged(false) {}

void CPlayerGun::fn_801CA8C8(CStateManager& mgr, bool b) {
  if (b || !m_0x810_b6) {
    fn_801CA958(mgr, false);
  }
  if (!m_0x810_b7) {
    m_currentBeam->fn_801DA364(mgr, m_0x770 != 0);
  }
  m_0x810_b7 = false;
}

void CPlayerGun::fn_801CA958(CStateManager& mgr, bool) {
  m_0x570->fn_801D6D8C();
  m_grappleArm->fn_801C3824(mgr, 0.f, false);
}

void CPlayerGun::fn_801CA9A8(CStateManager& mgr, bool b) {
  fn_801CA8C8(mgr, false);
  m_0x578->fn_801D6894(mgr, b);
  m_currentBeam->EnableSecondaryFx(b ? CGunWeapon::kSFT_None : CGunWeapon::kSFT_CancelCharge);
}

void CPlayerGun::fn_801cdca0(CStateManager& mgr, CPlayer* player, bool b) {
  if (b && !player->GetCameraManager()->IsInCinematicCamera()) {
    GetPlayer(mgr)->PlaySfxForPlayer(kUnkSfxTable[m_0x77c][0], m_0x3ac, mgr.GetNextAreaId().Value(),
                                     m_isUnderwater, 0);
  }
}

void CPlayerGun::fn_801CDD28(CStateManager& mgr) {
  if (m_outgoingBeam && m_outgoingBeam != m_currentBeam) {
    m_outgoingBeam->Unload(mgr);
  }
  m_loadingBeam = m_beams[m_nextBeamId];
  m_currentBeam->EnableFx(false);
  m_currentBeam->Unk11(mgr);
  m_0x660[6] = 0.f;
  m_0x794 = mgr.fn_80036F10() ? 0 : 2;
  fn_801cdca0(mgr, GetPlayerFromAll(mgr), true);
  m_morph.StartWipe(CGunMorph::kD_In);
}

void CPlayerGun::InitBeamData() {
  CGunWeapon* beams[4] = {m_powerBeam, m_darkBeam, m_lightBeam, m_annihilatorBeam};
  for (int i = 0; i < 4; ++i) {
    m_beams[i] = beams[i];
  }
  m_currentBeam = m_beams[0];
}

void CPlayerGun::fn_801C9E9C(CStateManager& mgr) {
  m_0x570->fn_801D6ED0(3, mgr, 0.f, false);
  m_grappleArm->fn_801C37BC(4);
}

CTransform4f CPlayerGun::GetLctrTransform(const CModelData& modelData, const rstl::string& name,
                                          bool dynamic) const {
  return dynamic ? modelData.GetScaledLocatorTransformDynamic(name, nullptr)
                 : modelData.GetScaledLocatorTransform(name);
}

static void CopyScreenTex() {
  GXSetTexCopySrc(320, 224, 320, 224);
  GXSetTexCopyDst(320, 224, GX_TF_RGBA8, GX_FALSE);
  GXCopyTex(CGraphics::GetDolphinSpareBuffer(), GX_FALSE);
  GXPixModeSync();
}

void CPlayerGun::fn_801CEA58(int type, bool b) {
  m_0x780 = 0;
  if (b) {
    m_0x780 = 2;
    return;
  }
  switch (m_0x564) {
  case 0:
    m_0x780 = 1;
    if (type > 0 && type < 2) {
      m_0x780 |= 4;
    }
    break;
  case 1:
    switch (type) {
    case 4:
    case 5:
      m_0x780 = 1;
      break;
    default:
      m_0x780 = 2;
      break;
    }
    m_0x780 |= 4;
    break;
  }
}

void CPlayerGun::fn_801CEBB0() {
  if ((m_0x780 & 1) == 1) {
    m_0x570->Unk7C().fn_801DD010();
  }
  if ((m_0x780 & 2) == 2) {
    m_currentBeam->fn_801D8F64();
  }
  if ((m_0x780 & 4) == 4 && m_grappleArm->GetUnk21C()) {
    m_grappleArm->GetUnk21C()->Unk30().fn_801DD010();
  }
  m_0x780 = 0;
}

void CPlayerGun::fn_801D19CC(CStateManager& mgr) {
  fn_801DE430(mgr);
  ResetCharge(mgr, false);
  fn_801C71F8(mgr);
  PlayAnim(mgr, 0, 0);
  if (m_0x810_b0) {
    ResetStateMachine(mgr);
  }
}

void CPlayerGun::fn_801D18D0(CStateManager& mgr) {
  if (mgr.fn_80036F10()) {
    fn_800E5C78(m_0x570);
    fn_801C5990(m_grappleArm, mgr);
    for (rstl::reserved_vector< CGunWeapon*, 4 >::iterator it = m_beams.begin();
         it != m_beams.end(); ++it) {
      fn_801D9F90(*it, mgr);
    }
  } else {
    if (GetPlayer(mgr)->GetMorphballTransitionState() != CPlayer::kMS_Morphed) {
      fn_800E5D80(m_0x570, mgr, false);
      fn_801D9F90(m_currentBeam, mgr);
      fn_801C5990(m_grappleArm, mgr);
    }
    if (m_loadingBeam) {
      fn_801D9F90(m_loadingBeam, mgr);
      fn_801D9F5C(m_loadingBeam, mgr);
    }
  }
}

void CPlayerGun::fn_801C71F8(CStateManager& mgr) {
  StopChargeSound(mgr, false);
  fn_801CD55C(mgr, false);
  m_seekerChargeState = kSCS_NotCharging;
  m_timerRelatedToSeekers = 0.f;
  m_0x7ec = kInvalidUniqueId;
  m_0x7f0 = 0.f;
  m_0x7f4 = 0.f;
  m_0x7c0.clear();
}

void CPlayerGun::fn_801CB344(int beamId, CStateManager& mgr) {
  // Use the vector's inline array directly; the original beam switch keeps this base pointer
  // in a separate register from the iterator.
  CGunWeapon** beams = reinterpret_cast< CGunWeapon** >(
      reinterpret_cast< char* >(&m_beams) + sizeof(int));
  for (int i = 0; i < 4; ++i) {
    beams[i]->Unk9(mgr);
  }
  m_nextBeamId = CPlayerState::EBeamId(beamId);
  m_currentBeamId = CPlayerState::EBeamId(beamId);
  m_currentBeam = beams[beamId];
  m_currentBeam->Load(mgr, true);
  m_currentBeam->SetWorldTransManager(m_worldTransManager);
  m_0x578->fn_801D5DD0(beamId, mgr);
}

bool CPlayerGun::fn_801CEAEC() {
  int done = 0;
  if ((m_0x780 & 1) == 1 && m_0x570->Unk7C().fn_801DCFF0()) {
    done |= 1;
  }
  if ((m_0x780 & 2) == 2 && m_currentBeam->fn_801D8F2C()) {
    done |= 2;
  }
  if ((m_0x780 & 4) == 4 && m_grappleArm->GetUnk21C() &&
      m_grappleArm->GetUnk21C()->Unk30().fn_801DCFF0()) {
    done |= 4;
  }
  return done == m_0x780;
}

void CPlayerGun::fn_801CE4FC(CStateManager& mgr) {
  switch (m_0x770) {
  case 1:
    if (AnimOver(mgr, CTriggerData(0.f))) {
      fn_801CDD28(mgr);
      m_0x770 = 2;
    }
    break;
  case 2:
    if (fn_801CE0DC(mgr)) {
      m_0x770 = 3;
    }
    break;
  case 3:
    if (AnimOver(mgr, CTriggerData(0.f))) {
      m_0x770 = 0;
    }
    break;
  }
}

bool CPlayerGun::StartCharge(CStateManager& mgr, const CTriggerData& data) {
  CPlayerState* playerState = GetPlayer(mgr)->GetPlayerState();
  if (!m_0x3ae_b2 && m_gunHolsterState == 2 && m_chargePhase == kCP_Phase_2 &&
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

void CPlayerGun::MissileClosing(CStateManager& mgr, EStateMsg msg, float arg) {
  switch (msg) {
  case 0:
    PlayAnim(mgr, 5, 0);
    break;
  case 1: {
    CAnimData* animData = m_currentBeam->SolidModelData()->AnimationData();
    if (animData->GetAnimTimeRemaining(rstl::string_l("Whole Body")) < 0.8) {
      m_0x810_b13 = true;
    }
    break;
  }
  case 2:
    m_0x810_b13 = false;
    m_0x6a0 = 0;
    m_0x3ae_b3 = false;
    if (!m_0x810_b3 && GetPlayer(mgr)->GetPlayerState()->ItemEnabled(CPlayerState::kIT_ChargeBeam) &&
        (m_0x38c & 4)) {
      m_chargePhase = kCP_Phase_1;
    }
    break;
  }
}

void CPlayerGun::MissileActive(CStateManager& mgr, EStateMsg msg, float arg) {
  switch (msg) {
  case kStateMsg_Activate:
    if (m_chargePhase != kCP_NotCharging) {
      ResetCharge(mgr, false);
    }
    m_0x6a0 = 1;
    m_0x3ae_b3 = true;
    break;
  case kStateMsg_Update:
    fn_801C72B4(mgr, arg);
    break;
  case kStateMsg_Deactivate:
    m_0x810_b12 = false;
    fn_801C71F8(mgr);
    m_0x660[0] = 7.f;
    if (m_0x810_b3) {
      if (m_0x770 == 0) {
        PlayAnim(mgr, 0, 0);
      }
      m_0x3ae_b3 = false;
    }
    break;
  }
}

bool CPlayerGun::ActivateMissile(CStateManager& mgr, const CTriggerData& data) {
  if (!(m_0x384 > 0.f)) {
    CPlayerState* playerState;
    bool b = (m_0x398 >> 1) & 1;
    if (b || (m_0x38c & 2)) {
      playerState = GetPlayer(mgr)->GetPlayerState();
      if (mgr.fn_80036F10() && playerState->HasPowerUp(CPlayerState::kIT_SuperMissile)) {
        return true;
      }
      if (playerState->HasPowerUp(CPlayerState::kIT_Missile) &&
          playerState->GetItemAmount(CPlayerState::kIT_Missile, true) > 0) {
        return true;
      }
      if (b) {
        GetPlayer(mgr)->PlaySfxForPlayer(kSomeSfxForCharge[m_0x77c], m_0x3ac,
                                         mgr.GetNextAreaId().Value(), m_isUnderwater, 0);
      }
    }
  }
  return false;
}
