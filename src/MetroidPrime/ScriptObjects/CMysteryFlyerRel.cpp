// CMysteryFlyerRel.cpp - MysteryFlyer's (module 45) head, .text 0x0..0x170: the eighteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` and
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`, and the ranges come from
// `config/G2ME01/rels/MysteryFlyer/symbols.txt`:
//
//   0x000 fn_45_0   0x08  li r3,1
//   0x008 fn_45_8   0x08  addi r3,r3,0x818
//   0x010 fn_45_10  0x3C  GetBoundingBox into a local, then fn_45_2BBC(out, &box)
//   0x04C fn_45_4C  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x05C fn_45_5C  0x08  lbz r3, 0x44f(r3)
//   0x064 fn_45_64  0x08  li r3,0
//   0x06C fn_45_6C  0x08  li r3,0
//   0x074 fn_45_74  0x10  *self = kInvalidUniqueId
//   0x084 fn_45_84  0x0C  byte at +0x34c, bit 3
//   0x090 fn_45_90  0x0C  lbl_8041B758
//   0x09C fn_45_9C  0x08  addi r3,r3,0x754
//   0x0A4 fn_45_A4  0x08  li r3,1
//   0x0AC fn_45_AC  0x08  li r3,0
//   0x0B4 fn_45_B4  0x1C  three floats from self+0x54 -> *out
//   0x0D0 fn_45_D0  0x2C  virtual dispatch, vtable slot 0x38
//   0x0FC RELExit   0x24  li r3,0 / bl fn_80232868
//   0x120 RELMain   0x20  bl fn_45_140
//   0x140 fn_45_140 0x30  lbl_45_bss_0 = fn_45_170 ; fn_80232868(&lbl_45_bss_0)
//
// The accessors 0x4C..0xFC are the bodies `CAtomicAlphaRel.cpp` carries (there at 0x10..0xC8),
// each measured there; see that file for the two that read oddly in dtk's rendering (`fn_45_84`'s
// `extrwi` and `fn_45_B4`'s interleaved copy).
//
// **`fn_45_10` is not an `optional_object` problem**, though it returns one. An earlier run read
// `fn_45_2BBC` (six words copied, then `stb 1, 0x18(r3)`) as `optional_object<CAABox>`'s
// converting constructor and stopped, because the header's constructor sets the flag in the
// mem-init and instantiating it would emit a trailing pool. Both are true and neither applies:
// retail *calls* the constructor, out of line at 0x2BBC, which this unit does not claim. So
// `fn_45_10` is written as the free function it compiles to - hidden return pointer in r3, `self`
// in r4 - and calls `fn_45_2BBC` by its dtk name, like every other unclaimed neighbour. `self`
// needs no move because `GetBoundingBox` takes `this` in r4 too (its own return pointer is r3).
//
// Everything from fn_45_170 (0x170, 0x30C), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `fn_80232868` is the DOL's 0x80232868, two instructions, immediately after
//     `LoadMysteryFlyer__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x8023283C:
//     `stw r3, gLoader_MysteryFlyer; blr`. So it stores the *address* of a loader slot, not a
//     loader. `LoadMysteryFlyer` in `src/MetroidPrime/ScriptLoader/MysteryFlyer.cpp` (a `Matching`
//     unit) reads it as `lwz r6, gLoader_MysteryFlyer; lwz r12, 0(r6); mtctr r12; bctrl`, which is
//     why the store below hands it `&lbl_45_bss_0` and why that slot is four bytes wide. That
//     file also records that the setter is deliberately not claimed in the DOL, because REL
//     modules import it by its retail name - so it stays in dtk's auto unit and this declaration
//     is the same one `CPlantScarabSwarmRel.cpp` makes for `fn_8022FFF8`. It is `extern "C"`:
//     an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_45_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_45_D0 dispatches to; see `CAtomicAlphaRel.cpp` for the layout.
class CMysteryFlyerDispatch {
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
CEntity* fn_45_170(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80232868(FScriptLoader* loader);
// .text 0x2BBC, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_45_2BBC(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_45_bss_0;
#else
FScriptLoader lbl_45_bss_0 = 0;
#endif

// .text 0x140, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_45_140() {
  lbl_45_bss_0 = fn_45_170;
  fn_80232868(&lbl_45_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`, `CPlantScarabSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this
// file in `files.cmake` would make the port link `fn_45_170` and `fn_80232868`, which it cannot,
// and `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps
// reading `MysteryFlyer.rel` off the disc through `platform/rel.cpp`, and the `#else` branch
// exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_45_140(); }

void RELExit() { fn_80232868(nullptr); }
#else
void mp_relmain_mysteryflyer() { fn_45_140(); }

void mp_relexit_mysteryflyer() { fn_80232868(nullptr); }
#endif

// .text 0xD0, 0x2C bytes. The call targets vtable slot 0x38.
void fn_45_D0(CMysteryFlyerDispatch* self) { self->Slot12(); }

// .text 0xB4, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4.
void fn_45_B4(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0xAC, 0x08 bytes. a predicate that is always false.
bool fn_45_AC(void*) { return false; }

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_45_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_45_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0c bytes. a constant float out of the DOL.
float fn_45_90(void*) { return lbl_8041B758; }

// .text 0x84, 0x0c bytes. the flag at +0x34c, bit 3.
bool fn_45_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x74, 0x10 bytes. resets the unique id to the invalid value.
void fn_45_74(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_45_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_45_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_45_5C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4C, 0x10 bytes. stores a DOL float at +0x448.
void fn_45_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_45_10(void* out, const CPhysicsActor* self) {
  fn_45_2BBC(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. the address of the member at +0x818.
void* fn_45_8(void* self) { return static_cast< char* >(self) + 0x818; }

// .text 0x0, 0x08 bytes. a predicate that is always true.
bool fn_45_0(void*) { return true; }
}
