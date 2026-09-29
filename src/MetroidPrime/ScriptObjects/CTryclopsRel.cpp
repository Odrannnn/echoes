// CTryclopsRel.cpp - Tryclops' (module 81) head, .text 0x0..0x178: the nineteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` and
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Tryclops/symbols.txt`:
//
//   0x0   fn_81_0   0x08   addi r3,r3,0x7c4
//   0x8   fn_81_8   0x08   li r3,1
//   0x10  fn_81_10  0x3C   GetBoundingBox into a local, then fn_81_4FEC(out, &box)
//   0x4C  fn_81_4C  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
//   0x5C  fn_81_5C  0x08   the byte at +0x44F
//   0x64  fn_81_64  0x08   li r3,0
//   0x6C  fn_81_6C  0x08   li r3,0
//   0x74  fn_81_74  0x08   li r3,0
//   0x7C  fn_81_7C  0x10   *self = kInvalidUniqueId
//   0x8C  fn_81_8C  0x0C   the byte at +0x34C, bit 3
//   0x98  fn_81_98  0x0C   lbl_8041B758
//   0xA4  fn_81_A4  0x08   addi r3,r3,0x754
//   0xAC  fn_81_AC  0x08   li r3,1
//   0xB4  fn_81_B4  0x08   li r3,0
//   0xBC  fn_81_BC  0x1C   three floats from self+0x54 -> *out
//   0xD8  fn_81_D8  0x2C   virtual dispatch, vtable slot 0x38
//   0x104 RELExit   0x24   li r3,0 / bl fn_80218D58
//   0x128 RELMain   0x20   bl fn_81_148
//   0x148 fn_81_148 0x30   lbl_81_bss_30 = fn_81_178 ; fn_80218D58(&lbl_81_bss_30)
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_81_178 (0x178, 0x30C), the module's own entity loader, and the 89 functions from there to
// fn_81_5028 are Tryclops' methods: behavioural class code, and it needs the
// CActor/CPatterned hierarchy this tree does not model.
//
// The claim starts at 0x0. `fn_81_10` returns `rstl::optional_object<CAABox>` through a hidden
// pointer, and an earlier version of this file stopped at 0x4C believing the template's
// constructor could not be reproduced. It does not have to be: retail calls it out of line at
// 0x4FEC, which stays unclaimed, exactly as `CMysteryFlyerRel.cpp`'s `fn_45_10` does.
//
// The thirteen-accessor block is AtomicAlpha's, and that is measured rather than assumed: the two
// accessor blocks, this module's `.text 0x4C..0xD8` and AtomicAlpha's `.text 0x10..0x9C`, are both
// 0x8C = 140 bytes and **35 instructions** with an identical instruction multiset. The diff moves
// two lines and adds none - Tryclops runs three `li r3,0; blr` predicates immediately after the
// byte read (0x64, 0x6C, 0x74) where AtomicAlpha runs two, its third sitting later beside the
// `li r3,0x1`. (46 is the count only for the ranges extended through the vtable entry, 0x4C..0x104
// and 0x10..0xC8.) So no spelling had to be discovered.
// Every body below is the one `CAtomicAlphaRel.cpp` / `AtomicBetaAccessors.cpp` carries, and the
// two odd ones carry over with their caveats:
//   - `fn_81_BC` loads and stores *interleaved* (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 /
//     stfs f0,4 / lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores. Do not
//     "improve" it into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in
//     spelling reverses the loads and the score falls.
//   - `fn_81_8C` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
//     `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
//     `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
//
// `fn_81_D8` is a vtable entry, not a free function: dtk puts it in the module's FORCEACTIVE
// list, and `.data:0x378` - a 0x148-byte table, two leading words plus one per virtual -
// stores it at offset 0x3C while offset 0x38 is `HealthInfo__3CAiFv`. So the call below is a
// member call, and that spelling is measured rather than guessed. Loading the vtable by hand -
// `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` - compiles to
// `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the
// function (measured in `CIngPuddleRel.cpp`, 2026-09-29, where the same bytes are `fn_32_8`).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out. Thirteen virtuals therefore put the last one at 0x38,
// and calling it gives retail's seven instructions byte for byte. The slots are named by position
// because no header here models a CPatterned virtual; none of them is defined or called from this
// file, because the only object that carries this vtable is the module's own retail bytes. Slot
// 12's return type is float because `.data:0x378` names it `HealthInfo__3CAiFv`; the call
// discards it, so this does not affect the bytes, but naming it anything else would misdescribe
// the vtable.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218D58` is the DOL's 0x80218D58, two instructions,
//     `stw r3, gLoader_Tryclops@sda21(r0); blr`. So it stores the *address* of a loader slot, not
//     a loader. `LoadTryclops` reads it as `lwz r6, gLoader_Tryclops; lwz r12, 0(r6); mtctr r12;
//     bctrl`, which is why the store below hands it `&lbl_81_bss_30` and why that slot is four
//     bytes wide. Unlike AtomicAlpha's setter this one is already a plain DOL symbol
//     (`strings build/G2ME01/Tryclops/Tryclops.preplf` gives `fn_80218D58`, not a mangled
//     `SetLoader_*`), so it needs no declaration trick to reach the retail name.
//   - `lbl_81_bss_30` is `.bss:0x30`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%. Only `flip_test.sh` catches that.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" const float lbl_8041AAB8;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// `GetBoundingBox__13CPhysicsActorCFv` through a one-method stand-in, not
// `MetroidPrime/CPhysicsActor.hpp`: that header puts 0x28 bytes of `.data` in this object (see
// `CMysteryFlyerRel.cpp`, where the module sha1 broke on exactly that).
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_81_D8 dispatches to; see the note above on the layout.
class CTryclopsDispatch {
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
CEntity* fn_81_178(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218D58(FScriptLoader* loader);
// .text 0x4FEC, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_81_4FEC(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_81_bss_30;
#else
FScriptLoader lbl_81_bss_30 = 0;
#endif

void fn_81_148() {
  lbl_81_bss_30 = fn_81_178;
  fn_80218D58(&lbl_81_bss_30);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CAtomicAlphaRel.cpp` and `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_81_178`, which it cannot, and `tools/link_check.sh --strict` fails on a
// growing undefined count. So the port keeps reading `Tryclops.rel` off the disc through
// `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid translation
// unit.
#ifdef __MWERKS__
void RELMain() { fn_81_148(); }

void RELExit() { fn_80218D58(nullptr); }
#else
void mp_relmain_tryclops() { fn_81_148(); }

void mp_relexit_tryclops() { fn_80218D58(nullptr); }
#endif

// .text 0xD8, 0x2C bytes. Vtable entry 0x3C of Tryclops; the call it makes targets vtable slot
// 0x38, which `.data:0x378` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CTryclopsDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_81_D8(CTryclopsDispatch* self) { self->Slot12(); }

// .text 0xBC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_81_BC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xB4, 0x08 bytes. a predicate that is always false.
bool fn_81_B4(void*) { return false; }

// .text 0xAC, 0x08 bytes. a predicate that is always true.
bool fn_81_AC(void*) { return true; }

// .text 0xA4, 0x08 bytes. the address of the member at +0x754.
void* fn_81_A4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x98, 0x0C bytes. a constant float out of the DOL.
float fn_81_98(void*) { return lbl_8041B758; }

// .text 0x8C, 0x0C bytes. the flag at +0x34c - dtk's `extrwi r3, r0, 1, 28` is
// `rlwinm r3, r0, 29, 31, 31`, the third `bool : 1` of its byte.
bool fn_81_8C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x7C, 0x10 bytes. resets the unique id to the invalid value.
void fn_81_7C(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x74, 0x08 bytes. a predicate that is always false.
bool fn_81_74(void*) { return false; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_81_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_81_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_81_5C(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x4C, 0x10 bytes. stores the default float at +0x448. The cast has to be to `char*` and
// not to a pointer-to-pointer: `sizeof(char*)` is 4 on this ABI, so a `char**` would scale the
// displacement by four and still look like an offset into an object.
void fn_81_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}

// .text 0x10, 0x3C bytes. returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// return pointer; the constructor is called out of line, as `CMysteryFlyerRel.cpp`'s `fn_45_10`.
void fn_81_10(void* out, const CPhysicsActor* self) {
  fn_81_4FEC(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_81_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the member at +0x7c4.
void* fn_81_0(void* self) { return static_cast< char* >(self) + 0x7C4; }
}
