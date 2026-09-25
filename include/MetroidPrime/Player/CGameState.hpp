#ifndef _CGAMESTATE
#define _CGAMESTATE

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"

class CGameMode;
class CWorldState;

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

  // Retail 0x80142520, 8 bytes, `addi r3,r3,60; blr` - so the whole body is the address of
  // +0x3C. Retail's `rstl::rc_ptr<CWorldState>` there is the pair its constructor writes at
  // 0x80144140: a `new(1200)`'d CWorldState at +0x3C and a separately allocated refcount word,
  // set to 1, at +0x40. Every caller then does one `lwz r3,0(r3)` to get through it before
  // calling a CWorldState method, so this hands out a *reference to the pointer* rather than
  // the pointer: that is what reproduces the pair exactly. Returning `rstl::rc_ptr&` instead
  // would be wrong on this port, whose `rc_ptr` is a single word pointing at a `CRefData` and
  // would need two dependent loads where retail needs one.
  //
  // Deliberately not inline. Retail's definition is in CGameState.cpp, a different translation
  // unit from its only caller, so `CGameArchitectureSupport::Update` (0x80007A14) calls it; an
  // inline accessor folds into `lwz r4,gpGameState; lwz r3,60(r4)` instead and drops that
  // function from 100% to 95.89%. There is no CGameState.cpp in the port, so the definition is
  // at the bottom of src/MetroidPrime/main.cpp under `#pragma inline_max_size(0)`.
  CWorldState*& GetWorldState();

  float GetUnk50() const { return x50_unk; }
  void SetUnk50(float value); // fn_801424EC

private:
  char pad1[0x3C];
  CWorldState* x3c_worldState; //!< retail: x0_ptr of an rc_ptr
  uint* x40_refCount;           //!< retail: x4_refCount of the same rc_ptr, allocated with *refCount = 1
  char pad1a[0xC];
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
