// CMetroidRel.cpp - Metroid's (module 40) head, .text 0x0..0x17C: the eighteen functions above
// the module's class code. Same arrangement as `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`
// and `MetroidPrime/ScriptObjects/CMediumIngRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Metroid/symbols.txt`:
//
//   0x000 fn_40_0   0x08  addi r3,r3,0x8c8
//   0x008 fn_40_8   0x08  li r3,1
//   0x010 fn_40_10  0x3C  GetBoundingBox into a local, then fn_40_85F4(out, &box)
//   0x04C fn_40_4C  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x05C fn_40_5C  0x08  lbz r3, 0x44f(r3)
//   0x064 fn_40_64  0x08  li r3,0
//   0x06C fn_40_6C  0x08  li r3,0
//   0x074 fn_40_74  0x08  li r3,0
//   0x07C fn_40_7C  0x10  *self = kInvalidUniqueId
//   0x08C fn_40_8C  0x0C  byte at +0x34c, bit 3
//   0x098 fn_40_98  0x0C  lbl_8041B758
//   0x0A4 fn_40_A4  0x08  addi r3,r3,0x754
//   0x0AC fn_40_AC  0x08  li r3,1
//   0x0B4 fn_40_B4  0x08  li r3,0
//   0x0BC fn_40_BC  0x2C  virtual dispatch, vtable slot 0x38
//   0x0E8 RELExit   0x24  li r3,0 / bl fn_80218B68
//   0x10C RELMain   0x20  bl fn_40_12C
//   0x12C fn_40_12C 0x50  lbl_40_bss_10 = {fn_40_17C, lbl_40_data_358}
//
// **The accessor block is the family close to MysteryFlyer's, but not identical, and that is
// measured rather than read off the `fn_<id>_<off>` names.** Diffing dtk's
// `auto_00_00000000_text.s` over the 0x17C this claims against `CMysteryFlyerRel.cpp`'s 0x170: both
// put the `GetBoundingBox` wrapper third, both have the `skDamageHitTime__10CPatterned` store at +0x448, the
// `0x44f` byte, the `kInvalidUniqueId` store, the `+0x34c` bit test, the `lbl_8041B758` accessor,
// the `+0x754` address, a `li r3,1` and the closing vtable-0x38 dispatch. The differences are
// four, and each was read off the disassembly rather than assumed from a sibling:
//   - the two leading eight-byte accessors are **swapped**: this module opens `addi r3,r3,0x8c8`
//     and then `li r3,1`, where MysteryFlyer opens `li r3,1` and then `addi r3,r3,0x818`;
//   - this module has **three** `li r3,0` predicates in the run where MysteryFlyer has two;
//   - it has **no** three-float copy - `fn_40_B4` is an eight-byte `li r3,0` where MysteryFlyer
//     has a 0x1C-byte accessor there - so it covers one member fewer than the rest of the family;
//   - and the registration is 0x50 bytes rather than 0x30, because this record is 0x10 bytes (see
//     the note on the struct below) where MysteryFlyer's is four.
// So nothing had to be re-derived: every body below is the one `CMysteryFlyerRel.cpp` and
// `CIngSpaceJumpGuardianRel.cpp` already reproduce at 100%, apart from `fn_40_12C`, which is
// `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` with one member-function pointer instead of two. The
// point is worth keeping, because `CMediumIngRel.cpp` records the opposite result for module 41:
// **a module head is not a sibling's head until the bytes say so.**
//
// One accessor reads oddly in dtk's rendering, and it is dtk's rendering rather than the source's:
// `fn_40_8C` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
// `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
// opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
// `fn_40_0` and `fn_40_A4` return the *address* of a member, so they are `void*` and the offsets
// are literal.
//
// Everything from fn_40_17C (0x17C, 0x79C), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CMetroidAlpha/CActor/CPatterned/CAi hierarchy this tree
// does not model. The 149 functions above it are the module's methods and stay retail. So dtk
// fills them and the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// **No dead-strip hazard, and that is measured**: `build/G2ME01/Metroid/ldscript.lcf` puts all
// fifteen of `fn_40_0` .. `fn_40_BC` in its FORCEACTIVE list, and `.data:0x364` - CMetroid's own
// vtable, 0x154 bytes = 85 words, two of them leading (offset-to-top and the RTTI pointer, both
// zero in this REL) - stores every one of them, so nothing here needs a `force_active:` entry in
// `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the
// module's own entry points and `fn_40_12C` is called from `RELMain`, so that trio survives for
// the reason it does everywhere in this family.
//
// `fn_40_10` is not an `optional_object` problem, though it returns one, and it is not a
// template-instantiation problem either: retail calls the converting constructor *out of line* at
// 0x85F4 (`li r0,1; lwz r5,0(r4); stb r0,0x18(r3)` then six word copies out of `r4+0x00..r4+0x14`,
// i.e. six words of `CAABox` and a validity byte at +0x18), and that function stays unclaimed. So
// `fn_40_10` is one call, `fn_40_85F4(out, self->GetBoundingBox())`, with the constructor declared
// by its dtk name exactly as `CMysteryFlyerRel.cpp` declares `fn_45_2BBC`. `self` needs no move
// because `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3); the frame is
// `-0x30` for the 0x18-byte temporary, and the constructor parameter must be a `const CAABox&` or
// the frame grows to 0x40.
//
// `fn_40_BC` is a vtable entry, not a free function: `.data:0x364` stores it at offset 0x3C, which
// calls slot 0x38, named `HealthInfo__3CAiFv` in the same table (and the second table,
// `.data:0x914` - CBabyMetroid's, also 85 words - stores it at the same 0x3C above
// `HealthInfo__3CAiFv`). So the call below is a member call, and that spelling is measured rather
// than guessed: loading the vtable by hand - `void* const* vt = *(void* const* const*)self;` and
// calling `vt[14]` - compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one
// register is 99.09% on the function (measured in `CIngPuddleRel.cpp`, 2026-09-29).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out - two leading words (offset-to-top, then the RTTI
// pointer, both zero in this REL) and then one word per virtual - so thirteen virtuals put the
// thirteenth at 0x38. The slots are named by position because no header here models a CPatterned
// virtual, and none of them is defined or called from this file, because the only object that
// carries this vtable is the module's own retail bytes. Slot 12's return type is float because the
// table names it `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218B68` is the DOL's 0x80218B68, two instructions,
//     `stw r3, gLoader_MetroidAlpha@sda21(r0); blr`
//     (`build/G2ME01/asm/auto_03_80218B68_text.s`), immediately after
//     `LoadMetroidAlpha__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x80218B3C, which is
//     0x2C bytes and so ends exactly at 0x80218B68. So it stores the *address* of a loader record,
//     not a loader. `src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp` - a `Matching` unit, and the
//     file that already records why the setter is deliberately not claimed in the DOL - reads the
//     slot as `(*gLoader_MetroidAlpha.value)(mgr, input, info)`, so the first word of the record is
//     the loader, and the record is 0x10 bytes rather than the four bytes most of this family uses.
//     That file's own reader is only half the story: the second reader is the DOL's
//     `OnDockTouch__13CMetroidAlphaFR13CStateManager` at 0x80218B10, which does
//     `lwz r5, gLoader_MetroidAlpha@sda21(r0); addi r12, r5, 0x4; bl __ptmf_scall`
//     (`build/G2ME01/asm/auto_03_80218B08_text.s`) - so **words 4..15 of the record are a
//     CodeWarrior pointer-to-member-function**, which is what makes it 12 bytes. The import name is
//     the plain `fn_80218B68` that `config/G2ME01/symbols.txt:9485` already gives it - not the long
//     MWCC-mangled `SetLoader_...` form `CIngSnatchingSwarmRel.cpp` had to spell out, and it is
//     checked in the module's own `build/G2ME01/Metroid/Metroid.preplf` import table - so no
//     `symbols.txt` rename is needed and the DOL is untouched. It is `extern "C"`: an alias would
//     be a different symbol and the call would resolve to nothing.
//   - `lbl_40_bss_10` is `.bss:0x10`, `size:0x10`: the module's own copy of the record. It is
//     **`lbl_40_bss_10` and not `lbl_40_bss_0`** - this module's `.bss:0x0` is a different 0x10-byte
//     object, read and written by `fn_40_17C`'s code above the head - which is why the name below
//     is the one it is. This unit's split claims `.text` only, so dtk's `.bss` object has to define
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

// **The record's second member is copied out of `.data`, not built here**, and that is what the
// three words at `.data:0x358` are: `0 / 0xFFFFFFFF / fn_40_FB4`, the same twelve-byte
// non-virtual pointer-to-member-function object as `CSnakeWeedSwarmRel.cpp`'s `lbl_71_data_18`,
// vtable offset -1 meaning "call it directly". `fn_40_FB4` (0xFB4, 0x8C) is the module's own
// `OnDockTouch` body and stays unclaimed. So the registration below assigns the extern object
// rather than naming a function, which is what puts the six `lwz`/`stw` pairs around the `stwu`
// exactly as they are. The type is spelled here rather than taken from
// `MetroidPrime/ScriptLoaderRel.hpp` for the reason `CSnakeWeedSwarmRel.cpp` gives: that header
// models `SPlayerActor_FuncPtrs`, which has this shape but is named for another module, and
// correcting a port-side model is a different item.
struct SMetroidAlpha_FuncPtrs {
  FScriptLoader loader;
  // Called through `__ptmf_scall` with `this` in r3 and the caller's `CStateManager&` in r4; the
  // signature below is the smallest that fits the layout, and it does not affect the bytes
  // because the assignment copies twelve bytes rather than building one.
  void (CEntity::*onDockTouch)(CStateManager&);
};

// The module's own copy of the member-function pointer, in `.data` and not claimed by this unit,
// so it is extern here: dtk's data object defines it.
extern void (CEntity::*lbl_40_data_358)(CStateManager&);

// The two DOL floats the accessor block stores and returns. Both are the globals every module of
// this family shares, so one body serves each of them.
extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that with
// every function at 100% (`CMysteryFlyerRel.cpp` measured it). Only the mangled name has to
// agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_40_BC dispatches to; see the note above on the layout.
class CMetroidDispatch {
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
CEntity* fn_40_17C(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80218B68(SMetroidAlpha_FuncPtrs* record);
// .text 0x85F4, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_40_85F4(void* out, const CAABox& box);

#ifdef __MWERKS__
extern SMetroidAlpha_FuncPtrs lbl_40_bss_10;
#else
SMetroidAlpha_FuncPtrs lbl_40_bss_10 = {0, 0};
#endif

// .text 0x12C, 0x50 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the record address out of `.bss`, stores the loader with `stwu` so the store writes
// the slot itself and leaves r3 holding its address for the setter call, and only then copies the
// three words of the `.data` member-function pointer back into it - so the member is assigned
// before the call, not built in place.
void fn_40_12C() {
  lbl_40_bss_10.loader = fn_40_17C;
  lbl_40_bss_10.onDockTouch = lbl_40_data_358;
  fn_80218B68(&lbl_40_bss_10);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CMediumIngRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_40_17C` and `fn_80218B68`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `Metroid.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so the
// file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_40_12C(); }

void RELExit() { fn_80218B68(nullptr); }
#else
void mp_relmain_metroid() { fn_40_12C(); }

void mp_relexit_metroid() { fn_80218B68(nullptr); }
#endif

// .text 0xBC, 0x2C bytes. Vtable entry 0x3C of CMetroid and of CBabyMetroid; the call it makes
// targets vtable slot 0x38. The class has no header here, so the object is reached as a
// `CMetroidDispatch*` - see the CIngPuddle section of `docs/research/raw_offsets.md` for why the
// hand-loaded vtable does not match.
void fn_40_BC(CMetroidDispatch* self) { self->Slot12(); }

// .text 0xB4, 0x08 bytes. a predicate that is always false.
bool fn_40_B4(void*) { return false; }

// .text 0xAC, 0x08 bytes. a predicate that is always true.
bool fn_40_AC(void*) { return true; }

// .text 0xA4, 0x08 bytes. the address of the member at +0x754.
void* fn_40_A4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x98, 0x0C bytes. a constant float out of the DOL.
float fn_40_98(void*) { return lbl_8041B758; }

// .text 0x8C, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_40_8C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x7C, 0x10 bytes. resets the unique id to the invalid value.
void fn_40_7C(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x74, 0x08 bytes. a predicate that is always false.
bool fn_40_74(void*) { return false; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_40_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_40_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_40_5C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4C, 0x10 bytes. stores a DOL float at +0x448.
void fn_40_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_40_10(void* out, const CPhysicsActor* self) {
  fn_40_85F4(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_40_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the address of the member at +0x8c8.
void* fn_40_0(void* self) { return static_cast< char* >(self) + 0x8C8; }
}
