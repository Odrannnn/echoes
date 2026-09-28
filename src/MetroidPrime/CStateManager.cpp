#include "MetroidPrime/CStateManager.hpp"

#include "Collision/CRayCastResult.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorModelParticles.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/CSortedLists.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CGameCollision.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CSaveGameScreen.hpp"
#include "MetroidPrime/CStateManagerContainer.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/CDamageInfo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Weapons/CWeaponMgr.hpp"

#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CARAMToken.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "rstl/vector.hpp"

const int gkPVSEnabled = 1;

extern "C" void fn_8030184C();
extern "C" int lbl_80419A10;
extern "C" int lbl_80419A18;
extern "C" int lbl_80418FB8;
extern "C" int lbl_80418FBC;
extern "C" uchar lbl_80419730;
extern "C" uchar lbl_80419745;
extern "C" uchar lbl_80419A98;

extern "C" void fn_80036F68(CStateManager* mgr, void* node) {
  // Port: the console stores these as 32-bit guest addresses; on a 64-bit host the
  // list link is a host pointer, so go through uintptr_t rather than truncating.
  *reinterpret_cast< uintptr_t* >(static_cast< char* >(node) + 0x9c) = mgr->x2458;
  mgr->x2458 = reinterpret_cast< uintptr_t >(node);
}

extern "C" void fn_8003A834(uint* out) { *out = lbl_80418FB8; }

extern "C" void fn_8003AD74(uchar value) {
  lbl_80419745 = value;
  lbl_80419A98 = value;
  lbl_80419730 = value;
}

extern "C" void fn_8003B648(uint* out) { *out = lbl_80418FBC; }

extern "C" bool fn_8003C59C() { return false; }

// The two vector initializers have distinct retail entry points.
extern "C" void fn_80038D40(int* vec) { vec[1] = 0; }
extern "C" void fn_80038D4C(int* vec) { vec[1] = 0; }

// Retail's CStateManager.o defines the out-of-line CLight copy constructor (0x80038C9C, 0xA4
// bytes): fn_80038C5C returns a {u16, CLight} aggregate and calls it to fill the second word, so
// it belongs to this translation unit rather than to src/Kyoto/Graphics/CLight.cpp. It copies all
// 0x4D bytes of the object member by member - lfs/stfs per float, lwz/stw for CColor's packed word
// and the two ids, and one lbz/stb for the dirty-flag byte, which is why those two flags are one
// struct member rather than two bitfields.
CLight::CLight(const CLight& other)
: mPos(other.mPos)
, mDir(other.mDir)
, mColor(other.mColor)
, mType(other.mType)
, mSpotCutoff(other.mSpotCutoff)
, mDistC(other.mDistC)
, mDistL(other.mDistL)
, mDistQ(other.mDistQ)
, mAngleC(other.mAngleC)
, mAngleL(other.mAngleL)
, mAngleQ(other.mAngleQ)
, mPriority(other.mPriority)
, mLightId(other.mLightId)
, mCachedRadius(other.mCachedRadius)
, mCachedIntensity(other.mCachedIntensity)
, mDirty(other.mDirty) {}

extern "C" void fn_80038624(CStateManager*);
extern "C" void fn_800388EC(CStateManager* mgr) { fn_80038624(mgr); }
extern "C" void fn_801EBBC8(void*);
extern "C" void fn_80039B1C(void* value) { fn_801EBBC8(value); }

// fn_80043180 / fn_800434CC / fn_80043688 (0xCF80/0xD2CC/0xD488) and fn_800391B4 (0x2FB4) are
// bare one-`bl` forwarders and all four are 100.00% as written here - but each forwards to a
// callee this unit does not define (fn_800431A0 / fn_800434EC / fn_800436A8 / fn_800391E4), so
// each one raises the port's undefined count by one. `tools/probe_sources.sh` gates that count
// against a baseline and reports STRICT FAIL when it grows, so they are left out until the
// callees land. See the notes file for the measured numbers.

void TouchPlayerActor(CEntity& ent, CStateManager& mgr);

struct queryOutput {
  int* unk0;
  int unk4;
};

void fn_80041518(queryOutput&, MapWorldInfoAreas& mapWorldInfoAreas, ushort ourIndex);

// m_graveyard (0x1608) is an rstl::list< rstl::reserved_vector<CEntity*, 32> >, and the
// two template members retail left out of line are the two functions below:
//
//   fn_8003C0C4 == list::create_node(node* prev, node* next, const T&)   (4 args:
//                  r3 = the list itself, which create_node never reads - that is the
//                  signature of a non-static member, and it is how the call in
//                  fn_8003C054 is register-for-register the same)
//   fn_8003C054 == list::do_insert_before(node* n, const T&)
//
// <rstl/list.hpp> spells both of them out inline, so writing them through the template
// inlines them into fn_8003BF84 and emits nothing for objdiff to pair. They are written
// here over the same layout instead. The list object and its node are modelled locally
// for the same reason: a name objdiff can pair is what earns the match.
typedef rstl::reserved_vector< CEntity*, 32 > GraveyardBucket;
// The same 132 bytes as GraveyardBucket, named so the node's storage is legible and so
// the layout is asserted rather than assumed. The element itself is the real
// reserved_vector: its copy constructor is what emits MWCC's unrolled block move, and
// a hand-written loop or a memcpy call here does not reproduce it.
struct GraveyardBucketView {
  uint x0_count;
  CEntity* x4_data[32];
};
struct GraveyardNode {
  GraveyardNode* x0_prev;
  GraveyardNode* x4_next;
  GraveyardBucketView x8_item;
  GraveyardNode* get_prev() const { return x0_prev; }
  GraveyardNode* get_next() const { return x4_next; }
  void set_prev(GraveyardNode* p) { x0_prev = p; }
  void set_next(GraveyardNode* n) { x4_next = n; }
  GraveyardBucket* get_value() { return reinterpret_cast< GraveyardBucket* >(&x8_item); }
};
struct GraveyardList {
  uint x0_allocator; //!< rstl::rmemory_allocator, empty
  GraveyardNode* x4_start;
  GraveyardNode* x8_end;
  GraveyardNode* xc_empty_prev;
  GraveyardNode* x10_empty_next;
  uint x14_count;
};
CHECK_SIZEOF(GraveyardBucketView, 0x84) // == sizeof(rstl::reserved_vector<CEntity*, 32>)
CHECK_SIZEOF(GraveyardNode, 0x8C)       // 140 - the `li r3,140` in fn_8003C0C4
CHECK_SIZEOF(GraveyardList, 0x18)

extern "C" GraveyardNode* fn_8003C0C4(GraveyardList*, GraveyardNode* prev, GraveyardNode* next,
                                    GraveyardBucket* val) {
  // `allocate(0x8C)`, not rmemory_allocator::allocate(n, 1): the templated out-parameter
  // form gives `n` two definitions and MWCC spills r3 across the copy loop, which costs
  // 9 instructions out of 59. The single-definition form is byte-exact.
  GraveyardNode* n = reinterpret_cast< GraveyardNode* >(rstl::rmemory_allocator::allocate(0x8C));
  n->x0_prev = prev;
  n->x4_next = next;
  rstl::construct(n->get_value(), *val);
  return n;
}

extern "C" GraveyardNode* fn_8003C054(GraveyardList* l, GraveyardNode* n, GraveyardBucket* val) {
  GraveyardNode* const nn = fn_8003C0C4(l, n->get_prev(), n, val);
  if (n == l->x4_start) {
    l->x4_start = nn;
  }
  nn->get_prev()->set_next(nn);
  nn->get_next()->set_prev(nn);
  ++l->x14_count;
  return nn;
}

extern "C" void fn_8003C02C(rstl::list< rstl::reserved_vector< CEntity*, 32 > >& v, void* value) {
  fn_8003C054(reinterpret_cast< GraveyardList* >(&v),
              reinterpret_cast< GraveyardNode* >(v.end().get_node()),
              reinterpret_cast< GraveyardBucket* >(value));
}

// The script-ID map is an rstl red-black tree. Keep the tree view local until
// its lower/upper-bound template instantiations can be named in rstl itself.
struct ScriptIdNode {
  ScriptIdNode* left;
  ScriptIdNode* right;
  ScriptIdNode* parent;
  int color;
  CStateManager::TIdList::value_type value;
};
struct ScriptIdMapView {
  char header[8];
  ScriptIdNode* leftmost;
  ScriptIdNode* rightmost;
  ScriptIdNode* root;
};
CHECK_SIZEOF(ScriptIdNode, 0x18)
CHECK_SIZEOF(ScriptIdMapView, 0x14)

extern "C" ScriptIdNode* fn_8003C2D8(const CStateManager::TIdList& ids,
                                      const TEditorId& eid) {
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  ScriptIdNode* found = nullptr;
  while (cur) {
    if (!(cur->value.first < eid)) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  bool noResult = false;
  if (!found || eid < found->value.first) {
    noResult = true;
  }
  if (noResult) {
    return nullptr;
  }
  return found;
}

template < typename T >
inline T* ScriptIdNodeCast(ScriptIdNode* node, T*) {
  return reinterpret_cast< T* >(node);
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C420(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  ScriptIdNode* found = nullptr;
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  while (cur) {
    if (!(cur->value.first < eid)) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(found, it.mNode);
  return it;
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C46C(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  ScriptIdNode* found = nullptr;
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  while (cur) {
    if (eid < cur->value.first) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(found, it.mNode);
  return it;
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C28C(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(fn_8003C2D8(ids, eid), it.mNode);
  return CStateManager::TIdList::const_iterator(it);
}

TUniqueId CStateManager::GetIdForScript(TEditorId eid) const {
  TIdList::const_iterator it = fn_8003C28C(m_scriptIdMap, eid);
  if (it != m_scriptIdMap.end()) {
    return it->second;
  }
  return kInvalidUniqueId;
}

extern "C" CStateManager::TIdListResult fn_8003C3A8(const CStateManager::TIdList& ids,
                                                      const TEditorId& eid) {
  // Direct return, not a named local that is then copied out of. Retail interleaves each
  // store with its own load (`lwz r0,0x10(r1) ; stw r0,0(r29) ; ...`); a copy of a copy makes
  // MWCC hoist all four words into r3/r4/r5/r0 before storing any. 78.70% -> 100.00%.
  return CStateManager::TIdListResult(fn_8003C420(ids, eid), fn_8003C46C(ids, eid));
}

CStateManager::TIdListResult CStateManager::GetIdListForScript(TEditorId eid) const {
  const TIdListResult result = fn_8003C3A8(m_scriptIdMap, eid);
  return result;
}

CStateManager::CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&,
                             const rstl::ncrc_ptr< CMapWorldInfo >&,
                             const rstl::ncrc_ptr< CPlayerState >&,
                             const rstl::ncrc_ptr< CWorldTransManager >&)
: m_nextFreeIndex(0)
, m_bossId(kInvalidUniqueId)
, m_uid_setBySpecialFunc(kInvalidUniqueId)
, m_playerActorHead(kInvalidUniqueId)
, m_planes()
, mPendingDockArea(kInvalidAreaId)
, mPendingDock(0)
, mShowSoftTransition(true) {}

CStateManager::~CStateManager() {}

TUniqueId CStateManager::AllocateUniqueId() {

  const ushort lastIndex = m_nextFreeIndex;
  ushort ourIndex;
  queryOutput query;
  do {
    ourIndex = m_nextFreeIndex;
    m_nextFreeIndex = (ourIndex + 1) % 1024;
    if (m_nextFreeIndex == lastIndex) {
      rs_debugger_printf("Object list full!");
    }
    fn_80041518(query, mapWorldInfoAreas, ourIndex);
  } while ((query.unk4 & *query.unk0) != 0);

  m_objectIndexArray[ourIndex] = (m_objectIndexArray[ourIndex] + 1) & 0x3f;
  if (TUniqueId(m_objectIndexArray[ourIndex], ourIndex) == kInvalidUniqueId) {
    m_objectIndexArray[ourIndex] = 0;
  }

  fn_80041518(query, mapWorldInfoAreas, ourIndex);
  *query.unk0 = *query.unk0 | query.unk4;

  return TUniqueId(m_objectIndexArray[ourIndex], ourIndex);
}

const CEntity* CStateManager::GetObjectById(TUniqueId uid) const {
  return GetObjectListById(kOL_All).GetObjectById(uid);
}

void CStateManager::SetIsDarkWorld(bool b) {
  m_isDarkWorld = b;
  gpGameState->SetIsDarkWorld(m_isDarkWorld);
}

bool CStateManager::ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee, float damage, const TUniqueId& uid1, const TUniqueId& uid2, const CDamageInfo& damageInfo, int unkParam) {
  CHealthInfo* healthInfo = damagee.HealthInfo();
  if (!healthInfo || damage < 0.0f) {
    return false;
  }

  float hp = healthInfo->GetHP();
  if (hp <= 0.0f) {
    fn_8003dd88(damagee, uid1, damageInfo, false, unkParam);
    return true;
  }

  CPlayer* player = TCastToPtr< CPlayer >(damagee);

  if (player && player->Get_x12f8() != 0) {
    if (player->Get_x12f8() != 3) {
      return false;
    }
    player->fn_8000d3ac(pos, *this);
    if (!player->fn_8000d40c(dir, *this)) {
      return false;
    }
  }
  TUniqueId playerId = player ? player->GetUniqueId() : kInvalidUniqueId;
  if (player) {
    int playerIndex = MaskUIdNumPlayers(playerId);
    CPlayerState& playerState = *PlayerState(playerIndex);

    if (GetCameraManager(playerIndex)->IsInCinematicCamera()) {
      return false;
    }

    if (gpGameState->GetHardModeEnabled()) {
      switch ((EWeaponType) damageInfo.GetWeaponMode1()) {
        case kWT_Power:
        case kWT_Dark:
        case kWT_Light:
        case kWT_Annihilator:
        case kWT_Bomb:
        case kWT_PowerBomb:
        case kWT_Missile:
        case kWT_BoostBall:
        case kWT_CannonBall:
        case kWT_ScrewAttack:
        case kWT_AI:
        case kWT_PoisonWater1:
        case kWT_PoisonWater2:
        case kWT_Lava:
        case kWT_Heat:
        case kWT_Unused1:
        case kWT_AreaDark:
          damage *= gpGameState->GetHardModeDamageMultiplier();
          break;
      }
    }

    float damageReduction = 0.0f;

    if (playerState.HasPowerUp(CPlayerState::kIT_VariaSuit)) {
      damageReduction = player->GetTweakPlayer()->GetVariaSuitDamageReduction();
    }
    if (playerState.HasPowerUp(CPlayerState::kIT_DarkSuit)) {
      float reduction = player->GetTweakPlayer()->GetDarkSuitDamageReduction();
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.HasPowerUp(CPlayerState::kIT_LightSuit)) {
      float reduction = player->GetTweakPlayer()->GetLightSuitDamageReduction();
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_AbsorbAttack, true) != 0) {
      float reduction = 1.5f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_LightShield, true) != 0) {
      // TODO: flag
      float reduction = 0.75f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_DarkShield, true) != 0) {
      // TODO: flag
      float reduction = 0.75f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    hp = playerState.CalculateHealth();
    damage = -(damageReduction * damage - damage);
  }
}

// Retail: `lis r4,31 ; li r0,0 ; addi r4,r4,-31616 ; stw r4,0x24dc(r3) ; stw r0,0x15f8(r3) ;
// stw r0,0x15fc(r3) ; stw r0,0x1600(r3)`. The constant is 0x1E8480 = 2000000, and the three
// cleared slots are the cached mCurrentRenderPlayer / m_playerState / m_cameraManager pointers.
// Retail's symbol is unmangled (`nm` prints `fn_8003B21C`, not `fn_8003B21C__13CStateManagerFv`),
// so it is a free function taking the manager, like `fn_8003AD74` above - declaring it as a
// member emits the bytes correctly but under the wrong symbol and objdiff never pairs the two.
extern "C" void fn_8003B21C(CStateManager* mgr) {
  mgr->mCurrentRenderPlayerIndex = 2000000;
  mgr->mCurrentRenderPlayer = nullptr;
  mgr->m_playerState = nullptr;
  mgr->m_cameraManager = nullptr;
}

void CStateManager::fn_8003BF84(CEntity* ent) {
  // Clear Graveyard? Retail hands fn_8003C02C the address of a 4-byte stack slot holding a
  // zero, and it passes a DIFFERENT slot in each branch - `stw r0,0x8c(r1)` in the empty()
  // branch, `stw r0,0x8(r1)` in the size() == 32 branch. Declared at function scope we got
  // one slot reused by both and a `stwu r1,-160(r1)` frame, 85.50%; declared inside each `if`
  // body we get both slots and `stwu r1,-288(r1)`, 100.00%. The two objects' 44 instructions
  // already agreed one-for-one; the frame was the whole percentage.
  if (m_graveyard.empty()) {
    GraveyardBucket fresh;
    fn_8003C02C(m_graveyard, &fresh);
  } else if ((--m_graveyard.end())->size() == 32) {
    GraveyardBucket fresh;
    fn_8003C02C(m_graveyard, &fresh);
  }
  (--m_graveyard.end())->push_back(ent);
}

void CStateManager::fn_8003BE54() {
  while (!m_scriptMsgs.empty()) {
    CScriptMsg msg = m_scriptMsgs.fn_8019E6BC();
    CEntity* ent = GetObjectByIdFromListAll(msg.GetId());
    if (ent) {
      bool flag = ent->GetActive();
      ent->AcceptScriptMsg(*this, msg);
      if (flag != ent->GetActive()) {
        if (CActor* actor = TCastToPtr< CActor >(ent)) {
          UpdateActorInSortedLists(actor);
        }
      }
      if (msg.GetMessage() == kSM_XDelete) {
        fn_8003BF84(ent);
        fn_800412EC(ent->GetUniqueId());
      }
    }
  }
}

void CStateManager::DeferStateTransition(EStateManagerTransition t) {
  if (!fn_80036F10()) {
    if (t == kSMT_InGame) {
      if (m_deferredTransition != kSMT_InGame) {
        m_world->SetLoadPauseState(false);
        m_deferredTransition = kSMT_InGame;
      }
    } else if (m_deferredTransition == kSMT_InGame) {
      m_world->SetLoadPauseState(true);
      m_deferredTransition = t;
      if (m_deferredTransition == kSMT_Unk) {
        m_saveGameScreen =
            new CSaveGameScreen(kSC_InGame, gpGameState->GetCardSerial());
      }
    }
  }
}

void CStateManager::ShowPausedHUDMemo(CAssetId strg, float time) {
  m_hudMessageTime = time;
  m_pauseHudMessage = strg;
  DeferStateTransition(kSMT_MessageScreen);
}

void CStateManager::SendScriptMsg(const CScriptMsg& msg) {
  m_scriptMsgs.Append(msg);
  int v = m_scriptMsgs.fn_8019E69C();
  if (0x80 < v && !m_unkFlagB3) {
    m_unkFlagB3 = true;
    fn_8003BE54();
    m_unkFlagB3 = false;
  }
}

bool CStateManager::fn_80036F10() const {
  // The live CGameMode's type (vtable word 17), not CGameState's deserialised field of the same
  // name. The `result` spelling is what gives retail's shared epilogue for both compares.
  int v = gpGameState->GetGameMode().GetGameModeType();
  bool result = false;
  if (v != 'SNGL' && v != 'FRND') {
    result = true;
  }
  return result;
}

uint CStateManager::MaskUIdNumPlayers(TUniqueId id) const {
  uint index = id.Value();
  return index < m_numPlayers ? index : 0;
}

CEntity* CStateManager::ObjectById(TUniqueId uid) {
  return ObjectListById(kOL_All).GetObjectById(uid);
}

void CStateManager::AddObject(CEntity* entity) {
  if (entity) {
    AddObject(*entity);
  }
}

void CStateManager::DeleteObjectRequest(TUniqueId id) {
  SendScriptMsg(id, kInvalidUniqueId, kSM_XDelete, kInvalidUniqueId);
}

void CStateManager::SendScriptMsg(TUniqueId dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  // CScriptMsg's third id is the destination (DeliverScriptMsg resolves ObjectById(GetId())).
  SendScriptMsg(CScriptMsg(src, other, dest, msg, kSS_InvalidState));
}

void CStateManager::SendScriptMsg(CEntity* dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  if (dest) {
    SendScriptMsg(
        CScriptMsg(src, other, dest->GetUniqueId(), msg, kSS_InvalidState));
  }
}

void CStateManager::SetupParticleHook(const CActor& actor) const {
  m_actorModelParticles->SetupHook(actor.GetUniqueId());
}

void CStateManager::BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CVector3f& pos, const CVector3f& dir,
                                  float mag, const CMaterialFilter& filter,
                                  const CActor* actor) const {
  m_sortedListManager->BuildNearList(out, pos, dir, mag, filter, actor);
}

void CStateManager::BuildColliderList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CActor& actor,
                                      const CAABox& aabb) const {
  m_sortedListManager->BuildNearList(out, actor, aabb);
}

void CStateManager::BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CAABox& aabb,
                                  const CMaterialFilter& filter, const CActor* actor) const {
  m_sortedListManager->BuildNearList(out, aabb, filter, actor);
}

bool CStateManager::RayCollideWorld(const CVector3f& start, const CVector3f& end,
                                    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CMaterialFilter& filter,
                                    const CActor* damagee) const {
  return RayCollideWorldInternal(start, end, filter, nearList, damagee);
}

CRayCastResult CStateManager::RayStaticIntersection(const CVector3f& pos, const CVector3f& dir,
                                                    float length,
                                                    const CMaterialFilter& filter) const {
  return CGameCollision::RayStaticIntersection(*this, pos, dir, length, filter);
}

CRayCastResult CStateManager::RayWorldIntersection(TUniqueId& idOut, const CVector3f& pos,
                                                   const CVector3f& dir, float length,
                                                   const CMaterialFilter& filter,
                                                   const rstl::reserved_vector< TUniqueId, 1024 >& list) const {
  return CGameCollision::RayWorldIntersection(*this, idOut, pos, dir, length, filter, list);
}

void CStateManager::QueueMessage(int frameCount, CAssetId msg, float f1) {
  m_forPausedHudMemo = frameCount;
  m_pausedHudMemoAssetId = msg;
  x2468 = f1;
}

void CStateManager::SetBossParams(TUniqueId bossId, float maxEnergy, uint stringIdx) {
  m_bossId = bossId;
  m_bossHealth = maxEnergy;
  m_bossLanguageTableIndex = stringIdx;
}

void CStateManager::fn_80039244() {}

void CStateManager::fn_8003FF1C() {}

void CStateManager::fn_8003FF20() {}

bool CStateManager::fn_8003FF24() {
  CFrameDelayedKiller::StallAndFlushAllAllocations();
  fn_8030184C();
  CARAMToken::UpdateAllDMAs();
  return true;
}

void CStateManager::fn_8003FF50() { fn_8003FF24(); }

void CStateManager::fn_8003FF70(int, int) {}

void CStateManager::fn_8003FF74(int value) {
  mRenderFrameIndex = value;
  lbl_80419A18 = mRenderFrameIndex;
  lbl_80419A10 = mRenderFrameIndex;
  fn_8003FF70(2, 0x180000);
}

void CStateManager::fn_800419C8() {}

bool CStateManager::fn_800421B4() const { return m_world != nullptr; }

int CStateManager::fn_80036B6C() const {
  if (m_numPlayers == 1u) {
    return 0;
  }
  int ret = 2;
  if (m_numPlayers == 2u) {
    ret = 1;
  }
  return ret;
}

int CStateManager::GetWeaponIdCount(TUniqueId id, EWeaponType type) {
  return m_weaponMgr->GetNumActive(id, type);
}

void CStateManager::AddWeaponId(TUniqueId id, EWeaponType type) {
  m_weaponMgr->fn_800B321C(id, type);
}

void CStateManager::RemoveWeaponId(TUniqueId id, EWeaponType type) {
  m_weaponMgr->fn_800B32E0(id, type);
}

void CStateManager::fn_8003A3C0(int& a, int& b, int type) const {
  int shift = 1;
  if (type == 3) {
    shift = 2;
  }
  a = 1 << shift;
  b = 0;
}

CScriptObjectLoaderHelper& CStateManager::fn_80036200() {
  return m_stateManagerContainer->ScriptObjectLoaderHelper();
}

CScriptObjectLoaderHelper& CStateManager::ScriptObjectLoaderHelper() {
  return m_stateManagerContainer->ScriptObjectLoaderHelper();
}

rstl::single_ptr< CPortalTransition >& CStateManager::fn_80036220() { return mPortalTransition; }

void CStateManager::SetPortalTransition(rstl::single_ptr< CPortalTransition >& ptr) { mPortalTransition = ptr; }

bool CStateManager::fn_80036284() {
  for (CGameArea::CConstChainIterator it = m_world->GetChainHead(CWorld::kC_Alive);
       it != CWorld::GetAliveAreasEnd(); ++it) {
    if (it->HasPendingLayerLoads()) {
      return true;
    }
  }
  return false;
}

// Render flags derived from the active visor; defined outside this unit's split.
extern "C" uint lbl_80419A9C;
extern "C" uint lbl_80419AA0;

void CStateManager::fn_80036650() {
  CPlayerState::EPlayerVisor visor = m_playerState->GetActiveVisor(*this);
  uint flagsA = 0;
  uint flagsB = 8;
  switch (visor) {
  case CPlayerState::kPV_Echo:
    flagsA |= 8;
    break;
  case CPlayerState::kPV_Combat:
  case CPlayerState::kPV_Scan:
    flagsB |= 1;
    break;
  case CPlayerState::kPV_Dark:
    flagsB |= 2;
    break;
  }
  uint flagsC = m_isDarkWorld ? flagsB | 4 : flagsB | 16;
  lbl_80419A9C = flagsA;
  lbl_80419AA0 = flagsC;
}

void CStateManager::fn_800362E0() {
  for (CGameArea::CChainIterator it = m_world->ChainHead(CWorld::kC_Alive); it != CWorld::AliveAreasEnd(); ++it) {
    it->UpdateDynamicLayers(*this);
  }
}

bool CStateManager::fn_80037904(TUniqueId id) {
  CStateManagerContainer::TIdList& list = m_stateManagerContainer->IdList13ED8();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037944(TUniqueId id) {
  CStateManagerContainer::TIdList& list = m_stateManagerContainer->IdList13F88();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037984(TUniqueId id) {
  CStateManagerContainer::TIdList& list = m_stateManagerContainer->IdList13F5C();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_800379C4(TUniqueId id) {
  CStateManagerContainer::TIdList& list = m_stateManagerContainer->IdList13F30();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037A04(TUniqueId id) {
  CStateManagerContainer::TIdList& list = m_stateManagerContainer->IdList13F04();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

void CStateManager::fn_8003EC0C() {
  // Retail's `FrameDone__6CModelFv` (0x803111A4) reached through its own
  // `CModel::FrameDone`, which is `src/Kyoto/Graphics/CModelPortStub.cpp` in the port - see
  // that file for why the host body is empty. This used to be called through the `fn_`
  // placeholder name, which no translation unit defines.
  CModel::FrameDone();
  gpSimplePool->Flush();
}

void CStateManager::fn_8003F970(CEntity* entity, float dt) { entity->Think(dt, *this); }

uint CStateManager::fn_800368E4(uint single, uint multi) const {
  return fn_80036F10() ? multi : single;
}

void CStateManager::AddDrawableActorPlane(const CActor& actor, const CPlane& plane,
                                          const CAABox& bounds) const {
  const_cast< CActor& >(actor).SetAddedToken(m_objectDrawToken + 1);
  gpRender->AddPlaneObject(&actor, bounds, plane, 0);
}

void CStateManager::AddDrawableActor(const CActor& actor, const CVector3f& pos,
                                     const CAABox& bounds) const {
  const_cast< CActor& >(actor).SetAddedToken(m_objectDrawToken + 1);
  gpRender->AddDrawable(&actor, pos, bounds, 0,
                        IRenderer::EDrawableSorting(actor.GetAlphaSorted()));
}

bool CStateManager::CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const {
  return m_weaponMgr->GetNumActive(id, type) < max;
}

void CStateManager::DeliverScriptMsg(const CScriptMsg& msg) {
  CEntity* entity = ObjectById(msg.GetId());
  if (entity) {
    entity->AcceptScriptMsg(*this, msg);
  }
}

void CStateManager::fn_80037784() {
  m_saveGameScreen = rs_new CSaveGameScreen(kSC_FrontEnd, gpGameState->GetCardSerial());
}

void CStateManager::KillSaveGameInterface() {
  m_unkFlagA5 = m_saveGameScreen->GetIowRet() == 1;
  m_saveGameScreen = nullptr;
}

float CStateManager::fn_80038364() { return gpGameState->GetEscapeTime(); }

void CStateManager::fn_80038370(float value) {
  gpGameState->SetEscapeTime(value);
  mEscapeTotalTime = value;
}

// Retail's `TouchSky__6CWorldCFv` (0x8004F770) reached through its own `CWorld::TouchSky`,
// which is `src/MetroidPrime/CWorldTouchSky.cpp` in the port. It used to be declared here as
// the placeholder `extern "C" void fn_8004F770(CWorld*)` and called through it, which asked the
// linker for a `fn_` symbol that no translation unit defines; the named method is the same
// function and is defined.
void CStateManager::TouchSky() { m_world->TouchSky(); }

float CStateManager::fn_80036F78(float value) {
  CPlayerState* playerState = m_playerState;
  if (playerState->GetActiveVisor(*this) == CPlayerState::kPV_Scan) {
    return value * (1.f - playerState->GetVisorTransitionFactor());
  }
  return value;
}

void CStateManager::TouchPlayerActor() {
  // By reference, so the compare and the argument are ONE load. Retail is
  // `lhz r4,0x2452(r3) ; cmplw r4,r0 ; beq ; sth r4,0x8(r1)`. Re-reading the member after
  // the branch emitted a second `lhz r0,0x2452(r31)` and measured 85.48%; this measures
  // 100.00%.
  const TUniqueId& head = m_playerActorHead;
  if (head != kInvalidUniqueId) {
    if (const CEntity* entity = GetObjectById(head)) {
      ::TouchPlayerActor(const_cast< CEntity& >(*entity), *this);
    }
  }
}

void CStateManager::fn_80039CCC(int pass) {
  if (x2944 > 0.f) {
    switch (pass) {
    case 0:
    case 2:
      gpRender->DrawDarkWorldCloud(x2944, x2938, x2948);
      break;
    case 1:
      break;
    }
  }
}

void CStateManager::fn_80039DDC(const TAreaId& area, int type, int mask, int targetMask) {
  switch (type) {
  case 1:
    break;
  case 2:
    gpRender->DrawSpecialGeometryAlpha(area.Value(), mask, targetMask);
    break;
  default:
    gpRender->DrawSpecialGeometry(area.Value(), mask, targetMask);
    break;
  }
}

TEditorId CStateManager::GetEditorIdForUniqueId(TUniqueId id) const {
  const CEntity* entity = GetObjectById(id);
  if (entity) {
    return entity->GetEditorId();
  }
  return kInvalidEditorId;
}
