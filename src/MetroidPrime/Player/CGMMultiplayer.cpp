#include "MetroidPrime/Player/CGMMultiplayer.hpp"

#include "Kyoto/Streams/COutputStream.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameModeListener.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpawnPoint.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/vector.hpp"

CGMMultiplayer::CGMMultiplayer(float timeLimit, bool flag)
: mTimeLimit(timeLimit)
, mElapsedTime(0.f)
, mMusicIndex(0)
, mSpawnPoints(4, kInvalidUniqueId)
, mResultIndex(-1)
, x34_24_(flag)
, mGameOver(false) {}

void CGMMultiplayer::PutTo(COutputStream& out) const {
  out.WriteBool(x34_24_);
  out.WriteReal32(mTimeLimit);
}

void CGMMultiplayer::OnPlayerDamaged(CStateManager& mgr, TUniqueId victim, TUniqueId attacker,
                                     float damage) {
  uint victimIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(victim)) != nullptr) {
    victimIndex = mgr.MaskUIdNumPlayers(victim);
  }
  uint attackerIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(attacker)) != nullptr) {
    attackerIndex = mgr.MaskUIdNumPlayers(attacker);
  }

  NotifyListeners(mgr, attackerIndex, victimIndex, kGE_Damage, &damage);
}

void CGMMultiplayer::NotifyStop(CStateManager& mgr, TUniqueId player) {
  uint playerIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(player)) != nullptr) {
    playerIndex = mgr.MaskUIdNumPlayers(player);
  }
  NotifyListeners(mgr, playerIndex, playerIndex, kGE_Stop, nullptr);
}

void CGMMultiplayer::OnPlayerKilled(CStateManager& mgr, TUniqueId victim, TUniqueId killer) {
  uint victimIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(victim)) != nullptr) {
    victimIndex = mgr.MaskUIdNumPlayers(victim);
  }
  uint killerIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(killer)) != nullptr) {
    killerIndex = mgr.MaskUIdNumPlayers(killer);
  }

  NotifyListeners(mgr, victimIndex, killerIndex, kGE_Score, nullptr);
  NotifyListeners(mgr, killerIndex, victimIndex, kGE_Kill, nullptr);
}

void CGMMultiplayer::OnPlayerScanned(CStateManager& mgr, TUniqueId target, TUniqueId scanner) {
  uint targetIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(target)) != nullptr) {
    targetIndex = mgr.MaskUIdNumPlayers(target);
  }
  uint scannerIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(scanner)) != nullptr) {
    scannerIndex = mgr.MaskUIdNumPlayers(scanner);
  }

  NotifyListeners(mgr, scannerIndex, targetIndex, kGE_Scan, nullptr);
}

void CGMMultiplayer::NotifyGenericEvent(CStateManager& mgr, TUniqueId player, uint value) {
  uint playerIndex = -1;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(player)) != nullptr) {
    playerIndex = mgr.MaskUIdNumPlayers(player);
  }
  NotifyListeners(mgr, playerIndex, playerIndex, kGE_Generic, &value);
}

void CGMMultiplayer::AddListener(CGameModeListener& listener, uint playerIndex) {
  mListeners.insert(TListener(playerIndex, &listener));
}

void CGMMultiplayer::RemoveListener(CGameModeListener& listener, uint playerIndex) {
  mListeners.erase(TListener(playerIndex, &listener));
}

void CGMMultiplayer::NotifyListeners(CStateManager& mgr, uint sourceIndex, uint targetIndex,
                                     EGameEvent event, const void* value) {
  if (targetIndex == uint(-1)) {
    return;
  }
  for (rstl::set< TListener >::const_iterator it = mListeners.begin();
       it != mListeners.end(); ++it) {
    if (it->first == targetIndex) {
      it->second->OnGameEvent(mgr, sourceIndex, targetIndex, event, value);
    }
  }
}

TUniqueId CGMMultiplayer::ChooseSpawnPoint(CStateManager& mgr, uint playerIndex,
                                           TUniqueId requested) {
  CObjectList& objects = mgr.ObjectListById(kOL_All);
  rstl::vector< TUniqueId > candidates;
  candidates.reserve(16);
  for (int index = objects.GetFirstObjectIndex(); index != -1;
       index = objects.GetNextObjectIndex(index)) {
    const CScriptSpawnPoint* spawn = TCastToPtr< CScriptSpawnPoint >(objects[index]);
    if (spawn == nullptr || (requested != kInvalidUniqueId && spawn->GetUniqueId() != requested)) {
      continue;
    }

    const CVector3f position = spawn->GetTransform().GetTranslation();
    float nearestDistance = 1000000.f;
    // `static_cast<uint>` so the loop test is the unsigned compare retail emits (`cmplw r22,r3`),
    // against the signed `cmpw`; the counter stays `int` so MWCC strength-reduces the players array
    // into the pointer induction retail has.
    for (int player = 0; static_cast<uint>(player) < GetNumPlayers(); ++player) {
      if (player != playerIndex) {
        const float distance = (mgr.GetPlayer(player)->GetTranslation() - position).Magnitude();
        if (distance < nearestDistance) {
          nearestDistance = distance;
        }
      }
    }
    if (!(nearestDistance < 7.f) && spawn->GetActive() && spawn->IsFirstSpawn()) {
      // The growth is spelled out here rather than left to `push_back` because retail's own
      // `push_back` is inlined *without* its zero-capacity fallback at this one site
      // (0x80196C80: `lwz cap ; lwz count ; cmpw ; bne ; addi r3,&vec ; slwi r4,cap,1 ; bl reserve`)
      // while the library's copy keeps it (0x800E9618) - MWCC folds the `cmpwi cap,0 / li 4 / beq`
      // away there and not here, and this tree does not reproduce that fold. Dropping the fallback
      // from the shared `rstl::vector` would match the bytes by deleting retail's own guard; it is
      // dead at *this* call site because `candidates` was `reserve(16)`'d above, so `capacity()` is
      // never 0 and `capacity() * 2` never is either.
      if (candidates.capacity() == candidates.size()) {
        candidates.reserve(candidates.capacity() * 2);
      }
      // A copy into a local, not a reference: retail reads the id out of the member after the
      // growth call (`lhz r5,8(r29)`, 0x80196CA0), and MWCC sinks a copy of a bare reference into
      // the scratch register the store wants instead.
      const TUniqueId value = spawn->GetUniqueIdRef();
      candidates.push_back_unsafe(value);
    }
  }

  if (candidates.empty()) {
    return kInvalidUniqueId;
  }
  return candidates[mgr.Random()->Range(0, candidates.size() - 1)];
}

void CGMMultiplayer::RespawnPlayer(CStateManager& mgr, uint playerIndex) {
  const uint idx = playerIndex;
  const TUniqueId spawnId = ChooseSpawnPoint(mgr, playerIndex, mSpawnPoints[idx]);
  if (spawnId == kInvalidUniqueId) {
    return;
  }

  CScriptSpawnPoint& spawn = static_cast< CScriptSpawnPoint& >(*mgr.ObjectById(spawnId));
  CPlayerState& state = *mgr.PlayerState(playerIndex);
  CPlayer& player = *mgr.Player(playerIndex);
  const CPlayerState::SPersistentState persistent = state.GetPersistentState();
  state = CPlayerState(playerIndex, nullptr);
  for (int i = 0; i < CPlayerState::kIT_Max; ++i) {
    const CPlayerState::EItemType item = CPlayerState::EItemType(i);
    if (state.GetItemCapacity2(item) != spawn.GetItemCapacity(item)) {
      state.AddPowerUp(item, spawn.GetItemCapacity(item) - state.GetItemCapacity2(item));
    }
    if (state.GetItemAmount(item) != spawn.GetItemAmount(item)) {
      state.IncrPickUp(item, spawn.GetItemAmount(item) - state.GetItemAmount(item));
    }
  }
  state.SetPersistentState(persistent);

  const CVector3f position = spawn.GetTransform().GetTranslation();
  CVector3f forward = spawn.GetTransform().GetForward();
  forward.SetZ(0.f);
  if (forward.CanBeNormalized()) {
    player.Teleport(CTransform4f::LookAt(position, position + forward, CVector3f::Up()), mgr, true);
  }
  player.AsyncLoadSuit(mgr);
  player.SetSpawnedMorphBallState(spawn.IsMorphed() ? CPlayer::kMS_Morphed : CPlayer::kMS_Unmorphed,
                                  mgr);
  player.fn_80019E40(mgr, 1);
  spawn.SendSpawnMessage(mgr, player);
  NotifyListeners(mgr, playerIndex, playerIndex, kGE_Spawn, nullptr);
}

void CGMMultiplayer::OnPlayerSpawned(CStateManager& mgr, uint playerIndex) {
  NotifyListeners(mgr, playerIndex, playerIndex, kGE_Spawn, nullptr);
}

void CGMMultiplayer::SetSpawnPoint(uint playerIndex, TUniqueId spawnPoint) {
  mSpawnPoints[playerIndex] = spawnPoint;
}

bool CGMMultiplayer::v21() const { return x34_24_; }

void CGMMultiplayer::EndGame(int resultIndex, CStateManager&) {
  mGameOver = true;
  mResultIndex = resultIndex;
}

int CGMMultiplayer::GetResultIndex() const { return mResultIndex; }

bool CGMMultiplayer::IsGameOver() { return mGameOver; }

void CGMMultiplayer::UpdateTimer(float dt, CStateManager& mgr) {
  if (!mgr.GetCameraManager(0)->fn_801ABD68()) {
    mElapsedTime += dt;
  }
}

bool CGMMultiplayer::IsMultiplayer() const { return true; }

void CGMMultiplayer::SetMusicIndex(int musicIndex) { mMusicIndex = musicIndex; }
