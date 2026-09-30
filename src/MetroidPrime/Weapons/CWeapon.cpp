#include "MetroidPrime/Weapons/CWeapon.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CFluidPlaneManager.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"

CWeapon::CWeapon(TUniqueId uid, TAreaId areaId, bool active, TUniqueId owner, EWeaponType type,
                 const rstl::string& name, const CTransform4f& xf, const CMaterialFilter& filter,
                 const CMaterialList& materials, const CDamageInfo& damageInfo, int attribs,
                 const CModelData& modelData)
: CActor(uid, name, CEntityInfo(areaId, CEntity::NullConnectionList, active), 0, xf, modelData,
         materials, CActorParameters(), kInvalidUniqueId)
, mProjectileAttribs(attribs)
, mOwnerId(owner)
, mWeaponType(type)
, mFilter(filter)
, mOrigDamageInfo(damageInfo)
, mCurDamageInfo(damageInfo)
, mCurTime(0.f)
, mDamageFalloffSpeed(0.f)
, mDamageDuration(0.f)
, mInterferenceDuration(0.f) {
  if ((attribs & 0x02000000) != 0) {
    SetAlphaSorted(true);
  }
}

CWeapon::~CWeapon() {}

void CWeapon::SetDamageFalloffSpeed(float speed) {
  if (speed > 0.f) {
    mDamageFalloffSpeed = 1.f / speed;
  }
}

void CWeapon::Think(float dt, CStateManager& mgr) {
  mCurTime += dt;
  if (HasAttrib(kPA_DamageFalloff)) {
    float max = 1.f - mCurTime * mDamageFalloffSpeed;
    float scale = max < 0.f ? 0.f : max;
    float damage = scale * mOrigDamageInfo.GetDamage();
    float radius = scale * mOrigDamageInfo.GetRadius();
    float knockback = scale * mOrigDamageInfo.GetKnockBackPower();
    mCurDamageInfo =
        CDamageInfo(mOrigDamageInfo.GetWeaponMode(), damage,
                    (double)(scale * mOrigDamageInfo.GetDamage()), radius, knockback);
  } else {
    mCurDamageInfo = mOrigDamageInfo;
  }
  CActor::Think(dt, mgr);
}

void CWeapon::FluidFXThink(EFluidState state, CScriptWater& water, CStateManager& mgr) {
  bool doRipple = true;
  float mag = 0.f;
  switch (mWeaponType) {
  case kWT_Power:
    mag = 0.1f;
    break;
  case kWT_Dark:
    mag = 0.3f;
    break;
  case kWT_Light:
    mag = 0.1f;
    break;
  case kWT_Annihilator:
    mag = 0.f;
    break;
  case kWT_Missile:
    mag = 0.5f;
    break;
  case kWT_Phazon:
    mag = 0.1f;
    break;
  default:
    doRipple = false;
    break;
  }

  if ((mProjectileAttribs & CWeapon::kPA_ComboShot) != 0) {
    if (state != kFS_InFluid) {
      mag += 0.5f;
    } else {
      doRipple = false;
    }
  }

  if ((mProjectileAttribs & CWeapon::kPA_Charged) != 0) {
    mag += 0.25f;
  }

  if (mag > 1.f) {
    mag = 1.f;
  }

  if (doRipple) {
    CVector3f pos(GetTranslation().GetX(), GetTranslation().GetY(),
                  water.GetTriggerBoundsWR().GetMaxPoint().GetZ());
    if ((mProjectileAttribs & CWeapon::kPA_ComboShot) != 0) {
      if (!water.CanRippleAtPoint(pos)) {
        doRipple = false;
      }
    } else if (state == kFS_InFluid) {
      doRipple = false;
    }

    if (doRipple) {
      bool sfx = state == kFS_EnteredFluid || state == kFS_LeftFluid;
      mgr.FluidPlaneManager()->CreateSplash(GetUniqueId(), mgr, water, pos, mag, sfx);
    }
  }
}

void CWeapon::Render(const CStateManager& mgr) const {}

EWeaponCollisionResponseTypes CWeapon::GetCollisionResponseType(const CVector3f& position,
                                                                const CVector3f& direction,
                                                                const CWeaponMode& mode,
                                                                int attribs) const {
  return kWCR_Projectile;
}
