// CElitePirateRel.cpp - ElitePirate's (module 15) head, .text 0x0..0x178: the nineteen functions
// above the module's class code. The arrangement matches
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp`; the ranges come from
// `config/G2ME01/rels/ElitePirate/symbols.txt`:
//
//   0x000 fn_15_0    0x08  the address of the member at +0xa50
//   0x008 fn_15_8    0x08  the address of the member at +0x9c0
//   0x010 fn_15_10   0x3C  GetBoundingBox into a local, then fn_15_C094(out, &box)
//   0x04C fn_15_4C   0x10  store the default float at +0x448
//   0x05C fn_15_5C   0x08  the byte at +0x44f
//   0x064 fn_15_64   0x08  false
//   0x06C fn_15_6C   0x08  false
//   0x074 fn_15_74   0x10  reset the unique id to the invalid value
//   0x084 fn_15_84   0x0C  the flag at +0x34c - `extrwi r3,r0,1,28`
//   0x090 fn_15_90   0x0C  a constant float out of the DOL
//   0x09C fn_15_9C   0x08  the address of the member at +0x754
//   0x0A4 fn_15_A4   0x08  true
//   0x0AC fn_15_AC   0x08  false
//   0x0B4 fn_15_B4   0x08  false
//   0x0BC fn_15_BC   0x1C  copy self[0x54..0x5c] into *out
//   0x0D8 fn_15_D8   0x2C  vtable call, slot 0x38
//   0x104 RELExit    0x24  li r3,0 / bl fn_80218AD4
//   0x128 RELMain    0x20  bl fn_15_148
//   0x148 fn_15_148  0x30  lbl_15_bss_0 = fn_15_178 ; fn_80218AD4(&lbl_15_bss_0)
//
// **This head is MysteryFlyer's, re-ordered, and the diff is measured rather than assumed**: over
// the ranges each claims (`0x0..0x178` here, `0x0..0x170` in MysteryFlyer) the two `.text`
// listings are 94 instructions against 92, in the same relative order, and the only differences are
// the two leading member-address accessors
// (`addi r3,r3,0xa50` and `addi r3,r3,0x9c0` where MysteryFlyer opens with `li r3,1` and
// `addi r3,r3,0x818`), one extra `li r3,0; blr` predicate in the run before the three-float
// copy, and the four `bl` displacements. So every body below is the one `CMysteryFlyerRel.cpp` already reproduces at 100%, and the
// read-off counts differ and must be read out of this module's own `symbols.txt`, never a
// sibling's.
//
// **`fn_15_10` is not an `optional_object` problem**, though it returns one, and the note in
// `CMysteryFlyerRel.cpp` says why. Retail *calls* the converting constructor, out of line at
// .text:0xC094 (`fn_15_C094`, 0x3C bytes, the module's own copy of the same function MysteryFlyer
// has at 0x2BBC), and this unit does not claim it. So `fn_15_10` is written as the free function
// it compiles to - hidden return pointer in r3, `self` in r4 - and calls `fn_15_C094` by its dtk
// name. `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own return
// pointer is r3), and the temporary is the 0x18-byte `CAABox` (two `CVector3f`; `fn_15_C094` sets
// the flag at 0x18) at r1+0x8, which is what makes the frame 0x30 and not 0x10.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_15_178 (0x178, 0xACC), the module's own entity loader: behavioural class code, and it needs
// the CActor/CPatterned/CAi hierarchy this tree does not model. The 217 text functions from
// fn_15_178 on - 241 symbols less this head's 19 and the five REL_Setup ones - are the class's
// own methods and stay retail for the same reason.
//
// The two callees of the registration path are named by what they are, not invented:
//   - `fn_80218AD4` is the DOL's 0x80218AD4, the module's own import-table name for the two-
//     instruction setter `stw r3, gLoader_ElitePirate; blr`
//     (`build/G2ME01/asm/auto_03_80218AD4_text.s`; `config/G2ME01/symbols.txt:9480` already names
//     it, so no `symbols.txt` rename and no DOL change). So it stores the *address* of a loader
//     slot, not a loader, which is why the store below hands it `&lbl_15_bss_0` and why that slot
//     is four bytes wide. The thunk beside it is `src/MetroidPrime/ScriptLoader/ElitePirate.cpp`,
//     which reads the slot as `(*gLoader_ElitePirate.value)(...)` and records why the setter is
//     deliberately unclaimed in the DOL.
//   - `lbl_15_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **fn_15_D8 is a vtable call, and the stand-in class carries thirteen virtuals.** Retail's call
// is `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`; loading the vtable by hand compiles
// to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99% on the
// function. `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a
// class's virtuals out the way retail's vtable is laid out - two leading words (offset-to-top,
// then the RTTI pointer, both zero in this REL) and one word per virtual - so thirteen virtuals
// put the last one at 0x38. `fn_15_D8` is entry 0x3C of the 140-word table at `.data:0x634`,
// which carries `TypesMatch__12CElitePirateCFi`, `PreThink__10CPatternedFfR13CStateManager` and
// `HealthInfo__3CAiFv`, and entry 0x38 is that `HealthInfo` - the `fn_12_8C` / `fn_45_D0` shape.
// None of the slots is defined; the only object carrying this vtable is the module's own retail
// bytes.
//
// **fn_15_BC is a copy, not a constructor call.** Retail reuses f0 for all three
// (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
// load/store pairs. A CVector3f constructor would hoist them into f0/f1/f2 and is the wrong
// shape. The disassembly says which: interleaved is a copy.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and the module hash breaking on a few bytes.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 would break on exactly
// that with every function at 100%. Only the mangled name has to agree, and one const member
// gives it. See `CMysteryFlyerRel.cpp` for the same stand-in.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_15_D8 dispatches to; see the note at the top of the file.
class CElitePirateVTable {
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
  virtual void* Slot12();
};

extern "C" {
extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// fn_15_178, the module's own entity loader, 0x178, 0xACC: left retail, named here only so the
// registration below can store its address.
CEntity* fn_15_178(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80218AD4(FScriptLoader* loader);
// .text 0xC094, 0x3C bytes, unclaimed: `optional_object<CAABox>`'s converting constructor, out
// of line - the same function MysteryFlyer's `fn_45_10` calls at its 0x2BBC.
void fn_15_C094(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_15_bss_0;
#else
FScriptLoader lbl_15_bss_0 = 0;
#endif

// .text 0x148, 0x30 bytes. lbl_15_bss_0 = fn_15_178 ; fn_80218AD4(&lbl_15_bss_0). Retail loads
// the loader out of `.text` and the slot address out of `.bss`, and stores the loader with
// `stwu` so the store writes the slot itself and leaves r3 holding its address for the setter
// call.
void fn_15_148() {
  lbl_15_bss_0 = fn_15_178;
  fn_80218AD4(&lbl_15_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CMysteryFlyerRel.cpp`,
// `CMetareeSwarmRel.cpp` and `CDarkTrooperRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_15_178` and `fn_80218AD4`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. So the port keeps reading `ElitePirate.rel` off the disc
// through `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid
// translation unit.
#ifdef __MWERKS__
void RELMain() { fn_15_148(); }

void RELExit() { fn_80218AD4(nullptr); }
#else
void mp_relmain_elitepirate() { fn_15_148(); }

void mp_relexit_elitepirate() { fn_80218AD4(nullptr); }
#endif

// .text 0xD8, 0x2C bytes. a member call into vtable slot 0x38 (CAi's HealthInfo).
void fn_15_D8(CElitePirateVTable* self) { self->Slot12(); }

// .text 0xBC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// copies a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. A copy, not a constructed
// CVector3f - see the note at the top.
void fn_15_BC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xB4, 0x08 bytes. a predicate that is always false.
bool fn_15_B4(void*) { return false; }

// .text 0xAC, 0x08 bytes. a predicate that is always false.
bool fn_15_AC(void*) { return false; }

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_15_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_15_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0C bytes. a constant float out of the DOL.
float fn_15_90(void*) { return lbl_8041B758; }

// .text 0x84, 0x0C bytes. the flag at +0x34c - `extrwi r3,r0,1,28`, the fourth `bool : 1` of its
// byte. Same source as `AtomicBetaAccessors.cpp`'s `fn_5_48` and `CDarkTrooperRel.cpp`'s
// `fn_12_38` over the same offset, and it transfers verbatim: the two objects hold the same
// three instructions, `88 03 03 4C / 54 03 EF FE / 4E 80 00 20`.
bool fn_15_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x74, 0x10 bytes. resets the unique id to the invalid value.
void fn_15_74(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_15_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_15_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_15_5C(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x4C, 0x10 bytes. stores the default float at +0x448.
void fn_15_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_15_10(void* out, const CPhysicsActor* self) {
  fn_15_C094(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. the address of the member at +0x9c0.
void* fn_15_8(void* self) { return static_cast< char* >(self) + 0x9C0; }

// .text 0x0, 0x08 bytes. the address of the member at +0xa50.
void* fn_15_0(void* self) { return static_cast< char* >(self) + 0xA50; }
}
