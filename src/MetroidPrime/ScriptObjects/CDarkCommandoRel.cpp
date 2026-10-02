// CDarkCommandoRel.cpp - DarkCommando's (module 3) head, .text 0x0..0x19C: the eighteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp`, and the ranges come from
// `config/G2ME01/rels/DarkCommando/symbols.txt`:
//
//   0x000 fn_3_0    0x08  li r3,1
//   0x008 fn_3_8    0x0C  lbl_3_rodata_0, this module's own .rodata:0x0
//   0x014 fn_3_14   0x68  GetBoundingBox, then an **inlined** optional_object<CAABox>
//   0x07C fn_3_7C   0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x08C fn_3_8C   0x08  lbz r3, 0x44f(r3)
//   0x094 fn_3_94   0x08  li r3,0
//   0x09C fn_3_9C   0x08  li r3,0
//   0x0A4 fn_3_A4   0x08  li r3,0
//   0x0AC fn_3_AC   0x10  *self = kInvalidUniqueId
//   0x0BC fn_3_BC   0x0C  byte at +0x34c, bit 3
//   0x0C8 fn_3_C8   0x08  addi r3,r3,0x754
//   0x0D0 fn_3_D0   0x08  li r3,1
//   0x0D8 fn_3_D8   0x08  li r3,0
//   0x0E0 fn_3_E0   0x1C  three floats from self+0x54 -> *out
//   0x0FC fn_3_FC   0x2C  virtual dispatch, vtable slot 0x38
//   0x128 RELExit   0x24  li r3,0 / bl fn_80235E00
//   0x14C RELMain   0x20  bl fn_3_16C
//   0x16C fn_3_16C  0x30  lbl_3_bss_1C = fn_3_19C ; fn_80235E00(&lbl_3_bss_1C)
//
// Everything from fn_3_19C (0x19C, 0x33C) up is left unclaimed, so dtk fills it from retail and
// the module's sha1 against `config/G2ME01/config.yml` still holds. That neighbour is the
// module's own entity loader - behavioural class code, and it needs the CActor/CPatterned
// hierarchy this tree does not model.
//
// **fn_3_0 and fn_3_D0 are the only two `li r3,1` in the block, and everything else is
// `li r3,0`.** That is worth stating because it is the one function here a plausible guess gets
// wrong: the family pattern (`CMysteryFlyerRel.cpp`, `CDarkTrooperRel.cpp`,
// `CIngSpaceJumpGuardianRel.cpp`) alternates true and false, so `fn_3_A4` reads as a `true` in the
// run after the byte accessor - and it is not. It is `li r3,0`, read off
// `orig/G2ME01/files/RelProd/DarkCommando.rel` at `.text` 0xA4. An earlier attempt at this file
// wrote it as `true`, which cost exactly one byte of the module's sha1 and held the unit at
// 17/18 (`fn_3_A4` 99.50%) with every other function at 100.00%. The disc image is the authority
// here, not the family: `build/G2ME01/DarkCommando/asm/auto_00_00000000_text.s` prints `fn_3_A4`
// as `li r3, 0x0` too, and `cmp -l orig/... build/...` names that single byte.
//
// **`fn_3_14` inlines the conversion; it does not call an out-of-line constructor.** This is the
// one place DarkCommando parts company with the same wrapper in the neighbouring modules, and it
// is measured, not assumed. `CMysteryFlyerRel.cpp`'s `fn_45_10`, `CTryclopsRel.cpp`'s
// `fn_81_10` and `CIngSpaceJumpGuardianRel.cpp`'s `fn_34_1C` are each 0x3C bytes and end in
// `bl <module>_ctor` - the module's own `optional_object<CAABox>` converting constructor, which
// lives elsewhere in the module's `.text` and stays unclaimed. `fn_3_14` is **0x68 bytes with no
// such call**: the flag store and the six-word `CAABox` copy are in the body, which is
// `src/MetroidPrime/ScriptObjects/DigitalGuardianAccessors.cpp`'s `fn_14_10` instruction for
// instruction (and Lumite's `fn_39_0`, DarkSamus's `fn_10_F5EC` and nine others).
//
// So the spelling is the real `rstl::optional_object<CAABox>` return type, and MWCC reproduces
// the bytes from it with no hand-written copy: the converting constructor sets `m_valid` in its
// mem-init list and then placement-constructs the box, which is exactly `li r0,1;
// stb r0,0x18(r31)` followed by six `lwz`/`stw` pairs. `CAABox` carries
// `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` in `include/Kyoto/Math/CAABox.hpp`, so that construction
// is a word-wise copy rather than a call. `CAABox GetBoundingBox() const` on the stand-in is
// load-bearing for the frame shape: it takes `this` in r4, the same register `self` arrives in, so
// no move is needed, and it returns the box through a pointer at r1+0x8, which is the 0x30 frame
// and the 0x34 saved-LR slot retail has.
//
// **Nothing had to be written by hand to get the flag ahead of the copy** (measured 2026-09-29):
// a stand-in struct written as `mValid = true; mBounds = box;` scores the same 104/104 bytes as
// the header's return type, and so does the reverse order - the flag-first rendering is
// mwcceppc's scheduling, not the source's. The header's spelling is kept because it is the one
// that explains *why* retail inlined the conversion here.
//
// **The other fourteen accessors are the family, and the one that is not the family's is
// `fn_3_8`.** Every other module head carries a `lbl_8041B758` accessor (`fn_45_90`,
// `fn_12_44`) reading a DOL constant; this head has none, and puts its **own** `lbl_3_rodata_0`
// (`.rodata:0x0`, `size:0x4`, `.float 50`) at 0x08 instead. The spelling is the same either way -
// a `float` return of an `extern "C" const float` - and it is what
// `CIngSpaceJumpGuardianRel.cpp` already does for its own `lbl_34_rodata_0` and
// `CScriptRubiksPuzzle.cpp` for its own `lbl_4_rodata_0`. This unit's split claims `.text` only,
// so `lbl_3_rodata_0` stays defined in dtk's `.rodata` object and the reference is an ordinary
// cross-object relocation, exactly as the DOL globals beside it are.
//
// **fn_3_BC and fn_3_FC are one bit test and one vtable call, and the stand-in class carries
// thirteen virtuals.** Retail's call is `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr r12 / bctrl`;
// loading the vtable by hand - `void* const* vt = *(void* const* const*)self; vt[14]` - compiles
// to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the
// function (measured 2026-09-29, `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its
// own virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is laid
// out - two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and one
// word per virtual - so thirteen virtuals put the last one at 0x38. The slots are named by
// position because no header here models a CActor virtual; none of them is defined or called from
// this file, because the only object that carries this vtable is the module's own retail bytes.
//
// **fn_3_E0 is a copy, not a constructor call.** Retail reuses f0 for all three
// (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`) - interleaved
// load/store pairs. The three-argument CVector3f constructor, which `CMetareeSwarmRel.cpp`'s
// fn_43_3C needs because retail there loads all three before storing any, would hoist them into
// f0/f1/f2 and is the wrong shape here. Do not "improve" it into a `CVector3f` copy.
// `CMetareeSwarmRel.cpp` records that the built-in spelling reverses the loads and the score
// falls.
//
// The two callees of the registration path are named by what they are, not invented:
//   - `fn_80235E00` is the DOL's 0x80235E00, `size:0x8`, and it is the module's own import-table
//     name for the two-instruction setter `stw r3, 0x80419660; blr`
//     (`config/G2ME01/symbols.txt` names the slot `gLoader_DarkCommando`). So it stores the
//     *address* of a loader slot, not a loader, which is why the store below hands it
//     `&lbl_3_bss_1C` and why that slot is four bytes wide.
//     `src/MetroidPrime/ScriptLoader/DarkCommando.cpp` holds the reader
//     (`(*gLoader_DarkCommando.value)(mgr, input, info)`) and records that the setter is
//     deliberately not claimed in the DOL, because REL modules import it by its retail name - so
//     it stays in dtk's auto unit and this declaration is the same one
//     `CDarkTrooperRel.cpp` makes for `fn_80218DF4`. It is `extern "C"`: an alias would be a
//     different symbol and the call would resolve to nothing.
//   - `lbl_3_bss_1C` is `.bss:0x1C`, `size:0x4 data:4byte`
//     (build/G2ME01/DarkCommando/asm/auto_05_00000000_bss.s): the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and the module hash breaking on a few bytes. Only
// `tools/flip_test.sh` catches that, and
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp`
// before promising a promotion.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "rstl/optional_object.hpp"
#include "types.h"

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The stand-in vtable for fn_3_FC. Thirteen virtuals; the call is to the last one, at 0x38.
class CDarkCommandoVTable {
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
extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const unsigned short kInvalidUniqueId;
// This module's own .rodata:0x0, `.float 50`. Defined by dtk's `.rodata` object, which this
// unit's .text-only split does not claim.
extern "C" const float lbl_3_rodata_0;

// fn_3_19C, the module's own entity loader, 0x19C, 0x33C: left retail, named here only so the
// registration below can store its address.
CEntity* fn_3_19C(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80235E00(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_3_bss_1C;
#else
FScriptLoader lbl_3_bss_1C = 0;
#endif

// .text 0x16C, 0x30 bytes. lbl_3_bss_1C = fn_3_19C ; fn_80235E00(&lbl_3_bss_1C). Retail loads the
// loader out of `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so
// the store writes the slot itself and leaves r3 holding its address for the setter call.
void fn_3_16C() {
  lbl_3_bss_1C = fn_3_19C;
  fn_80235E00(&lbl_3_bss_1C);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp` and `CDarkTrooperRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_3_19C` and `fn_80235E00`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. The port keeps reading
// `DarkCommando.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only
// so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_3_16C(); }

void RELExit() { fn_80235E00(nullptr); }
#else
void mp_relmain_darkcommando() { fn_3_16C(); }

void mp_relexit_darkcommando() { fn_80235E00(nullptr); }
#endif

// .text 0xFC, 0x2C bytes. a member call into vtable slot 0x38.
void fn_3_FC(CDarkCommandoVTable* self) { self->Slot12(); }

// .text 0xE0, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// copies a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. A copy, not a constructed
// CVector3f - see the note at the top.
void fn_3_E0(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xD8, 0x08 bytes. a predicate that is always false.
bool fn_3_D8(void*) { return false; }

// .text 0xD0, 0x08 bytes. a predicate that is always true.
bool fn_3_D0(void*) { return true; }

// .text 0xC8, 0x08 bytes. the address of the member at +0x754.
void* fn_3_C8(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0xBC, 0x0C bytes. the flag at +0x34c - `extrwi r3,r0,1,28`, the fourth `bool : 1` of its
// byte. Same source as `CDarkTrooperRel.cpp`'s `fn_12_38` over the same offset, and it transfers
// verbatim: the two objects hold the same three instructions,
// `88 03 03 4C / 54 03 EF FE / 4E 80 00 20`.
bool fn_3_BC(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0xAC, 0x10 bytes. resets the unique id to the invalid value.
void fn_3_AC(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0xA4, 0x08 bytes. a predicate that is always **false** - `38 60 00 00`, read off the disc
// image. The family's alternation makes this read as `true`; see the note at the top.
bool fn_3_A4(void*) { return false; }

// .text 0x9C, 0x08 bytes. a predicate that is always false.
bool fn_3_9C(void*) { return false; }

// .text 0x94, 0x08 bytes. a predicate that is always false.
bool fn_3_94(void*) { return false; }

// .text 0x8C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_3_8C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x7C, 0x10 bytes. stores the default float at +0x448.
void fn_3_7C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x14, 0x68 bytes. Returns `rstl::optional_object<CAABox>(self->GetBoundingBox())` through
// the hidden pointer in r3, with `self` in r4; see the note at the top.
rstl::optional_object< CAABox > fn_3_14(const CPhysicsActor* self) { return self->GetBoundingBox(); }

// .text 0x8, 0x0C bytes. this module's own `lbl_3_rodata_0`, `.float 50`.
float fn_3_8(void*) { return lbl_3_rodata_0; }

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_3_0(void*) { return true; }
}
