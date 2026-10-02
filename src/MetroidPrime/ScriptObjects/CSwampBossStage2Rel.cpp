// CSwampBossStage2Rel.cpp - SwampBossStage2's (module 79) head, .text 0x0..0x170: the fifteen
// accessors above the module's class code, plus RELExit, RELMain and the loader registration
// RELMain calls. Same arrangement as `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CSwampBossStage1Rel.cpp`, and the ranges come from
// `config/G2ME01/rels/SwampBossStage2/symbols.txt`:
//
//   0x000 fn_79_0   0x08  li r3,1
//   0x008 fn_79_8   0x3C  GetBoundingBox into a local, then fn_79_D4E8(out, &box)
//   0x044 fn_79_44  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x054 fn_79_54  0x08  lbz r3, 0x44f(r3)
//   0x05C fn_79_5C  0x08  li r3,0
//   0x064 fn_79_64  0x08  li r3,0
//   0x06C fn_79_6C  0x08  li r3,0
//   0x074 fn_79_74  0x10  *self = kInvalidUniqueId
//   0x084 fn_79_84  0x0C  byte at +0x34c, bit 28
//   0x090 fn_79_90  0x0C  lbl_8041B758
//   0x09C fn_79_9C  0x08  addi r3,r3,0x754
//   0x0A4 fn_79_A4  0x08  li r3,1
//   0x0AC fn_79_AC  0x08  li r3,0
//   0x0B4 fn_79_B4  0x1C  three floats from self+0x54 -> *out
//   0x0D0 fn_79_D0  0x2C  virtual dispatch, vtable slot 0x38
//   0x0FC RELExit   0x24  li r3,0 / bl fn_8022EC30
//   0x120 RELMain   0x20  bl fn_79_140
//   0x140 fn_79_140 0x30  lbl_79_bss_8 = fn_79_170 ; fn_8022EC30(&lbl_79_bss_8)
//
// **The block is MysteryFlyer's with the `+0x818` member accessor traded for a fourth `li r3,0`
// predicate**, and that is measured - from diffing
// `build/G2ME01/SwampBossStage2/asm/auto_00_00000000_text.s` over 0x0..0xFC against
// `CMysteryFlyerRel.cpp`'s over the same range, not from the `fn_<id>_<off>` names, which say
// nothing about which function is which. Both are 63 instructions over 15 accessors and the two
// multisets differ by exactly one `addi r3, r3, 0x818` lost and one `li r3, 0x0` gained;
// nothing else. Against `CSwampBossStage1Rel.cpp` (59 instructions over 14 accessors, 0x0..0xEC)
// the only difference is this module's four-instruction `skDamageHitTime__10CPatterned` store at +0x448, which
// SwampBossStage1 does not have. So the block is 0x0..0xFC with fifteen functions, and the head is
// 0x0..0x170 with eighteen - MysteryFlyer's shape exactly. Nothing is missing: every head
// function this module has is a vtable entry of the 0x148-byte table at `.data:0x424`
// (`build/G2ME01/SwampBossStage2/asm/auto_04_00000000_data.s`, which stores all fifteen
// `fn_79_0`..`fn_79_D0`), so there is no `force_active:` hazard either.
//
// `fn_79_8` is not an `optional_object` problem, though it returns one. Retail *calls* the
// constructor, out of line at 0xD4E8 (`fn_79_D4E8`), which this unit does not claim, so `fn_79_8`
// is written as the free function it compiles to - hidden return pointer in r3, `self` in r4 - and
// calls `fn_79_D4E8` by its dtk name. `self` needs no move because `GetBoundingBox` takes `this`
// in r4 too (its own return pointer is r3). The frame is 0x30 bytes, so the `const CAABox&` below
// has to stay a const reference: by value would grow it to 0x40.
//
// Everything from fn_79_170 (0x170, 0x2F0), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model. The
// other functions in the module are left retail too (243 text symbols in all, which is what
// `tools/audit_rel_claim.py` counts: 18 ours, 5 `REL_Setup`, 220 unclaimed), so dtk fills them and
// the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022EC30` is the DOL's 0x8022EC30, two instructions,
//     `stw r3, gLoader_SwampBossStage2@sda21(r0); blr` (see
//     `build/G2ME01/asm/auto_03_8022EC30_text.s`), immediately *before*
//     `LoadSwampBossStage2__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EC04, which
//     is 44 bytes and so ends exactly there. So it stores the *address* of a loader slot, not a
//     loader. `LoadSwampBossStage2` in `src/MetroidPrime/ScriptLoader/SwampBossStage2.cpp` (a
//     `Matching` unit) reads it as `lwz r6, gLoader_SwampBossStage2; lwz r12, 0(r6); mtctr r12;
//     bctrl`, which is why the store below hands it `&lbl_79_bss_8` and why that slot is four
//     bytes wide. That source also records that the setter is deliberately not claimed in the
//     DOL, because REL modules import it by its retail name - so it stays in dtk's auto unit, and
//     **it is the plain DOL symbol, so no `symbols.txt` rename and no DOL change are needed**. It
//     is `extern "C"`: an alias would be a different symbol and the call would resolve to
//     nothing.
//   - `lbl_79_bss_8` is `.bss:0x8`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **not** `.bss:0x0` as in MysteryFlyer - this module's `.bss` holds four
//     objects (`auto_05_00000000_bss.s`: `lbl_79_bss_0` `size:0x4` at 0x0, `lbl_79_bss_4`
//     `size:0x1` at 0x4, an unnamed 3-byte gap at 0x5, and this one at 0x8), and only this one is
//     the loader slot. This unit's split claims .text only, so dtk's `.bss` object has to define
//     it, and a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
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

// The vtable slot fn_79_D0 dispatches to; see `CAtomicAlphaRel.cpp` for the layout.
class CSwampBossStage2Dispatch {
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
CEntity* fn_79_170(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_8022EC30(FScriptLoader* loader);
// .text 0xD4E8, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_79_D4E8(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_79_bss_8;
#else
FScriptLoader lbl_79_bss_8 = 0;
#endif

// .text 0x140, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_79_140() {
  lbl_79_bss_8 = fn_79_170;
  fn_8022EC30(&lbl_79_bss_8);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CMetareeSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file
// in `files.cmake` would make the port link `fn_79_170` and `fn_8022EC30`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `SwampBossStage2.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_79_140(); }

void RELExit() { fn_8022EC30(nullptr); }
#else
void mp_relmain_swampbossstage2() { fn_79_140(); }

void mp_relexit_swampbossstage2() { fn_8022EC30(nullptr); }
#endif

// .text 0xD0, 0x2C bytes. The call targets vtable slot 0x38.
void fn_79_D0(CSwampBossStage2Dispatch* self) { self->Slot12(); }

// .text 0xB4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_79_B4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xAC, 0x08 bytes. a predicate that is always false.
bool fn_79_AC(void*) { return false; }

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_79_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_79_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0c bytes. a constant float out of the DOL.
float fn_79_90(void*) { return lbl_8041B758; }

// .text 0x84, 0x0c bytes. the flag at +0x34c, bit 3.
//
// dtk renders retail's second instruction as `extrwi r3, r0, 1, 28`, which is bit 28 of the
// zero-extended byte and so always 0. It is written here as `& 8` anyway, because that is the
// spelling that emits those exact three bytes: `CMysteryFlyerRel.cpp`'s `fn_45_84` and
// `CSwampBossStage1Rel.cpp`'s `fn_78_74` carry the same three retail bytes `54 03 EF FE` and both
// are `Matching` units in modules whose sha1 holds. The two bit positions are the one place where
// this reading is not what a naive reading of the disassembly would say, and it is measured twice
// over rather than argued.
bool fn_79_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x74, 0x10 bytes. resets the unique id to the invalid value.
void fn_79_74(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_79_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_79_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. a predicate that is always false.
bool fn_79_5C(void*) { return false; }

// .text 0x54, 0x08 bytes. the byte at +0x44f.
unsigned char fn_79_54(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x44, 0x10 bytes. stores a DOL float at +0x448.
void fn_79_44(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x8, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_79_8(void* out, const CPhysicsActor* self) {
  fn_79_D4E8(out, self->GetBoundingBox());
}

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_79_0(void*) { return true; }
}
