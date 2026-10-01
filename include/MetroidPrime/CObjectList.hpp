#ifndef _COBJECTLIST
#define _COBJECTLIST

#include "TGameTypes.hpp"

class CEntity;

#define kMaxObjects 1024

enum EGameObjectList {
  kOL_Invalid = -1,
  kOL_All,
  kOL_Actor,
  kOL_PhysicsActor,
  kOL_GameCamera,
  kOL_GameLight,
  kOL_ListeningAi,
  kOL_AiWaypoint,
  kOL_PlatformAndDoor,
  kOL_Unk,
  // Measured, not inferred: retail reads this one as `*(CObjectList**)(CStateManager + 0x848)`,
  // which is `m_objectLists[7]` - the element pointer sits +8 from the vector's data base
  // (`auto_ptr<T>`'s pointer is its second word). Its occupants are read off it as
  // CScriptTrigger (`CCameraManager::UpdateCameraTriggers` 0x801AC4C4,
  // `TransferCameraTriggers` 0x801AC638, `UpdateCameraTriggerOccupancy` 0x801AC588) and
  // CScriptWater (`fn_8000BA60` 0x8000BA60) - both `CActor` descendants. `kOL_All` (index 0,
  // +0x810) is confirmed independently by `CStateManager::ObjectById`, which uses it.
  // The retail enumerator's own name is unknown; the names above are Prime's, and `kOL_Actor`
  // sits at a different measured index, so nothing above was renumbered to make this work.
  kOL_ScriptActors = 7,
};

class CObjectList {
  struct SObjectListEntry {
    CEntity* mEntity;
    short mNext;
    short mPrev;
    SObjectListEntry() : mEntity(nullptr), mNext(-1), mPrev(-1) {}
  };

public:
  CObjectList(EGameObjectList list, bool flag);
  virtual uchar IsQualified(const CEntity& entity);

  // Echoes names below are inferred from Prime and their implementations.
  void Clear();
  void AddObject(CEntity& entity);
  void AddObjectIfAbsent(CEntity& entity);
  void RemoveObject(TUniqueId uid);
  CEntity* GetObjectById(TUniqueId uid);
  const CEntity* GetObjectById(TUniqueId uid) const;
  CEntity* operator[](int idx);
  const CEntity* operator[](int idx) const;

  int size() const { return mCount; }
  int GetFirstObjectIndex() const { return mFirstId; }
  int GetNextObjectIndex(int idx) const {
    if (idx != -1) {
      return mObjects[idx].mNext;
    } else {
      return -1;
    }
  }

private:
  SObjectListEntry mObjects[kMaxObjects];
  EGameObjectList mListType;
  short mFirstId;
  short mCount;
  bool x200c_;
};
CHECK_SIZEOF(CObjectList, 0x2010)

#endif // _COBJECTLIST
