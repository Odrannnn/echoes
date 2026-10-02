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
class CPatterned;
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

// Values are retail's, read off `DeferStateTransition` (0x80037828) and its callers:
// 0 InGame, 1 MapScreen (AcceptMapStation), 2 PauseGame (AcceptPauseGame), 3 is reached only from
// fn_8001EE58, 4 LogBook (AcceptLogbook), 5 SaveGame - the value `DeferStateTransition` branches on
// to allocate the CSaveGameScreen, and the one AcceptSaveStation passes - 6 MessageScreen
// (ShowPausedHUDMemo). The names for 3..5 were previously shifted by one: 5 is the value that
// builds the save screen, not an unknown, and `CStateManager::DeferStateTransition` compares
// against `kSMT_SaveGame` to keep that at 5.
enum EStateManagerTransition {
  kSMT_InGame,
  kSMT_MapScreen,
  kSMT_PauseGame,
  kSMT_Unk,
  kSMT_LogBook,
  kSMT_SaveGame,
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
    CScriptMsg mMessages[192];
    uint mWriteIndex; //!< 0xC00, the write cursor
    //! 0xC04, the read cursor. `mutable` because retail's `fn_8019E6BC` is a **const** member
    //! function (`...14ScriptMsgArrayCFv` in `config/G2ME01/symbols.txt`) that advances it.
    mutable uint mReadIndex;

    void Append(const CScriptMsg& msg);
    int fn_8019E69C();
    // **By value, not by reference.** Retail builds the popped message into the caller's return
    // slot in `r3` - `sth`/`sth`/`sth`/`stw`/`stw` straight to `0(r3)`..`12(r3)` - so a reference
    // return cannot compile to it. `CStateManager::fn_8003BE54`'s `CScriptMsg msg = ...` still
    // reads the same.
    CScriptMsg fn_8019E6BC() const;

    bool empty() const { return mWriteIndex == mReadIndex; }
  };

public:
  typedef rstl::map< TEditorId, TUniqueId > TIdList;
  typedef rstl::pair< TIdList::const_iterator, TIdList::const_iterator > TIdListResult;

  // Guessed phase names, derived from world initialization.
  enum EInitPhase { kIP_LoadAudioGroups, kIP_LoadWorld, kIP_LoadFirstArea, kIP_Done };
  // Guessed names, based on the update dispatch and Prime's corresponding state.
  enum EGameState { kGS_Running, kGS_SoftPaused };

  CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&, const rstl::ncrc_ptr< CMapWorldInfo >&,
                const rstl::ncrc_ptr< CPlayerState >&, const rstl::ncrc_ptr< CWorldTransManager >&);
  ~CStateManager();

  TUniqueId AllocateUniqueId();
  CScriptObjectLoaderHelper& ScriptObjectLoaderHelper();
  uint MaskUIdNumPlayers(TUniqueId id) const;
  void SetBossParams(TUniqueId bossId, float maxEnergy, uint stringIdx);
  void SetIsDarkWorld(bool);
  bool GetIsDarkWorld() const { return mIsDarkWorld; }
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
  // Retail 0x8003E25C.
  void ApplyDamage(TUniqueId sender, TUniqueId victim, TUniqueId owner, const CDamageInfo& damage,
                   const CMaterialFilter& filter, const CVector3f& direction);
  void ApplyDamageToWorld(TUniqueId owner, CActor& projectile, const CVector3f& position,
                          const CDamageInfo& damage, const CMaterialFilter& filter);
  void DrawSpaceWarp(const CVector3f& position, float strength) const;

  // void: retail 0x80037A90 ends in `bctrl` then the epilogue, leaving r3 untouched.
  void AddDrawableActor(const CActor& actor, const CVector3f& pos, const CAABox& bounds) const;
  void AddDrawableActorPlane(const CActor& actor, const CPlane& plane, const CAABox& bounds) const;
  bool CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const;
  void SetupParticleHook(const CActor& actor) const;
  const CActorModelParticles* GetActorModelParticles() const { return mActorModelParticles; }

  CEntity* ObjectById(TUniqueId uid);
  const CEntity* GetObjectById(TUniqueId uid) const;
  CEntity* GetObjectByIdFromListAll(TUniqueId uid);
  bool RayCollideWorld(const CVector3f& start, const CVector3f& end, const CMaterialFilter& filter,
                       const CActor* damagee);
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
  void BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CVector3f& position,
                     const CVector3f& direction, float length, const CMaterialFilter& filter,
                     const CActor* ignoreActor) const;
  void BuildColliderList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CActor& actor,
                         const CAABox& aabb) const;
  void BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CAABox& bounds,
                     const CMaterialFilter& filter, const CActor* ignoreActor) const;

  TEditorId GetEditorIdForUniqueId(TUniqueId) const;
  TUniqueId GetIdForScript(TEditorId eid) const;
  TIdListResult GetIdListForScript(TEditorId) const;

  CWorld* World() { return mWorld; }
  const CWorld* GetWorld() const { return mWorld; }
  CFluidPlaneManager* FluidPlaneManager() { return mFluidPlaneManager; }
  bool IsFullyInitialized() const { return mInitPhase == kIP_Done; }
  CEnvFxManager* EnvFxManager() { return mEnvFxManager; }
  const CEnvFxManager* GetEnvFxManager() const { return mEnvFxManager; }
  CRandom16* Random() { return &mRandom; }
  int GetUpdateFrameIdx() const { return mUpdateFrameIdx; }
  int GetRenderFrameIndex() const { return mRenderFrameIndex; } // Guessed name

  TAreaId GetNextAreaId() const { return mNextAreaId; }
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

  const CFrustumPlanes& GetFrustumPlanes() const { return mPlanes; }
  int Get0x244c() const { return x244c; }

  int GetNumPlayers() const { return mNumPlayers; }
  uint ReturnFirstIfSingleElseSecond(uint single, uint multi) const; // Guessed name.
  CPlayer* GetPlayer(int index) { return mPlayers[index]; }
  const CPlayer* GetPlayer(int index) const { return mPlayers[index]; }
  CPlayer* Player(int index) { return mPlayers[index]; }

  CObjectList& ObjectListById(EGameObjectList id) { return *mObjectLists[id]; }
  const CObjectList& GetObjectListById(EGameObjectList id) const { return *mObjectLists[id]; }
  // Guessed names. The first filtered list qualifies only CScriptDoor objects.
  const rstl::list< CEntity* >& GetDoorList() const {
    return mFilteredObjectLists[0]->GetObjects();
  }
  CMapWorldInfo* MapWorldInfo() { return mMapWorldInfo.GetPtr(); }

  void UpdateActorInSortedLists(CActor*);

  bool ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee, float damage,
                        const TUniqueId& uid1, const TUniqueId& uid2, const CDamageInfo& info, int);

  void fn_8003dd88(CActor&, TUniqueId, const CDamageInfo& info, bool, int);
  void fn_8003BF84(CEntity*);
  void fn_800412EC(TUniqueId);
  bool IsMultiplayer() const; // Guessed name
  void fn_8003BE54();
  void InformListeners(const CVector3f& position, EListenNoiseType type);
  void Think(float dt);
  void MoveActors(float dt);
  // Guessed helper names, recovered from their update-loop consumers.
  bool ShouldUpdatePatterned(const CPatterned& actor);
  void ThinkEntity(float dt, CEntity& entity);

  // State transitions
  void DeferStateTransition(EStateManagerTransition t);
  void EnterMapScreen() { DeferStateTransition(kSMT_MapScreen); }
  void EnterPauseScreen() { DeferStateTransition(kSMT_PauseGame); }
  void EnterLogBookScreen() { DeferStateTransition(kSMT_LogBook); }
  void EnterSaveGameScreen() { DeferStateTransition(kSMT_SaveGame); }
  void EnterMessageScreen(uint, float);
  bool GetWantsToEnterMapScreen() const { return mDeferredTransition == kSMT_MapScreen; }
  bool GetWantsToEnterPauseScreen() const { return mDeferredTransition == kSMT_PauseGame; }
  void SetCinematicPause(bool paused) { mCinematicPause = paused; } // Guessed name
  void SetSkipCinematicReceiver(TUniqueId uid) { mSpecialFunctionId = uid; } // Guessed name
  bool GetWantsToEnterLogBookScreen() const { return mDeferredTransition == kSMT_LogBook; }
  bool GetWantsToEnterSaveGameScreen() const { return mDeferredTransition == kSMT_SaveGame; }
  bool GetWantsToEnterMessageScreen() const { return mDeferredTransition == kSMT_MessageScreen; }

  const CCameraManager* GetCameraManager(int playerIndex) const {
    return mCameraManagers[playerIndex];
  }
  CCameraManager* CameraManager(int playerIndex) { return mCameraManagers[playerIndex]; }
  const CPlayerState* GetPlayerState() const { return mPlayerState; }
  const CPlayer* GetCurrentRenderPlayer() const { return mCurrentRenderPlayer; } // Guessed name
  int GetCurrentRenderPlayerIndex() const { return mCurrentRenderPlayerIndex; }  // Guessed name
  const CCameraManager* GetCurrentRenderCameraManager() const {
    return mCameraManager;
  } // Guessed name
  const CPlayerState* GetPlayerState(int playerIndex) const { return mPlayerStates[playerIndex]; }
  CPlayerState* PlayerState(int playerIndex) { return mPlayerStates[playerIndex]; }
  CRumbleManager* RumbleManager(int playerIndex) { return mRumbleManagers[playerIndex]; }
  const CWeaponMgr* GetWeaponManager() const { return mWeaponMgr; }

  bool fn_800366e4(CActor*);
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
  void fn_8003FF1C();
  void fn_8003FF20();
  static bool fn_8003FF24();
  static void fn_8003FF50();
  void fn_8003FF70(int, int);
  void fn_8003FF74(int);
  bool fn_800421B4() const;
  int fn_801EDD8C(TUniqueId) const;

public:
  ushort mNextFreeIndex;
  rstl::reserved_vector< ushort, 1024 > mObjectIndexArray;                // x0x4
  rstl::reserved_vector< rstl::auto_ptr< CObjectList >, 8 > mObjectLists; // 0x808
  rstl::reserved_vector< CObjectList*, 8 > mDynamicObjectLists;
  rstl::reserved_vector< rstl::auto_ptr< CFilteredObjectList >, 6 > mFilteredObjectLists;
  rstl::reserved_vector< CFilteredObjectList*, 6 > mDynamicFilteredObjectLists;
  MapWorldInfoAreas mAllocatedObjectIndices;
  char x8d4_[0x18];
  ScriptMsgArray mScriptMsgs;
  CArchitectureQueue* mArchQueue;
  int mNumPlayers;
  CPlayer* mPlayers[4];
  CPlayerState* mPlayerStates[4];
  CCameraManager* mCameraManagers[4];
  CRumbleManager* mRumbleManagers[4];
  CFinalInput mFinalInputs[4];
  char x15ec_[0xc];
  CPlayer* mCurrentRenderPlayer; // 0x15f8, guessed name
  CPlayerState* mPlayerState;
  CCameraManager* mCameraManager;
  CWorld* mWorld;                                                 // 0x1604
  rstl::list< rstl::reserved_vector< CEntity*, 32 > > mGraveyard; // 0x1608
  rstl::single_ptr< CStateManagerContainer > mStateManagerContainer;
  SL::CSortedListManager* mSortedListManager;
  CWeaponMgr* mWeaponMgr;
  CFluidPlaneManager* mFluidPlaneManager;
  CEnvFxManager* mEnvFxManager;               // 0x1630
  CActorModelParticles* mActorModelParticles; // 0x1634
  void* x1638;
  TIdList mScriptIdMap; // 0x163c
  char mUnknownData1[0x2C];
  // Four rc_ptrs: retail's destructor releases 0x167C, 0x1684, 0x168C and 0x1694. The second
  // upstream sync (c3537e0) added mMapWorldInfo after mRelayTracker, which put every member
  // from 0x168C up 8 bytes high; it belongs in the 0x167C slot.
  rstl::rc_ptr< CMapWorldInfo > mMapWorldInfo;
  rstl::rc_ptr< CRelayTracker > mRelayTracker;
  rstl::rc_ptr< CWorldTransManager > mWorldTransManager;
  CWorldLayerState* mCurrentWorldLayerState;
  int* x1698;
  rstl::single_ptr< CSaveGameScreen > mSaveGameScreen; // x169C
  TAreaId mNextAreaId;                                 // x16a0
  TAreaId mPreviousAreaId;
  int mRenderFrameIndex; // Guessed name: visibility age used by projectile impacts.
  int mUpdateFrameIdx;   // 16AC
  int mObjectDrawToken;  // 16B0
  char mUnknownData2[0x30]; // 16B4
  CRandom16 mRandom;
  char x16e8_[4];
  EGameState mGameState;
  EInitPhase mInitPhase;
  char x16f4_[0xD40];

  CAssetId mPauseHudMessage; // 0x2434
  float mEscapeTotalTime;
  float x243c;
  TUniqueId mBossId; // 0x2440
  float mBossHealth;
  uint mBossLanguageTableIndex;
  int x244c; // unk type
  TUniqueId mSpecialFunctionId;
  TUniqueId mPlayerActorHead;   // 0x2452
  float mHudMessageTime;        // 0x2454
  uintptr_t x2458;              // unk type; a list link, host pointer width on the port
  int mHudMessageFrameCount;    // 0x245c
  int mPausedHudMemoFrameCount; // 0x2460
  CAssetId mPausedHudMemoAssetId;
  float x2468;
  CAssetId mMapTeleportWorldId; // Guessed name
  EStateManagerTransition mDeferredTransition;

  char mUnknownData3[4];
  CFrustumPlanes mPlanes;        // 0x2478
  int mCurrentRenderPlayerIndex; // Guessed name
  char mUnknownData4[0x28f8 - 0x24e0];
  TAreaId mPendingDockArea;                                // Guessed name.
  int mPendingDock;                                        // Guessed name.
  rstl::single_ptr< CPortalTransition > mPortalTransition; // Guessed name.
  char x2904_[0x2938 - 0x2904];

  CVector3f x2938;
  float x2944;
  CColor x2948;
  // Positions are fixed by retail and cannot be renumbered: bit 28 is the fifth field
  // (`KillSaveGameInterface` writes it) and bit 26 is the third (`CScriptSpecialFunction::
  // fn_80107458` sets and clears it on Increment/Decrement). Only the *names* were off - the
  // guessed `mCinematicPause` sat on the sixth field, so it moved to the third and the unnamed
  // flags shifted up one place, leaving every bit where retail has it.
  bool mUnkFlagA1 : 1;
  bool mUnkFlagA2 : 1;
  bool mCinematicPause : 1; // Guessed name; 0x294c bit 26
  bool mUnkFlagA4 : 1;
  bool mUnkFlagA5 : 1;
  bool mUnkFlagA6 : 1;
  bool mUnkFlagA7 : 1;
  bool mIsDarkWorld : 1; // 0x294c
  bool mShowSoftTransition : 1; // Guessed name.
  bool mUnkFlagB2 : 1;
  bool mDispatchingScriptMessages : 1;
  bool mUnkFlagB4 : 1;
  bool mUnkFlagB5 : 1;
  bool mUnkFlagB6 : 1;
  bool mUnkFlagB7 : 1;
  bool mUnkFlagB8 : 1;
};
// CHECK_OFFSETOF(CStateManager, mWorld, 0x1604)
// CHECK_OFFSETOF(CStateManager, mEnvFxManager, 0x1630)

#endif // _CSTATEMANAGER
