#ifndef _CWEAPONMGR
#define _CWEAPONMGR

#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Weapons/WeaponTypes.hpp"

class CWeaponMgr {
public:
  int GetNumActive(TUniqueId senderId, EWeaponType type) const;
  void fn_800B321C(TUniqueId senderId, EWeaponType type);
  void fn_800B32E0(TUniqueId senderId, EWeaponType type);
};

#endif // _CWEAPONMGR
