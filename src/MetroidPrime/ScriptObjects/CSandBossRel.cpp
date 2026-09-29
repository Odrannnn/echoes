// CSandBossRel.cpp - SandBoss's (module 55) head, .text 0x0..0x178: the sixteen
// accessors above the module's class code, plus RELExit, RELMain and the loader
// registration RELMain calls. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp`, and the ranges come from
// `config/G2ME01/rels/SandBoss/symbols.txt`:
//
//   0x000 fn_55_0   0x08  li r3,1
//   0x008 fn_55_8   0x08  li r3,1
//   0x010 fn_55_10  0x3C  GetBoundingBox into a local, then fn_55_10C78(out, &box)
//   0x04C fn_55_4C  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
//   0x05C fn_55_5C  0x08  lbz r3, 0x44f(r3)
//   0x064 fn_55_64  0x08  li r3,0
//   0x06C fn_55_6C  0x08  li r3,0
//   0x074 fn_55_74  0x08  li r3,0
//   0x07C fn_55_7C  0x10  *self = kInvalidUniqueId
//   0x08C fn_55_8C  0x0C  byte at +0x34c, bit 3
//   0x098 fn_55_98  0x0C  lbl_8041B758
//   0x0A4 fn_55_A4  0x08  lbl -> *((char*)(self + 0x754))
//   0x0AC fn_55_AC  0x08  li r3,1
//   0x0B4 fn_55_B4  0x08  li r3,0
//   0x0BC fn_55_BC  0x1C  three floats from self+0x54 -> *out
//   0x0D8 fn_55_D8  0x2C  virtual dispatch, vtable slot 0x38
//   0x104 RELExit   0x24  li r3,0 / bl fn_802189D0
//   0x128 RELMain   0x20  bl fn_55_148
//   0x148 fn_55_148 0x30  lbl_55_bss_4 = fn_55_178 ; fn_802189D0(&lbl_55_bss_4)
//
// **The block is the family's in a different order and with one extra member, and that is
// measured** - from diffing `build/G2ME01/SandBoss/asm/auto_00_00000000_text.s` against
// `CMysteryFlyerRel.cpp`'s, not from the `fn_<id>_<off>` names, which say nothing about which
// function is which. MysteryFlyer's opens with `li r3,1` then `+0x818` and has two `li r3,0`
// predicates; this one opens with `li r3,1` twice - so it covers one member fewer at the front -
// and runs three `li r3,0` predicates in a row, which pushes `kInvalidUniqueId` from 0x74 to 0x7C
// and every accessor above it by 8 bytes. The fourteen bodies below are otherwise MysteryFlyer's,
// and that is counted rather than eyeballed: 39 instructions over the 14 accessors here against
// 37 over MysteryFlyer's 13, and the two multisets differ by exactly one `li r3,0` gained and one
// `addi r3,r3,0x818` lost - the two differences above, with nothing else between them.
//
// `fn_55_10` is not an `optional_object` problem, though it returns one. Retail *calls* the
// constructor, out of line at 0x10C78 (`fn_55_10C78`: six words copied, then `stb 1, 0x18(r3)`),
// which this unit does not claim, so `fn_55_10` is written as the free function it compiles to -
// hidden return pointer in r3, `self` in r4 - and calls `fn_55_10C78` by its dtk name.
// `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own return pointer
// is r3). The frame is 0x30 bytes, so the `const CAABox&` below has to stay a const reference:
// by value would grow it to 0x40.
//
// Everything from fn_55_178 (0x178, 0x33C), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model. The
// other 298 functions in the module are left retail too (322 text symbols in all: 19 ours, 5
// `REL_Setup`, 298 unclaimed), so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds.
//
// The two callees are named by what they are, not invented:
//   - `fn_802189D0` is the DOL's 0x802189D0, two instructions, immediately after
//     `LoadSandBoss__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x802189A4:
//     `stw r3, gLoader_SandBoss@sda21(r0); blr`. So it stores the *address* of a loader slot, not
//     a loader. `LoadSandBoss` in `src/MetroidPrime/ScriptLoader/SandBoss.cpp` (a `Matching`
//     unit) reads it as `lwz r6, gLoader_SandBoss; lwz r12, 0(r6); mtctr r12; bctrl`, which is
//     why the store below hands it `&lbl_55_bss_4` and why that slot is four bytes wide. That
//     source also records that the setter is deliberately not claimed in the DOL, because REL
//     modules import it by its retail name - so it stays in dtk's auto unit, and **it is the
//     plain DOL symbol, so no `symbols.txt` rename and no DOL change are needed** (unlike
//     MysteryFlyer's `fn_80232868`, which `CPlantScarabSwarmRel.cpp` had to declare the same
//     way). It is `extern "C"`: an alias would be a different symbol and the call would resolve
//     to nothing.
//   - `lbl_55_bss_4` is `.bss:0x4`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **not** `.bss:0x0` as in MysteryFlyer - this module's `.bss` holds three
//     objects (`auto_05_00000000_bss.s`: `lbl_55_bss_0` at 0x0, this one at 0x4, and
//     `lbl_55_bss_8` at 0x8), and only the middle one is the loader slot. This unit's split
//     claims .text only, so dtk's `.bss` object has to define it, and a second definition under
//     MWCC is what produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern
//     under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float lbl_8041AAB8;
extern "C" const float lbl_8041B758;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_55_D8 dispatches to; see `CAtomicAlphaRel.cpp` for the layout.
class CSandBossDispatch {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual void Slot4();
  virtual void Slot5();
  virtual void Slot6();
  virtual void Slot7();
  virtual void Slot8();
  virtual void Slot9();
  virtual void Slot10();
  virtual void Slot11();
  virtual float Slot12();
};

extern "C" {
CEntity* fn_55_178(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_802189D0(FScriptLoader* loader);
// .text 0x10C78, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_55_10C78(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_55_bss_4;
#else
FScriptLoader lbl_55_bss_4 = 0;
#endif

// .text 0x148, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_55_148() {
  lbl_55_bss_4 = fn_55_178;
  fn_802189D0(&lbl_55_bss_4);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CMetareeSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file
// in `files.cmake` would make the port link `fn_55_178` and `fn_802189D0`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `SandBoss.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so
// the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_55_148(); }

void RELExit() { fn_802189D0(nullptr); }
#else
void mp_relmain_sandboss() { fn_55_148(); }

void mp_relexit_sandboss() { fn_802189D0(nullptr); }
#endif

// .text 0xD8, 0x2C bytes. The call targets vtable slot 0x38.
void fn_55_D8(CSandBossDispatch* self) { self->Slot12(); }

// .text 0xBC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_55_BC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xB4, 0x08 bytes. a predicate that is always false.
bool fn_55_B4(void*) { return false; }

// .text 0xAC, 0x08 bytes. a predicate that is always true.
bool fn_55_AC(void*) { return true; }

// .text 0xA4, 0x08 bytes. the address of the member at +0x754.
void* fn_55_A4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x98, 0x0c bytes. a constant float out of the DOL.
float fn_55_98(void*) { return lbl_8041B758; }

// .text 0x8C, 0x0c bytes. the flag at +0x34c, bit 3.
bool fn_55_8C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x7C, 0x10 bytes. resets the unique id to the invalid value.
void fn_55_7C(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x74, 0x08 bytes. a predicate that is always false.
bool fn_55_74(void*) { return false; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_55_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_55_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_55_5C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4C, 0x10 bytes. stores a DOL float at +0x448.
void fn_55_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_55_10(void* out, const CPhysicsActor* self) {
  fn_55_10C78(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_55_8(void*) { return true; }

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_55_0(void*) { return true; }
}
