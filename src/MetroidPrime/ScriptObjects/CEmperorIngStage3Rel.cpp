// CEmperorIngStage3Rel.cpp - EmperorIngStage3's (module 18) head, .text 0x0..0xF8: the fourteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/KrocussAccessors.cpp`, and the ranges come from
// `config/G2ME01/rels/EmperorIngStage3/symbols.txt`:
//
//   0x000 fn_18_0   0x08  li r3,1                       a predicate that is always true
//   0x008 fn_18_8   0x3C  GetBoundingBox into a local, then fn_18_DAEC(out, &box)
//   0x044 fn_18_44  0x08  lbz r3, 0x44f(r3)
//   0x04C fn_18_4C  0x08  li r3,0
//   0x054 fn_18_54  0x08  li r3,0
//   0x05C fn_18_5C  0x08  li r3,0
//   0x064 fn_18_64  0x08  li r3,0
//   0x06C fn_18_6C  0x10  *self = kInvalidUniqueId
//   0x07C fn_18_7C  0x0C  byte at +0x34c, bit 3
//   0x088 fn_18_88  0x08  addi r3,r3,0x754
//   0x090 fn_18_90  0x08  li r3,1
//   0x098 fn_18_98  0x1C  three floats from self+0x54 -> *out
//   0x0B4 fn_18_B4  0x2C  virtual dispatch, vtable slot 0x38
//   0x0E0 fn_18_E0  0x18  *(self+0x48c)+0x37c == 6
//
// The claim is **not** byte for byte identical to Krocuss's or MysteryFlyer's block, and the
// differences are visible in the table above rather than guessed:
//   - this module has no `lbl_8041AAB8` float store at +0x448 and no `lbl_8041B758` float
//     accessor, so the only DOL global its relocations name is `kInvalidUniqueId`;
//   - there are four `li r3,0` predicates in a row above `fn_18_6C`, not three and not two;
//   - `fn_18_90` is an always-true predicate where the family usually puts an always-false one;
//   - and there is a function the family does not have at all, `fn_18_E0`.
// Those four accessors that do coincide are written exactly as `KrocussAccessors.cpp` writes
// them, each measured there; see that file for the two that read oddly in dtk's rendering
// (`fn_18_7C`'s `extrwi` and `fn_18_98`'s interleaved copy).
//
// **`fn_18_8` is not an `optional_object` problem**, though it returns one. Retail *calls* the
// converting constructor, out of line at `fn_18_DAEC`, which this unit does not claim, so the
// wrapper is the free function it compiles to - hidden return pointer in r3, `self` in r4 - and
// calls `fn_18_DAEC` by its dtk name, like every other unclaimed neighbour. `self` needs no move
// because `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3). That is
// instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`.
//
// `fn_18_B4` is a vtable entry, and loading the vtable by hand gives `lwz r3,0(r3)` where retail
// has `lwz r12,0(r3)`; it is a member call against a stand-in class with thirteen virtuals,
// which puts the last one at slot 0x38 and gives retail's seven instructions byte for byte. The
// same arrangement `CMysteryFlyerRel.cpp` and `CAtomicAlphaRel.cpp` measure. The stand-in emits
// no vtable, because nothing here constructs it: the call is indirect.
//
// Everything from fn_18_F8 (0xF8, 0x11C) up is left unclaimed: the module's own entity code,
// which is behavioural class code needing the CActor/CPatterned hierarchy this tree does not
// model. **`RELMain` and `RELExit` are in that unclaimed remainder** - they are at 0xC330 and
// 0xC30C, immediately above `fn_18_C290` (0xC290, 0x7C), so they are not contiguous with this
// head and one unit cannot claim both. See the Attempted modules table in
// `docs/RUNNING_THE_DECOMP.md`.
//
// The one out-of-line callee is named by what it is, not invented: `fn_18_DAEC` is this module's
// own `optional_object<CAABox>` converting constructor, and it is declared `extern "C"` because
// inside a `C++` namespace an alias would be a different symbol and the call would resolve to
// nothing.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "types.h"

extern "C" const unsigned short kInvalidUniqueId;

// `fn_18_8` is the only body here that is not raw offsets, and it is the reason this file is
// **listed in `files.cmake` behind an `#ifdef __MWERKS__`**, the arrangement
// `KrocussAccessors.cpp` measures. On the host it would make the port link `fn_18_DAEC` and a
// host-mangled `CPhysicsActor::GetBoundingBox`, and `tools/link_check.sh --strict` fails on a
// growing undefined count. With the guard the port's undefined count is unchanged (measured: 259
// undefined, 0 duplicates), and the MWCC branch is the retail source token for token, so the
// matching build cannot see the change. The other twelve functions read raw offsets and DOL
// globals and nothing else, which is what made the rest of this family safe to list.
//
// The port reads `EmperorIngStage3.rel` off the disc through `platform/rel.cpp` and never calls into
// the module, so nothing is lost.
#ifdef __MWERKS__
#include "Kyoto/Math/CAABox.hpp"

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};
#endif

// The vtable slot fn_18_B4 dispatches to; see `CMysteryFlyerRel.cpp` for the layout. Nothing here
// constructs it, so it emits no vtable and the call is indirect - which is why this class, unlike
// the `CPhysicsActor` stand-in above, needs no guard.
class CEmperorIngStage3Dispatch {
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
// .text 0xDAEC, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
// `const CAABox&` is load-bearing: by value the frame in `fn_18_8` grows to 0x40 and the unit no
// longer matches.
void fn_18_DAEC(void* out, const CAABox& box);
#endif

// .text 0xE0, 0x18 bytes. `self+0x48c` is a pointer to the sub-object holding the word at +0x37c,
// and the tail is the `== 6` idiom (`subfic`/`cntlzw`/`srwi 5`) rather than a compare and a branch,
// because the result is the return value and nothing is branched on.
bool fn_18_E0(const void* self) {
  const char* sub = *reinterpret_cast< const char* const* >(static_cast< const char* >(self) + 0x48C);
  return *reinterpret_cast< const int* >(sub + 0x37C) == 6;
}

// .text 0xB4, 0x2C bytes. The call targets vtable slot 0x38.
void fn_18_B4(CEmperorIngStage3Dispatch* self) { self->Slot12(); }

// .text 0x98, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_18_98(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x90, 0x08 bytes. a predicate that is always true.
bool fn_18_90(void*) { return true; }

// .text 0x88, 0x08 bytes. the address of the member at +0x754.
void* fn_18_88(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x7C, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_18_7C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x6C, 0x10 bytes. resets the unique id to the invalid value.
void fn_18_6C(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_18_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. a predicate that is always false.
bool fn_18_5C(void*) { return false; }

// .text 0x54, 0x08 bytes. a predicate that is always false.
bool fn_18_54(void*) { return false; }

// .text 0x4C, 0x08 bytes. a predicate that is always false.
bool fn_18_4C(void*) { return false; }

// .text 0x44, 0x08 bytes. the byte at +0x44f.
unsigned char fn_18_44(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

#ifdef __MWERKS__
// .text 0x8, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_18_8(void* out, const CPhysicsActor* self) {
  fn_18_DAEC(out, self->GetBoundingBox());
}
#endif

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_18_0(void*) { return true; }
}
