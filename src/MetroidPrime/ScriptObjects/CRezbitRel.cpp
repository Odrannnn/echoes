// CRezbitRel.cpp - Rezbit's (module 53) head, .text 0x0..0x168: the seventeen functions above
// the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, `CMetroidRel.cpp` and `CMediumIngRel.cpp`,
// and the ranges come from `config/G2ME01/rels/Rezbit/symbols.txt`:
//
//   0x000 fn_53_0   0x08  addi r3,r3,0xac0
//   0x008 fn_53_8   0x08  li r3,1
//   0x010 fn_53_10  0x3C  GetBoundingBox into a local, then fn_53_88A4(out, &box)
//   0x04C fn_53_4C  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x05C fn_53_5C  0x08  lbz r3, 0x44f(r3)
//   0x064 fn_53_64  0x08  li r3,0
//   0x06C fn_53_6C  0x08  li r3,0
//   0x074 fn_53_74  0x10  *self = kInvalidUniqueId
//   0x084 fn_53_84  0x0C  byte at +0x34c, bit 3
//   0x090 fn_53_90  0x0C  lbl_8041B758
//   0x09C fn_53_9C  0x08  addi r3,r3,0x754
//   0x0A4 fn_53_A4  0x08  li r3,1
//   0x0AC fn_53_AC  0x1C  three floats from self+0x54 -> *out
//   0x0C8 fn_53_C8  0x2C  virtual dispatch, vtable slot 0x38
//   0x0F4 RELExit   0x24  li r3,0 / bl fn_80227AF8
//   0x118 RELMain  0x20  bl fn_53_138
//   0x138 fn_53_138 0x30  lbl_53_bss_0 = fn_53_168 ; fn_80227AF8(&lbl_53_bss_0)
//
// **The block is MysteryFlyer's with two differences, and both are measured rather than assumed
// from the `fn_<id>_<off>` names, which say nothing about which function is which.** Diffing
// `build/G2ME01/Rezbit/asm/auto_00_00000000_text.s` over the 0x168 this claims against
// `CMysteryFlyerRel.cpp`'s 0x170 instruction for instruction: the `skDamageHitTime__10CPatterned` store at +0x448,
// the +0x44f byte, the `kInvalidUniqueId` store, the +0x34c bit test, the `lbl_8041B758`
// accessor, the +0x754 address, the three-float copy, the vtable-0x38 dispatch and the whole
// RELExit/RELMain/registration trio are the same bodies. The two differences are:
//   - **the two leading eight-byte accessors are swapped in a way the family has no precedent
//     for**: this module opens `addi r3,r3,0xac0` (the address of a member) and then `li r3,1`,
//     where MysteryFlyer opens `li r3,1` and then `addi r3,r3,0x818`, Metroid opens
//     `addi r3,r3,0x8c8` and then `li r3,1`, and MediumIng opens `addi r3,r3,0x7c0` and then puts
//     the wrapper at 0x8;
//   - it runs **two** `li r3,0` predicates in the block where MysteryFlyer runs three, so the
//     three-float copy sits at 0xAC rather than 0xB4 and the whole tail - dispatch, RELExit,
//     RELMain, registration - is eight bytes lower than MysteryFlyer's, which is why the claim
//     ends at 0x168 and not 0x170.
// So this is one function short of MysteryFlyer's count, not because a body is missing but
// because this module's block has two `li r3,0` where the family's has three.
//
// `fn_53_10` is not an `optional_object` problem, though it returns one, and it is not a
// template-instantiation problem either: retail calls the converting constructor *out of line* at
// 0x88A4 (`li r0,1; lwz r5,0(r4); stb r0,0x18(r3)` then six word copies out of `r4+0x00..r4+0x14`,
// i.e. six words of `CAABox` and a validity byte at +0x18), and that function stays unclaimed. So
// `fn_53_10` is one call, `fn_53_88A4(out, self->GetBoundingBox())`, with the constructor declared
// by its dtk name exactly as `CMysteryFlyerRel.cpp` declares `fn_45_2BBC`. `self` needs no move
// because `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3); the frame is
// `-0x30` for the 0x18-byte temporary, and the constructor parameter must be a `const CAABox&` or
// the frame grows to 0x40.
//
// Everything from fn_53_168 (0x168, 0x330), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model. The
// 146 functions above it are the module's methods and stay retail. So dtk fills them and the
// module's sha1 against `config/G2ME01/config.yml` still holds.
//
// **No dead-strip hazard, and that is measured**: `build/G2ME01/Rezbit/ldscript.lcf` puts all
// fifteen of `fn_53_0`..`fn_53_C8` in its FORCEACTIVE list, and `fn_53_168` - the loader
// registration stores - is in it too, so nothing here needs a `force_active:` entry in
// `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the
// module's own entry points and `fn_53_138` is called from `RELMain`, so that trio survives for
// the reason it does everywhere in this family.
//
// The two callees are named by what they are, not invented:
//   - `fn_80227AF8` is the DOL's 0x80227AF8, two instructions,
//     `stw r3, gLoader_Rezbit@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80227AF8_text.s`),
//     immediately after `LoadRezbit__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
//     0x80227ACC, which is 44 bytes and so ends exactly at 0x80227AF8. So it stores the
//     *address* of a loader slot, not a loader.
//     `src/MetroidPrime/ScriptLoader/Rezbit.cpp` - a `Matching` unit, and the file that already
//     records why the setter is deliberately not claimed in the DOL - reads the slot as
//     `(*gLoader_Rezbit.value)(mgr, input, info)`, which is why the store below hands it
//     `&lbl_53_bss_0` and why that slot is four bytes wide. The import name is the plain
//     `fn_80227AF8` that `config/G2ME01/symbols.txt` already gives it - not the long MWCC-mangled
//     `SetLoader_...` form `CIngSnatchingSwarmRel.cpp` had to spell out - so no `symbols.txt`
//     rename is needed and the DOL is untouched. It is `extern "C"`: an alias would be a
//     different symbol and the call would resolve to nothing.
//   - `lbl_53_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims `.text` only, so dtk's `.bss` object has to define it,
//     and a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition. **The `.bss` size is the
//     cheap check to read first**: `build/G2ME01/Rezbit/asm/auto_05_00000000_bss.s` gives
//     `lbl_53_bss_0` `size:0x4` where `CMetroidRel.cpp` has to place its slot at `.bss:0x10`,
//     because MetroidAlpha's record is a `__ptmf_scall` target as well as a loader.
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
// with every function at 100% (`CMysteryFlyerRel.cpp` measured it). Only the mangled name has to
// agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_53_C8 dispatches to; see `CMysteryFlyerRel.cpp` for the layout. mwcceppc lays
// a class's virtuals out the way retail's vtable is laid out - two leading words (offset-to-top,
// then the RTTI pointer, both zero in this REL) and then one word per virtual - so thirteen
// virtuals put the thirteenth at 0x38. The slots are named by position because no header here
// models a CPatterned virtual, and none of them is defined or called from this file, because the
// only object that carries this vtable is the module's own retail bytes. Slot 12 returns float
// because the table names it `HealthInfo__3CAiFv`; the call discards it.
class CRezbitDispatch {
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
CEntity* fn_53_168(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80227AF8(FScriptLoader* loader);
// .text 0x88A4, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_53_88A4(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_53_bss_0;
#else
FScriptLoader lbl_53_bss_0 = 0;
#endif

// .text 0x138, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_53_138() {
  lbl_53_bss_0 = fn_53_168;
  fn_80227AF8(&lbl_53_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CMetroidRel.cpp` and `CMediumIngRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_53_168` and `fn_80227AF8`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `Rezbit.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so the
// file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_53_138(); }

void RELExit() { fn_80227AF8(nullptr); }
#else
void mp_relmain_rezbit() { fn_53_138(); }

void mp_relexit_rezbit() { fn_80227AF8(nullptr); }
#endif

// .text 0xC8, 0x2C bytes. The call targets vtable slot 0x38.
void fn_53_C8(CRezbitDispatch* self) { self->Slot12(); }

// .text 0xAC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_53_AC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_53_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_53_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0C bytes. a constant float out of the DOL.
float fn_53_90(void*) { return lbl_8041B758; }

// .text 0x84, 0x0C bytes. the flag at +0x34c, bit 3. dtk prints the middle word
// `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
// opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
bool fn_53_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x74, 0x10 bytes. resets the unique id to the invalid value.
void fn_53_74(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_53_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_53_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_53_5C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4C, 0x10 bytes. stores a DOL float at +0x448.
void fn_53_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_53_10(void* out, const CPhysicsActor* self) {
  fn_53_88A4(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_53_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the address of the member at +0xac0.
void* fn_53_0(void* self) { return static_cast< char* >(self) + 0xAC0; }
}
