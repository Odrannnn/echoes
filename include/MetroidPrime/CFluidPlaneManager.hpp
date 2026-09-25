#ifndef _CFLUIDPLANEMANAGER
#define _CFLUIDPLANEMANAGER

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

class CScriptWater;
class CStateManager;
class CVector3f;

// Partial declaration: only the calls CAi::FluidFXThink makes.
class CFluidPlaneManager {
public:
  float GetLastSplashDeltaTime(TUniqueId uid) const; // fn_800ECFD8; name from Prime 1
  // fn_800ECCDC; signature from Trilogy
  void CreateSplash(TUniqueId splasher, CStateManager& mgr, const CScriptWater& water,
                    const CVector3f& pos, float factor, bool sfx);
};

#endif // _CFLUIDPLANEMANAGER
