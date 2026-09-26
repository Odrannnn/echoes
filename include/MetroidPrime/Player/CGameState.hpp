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
  CGameOptions gameOptions;             //!< +0x80, 0x44
  CHintOptions hintOptions;             //!< +0xC4, 0x18 - see that header for why 0x18
  CPersistentOptions persistentOptions; //!< +0xDC, 0x2C
  u64 cardSerial;                       //!< +0x108

  char pad2[0x1E0];                     //!< +0x110 .. +0x2F0
};

// **0x2F0, measured, and it agrees with `operator new(0x2F0)`.** `CGameGlobalObjects`'s
// constructor does `li r3,752` / `bl __nw__FUlPCcPCc` (0x800084C4) and the store
// `stw r4,-28360(r13)` at 0x80008548; 752 is 0x2F0. `tools/size_probe_gs.cpp` re-derives
// sizeof and every offset below with **mwcceppc's own flags** - the host compiler must not be
// used, it is 64-bit and `rstl::string` is 24 bytes there against retail's 0x10.
//
//   sizeof(CGameState) 0x2F0   +0x3C x3c_worldState   +0x40 x40_refCount
//   +0x50 x50_unk             +0x80 gameOptions        +0xC4 hintOptions
//   +0xDC persistentOptions   +0x108 cardSerial
//
// Each of those is independently confirmed by retail's own code, which is what makes this a
// measurement rather than a restatement of the header:
//
//   +0x3C  `stw r0,60(r30)`  0x801441A0  and  +0x40 `stw r3,64(r30)` 0x801441C4
//   +0x50  `stfs f0,80(r30)` 0x801441D8 and  `stfs f1,80(r30)` 0x80144544
//   +0x80  `addi r3,r30,128`  0x801441E0 -> `__ct__12CGameOptionsFv`, and again at
//         0x80142D40 / 0x80142E2C as `PutTo__12CGameOptionsFR16CBitStreamWriter(this+128)`
//   +0xC4  `addi r3,r30,196`  0x801441E8 -> `fn_80180738`, and 0x801444B0 -> `fn_801447C4`
//   +0xDC  `addi r3,r30,220`  0x801441F0 -> `fn_80146154`, and 0x80144558 -> `fn_8000401C`
//   +0x108 src/MetroidPrime/main.cpp:746, "new->x10C = the old x10C, new->x108 = the old x108"
//
// The rest of the object is mapped, and the map is what a `CGameState(CInputStream&, int)`
// needs to be written: docs/research/cgamestate_layout.md. Two of its boundaries are pinned by
// functions this constructor does not call, and both matter:
//
//   +0x110  `addi r3,r30,272` -> `fn_80144924` writes 3 here and 3 x 16 bytes at +0x114
//   +0x144  `addi r3,r30,324` -> `fn_80144924` likewise, and `fn_80142DD4` (0x80142DD4) confirms
//           the +0x144 block with `slwi r0,r4,4; add r31,r30,r0; addi r31,r31,328`, i.e.
//           `this + 0x148 + i*16` for i = 0,1,2
//   +0x178  `fn_80142CF8` (0x80142CF8) does `addi r3,r31,376; bl fn_80142BA4; lwz r4,388(r31)`,
//           so the block at +0x178 is 0x10 bytes with a data pointer at +0x184
//   +0x188  the constructor zeroes +0x18C, +0x190, +0x194: the same {?, count, cap, data}
//           0x10-byte shape, and main.cpp:723 names +0x188 as the source of a local
//   +0x198  `stb` (the pointer is non-null) and +0x19C (the `new(12)`'d pointer), then +0x1A0
//           is passed to `fn_80007040` and to `fn_80003BE8`
//   +0x2EC  the flag byte: three `ReadBits(1)` results forced into bits 7, 6 and 5
CHECK_SIZEOF(CGameState, 0x2f0)

extern CGameState* gpGameState;

#endif // _CGAMESTATE
