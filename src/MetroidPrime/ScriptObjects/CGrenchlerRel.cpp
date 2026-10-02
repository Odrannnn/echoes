// CGrenchlerRel.cpp - Grenchler's (module 27) head, .text 0x0..0x168: the eighteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMediumIngRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Grenchler/symbols.txt`:
//
//   0x000 fn_27_0   0x08  addi r3,r3,0x7c0
//   0x008 fn_27_8   0x3C  GetBoundingBox into a local, then fn_27_13C6C(out, &box)
//   0x044 fn_27_44  0x08  lbz r3, 0x44f(r3)
//   0x04C fn_27_4C  0x08  li r3,0
//   0x054 fn_27_54  0x08  li r3,0
//   0x05C fn_27_5C  0x08  li r3,0
//   0x064 fn_27_64  0x10  *id = kInvalidUniqueId
//   0x074 fn_27_74  0x0C  byte at +0x34c, bit 3
//   0x080 fn_27_80  0x0C  lbl_8041B758
//   0x08C fn_27_8C  0x08  addi r3,r3,0x754
//   0x094 fn_27_94  0x08  li r3,1
//   0x09C fn_27_9C  0x08  li r3,0
//   0x0A4 fn_27_A4  0x08  li r3,0
//   0x0AC fn_27_AC  0x1C  three floats from self+0x54 -> *out
//   0x0C8 fn_27_C8  0x2C  virtual dispatch, vtable slot 0x38
//   0x0F4 RELExit   0x24  li r3,0 / bl fn_80218A38
//   0x118 RELMain   0x20  bl fn_27_138
//   0x138 fn_27_138 0x30  lbl_27_bss_40 = fn_27_168 ; fn_80218A38(&lbl_27_bss_40)
//
// **This block is the family's in the same order as `CMediumIngRel.cpp`'s, plus three
// predicates**, and that is measured rather than read off the `fn_<id>_<off>` names, which say
// nothing about which function is which. Diffing dtk's `auto_00_00000000_text.s` for the 0x168
// this module claims against `CMediumIngRel.cpp`'s 0x150: both open `addi r3,r3,0x7c0` and then
// the `GetBoundingBox` wrapper, both run three `li r3,0` predicates in a row and both have **no**
// `skDamageHitTime__10CPatterned` store at +0x448, but MediumIng goes straight from `addi r3,r3,0x754` to its
// three-float copy where this module runs `li r3,1`, `li r3,0`, `li r3,0` first - which is the
// whole of the 0x18-byte difference between the two claims (0x150 vs 0x168, 18 functions against
// 15). So none of the spellings had to be discovered: each is the one `CMediumIngRel.cpp` or
// `CMysteryFlyerRel.cpp` already reproduces at 100%. The two that read oddly in dtk's rendering
// carry over with their caveats, and they are dtk's rendering rather than the source's: `fn_27_74`'s
// `extrwi r3, r0, 1, 28` is the word `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to
// one bit - bit 3 - and `fn_27_AC` interleaves its loads and stores, so it is written as three
// subscript stores rather than as a `CVector3f` copy.
//
// `fn_27_8` is not an `optional_object` problem, though it returns one. Retail calls the
// converting constructor *out of line* at 0x13C6C - six words copied out of `r4+0x00..r4+0x14`
// and a `stb 1, 0x18(r3)`, i.e. six words of `CAABox` and a validity byte at +0x18 - and that
// function stays unclaimed. So `fn_27_8` is one call, `fn_27_13C6C(out, self->GetBoundingBox())`,
// with the constructor declared by its dtk name exactly as `CMysteryFlyerRel.cpp` declares
// `fn_45_2BBC` and `CMediumIngRel.cpp` declares `fn_41_A708`. `self` needs no move because
// `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3); the frame is `-0x30`
// for the 0x18-byte temporary.
//
// `fn_27_C8` is a vtable entry, not a free function: `.data:0xA54` - CGrenchler's own vtable -
// stores it at offset 0x3C, which calls slot 0x38, named `HealthInfo__3CAiFv` in the same table
// (and the second table, `.data:0xED4`, stores it at the same 0x3C above `HealthInfo__6CActorFv`).
// So the call below is a member call, and that spelling is measured rather than guessed: loading
// the vtable by hand compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured
// 99.09% in `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its own virtual-dispatch
// path, and it lays a class's virtuals out the way retail's vtable is laid out - two leading words
// then one word per virtual - so thirteen virtuals put the thirteenth at 0x38. The slots are named
// by position because no header here models a CActor virtual, and none of them is defined or
// called from this file, because the only object that carries this vtable is the module's own
// retail bytes.
//
// **No dead-strip hazard, and that is measured**: `config/G2ME01/rels/Grenchler/ldscript.lcf`
// puts all fifteen of `fn_27_0` .. `fn_27_C8` in its FORCEACTIVE list and `.data:0xA54` stores
// every one of them, so nothing here needs a `force_active:` entry in
// `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the
// module's own entry points and `fn_27_138` is called from `RELMain`, so the trio survives for
// the reason it does everywhere in this family.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218A38` is the DOL's 0x80218A38, two instructions,
//     `stw r3, gLoader_Grenchler@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218A38_text.s`),
//     immediately after `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
//     0x80218A0C, which is 0x2C bytes and so ends exactly at 0x80218A38. So it stores the
//     *address* of a loader slot, not a loader. `src/MetroidPrime/ScriptLoader/Grenchler.cpp`
//     reads it as a loader pointer to call, which is why the store below hands it
//     `&lbl_27_bss_40` and why that slot is four bytes wide. **The import name is the plain
//     `fn_80218A38`** that `config/G2ME01/symbols.txt:9474` already gives it - not the long
//     MWCC-mangled `SetLoader_...` form `CIngSnatchingSwarmRel.cpp` had to spell out - so no
//     `symbols.txt` rename is needed and the DOL is untouched. It is `extern "C"`: an alias
//     would be a different symbol and the call would resolve to nothing.
//   - `lbl_27_bss_40` is `.bss:0x40`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **`lbl_27_bss_40` and not `lbl_27_bss_0`** - this module's `.bss:0x0` is a
//     different 8-byte object, read and written far above the head - which is why the name below
//     is the one it is. This unit's split claims `.text` only, so dtk's `.bss` object has to
//     define it, and a second definition under MWCC is what produced mwldeppc's internal linker
//     error on ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Everything from fn_27_168 (0x168, 0xE00), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model.
// The 300-odd functions above it are CGrenchler's members and stay retail.
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

// The vtable slot fn_27_C8 dispatches to; see the note above on the layout.
class CGrenchlerDispatch {
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
CEntity* fn_27_168(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218A38(FScriptLoader* loader);
// .text 0x13C6C, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_27_13C6C(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_27_bss_40;
#else
FScriptLoader lbl_27_bss_40 = 0;
#endif

// .text 0x138, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_27_138() {
  lbl_27_bss_40 = fn_27_168;
  fn_80218A38(&lbl_27_bss_40);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMediumIngRel.cpp`, `CMysteryFlyerRel.cpp` and `CDarkCommandoRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_27_168` and `fn_80218A38`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `Grenchler.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so
// the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_27_138(); }

void RELExit() { fn_80218A38(nullptr); }
#else
void mp_relmain_grenchler() { fn_27_138(); }

void mp_relexit_grenchler() { fn_80218A38(nullptr); }
#endif

// .text 0xC8, 0x2C bytes. Vtable entry 0x3C of CGrenchler; the call it makes targets vtable slot
// 0x38, which `.data:0xA54` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CGrenchlerDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_27_C8(CGrenchlerDispatch* self) { self->Slot12(); }

// .text 0xAC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_27_AC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xA4, 0x08 bytes. a predicate that is always false.
bool fn_27_A4(void*) { return false; }

// .text 0x9C, 0x08 bytes. a predicate that is always false.
bool fn_27_9C(void*) { return false; }

// .text 0x94, 0x08 bytes. a predicate that is always true.
bool fn_27_94(void*) { return true; }

// .text 0x8C, 0x08 bytes. the address of the member at +0x754.
void* fn_27_8C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x80, 0x0C bytes. a constant float out of the DOL.
float fn_27_80(void*) { return lbl_8041B758; }

// .text 0x74, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_27_74(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x64, 0x10 bytes. resets the unique id to the invalid value.
void fn_27_64(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x5C, 0x08 bytes. a predicate that is always false.
bool fn_27_5C(void*) { return false; }

// .text 0x54, 0x08 bytes. a predicate that is always false.
bool fn_27_54(void*) { return false; }

// .text 0x4C, 0x08 bytes. a predicate that is always false.
bool fn_27_4C(void*) { return false; }

// .text 0x44, 0x08 bytes. the byte at +0x44f.
unsigned char fn_27_44(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x8, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_27_8(void* out, const CPhysicsActor* self) {
  fn_27_13C6C(out, self->GetBoundingBox());
}

// .text 0x0, 0x08 bytes. the address of the member at +0x7C0.
void* fn_27_0(void* self) { return static_cast< char* >(self) + 0x7C0; }
}
