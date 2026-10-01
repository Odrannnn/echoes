#include "MetroidPrime/ScriptObjects/CScriptTeamAiMgr.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CAi.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/algorithm.hpp"

#include <float.h>
#include <limits.h>

// libc/float.h's `FLT_MAX` is `(*(float*)__float_max)`, which makes mwcceppc materialise the
// address in a register and load through it instead of reading the constant in place. Retail
// reads it in place, so the constant is spelled as a literal. Same finding, same workaround, as
// src/MetroidPrime/PathFinding/CPathFindArea.cpp:17 and CPlayerVisor.cpp:22.
#undef FLT_MAX
#define FLT_MAX 3.402823466e+38f

// Guessed name
class CTeamAiPredicate : public CValidEntityPredicate {
public:
  // CValidEntityPredicate
  bool IsValid(const CStateManager& mgr, TUniqueId id) const override;
};

// Guessed name, following the Prime counterpart.
struct CRoleSorter {
  CRoleSorter(const CVector3f& position, int type) : mPosition(position), mType(type) {}
  bool operator()(const CTeamAiRole& a, const CTeamAiRole& b) const;

  CVector3f mPosition;
  int mType;
};

bool CTeamAiPredicate::IsValid(const CStateManager& mgr, TUniqueId id) const {
  return TCastToConstPtr< CScriptTeamAiMgr >(mgr.GetObjectById(id)) != nullptr;
}

bool CRoleSorter::operator()(const CTeamAiRole& a, const CTeamAiRole& b) const {
  const float aDist = (mPosition - a.GetTeamPosition()).MagSquared();
  const float bDist = (mPosition - b.GetTeamPosition()).MagSquared();

  switch (mType) {
  case 0:
    return a.GetOwnerId().Value() < b.GetOwnerId().Value();
  case 1:
    return aDist < bDist;
  case 2:
  default:
    if (a.GetTeamAiRole() == b.GetTeamAiRole()) {
      return aDist < bDist;
    }
    return a.GetTeamAiRole() < b.GetTeamAiRole();
  }
}

CScriptTeamAiMgr::CTeamAiData::CTeamAiData()
: mAiCount(0)
, mMeleeCount(0)
, mProjectileCount(0)
, mOtherRoleCount(0)
, mMaxMeleeAttackerCount(0)
, mMaxProjectileAttackerCount(0)
, mPositionMode(0)
, mMeleeTimeInterval(0.f)
, mProjectileTimeInterval(0.f) {}

CScriptTeamAiMgr::CScriptTeamAiMgr(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                                   const CTeamAiData& data)
: CEntity(uid, info, name, 0)
, mData(data)
, mRoles()
, mMeleeAttackers()
, mProjectileAttackers()
, mTimeDirty(0.f)
, mTeamCaptainId(kInvalidUniqueId)
, mTimeSinceMelee(data.mMeleeTimeInterval)
, mTimeSinceProjectile(data.mProjectileTimeInterval)
, mWasHit(false)
, mPlayerForwardProjectionDistance(0.f)
, mTeamActions() {
  if (mData.mAiCount != 0) {
    mRoles.reserve(mData.mAiCount);
  }
  if (mData.mMeleeCount != 0) {
    mMeleeAttackers.reserve(mData.mMeleeCount);
  }
  if (mData.mProjectileCount != 0) {
    mProjectileAttackers.reserve(mData.mProjectileCount);
  }
}

CScriptTeamAiMgr::~CScriptTeamAiMgr() {}

TUniqueId CScriptTeamAiMgr::GetAssociatedTeamId(const CAi& ai, CStateManager& mgr) {
  return ai.CheckConnectedObject_if(mgr, kSS_Active, kSM_Play, CTeamAiPredicate());
}

const CTeamAiRole* CScriptTeamAiMgr::GetTeamAiRole(const CStateManager& mgr, TUniqueId teamId,
                                                   TUniqueId memberId) {
  if (const CScriptTeamAiMgr* team =
          TCastToConstPtr< CScriptTeamAiMgr >(mgr.GetObjectById(teamId))) {
    return team->GetRole(memberId);
  }
  return nullptr;
}

bool CScriptTeamAiMgr::CanStartAttack(EAttackType type, CStateManager& mgr, TUniqueId teamId,
                                      TUniqueId memberId) {
  if (CScriptTeamAiMgr* team =
          TCastToPtr< CScriptTeamAiMgr >(mgr.GetObjectByIdFromListAll(teamId))) {
    if (team->HasTeamAiRole(memberId)) {
      if (type == kAT_Melee) {
        return team->CanStartMeleeAttack(memberId);
      }
      if (type == kAT_Projectile) {
        return team->CanStartProjectileAttack(memberId);
      }
    }
  }
  return false;
}

bool CScriptTeamAiMgr::StartAttack(EAttackType type, CStateManager& mgr, TUniqueId teamId,
                                   TUniqueId memberId) {
  if (CScriptTeamAiMgr* team =
          TCastToPtr< CScriptTeamAiMgr >(mgr.GetObjectByIdFromListAll(teamId))) {
    if (team->HasTeamAiRole(memberId)) {
      if (type == kAT_Melee) {
        return team->StartMeleeAttack(memberId);
      }
      if (type == kAT_Projectile) {
        return team->StartProjectileAttack(memberId);
      }
    }
  }
  return false;
}

void CScriptTeamAiMgr::EndAttack(EAttackType type, CStateManager& mgr, TUniqueId teamId,
                                 TUniqueId memberId, bool clearRole) {
  if (CScriptTeamAiMgr* team =
          TCastToPtr< CScriptTeamAiMgr >(mgr.GetObjectByIdFromListAll(teamId))) {
    if (team->HasTeamAiRole(memberId)) {
      if (type == kAT_Melee) {
        team->EndMeleeAttack(memberId);
      } else if (type == kAT_Projectile) {
        team->EndProjectileAttack(memberId);
      }
      if (clearRole) {
        team->ClearTeamAiRole(memberId);
      }
    }
  }
}

void CScriptTeamAiMgr::Think(float dt, CStateManager& mgr) {
  CEntity::Think(dt, mgr);
  if (ShouldUpdateRoles(dt)) {
    UpdateRoles(mgr);
  }
  PositionTeam(mgr);
  RemoveInvalidTeamActions(mgr);
  mTimeSinceMelee += dt;
  mTimeSinceProjectile += dt;
}

bool CScriptTeamAiMgr::JoinTeam(const CAi& ai, CTeamAiRole::ETeamAiRole roleA,
                                CTeamAiRole::ETeamAiRole roleB, CTeamAiRole::ETeamAiRole roleC) {
  const CTeamAiRole role(ai.GetUniqueId(), roleA, roleB, roleC);
  rstl::vector< CTeamAiRole >::iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  if (found == mRoles.end()) {
    if (mRoles.size() >= mRoles.capacity()) {
      return false;
    }
    mRoles.insert(rstl::lower_bound(mRoles.begin(), mRoles.end(), role), role);
  } else {
    *found = role;
  }

  UpdateTeamCaptain();
  return true;
}

void CScriptTeamAiMgr::QuitTeam(TUniqueId id) {
  EndMeleeAttack(id);
  EndProjectileAttack(id);
  const CTeamAiRole role(id);
  mRoles.erase(rstl::binary_find(mRoles.begin(), mRoles.end(), role));
  UpdateTeamCaptain();
}

const CTeamAiRole* CScriptTeamAiMgr::GetRole(TUniqueId id) const {
  const CTeamAiRole role(id);
  rstl::vector< CTeamAiRole >::const_iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  return found != mRoles.end() ? found.operator->() : nullptr;
}

void CScriptTeamAiMgr::ClearTeamAiRole(TUniqueId id) {
  const CTeamAiRole role(id);
  rstl::vector< CTeamAiRole >::iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  if (found != mRoles.end()) {
    found->mCurRole = CTeamAiRole::kTAR_Initial;
  }
}

bool CScriptTeamAiMgr::HasTeamAiRole(TUniqueId id) const {
  const CTeamAiRole role(id);
  rstl::vector< CTeamAiRole >::const_iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  return found != mRoles.end() && found->HasTeamAiRole();
}

bool CScriptTeamAiMgr::IsPartOfTeam(TUniqueId id) const {
  const CTeamAiRole role(id);
  return rstl::binary_find(mRoles.begin(), mRoles.end(), role) != mRoles.end();
}

bool CScriptTeamAiMgr::IsMeleeAttacking(TUniqueId id) const {
  return rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id) !=
         mMeleeAttackers.end();
}

bool CScriptTeamAiMgr::CanStartMeleeAttack(TUniqueId id) const {
  if (mTimeSinceMelee >= mData.mMeleeTimeInterval &&
      mMeleeAttackers.size() < mData.mMaxMeleeAttackerCount) {
    return true;
  }
  return rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id) !=
         mMeleeAttackers.end();
}

bool CScriptTeamAiMgr::StartMeleeAttack(TUniqueId id) {
  if (mTimeSinceMelee >= mData.mMeleeTimeInterval &&
      mMeleeAttackers.size() < mData.mMaxMeleeAttackerCount && HasTeamAiRole(id)) {
    if (rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id) ==
        mMeleeAttackers.end()) {
      mMeleeAttackers.reserve(mMeleeAttackers.size() + 1);
      mMeleeAttackers.insert(rstl::lower_bound(mMeleeAttackers.begin(), mMeleeAttackers.end(), id),
                             id);
      mTimeSinceMelee = 0.f;
    }
    return true;
  }
  return false;
}

void CScriptTeamAiMgr::EndMeleeAttack(TUniqueId id) {
  rstl::vector< TUniqueId >::iterator found =
      rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id);
  if (found != mMeleeAttackers.end()) {
    mMeleeAttackers.erase(found);
  }
}

bool CScriptTeamAiMgr::CanStartProjectileAttack(TUniqueId id) const {
  if (mTimeSinceProjectile >= mData.mProjectileTimeInterval &&
      mProjectileAttackers.size() < mData.mMaxProjectileAttackerCount) {
    return true;
  }
  return rstl::binary_find(mProjectileAttackers.begin(), mProjectileAttackers.end(), id) !=
         mProjectileAttackers.end();
}

bool CScriptTeamAiMgr::StartProjectileAttack(TUniqueId id) {
  if (mTimeSinceProjectile >= mData.mProjectileTimeInterval &&
      mProjectileAttackers.size() < mData.mMaxProjectileAttackerCount && HasTeamAiRole(id)) {
    if (rstl::binary_find(mProjectileAttackers.begin(), mProjectileAttackers.end(), id) ==
        mProjectileAttackers.end()) {
      mProjectileAttackers.reserve(mProjectileAttackers.size() + 1);
      mProjectileAttackers.insert(
          rstl::lower_bound(mProjectileAttackers.begin(), mProjectileAttackers.end(), id), id);
      mTimeSinceProjectile = 0.f;
    }
    return true;
  }
  return false;
}

void CScriptTeamAiMgr::EndProjectileAttack(TUniqueId id) {
  rstl::vector< TUniqueId >::iterator found =
      rstl::binary_find(mProjectileAttackers.begin(), mProjectileAttackers.end(), id);
  if (found != mProjectileAttackers.end()) {
    mProjectileAttackers.erase(found);
  }
}

bool CScriptTeamAiMgr::ShouldUpdateRoles(float dt) {
  if (mRoles.size() > 0) {
    mTimeDirty += dt;
    if (mTimeDirty >= 1.5f) {
      return true;
    }
    for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end();
         ++it) {
      if (!it->HasTeamAiRole()) {
        return true;
      }
    }
  }
  return false;
}

void CScriptTeamAiMgr::UpdateRoles(CStateManager& mgr) {
  ResetRoles(mgr);
  const CPlayer& player = *mgr.GetPlayer(0);
  const CVector3f aim = player.GetAimPosition(mgr, 0.f);
  const CVector3f position =
      aim + mPlayerForwardProjectionDistance * player.GetTransform().GetForward().AsNormalized();
  rstl::sort(mRoles.begin(), mRoles.end(), CRoleSorter(position, 1));
  AssignRoles(CTeamAiRole::kTAR_Melee, mData.mMeleeCount);
  AssignRoles(CTeamAiRole::kTAR_Projectile, mData.mProjectileCount);
  AssignRoles(CTeamAiRole::kTAR_Unknown, mData.mOtherRoleCount);

  for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (!it->HasTeamAiRole()) {
      it->mCurRole = CTeamAiRole::kTAR_Unassigned;
    }
  }
  rstl::sort(mRoles.begin(), mRoles.end(), CRoleSorter(position, 0));
  mTimeDirty = 0.f;
}

void CScriptTeamAiMgr::ResetRoles(CStateManager& mgr) {
  for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    CTeamAiRole& role = *it;
    role.mCurRole = CTeamAiRole::kTAR_Initial;
    role.mRoleIndex = 0;
    if (const CAi* ai = static_cast< const CAi* >(mgr.GetObjectById(TUniqueId(role.mOwnerId)))) {
      role.mPosition = ai->GetTranslation();
    }
  }
}

void CScriptTeamAiMgr::AssignRoles(CTeamAiRole::ETeamAiRole role, uint count) {
  if (count == 0) {
    return;
  }

  uint roleIndex = 0;
  for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    CTeamAiRole& member = *it;
    if (member.mCurRole == CTeamAiRole::kTAR_Initial && member.AllowsRole(role)) {
      member.mCurRole = role;
      member.mRoleIndex = roleIndex++;
      if (roleIndex == count) {
        return;
      }
    }
  }
}

void CScriptTeamAiMgr::SetPlayerForwardProjectionDistance(float distance) {
  mPlayerForwardProjectionDistance = distance;
}

void CScriptTeamAiMgr::PositionTeam(CStateManager& mgr) {
  const CPlayer& player = *mgr.GetPlayer(0);
  const CVector3f aim = player.GetAimPosition(mgr, 0.f);
  const CVector3f position =
      aim + mPlayerForwardProjectionDistance * player.GetTransform().GetForward().AsNormalized();
  // Retail tests `cmpwi r0,1 / beq SpacingSort / bge loop / b loop` (main.elf
  // 0x801739F4-0x80173A04): only mode == 1 reaches SpacingSort, and mode >= 2 reaches the
  // per-member GetOrigin loop. `>= 1` emits `cmplwi r0,1 / blt loop` and routes every mode >= 1 to
  // SpacingSort, which is a different behaviour.
  if (mData.mPositionMode == 1) {
    SpacingSort(mgr, position);
  } else {
    for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
      CTeamAiRole& role = *it;
      if (CPatterned* ai =
              TCastToPtr< CPatterned >(mgr.GetObjectByIdFromListAll(TUniqueId(role.mOwnerId)))) {
        role.mPosition = ai->GetOrigin(mgr, role, position);
      }
    }
  }
}

void CScriptTeamAiMgr::SpacingSort(CStateManager& mgr, const CVector3f& position) {
  rstl::sort(mRoles.begin(), mRoles.end(), CRoleSorter(position, 2));

  float tierStagger = 4.5f;
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end();
       ++it) {
    if (const CPatterned* ai =
            TCastToPtr< CPatterned >(mgr.GetObjectByIdFromListAll(TUniqueId(it->mOwnerId)))) {
      const CAABox& bounds = ai->GetBaseBoundingBox();
      const float length = (bounds.GetMaxPoint().GetY() - bounds.GetMinPoint().GetY()) * 1.5f;
      if (length > tierStagger) {
        tierStagger = length;
      }
    }
  }

  float tierDistance = tierStagger;
  int tierSize = 0;
  int maxTierSize = 3;
  for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    CTeamAiRole& role = *it;
    if (const CPatterned* ai =
            TCastToPtr< CPatterned >(mgr.GetObjectByIdFromListAll(TUniqueId(role.mOwnerId)))) {
      CVector3f delta = ai->GetTranslation() - position;
      delta.SetZ(0.f);
      CVector3f newPosition =
          position + tierDistance * (delta.CanBeNormalized() ? delta.AsNormalized()
                                                             : ai->GetTransform().GetForward());
      newPosition.SetZ(ai->GetTranslation().GetZ());
      role.mPosition = newPosition;
      if (++tierSize > maxTierSize) {
        tierDistance += tierStagger;
        tierSize = 0;
        ++maxTierSize;
      }
    }
  }
  rstl::sort(mRoles.begin(), mRoles.end(), CRoleSorter(position, 0));
}

void CScriptTeamAiMgr::UpdateTeamCaptain() {
  int priority = INT_MIN;
  mTeamCaptainId = kInvalidUniqueId;
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end();
       ++it) {
    if (it->mCaptainPriority > priority) {
      priority = it->mCaptainPriority;
      mTeamCaptainId = it->mOwnerId;
    }
  }
}

bool CScriptTeamAiMgr::IsTeamMemberInRange(const CStateManager& mgr, const CActor& actor,
                                           float range) const {
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (it->mOwnerId != actor.GetUniqueId()) {
      if (const CActor* member = TCastToConstPtr< CActor >(mgr.GetObjectById(it->mOwnerId))) {
        if ((actor.GetTranslation() - member->GetTranslation()).MagSquared() < range * range) {
          return true;
        }
      }
    }
  }
  return false;
}

TUniqueId CScriptTeamAiMgr::FindBestIndividualAttackTarget(CStateManager& mgr, const CAi& ai) {
  int targetCounts[4] = {0, 0, 0, 0};
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (it->mOwnerId != ai.GetUniqueId()) {
      for (int player = 0; player < mgr.GetNumPlayers(); ++player) {
        if (it->mTargetId == mgr.GetPlayer(player)->GetUniqueId()) {
          ++targetCounts[player];
          break;
        }
      }
    }
  }

  TUniqueId target = kInvalidUniqueId;
  float bestScore = 1000.f;
  for (int i = 0; i < mgr.GetNumPlayers(); ++i) {
    if (mgr.GetPlayerState(i)->IsPlayerAlive()) {
      const float penalty = 100.f * targetCounts[i];
      if (penalty < bestScore) {
        const CPlayer& player = *mgr.GetPlayer(i);
        const float score = penalty + (player.GetTranslation() - ai.GetTranslation()).Magnitude();
        if (score < bestScore) {
          target = player.GetUniqueId();
          bestScore = score;
        }
      }
    }
  }
  return target;
}

TUniqueId CScriptTeamAiMgr::ChoosePlayer(const CStateManager& mgr, const CActor& actor) {
  TUniqueId target = kInvalidUniqueId;
  float bestScore = FLT_MAX;
  const CVector3f forward = actor.GetTransform().GetForward();
  const CVector3f actorPosition = actor.GetTranslation();
  for (int i = 0; i < mgr.GetNumPlayers(); ++i) {
    const CPlayer& player = *mgr.GetPlayer(i);
    const CVector3f delta = player.GetTranslation() - actorPosition;
    const float distanceSquared = delta.MagSquared();
    if (distanceSquared < bestScore) {
      const float score =
          distanceSquared * CVector3f::GetAngleDiff(delta, forward) + distanceSquared;
      if (score < bestScore) {
        bestScore = score;
        target = player.GetUniqueId();
      }
    }
  }
  return target;
}

void CScriptTeamAiMgr::SetMemberTargetId(TUniqueId memberId, TUniqueId targetId) {
  const CTeamAiRole role(memberId);
  rstl::vector< CTeamAiRole >::iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  if (found != mRoles.end()) {
    found->mTargetId = targetId;
  }
}

CTeamAiRole::ETeamAiRole CScriptTeamAiMgr::GetTeamRole(TUniqueId memberId) const {
  const CTeamAiRole role(memberId);
  rstl::vector< CTeamAiRole >::const_iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  return found != mRoles.end() ? found->mCurRole : CTeamAiRole::kTAR_Initial;
}

void CScriptTeamAiMgr::NotifyWasHit() { mWasHit = true; }

bool CScriptTeamAiMgr::GetWasHit() const { return mWasHit; }

void CScriptTeamAiMgr::StartTeamAction(TUniqueId id, ETeamAction action) {
  if (IsPerformingTeamAction(id, action) != true) {
    if (mTeamActions.size() == mTeamActions.capacity()) {
      mTeamActions.reserve(mTeamActions.size() + 4);
    }
    mTeamActions.push_back_unsafe(STeamAction(id, action));
  }
}

void CScriptTeamAiMgr::EndTeamAction(TUniqueId id, ETeamAction action) {
  for (rstl::vector< STeamAction >::iterator it = mTeamActions.begin(); it != mTeamActions.end();
       ++it) {
    if (it->mOwnerId == id && it->mAction == action) {
      mTeamActions.erase(it);
      return;
    }
  }
}

int CScriptTeamAiMgr::GetTeamActionCount(ETeamAction action) const {
  int count = 0;
  for (rstl::vector< STeamAction >::const_iterator it = mTeamActions.begin();
       it != mTeamActions.end(); ++it) {
    if (action == it->mAction) {
      ++count;
    }
  }
  return count;
}

// Guessed name
bool CScriptTeamAiMgr::IsPerformingTeamAction(TUniqueId id, ETeamAction action) const {
  for (rstl::vector< STeamAction >::const_iterator it = mTeamActions.begin();
       it != mTeamActions.end(); ++it) {
    if (it->mOwnerId == id && action == it->mAction) {
      return true;
    }
  }
  return false;
}

// Guessed name
void CScriptTeamAiMgr::RemoveInvalidTeamActions(CStateManager& mgr) {
  bool removed;
  do {
    removed = false;
    for (rstl::vector< STeamAction >::iterator it = mTeamActions.begin(); it != mTeamActions.end();
         ++it) {
      if (mgr.GetObjectById(it->mOwnerId) == nullptr) {
        mTeamActions.erase(it);
        removed = true;
        break;
      }
    }
  } while (removed);
}

bool CScriptTeamAiMgr::AnyMembersInCircle(CStateManager& mgr, const CVector3f& position,
                                          float radius, TUniqueId excludeId) const {
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (it->mOwnerId != excludeId) {
      const CActor* actor = TCastToPtr< CActor >(mgr.GetObjectByIdFromListAll(it->mOwnerId));
      if ((actor->GetTranslation() - position).MagSquared() < radius * radius) {
        return true;
      }
    }
  }
  return false;
}

CVector3f CScriptTeamAiMgr::GetCenter(CStateManager& mgr) const {
  CVector3f center = CVector3f::Zero();
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (const CAi* ai = static_cast< const CAi* >(mgr.GetObjectById(TUniqueId(it->mOwnerId)))) {
      center += ai->GetTranslation();
    }
  }
  const float scale = 1.f / mRoles.size();
  center *= scale;
  return center;
}

TUniqueId CScriptTeamAiMgr::TouchingAnyTeammates(CStateManager& mgr, TUniqueId id,
                                                 float margin) const {
  const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(id));
  if (actor == nullptr) {
    return kInvalidUniqueId;
  }

  const rstl::optional_object< CAABox > bounds = actor->GetTouchBounds();
  if (!bounds) {
    return kInvalidUniqueId;
  }
  const CVector3f expansion = margin * CVector3f::One();
  const CAABox expanded(bounds->GetMinPoint() - expansion, bounds->GetMaxPoint() + expansion);
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (it->mOwnerId != id) {
      if (const CActor* member = TCastToConstPtr< CActor >(mgr.GetObjectById(it->mOwnerId))) {
        if (member->GetActive()) {
          const rstl::optional_object< CAABox > memberBounds = member->GetTouchBounds();
          if (memberBounds && expanded.DoBoundsOverlap(*memberBounds)) {
            return member->GetUniqueId();
          }
        }
      }
    }
  }
  return kInvalidUniqueId;
}
