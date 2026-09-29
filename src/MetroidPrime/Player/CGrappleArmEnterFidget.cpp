// `CGrappleArm::EnterFidget` - retail `EnterFidget__11CGrappleArmFR13CStateManageriii`.
//
// `CPlayerGun::EnterFidget` (src/MetroidPrime/Player/CPlayerGun.cpp) calls it, and `files.cmake`
// does not list `src/MetroidPrime/Player/CGrappleArm.cpp`, so the port's link had no definition
// and the symbol went undefined when that body was decompiled.
//
// **Why a carve-out and not `CGrappleArm.cpp`.** Same reason as
// `CGrappleArmTouchModel.cpp` and `CGrappleArmReturnToDefault.cpp`: that file is a whole
// `NonMatching` unit and listing it is a net *rise* in the port's undefined count.
//
// The body is the one already written in `CGrappleArm.cpp:657`, verbatim. Its two callees are
// both already resolved by the port: `SetStateFlags` by `CGrappleArmReturnToDefault.cpp`, and
// `CGunController::EnterFidget` by `src/MetroidPrime/Weapons/GunController/CGunController.cpp`,
// which `files.cmake` does list. So this is net -1 with no new callees.
#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Weapons/GunController/CGunController.hpp"

void CGrappleArm::EnterFidget(CStateManager& mgr, int type, int gunId, int animSet) {
  SetStateFlags(kSF_Fidget);
  if (!mDependenciesLoading) {
    mGunController->EnterFidget(mgr, type, mBeamId, animSet);
  }
}
