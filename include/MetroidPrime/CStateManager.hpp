#ifndef _CSTATEMANAGER
#define _CSTATEMANAGER

extern const int gkPVSEnabled;

#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CFilteredObjectList.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CScriptObjectLoaderHelper.hpp"
#include "MetroidPrime/Enemies/EListenNoiseType.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "TGameTypes.hpp"
#include "MetroidPrime/CScriptObjectLoaderHelper.hpp"
#include "MetroidPrime/Weapons/WeaponTypes.hpp"

#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/bit_vector.hpp"
#include "rstl/list.hpp"
#include "rstl/map.hpp"
#include "rstl/pair.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/reserved_vector.hpp"
#include "rstl/single_ptr.hpp"

class CWorld;
class CPortalTransition;
class CArchitectureQueue;
class CEnvFxManager;
class CEntity;
class CActor;
class CMaterialFilter;
class CRayCastResult;
class CScriptMailbox;
class CMapWorldInfo;
class CPlayerState;
class CWorldTransManager;
class CPlayer;
class CCameraManager;
class CRumbleManager;
class CSaveGameScreen;
class CActorModelParticles;
class CRelayTracker;
class CWorldLayerState;
class CStateManagerContainer;
class CInputStream;
class CStateManager;

class CPlane;
class CPortalTransition;
namespace SL {
class CSortedListManager;
}
class CWeaponMgr;
class CFluidPlaneManager;
class CDamageInfo;
class CAABox;

typedef rstl::bit_vector<> MapWorldInfoAreas;

enum EStateManagerTransition {
  kSMT_InGame,
  kSMT_MapScreen,
  kSMT_PauseGame,
  kSMT_LogBook,
  kSMT_SaveGame,
  kSMT_Unk,
  kSMT_MessageScreen
};

class CStateManager {
  // A 192-entry ring buffer, not a vector. Retail's three functions index with `msgs[index]` and
  // advance `index = (index + 1) % 192` through an **unsigned** multiply-high sequence (`lis
  // r5,-21845` = 0xAAAAAAAA, `mulhwu`, `srwi 7`, `mulli 192`), which a signed `%` will not
  // produce - so the two cursors are `uint`. The sizes and offsets are measured, not assumed:
  // `sizeof(CScriptMsg)` is 0x10 with `m_unk`/`m_originator`/`m_id` as two-byte `TUniqueId`s at
  // 0/2/4 and `m_msg`/`m_state` as four-byte enums at 8/0xC, so the array is 0xC00 and the
  // cursors land on retail's 0xC00 and 0xC04 (mwcceppc probe, 2026-09-26).
  struct ScriptMsgArray {
    CScriptMsg msgs[192];
    uint lastIndex;  //!< 0xC00, the write cursor
    //! 0xC04, the read cursor. `mutable` because retail's `fn_8019E6BC` is a **const** member
    //! function (`...14ScriptMsgArrayCFv` in `config/G2ME01/symbols.txt`) that advances it.
    mutable uint otherIndex;

    void Append(const CScriptMsg& msg);
    int fn_8019E69C();
    // **By value, not by reference.** Retail builds the popped message into the caller's return
    // slot in `r3` - `sth`/`sth`/`sth`/`stw`/`stw` straight to `0(r3)`..`12(r3)` - so a reference
    // return cannot compile to it. `CStateManager::fn_8003BE54`'s `CScriptMsg msg = ...` still
    // reads the same.
    CScriptMsg fn_8019E6BC() const;

    bool empty() const { return lastIndex == otherIndex; }
  };

public:
  typedef rstl::map< TEditorId, TUniqueId > TIdList;
  typedef rstl::pair< TIdList::const_iterator, TIdList::const_iterator > TIdListResult;

  // Guessed phase names, derived from world initialization.
  enum EInitPhase { kIP_LoadAudioGroups, kIP_LoadWorld, kIP_LoadFirstArea, kIP_Done };

  CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&, const rstl::ncrc_ptr< CMapWorldInfo >&,
                const rstl::ncrc_ptr< CPlayerState >&, const rstl::ncrc_ptr< CWorldTransManager >&);
  ~CStateManager();

  TUniqueId AllocateUniqueId();
  CScriptObjectLoaderHelper& ScriptObjectLoaderHelper();
  uint MaskUIdNumPlayers(TUniqueId id) const;
  void SetBossParams(TUniqueId bossId, float maxEnergy, uint stringIdx);
  void SetIsDarkWorld(bool);
  bool GetIsDarkWorld() const { return m_isDarkWorld; }
  void SetMapTeleportWorldId(CAssetId id) { mMapTeleportWorldId = id; } // Guessed name
  void DisplayAlertAboutOutOfAmmo(const CPlayer&, CPlayerState::EItemType) const;
  rstl::pair< int, int > CalculateScanCompletionRate() const;

  //
  void ShowPausedHUDMemo(CAssetId strg, float time);
  void QueueMessage(int frameCount, CAssetId msg, float f1);
  int GetHUDMessageFrameCount() const { return mHudMessageFrameCount; }
  // float GetHUDMessageTime() const { return mHudMessageTime; }
  void IncrementHUDMessageFrameCounter() { ++mHudMessageFrameCount; }

  void SendScriptMsg(const CScriptMsg& msg);
  void DeliverScriptMsg(const CScriptMsg& msg); // Guessed name
  void SendScriptMsg(CEntity*, TUniqueId, EScriptObjectMessage, TUniqueId);
  void SendScriptMsg(TUniqueId target, TUniqueId sender, EScriptObjectMessage message,
                     TUniqueId actor); // Guessed overload name.

  void AddObject(CEntity&);
  void AddObject(CEntity*);
  void DeleteObjectRequest(TUniqueId);
  void UpdateObjectInLists(CEntity&);
  // Retail 0x80037F90 returns CWeaponMgr::GetNumActive (CPlayerGun reads it before firing).
  // 0x80037FC0 is the remove (fn_800B321C decrements and erases at zero), 0x80037FF0 the add
  // (fn_800B32E0 inserts and increments) - upstream's order since #286, confirmed on the asm.
  int GetWeaponIdCount(TUniqueId owner, EWeaponType type);
  void AddWeaponId(TUniqueId owner, EWeaponType type);
  void RemoveWeaponId(TUniqueId owner, EWeaponType type);
  void ApplyDamageToWorld(TUniqueId owner, CActor& projectile, const CVector3f& position,
                          const CDamageInfo& damage, const CMaterialFilter& filter);
  void DrawSpaceWarp(const CVector3f& position, float strength) const;

  // void: retail 0x80037A90 ends in `bctrl` then the epilogue, leaving r3 untouched.
  void AddDrawableActor(const CActor& actor, const CVector3f& pos, const CAABox& bounds) const;
  void AddDrawableActorPlane(const CActor& actor, const CPlane& plane, const CAABox& bounds) const;
  bool CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const;
  void SetupParticleHook(const CActor& actor) const;
  const CActorModelParticles* GetActorModelParticles() const { return m_actorModelParticles; }

  CEntity* ObjectById(TUniqueId uid);
  const CEntity* GetObjectById(TUniqueId uid) const;
  CEntity* GetObjectByIdFromListAll(TUniqueId uid);
  bool RayCollideWorld(const CVector3f& start, const CVector3f& end,
                       const CMaterialFilter& filter, const CActor* damagee);
  bool RayCollideWorld(const CVector3f& start, const CVector3f& end,
                       const rstl::reserved_vector< TUniqueId, 1024 >& nearList,
                       const CMaterialFilter& filter, const CActor* damagee) const;
  bool RayCollideWorldInternal(const CVector3f& start, const CVector3f& end,
                               const CMaterialFilter& filter,
                               const rstl::reserved_vector< TUniqueId, 1024 >& nearList,
                               const CActor* damagee) const;
  CRayCastResult RayWorldIntersection(TUniqueId& idOut, const CVector3f& pos, const CVector3f& dir,
                                      float length, const CMaterialFilter& filter,
                                      const rstl::reserved_vector< TUniqueId, 1024 >& list) const;
  CRayCastResult RayStaticIntersection(const CVector3f& position, const CVector3f& direction,
                                       float length, const CMaterialFilter& filter) const;
  void BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& nearList,
                     const CVector3f& position, const CVector3f& direction, float length,
                     const CMaterialFilter& filter, const CActor* ignoreActor) const;
  void BuildColliderList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CActor& actor,
                         const CAABox& aabb) const;
  void BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CAABox& bounds,
                     const CMaterialFilter& filter, const CActor* ignoreActor) const;

  TEditorId GetEditorIdForUniqueId(TUniqueId) const;
  TUniqueId GetIdForScript(TEditorId eid) const;
  TIdListResult GetIdListForScript(TEditorId) const;

  CWorld* World() { return m_world; }
  const CWorld* GetWorld() const { return m_world; }
  CFluidPlaneManager* FluidPlaneManager() { return m_fluidPlaneManager; }
  bool IsFullyInitialized() const { return mInitPhase == kIP_Done; }
  CEnvFxManager* EnvFxManager() { return m_envFxManager; }
  const CEnvFxManager* GetEnvFxManager() const { return m_envFxManager; }
  CRandom16* Random() { return &mRandom; }
  int GetUpdateFrameIdx() const { return m_updateFrameIdx; }
  int GetRenderFrameIndex() const { return mRenderFrameIndex; } // Guessed name

  TAreaId GetNextAreaId() const { return m_nextAreaId; }
  TAreaId GetPreviousAreaId() const { return mPreviousAreaId; }
  void SetCurrentAreaId(TAreaId);
  void AreaLoaded(TAreaId area); // Guessed name, corresponding to Prime's area-load notification.
  void PrepareAreaUnload(TAreaId area); // Guessed name from Prime.
  // Retail 0x800419C8 is a single `blr`: AreaUnloaded is a no-op. It was previously written as
  // `fn_800419C8()`, so objdiff paired neither symbol and the report read as an unwritten
  // function next to an unpaired one. See docs/RUNNING_THE_DECOMP.md, 2026-09-29.
  void AreaUnloaded(TAreaId area);
  void SetActorAreaId(CActor& actor, TAreaId);
  // Guessed names.
  void SetPortalTransition(rstl::single_ptr< CPortalTransition >& transition);
  void SetPendingDockTransition(TAreaId area, int dock, bool showSoftTransition) {
    mPendingDockArea = area;
    mPendingDock = dock;
    mShowSoftTransition = showSoftTransition;
  }

  const CFrustumPlanes& GetFrustumPlanes() const { return m_planes; }
  int Get0x244c() const { return x244c; }

  int GetNumPlayers() const { return m_numPlayers; }
  uint ReturnFirstIfSingleElseSecond(uint single, uint multi) const; // Guessed name.
  CPlayer* GetPlayer(int index) { return m_players[index]; }
  const CPlayer* GetPlayer(int index) const { return m_players[index]; }
  CPlayer* Player(int index) { return m_players[index]; }

  CObjectList& ObjectListById(EGameObjectList id) { return *m_objectLists[id]; }
  const CObjectList& GetObjectListById(EGameObjectList id) const { return *m_objectLists[id]; }
  // Guessed names. The first filtered list qualifies only CScriptDoor objects.
  const rstl::list< CEntity* >& GetDoorList() const { return mFilteredObjectLists[0]->GetObjects(); }
  CMapWorldInfo* MapWorldInfo() { return mMapWorldInfo.GetPtr(); }

  void UpdateActorInSortedLists(CActor*);

  bool ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee, float damage, const TUniqueId& uid1, const TUniqueId& uid2, const CDamageInfo& info, int);

  void fn_8003dd88(CActor&, TUniqueId, const CDamageInfo& info, bool, int);
  void fn_8003BF84(CEntity*);
  void fn_800412EC(TUniqueId);
  bool fn_80036F10() const; // Maybe_CheckIsMultiplayer
  void fn_8003BE54();
  void InformListeners(const CVector3f& position, EListenNoiseType type);

  // State transitions
  void DeferStateTransition(EStateManagerTransition t);
  void EnterMapScreen() { DeferStateTransition(kSMT_MapScreen); }
  void EnterPauseScreen() { DeferStateTransition(kSMT_PauseGame); }
  void EnterLogBookScreen() { DeferStateTransition(kSMT_LogBook); }
  void EnterSaveGameScreen() { DeferStateTransition(kSMT_SaveGame); }
  void EnterMessageScreen(uint, float);
  bool GetWantsToEnterMapScreen() const { return m_deferredTransition == kSMT_MapScreen; }
  bool GetWantsToEnterPauseScreen() const { return m_deferredTransition == kSMT_PauseGame; }
  void SetCinematicPause(bool paused) { mCinematicPause = paused; } // Guessed name
  bool GetWantsToEnterLogBookScreen() const { return m_deferredTransition == kSMT_LogBook; }
  bool GetWantsToEnterSaveGameScreen() const { return m_deferredTransition == kSMT_SaveGame; }
  bool GetWantsToEnterMessageScreen() const { return m_deferredTransition == kSMT_MessageScreen; }

  const CCameraManager* GetCameraManager(int playerIndex) const { return m_cameraManagers[playerIndex]; }
  CCameraManager* CameraManager(int playerIndex) { return m_cameraManagers[playerIndex]; }
  const CPlayerState* GetPlayerState() const { return m_playerState; }
  const CPlayer* GetCurrentRenderPlayer() const { return mCurrentRenderPlayer; } // Guessed name
  int GetCurrentRenderPlayerIndex() const { return mCurrentRenderPlayerIndex; } // Guessed name
  const CCameraManager* GetCurrentRenderCameraManager() const { return m_cameraManager; } // Guessed name
  const CPlayerState* GetPlayerState(int playerIndex) const { return m_playerStates[playerIndex]; }
  CPlayerState* PlayerState(int playerIndex) { return m_playerStates[playerIndex]; }
  CRumbleManager* RumbleManager(int playerIndex) { return m_rumbleManagers[playerIndex]; }
  const CWeaponMgr* GetWeaponManager() const { return m_weaponMgr; }

  int fn_800366e4(CActor*);
  CScriptObjectLoaderHelper& fn_80036200();
  rstl::single_ptr< CPortalTransition >& fn_80036220();
  bool fn_80036284();
  void fn_80036650();
  void fn_80037784();
  void fn_800362E0();
  int fn_80036B6C() const;
  bool fn_80037904(TUniqueId id);
  bool fn_80037944(TUniqueId id);
  bool fn_80037984(TUniqueId id);
  bool fn_800379C4(TUniqueId id);
  bool fn_80037A04(TUniqueId id);
  float fn_80036F78(float value);
  float fn_80038364();
  void KillSaveGameInterface();
  void fn_80038370(float value);
  void TouchSky();
  void TouchPlayerActor();
  void fn_80039CCC(int pass);
  void fn_80039DDC(const TAreaId& area, int type, int mask, int targetMask);
  void fn_80039244();
  void fn_8003A3C0(int& a, int& b, int type) const;
  static void fn_8003EC0C();
  void fn_8003F970(CEntity* entity, float dt);
  void fn_8003FF1C();
  void fn_8003FF20();
  static bool fn_8003FF24();
  static void fn_8003FF50();
  void fn_8003FF70(int, int);
  void fn_8003FF74(int);
  bool fn_800421B4() const;
  int fn_801EDD8C(TUniqueId) const;

public:
  ushort m_nextFreeIndex;
  rstl::reserved_vector< ushort, 1024 > m_objectIndexArray;                // x0x4
  rstl::reserved_vector< rstl::auto_ptr< CObjectList >, 8 > m_objectLists; // 0x808
  rstl::reserved_vector< CObjectList*, 8 > mDynamicObjectLists;
  rstl::reserved_vector< rstl::auto_ptr< CFilteredObjectList >, 6 > mFilteredObjectLists;
  rstl::reserved_vector< CFilteredObjectList*, 6 > mDynamicFilteredObjectLists;
  MapWorldInfoAreas mapWorldInfoAreas;
  char x8d4_[0x18];
  ScriptMsgArray m_scriptMsgs;
  CArchitectureQueue* m_archQueue;
  int m_numPlayers;
  CPlayer* m_players[4];
  CPlayerState* m_playerStates[4];
  CCameraManager* m_cameraManagers[4];
  CRumbleManager* m_rumbleManagers[4];
  CFinalInput m_finalInputs[4];
  char x15ec_[0xc];
  CPlayer* mCurrentRenderPlayer; // 0x15f8, guessed name
  CPlayerState* m_playerState;
  CCameraManager* m_cameraManager;
  CWorld* m_world;                                                 // 0x1604
  rstl::list< rstl::reserved_vector< CEntity*, 32 > > m_graveyard; // 0x1608
  rstl::single_ptr< CStateManagerContainer > m_stateManagerContainer;
  SL::CSortedListManager* m_sortedListManager;
  CWeaponMgr* m_weaponMgr;
  CFluidPlaneManager* m_fluidPlaneManager;
  CEnvFxManager* m_envFxManager;               // 0x1630
  CActorModelParticles* m_actorModelParticles; // 0x1634
  void* x1638;
  TIdList m_scriptIdMap; // 0x163c
  char pad2_2[0x2C];
  // Four rc_ptrs: retail's destructor releases 0x167C, 0x1684, 0x168C and 0x1694. The second
  // upstream sync (c3537e0) added mMapWorldInfo after m_relayTracker, which put every member
  // from 0x168C up 8 bytes high; it belongs in the 0x167C slot.
  rstl::rc_ptr< CMapWorldInfo > mMapWorldInfo;
  rstl::rc_ptr< CRelayTracker > m_relayTracker;
  rstl::rc_ptr< CWorldTransManager > m_worldTransManager;
  CWorldLayerState* m_currentWorldLayerState;
  int* x1698;
  rstl::single_ptr< CSaveGameScreen > m_saveGameScreen; // x169C
  TAreaId m_nextAreaId; // x16a0
  TAreaId mPreviousAreaId;
  int mRenderFrameIndex; // Guessed name: visibility age used by projectile impacts.
  int m_updateFrameIdx; // 16AC
  int m_objectDrawToken; // 16B0
  char pad4[0x30]; // 16B4
  CRandom16 mRandom;
  char x16e8_[8];
  EInitPhase mInitPhase;
  char x16f4_[0xD40];

  CAssetId m_pauseHudMessage; // 0x2434
  float mEscapeTotalTime;
  float x243c;
  TUniqueId m_bossId; // 0x2440
  float m_bossHealth;
  uint m_bossLanguageTableIndex;
  int x244c; // unk type
  TUniqueId m_uid_setBySpecialFunc;
  TUniqueId m_playerActorHead; // 0x2452
  float m_hudMessageTime;     // 0x2454
  uintptr_t x2458;            // unk type; a list link, host pointer width on the port
  int mHudMessageFrameCount; // 0x245c
  int m_forPausedHudMemo;     // 0x2460
  CAssetId m_pausedHudMemoAssetId;
  float x2468;
  CAssetId mMapTeleportWorldId; // Guessed name
  EStateManagerTransition m_deferredTransition;

  char pad5[4]; // 0x246c
  CFrustumPlanes m_planes; // 0x2478
  int mCurrentRenderPlayerIndex; // Guessed name
  char pad6[0x28f8 - 0x24e0];
  TAreaId mPendingDockArea; // Guessed name.
  int mPendingDock; // Guessed name.
  rstl::single_ptr< CPortalTransition > mPortalTransition; // Guessed name.
  char x2904_[0x2938 - 0x2904];

  CVector3f x2938;
  float x2944;
  CColor x2948;
  bool m_unkFlagA1 : 1;
  bool m_unkFlagA2 : 1;
  bool m_unkFlagA3 : 1;
  bool m_unkFlagA4 : 1;
  bool m_unkFlagA5 : 1;
  bool mCinematicPause : 1;
  bool m_unkFlagA7 : 1;
  bool m_isDarkWorld : 1; // 0x294c
  bool mShowSoftTransition : 1; // Guessed name.
  bool m_unkFlagB2 : 1;
  bool m_unkFlagB3 : 1;
  bool m_unkFlagB4 : 1;
  bool m_unkFlagB5 : 1;
  bool m_unkFlagB6 : 1;
  bool m_unkFlagB7 : 1;
  bool m_unkFlagB8 : 1;
};
// CHECK_OFFSETOF(CStateManager, m_world, 0x1604)
// CHECK_OFFSETOF(CStateManager, m_envFxManager, 0x1630)

#endif // _CSTATEMANAGER
