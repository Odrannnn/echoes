#include "MetroidPrime/ScriptObjects/CScriptSpawnPoint.hpp"

#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "rstl/math.hpp"

CScriptSpawnPoint::CScriptSpawnPoint(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CTransform4f& xf,
    const rstl::reserved_vector< int, int(CPlayerState::kIT_Max) >& amountForItem,
    const rstl::reserved_vector< int, int(CPlayerState::kIT_Max) >& capacityForItem,
    bool firstSpawn, bool isMorphed)
: CEntity(uid, info, name, 0)
, m_xf(xf)
, m_amountForItem(amountForItem)
, m_capacityForItem(capacityForItem)
, m_firstSpawn(firstSpawn)
, m_morphed(isMorphed) {}

CScriptSpawnPoint::~CScriptSpawnPoint() {}

const CTransform4f& CScriptSpawnPoint::GetTransform() const { return m_xf; }

int CScriptSpawnPoint::GetItemAmount(CPlayerState::EItemType type) const {
  if (CPlayerState::kIT_Max <= type || type < 0) {
    return m_amountForItem.front();
  }
  return m_amountForItem[type];
}

int CScriptSpawnPoint::GetItemCapacity(CPlayerState::EItemType type) const {
  if (CPlayerState::kIT_Max <= type || type < 0) {
    return m_amountForItem.front();
  }
  return rstl::max_val(m_amountForItem[type], m_capacityForItem[type]);
}

void CScriptSpawnPoint::SendSpawnMessage(CStateManager& mgr, CEntity& player) {
  SendScriptMsgs(kSS_Zero, mgr, player.GetUniqueId(), kSM_None);
}

void CScriptSpawnPoint::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  CEntity::AcceptScriptMsg(mgr, msg);

  switch (message) {
  case kSM_Reset:
    for (int playerIndex = 0; playerIndex < static_cast< uint >(mgr.GetNumPlayers());
         ++playerIndex) {
      for (int i = 0; i < CPlayerState::kIT_Max; ++i) {
        const CPlayerState::EItemType e = CPlayerState::EItemType(i);
        mgr.PlayerState(playerIndex)->ReInitializePowerUp(e, GetItemCapacity(e));
        mgr.PlayerState(playerIndex)->ResetAndIncrPickUp(e, GetItemAmount(e));
      }
    }
  case kSM_SetToZero:
    if (GetActive()) {
      for (int playerIndex = 0; playerIndex < static_cast< uint >(mgr.GetNumPlayers());
           ++playerIndex) {
        CPlayer* player = mgr.Player(playerIndex);
        TAreaId thisAreaId = GetCurrentAreaId();
        TAreaId nextAreaId = mgr.GetNextAreaId();

        if (nextAreaId != thisAreaId) {
          bool propagateAgain = false;

          CGameArea* area = mgr.World()->Area(thisAreaId);
          CGameArea::EOcclusionState occlusionState = area->GetOcclusionState();

          if (occlusionState == CGameArea::kOS_Occluded) {
            while (!area->TryTakingOutOfARAM()) {
            }
            CWorld::PropogateAreaChain(CGameArea::kOS_Visible, area, mgr.World());
            propagateAgain = true;
          }

          mgr.SetCurrentAreaId(thisAreaId);
          mgr.SetActorAreaId(*player, thisAreaId);
          player->Teleport(m_xf, mgr, false);
          player->SetSpawnedMorphBallState(
              m_morphed ? CPlayer::kMS_Morphed : CPlayer::kMS_Unmorphed, mgr);

          if (propagateAgain) {
            CWorld::PropogateAreaChain(CGameArea::kOS_Occluded, mgr.World()->Area(nextAreaId),
                                       mgr.World());
          }

        } else {
          player->Teleport(m_xf, mgr, false);
          player->SetSpawnedMorphBallState(
              m_morphed ? CPlayer::kMS_Morphed : CPlayer::kMS_Unmorphed, mgr);
        }

        if (player->GetCameraManager()->IsInCinematicCamera()) {
          player->fn_80019E40(mgr, 0);
        } else {
          player->fn_80019E40(mgr, 1);
        }
      }
      CEntity::SendScriptMsgs(kSS_Zero, mgr, kInvalidUniqueId, kSM_None);
    }
  }
}

// Retail's 124 bytes at 0x800B9C60, unnamed in the symbol table, are
// `rstl::reserved_vector<int, CPlayerState::kIT_Max>::resize` and are called only from
// `LoadSpawnPoint` below. Retail emits the instantiation under an unmangled name, so objdiff never
// pairs it with the mangled symbol the compiler writes for us; it is therefore written out here
// under the retail name, for the same reason `rstl::reserved_vector`'s `operator=` is (see
// `include/rstl/reserved_vector.hpp` and `src/MetroidPrime/Player/CGameState.cpp`). The statements
// are the header's `resize`, so the code is the same code; only the symbol is retail's. The header
// is untouched, so no other unit's bytes move.
//
// The header's `resize` is inline, so the body is spelled out rather than delegated to it: mwcceppc
// emits it as a weak out-of-line copy and tail-calls it, which is 5 bytes of prologue retail has no
// counterpart for (measured: 17.06% with `self->resize(...)`, 100.00% with the body written out).
// Every instruction is retail's, including the `srwi. r7,3` / eight-wide `stw` unroll that
// `uninitialized_fill_n` produces.
extern "C" void fn_800B9C60(
    rstl::reserved_vector< int, int(CPlayerState::kIT_Max) >* self, int count, const int& value) {
  if (self->mCount == count) {
    return;
  }
  if (self->mCount <= count) {
    rstl::uninitialized_fill_n(self->data() + self->mCount, count - self->mCount, value);
  }
  self->mCount = count;
}

CEntity* LoadSpawnPoint(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  
}
