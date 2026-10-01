#ifndef _CGAMEMODE
#define _CGAMEMODE

#include "MetroidPrime/TGameTypes.hpp"

class CGameModeListener;
class COutputStream;
class CStateManager;

// Method names are guessed from the GameCube virtual interface.
class CGameMode {
public:
  // Retail's `CGameMode::~CGameMode` (0x80004798, 0x48 bytes) is the null guard, the
  // `__vt__7CGameMode` store at 0x803B0D68, and the D0 test with `operator delete` - and
  // `__dt__17CFrontEndGameModeFv` (0x80143B94) has that **inlined**, with the D0 test hoisted out and
  // shared with the derived destructor. A destructor declared `{}` here is inline and visible to
  // every derived class's destructor, which is what reproduces that; declared only, the same
  // teardown is an out-of-line `bl __dt__9CGameModeFv` and `__dt__17CFrontEndGameModeFv` costs four
  // instructions of prologue, three of epilogue and five of base teardown.
  virtual ~CGameMode() {}
  virtual void PutTo(COutputStream& out) const = 0;
  virtual void Update(float dt, CStateManager& mgr) = 0;
  virtual void OnPlayerKilled(CStateManager& mgr, TUniqueId victim, TUniqueId killer) = 0;
  virtual void NotifyGenericEvent(CStateManager& mgr, TUniqueId player, uint value) = 0;
  virtual void RespawnPlayer(CStateManager& mgr, uint playerIndex) = 0;
  virtual bool IsMultiplayer() const = 0;
  virtual void OnPlayerSpawned(CStateManager& mgr, uint playerIndex) = 0;
  virtual void SetSpawnPoint(uint playerIndex, TUniqueId spawnPoint) = 0;
  virtual void OnPlayerDamaged(CStateManager& mgr, TUniqueId victim, TUniqueId attacker,
                               float damage) = 0;
  virtual void NotifyStop(CStateManager& mgr, TUniqueId player) = 0;
  virtual uint GetNumPlayers() const = 0;
  virtual bool IsGameOver() = 0;
  virtual void EndGame(int resultIndex, CStateManager& mgr) = 0;
  virtual int GetResultIndex() const = 0;
  virtual int GetGameModeType() = 0;
  virtual void GiveScore(CStateManager& mgr, uint playerIndex, uint amount) = 0;
  virtual int GetItemAmount(const CStateManager& mgr, uint playerIndex) const = 0;
  virtual bool IsNearScoreLimit(const CStateManager& mgr, uint playerIndex) const = 0;
  virtual void AddListener(CGameModeListener& listener, uint playerIndex) = 0;
  virtual void RemoveListener(CGameModeListener& listener, uint playerIndex) = 0;
  virtual bool v21() const = 0;
  virtual float GetElapsedTime() const = 0;
  virtual float GetMatchTimeLimit() const = 0;
};

CHECK_SIZEOF(CGameMode, 4)

#endif // _CGAMEMODE
