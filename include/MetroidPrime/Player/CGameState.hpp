#ifndef _CGAMESTATE
#define _CGAMESTATE

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"

class CGameMode;

class CGameState {
public:
  CGameState();
  CGameState(CInputStream& in, int saveIdx);

  void ReadSystemOptions(CInputStream& in);
  void PutTo(COutputStream& out) const;
  void WriteSystemOptions(COutputStream& out);

  void SetIsDarkWorld(bool);
  CGameMode& GetGameMode();

  CGameOptions& GameOptions() { return gameOptions; }

  u64 GetCardSerial() const { return cardSerial; }
  float GetHardModeDamageMultiplier() const;
  bool GetHardModeEnabled() const;

  float GetUnk50() const { return x50_unk; }
  void SetUnk50(float value); // fn_801424EC

private:
  char pad1[0x50];
  float x50_unk;
  char pad1b[0x2C];
  CGameOptions gameOptions;
  CHintOptions hintOptions;
  CPersistentOptions persistentOptions;
  u64 cardSerial;

  char pad2[0x1E0];
};

CHECK_SIZEOF(CGameState, 0x2f0)

extern CGameState* gpGameState;

#endif // _CGAMESTATE
