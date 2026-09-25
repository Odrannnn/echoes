#include "MetroidPrime/CStateManager.hpp"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorModelParticles.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CGameCollision.hpp"
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

#include "rstl/vector.hpp"

extern "C" void fn_8030184C();
extern "C" void fn_803111A4();
extern "C" int lbl_80419A10;
extern "C" int lbl_80419A18;

void TouchPlayerActor(CEntity& ent, CStateManager& mgr);

struct queryOutput {
  int* unk0;
  int unk4;
};

void fn_80041518(queryOutput&, MapWorldInfoAreas& mapWorldInfoAreas, ushort ourIndex);
void fn_8003C02C(rstl::list< rstl::reserved_vector< CEntity*, 32 > >& v, int);

CStateManager::CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&,
                             const rstl::ncrc_ptr< CMapWorldInfo >&,
                             const rstl::ncrc_ptr< CPlayerState >&,
                             const rstl::ncrc_ptr< CWorldTransManager >&)
: m_nextFreeIndex(0), m_bossId(kInvalidUniqueId), m_uid_setBySpecialFunc(kInvalidUniqueId),
  m_playerActorHead(kInvalidUniqueId),
m_planes() {}

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
  return GetObjectListById(kOL_All).fn_8000B538(uid);
}

void CStateManager::SetIsDarkWorld(bool b) {
  m_isDarkWorld = b;
  gpGameState->SetIsDarkWorld(m_isDarkWorld);
}

bool CStateManager::ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee, float damage, const TUniqueId& uid1, const TUniqueId& uid2, const CDamageInfo& damageInfo, int unkParam) {
  CHealthInfo* healthInfo = damagee.HealthInfo(*this);
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

void CStateManager::fn_8003BF84(CEntity* ent) {
  // Clear Graveyard?
  if (m_graveyard.empty()) {
    fn_8003C02C(m_graveyard, 0);
  } else if ((--m_graveyard.end())->size() == 32) {
    fn_8003C02C(m_graveyard, 0);
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
            new CSaveGameScreen(1, gpGameState->GetCardSerial());
      }
    }
  }
}

void CStateManager::ShowPausedHUDMemo(CAssetId strg, float time) {
  m_hudMessageTime = time;
  m_pauseHudMessage = strg;
  DeferStateTransition(kSMT_MessageScreen);
}

void CStateManager::SendScriptMsg_fn_80037100(const CScriptMsg& msg) {
  m_scriptMsgs.Append(msg);
  int v = m_scriptMsgs.fn_8019E69C();
  if (0x80 < v && !m_unkFlagB3) {
    m_unkFlagB3 = true;
    fn_8003BE54();
    m_unkFlagB3 = false;
  }
}

bool CStateManager::fn_80036F10() const {
  int v = gpGameState->GetGameMode().v15();
  return v != 'SNGL' && v != 'FRND';
}

uint CStateManager::MaskUIdNumPlayers(TUniqueId id) const {
  uint index = id.Value();
  return index < m_numPlayers ? index : 0;
}

CEntity* CStateManager::ObjectById(TUniqueId uid) {
  return ObjectListById(kOL_All).fn_8000B588(uid);
}

void CStateManager::AddObject(CEntity* entity) {
  if (entity) {
    AddObject(*entity);
  }
}

void CStateManager::DeleteObjectRequest(TUniqueId id) {
  DeliverScriptMsg(id, kInvalidUniqueId, kSM_XDelete, kInvalidUniqueId);
}

void CStateManager::DeliverScriptMsg(TUniqueId dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  SendScriptMsg_fn_80037100(CScriptMsg(src, dest, other, msg, kSS_InvalidState));
}

void CStateManager::DeliverScriptMsg(CEntity* dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  if (dest) {
    SendScriptMsg_fn_80037100(
        CScriptMsg(src, dest->GetUniqueId(), other, msg, kSS_InvalidState));
  }
}

void CStateManager::SetupParticleHook(const CActor& actor) const {
  m_actorModelParticles->SetupHook(actor.GetUniqueId());
}

void CStateManager::BuildNearList(TEntityList& out, const CVector3f& pos, const CVector3f& dir,
                                  float mag, const CMaterialFilter& filter,
                                  const CActor* actor) const {
  m_sortedListManager->BuildNearList(out, pos, dir, mag, filter, actor);
}

void CStateManager::BuildColliderList(TEntityList& out, const CActor& actor,
                                      const CAABox& aabb) const {
  m_sortedListManager->BuildColliderList(out, actor, aabb);
}

void CStateManager::BuildNearList(TEntityList& out, const CAABox& aabb,
                                  const CMaterialFilter& filter, const CActor* actor) const {
  m_sortedListManager->BuildNearList(out, aabb, filter, actor);
}

bool CStateManager::RayCollideWorld(const CVector3f& start, const CVector3f& end,
                                    const TEntityList& nearList, const CMaterialFilter& filter,
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
                                                   const TEntityList& list) const {
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
  x16a8 = value;
  lbl_80419A18 = x16a8;
  lbl_80419A10 = x16a8;
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

int CStateManager::fn_80037F90(TUniqueId id, EWeaponType type) {
  return m_weaponMgr->GetNumActive(id, type);
}

void CStateManager::fn_80037FC0(TUniqueId id, EWeaponType type) {
  m_weaponMgr->fn_800B321C(id, type);
}

void CStateManager::fn_80037FF0(TUniqueId id, EWeaponType type) {
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

CStateManagerContainerUnk13EC0& CStateManager::fn_80036200() {
  return m_stateManagerContainer->Unk13EC0();
}

const CStateManagerContainerUnk13EC0& CStateManager::fn_80036210() const {
  return m_stateManagerContainer->GetUnk13EC0();
}

rstl::single_ptr< CStateManagerUnk2900 >& CStateManager::fn_80036220() { return x2900; }

void CStateManager::fn_80036228(rstl::single_ptr< CStateManagerUnk2900 >& ptr) { x2900 = ptr; }

bool CStateManager::fn_80036284() {
  for (CGameArea::CConstChainIterator it = m_world->GetChainHead();
       it != CWorld::GetAliveAreasEnd(); ++it) {
    if (it->fn_80057550()) {
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
  for (CGameArea::CChainIterator it = m_world->ChainHead(); it != CWorld::AliveAreasEnd(); ++it) {
    it->fn_800575BC(*this);
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
  fn_803111A4();
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
                        IRenderer::EDrawableSorting(actor.GetSortedDrawCallback()));
}

bool CStateManager::CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const {
  return m_weaponMgr->GetNumActive(id, type) < max;
}

void CStateManager::DeliverScriptMsgImmediate(const CScriptMsg& msg) {
  CEntity* entity = ObjectById(msg.GetId());
  if (entity) {
    entity->AcceptScriptMsg(*this, msg);
  }
}

void CStateManager::fn_80037784() {
  m_saveGameScreen = rs_new CSaveGameScreen(0, gpGameState->GetCardSerial());
}

void CStateManager::KillSaveGameInterface() {
  m_unkFlagA5 = m_saveGameScreen->GetUnk80() == 1;
  m_saveGameScreen = nullptr;
}

float CStateManager::fn_80038364() { return gpGameState->GetUnk50(); }

void CStateManager::fn_80038370(float value) {
  gpGameState->SetUnk50(value);
  x2438_escapeTotalTime = value;
}

float CStateManager::fn_80036F78(float value) {
  CPlayerState* playerState = m_playerState;
  if (playerState->GetActiveVisor(*this) == CPlayerState::kPV_Scan) {
    return value * (1.f - playerState->GetVisorTransitionFactor());
  }
  return value;
}

void CStateManager::TouchPlayerActor() {
  if (m_playerActorHead != kInvalidUniqueId) {
    if (const CEntity* entity = GetObjectById(m_playerActorHead)) {
      ::TouchPlayerActor(const_cast< CEntity& >(*entity), *this);
    }
  }
}

void CStateManager::fn_80039CCC(int pass) {
  if (x2944 > 0.f) {
    switch (pass) {
    case 0:
    case 2:
      gpRender->UnkL(x2938, x2948);
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
    gpRender->UnkB(area.Value(), mask, targetMask);
    break;
  default:
    gpRender->DrawStaticGeometry(area.Value(), mask, targetMask);
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
