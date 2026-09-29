// CMediumIngRel.cpp - MediumIng's (module 41) head, .text 0x0..0x150: the fifteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp`, and the ranges come from
// `config/G2ME01/rels/MediumIng/symbols.txt`:
//
//   0x000 fn_41_0   0x08  addi r3,r3,0x7c0
//   0x008 fn_41_8   0x3C  GetBoundingBox into a local, then fn_41_A708(out, &box)
//   0x044 fn_41_44  0x08  lbz r3, 0x44f(r3)
//   0x04C fn_41_4C  0x08  li r3,0
//   0x054 fn_41_54  0x08  li r3,0
//   0x05C fn_41_5C  0x08  li r3,0
//   0x064 fn_41_64  0x10  *self = kInvalidUniqueId
//   0x074 fn_41_74  0x0C  byte at +0x34c, bit 3
//   0x080 fn_41_80  0x0C  lbl_8041B758
//   0x08C fn_41_8C  0x08  addi r3,r3,0x754
//   0x094 fn_41_94  0x1C  three floats from self+0x54 -> *out
//   0x0B0 fn_41_B0  0x2C  virtual dispatch, vtable slot 0x38
//   0x0DC RELExit   0x24  li r3,0 / bl fn_80218A6C
//   0x100 RELMain   0x20  bl fn_41_120
//   0x120 fn_41_120 0x30  lbl_41_bss_10 = fn_41_150 ; fn_80218A6C(&lbl_41_bss_10)
//
// **This block is the family in a different order, and that is measured rather than assumed.**
// Diffing dtk's `auto_00_00000000_text.s` for the 0x150 this module claims against
// `CMysteryFlyerRel.cpp`'s 0x170, the two share no two-accessor prefix at all: MysteryFlyer opens
// `li r3,1` / `addi r3,r3,0x818` and this module opens `addi r3,r3,0x7c0` / the `GetBoundingBox`
// wrapper, this module has **three** `li r3,0` predicates in a row where MysteryFlyer has two
// (Tryclops three, Krocuss four), it has **no** `lbl_8041AAB8` float store at +0x448 and **no**
// `li r3,1` anywhere, and its `fn_41_94` - the three-float copy - sits at 0x94 where the family
// usually puts the `lbl_8041B758` accessor. So the set of bodies is the family's and none of the
// spellings had to be discovered: each is the one `CMysteryFlyerRel.cpp` or
// `CIngSpaceJumpGuardianRel.cpp` already reproduces at 100%. The two that read oddly in dtk's
// rendering carry over with their caveats, and they are dtk's rendering rather than the source's:
// `fn_41_74`'s `extrwi r3, r0, 1, 28` is the word `rlwinm r3, r0, 29, 31, 31`, a rotate left by
// 32-3 masked to one bit - bit 3 - and `fn_41_94` interleaves its loads and stores, so it is
// written as three subscript stores rather than as a `CVector3f` copy (`CMetareeSwarmRel.cpp`
// records that the built-in spelling reverses the loads and the score falls).
//
// `fn_41_8` is not an `optional_object` problem, though it returns one. Retail calls the
// converting constructor *out of line* at 0xA708 - six words copied out of `r4+0x00..r4+0x14`
// and a `stb 1, 0x18(r3)`, i.e. six words of `CAABox` and a validity byte at +0x18 - and that
// function stays unclaimed. So `fn_41_8` is one call, `fn_41_A708(out, self->GetBoundingBox())`,
// with the constructor declared by its dtk name exactly as `CMysteryFlyerRel.cpp` declares
// `fn_45_2BBC`. `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own
// return pointer is r3); the frame is `-0x30` for the 0x18-byte temporary.
//
// `fn_41_B0` is a vtable entry, not a free function: `.data:0x5C8` - CMediumIng's own vtable,
// 0x148 bytes, two leading words and one per virtual - stores it at offset 0x3C, which calls
// slot 0x38, named `HealthInfo__3CAiFv` in the same table (and the second table, `.data:0x77C`,
// stores it at the same 0x3C above `HealthInfo__6CActorFv`). So the call below is a member call,
// and that spelling is measured rather than guessed: loading the vtable by hand compiles to
// `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% in `CIngPuddleRel.cpp`).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out - two leading words then one word per virtual - so
// thirteen virtuals put the thirteenth at 0x38. The slots are named by position because no header
// here models a CActor virtual, and none of them is defined or called from this file, because the
// only object that carries this vtable is the module's own retail bytes.
//
// **No dead-strip hazard, and that is measured**: `config/G2ME01/rels/MediumIng/ldscript.lcf`
// puts all twelve of `fn_41_0` .. `fn_41_B0` in its FORCEACTIVE list, and `.data:0x5C8` stores
// every one of them, so nothing here needs a `force_active:` entry in `config/G2ME01/config.yml`
// (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the module's own entry points and
// `fn_41_120` is called from `RELMain`, so the trio survives for the reason it does everywhere
// in this family.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218A6C` is the DOL's 0x80218A6C, two instructions,
//     `stw r3, gLoader_MediumIng@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218A6C_text.s`),
//     immediately after `LoadMediumIng__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
//     0x80218A40, which is 0x2C bytes and so ends exactly at 0x80218A6C. So it stores the
//     *address* of a loader slot, not a loader. `src/MetroidPrime/ScriptLoader/MediumIng.cpp` -
//     a `Matching` unit, and the file that already records why this setter is deliberately not
//     claimed in the DOL - reads the slot as `(*gLoader_MediumIng.value)(mgr, input, info)`,
//     which is why the store below hands it `&lbl_41_bss_10` and why that slot is four bytes
//     wide. **The import name is the plain `fn_80218A6C`** that `config/G2ME01/symbols.txt:9476`
//     already gives it - not the long MWCC-mangled `SetLoader_...` form
//     `CIngSnatchingSwarmRel.cpp` had to spell out - so no `symbols.txt` rename is needed and
//     the DOL is untouched. It is `extern "C"`: an alias would be a different symbol and the
//     call would resolve to nothing.
//   - `lbl_41_bss_10` is `.bss:0x10`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **`lbl_41_bss_10` and not `lbl_41_bss_0`** - this module's `.bss:0x0` is a
//     different 4-byte slot, read and written by `fn_41_2A74`'s code far above the head - which
//     is why the name below is the one it is. This unit's split claims `.text` only, so dtk's
//     `.bss` object has to define it, and a second definition under MWCC is what produced
//     mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and a host
//     definition.
//
// Everything from fn_41_150 (0x150, 0x868), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned/CAi hierarchy this tree does not
// model. The 160 functions above it are CMediumIng's members and stay retail.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

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

// The vtable slot fn_41_B0 dispatches to; see the note above on the layout.
class CMediumIngDispatch {
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
CEntity* fn_41_150(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218A6C(FScriptLoader* loader);
// .text 0xA708, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_41_A708(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_41_bss_10;
#else
FScriptLoader lbl_41_bss_10 = 0;
#endif

// .text 0x120, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_41_120() {
  lbl_41_bss_10 = fn_41_150;
  fn_80218A6C(&lbl_41_bss_10);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp` and `CIngSpaceJumpGuardianRel.cpp`: listing this file in `files.cmake`
// would make the port link `fn_41_150` and `fn_80218A6C`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `MediumIng.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so
// the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_41_120(); }

void RELExit() { fn_80218A6C(nullptr); }
#else
void mp_relmain_mediuming() { fn_41_120(); }

void mp_relexit_mediuming() { fn_80218A6C(nullptr); }
#endif

// .text 0xB0, 0x2C bytes. Vtable entry 0x3C of CMediumIng; the call it makes targets vtable slot
// 0x38, which `.data:0x5C8` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CMediumIngDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_41_B0(CMediumIngDispatch* self) { self->Slot12(); }

// .text 0x94, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_41_94(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x8C, 0x08 bytes. the address of the member at +0x754.
void* fn_41_8C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x80, 0x0C bytes. a constant float out of the DOL.
float fn_41_80(void*) { return lbl_8041B758; }

// .text 0x74, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_41_74(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x64, 0x10 bytes. resets the unique id to the invalid value.
void fn_41_64(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x5C, 0x08 bytes. a predicate that is always false.
bool fn_41_5C(void*) { return false; }

// .text 0x54, 0x08 bytes. a predicate that is always false.
bool fn_41_54(void*) { return false; }

// .text 0x4C, 0x08 bytes. a predicate that is always false.
bool fn_41_4C(void*) { return false; }

// .text 0x44, 0x08 bytes. the byte at +0x44f.
unsigned char fn_41_44(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x8, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_41_8(void* out, const CPhysicsActor* self) {
  fn_41_A708(out, self->GetBoundingBox());
}

// .text 0x0, 0x08 bytes. the address of the member at +0x7C0.
void* fn_41_0(void* self) { return static_cast< char* >(self) + 0x7C0; }
}
