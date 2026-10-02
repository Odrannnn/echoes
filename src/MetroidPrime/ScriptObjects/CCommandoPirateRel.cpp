// CCommandoPirateRel.cpp - CommandoPirate's (module 9) head, .text 0x0..0x168: the seventeen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/CommandoPirate/symbols.txt`:
//
//   0x000 fn_9_0   0x08  addi r3,r3,0x928
//   0x008 fn_9_8   0x08  li r3,1
//   0x010 fn_9_10  0x0C  lbl_9_rodata_400, this module's own .rodata:0x400
//   0x01C fn_9_1C  0x3C  GetBoundingBox into a local, then fn_9_F9BC(out, &box)
//   0x058 fn_9_58  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x068 fn_9_68  0x08  lbz r3, 0x44f(r3)
//   0x070 fn_9_70  0x08  li r3,0
//   0x078 fn_9_78  0x08  li r3,0
//   0x080 fn_9_80  0x10  *self = kInvalidUniqueId
//   0x090 fn_9_90  0x0C  byte at +0x34c, bit 3
//   0x09C fn_9_9C  0x08  addi r3,r3,0x754
//   0x0A4 fn_9_A4  0x08  li r3,1
//   0x0AC fn_9_AC  0x1C  three floats from self+0x54 -> *out
//   0x0C8 fn_9_C8  0x2C  virtual dispatch, vtable slot 0x38
//   0x0F4 RELExit  0x24  li r3,0 / bl fn_802188B0
//   0x118 RELMain  0x20  bl fn_9_138
//   0x138 fn_9_138 0x30  lbl_9_bss_28 = fn_9_168 ; fn_802188B0(&lbl_9_bss_28)
//
// **Which accessor sits in which slot is read off the bytes, not inherited from the family**,
// because this module's block is `CIngSpaceJumpGuardianRel.cpp`'s with two differences that only
// the disassembly settles:
//
//   - it opens with **two** leading accessors, `addi r3,r3,0x928` then `li r3,1`, where module 34
//     opens with `addi r3,r3,0x8d0` and whose second slot is the same `li r3,1`;
//   - it puts a **module-local `.rodata` constant** at 0x10 where module 34 also puts one (but at
//     `.rodata:0x0`; this module's is `.rodata:0x400`), and the
//     `lbl_8041B758`-style accessor some modules carry at that slot is not here at all.
//
// `fn_9_90` is the one function whose *identity* differs from module 34's block: there the
// `lbl_8041B758` accessor is missing and the +0x34c bit test moved up into 0x90. Its body is the
// same `& 8` test, so the spelling is unchanged.
//
// Two of these carry dtk's rendering rather than the source's:
//   - `fn_9_90` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
//     `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
//     opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
//   - `fn_9_AC` loads and stores *interleaved* (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 /
//     stfs f0,4 / lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores. Do not
//     "improve" it into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in
//     spelling reverses the loads and the score falls.
//
// `fn_9_10` returns this module's own `lbl_9_rodata_400` (`.rodata:0x400`, `size:0x4`,
// `.float 50`). The spelling is the same one `CScriptRubiksPuzzle.cpp` already uses for its own
// `lbl_4_rodata_0` and `CIngSpaceJumpGuardianRel.cpp` for `lbl_34_rodata_0`: a `float` return of an
// `extern "C" const float`. This unit's split claims `.text` only, so dtk's `.rodata` object keeps
// defining `lbl_9_rodata_400` and the reference is an ordinary cross-object relocation, exactly
// as the DOL globals beside it are.
//
// **`fn_9_1C` is not an `optional_object` problem**, though it returns one, and it is not a
// template-instantiation problem either: retail calls the converting constructor *out of line*
// at 0xF9BC, and that function stays unclaimed. So `fn_9_1C` is one call,
// `fn_9_F9BC(out, self->GetBoundingBox())`, with the constructor declared by its dtk name exactly
// as `CIngSpaceJumpGuardianRel.cpp` declares `fn_34_6814`. `fn_9_F9BC` is this module's own
// `optional_object<CAABox>(const CAABox&)`: six word copies out of `r4+0x00..r4+0x14` and a
// `stb 1, 0x18(r3)`, i.e. six words of `CAABox` and a validity byte at +0x18. `const CAABox&` is
// load-bearing - the constructor reads the source through r4 - so it is not a by-value spelling.
//
// `fn_9_C8` is a vtable entry, not a free function: `.data:0x828` (0x148 bytes, two leading words
// and one per virtual) stores it at offset 0x3C, which calls slot 0x38, named
// `HealthInfo__3CAiFv` in the same table. So the call below is a member call, and that spelling
// is measured rather than guessed: loading the vtable by hand -
// `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` - compiles to
// `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the
// function (measured in `CIngPuddleRel.cpp`, 2026-09-29). `mwcceppc` only reaches for r12 on its
// own virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is laid
// out: two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and then
// one word per virtual. Thirteen virtuals therefore put the thirteenth - the `CAi::HealthInfo`
// one - at 0x38. The slots are named by position because no header here models a CPatterned
// virtual, and none of them is defined or called from this file, because the only object that
// carries this vtable is the module's own retail bytes.
//
// The two callees are named by what they are, not invented:
//   - `fn_802188B0` is the DOL's 0x802188B0, two instructions,
//     `stw r3, gLoader_CommandPirate@sda21(r0); blr` (`build/G2ME01/asm/auto_03_802188B0_text.s`,
//     and `config/G2ME01/symbols.txt:9463` already gives it that name). So it stores the *address*
//     of a loader slot, not a loader. `LoadCommandPirate` at 0x80218884 reads it as
//     `lwz r6, gLoader_CommandPirate; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the store
//     below hands it `&lbl_9_bss_28`. It is deliberately unclaimed in the DOL -
//     `MetroidPrime/ScriptLoader/CommandPirate.cpp` records that REL modules import it by its
//     retail name, so it cannot be renamed - hence no `symbols.txt` rename and no DOL change. It is
//     `extern "C"`, because an alias would be a different symbol and the call would resolve to
//     nothing.
//   - `lbl_9_bss_28` is `.bss:0x28`, `size:0x8`, and **eight bytes wide**, where module 34's
//     `lbl_34_bss_0` is four. It mirrors the DOL's `gLoader_CommandPirate`, which
//     `MetroidPrime/ScriptLoader/CommandPirate.cpp` already spells as an `FScriptLoader` plus one
//     padding word, so it is spelled the same way here rather than as a bare function pointer,
//     which would compile and describe a four-byte retail object as four bytes. There is no retail
//     type for it in `include/MetroidPrime/ScriptLoaderRel.hpp` - that header names the loaders
//     the DOL setter takes as a *pointer*, and it takes this one as a pointer too - so it is
//     spelled out locally rather than added to a header that does not describe it. Only the first
//     word is ever written. This unit's split claims `.text` only, so dtk's `.bss` object has to
//     define `lbl_9_bss_28`, and a second definition under MWCC is what produced mwldeppc's
//     internal linker error on ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Everything from fn_9_168 (0x168, 0x76C), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned/CAi hierarchy this tree does not model.
// That range holds 231 functions - fn_9_168 and CCommandoPirate's other 230 members - and they
// stay retail.
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
// `TUniqueId` retail has, so `fn_9_80` is spelled `*id = kInvalidUniqueId` on a `TUniqueId*`
// rather than the `unsigned short` alias CMysteryFlyerRel.cpp's header comment mentions.
// .rodata:0x400 of this module: `.float 50`. See the note above; the split claims .text only.
extern "C" const float lbl_9_rodata_400;

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

// The vtable slot fn_9_C8 dispatches to; see the note above on the layout.
class CCommandoPirateDispatch {
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

// `lbl_9_bss_28` is eight bytes and only its first word is written; see the note above.
struct SCommandPirateLoaderSlot {
  FScriptLoader value;
  unsigned int padding;
};

extern "C" {
CEntity* fn_9_168(CStateManager&, CInputStream&, CEntityInfo&);
// The DOL's 0x802188B0, imported by retail name; see the note above.
void fn_802188B0(SCommandPirateLoaderSlot* loader);
// .text 0xF9BC, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_9_F9BC(void* out, const CAABox& box);

#ifdef __MWERKS__
extern SCommandPirateLoaderSlot lbl_9_bss_28;
#else
SCommandPirateLoaderSlot lbl_9_bss_28 = {0, 0};
#endif

// .text 0x138, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_9_138() {
  lbl_9_bss_28.value = fn_9_168;
  fn_802188B0(&lbl_9_bss_28);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CIngSpaceJumpGuardianRel.cpp` and `CMysteryFlyerRel.cpp`: listing this file in `files.cmake`
// would make the port link `fn_9_168` and `fn_802188B0`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `CommandoPirate.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only
// so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_9_138(); }

void RELExit() { fn_802188B0(nullptr); }
#else
void mp_relmain_commandpirate() { fn_9_138(); }

void mp_relexit_commandpirate() { fn_802188B0(nullptr); }
#endif

// .text 0xC8, 0x2C bytes. Vtable entry 0x3C of CCommandoPirate; the call it makes targets vtable
// slot 0x38, which `.data:0x828` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CCommandoPirateDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_9_C8(CCommandoPirateDispatch* self) { self->Slot12(); }

// .text 0xAC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_9_AC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_9_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_9_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_9_90(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x80, 0x10 bytes. resets the unique id to the invalid value.
void fn_9_80(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x78, 0x08 bytes. a predicate that is always false.
bool fn_9_78(void*) { return false; }

// .text 0x70, 0x08 bytes. a predicate that is always false.
bool fn_9_70(void*) { return false; }

// .text 0x68, 0x08 bytes. the byte at +0x44f.
unsigned char fn_9_68(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x58, 0x10 bytes. stores a DOL float at +0x448.
void fn_9_58(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x1C, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_9_1C(void* out, const CPhysicsActor* self) {
  fn_9_F9BC(out, self->GetBoundingBox());
}

// .text 0x10, 0x0C bytes. a constant float out of this module's own `.rodata`.
float fn_9_10(void*) { return lbl_9_rodata_400; }

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_9_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the address of the member at +0x928.
void* fn_9_0(void* self) { return static_cast< char* >(self) + 0x928; }
}