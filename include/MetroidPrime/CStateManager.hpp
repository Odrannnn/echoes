#ifndef _CSTATEMANAGER
#define _CSTATEMANAGER

#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CSortedListManager.hpp"
#include "TGameTypes.hpp"
#include "MetroidPrime/Weapons/WeaponTypes.hpp"

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/map.hpp"
#include "rstl/pair.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/reserved_vector.hpp"
#include "rstl/single_ptr.hpp"

class CWorld;
class CArchitectureQueue;
class CEnvFxManager;
class CEntity;
class CActor;
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
class CStateManagerContainerUnk13EC0;

// Held by CStateManager at 0x2900; only its destructor (fn_80230DA0) is known.
class CStateManagerUnk2900 {
public:
  ~CStateManagerUnk2900();
};
class CMaterialFilter;
class CPlane;
class CRayCastResult;
class CWeaponMgr;
class CFluidPlaneManager;
class CDamageInfo;
class CAABox;

struct MapWorldInfoAreas {};

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

  CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&, const rstl::ncrc_ptr< CMapWorldInfo >&,
                const rstl::ncrc_ptr< CPlayerState >&, const rstl::ncrc_ptr< CWorldTransManager >&);
  ~CStateManager();

  TUniqueId AllocateUniqueId();
  uint MaskUIdNumPlayers(TUniqueId id) const;
  void ShowPausedHUDMemo(CAssetId strg, float time);
  void QueueMessage(int frameCount, CAssetId msg, float f1);
  void SetBossParams(TUniqueId bossId, float maxEnergy, uint stringIdx);
  void SetIsDarkWorld(bool);
  bool GetIsDarkWorld() const { return m_isDarkWorld; }
  void DisplayAlertAboutOutOfAmmo(const CPlayer&, CPlayerState::EItemType) const;

  void SendScriptMsg_fn_80037100(const CScriptMsg&);
  void DeliverScriptMsgImmediate(const CScriptMsg&);
  void DeliverScriptMsg(CEntity*, TUniqueId, EScriptObjectMessage, TUniqueId);
  void DeliverScriptMsg(TUniqueId, TUniqueId, EScriptObjectMessage, TUniqueId);

  void AddObject(CEntity&);
  void AddObject(CEntity*);
  void DeleteObjectRequest(TUniqueId);
  void UpdateObjectInLists(CEntity&);
  
  void AddDrawableActor(const CActor& actor, const CVector3f& pos, const CAABox& bounds) const;
  void AddDrawableActorPlane(const CActor& actor, const CPlane& plane, const CAABox& bounds) const;
  bool CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const;
  uint fn_800368E4(uint single, uint multi) const;
  void SetupParticleHook(const CActor& actor) const;
  const CActorModelParticles* GetActorModelParticles() const { return m_actorModelParticles; }

  void BuildNearList(TEntityList& out, const CVector3f& pos, const CVector3f& dir, float mag,
                     const CMaterialFilter& filter, const CActor* actor) const;
  void BuildColliderList(TEntityList& out, const CActor& actor, const CAABox& aabb) const;
  void BuildNearList(TEntityList& out, const CAABox& aabb, const CMaterialFilter& filter,
                     const CActor* actor) const;

  bool RayCollideWorld(const CVector3f& start, const CVector3f& end, const TEntityList& nearList,
                       const CMaterialFilter& filter, const CActor* damagee) const;
  bool RayCollideWorldInternal(const CVector3f& start, const CVector3f& end,
                               const CMaterialFilter& filter, const TEntityList& nearList,
                               const CActor* damagee) const;
  CRayCastResult RayStaticIntersection(const CVector3f& pos, const CVector3f& dir, float length,
                                       const CMaterialFilter& filter) const;
  CRayCastResult RayWorldIntersection(TUniqueId& idOut, const CVector3f& pos, const CVector3f& dir,
                                      float length, const CMaterialFilter& filter,
                                      const TEntityList& list) const;

  CEntity* ObjectById(TUniqueId uid);
  const CEntity* GetObjectById(TUniqueId uid) const;
  CEntity* GetObjectByIdFromListAll(TUniqueId uid);

  TEditorId GetEditorIdForUniqueId(TUniqueId) const;
  TUniqueId GetIdForScript(TEditorId eid) const;
  TIdListResult GetIdListForScript(TEditorId) const;

  CWorld* World() { return m_world; }
  const CWorld* GetWorld() const { return m_world; }
  CFluidPlaneManager* FluidPlaneManager() { return m_fluidPlaneManager; }
  CEnvFxManager* EnvFxManager() { return m_envFxManager; }
  const CEnvFxManager* GetEnvFxManager() const { return m_envFxManager; }
  // CRandom16* Random() const { return x900_random; }
  int GetUpdateFrameIdx() const { return m_updateFrameIdx; }

  TAreaId GetNextAreaId() const { return m_nextAreaId; }
  void SetCurrentAreaId(TAreaId);
  void SetActorAreaId(CActor& actor, TAreaId);

  const CFrustumPlanes& GetFrustumPlanes() const { return m_planes; }
  int Get0x244c() const { return x244c; }

  int GetNumPlayers() const { return m_numPlayers; }
  CPlayer* GetPlayer(int index) { return m_players[index]; }
  const CPlayer* GetPlayer(int index) const { return m_players[index]; }
  CPlayer* Player(int index) { return m_players[index]; }

  CObjectList& ObjectListById(EGameObjectList id) { return *m_objectLists[id]; }
  const CObjectList& GetObjectListById(EGameObjectList id) const { return *m_objectLists[id]; }

  void UpdateActorInSortedLists(CActor*);

  bool ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee, float damage, const TUniqueId& uid1, const TUniqueId& uid2, const CDamageInfo& info, int);

  void fn_8003dd88(CActor&, TUniqueId, const CDamageInfo& info, bool, int);
  void fn_8003BF84(CEntity*);
  void fn_800412EC(TUniqueId);
  bool fn_80036F10() const; // Maybe_CheckIsMultiplayer
  void fn_8003BE54();
  void fn_8003C4B8(const CVector3f&, int);

  // State transitions
  void DeferStateTransition(EStateManagerTransition t);
  void EnterMapScreen() { DeferStateTransition(kSMT_MapScreen); }
  void EnterPauseScreen() { DeferStateTransition(kSMT_PauseGame); }
  void EnterLogBookScreen() { DeferStateTransition(kSMT_LogBook); }
  void EnterSaveGameScreen() { DeferStateTransition(kSMT_SaveGame); }
  void EnterMessageScreen(uint, float);
  bool GetWantsToEnterMapScreen() const { return m_deferredTransition == kSMT_MapScreen; }
  bool GetWantsToEnterPauseScreen() const { return m_deferredTransition == kSMT_PauseGame; }
  bool GetWantsToEnterLogBookScreen() const { return m_deferredTransition == kSMT_LogBook; }
  bool GetWantsToEnterSaveGameScreen() const { return m_deferredTransition == kSMT_SaveGame; }
  bool GetWantsToEnterMessageScreen() const { return m_deferredTransition == kSMT_MessageScreen; }

  const CCameraManager* GetCameraManager(int playerIndex) const { return m_cameraManagers[playerIndex]; }
  const CPlayerState* GetPlayerState() const { return m_playerState; }
  const CPlayerState* GetPlayerState(int playerIndex) const { return m_playerStates[playerIndex]; }
  CPlayerState* PlayerState(int playerIndex) { return m_playerStates[playerIndex]; }
  CRumbleManager* RumbleManager(int playerIndex) { return m_rumbleManagers[playerIndex]; }
  const CWeaponMgr* GetWeaponManager() const { return m_weaponMgr; }

  int fn_800366e4(CActor *);
  CStateManagerContainerUnk13EC0& fn_80036200();
  const CStateManagerContainerUnk13EC0& fn_80036210() const;
  rstl::single_ptr< CStateManagerUnk2900 >& fn_80036220();
  void fn_80036228(rstl::single_ptr< CStateManagerUnk2900 >& ptr);
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
  int fn_80037F90(TUniqueId id, EWeaponType type);
  void fn_80037FC0(TUniqueId id, EWeaponType type);
  void fn_80037FF0(TUniqueId id, EWeaponType type);
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
  void fn_800419C8();
  bool fn_800421B4() const;
  int fn_801EDD8C(TUniqueId) const;

public:
  ushort m_nextFreeIndex;
  rstl::reserved_vector< ushort, 1024 > m_objectIndexArray;                // x0x4
  rstl::reserved_vector< rstl::auto_ptr< CObjectList >, 9 > m_objectLists; // 0x808
  MapWorldInfoAreas mapWorldInfoAreas;
  char pad1[0x94]; // 0x408
  ScriptMsgArray m_scriptMsgs;
  CArchitectureQueue* m_archQueue;
  int m_numPlayers;
  CPlayer* m_players[4];
  CPlayerState* m_playerStates[4];
  CCameraManager* m_cameraManagers[4];
  CRumbleManager* m_rumbleManagers[4];
  CFinalInput m_finalInputs[4];
  CPlayerState* m_playerState;
  CCameraManager* m_cameraManager;
  CWorld* m_world;                                                 // 0x1604
  rstl::list< rstl::reserved_vector< CEntity*, 32 > > m_graveyard; // 0x1608
  rstl::single_ptr< CStateManagerContainer > m_stateManagerContainer;
  CSortedListManager* m_sortedListManager;
  CWeaponMgr* m_weaponMgr;
  CFluidPlaneManager* m_fluidPlaneManager;
  CEnvFxManager* m_envFxManager;               // 0x1630
  CActorModelParticles* m_actorModelParticles; // 0x1634
  void* x1638;
  TIdList m_scriptIdMap; // 0x163c
  char pad2_2[0x34];
  rstl::rc_ptr< CRelayTracker > m_relayTracker;
  int x1684;
  int x1688;
  rstl::rc_ptr< CWorldTransManager > m_worldTransManager;
  CWorldLayerState* m_currentWorldLayerState;
  int* x1698;
  rstl::single_ptr< CSaveGameScreen > m_saveGameScreen; // x169C
  TAreaId m_nextAreaId; // x16a0
  char pad3[0x4]; // 16A4
  int x16a8;
  int m_updateFrameIdx; // 16AC
  int m_objectDrawToken; // 16B0
  char pad4[0xD80]; // 16B4

  CAssetId m_pauseHudMessage; // 0x2434
  float x2438_escapeTotalTime;
  float x243c;
  TUniqueId m_bossId; // 0x2440
  float m_bossHealth;
  uint m_bossLanguageTableIndex;
  int x244c; // unk type
  TUniqueId m_uid_setBySpecialFunc;
  TUniqueId m_playerActorHead; // 0x2452
  float m_hudMessageTime;     // 0x2454
  uintptr_t x2458;            // unk type; a list link, host pointer width on the port
  int m_hudMessageFrameCount; // 0x245c
  int m_forPausedHudMemo;     // 0x2460
  CAssetId m_pausedHudMemoAssetId;
  float x2468;
  int x246c;
  EStateManagerTransition m_deferredTransition;

  char pad5[4]; // 0x246c
  CFrustumPlanes m_planes; // 0x2478
  char pad6[0x424]; // 0x24DC
  rstl::single_ptr< CStateManagerUnk2900 > x2900; // owner class not yet named; dtor fn_80230DA0
  char pad6b[0x34];

  CVector3f x2938;
  float x2944;
  CColor x2948;
  bool m_unkFlagA1 : 1;
  bool m_unkFlagA2 : 1;
  bool m_unkFlagA3 : 1;
  bool m_unkFlagA4 : 1;
  bool m_unkFlagA5 : 1;
  bool m_unkFlagA6 : 1;
  bool m_unkFlagA7 : 1;
  bool m_isDarkWorld : 1; // 0x294c
  bool m_unkFlagB1 : 1;
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
