// CPillBugRel.cpp - PillBug's (module 48) head, .text 0x0..0x130: the seventeen functions
// above the module's class code. The arrangement matches
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`, `CMetareeSwarmRel.cpp` and
// `CIngPuddleRel.cpp`; the ranges come from `config/G2ME01/rels/PillBug/symbols.txt`:
//
//   0x00  fn_48_0    0x10  store the default float at +0x448
//   0x10  fn_48_10   0x08  the byte at +0x44f
//   0x18  fn_48_18   0x08  false      0x20 fn_48_20 0x08 false
//   0x28  fn_48_28   0x08  false      0x30 fn_48_30 0x08 false
//   0x38  fn_48_38   0x10  reset the unique id to the invalid value
//   0x48  fn_48_48   0x0C  a constant float out of the DOL
//   0x54  fn_48_54   0x08  the address of the member at +0x754
//   0x5C  fn_48_5C   0x08  true       0x64 fn_48_64 0x08 false
//   0x6C  fn_48_6C   0x08  false
//   0x74  fn_48_74   0x1C  copy self[0x54..0x5c] into *out
//   0x90  fn_48_90   0x2C  vtable call, slot 0x38
//   0xBC  RELExit    0x24  li r3,0 / bl fn_80200F30
//   0xE0  RELMain    0x20  bl fn_48_100
//   0x100 fn_48_100  0x30  lbl_48_bss_0 = fn_48_130 ; fn_80200F30(&lbl_48_bss_0)
//
// The thirteen short accessors at 0x0..0x90 are the accessor set the REL loader generator emits
// at the head of a scripted-actor module. The same thirteen already reproduce, in the DOL, as
// `MetroidPrime/ScriptObjects/GlowbugAccessors.cpp` and the other `*Accessors.cpp` units, sharing
// the three globals the relocations name - `lbl_8041AAB8`, `kInvalidUniqueId` and `lbl_8041B758`
// - all of which live in the DOL, so one body serves every module of the family. PillBug's
// fourteenth function, fn_48_90, is a vtable call the Glowbug block does not have, so this is the
// same thirteen, not the same fourteen: read the offsets out of this module's own symbols.txt,
// never a sibling's.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_48_130 (0x130, 0x5A4), the module's own entity loader: behavioural class code, and it needs
// the CActor/CPatterned/CAi hierarchy this tree does not model.
//
// The two callees of the registration path are named by what they are, not invented:
//   - `fn_80200F30` is the DOL's 0x80200F30, two instructions, `stw r3, gLoader_PillBug; blr`
//     (the displacement resolves through _SDA_BASE_ to 0x80419370, which config/G2ME01/symbols.txt
//     names gLoader_PillBug). So it stores the *address* of a loader slot, not a loader.
//     `src/MetroidPrime/ScriptLoader/PillBug.cpp` reads it as `(*gLoader_PillBug.value)(...)`,
//     which is why the store below hands it `&lbl_48_bss_0` and why that slot is four bytes wide.
//   - `lbl_48_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **fn_48_90 calls vtable slot 0x38, and the stand-in class carries thirteen virtuals.** Retail's
// is `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`. Loading the vtable by hand -
// `void* const* vt = *(void* const* const*)self;` - compiles to `lwz r3,0(r3)` where retail has
// `lwz r12,0(r3)`, and that one register is 99% on the function. `mwcceppc` only reaches for r12
// on its own virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is
// laid out - two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and
// one word per virtual - so thirteen virtuals put the last one at 0x38 and calling it gives retail's
// twelve instructions byte for byte. The slot is CAi's HealthInfo: PillBug's table is CAi's, not
// CActor's (TypesMatch__8CPillBugCFi and DamageVulnerability__3CAiFv are in it). None of the slots
// is defined; the only object carrying the vtable is the module's own retail bytes.
//
// **fn_48_74 is a copy, not a constructor call.** Retail reuses f0 for all three
// (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
// load/store pairs. The three-argument CVector3f constructor, which is what CMetareeSwarmRel.cpp's
// fn_43_3C needs because retail there loads all three before storing any, would hoist them into
// f0/f1/f2 and is the wrong shape here. The disassembly says which: interleaved is a copy.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and the module hash breaking on a few bytes.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"
#include "types.h"

class CEntity;
class CStateManager;
class CInputStream;
class CEntityInfo;

// The stand-in vtable for fn_48_90. Thirteen virtuals; the call is to the last one, at 0x38.
class CPillBugVTable {
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
extern "C" const float lbl_8041AAB8;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// fn_48_130, the module's own entity loader, 0x130, 0x5A4: left retail, named here only so the
// registration below can store its address.
CEntity* fn_48_130(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80200F30(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_48_bss_0;
#else
FScriptLoader lbl_48_bss_0 = 0;
#endif

// .text 0x100, 0x30 bytes. lbl_48_bss_0 = fn_48_130 ; fn_80200F30(&lbl_48_bss_0).
void fn_48_100() {
  lbl_48_bss_0 = fn_48_130;
  fn_80200F30(&lbl_48_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp` and `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make the
// port link `fn_48_130` and `fn_80200F30`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. The port keeps reading `PillBug.rel` off the disc through
// `platform/rel.cpp`.
#ifdef __MWERKS__
void RELMain() { fn_48_100(); }

void RELExit() { fn_80200F30(nullptr); }
#else
void mp_relmain_pillbug() { fn_48_100(); }

void mp_relexit_pillbug() { fn_80200F30(nullptr); }
#endif

// .text 0x90, 0x2C bytes. a member call into vtable slot 0x38 (CAi's HealthInfo).
void fn_48_90(CPillBugVTable* self) { self->Slot12(); }

// .text 0x74, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// copies a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. A copy, not a constructed
// CVector3f - see the note at the top.
void fn_48_74(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_48_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_48_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. a predicate that is always true.
bool fn_48_5C(void*) { return true; }

// .text 0x54, 0x08 bytes. the address of the member at +0x754.
void* fn_48_54(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x48, 0x0C bytes. a constant float out of the DOL.
float fn_48_48(void*) { return lbl_8041B758; }

// .text 0x38, 0x10 bytes. resets the unique id to the invalid value.
void fn_48_38(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x30, 0x08 bytes. a predicate that is always false.
bool fn_48_30(void*) { return false; }

// .text 0x28, 0x08 bytes. a predicate that is always false.
bool fn_48_28(void*) { return false; }

// .text 0x20, 0x08 bytes. a predicate that is always false.
bool fn_48_20(void*) { return false; }

// .text 0x18, 0x08 bytes. a predicate that is always false.
bool fn_48_18(void*) { return false; }

// .text 0x10, 0x08 bytes. the byte at +0x44f.
unsigned char fn_48_10(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x0, 0x10 bytes. stores the default float at +0x448.
void fn_48_0(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}
}
