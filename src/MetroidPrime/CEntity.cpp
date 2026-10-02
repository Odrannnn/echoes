#include "MetroidPrime/CEntity.hpp"

#include "MetroidPrime/CStateManager.hpp"

// Retail's object ends with two functions nothing in this file calls:
// `rstl::vector<SConnection>::reserve` (152 B) and the `rstl::uninitialized_copy` it calls
// (60 B), between `__distance<...>` and `__sinit_CEntity_cpp`. They are mwccceppc's out-of-line
// copies for the instantiation, and this object comes first in link order, so mwldeppc keeps
// these two and the rest of the DOL binds to them - the one caller in the DOL is the
// connection-list reader at 0x80234028. Nothing here odr-uses `reserve`:
// `CEntityInfo`'s copy constructor allocates straight through `rmemory_allocator::allocate`, so
// the template is never instantiated and both functions come out at 0.00%. Spelling the member
// out is how this repo reproduces such a copy - see
// `src/MetroidPrime/Player/CStaticInterference.cpp` and `src/MetroidPrime/CGameHintInfo.cpp`,
// which do the same for their own vectors - and the body is the one in `rstl/vector.hpp`,
// unchanged.
template <>
void rstl::vector< SConnection >::reserve(int newSize) {
  if (newSize <= mCapacity) {
    return;
  }

  SConnection* newData;
  mAllocator.allocate(newData, newSize);
  uninitialized_copy(begin(), end(), newData);
  destroy(mItems, mItems + mCount);
  mAllocator.deallocate(mItems);
  mItems = newData;
  mCapacity = newSize;
}

rstl::vector< SConnection > CEntity::NullConnectionList;

CEntityInfo CEntity::NullEntityInfo =
    CEntityInfo(kInvalidAreaId, NullConnectionList, true, kInvalidEditorId);

CEntityInfo::CEntityInfo(TAreaId aid, const rstl::vector< SConnection >& connections, bool isActive,
                         TEditorId eid)
: mAreaId(aid)
, mConnections(connections)
, mEditorId(eid)
, mActive(isActive)
, mUpdateWhileOccluded(true)
, mUpdateDuringCinematicSkip(true) {}

CEntity::CEntity(TUniqueId id, const CEntityInfo& info, const rstl::string& name, uint castFlags)
: mAreaId(info.GetAreaId())
, mUniqueId(id)
, mEditorId(info.GetEditorId())
, mConnections(info.GetConnectionList())
, mActive(info.GetActive())
, mNotInArea(mAreaId == kInvalidAreaId)
, mCastFlags(castFlags)
, mUpdateWhileOccluded(info.GetUpdateWhileOccluded())
, mUpdateDuringCinematicSkip(info.GetUpdateDuringCinematicSkip()) {}

CEntity::~CEntity() {}

void CEntity::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_Activate:
    if (!mActive) {
      SetActive(true);
      SendScriptMsgs(kSS_Active, mgr);
    }
    break;
  case kSM_Deactivate:
    if (mActive) {
      SetActive(false);
      SendScriptMsgs(kSS_Inactive, mgr);
    }
    break;
  case kSM_ToggleActive: {
    EScriptObjectMessage next = mActive ? kSM_Deactivate : kSM_Activate;
    CScriptMsg newMsg(msg.GetUnk(), msg.GetId(), msg.GetOriginator(), next, msg.GetState());
    AcceptScriptMsg(mgr, newMsg);
    break;
  }
  }
}

void CEntity::SendScriptMsgs(EScriptObjectState state, CStateManager& mgr, TUniqueId id,
                             EScriptObjectMessage skipMsg) {
  rstl::vector< SConnection >::const_iterator it = mConnections.begin();
  for (; it != mConnections.end(); ++it) {
    if (it->state == state && it->msg != skipMsg) {
      CStateManager::TIdListResult search = mgr.GetIdListForScript(it->objId);
      CStateManager::TIdList::const_iterator current = search.first;
      CStateManager::TIdList::const_iterator end = search.second;
      // `it->state` and the `state` argument are equal here (that is the test just above), and
      // retail passes the argument: its fifth slot is the register the parameter came in, with
      // no reload of `it->state`.
      while (current != end) {
        mgr.SendScriptMsg(CScriptMsg(GetUniqueId(), current->second, id, it->msg, state));
        ++current;
      }
    }
  }
}

void CEntity::PreThink(float dt, CStateManager& mgr) {}

void CEntity::Think(float dt, CStateManager& mgr) {}

// `SendActive` is defined above `SetActive`, the opposite of the header's order: mwcceppc emits
// definitions in reverse source order, and retail's object has `SetActive` (0x8b0, 16 B) before
// `SendActive` (0x8c0, 128 B).
void CEntity::SendActive(CStateManager& mgr, bool active) {
  if (active != GetActive()) {
    mgr.SendScriptMsg(this, GetUniqueId(), active ? kSM_Activate : kSM_Deactivate,
                      kInvalidUniqueId);
  }
}

void CEntity::SetActive(const bool active) { mActive = active; }

TAreaId CEntity::GetAreaIdForPersistence() const { return mNotInArea ? kInvalidAreaId : mAreaId; }

TUniqueId CEntity::FindConnectedObject(const CStateManager& mgr, EScriptObjectState state,
                                       EScriptObjectMessage msg) const {
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        return ids.first->second;
      }
    }
  }
  return kInvalidUniqueId;
}

TUniqueId CEntity::FindConnectedObject_if(const CStateManager& mgr, EScriptObjectState state,
                                          EScriptObjectMessage msg,
                                          const CValidEntityPredicate& predicate) const {
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        if (predicate.IsValid(mgr, ids.first->second)) {
          return ids.first->second;
        }
      }
    }
  }
  return kInvalidUniqueId;
}

rstl::vector< TUniqueId > CEntity::FindConnectedObjects(const CStateManager& mgr,
                                                        EScriptObjectState state,
                                                        EScriptObjectMessage msg) const {
  rstl::vector< TUniqueId > result;
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        result.reserve(result.size() + rstl::distance(ids.first, ids.second));
        for (CStateManager::TIdList::const_iterator current = ids.first; current != ids.second;
             ++current) {
          result.data()[result.mCount++] = current->second;
        }
      }
    }
  }
  return result;
}

rstl::vector< TUniqueId >
CEntity::FindConnectedObjects_if(const CStateManager& mgr, EScriptObjectState state,
                                 EScriptObjectMessage msg,
                                 const CValidEntityPredicate& predicate) const {
  rstl::vector< TUniqueId > result;
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        result.reserve(result.size() + rstl::distance(ids.first, ids.second));
        for (CStateManager::TIdList::const_iterator current = ids.first; current != ids.second;
             ++current) {
          if (predicate.IsValid(mgr, current->second)) {
            result.data()[result.mCount++] = current->second;
          }
        }
      }
    }
  }
  return result;
}

TUniqueId CEntity::CheckConnectedObject(const CStateManager& mgr, EScriptObjectState state,
                                        EScriptObjectMessage msg) const {
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        return ids.first->second;
      }
    }
  }
  return kInvalidUniqueId;
}

TUniqueId CEntity::CheckConnectedObject_if(const CStateManager& mgr, EScriptObjectState state,
                                           EScriptObjectMessage msg,
                                           const CValidEntityPredicate& predicate) const {
  for (rstl::vector< SConnection >::const_iterator it = mConnections.begin();
       it != mConnections.end(); ++it) {
    if ((state == kSS_InvalidState || state == it->state) && (msg == kSM_None || msg == it->msg)) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
      if (!(ids.first == ids.second)) {
        if (predicate.IsValid(mgr, ids.first->second)) {
          return ids.first->second;
        }
      }
    }
  }
  return kInvalidUniqueId;
}

CValidEntityPredicate::~CValidEntityPredicate() {}

bool CValidEntityPredicate::IsValid(const CStateManager&, TUniqueId) const { return true; }
