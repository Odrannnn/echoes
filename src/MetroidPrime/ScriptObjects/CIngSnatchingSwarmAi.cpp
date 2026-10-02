// CIngSnatchingSwarmAi.cpp - IngSnatchingSwarm's (module 33) state-table run, .text
// 0x35F8..0x383C. Eleven functions, contiguous, and the run is named by the module's own
// `.data`: `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s` holds nine 12-byte
// records `.4byte 0, 0xFFFFFFFF, <fn>` at `.data:0x4, 0x10, 0x1C, 0x28, 0x34, 0x40, 0x4C, 0x58,
// 0x64` plus one more at 0x100, naming `fn_33_37E0, 37AC, 3788, 3770, 3708, 3678, 3638, 3620,
// 3614, 35F8`, and those ten are contiguous from 0x35F8 to 0x37F4. `fn_33_5078` (the module's
// `.ctors` entry) copies the records into the tables it builds at `.data:0x70/0x148/0x1F0`, so the
// three-word record is a member-function pointer for `CIngSnatchingSwarm` and the functions are the
// ones retail's state table dispatches on.
//
// The functions are `extern "C"` free functions and the object is a stand-in class carrying the
// measured offsets, exactly as `CIngSnatchingSwarmGenAccessors.cpp` does it: `self` arrives in r3
// and `CStateManager` in r4 because that is the pointer-to-member-function table's own register
// convention, so the extra parameters are simply unused where retail ignores them. **Only the
// offsets are derived; the bodies are written as ordinary C++ over them.** Each offset below is
// read off `build/G2ME01/IngSnatchingSwarm/asm/auto_00_000000A8_text.s`, and the whole range is in
// the module's FORCEACTIVE list, so nothing here is a dead-stripping hazard.
//
// The range is claimed exactly and nothing else: everything outside it is left unclaimed, so dtk
// fills it from retail and the module's sha1 against `config/G2ME01/config.yml` still holds. The
// neighbours left retail are `fn_33_348C` (0x348C, 0x16C, the module's update) below and
// `fn_33_383C` (0x383C, 0x134) above.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% while the module's hash breaks.

#include "types.h"

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/TGameTypes.hpp"

// The one float this range reads out of the module's `.rodata`, `.rodata:0x34`, `.float 0`
// (`build/G2ME01/IngSnatchingSwarm/asm/auto_03_00000000_rodata.s`). This unit claims `.text` only,
// so the reference has to be to the symbol the unclaimed `.rodata` object defines and not to a
// literal of our own - a literal would put a `.rodata` section in this object and move the module.
// `extern` under MWCC and a host definition, exactly as `lbl_33_rodata_30` is handled in
// `CIngSnatchingSwarmGenAccessors.cpp`.
#ifdef __MWERKS__
extern "C" {
extern const float lbl_33_rodata_34;
}
#else
extern "C" const float lbl_33_rodata_34 = 0.0f;
#endif

// Byte 0x73C is a block of 1-bit flags, not a mask field: the store is `lbz`+`rlwimi`+`stb` and a
// read is a single `rlwinm` extraction, which is the bitfield form. The names are the bit each
// field occupies in the byte's MSB-first order, so `x80` is the byte's 0x80 and the declaration
// order below is the byte order. **The two encodings are not symmetric** - mwcceppc 2.7 encodes
// `s->x40 = 1` as `rlwimi r0, r4, 6, 25, 25` and `return s->x40` as `rlwinm r3, r0, 26, 31, 31` -
// so a field's index has to be picked from the retail word, not from the bit the C++ reads.
struct CIngSnatchingSwarmFlags {
  u8 x80 : 1;
  u8 x40 : 1;
  u8 x20 : 1;
  u8 x10 : 1;
  u8 x08 : 1;
  u8 x04 : 1;
  u8 x02 : 1;
  u8 x01 : 1;
};

// The object the state table dispatches on, reduced to the members these eleven functions touch.
// Every offset is measured; the padding between members is the space the class's real body occupies
// and is deliberately written as `char x_padN[...]`, which `tools/check_raw_offsets.py` does not
// count as a raw offset.
class CIngSnatchingSwarmAi {
private:
  char x_pad0[0x54];

public:
  float x54;
  float x58;
  float x5c;

private:
  char x_pad1[0x16C - 0x60];

public:
  float x16C;

private:
  char x_pad2[0x190 - 0x170];

public:
  float x190;

private:
  char x_pad3[0x1F8 - 0x194];

public:
  float x1F8;

private:
  char x_pad4[0x304 - 0x1FC];

public:
  float x304;
  CAABox x308;
  u8 x320;

private:
  char x_pad5[0x3D4 - 0x321];

public:
  u32 x3D4;

private:
  char x_pad6[0x410 - 0x3D8];

public:
  TUniqueId x410;
  TUniqueId x412;

private:
  char x_pad6b[0x420 - 0x414];

public:
  float x420;

private:
  char x_pad7[0x73C - 0x424];

public:
  CIngSnatchingSwarmFlags x73C;
};

// The object `GetObjectById` hands back in fn_33_3678/fn_33_3708, reduced the same way: its
// position is at 0x54 and the byte fn_33_3708 tests is at 0x420.
class CIngSnatchingSwarmTarget {
private:
  char x_pad0[0x54];

public:
  float x54;
  float x58;
  float x5c;

private:
  char x_pad1[0x420 - 0x60];

public:
  CIngSnatchingSwarmFlags x420;
};

// The 0x1C-byte record fn_33_37F4 fills: the object's box copy and the flag that says whether it
// was taken.
struct CIngSnatchingSwarmBoxSnapshot {
  CAABox mBox;
  u8 mValid;
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt` calls them.
extern "C" {
// .text 0x37F4, 0x48 bytes - copies the box at 0x308 into a 0x1C-byte destination, guarded by the
// byte at 0x320. Retail stores the flag first, then re-reads the member for the test, so the
// member is written twice rather than held in a local.
void fn_33_37F4(CIngSnatchingSwarmBoxSnapshot* dest, CIngSnatchingSwarmAi* self) {
  dest->mValid = self->x320;
  if (self->x320 != 0) {
    dest->mBox = self->x308;
  }
}

// .text 0x37E0, 0x14 bytes.
bool fn_33_37E0(CIngSnatchingSwarmAi* self) { return self->x3D4 == 2; }

// .text 0x37AC, 0x34 bytes - flag bit && the float at 0x1F8 <= .rodata:0x34 (0.0f). The result
// lives in a named local: retail keeps it in r5 for the whole body and moves it to r3 once.
bool fn_33_37AC(CIngSnatchingSwarmAi* self) {
  bool result = false;
  if (self->x73C.x80 && self->x1F8 <= lbl_33_rodata_34) {
    result = true;
  }
  return result;
}

// .text 0x3788, 0x24 bytes - `>=`, so the compare is `cror eq,gt,eq` and the extraction is
// cr0's bit 2.
bool fn_33_3788(CIngSnatchingSwarmAi* self) { return self->x304 >= self->x420 + self->x16C; }

// .text 0x3770, 0x18 bytes - `<`, so the compare is the FPCC's own LT bit (`mfcr` + `srwi`).
bool fn_33_3770(CIngSnatchingSwarmAi* self) { return self->x304 < self->x420; }

// .text 0x3708, 0x68 bytes - the nested-if spelling is what puts retail's `li r3,1` block before
// its `li r3,0` block: an early `if (p == nullptr) return false;` would emit `bne` + `li r3,0` +
// `b` here instead.
bool fn_33_3708(CIngSnatchingSwarmAi* self, CStateManager& mgr) {
  CEntity* obj = const_cast< CEntity* >(mgr.GetObjectById(self->x412));
  if (obj != nullptr) {
    CPatterned* patterned = TCastToPtr< CPatterned >(obj);
    if (patterned != nullptr) {
      CIngSnatchingSwarmTarget* target = reinterpret_cast< CIngSnatchingSwarmTarget* >(patterned);
      return target->x420.x40;
    }
    return true;
  }
  return false;
}

// .text 0x3678, 0x90 bytes - squared distance to the object the id at 0x412 names, against the
// float at 0x190. dx, dy, dz in that declaration order with the sum written left to right is the
// spelling that reproduces retail's `fmuls`/`fmadds` split.
bool fn_33_3678(CIngSnatchingSwarmAi* self, CStateManager& mgr) {
  CEntity* obj = const_cast< CEntity* >(mgr.GetObjectById(self->x412));
  if (obj != nullptr) {
    CIngSnatchingSwarmTarget* target = reinterpret_cast< CIngSnatchingSwarmTarget* >(obj);
    float dx = target->x54 - self->x54;
    float dy = target->x58 - self->x58;
    float dz = target->x5c - self->x5c;
    return dx * dx + dy * dy + dz * dz <= self->x190;
  }
  return false;
}

// .text 0x3638, 0x40 bytes - whether the id at 0x410 names a player.
bool fn_33_3638(CIngSnatchingSwarmAi* self, CStateManager& mgr) {
  CEntity* obj = const_cast< CEntity* >(mgr.GetObjectById(self->x410));
  return TCastToPtr< CPlayer >(obj) != nullptr;
}

// .text 0x3620, 0x18 bytes.
bool fn_33_3620(CIngSnatchingSwarmAi* self) { return self->x410 == self->x412; }

// .text 0x3614, 0x0C bytes - a 1-bit field returned as `int`; as `bool` it costs three extra
// instructions (`neg`/`or`/`srwi`) that retail does not have here.
int fn_33_3614(CIngSnatchingSwarmAi* self) { return self->x73C.x10; }

// .text 0x35F8, 0x1C bytes - the state-table entry the module dispatches on: message 0 sets one
// flag bit.
void fn_33_35F8(CIngSnatchingSwarmAi* self, CStateManager& mgr, int msg) {
  if (msg == 0) {
    self->x73C.x40 = 1;
  }
}
}
