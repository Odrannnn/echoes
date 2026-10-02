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
    if (mRoles.size() < mRoles.capacity()) {
      rstl::vector< CTeamAiRole >::iterator pos =
          rstl::lower_bound(mRoles.begin(), mRoles.end(), role);
      mRoles.insert(pos, role);
    } else {
      return false;
    }
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
  // Named local, as in `IsPartOfTeam` and `JoinTeam`: the search result in a local is what puts
  // the three argument slots in retail's order. 99.88% -> 100%.
  rstl::vector< CTeamAiRole >::iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  mRoles.erase(found);
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
  // Spelled as a positive `if` with the `false` arm last, not `found != mRoles.end() &&
  // found->HasTeamAiRole()`. Retail (main.elf 0x80172E50) puts the not-found `li r3,0` after the
  // `HasTeamAiRole` chain and branches to it; the `&&` form hoists the `li r3,0` above the test
  // and reaches only 93.34%.
  if (found != mRoles.end()) {
    return found->HasTeamAiRole();
  }
  return false;
}

// Superseded 2026-10-02: the rule below ("`end()` is the LEFT operand") was the best available
// spelling then and is superseded by the named-local form used in every function below - putting
// the search result in a local makes the `end()` be evaluated after the call, which is what
// actually forces mwcceppc to re-read `mCount`/`mItems` and rebuild the end pointer
// (`lwz r0,76(r31) / lwz r3,84(r31) / mulli r0,r0,44 / add r0,r3,r0`). Kept for the reasoning,
// not as guidance: with the named local the operand order is `found != mRoles.end()`.
// Written inline as `end() != binary_find(...)` the old spelling measured 92.27% here and 79.78%
// in `IsMeleeAttacking`; the named local reaches 100% in both.
bool CScriptTeamAiMgr::IsPartOfTeam(TUniqueId id) const {
  const CTeamAiRole role(id);
  // A *named* local, and `end()` on its right. Written inline as `end() != binary_find(...)` the
  // tail's two values land in the opposite registers (99.24%); with the result in a local the
  // `end()` is evaluated after the call - which is what makes mwcceppc re-read
  // `mCount`/`mItems` and rebuild the pointer instead of reusing the stored copy - and the
  // registers fall the right way round.
  rstl::vector< CTeamAiRole >::const_iterator found =
      rstl::binary_find(mRoles.begin(), mRoles.end(), role);
  return found != mRoles.end();
}

bool CScriptTeamAiMgr::IsMeleeAttacking(TUniqueId id) const {
  // The named-local spelling of `IsPartOfTeam`: 99.12% -> 100%.
  rstl::vector< TUniqueId >::const_iterator found =
      rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id);
  return found != mMeleeAttackers.end();
}

bool CScriptTeamAiMgr::CanStartMeleeAttack(TUniqueId id) const {
  if (mTimeSinceMelee >= mData.mMeleeTimeInterval &&
      mMeleeAttackers.size() < mData.mMaxMeleeAttackerCount) {
    return true;
  }
  // A named local for the search result, `end()` on its right: see `IsPartOfTeam`. It is what
  // makes the `end()` be evaluated after the call, so the tail re-reads `mCount`/`mItems` and
  // rebuilds the pointer. 99.21% -> 100%.
  rstl::vector< TUniqueId >::const_iterator found =
      rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id);
  if (found != mMeleeAttackers.end()) {
    return true;
  }
  return false;
}

bool CScriptTeamAiMgr::StartMeleeAttack(TUniqueId id) {
  if (mTimeSinceMelee >= mData.mMeleeTimeInterval &&
      mMeleeAttackers.size() < mData.mMaxMeleeAttackerCount && HasTeamAiRole(id)) {
    rstl::vector< TUniqueId >::const_iterator found =
        rstl::binary_find(mMeleeAttackers.begin(), mMeleeAttackers.end(), id);
    if (found == mMeleeAttackers.end()) {
      mMeleeAttackers.reserve(mMeleeAttackers.size() + 1);
      rstl::vector< TUniqueId >::iterator pos =
          rstl::lower_bound(mMeleeAttackers.begin(), mMeleeAttackers.end(), id);
      mMeleeAttackers.insert(pos, id);
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
  rstl::vector< TUniqueId >::const_iterator found =
      rstl::binary_find(mProjectileAttackers.begin(), mProjectileAttackers.end(), id);
  if (found != mProjectileAttackers.end()) {
    return true;
  }
  return false;
}

bool CScriptTeamAiMgr::StartProjectileAttack(TUniqueId id) {
  if (mTimeSinceProjectile >= mData.mProjectileTimeInterval &&
      mProjectileAttackers.size() < mData.mMaxProjectileAttackerCount && HasTeamAiRole(id)) {
    rstl::vector< TUniqueId >::const_iterator found =
        rstl::binary_find(mProjectileAttackers.begin(), mProjectileAttackers.end(), id);
    if (found == mProjectileAttackers.end()) {
      mProjectileAttackers.reserve(mProjectileAttackers.size() + 1);
      rstl::vector< TUniqueId >::iterator pos =
          rstl::lower_bound(mProjectileAttackers.begin(), mProjectileAttackers.end(), id);
      mProjectileAttackers.insert(pos, id);
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
  // `mgr.GetPlayer(0)` is written out twice rather than bound to a `const CPlayer&`. Retail
  // re-reads `mgr->mPlayers[0]` (`lwz r5,5372(r30)`) at both use sites, so it holds no player
  // reference across the aim call; binding one costs a third callee-saved register and an extra
  // `stw r29,148(r1)` in the prologue (93.65% -> 100%).
  const CVector3f aim = mgr.GetPlayer(0)->GetAimPosition(mgr, 0.f);
  const CVector3f position = aim + mPlayerForwardProjectionDistance *
                                  mgr.GetPlayer(0)->GetTransform().GetForward().AsNormalized();
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
  const CVector3f aim = mgr.GetPlayer(0)->GetAimPosition(mgr, 0.f);
  const CVector3f position = aim + mPlayerForwardProjectionDistance *
                                  mgr.GetPlayer(0)->GetTransform().GetForward().AsNormalized();
  // The routing is exactly `if (mode == 1) SpacingSort(...); else <member loop>` - `case 0:` and
  // `default:` are the same code, so no value but 1 reaches `SpacingSort`. Retail tests
  // `cmpwi r0,1 / beq SpacingSort / bge loop / b loop` (main.elf 0x801739F4-0x80173A04), i.e. all
  // three outcomes branch and `SpacingSort` sits out of line; the extra `case 0:` label is what
  // makes mwcceppc build that three-way dispatch instead of the two-branch `beq / b` that a bare
  // `case 1:` + `default:` gives (97.98% -> 99.04% -> 100%). DO NOT "simplify" this back to
  // `>= 1`: that emits `cmplwi r0,1 / blt loop` and sends every mode >= 1 to `SpacingSort`, which
  // is a different behaviour (a reviewer rejected it on 2026-10-01). The `static_cast<int>` is the
  // signedness of the compare, not the predicate: `mPositionMode` is a `uint`, and a `uint` left
  // operand makes mwcceppc emit the unsigned `cmplwi r0,1` where retail has the signed `cmpwi r0,1`
  // (90.99% -> 97.98%).
  switch (static_cast<int>(mData.mPositionMode)) {
  case 1:
    SpacingSort(mgr, position);
    break;
  case 0:
  default:
    for (rstl::vector< CTeamAiRole >::iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
      CTeamAiRole& role = *it;
      if (CPatterned* ai =
              TCastToPtr< CPatterned >(mgr.GetObjectByIdFromListAll(TUniqueId(role.mOwnerId)))) {
        role.mPosition = ai->GetOrigin(mgr, role, position);
      }
    }
    break;
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
      // The `position + tierDistance * ...` product is inside both arms of the `?:`, not hoisted
      // into a `CVector3f` temporary. Retail computes and stores the whole product separately in
      // each arm (two identical `fmuls`/`fadds`/`stfs` runs); with a shared temporary the ternary
      // result has to be materialised in the frame and the multiply emitted once after the join
      // (79.49% -> 100%).
      CVector3f newPosition = delta.CanBeNormalized()
          ? position + tierDistance * delta.AsNormalized()
          : position + tierDistance * ai->GetTransform().GetForward();
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
    // `TUniqueId ownerId(it->mOwnerId)`, used by both the test and the lookup. The explicit copy
    // is what makes retail's `clrlwi r3,r4,16` (the 16-bit narrowing of the compared value) and its
    // second, dead, `sth r4,12(r1)` appear; with `it->mOwnerId` read twice mwcceppc folds the
    // compare to a bare `cmplw` and the whole rest of the frame shifts down 4 bytes
    // (96.72% -> 100%).
    const TUniqueId ownerId(it->mOwnerId);
    if (ownerId != actor.GetUniqueId()) {
      if (const CActor* member = TCastToConstPtr< CActor >(mgr.GetObjectById(ownerId))) {
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
      // The bound, not the counter, is what carries the signedness: `static_cast<uint>` here is
      // what makes mwcceppc emit the unsigned `cmplwi`/`cmplw` retail uses. Making the counter a
      // `uint` instead gets the encoding but throws the loop back to an indexed `lwzx` form.
      for (int player = 0; player < static_cast<uint>(mgr.GetNumPlayers()); ++player) {
        if (it->mTargetId == mgr.GetPlayer(player)->GetUniqueId()) {
          ++targetCounts[player];
          break;
        }
      }
    }
  }

  TUniqueId target = kInvalidUniqueId;
  float bestScore = 1000.f;
  for (int i = 0; i < static_cast<uint>(mgr.GetNumPlayers()); ++i) {
    if (mgr.GetPlayerState(i)->IsPlayerAlive()) {
      const float penalty = 100.f * targetCounts[i];
      if (penalty < bestScore) {
        const float score =
            penalty + (mgr.GetPlayer(i)->GetTranslation() - ai.GetTranslation()).Magnitude();
        if (score < bestScore) {
          target = mgr.GetPlayer(i)->GetUniqueId();
          bestScore = score;
        }
      }
    }
  }
  return target;
}

TUniqueId CScriptTeamAiMgr::ChoosePlayer(const CStateManager& mgr, const CActor& actor) {
  // Declaration order matters: retail loads the `FLT_MAX` constant (`lfs f27,lbl_8041C828`)
  // before the `kInvalidUniqueId` SDA21 pair, so `bestScore` is declared first. Swapping them
  // costs the two loads their order and the function 1.6% (97.63% -> 99.21%).
  float bestScore = FLT_MAX;
  TUniqueId target = kInvalidUniqueId;
  const CVector3f forward = actor.GetTransform().GetForward();
  const CVector3f actorPosition = actor.GetTranslation();
  // See `FindBestIndividualAttackTarget`: the unsigned cast is on the bound, not the counter.
  for (int i = 0; i < static_cast<uint>(mgr.GetNumPlayers()); ++i) {
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
    // `== true`, not a bare `removed`. Retail's do-while tail is
    // `clrlwi r0,r31,24 / cmplwi r0,1 / beq` (main.elf 0x80172248-0x8017224C): it materialises
    // the byte and compares it against 1. A bare `while (removed)` lets mwcceppc fold the test
    // into the producing instruction's record bit and emits `clrlwi. / bne` instead, which is
    // one instruction shorter and leaves the function at 97.71%.
  } while (removed == true);
}

bool CScriptTeamAiMgr::AnyMembersInCircle(CStateManager& mgr, const CVector3f& position,
                                          float radius, TUniqueId excludeId) const {
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    // Both halves of this are load-bearing, see `IsTeamMemberInRange` for the `ownerId` copy
    // (83.58% -> 96.04%) and below for the named `delta` (96.04% -> 100%).
    const TUniqueId ownerId(it->mOwnerId);
    if (ownerId != excludeId) {
      const CActor* actor = TCastToPtr< CActor >(mgr.GetObjectByIdFromListAll(ownerId));
      // The difference is a named local, not a temporary inside the call. Retail contracts the
      // magnitude into two `fmadds` and spills nothing; with `(a - b).MagSquared()` the temporary
      // is spilled as three `stfs` and the adds are left unfused.
      const CVector3f delta = actor->GetTranslation() - position;
      if (delta.MagSquared() < radius * radius) {
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
  // Named corners, not two temporaries in the constructor call (88.46% -> 88.88%). Retail still
  // keeps two more `CVector3f` copies of `expansion` in the frame than we do, which is the whole
  // of the remaining 11%.
  const CVector3f minPoint = bounds->GetMinPoint() - expansion;
  const CVector3f maxPoint = bounds->GetMaxPoint() + expansion;
  const CAABox expanded(minPoint, maxPoint);
  for (rstl::vector< CTeamAiRole >::const_iterator it = mRoles.begin(); it != mRoles.end(); ++it) {
    if (it->mOwnerId != id) {
      if (const CActor* member = TCastToConstPtr< CActor >(mgr.GetObjectById(it->mOwnerId))) {
        if (member->GetActive()) {
          const rstl::optional_object< CAABox > memberBounds = member->GetTouchBounds();
          // `== true` for the same reason as `RemoveInvalidTeamActions` below: retail tests the
          // returned byte against 1 rather than folding the test into its record bit.
          if (memberBounds && expanded.DoBoundsOverlap(*memberBounds) == true) {
            return member->GetUniqueId();
          }
        }
      }
    }
  }
  return kInvalidUniqueId;
}
