// CSplitterRel.cpp - Splitter's (module 75) head, .text 0x0..0xFC: the fifteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CSandBossRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Splitter/symbols.txt`:
//
//   0x000 fn_75_0   0x08  the address of the member at +0xd6c
//   0x008 fn_75_8   0x08  the address of the member at +0xe5c
//   0x010 fn_75_10  0x08  a predicate that is always true
//   0x018 fn_75_18  0x3C  GetBoundingBox into a local, then fn_75_7C44(out, &box)
//   0x054 fn_75_54  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x064 fn_75_64  0x08  the byte at +0x44f
//   0x06C fn_75_6C  0x08  a predicate that is always false
//   0x074 fn_75_74  0x08  a predicate that is always false
//   0x07C fn_75_7C  0x10  *self = kInvalidUniqueId
//   0x08C fn_75_8C  0x0C  byte at +0x34c, bit 3
//   0x098 fn_75_98  0x0C  lbl_8041B758
//   0x0A4 fn_75_A4  0x08  the address of the member at +0x754
//   0x0AC fn_75_AC  0x08  a predicate that is always true
//   0x0B4 fn_75_B4  0x1C  three floats from self+0x54 -> *out
//   0x0D0 fn_75_D0  0x2C  virtual dispatch, vtable slot 0x38
//
// **The block is the family with two member-address accessors in front**, and that is measured
// from `build/G2ME01/Splitter/asm/auto_00_00000000_text.s`, not from the `fn_<id>_<off>` names,
// which say nothing about which function is which. SandBoss opens with `li r3,1` twice and runs
// three `li r3,0` predicates in a row; this one opens with `addi r3,r3,0xd6c` then
// `addi r3,r3,0xe5c`, so its GetBoundingBox wrapper sits at 0x18 rather than at 0x10 or 0x0, and
// it runs two `li r3,0` predicates where MysteryFlyer runs two but SandBoss runs three. The
// thirteen bodies from 0x54 on are otherwise the family's, and that is counted rather than
// eyeballed: 39 instructions over the 14 accessors below the wrapper here against SandBoss's 39
// over its 14, and the two multisets differ by exactly one `li r3,1` and one `li r3,0` traded
// for the two `addi r3,r3,<member>` above, with nothing else between them.
//
// `fn_75_18` is not an `optional_object` problem, though it returns one. Retail *calls* the
// constructor, out of line at 0x7C44 (`fn_75_7C44`: `li r0,1`, six words copied, then
// `stb r0,0x18(r3)`), which this unit does not claim, so `fn_75_18` is written as the free
// function it compiles to - hidden return pointer in r3, `self` in r4 - and calls `fn_75_7C44` by
// its dtk name, like every other unclaimed neighbour. `self` needs no move because
// `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3). The frame is 0x30
// bytes, so the `const CAABox&` below has to stay a const reference: by value would grow it to
// 0x40.
//
// Everything from fn_75_FC (0xFC, 0x370) up is left unclaimed: behavioural class code that needs
// the CActor/CPatterned hierarchy this tree does not model. This module's own loader
// registration `fn_75_8254` (0x8254) and the RELExit/RELMain pair at 0x8210/0x8234 are not
// adjacent to this range - they sit at the far end of `.text` - so they are the separate unit
// `CSplitterRelMain.cpp`, not part of this one. The other 301 functions in the module are left retail too (317
// text symbols in all: 15 ours, 5 `REL_Setup`, 297 unclaimed), so dtk fills them and the
// module's sha1 against `config/G2ME01/config.yml` still holds.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/TGameTypes.hpp"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
//
// This file is in `files.cmake` (it has no RELMain/RELExit, so `tools/check_files_cmake.py`
// requires it), and like `RipperAccessors.cpp` the wrapper and its stand-in are MWCC-only: on the
// host they would make the port link `fn_75_7C44` and a host-mangled `GetBoundingBox`. The port
// reads `Splitter.rel` off the disc and never calls into the module, so nothing is lost.
#ifdef __MWERKS__
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};
#endif

// The vtable slot fn_75_D0 dispatches to; see `CAtomicAlphaRel.cpp` for the layout.
class CSplitterDispatch {
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
#ifdef __MWERKS__
// .text 0x7C44, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_75_7C44(void* out, const CAABox& box);
#endif

// .text 0xD0, 0x2C bytes. The call targets vtable slot 0x38.
void fn_75_D0(CSplitterDispatch* self) { self->Slot12(); }

// .text 0xB4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_75_B4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xAC, 0x08 bytes. a predicate that is always true.
bool fn_75_AC(void*) { return true; }

// .text 0xA4, 0x08 bytes. the address of the member at +0x754.
void* fn_75_A4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x98, 0x0c bytes. a constant float out of the DOL.
float fn_75_98(void*) { return lbl_8041B758; }

// .text 0x8C, 0x0c bytes. the flag at +0x34c, bit 3.
bool fn_75_8C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x7C, 0x10 bytes. resets the unique id to the invalid value.
void fn_75_7C(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x74, 0x08 bytes. a predicate that is always false.
bool fn_75_74(void*) { return false; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_75_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. the byte at +0x44f.
unsigned char fn_75_64(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x54, 0x10 bytes. stores a DOL float at +0x448.
void fn_75_54(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

#ifdef __MWERKS__
// .text 0x18, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_75_18(void* out, const CPhysicsActor* self) {
  fn_75_7C44(out, self->GetBoundingBox());
}
#endif

// .text 0x10, 0x08 bytes. a predicate that is always true.
bool fn_75_10(void*) { return true; }

// .text 0x8, 0x08 bytes. the address of the member at +0xe5c.
void* fn_75_8(void* self) { return static_cast< char* >(self) + 0xe5c; }

// .text 0x0, 0x08 bytes. the address of the member at +0xd6c.
void* fn_75_0(void* self) { return static_cast< char* >(self) + 0xd6c; }
}
