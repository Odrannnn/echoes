#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerCameraBob.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"

class CPatterned;
class CScriptGrapplePoint;
class CSwarmBasics;

// NonMatching scaffold. Definitions are in reverse target order for deferred inlining.

bool CPlayer::ValidateOrbitTargetIdAndPointer(const TUniqueId target,
                                              const CStateManager& mgr) const {
  if (target == kInvalidUniqueId) {
    return false;
  }
  return TCastToConstPtr< CActor >(mgr.GetObjectById(target)) != nullptr;
}

int CPlayer::ValidateCurrentOrbitTargetId(CStateManager& mgr) {
  // TODO: Recover orbit-validation result values and target checks.
  return 0;
}

int CPlayer::ValidateOrbitTargetId(TUniqueId target, CStateManager& mgr) const {
  // TODO: Recover the remaining target behavior.
  return 0;
}

float CPlayer::GetOrbitMaxTargetDistance() const {
  float distance = GetTweakPlayer()->GetOrbitMaxTargetDistance();
  if (mPlayerState->GetCurrentVisor() == CPlayerState::kPV_Scan) {
    distance = GetTweakPlayer()->GetScanMaxTargetDistance();
  }
  return distance;
}

float CPlayer::GetOrbitMaxLockDistance() const {
  float distance = GetTweakPlayer()->GetOrbitMaxLockDistance();
  if (mPlayerState->GetCurrentVisor() == CPlayerState::kPV_Scan) {
    distance = GetTweakPlayer()->GetScanMaxLockDistance();
  }
  return distance;
}

void CPlayer::UpdateOrbitTarget(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateOrbitOrientation(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateOrbitSelection(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::ActivateOrbitSource(CStateManager& mgr) {
  switch (mOrbitSource) {
  case 0:
  default:
    OrbitCarcass(mgr);
    break;
  case 1:
    SetOrbitRequest(kOR_InvalidateTarget, mgr);
    break;
  case 2:
    OrbitPoint(kOT_Far, mgr);
    break;
  }
}

void CPlayer::UpdateOrbitInput(const CFinalInput& input, float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateOrbitZone() {
  if (mPlayerState->GetCurrentVisor() != CPlayerState::kPV_Scan) {
    mOrbitZoneType = kZT_Ellipse;
    mOrbitScreenBoxType = 1;
    mOrbitZoneMode = kZI_Targeting;
  } else {
    mOrbitZoneType = kZT_Box;
    mOrbitScreenBoxType = 2;
    mOrbitZoneMode = kZI_Scan;
  }
}

void CPlayer::UpdateOrbitModeTimer(float dt) {
  if (mOrbitState == kOS_NoOrbit && mOrbitModeTimer > 0.f) {
    mOrbitModeTimer -= dt;
  } else {
    mOrbitModeTimer = 0.f;
  }
}

void CPlayer::UpdateOrbitPreventionTimer(float dt) {
  if (mOrbitPreventionTimer > 0.f) {
    mOrbitPreventionTimer -= dt;
  }
}

void CPlayer::AddOrbitDisableSource(CStateManager& mgr, TUniqueId id) {
  if (mOrbitDisableSources.size() >= 5) {
    return;
  }
  for (rstl::reserved_vector< TUniqueId, 5 >::iterator it = mOrbitDisableSources.begin();
       it != mOrbitDisableSources.end(); ++it) {
    if (*it == id) {
      return;
    }
  }
  mOrbitDisableSources.push_back(id);
  SetAimTarget(kInvalidUniqueId);
  const TUniqueId orbitTarget = GetOrbitTargetId();
  if (!TCastToConstPtr< CScriptGrapplePoint >(mgr.GetObjectById(orbitTarget))) {
    SetOrbitTargetId(kInvalidUniqueId, mgr);
  }
}

void CPlayer::RemoveOrbitDisableSource(TUniqueId id) {
  for (rstl::reserved_vector< TUniqueId, 5 >::iterator it = mOrbitDisableSources.begin();
       it != mOrbitDisableSources.end(); ++it) {
    if (*it == id) {
      mOrbitDisableSources.erase(it);
      break;
    }
  }
}

bool CPlayer::CheckOrbitDisableSourceList() const { return !mOrbitDisableSources.empty(); }

bool CPlayer::CheckOrbitDisableSourceList(const CStateManager& mgr) {
  rstl::reserved_vector< TUniqueId, 5 >::iterator it = mOrbitDisableSources.begin();
  while (it != mOrbitDisableSources.end()) {
    if (mgr.GetObjectById(*it) == nullptr) {
      mOrbitDisableSources.erase(it);
      it = mOrbitDisableSources.begin();
    } else {
      ++it;
    }
  }
  return !mOrbitDisableSources.empty();
}

bool CPlayer::WithinOrbitScreenEllipse(const CVector3f& screenPosition,
                                       EPlayerZoneInfo zone) const {
  // TODO: Recover the remaining target behavior.
  return false;
}

bool CPlayer::WithinOrbitScreenBox(const CVector3f& screenPosition, EPlayerZoneInfo zone,
                                   EPlayerZoneType type) const {
  // TODO: Recover the remaining target behavior.
  return false;
}

void CPlayer::FindOrbitableObjects(const rstl::reserved_vector< TUniqueId, 1024 >& candidates,
                                   rstl::reserved_vector< TUniqueId, 64 >& objects,
                                   EPlayerZoneInfo zone, EPlayerZoneType type, CStateManager& mgr,
                                   bool offScreen) {
  // TODO: Recover the remaining target behavior.
}

TUniqueId CPlayer::FindBestOrbitableObject(const rstl::reserved_vector< TUniqueId, 64 >& objects,
                                           EPlayerZoneInfo zone, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return kInvalidUniqueId;
}

void CPlayer::UpdateOrbitableObjects(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

TUniqueId CPlayer::FindOrbitTargetId(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return kInvalidUniqueId;
}

// Guessed name
TUniqueId CPlayer::CheckEnemyAgainstOrbitZone(TUniqueId target, EPlayerZoneInfo zone,
                                              EPlayerZoneType type, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return kInvalidUniqueId;
}

TUniqueId CPlayer::FindAimTargetId(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return kInvalidUniqueId;
}

// Guessed name
void CPlayer::UpdateAimCandidates(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::ValidateObjectForMode(TUniqueId target, CStateManager& mgr) const {
  // TODO: Recover the remaining target behavior.
  return false;
}

bool CPlayer::ValidateAimTargetId(TUniqueId target, CStateManager& mgr, float dt) {
  // TODO: Recover the remaining target behavior.
  return false;
}

void CPlayer::UpdateAimTargetTimer(float dt) {
  if (mAimTarget != kInvalidUniqueId && mAimTargetTimer > 0.f) {
    mAimTargetTimer -= dt;
  }
}

void CPlayer::UpdateAimTarget(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::SetOrbitPosition(float distance) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateOrbitFixedPosition() {
  const CVector3f eye = GetEyePosition();
  const CVector3f rot = GetTransform().Rotate(mOrbitVector);
  mOrbitPoint = eye + rot;
}

void CPlayer::UpdateOrbitZPosition() {
  switch (mOrbitState) {
  case kOS_OrbitPoint:
    if (CMath::AbsF(mOrbitVector.GetZ()) < GetTweakPlayer()->GetOrbitZRange()) {
      mOrbitPoint.SetZ(mOrbitVector[kDZ] + (GetTranslation().GetZ() + GetEyeHeight()));
    }
    break;
  default:
    break;
  }
}

void CPlayer::UpdateOrbitPosition(float distance, const CStateManager& mgr) {
  switch (mOrbitState) {
  case kOS_NoOrbit:
    break;
  case kOS_OrbitPoint:
  case kOS_OrbitCarcass:
    SetOrbitPosition(distance);
    break;
  case kOS_ForcedOrbitObject:
  case kOS_Grapple:
  case kOS_OrbitObject:
    if (const CActor* act = TCastToConstPtr< CActor >(mgr.GetObjectById(GetOrbitTargetId()))) {
      if (mOrbitTargetId != kInvalidUniqueId) {
        mOrbitPoint = act->GetOrbitPosition(mgr);
      }
    }
    break;
  default:
    break;
  }
}

void CPlayer::SetOrbitTargetId(TUniqueId target, const CStateManager& mgr) {
  if (target != kInvalidUniqueId) {
    const CPatterned* patterned = TCastToConstPtr< CPatterned >(mgr.GetObjectById(target));
    const CSwarmBasics* swarm = TCastToConstPtr< CSwarmBasics >(mgr.GetObjectById(target));
    if (patterned || swarm) {
      mOrbitingEnemy = true;
    } else {
      mOrbitingEnemy = false;
    }
    x591_ = true;
  }
  mOrbitTargetId = target;
  if (mOrbitTargetId == kInvalidUniqueId) {
    mOrbitLockEstablished = false;
    x591_ = false;
  }
}

void CPlayer::SetOrbitState(EPlayerOrbitState state, const CStateManager& mgr) {
  mOrbitState = state;
  x594_ = 0.f;
  CFirstPersonCamera* camera = mCameraManager->FirstPersonCamera();
  switch (mOrbitState) {
  case kOS_OrbitObject:
    camera->SetLockCamera(false);
    break;
  case kOS_OrbitCarcass: {
    camera->SetLockCamera(true);
    CVector3f playerToPoint = mOrbitPoint - GetTranslation();
    playerToPoint.SetZ(0.f);
    if (playerToPoint.CanBeNormalized()) {
      mOrbitPointDistance = playerToPoint.Magnitude();
    } else {
      mOrbitPointDistance = 0.f;
    }
    SetOrbitTargetId(kInvalidUniqueId, mgr);
    mOrbitNextTargetId = kInvalidUniqueId;
    break;
  }
  case kOS_NoOrbit:
    mOrbitModeTimer = GetTweakPlayer()->GetOrbitModeTimer();
    mOrbitModeTimer = 0.28f;
    SetOrbitTargetId(kInvalidUniqueId, mgr);
    break;
  case kOS_OrbitPoint:
    SetOrbitTargetId(kInvalidUniqueId, mgr);
    break;
  default:
    break;
  }
}

CVector3f CPlayer::GetHUDOrbitTargetPosition() const {
  return mOrbitPoint + mCameraBob->GetCameraBobTransformation().GetTranslation();
}

float CPlayer::CalculateOrbitMinDistance(EPlayerOrbitType type) const {
  float distance = GetTweakPlayer()->GetOrbitMinDistance(type);
  distance *= CMath::Clamp(1.f, CMath::AbsF(mOrbitPoint.GetZ() - GetTranslation().GetZ()) / 20.f,
                           4.f);
  return distance;
}

void CPlayer::OrbitPoint(EPlayerOrbitType type, CStateManager& mgr) {
  mOrbitType = type;
  SetOrbitState(kOS_OrbitPoint, mgr);
  SetOrbitPosition(GetTweakPlayer()->GetOrbitNormalDistance(mOrbitType));
}

void CPlayer::OrbitCarcass(CStateManager& mgr) {
  if (mOrbitState == kOS_OrbitObject) {
    mOrbitType = kOT_Default;
    SetOrbitState(kOS_OrbitCarcass, mgr);
  }
}

void CPlayer::PreventFallingCameraPitch() {
  mJumpCameraTimer = 0.f;
  mFallCameraTimer = 0.01f;
  mCancelCameraPitch = true;
}

bool CPlayer::InGrappleJumpCooldown() const {
  if (mMovementState != NPlayer::kMS_OnGround &&
      (mGrappleJumpTimeout > 0.f || (mJumpCameraTimer == 0.f && mOrbitState == kOS_NoOrbit))) {
    return true;
  } else {
    return false;
  }
}

void CPlayer::fn_8011eac4(EPlayerOrbitRequest request, CStateManager& mgr) {
  for (int i = 0; i < static_cast< uint >(mgr.GetNumPlayers()); i++) {
    CPlayer* player = mgr.GetPlayer(i);
    if (player->GetUniqueId() == GetUniqueId()) {
      continue;
    }
    if (player->GetOrbitTargetId() == GetUniqueId()) {
      player->SetOrbitRequestForTarget(GetUniqueId(), request, mgr);
    }
  }
}

void CPlayer::SetOrbitRequestForTarget(TUniqueId target, EPlayerOrbitRequest request,
                                       CStateManager& mgr) {
  if ((mOrbitState == kOS_OrbitObject || mOrbitState == kOS_Grapple ||
       mOrbitState == kOS_ForcedOrbitObject) &&
      target == mOrbitTargetId) {
    SetOrbitRequest(request, mgr);
  }
}

void CPlayer::SetOrbitRequest(EPlayerOrbitRequest request, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::BreakGrapple(EPlayerOrbitRequest request, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::BeginGrapple(CVector3f& direction, CStateManager& mgr) {
  direction.SetZ(0.f);
  if (direction.CanBeNormalized()) {
    mGrappleSwingAxis.SetX(direction.GetY());
    mGrappleSwingAxis.SetY(-direction.GetX());
    mGrappleSwingAxis.Normalize();
    mGrappleSwingTimer = 0.f;
    SetOrbitState(kOS_Grapple, mgr);
    mGrappleState = kGS_Pull;
    RemoveMaterial(kMT_GroundCollider, mgr);
  }
}

void CPlayer::ApplyGrappleJump(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateGrappleState(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::ValidateFPPosition(CVector3f position, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return false;
}

void CPlayer::ApplyGrappleForces(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdateGrappleArmTransform(const CVector3f& offset, CStateManager& mgr, float dt) {
  // TODO: Recover the remaining target behavior.
}

CVector3f CPlayer::fn_8011ca08() const {
  const CVector3f eye = GetEyePosition();
  float distance;
  if (mOrbitState == kOS_OrbitObject) {
    distance = (mOrbitPoint - eye).Magnitude();
  } else {
    distance = 0.5f;
  }
  const CVector3f dir = fn_80019360().GetForward();
  return eye + dir * distance;
}

void CPlayer::fn_8011c3c0() {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
// Guessed name
// Guessed name
void* CPlayer::GetReflectionTextureData() const { return mReflectionTextureData; }

void* CPlayer::GetIndirectTextureData() const { return mIndirectTextureData; }

void* CPlayer::GetMaskTextureData() const { return mMaskTextureData; }
