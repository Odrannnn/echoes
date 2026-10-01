// `CGrappleArm::SetAnimState` - the helper `CPlayer::BreakGrapple`
// (src/MetroidPrime/Player/CPlayerOrbit.cpp) calls to put the arm back out of the grapple
// animation when a grapple is released. The body is the one already written in
// `src/MetroidPrime/Player/CGrappleArm.cpp`, which `configure.py` holds as a `NonMatching`
// unit (line 491) and `files.cmake` does not list, so nothing in the port compiled it.
//
// **Why a carve-out and not `CGrappleArm.cpp`.**  That file is a whole `NonMatching` unit
// holding the arm's entire state machine - the enter/struck/fidget transitions, the grapple
// beam connect and disconnect, the arm model and its animation queries - and listing it
// would add every one of those bodies and their callees. One function per file is the
// arrangement the other single-body port files use (see `CGrappleArmReturnToDefault.cpp`).
//
// `PlayGrappleAnimation` comes with it because `SetAnimState` is its only caller, and
// `SetStateFlags` and `DisconnectGrappleBeam` because this switch needs both; the first is
// already defined in `CGrappleArmReturnToDefault.cpp`, which is in `files.cmake`, so it is
// not repeated here. `DisconnectGrappleBeam` in turn needs `GrappleBeamDisconnected`, so
// that follows too - the alternative is a stub, and a stub that announces itself buys a
// number rather than the behaviour.
//
// `PlayGrappleAnimation` reaches the PAS database through `CPASDatabase::FindBestAnimation`,
// `CPASAnimParmData` and `CAnimPlaybackParms`; all three are already undefined symbols in
// `docs/research/port_link_baseline.txt`, so this file adds no *new* kind of callee.
#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/Weapons/GunController/CGunController.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Animation/CPASAnimParm.hpp"
#include "Kyoto/Animation/CPASAnimParmData.hpp"
#include "Kyoto/Animation/CPASDatabase.hpp"

// Copied from src/MetroidPrime/Player/CGrappleArm.cpp:32, where it is a file-local enum
// because nothing outside that unit names it. `SetAnimState` needs the grapple entry.
enum EArmPASState { kAPS_Fidget = 10, kAPS_Grapple = 11 };

void CGrappleArm::PlayGrappleAnimation(CAnimData& animData, int anim) {
  const CPASAnimParmData parms(static_cast< pas::EAnimationState >(kAPS_Grapple),
                               CPASAnimParm::FromEnum(anim));
  const int animId = animData.GetPASDatabase().FindBestAnimation(parms, -1).second;
  animData.SetAnimation(CAnimPlaybackParms(animId, -1, 1.f, true), false);
}

void CGrappleArm::SetAnimState(EArmState state) {
  if (!mArmModel) {
    mAnimationState = kAS_Done;
    mStateFlags &= ~kSF_Grappling;
    return;
  }
  if (mAnimationState == state) {
    return;
  }
  CAnimData& animData = *mArmModel->AnimationData();
  animData.EnableLooping(false);
  SetStateFlags(kSF_Grappling);
  switch (state) {
  case kAS_IntoGrapple:
    ResetAuxParams(true);
    PlayGrappleAnimation(animData, 0);
    mBeamActive = false;
    break;
  case kAS_IntoGrappleIdle:
    animData.EnableLooping(true);
    PlayGrappleAnimation(animData, 1);
    break;
  case kAS_FireGrapple:
    PlayGrappleAnimation(animData, 2);
    break;
  case kAS_ConnectGrapple:
    PlayGrappleAnimation(animData, 3);
    break;
  case kAS_Connected:
    PlayGrappleAnimation(animData, 3);
    break;
  case kAS_OutOfGrapple:
    PlayGrappleAnimation(animData, 4);
    DisconnectGrappleBeam();
    break;
  case kAS_Done:
    mStateFlags &= ~kSF_Grappling;
    break;
  default:
    break;
  }
  mAnimationState = state;
}

void CGrappleArm::DisconnectGrappleBeam() {
  mClawGenerator->SetParticleEmission(false);
  mMuzzleGenerator->SetParticleEmission(false);
  mBeamActive = false;
  mSwingT = 0.f;
  GrappleBeamDisconnected();
}

void CGrappleArm::GrappleBeamDisconnected() {
  if (mGrappleLoopSfx) {
    CSfxManager::SfxStop(mGrappleLoopSfx);
    mGrappleLoopSfx.Clear();
  }
}

void CGrappleArm::ResetAuxParams(bool resetGunController) {
  mAuxTransform = CTransform4f::Identity();
  if (resetGunController) {
    mGunController->Reset();
  }
}