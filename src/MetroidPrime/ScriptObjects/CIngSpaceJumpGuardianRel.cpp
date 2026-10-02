// CIngSpaceJumpGuardianRel.cpp - IngSpaceJumpGuardian's (module 34) head, .text 0x0..0x170:
// the eighteen functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CTryclopsRel.cpp`, and the ranges come from
// `config/G2ME01/rels/IngSpaceJumpGuardian/symbols.txt`:
//
//   0x000 fn_34_0   0x08  addi r3,r3,0x8d0
//   0x008 fn_34_8   0x08  li r3,1
//   0x010 fn_34_10  0x0C  lbl_34_rodata_0, this module's own .rodata:0x0
//   0x01C fn_34_1C  0x3C  GetBoundingBox into a local, then fn_34_6814(out, &box)
//   0x058 fn_34_58  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x068 fn_34_68  0x08  lbz r3, 0x44f(r3)
//   0x070 fn_34_70  0x08  li r3,0
//   0x078 fn_34_78  0x08  li r3,0
//   0x080 fn_34_80  0x08  li r3,0
//   0x088 fn_34_88  0x10  *self = kInvalidUniqueId
//   0x098 fn_34_98  0x0C  byte at +0x34c, bit 3
//   0x0A4 fn_34_A4  0x08  addi r3,r3,0x754
//   0x0AC fn_34_AC  0x08  li r3,1
//   0x0B4 fn_34_B4  0x1C  three floats from self+0x54 -> *out
//   0x0D0 fn_34_D0  0x2C  virtual dispatch, vtable slot 0x38
//   0x0FC RELExit   0x24  li r3,0 / bl fn_8021DC2C
//   0x120 RELMain   0x20  bl fn_34_140
//   0x140 fn_34_140 0x30  lbl_34_bss_0 = fn_34_170 ; fn_8021DC2C(&lbl_34_bss_0)
//
// **The accessor block is Tryclops' with one function moved, and that is measured rather than
// assumed.** `config/G2ME01/rels/IngSpaceJumpGuardian/ldscript.lcf` puts all fifteen of
// `fn_34_0` .. `fn_34_D0` in its FORCEACTIVE list, and `.data:0x3E4` - the module's own vtable,
// 0x148 bytes, two leading words and one per virtual - stores fifteen of them, so no dead-strip
// hazard and a missing `force_active:` entry is not the trap `CGeomBlobV2` measured. Which of
// them is where is *not* the family's usual order, and the diff is the reason to read the bytes:
// this module opens with `addi r3,r3,0x8d0` (Tryclops opens `addi r3,r3,0x7c4`) and `li r3,1`,
// then puts a **module-local `.rodata` constant** at 0x10 where Tryclops puts the
// `GetBoundingBox` wrapper, and the `lbl_8041B758` accessor Tryclops has at 0x98 is not here at
// all. So no spelling had to be discovered for anything except `fn_34_10`, and every body below
// is the one `CMysteryFlyerRel.cpp` / `CTryclopsRel.cpp` already reproduces at 100%. The two
// that read oddly in dtk's rendering carry over with their caveats, and they are dtk's
// rendering rather than the source's:
//   - `fn_34_98` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
//     `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word
//     is opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit -
//     bit 3.
//   - `fn_34_B4` loads and stores *interleaved* (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 /
//     stfs f0,4 / lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores. Do not
//     "improve" it into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in
//     spelling reverses the loads and the score falls.
//
// `fn_34_10` returns the module's own `lbl_34_rodata_0` (`.rodata:0x0`, `size:0x4`, `.float 60`),
// not a DOL constant like the `lbl_8041B758` accessor this family usually carries at that slot.
// The spelling is the same one either way - a `float` return of an `extern "C" const float` -
// and it is what `CScriptRubiksPuzzle.cpp` already does for its own module's `lbl_4_rodata_0`.
// This unit's split claims `.text` only, so `lbl_34_rodata_0` stays defined in dtk's `.rodata`
// object and the reference is an ordinary cross-object relocation, exactly as the DOL globals
// beside it are.
//
// **`fn_34_1C` is not an `optional_object` problem**, though it returns one, and it is not a
// template-instantiation problem either: retail calls the converting constructor *out of line*
// at 0x6814, and that function stays unclaimed. So `fn_34_1C` is one call,
// `fn_34_6814(out, self->GetBoundingBox())`, with the constructor declared by its dtk name
// exactly as `CMysteryFlyerRel.cpp` declares `fn_45_2BBC`. `fn_34_6814` is this module's own
// `optional_object<CAABox>(const CAABox&)`: six word copies out of `r4+0x00..r4+0x14` and a
// `stb 1, 0x18(r3)`, i.e. six words of `CAABox` and a validity byte at +0x18.
//
// `fn_34_D0` is a vtable entry, not a free function: `.data:0x3E4` stores it at offset 0x3C,
// which calls slot 0x38, named `HealthInfo__3CAiFv` in the same table. So the call below is a
// member call, and that spelling is measured rather than guessed. Loading the vtable by hand -
// `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` - compiles to
// `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the
// function (measured in `CIngPuddleRel.cpp`, 2026-09-29). `mwcceppc` only reaches for r12 on its
// own virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is laid
// out: two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and then
// one word per virtual. Thirteen virtuals therefore put the thirteenth at 0x38. The slots are
// named by position because no header here models a CPatterned virtual, and none of them is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes. Slot 12's return type is float because the table names it
// `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes.
//
// The two callees are named by what they are, not invented:
//   - `fn_8021DC2C` is the DOL's 0x8021DC2C, two instructions,
//     `stw r3, gLoader_IngSpaceJumpGuardian@sda21(r0); blr`
//     (`build/G2ME01/asm/auto_03_8021DC2C_text.s`, and `config/G2ME01/symbols.txt:9591` already
//     gives it that name). So it stores the *address* of a loader slot, not a loader.
//     `LoadIngSpaceJumpGuardian__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x8021DC00
//     reads it as `lwz r6, gLoader_IngSpaceJumpGuardian; lwz r12, 0(r6); mtctr r12; bctrl`,
//     which is why the store below hands it `&lbl_34_bss_0` and why that slot is four bytes
//     wide. It is a plain DOL symbol, so like `CTryclopsRel.cpp`'s `fn_80218D58` it needs no
//     `symbols.txt` rename and no DOL change; it is `extern "C"`, because an alias would be a
//     different symbol and the call would resolve to nothing.
//   - `lbl_34_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims `.text` only, so dtk's `.bss` object has to define it,
//     and a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Everything from fn_34_170 (0x170, 0x330), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned/CAi hierarchy this tree does not
// model. The 125 functions above it are CIngSpaceJumpGuardian's members and stay retail.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
// `kInvalidUniqueId` comes from MetroidPrime/TGameTypes.hpp, which declares it as the
// `TUniqueId` retail has, so `fn_34_88` is spelled `*id = kInvalidUniqueId` on a `TUniqueId*`
// rather than the `unsigned short` alias CMysteryFlyerRel.cpp's header comment mentions.
// .rodata:0x0 of this module: `.float 60`. See the note above; the split claims .text only.
extern "C" const float lbl_34_rodata_0;

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

// The vtable slot fn_34_D0 dispatches to; see the note above on the layout.
class CIngSpaceJumpGuardianDispatch {
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
CEntity* fn_34_170(CStateManager&, CInputStream&, CEntityInfo&);
void fn_8021DC2C(FScriptLoader* loader);
// .text 0x6814, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_34_6814(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_34_bss_0;
#else
FScriptLoader lbl_34_bss_0 = 0;
#endif

// .text 0x140, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_34_140() {
  lbl_34_bss_0 = fn_34_170;
  fn_8021DC2C(&lbl_34_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp` and `CTryclopsRel.cpp`: listing this file in `files.cmake` would make the
// port link `fn_34_170` and `fn_8021DC2C`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. So the port keeps reading `IngSpaceJumpGuardian.rel` off
// the disc through `platform/rel.cpp`, and the `#else` branch exists only so the file is still a
// valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_34_140(); }

void RELExit() { fn_8021DC2C(nullptr); }
#else
void mp_relmain_ingspacejumpguardian() { fn_34_140(); }

void mp_relexit_ingspacejumpguardian() { fn_8021DC2C(nullptr); }
#endif

// .text 0xD0, 0x2C bytes. Vtable entry 0x3C of CIngSpaceJumpGuardian; the call it makes targets
// vtable slot 0x38, which `.data:0x3E4` names `HealthInfo__3CAiFv`. The class has no header
// here, so the object is reached as a `CIngSpaceJumpGuardianDispatch*` - see the CIngPuddle
// section of `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_34_D0(CIngSpaceJumpGuardianDispatch* self) { self->Slot12(); }

// .text 0xB4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_34_B4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xAC, 0x08 bytes. a predicate that is always true.
bool fn_34_AC(void*) { return true; }

// .text 0xA4, 0x08 bytes. the address of the member at +0x754.
void* fn_34_A4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x98, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_34_98(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x88, 0x10 bytes. resets the unique id to the invalid value.
void fn_34_88(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x80, 0x08 bytes. a predicate that is always false.
bool fn_34_80(void*) { return false; }

// .text 0x78, 0x08 bytes. a predicate that is always false.
bool fn_34_78(void*) { return false; }

// .text 0x70, 0x08 bytes. a predicate that is always false.
bool fn_34_70(void*) { return false; }

// .text 0x68, 0x08 bytes. the byte at +0x44f.
unsigned char fn_34_68(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x58, 0x10 bytes. stores a DOL float at +0x448.
void fn_34_58(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x1C, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_34_1C(void* out, const CPhysicsActor* self) {
  fn_34_6814(out, self->GetBoundingBox());
}

// .text 0x10, 0x0C bytes. a constant float out of this module's own `.rodata`.
float fn_34_10(void*) { return lbl_34_rodata_0; }

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_34_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the address of the member at +0x8D0.
void* fn_34_0(void* self) { return static_cast< char* >(self) + 0x8D0; }
}
