#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"

#include "Kyoto/Math/CRelAngle.hpp"
#include "MetroidPrime/Cameras/CCameraShakerData.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.hpp"

// Every CDamageInfo the tweak hands out is built here rather than by writing
// `CDamageInfo(mData->...)` at the call site. That is a codegen requirement, not taste: retail
// keeps the hidden return pointer live across the constructor call and spills it to r31, while
// a direct `return CDamageInfo(mData->...)` lets mwcceppc drop those three instructions
// (`stw r31`/`mr r31,r3`/`lwz r31`) and leave the function 16 bytes short. Routing the loader
// record through this inlined helper is what makes mwcceppc keep the pointer live, and it is
// why all ten CDamageInfo/SWeaponInfo builders below match retail byte for byte.
static inline CDamageInfo LdrDamage(const SLdrTDamageInfo& data, bool charged = false,
                                   bool comboed = false, bool noImmunity = false,
                                   bool flag = false) {
  return CDamageInfo(data, charged, comboed, noImmunity, flag);
}

void CTweakPlayerGun::BuildCache() {
  mBeamInfo.clear();
  mBeamInfo.push_back(SWeaponInfo(mData->weapons.power_Beam.delayBetweenShots,
                                  LdrDamage(mData->weapons.power_Beam.damageInfo.normal),
                                  LdrDamage(mData->weapons.power_Beam.damageInfo.charged, true)));
  mBeamInfo.push_back(SWeaponInfo(mData->weapons.dark_Beam.delayBetweenShots,
                                  LdrDamage(mData->weapons.dark_Beam.damageInfo.normal),
                                  LdrDamage(mData->weapons.dark_Beam.damageInfo.charged, true)));
  mBeamInfo.push_back(SWeaponInfo(mData->weapons.light_Beam.delayBetweenShots,
                                  LdrDamage(mData->weapons.light_Beam.damageInfo.normal),
                                  LdrDamage(mData->weapons.light_Beam.damageInfo.charged, true)));
  mBeamInfo.push_back(
      SWeaponInfo(mData->weapons.annihilator_Beam.delayBetweenShots,
                  LdrDamage(mData->weapons.annihilator_Beam.damageInfo.normal),
                  LdrDamage(mData->weapons.annihilator_Beam.damageInfo.charged, true)));
}

const SWeaponInfo& CTweakPlayerGun::GetBeamInfo(CPlayerState::EBeamId beam) const {
  return mBeamInfo[beam];
}

CDamageInfo CTweakPlayerGun::GetDarkBeamBlobDamage() const {
  return LdrDamage(mData->weapons.dark_Beam_Blob);
}

SWeaponInfo::SWeaponInfo(float coolDown, const CDamageInfo& normal, const CDamageInfo& charged)
: mCoolDown(coolDown), mNormal(normal), mCharged(charged) {}

SWeaponInfo CTweakPlayerGun::GetPhazonBeamInfo() const {
  return SWeaponInfo(mData->weapons.phazon_Beam.delayBetweenShots,
                     LdrDamage(mData->weapons.phazon_Beam.damageInfo.normal),
                     LdrDamage(mData->weapons.phazon_Beam.damageInfo.charged, true));
}

CDamageInfo CTweakPlayerGun::GetMissileDamage() const {
  return LdrDamage(mData->weapons.missile);
}

CDamageInfo CTweakPlayerGun::GetBombInfo() const { return LdrDamage(mData->weapons.bomb); }

CDamageInfo CTweakPlayerGun::GetPowerBombInfo() const {
  return LdrDamage(mData->weapons.power_Bomb);
}

CDamageInfo CTweakPlayerGun::GetBlackHoleDamage() const {
  return LdrDamage(mData->beam_Misc.blackhole_Dark, false, true, true);
}

CDamageInfo CTweakPlayerGun::GetSunBurstRaysDamage() const {
  return LdrDamage(mData->beam_Misc.sunBurstRays_Light, false, true, true, true);
}

CDamageInfo CTweakPlayerGun::GetImploderDamage() const {
  return LdrDamage(mData->beam_Misc.imploder_Annihilator, false, true, true, true);
}

float CTweakPlayerGun::GetAIBurnDamage() const { return mData->beam_Misc.aIBurnDamage; }

float CTweakPlayerGun::GetPlayerBurnDamage() const { return mData->beam_Misc.playerBurnDamage; }

int CTweakPlayerGun::GetMaxAbsorbedPhazonShots() const {
  return mData->beam_Misc.maxAbsorbedPhazonShots;
}

float CTweakPlayerGun::GetPhazonShotAbsorbRadius() const {
  return mData->beam_Misc.phazonShotAbsorbRadius;
}

float CTweakPlayerGun::GetGunExtendDistance() const { return mData->position.unknown_0x1547d77b; }

CVector3f CTweakPlayerGun::GetGunPosition() const {
  return CVector3f(mData->position.x, mData->position.y, mData->position.z);
}

CVector3f CTweakPlayerGun::GetGrapplingArmPosition() const { return mData->arm_Position.grappling; }

float CTweakPlayerGun::GetGunHolsterTime() const { return mData->holstering.gunHolsterTime; }

float CTweakPlayerGun::GetGunNotFiringTime() const { return mData->holstering.gunNotFiringTime; }

float CTweakPlayerGun::GetFixedVerticalAim() const {
  return CRelAngle::FromDegrees(mData->holstering.gunHolsteredAngle).AsRadians();
}

float CTweakPlayerGun::GetBombTriggerRadius() const { return mData->weapons.unknown_0xe8907530; }

float CTweakPlayerGun::GetBombDropDelayTime() const { return mData->weapons.bombDropDelayTime; }

float CTweakPlayerGun::GetHoloHoldTime() const { return mData->misc.hologramDisplayTime; }

float CTweakPlayerGun::GetGunTransformTime() const { return mData->misc.gunTransformTime; }

CDamageInfo CTweakPlayerGun::GetComboDamage(CPlayerState::EBeamId beam) const {
  switch (beam) {
  default:
  case CPlayerState::kBI_Power:
    return LdrDamage(mData->beam_Combo.superMissile_Power, false, true);
  case CPlayerState::kBI_Dark:
    return LdrDamage(mData->beam_Combo.darkCombo_Dark, false, true);
  case CPlayerState::kBI_Light:
    return LdrDamage(mData->beam_Combo.lightCombo_Light, false, true);
  case CPlayerState::kBI_Annihilator:
    return LdrDamage(mData->beam_Combo.annihilatorCombo_Annihilator, false, true);
  }
}

CCameraShakerData CTweakPlayerGun::GetRecoilCameraShakerData() const {
  const SLdrCameraShakerData& shaker = mData->recoil;
  // All three native presets use the recoil record's audio effect.
  return CCameraShakerData(shaker.attenuationDistance, shaker.duration, shaker.flagsCameraShaker,
                           CVector3f::Zero(), shaker.horizontalMotion, shaker.verticalMotion,
                           shaker.forwardMotion, mData->recoil.audioEffect);
}

CCameraShakerData CTweakPlayerGun::GetProjectileRecoilCameraShakerData() const {
  const SLdrCameraShakerData& shaker = mData->projectileRecoil;
  // All three native presets use the recoil record's audio effect.
  return CCameraShakerData(shaker.attenuationDistance, shaker.duration, shaker.flagsCameraShaker,
                           CVector3f::Zero(), shaker.horizontalMotion, shaker.verticalMotion,
                           shaker.forwardMotion, mData->recoil.audioEffect);
}

CCameraShakerData CTweakPlayerGun::GetProjectileImpactCameraShakerData() const {
  const SLdrCameraShakerData& shaker = mData->projectileImpact;
  // All three native presets use the recoil record's audio effect.
  return CCameraShakerData(shaker.attenuationDistance, shaker.duration, shaker.flagsCameraShaker,
                           CVector3f::Zero(), shaker.horizontalMotion, shaker.verticalMotion,
                           shaker.forwardMotion, mData->recoil.audioEffect);
}
