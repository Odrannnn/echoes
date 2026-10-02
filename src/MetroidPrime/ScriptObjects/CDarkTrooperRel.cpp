// CDarkTrooperRel.cpp - DarkTrooper's (module 12) head, .text 0x0..0x12C: the sixteen functions
// above the module's class code. The arrangement matches
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`, `CMetareeSwarmRel.cpp` and
// `CPillBugRel.cpp`; the ranges come from `config/G2ME01/rels/DarkTrooper/symbols.txt`:
//
//   0x00  fn_12_0    0x08  the address of the member at +0x7c0
//   0x08  fn_12_8    0x10  store the default float at +0x448
//   0x18  fn_12_18   0x08  the byte at +0x44f
//   0x20  fn_12_20   0x08  false
//   0x28  fn_12_28   0x10  reset the unique id to the invalid value
//   0x38  fn_12_38   0x0C  the flag at +0x34c - `extrwi r3,r0,1,28`
//   0x44  fn_12_44   0x0C  a constant float out of the DOL
//   0x50  fn_12_50   0x08  the address of the member at +0x754
//   0x58  fn_12_58   0x08  true       0x60 fn_12_60 0x08 false
//   0x68  fn_12_68   0x08  false
//   0x70  fn_12_70   0x1C  copy self[0x54..0x5c] into *out
//   0x8C  fn_12_8C   0x2C  vtable call, slot 0x38
//   0xB8  RELExit    0x24  li r3,0 / bl fn_80218DF4
//   0xDC  RELMain    0x20  bl fn_12_FC
//   0xFC  fn_12_FC   0x30  lbl_12_bss_0 = fn_12_12C ; fn_80218DF4(&lbl_12_bss_0)
//
// **This head is PillBug's, re-ordered, and the diff is measured rather than assumed**: both
// `.text 0x0..0xBC`-ish accessor runs come from the same generator, the same three DOL
// relocations (`skDamageHitTime__10CPatterned`, `kInvalidUniqueId`, `lbl_8041B758`) and the same three float-copy
// and vtable-call shapes, so every body below is the one `CPillBugRel.cpp` already reproduces at
// 100% and `AtomicBetaAccessors.cpp` reproduces in the DOL. PillBug's block opens with the 0x10-byte
// float store and runs four `li r3,0; blr` predicates in the run right after the byte read, six
// over the block; this one opens with an 8-byte member-address accessor, moves the float store to
// 0x08, runs one in that run and three over the block, and carries one function PillBug's does not
// have - `fn_12_38`. So the read-off counts differ and must be read out of this module's own
// `symbols.txt`, never a sibling's.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_12_12C (0x12C, 0x614), the module's own entity loader: behavioural class code, and it needs
// the CActor/CPatterned/CAi hierarchy this tree does not model.
//
// The two callees of the registration path are named by what they are, not invented:
//   - `fn_80218DF4` is the DOL's 0x80218DF4, the module's own import-table name for the two-
//     instruction setter `stw r3, gLoader_DarkTrooper; blr` (the displacement resolves through
//     _SDA_BASE_ to 0x80419468, which config/G2ME01/symbols.txt names gLoader_DarkTrooper; the
//     thunk beside it is `src/MetroidPrime/ScriptLoader/DarkTrooper.cpp`, which reads the slot as
//     `(*gLoader_DarkTrooper.value)(...)`). So it stores the *address* of a loader slot, not a
//     loader, which is why the store below hands it `&lbl_12_bss_0` and why that slot is four
//     bytes wide. A friendlier name will not link.
//   - `lbl_12_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **fn_12_38 and fn_12_8C are one bit test and one vtable call, and the stand-in class carries
// thirteen virtuals.** Retail's call is `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`;
// loading the vtable by hand compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that
// one register is 99% on the function. `mwcceppc` only reaches for r12 on its own virtual-dispatch
// path, and it lays a class's virtuals out the way retail's vtable is laid out - two leading words
// (offset-to-top, then the RTTI pointer, both zero in this REL) and one word per virtual - so
// thirteen virtuals put the last one at 0x38 and calling it gives retail's twelve instructions
// byte for byte. The slot is CAi's HealthInfo: DarkTrooper's table (`lbl_12_data_2BC`, 0x1A8
// bytes) carries `TypesMatch__10CPatternedCFi`, `DamageVulnerability__3CAiFv` and
// `HealthInfo__3CAiFv`, and `fn_12_8C` is its entry at 0x3C - entry 0x3C calling slot 0x38, which
// is the `fn_6_0` / `fn_32_8` shape. None of the slots is defined; the only object carrying the
// vtable is the module's own retail bytes.
//
// **fn_12_70 is a copy, not a constructor call.** Retail reuses f0 for all three
// (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
// load/store pairs. The three-argument CVector3f constructor, which `CMetareeSwarmRel.cpp`'s
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

// The stand-in vtable for fn_12_8C. Thirteen virtuals; the call is to the last one, at 0x38.
class CDarkTrooperVTable {
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

// fn_12_12C, the module's own entity loader, 0x12C, 0x614: left retail, named here only so the
// registration below can store its address.
CEntity* fn_12_12C(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218DF4(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_12_bss_0;
#else
FScriptLoader lbl_12_bss_0 = 0;
#endif

// .text 0xFC, 0x30 bytes. lbl_12_bss_0 = fn_12_12C ; fn_80218DF4(&lbl_12_bss_0).
void fn_12_FC() {
  lbl_12_bss_0 = fn_12_12C;
  fn_80218DF4(&lbl_12_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CMetareeSwarmRel.cpp` and
// `CPillBugRel.cpp`: listing this file in files.cmake would make the port link `fn_12_12C` and
// `fn_80218DF4`, which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined
// count. The port keeps reading `DarkTrooper.rel` off the disc through `platform/rel.cpp`.
#ifdef __MWERKS__
void RELMain() { fn_12_FC(); }

void RELExit() { fn_80218DF4(nullptr); }
#else
void mp_relmain_darktrooper() { fn_12_FC(); }

void mp_relexit_darktrooper() { fn_80218DF4(nullptr); }
#endif

// .text 0x8C, 0x2C bytes. a member call into vtable slot 0x38 (CAi's HealthInfo).
void fn_12_8C(CDarkTrooperVTable* self) { self->Slot12(); }

// .text 0x70, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// copies a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. A copy, not a constructed
// CVector3f - see the note at the top.
void fn_12_70(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x68, 0x08 bytes. a predicate that is always false.
bool fn_12_68(void*) { return false; }

// .text 0x60, 0x08 bytes. a predicate that is always false.
bool fn_12_60(void*) { return false; }

// .text 0x58, 0x08 bytes. a predicate that is always true.
bool fn_12_58(void*) { return true; }

// .text 0x50, 0x08 bytes. the address of the member at +0x754.
void* fn_12_50(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x44, 0x0C bytes. a constant float out of the DOL.
float fn_12_44(void*) { return lbl_8041B758; }

// .text 0x38, 0x0C bytes. the flag at +0x34c - `extrwi r3,r0,1,28`, the fourth `bool : 1` of its
// byte. Same source as `AtomicBetaAccessors.cpp`'s `fn_5_48` over the same offset, and it
// transfers verbatim: the two objects hold the same three instructions,
// `88 03 03 4C / 54 03 EF FE / 4E 80 00 20`. That unit's own comment says `rlwinm r3,r0,29,31,31`
// and is stale - the word appears nowhere in `build/`.
bool fn_12_38(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x28, 0x10 bytes. resets the unique id to the invalid value.
void fn_12_28(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x20, 0x08 bytes. a predicate that is always false.
bool fn_12_20(void*) { return false; }

// .text 0x18, 0x08 bytes. the byte at +0x44f.
unsigned char fn_12_18(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x8, 0x10 bytes. stores the default float at +0x448.
void fn_12_8(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x0, 0x08 bytes. the address of the member at +0x7c0.
void* fn_12_0(void* self) { return static_cast< char* >(self) + 0x7C0; }
}
