// CLumiteRel.cpp - Lumite's (module 39) head, .text 0x0..0x190: the seventeen functions above
// the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Lumite/symbols.txt`:
//
//   0x000 fn_39_0   0x68  GetBoundingBox, then an **inlined** optional_object<CAABox>
//   0x068 fn_39_68  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
//   0x078 fn_39_78  0x08  lbz r3, 0x44f(r3)
//   0x080 fn_39_80  0x08  li r3,0
//   0x088 fn_39_88  0x08  li r3,0
//   0x090 fn_39_90  0x08  li r3,0
//   0x098 fn_39_98  0x10  *self = kInvalidUniqueId
//   0x0A8 fn_39_A8  0x0C  byte at +0x34c, bit 3
//   0x0B4 fn_39_B4  0x08  addi r3,r3,0x754
//   0x0BC fn_39_BC  0x08  li r3,1
//   0x0C4 fn_39_C4  0x08  li r3,0
//   0x0CC fn_39_CC  0x08  li r3,0
//   0x0D4 fn_39_D4  0x1C  three floats from self+0x54 -> *out
//   0x0F0 fn_39_F0  0x2C  virtual dispatch, vtable slot 0x38
//   0x11C RELExit   0x24  li r3,0 / bl fn_80218BFC
//   0x140 RELMain   0x20  bl fn_39_160
//   0x160 fn_39_160 0x30  lbl_39_bss_40 = fn_39_190 ; fn_80218BFC(&lbl_39_bss_40)
//
// Everything from fn_39_190 (0x190, 0x5A8) up is left unclaimed, so dtk fills it from retail and
// the module's sha1 against `config/G2ME01/config.yml` still holds. The first of those is the
// module's own entity loader - behavioural class code, and it needs the CActor/CPatterned
// hierarchy this tree does not model.
//
// **`fn_39_0` inlines the conversion; it does not call an out-of-line constructor.** This is the
// same split `CDarkCommandoRel.cpp` records for its `fn_3_14`, and it is measured again here: the
// family's other heads (`CMysteryFlyerRel.cpp`'s `fn_45_10`, `CIngSpaceJumpGuardianRel.cpp`'s
// `fn_34_1C`) are 0x3C bytes and end in a call to the module's own `optional_object<CAABox>`
// converting constructor; Lumite has **no such symbol**. `fn_39_0` is 0x68 bytes and holds the
// flag store (`li r0,1` / `stb r0,0x18(r31)`) and the six-word box copy in its own body:
//
//   0x0000 stwu r1,-0x30 / mflr r0 / stw r0,0x34(r1) / stw r31,0x2c(r1) / mr r31,r3
//   0x0014 addi r3,r1,0x8 / bl GetBoundingBox__13CPhysicsActorCFv
//   0x001C li r0,1 / stb r0,0x18(r31) / six lwz+stw pairs from r1+0x8 into r31+0x0
//   0x0054 the epilogue
//
// So the spelling is the real `rstl::optional_object<CAABox>` return type, and MWCC reproduces
// those bytes from it with no hand-written copy: the converting constructor sets `m_valid` in its
// mem-init list and then placement-constructs the box, and `CAABox` carries
// `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` in `include/Kyoto/Math/CAABox.hpp`, so that construction
// is a word-wise copy rather than a call. The one-method `CPhysicsActor` stand-in is load-bearing
// for the frame shape: `CAABox GetBoundingBox() const` takes `this` in r4 - the register `self`
// already arrives in, so no move is needed - and returns through a pointer at r1+0x8, which is the
// 0x30 frame and the 0x34 saved-LR slot retail has. A hand-written flag store plus copy compiles to
// a **0x100-byte** function instead (MWCC materialises the box on the stack and copies it through
// f0..f4), which shifts the other sixteen functions and breaks the claim.
//
// **Do not include `MetroidPrime/CPhysicsActor.hpp`**: it reaches `Collision/CMaterialList.hpp`,
// whose file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28
// bytes of `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly
// that with every function at 100%. Only the mangled name has to agree, and one const member
// gives it.
//
// **The predicate run is not the family's alternation.** `fn_39_80`, `fn_39_88` and `fn_39_90` are
// three `li r3,0` in a row, then `fn_39_BC` is the block's only `li r3,1`, and `fn_39_C4`/`fn_39_CC`
// are `li r3,0` again. Read off `build/G2ME01/Lumite/asm/auto_00_00000000_text.s` (and confirmed
// byte for byte against `orig/G2ME01/files/RelProd/Lumite.rel`), not assumed from a sibling: the
// family pattern alternates, and the first three would read as true/false/true from it.
//
// **fn_39_D4 is a copy, not a constructed CVector3f.** Retail reuses f0 for all three loads
// (`lfs f0,0x54(r4) / stfs f0,0x0(r3) / lfs f0,0x58(r4) / stfs f0,0x4(r3) / ...`) - interleaved
// load/store pairs. The three-argument constructor would hoist the three loads into f0/f1/f2 and
// is the wrong shape here; `CMetareeSwarmRel.cpp` records that it is the right shape there, so it
// is a per-module measurement, not a family rule.
//
// **fn_39_F0 dispatches through a stand-in class with thirteen virtuals.** Retail is
// `lwz r12,0x0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`; loading the vtable by hand -
// `void* const* vt = *static_cast<void* const* const*>(self); vt[14]` - compiles to `lwz r3,0(r3)`
// where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the function (measured
// 2026-09-29, `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its own virtual-dispatch
// path, and it lays a class's virtuals out the way retail's vtable is laid out - two leading words
// (offset-to-top, then the RTTI pointer, both zero in this REL) and one word per virtual - so
// thirteen virtuals put the last one at 0x38. The slots are named by position because no header
// here models a CActor virtual; none of them is defined or called from this file, because the only
// object that carries this vtable is the module's own retail bytes.
//
// The two callees of the registration path are named by what they are, not invented:
//   - `fn_80218BFC` is the DOL's 0x80218BFC, `size:0x8` (`config/G2ME01/symbols.txt`), the
//     module's own import-table name for the two-instruction setter `stw r3, 0x80419428; blr`
//     (`config/G2ME01/symbols.txt` names the slot `gLoader_Lumite`). So it stores the *address* of
//     a loader slot, not a loader, which is why the store below hands it `&lbl_39_bss_40` and why
//     that slot is four bytes wide. `src/MetroidPrime/ScriptLoader/Lumite.cpp` holds the reader
//     (`(*gLoader_Lumite.value)(mgr, input, info)`) and records that the setter is deliberately
//     not claimed in the DOL, because REL modules import it by its retail name - so it stays in
//     dtk's auto unit, and no `symbols.txt` rename is needed. It is `extern "C"`: an alias would
//     be a different symbol and the call would resolve to nothing.
//   - `lbl_39_bss_40` is `.bss:0x40`, `size:0x4 data:4byte`
//     (`build/G2ME01/Lumite/asm/auto_05_00000000_bss.s`): the module's own copy of the loader
//     pointer. **Not `lbl_39_bss_0`** - this is the module whose `.bss` holds five objects
//     (`lbl_39_bss_0` 8, `_8` 0x18, `_20` 8, `_28` 0x18, `_40` 4). This unit's split claims .text
//     only, so dtk's `.bss` object has to define it, and a second definition under MWCC is what
//     produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and
//     a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and the module hash breaking on a few bytes. Only
// `tools/flip_test.sh` catches that, and
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CLumiteRel.cpp` before
// promising a promotion.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "rstl/optional_object.hpp"
#include "types.h"

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`; see the note at the top.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The stand-in vtable for fn_39_F0. Thirteen virtuals; the call is to the last one, at 0x38.
class CLumiteVTable {
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
  virtual void Slot12();
};

extern "C" {
extern "C" const float lbl_8041AAB8;
extern "C" const unsigned short kInvalidUniqueId;

// fn_39_190, the module's own entity loader, 0x190, 0x5A8: left retail, named here only so the
// registration below can store its address.
CEntity* fn_39_190(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218BFC(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_39_bss_40;
#else
FScriptLoader lbl_39_bss_40 = 0;
#endif

// .text 0x160, 0x30 bytes. lbl_39_bss_40 = fn_39_190 ; fn_80218BFC(&lbl_39_bss_40). Retail loads
// the loader out of `.text` and the slot address out of `.bss`, and stores the loader with `stwu`
// so the store writes the slot itself and leaves r3 holding its address for the setter call.
void fn_39_160() {
  lbl_39_bss_40 = fn_39_190;
  fn_80218BFC(&lbl_39_bss_40);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CDarkCommandoRel.cpp`
// and `CMysteryFlyerRel.cpp`: listing this file in `files.cmake` would make the port link
// `fn_39_190` and `fn_80218BFC`, which it cannot, and `tools/link_check.sh --strict` fails on a
// growing undefined count. The port keeps reading `Lumite.rel` off the disc through
// `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid translation
// unit.
#ifdef __MWERKS__
void RELMain() { fn_39_160(); }

void RELExit() { fn_80218BFC(nullptr); }
#else
void mp_relmain_lumite() { fn_39_160(); }

void mp_relexit_lumite() { fn_80218BFC(nullptr); }
#endif

// .text 0xF0, 0x2C bytes. a member call into vtable slot 0x38.
void fn_39_F0(CLumiteVTable* self) { self->Slot12(); }

// .text 0xD4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// copies a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. A copy, not a constructed
// CVector3f - see the note at the top.
void fn_39_D4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xCC, 0x08 bytes. a predicate that is always false.
bool fn_39_CC(void*) { return false; }

// .text 0xC4, 0x08 bytes. a predicate that is always false.
bool fn_39_C4(void*) { return false; }

// .text 0xBC, 0x08 bytes. the block's only always-true predicate - `38 60 00 01`, read off the
// disc image; the three `li r3,0` above make this read as `false` from the family's alternation.
bool fn_39_BC(void*) { return true; }

// .text 0xB4, 0x08 bytes. the address of the member at +0x754.
void* fn_39_B4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0xA8, 0x0C bytes. the flag at +0x34c - `extrwi r3,r0,1,28`, the fourth `bool : 1` of its
// byte, the same body `CDarkCommandoRel.cpp`'s fn_3_BC and `CIngSpaceJumpGuardianRel.cpp`'s
// fn_34_98 carry over the same offset.
bool fn_39_A8(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x98, 0x10 bytes. resets the unique id to the invalid value.
void fn_39_98(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x90, 0x08 bytes. a predicate that is always false.
bool fn_39_90(void*) { return false; }

// .text 0x88, 0x08 bytes. a predicate that is always false.
bool fn_39_88(void*) { return false; }

// .text 0x80, 0x08 bytes. a predicate that is always false.
bool fn_39_80(void*) { return false; }

// .text 0x78, 0x08 bytes. the byte at +0x44f.
unsigned char fn_39_78(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x68, 0x10 bytes. stores the default float at +0x448.
void fn_39_68(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}

// .text 0x0, 0x68 bytes. Returns `rstl::optional_object<CAABox>(self->GetBoundingBox())` through
// the hidden pointer in r3, with `self` in r4; see the note at the top.
rstl::optional_object< CAABox > fn_39_0(const CPhysicsActor* self) { return self->GetBoundingBox(); }
}
