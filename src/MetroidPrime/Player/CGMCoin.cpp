#include "MetroidPrime/Player/CGMCoin.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "rstl/math.hpp"

// Guessed name. Initial coin count decreases with the player's death count.
static int sRespawnCoins[] = {100, 80, 60, 50, 40, 30, 20};

// mPlayers is what puts the two rstl::vector<CGMCoin::SPlayerState> template
// instantiations into this unit. They are the only two functions retail leaves
// unnamed here: fn_80195DC4 (0x80195DC4, 132 bytes) is the destructor and
// fn_8019648C (0x8019648C, 156 bytes) the (count, value) constructor, and both are
// byte-identical to ours apart from the `bl` relocation fields - 2 differing
// instructions in the destructor (the bl to CMemory::Free and the one after it) and
// 1 in the constructor (the bl to rstl::rmemory_allocator::allocate).
// config/G2ME01/symbols.txt names them accordingly, which is what objdiff needs to
// pair them with ours; they scored 0.00% before that because they had no name to
// match on.
CGMCoin::CGMCoin(int playerCount, int coinLimit, float timeLimit, bool flag)
: CGMMultiplayer(timeLimit, flag)
, x38_(false)
, mPlayerCount(playerCount)
, mCoinLimit(coinLimit)
, mPlayers(playerCount, SPlayerState())
, mCoinLimitReached(false) {}

void CGMCoin::Update(float dt, CStateManager& mgr) {
  UpdateTimer(dt, mgr);
  if (GetMatchTimeLimit() > 0.f && GetElapsedTime() > GetMatchTimeLimit()) {
    EndGame(GetResultIndex(), mgr);
  }

  for (uint i = 0; i < uint(mPlayerCount); ++i) {
    CPlayer* pl = mgr.mPlayers[i];
    SPlayerState& player = mPlayers[i];
    CPlayerState& state = *mgr.mPlayerStates[i];
    state.ReInitializePowerUp(CPlayerState::kIT_CoinCounter, 0x8000);
    if (player.mDead) {
      player.mRespawnTimer -= dt;
      if (player.mRespawnTimer < 0.f && player.mCanRespawn && pl->fn_80019e20(mgr)) {
        RespawnPlayer(mgr, i);
        player.mDead = false;
      }
    } else if (!state.IsPlayerAlive()) {
      player.mDead = true;
      state.AddPowerUp(CPlayerState::kIT_DiedCount, 1);
      state.IncrPickUp(CPlayerState::kIT_DiedCount, 1);
      player.mRespawnTimer = 1.f;
    }
    if (state.GetItemAmount(CPlayerState::kIT_CoinCounter) >= mCoinLimit && mCoinLimit != -1) {
      EndGame(GetResultIndex(), mgr);
      mCoinLimitReached = true;
    }
  }
}

void CGMCoin::RespawnPlayer(CStateManager& mgr, uint playerIndex) {
  CGMMultiplayer::RespawnPlayer(mgr, playerIndex);
  CPlayerState& state = *mgr.PlayerState(playerIndex);
  int deaths = state.GetPowerUp(CPlayerState::kIT_DiedCount).mAmount;
  if (deaths >= 7) {
    deaths = 6;
  }
  state.PowerUp(CPlayerState::kIT_CoinCounter).mAmount = sRespawnCoins[deaths];
}

void CGMCoin::OnPlayerSpawned(CStateManager& mgr, uint playerIndex) {
  CGMMultiplayer::OnPlayerSpawned(mgr, playerIndex);
  mgr.PlayerState(playerIndex)->PowerUp(CPlayerState::kIT_CoinCounter).mAmount = sRespawnCoins[0];
}

uint CGMCoin::GetNumPlayers() const { return mPlayerCount; }

bool CGMCoin::IsGameOver() { return x38_ || CGMMultiplayer::IsGameOver(); }

void CGMCoin::EndGame(int resultIndex, CStateManager& mgr) {
  CGMMultiplayer::EndGame(resultIndex, mgr);
  for (int i = 0; i < mPlayerCount; ++i) {
    CPlayerState& state = *mgr.PlayerState(uint(i));
    mPlayers[i].mScore = GetItemAmount(mgr, i);
    mPlayers[i].mPlayerSelection = state.GetPlayerSelection();
  }
}

int CGMCoin::GetGameModeType() { return 'COIN'; }

void CGMCoin::GiveScore(CStateManager&, uint, uint) {}

int CGMCoin::GetItemAmount(const CStateManager& mgr, uint playerIndex) const {
  return mgr.GetPlayerState(playerIndex)->GetItemAmount(CPlayerState::kIT_CoinCounter);
}

bool CGMCoin::IsNearScoreLimit(const CStateManager& mgr, uint playerIndex) const {
  return mCoinLimit - GetItemAmount(mgr, playerIndex) <= 1 && mCoinLimit > 1;
}

bool CGMCoin::IsNearTimeLimit() const {
  const float remaining = GetMatchTimeLimit() - GetElapsedTime();
  if (GetMatchTimeLimit() <= 0.f) {
    return false;
  }
  if (GetMatchTimeLimit() <= 60.f) {
    return 0.5f * GetMatchTimeLimit() < GetElapsedTime();
  }
  return remaining < 60.f;
}

int CGMCoin::GetCoinLimit() const { return mCoinLimit; }

CGMCoin::~CGMCoin() {}
