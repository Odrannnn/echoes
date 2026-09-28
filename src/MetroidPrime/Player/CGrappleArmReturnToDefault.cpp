// `CGrappleArm::ReturnToDefault` and `CGrappleArm::SetStateFlags` - the two state helpers
// `CPlayerGun::ReturnArmAndGunToDefault` (src/MetroidPrime/Player/CPlayerGun.cpp:956) calls.
// Both bodies are the ones already written in `src/MetroidPrime/Player/CGrappleArm.cpp`,
// which `configure.py` holds as a `NonMatching` unit (line 452) and `files.cmake` does not
// list, so nothing in the port compiled them.
//
// **Why a carve-out and not `CGrappleArm.cpp`.**  That file is a whole `NonMatching` unit
// holding the arm's entire state machine - the enter/struck/fidget transitions, the
// grapple beam connect and disconnect, the arm model and its animation queries - and
// listing it would add every one of those bodies and their callees, which is a net *rise*
// in the port's undefined count.  One function per file is the arrangement the other
// single-body port files use (see `CGameAreaSetAreaAttributes.cpp`).
//
// `SetStateFlags` is retail's read/merge/write of the flag byte: going back to
// `kSF_Default` is the only transition that has to *preserve* the two flags which are
// owned by something other than the caller - `kSF_GunChanging`, which the gun controller
// sets while a beam swap is in flight, and `kSF_Grappling`, which the arm itself sets on a
// live grapple - so a return-to-default does not silently cancel a beam swap or drop a
// grapple.  Every other transition replaces the byte outright, and `0` clears it.
//
// `ReturnToDefault` calls `CGunController::ReturnToDefault`, which is already an undefined
// symbol in `docs/research/port_link_baseline.txt`, so this is net -1 with no new callees.
#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Weapons/GunController/CGunController.hpp"

void CGrappleArm::ReturnToDefault(CStateManager& mgr, float delay, bool reset) {
  if (mStateFlags != 0) {
    SetStateFlags(kSF_Default);
    mGunController->ReturnToDefault(mgr, delay, reset);
  }
}

void CGrappleArm::SetStateFlags(uint flags) {
  uint preserved = 0;
  if (flags == kSF_Default) {
    if (mStateFlags & kSF_GunChanging) {
      preserved = kSF_GunChanging;
    }
    if (mStateFlags & kSF_Grappling) {
      preserved = kSF_Grappling;
    }
  }
  mStateFlags = flags != 0 ? flags | kSF_Default | preserved : 0;
}
