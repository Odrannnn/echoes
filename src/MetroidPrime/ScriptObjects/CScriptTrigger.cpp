#include "MetroidPrime/ScriptObjects/CScriptTrigger.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CCollisionActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Enemies/CAi.hpp"
#include "MetroidPrime/TCastTo.hpp"

CScriptTrigger::CScriptTrigger(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                               const CVector3f& position, const CAABox& bounds,
                               const CDamageInfo& damage, const CVector3f& forceField, uint flags,
                               bool deactivateOnEntered, bool deactivateOnExited)
: CActor(uid, name, info, 0, CTransform4f::Translate(position), CModelData(),
         CMaterialList(kMT_Trigger), CActorParameters(), kInvalidUniqueId)
, mAttachedTrigger(kInvalidUniqueId)
, mDamageInfo(damage)
, mForceField(forceField)
, mForceMagnitude(forceField.Magnitude())
, mFlags(flags)
, mBounds(bounds)
, mDeactivateOnEntered(deactivateOnEntered)
, mDeactivateOnExited(deactivateOnExited) {
  for (int i = 0; i < 4; ++i) {
    mPlayerInside[i] = false;
    mPlayerEnvironmentDamage[i] = false;
  }
  if (mFlags & kTFL_DetectPlayer) {
    mFlags &= ~kTFL_DetectPlayer;
    mFlags |= kTFL_DetectMorphedPlayer | kTFL_DetectUnmorphedPlayer | kTFL_DetectScrewAttack;
  }
  SetCallTouch(false);
}

CScriptTrigger::~CScriptTrigger() {}

void CScriptTrigger::Touch(CActor& actor, CStateManager& mgr) {
  // TODO: apply Echoes actor/player filters and register with this or the attached trigger.
}

CScriptTrigger::CObjectTracker::CObjectTracker(TUniqueId id, TUniqueId triggerId) : mId(id) {
  mTriggers.push_back(triggerId);
}

void CScriptTrigger::AddInhabitant(CStateManager& mgr, int playerIndex, TUniqueId id,
                                   TUniqueId triggerId) {
  // TODO: track the contributing trigger, activate the player and process entry effects.
}

CAABox CScriptTrigger::GetTriggerBoundsWR() const {
  return CAABox(mBounds.GetMinPoint() + GetTranslation(), mBounds.GetMaxPoint() + GetTranslation());
}

rstl::optional_object< CAABox > CScriptTrigger::GetTouchBounds() const {
  if (GetActive()) {
    return GetTriggerBoundsWR();
  }
  return rstl::optional_object_null();
}

void CScriptTrigger::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: resolve the Connect/Attach target on area load.
  if (GetActive() && (msg.GetMessage() == kSM_Deactivate || msg.GetMessage() == kSM_XDelete)) {
    ClearInhabitants(mgr);
    for (int i = 0; i < 4; ++i) {
      SetPlayerInside(mgr, false, i);
    }
  }
  CActor::AcceptScriptMsg(mgr, msg);
}

void CScriptTrigger::Think(float dt, CStateManager& mgr) {
  if (GetActive()) {
    UpdateInhabitants(dt, mgr);
  }
}

bool CScriptTrigger::BoundsOverlap(const CAABox& bounds) const {
  const rstl::optional_object< CAABox > touchBounds = GetTouchBounds();
  return touchBounds && touchBounds->DoBoundsOverlap(bounds);
}

void CScriptTrigger::ClearInhabitants(CStateManager& mgr) {
  // TODO: notify each remaining actor and release its per-player trigger state before clearing.
}

void CScriptTrigger::NotifyInhabitantExited(CActor& actor, CStateManager& mgr) {
  InhabitantExited(actor, mgr);
  if (ShouldSendScriptMsgs(actor, mgr)) {
    SendScriptMsgs(kSS_Exited, mgr, actor.GetUniqueId(), kSM_None);
  }
}

void CScriptTrigger::NotifyInhabitantAdded(CActor& actor, CStateManager& mgr) {
  InhabitantAdded(actor, mgr);
  if (ShouldSendScriptMsgs(actor, mgr)) {
    SendScriptMsgs(kSS_Entered, mgr, actor.GetUniqueId(), kSM_None);
  }
}

void CScriptTrigger::NotifyInhabitantIdle(CActor& actor, CStateManager& mgr) {
  InhabitantIdle(actor, mgr);
  if (ShouldSendScriptMsgs(actor, mgr)) {
    SendScriptMsgs(kSS_Inside, mgr, actor.GetUniqueId(), kSM_None);
  }
}

void CScriptTrigger::UpdateCameraInhabitant(TUniqueId id, CStateManager& mgr) {
  // TODO: maintain camera overlaps, including forwarding to an attached trigger.
}

void CScriptTrigger::SetPlayerInside(CStateManager& mgr, bool inside, int playerIndex) {
  // TODO: synchronize per-player membership, environment damage and force-trigger ownership.
}

void CScriptTrigger::UpdateInhabitants(float dt, CStateManager& mgr) {
  // TODO: update linked-trigger overlaps, player filters, damage, force and exit events.
}

bool CScriptTrigger::HasInhabitant(TUniqueId id) const {
  for (rstl::list< CObjectTracker >::const_iterator it = mInhabitants.begin();
       it != mInhabitants.end(); ++it) {
    if (it->GetObjectId() == id) {
      const rstl::list< TUniqueId >& triggers = it->GetTriggers();
      for (rstl::list< TUniqueId >::const_iterator trigger = triggers.begin();
           trigger != triggers.end(); ++trigger) {
        if (*trigger == GetUniqueId()) {
          return true;
        }
      }
    }
  }
  return false;
}

void CScriptTrigger::InhabitantAdded(CActor&, CStateManager&) {}

void CScriptTrigger::InhabitantIdle(CActor&, CStateManager&) {}

void CScriptTrigger::InhabitantExited(CActor&, CStateManager&) {}

void CScriptTrigger::InhabitantRejected(CActor&, CStateManager&) {}

bool CScriptTrigger::ShouldSendScriptMsgs(CActor& actor, CStateManager& mgr) const {
  // Only the camera the player is currently looking through counts as an inhabitant.
  if (CGameCamera* cam = TCastToPtr< CGameCamera >(actor)) {
    if (cam->CameraManager(mgr).GetCurrentCameraId(true) != cam->GetUniqueId()) {
      return false;
    }
  }
  return true;
}

bool CScriptTrigger::GetPlayerInside(int playerIndex) const { return mPlayerInside[playerIndex]; }

bool CScriptTrigger::IsAI(CStateManager& mgr, CActor& actor) const {
  // An AI directly, or the owner of a collision actor standing in for it.
  if (TCastToConstPtr< CAi >(actor) != nullptr) {
    return true;
  }
  if (const CCollisionActor* collisionActor = TCastToConstPtr< CCollisionActor >(actor)) {
    if (TCastToConstPtr< CAi >(mgr.GetObjectById(collisionActor->GetOwner())) != nullptr) {
      return true;
    }
  }
  return false;
}

bool CScriptTrigger::ReplaceInhabitant(TUniqueId oldId, TUniqueId newId, CStateManager& mgr) {
  const CActor* oldActor = TCastToConstPtr< CActor >(mgr.GetObjectById(oldId));
  const CActor* newActor = TCastToConstPtr< CActor >(mgr.GetObjectById(newId));
  if (oldActor == nullptr || newActor == nullptr) {
    return false;
  }
  bool replaced = false;
  if (!HasInhabitant(newId)) {
    for (rstl::list< CObjectTracker >::iterator it = mInhabitants.begin();
         it != mInhabitants.end(); ++it) {
      if (it->GetObjectId() == oldId) {
        it->SetObjectId(newId);
        replaced = true;
        break;
      }
    }
  } else {
    for (rstl::list< CObjectTracker >::iterator it = mInhabitants.begin();
         it != mInhabitants.end(); ++it) {
      if (it->GetObjectId() == oldId) {
        mInhabitants.erase(it);
        break;
      }
    }
  }
  return replaced;
}

bool CScriptTrigger::RemoveInhabitantIfOutside(TUniqueId id, CStateManager& mgr) {
  const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(id));
  if (actor == nullptr) {
    return false;
  }
  bool removed = false;
  for (rstl::list< CObjectTracker >::iterator it = mInhabitants.begin(); it != mInhabitants.end();
       ++it) {
    if (it->GetObjectId() == id) {
      const rstl::optional_object< CAABox > bounds = GetTouchBounds();
      const rstl::optional_object< CAABox > actorBounds = actor->GetTouchBounds();
      if (bounds && actorBounds && !BoundsOverlap(*actorBounds)) {
        mInhabitants.erase(it);
        removed = true;
      }
      break;
    }
  }
  return removed;
}

bool CScriptTrigger::RemoveInhabitant(TUniqueId id, CStateManager& mgr) {
  if (TCastToConstPtr< CActor >(mgr.GetObjectById(id)) == nullptr) {
    return false;
  }
  bool removed = false;
  for (rstl::list< CObjectTracker >::iterator it = mInhabitants.begin(); it != mInhabitants.end();
       ++it) {
    if (it->GetObjectId() == id) {
      mInhabitants.erase(it);
      removed = true;
      break;
    }
  }
  return removed;
}
