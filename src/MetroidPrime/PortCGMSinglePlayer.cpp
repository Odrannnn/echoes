// Port-only: retail's `CGMSinglePlayer` (ctor 0x80193E08, dtor 0x80193BD4, virtuals
// 0x80193C30-0x80193E04, vtable 0x803B5CB0). Retail's code lives in unsplit text ranges, so it
// cannot be a configure.py unit; the port needs a real object because `CGameState`'s
// constructor does `rs_new CGMSinglePlayer` and `CMainFlow::SetGameState` then calls
// `GetGameMode().GetGameModeType()` through its vtable. With the ctor reach-stubbed that
// object was 12 uninitialised bytes and the call faulted on a garbage vtable.
//
// Every body below is read from the disassembly (the one-instruction virtuals from main.dol):
//   0x80193C30 GetNumPlayers   li r3,1
//   0x80193C38 GetGameModeType 'SNGL' (0x534E474C)
//   0x80193C44 GetElapsedTime / 0x80193C4C GetMatchTimeLimit: lbl_8041CB08 (-1.0f)
//   0x80193C84 GetResultIndex  x8_;  0x80193DA4 IsGameOver  x5_
//   0x80193DAC OnPlayerSpawned x4_ = true
//   0x80193DC4 Update          if (player 0 IsPlayerDeadEnough) x5_ = true
//   0x80193C8C EndGame         see below
// PutTo, OnPlayerKilled, NotifyGenericEvent, RespawnPlayer, SetSpawnPoint, OnPlayerDamaged,
// NotifyStop, GiveScore, AddListener and RemoveListener are bare `blr`; IsMultiplayer,
// GetItemAmount, IsNearScoreLimit and v21 return 0.
#include "MetroidPrime/Player/CGMSinglePlayer.hpp"

#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

// 0x80193D58: the completion tier of player state 0 (0x150c), <75% -> 1, <100% -> 2, else 3.
static int GetEndingTier(const CStateManager& mgr) {
  int pct = mgr.GetPlayerState(0)->GetItemPercentageRatio();
  if (pct < 75) {
    return 1;
  }
  return pct < 100 ? 2 : 3;
}

CGMSinglePlayer::CGMSinglePlayer() : x4_(false), x5_(false), x8_(0) {}

CGMSinglePlayer::~CGMSinglePlayer() {}

void CGMSinglePlayer::PutTo(COutputStream&) const {}

void CGMSinglePlayer::Update(float, CStateManager& mgr) {
  if (mgr.GetPlayer(0)->IsPlayerDeadEnough(mgr)) {
    x5_ = true;
  }
}

void CGMSinglePlayer::OnPlayerKilled(CStateManager&, TUniqueId, TUniqueId) {}
void CGMSinglePlayer::NotifyGenericEvent(CStateManager&, TUniqueId, uint) {}
void CGMSinglePlayer::RespawnPlayer(CStateManager&, uint) {}
bool CGMSinglePlayer::IsMultiplayer() const { return false; }
void CGMSinglePlayer::OnPlayerSpawned(CStateManager&, uint) { x4_ = true; }
void CGMSinglePlayer::SetSpawnPoint(uint, TUniqueId) {}
void CGMSinglePlayer::OnPlayerDamaged(CStateManager&, TUniqueId, TUniqueId, float) {}
void CGMSinglePlayer::NotifyStop(CStateManager&, TUniqueId) {}
uint CGMSinglePlayer::GetNumPlayers() const { return 1; }
bool CGMSinglePlayer::IsGameOver() { return x5_; }

void CGMSinglePlayer::EndGame(int resultIndex, CStateManager& mgr) {
  x8_ = GetEndingTier(mgr);
  if (resultIndex > 0) {
    gpMain->SetRestartMode(CMain::kRM_EndAutoSave);
  } else {
    switch (x8_) {
    case 1:
      gpMain->SetRestartMode(CMain::kRM_Credits1);
      break;
    case 2:
      gpMain->SetRestartMode(CMain::kRM_Credits2);
      break;
    case 3:
      gpMain->SetRestartMode(CMain::kRM_EndMovie1);
      break;
    }
  }
  x5_ = true;
  mgr.mUnkFlagA2 = true; // `rlwimi r0,r3,6,25,25` on 0x294c: mask 0x40
}

int CGMSinglePlayer::GetResultIndex() const { return x8_; }
int CGMSinglePlayer::GetGameModeType() { return 'SNGL'; }
void CGMSinglePlayer::GiveScore(CStateManager&, uint, uint) {}
int CGMSinglePlayer::GetItemAmount(const CStateManager&, uint) const { return 0; }
bool CGMSinglePlayer::IsNearScoreLimit(const CStateManager&, uint) const { return false; }
void CGMSinglePlayer::AddListener(CGameModeListener&, uint) {}
void CGMSinglePlayer::RemoveListener(CGameModeListener&, uint) {}
bool CGMSinglePlayer::v21() const { return false; }
float CGMSinglePlayer::GetElapsedTime() const { return -1.f; }
float CGMSinglePlayer::GetMatchTimeLimit() const { return -1.f; }
