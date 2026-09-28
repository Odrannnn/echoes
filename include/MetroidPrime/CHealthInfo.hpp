#ifndef _CHEALTHINFO
#define _CHEALTHINFO

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Weapons/WeaponTypes.hpp"

class CHealthInfo {
public:
  CHealthInfo(float hp, float resist);
#if defined(MP_RETAIL_OUT_OF_LINE_COPIES) && !defined(TARGET_PC)
  // Opt-in, for TUs whose object must not carry the weak copy: retail's is at 0x80070D60 and
  // CAi's constructor calls it there. See src/MetroidPrime/Enemies/CAi.cpp.
  CHealthInfo(const CHealthInfo&);
#endif

  void SetHP(float hp) { healthB = hp; }
  void SetKnockbackResistance(float resist) { knockbackResistance = resist; }
  float GetHP() const { return healthB; }

  void SetCauseOfDeathWeapon(CWeaponMode mode, TUniqueId, TUniqueId, bool, bool);
  void fn_8014206C(const CWeaponMode&, TUniqueId, TUniqueId, bool);

private:
  float healthA;
  float healthB;
  float knockbackResistance;
  CWeaponMode weaponModeA;
  TUniqueId uidA;
  TUniqueId uidB;
  CWeaponMode weaponModeB;
  TUniqueId uidC;
  TUniqueId uidD;
  bool flagA : 1;
  bool flagB : 1;
};
CHECK_SIZEOF(CHealthInfo, 0x20)

#endif // _CHEALTHINFO
