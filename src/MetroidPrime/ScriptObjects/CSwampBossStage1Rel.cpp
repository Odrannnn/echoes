// CSwampBossStage1Rel.cpp - SwampBossStage1's (module 78) head, .text 0x0..0x160: the fourteen
// accessors above the module's class code, plus RELExit, RELMain and the loader registration
// RELMain calls. Same arrangement as `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CSandBossRel.cpp`, and the ranges come from
// `config/G2ME01/rels/SwampBossStage1/symbols.txt`:
//
//   0x000 fn_78_0   0x08  li r3,1
//   0x008 fn_78_8   0x3C  GetBoundingBox into a local, then fn_78_BCD0(out, &box)
//   0x044 fn_78_44  0x08  lbz r3, 0x44f(r3)
//   0x04C fn_78_4C  0x08  li r3,0
//   0x054 fn_78_54  0x08  li r3,0
//   0x05C fn_78_5C  0x08  li r3,0
//   0x064 fn_78_64  0x10  *self = kInvalidUniqueId
//   0x074 fn_78_74  0x0C  byte at +0x34c, bit 3
//   0x080 fn_78_80  0x0C  lbl_8041B758
//   0x08C fn_78_8C  0x08  *((char*)(self + 0x754))
//   0x094 fn_78_94  0x08  li r3,1
//   0x09C fn_78_9C  0x08  li r3,0
//   0x0A4 fn_78_A4  0x1C  three floats from self+0x54 -> *out
//   0x0C0 fn_78_C0  0x2C  virtual dispatch, vtable slot 0x38
//   0x0EC RELExit   0x24  li r3,0 / bl fn_8022EC64
//   0x110 RELMain   0x20  bl fn_78_130
//   0x130 fn_78_130 0x30  lbl_78_bss_20 = fn_78_160 ; fn_8022EC64(&lbl_78_bss_20)
//
// **The block is MysteryFlyer's with two members dropped and one predicate gained, and that is
// measured** - from diffing `build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s` against
// `CMysteryFlyerRel.cpp`'s, not from the `fn_<id>_<off>` names, which say nothing about which
// function is which. MysteryFlyer opens `li r3,1` then `addi r3,r3,0x818`; this one opens `li
// r3,1` and then goes straight to the `GetBoundingBox` wrapper, so it has no `+0x818` member
// accessor. MysteryFlyer also carries the `skDamageHitTime__10CPatterned` store at +0x448 and runs two `li r3,0`
// predicates; this one carries **no** `skDamageHitTime__10CPatterned` store and runs **three** `li r3,0`
// predicates in a row. The two multisets of instructions over the block are 55 here against 59
// there, and differ by exactly one `li r3,0` gained (+2), one `addi r3,r3,0x818` lost (-2) and
// the four-instruction `skDamageHitTime__10CPatterned` store lost (-4) - the two differences above and nothing
// else. So the block is 14 functions and 0x0..0xEC where MysteryFlyer's is 15 and 0x0..0xFC, and
// the head is 17 functions ending at 0x160 rather than 18 ending at 0x170. Nothing is missing:
// every head function this module has is a vtable entry of the 0x148-byte table at `.data:0x510`
// (`build/G2ME01/SwampBossStage1/asm/auto_04_00000000_data.s`).
//
// `fn_78_8` is not an `optional_object` problem, though it returns one. Retail *calls* the
// constructor, out of line at 0xBCD0 (`fn_78_BCD0`: `stb 1, 0x18(r3)` then six words copied),
// which this unit does not claim, so `fn_78_8` is written as the free function it compiles to -
// hidden return pointer in r3, `self` in r4 - and calls `fn_78_BCD0` by its dtk name.
// `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own return pointer
// is r3). The frame is 0x30 bytes, so the `const CAABox&` below has to stay a const reference:
// by value would grow it to 0x40.
//
// Everything from fn_78_160 (0x160, 0x314), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model. The
// other functions in the module are left retail too (251 text symbols in all, which is what
// `tools/audit_rel_claim.py` counts: 17 ours, 5 `REL_Setup`, 229 unclaimed), so dtk fills them
// and the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022EC64` is the DOL's 0x8022EC64, two instructions, immediately after
//     `LoadSwampBossStage1__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x8022EC38 (44
//     bytes, so it ends exactly there): `stw r3, gLoader_SwampBossStage1@sda21(r0); blr`. So it
//     stores the *address* of a loader slot, not a loader. `LoadSwampBossStage1` in
//     `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp` (a `Matching` unit) reads it as
//     `lwz r6, gLoader_SwampBossStage1; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the
//     store below hands it `&lbl_78_bss_20` and why that slot is four bytes wide. That source
//     also records that the setter is deliberately not claimed in the DOL, because REL modules
//     import it by its retail name - so it stays in dtk's auto unit, and **it is the plain DOL
//     symbol, so no `symbols.txt` rename and no DOL change are needed**. It is `extern "C"`: an
//     alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_78_bss_20` is `.bss:0x20`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **not** `.bss:0x0` as in MysteryFlyer - this module's `.bss` holds three
//     objects (`auto_05_00000000_bss.s`: `lbl_78_bss_0` `size:0x8` at 0x0, `lbl_78_bss_8`
//     `size:0x18` at 0x8, and this one at 0x20), and only the last is the loader slot. This
//     unit's split claims .text only, so dtk's `.bss` object has to define it, and a second
//     definition under MWCC is what produced mwldeppc's internal linker error on ScriptPlayerProxy.
//     Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

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

// The vtable slot fn_78_C0 dispatches to; see `CAtomicAlphaRel.cpp` for the layout.
class CSwampBossStage1Dispatch {
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
CEntity* fn_78_160(CStateManager&, CInputStream&, CEntityInfo&);
void fn_8022EC64(FScriptLoader* loader);
// .text 0xBCD0, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_78_BCD0(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_78_bss_20;
#else
FScriptLoader lbl_78_bss_20 = 0;
#endif

// .text 0x130, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_78_130() {
  lbl_78_bss_20 = fn_78_160;
  fn_8022EC64(&lbl_78_bss_20);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CMetareeSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file
// in `files.cmake` would make the port link `fn_78_160` and `fn_8022EC64`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `SwampBossStage1.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_78_130(); }

void RELExit() { fn_8022EC64(nullptr); }
#else
void mp_relmain_swampbossstage1() { fn_78_130(); }

void mp_relexit_swampbossstage1() { fn_8022EC64(nullptr); }
#endif

// .text 0xC0, 0x2C bytes. The call targets vtable slot 0x38.
void fn_78_C0(CSwampBossStage1Dispatch* self) { self->Slot12(); }

// .text 0xA4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_78_A4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x9C, 0x08 bytes. a predicate that is always false.
bool fn_78_9C(void*) { return false; }

// .text 0x94, 0x08 bytes. a predicate that is always true.
bool fn_78_94(void*) { return true; }

// .text 0x8C, 0x08 bytes. the address of the member at +0x754.
void* fn_78_8C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x80, 0x0c bytes. a constant float out of the DOL.
float fn_78_80(void*) { return lbl_8041B758; }

// .text 0x74, 0x0c bytes. the flag at +0x34c, bit 3.
bool fn_78_74(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x64, 0x10 bytes. resets the unique id to the invalid value.
void fn_78_64(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x5C, 0x08 bytes. a predicate that is always false.
bool fn_78_5C(void*) { return false; }

// .text 0x54, 0x08 bytes. a predicate that is always false.
bool fn_78_54(void*) { return false; }

// .text 0x4C, 0x08 bytes. a predicate that is always false.
bool fn_78_4C(void*) { return false; }

// .text 0x44, 0x08 bytes. the byte at +0x44f.
unsigned char fn_78_44(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x8, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_78_8(void* out, const CPhysicsActor* self) {
  fn_78_BCD0(out, self->GetBoundingBox());
}

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_78_0(void*) { return true; }
}
