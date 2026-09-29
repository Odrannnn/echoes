// `CGrappleArm::TouchModel` - retail `TouchModel__11CGrappleArmCFRC13CStateManager`.
//
// `CPlayerGun::TouchModel` (src/MetroidPrime/Player/CPlayerGun.cpp) calls it, and `files.cmake`
// does not list `src/MetroidPrime/Player/CGrappleArm.cpp`, so the port's link had no definition
// and the symbol went undefined when that body was decompiled.
//
// **Why a carve-out and not `CGrappleArm.cpp`.** That file is a whole `NonMatching` unit holding
// the arm's entire state machine - the enter/struck/fidget transitions, the grapple beam
// connect and disconnect, the arm model and its animation queries - and listing it would add
// every one of those bodies and their callees, which is a net *rise* in the port's undefined
// count. `CGrappleArmReturnToDefault.cpp` is the same arrangement for the same reason.
//
// The body is the one already written in `CGrappleArm.cpp:165`, verbatim. Its two callees,
// `CModel::Touch(int)` and `CModelData::Touch(const CStateManager&, int)`, resolve inside the
// port (`src/Kyoto/Graphics/CModelTouch.cpp` and `CGunWeaponTouch.cpp`'s sibling), so this is
// net -1 with no new callees.
#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "Kyoto/Graphics/CModel.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"

void CGrappleArm::TouchModel(const CStateManager& mgr) const {
  if (mStateFlags != 0) {
    mArmModel->Touch();
    if (!mGrappleGearModel.IsNull()) {
      mGrappleGearModel.Touch(mgr, 0);
    }
  }
}
