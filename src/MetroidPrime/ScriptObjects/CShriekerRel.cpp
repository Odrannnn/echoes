// CShriekerRel.cpp - Shrieker's (module 69) head, .text 0x0..0x154: the seventeen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CGrenchlerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Shrieker/symbols.txt`:
//
//   0x000 fn_69_0    0x08  addi r3,r3,0x8c4
//   0x008 fn_69_8    0x08  li r3,1
//   0x010 fn_69_10   0x3C  GetBoundingBox into a local, then fn_69_6EE4(out, &box)
//   0x04C fn_69_4C   0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x05C fn_69_5C   0x08  lbz r3, 0x44f(r3)
//   0x064 fn_69_64   0x08  li r3,0
//   0x06C fn_69_6C   0x08  li r3,0
//   0x074 fn_69_74   0x10  *id = kInvalidUniqueId
//   0x084 fn_69_84   0x0C  byte at +0x34c, bit 3
//   0x090 fn_69_90   0x0C  lbl_8041B758
//   0x09C fn_69_9C   0x08  addi r3,r3,0x754
//   0x0A4 fn_69_A4   0x08  li r3,1
//   0x0AC fn_69_AC   0x08  li r3,0
//   0x0B4 fn_69_B4   0x2C  virtual dispatch, vtable slot 0x38
//   0x0E0 RELExit    0x24  li r3,0 / bl fn_80218C30
//   0x104 RELMain    0x20  bl fn_69_124
//   0x124 fn_69_124  0x30  lbl_69_bss_60 = fn_69_154 ; fn_80218C30(&lbl_69_bss_60)
//
// **The block is `CMysteryFlyerRel.cpp`'s with three differences, and all three are measured**
// by diffing `build/G2ME01/Shrieker/asm/auto_00_00000000_text.s` over 0x0..0x154 against
// `CMysteryFlyerRel.cpp`'s over 0x0..0x170 rather than read off the `fn_<id>_<off>` names,
// which say nothing about which function is which:
//   - the first two are swapped in role: MysteryFlyer opens `li r3,1` and puts its
//     `addi r3,r3,0x818` second, this module opens `addi r3,r3,0x8c4` and puts `li r3,1`
//     second, so the member offset is 0x8C4 rather than 0x818;
//   - this module has **no** three-float copy (`CMysteryFlyerRel.cpp`'s `fn_45_B4`, 0x1C bytes
//     at 0xB4), so the vtable entry `fn_69_B4` sits at 0xB4 instead of 0xD0; and
//   - everything from 0x4C to 0xB4 is the same fourteen-accessor block, `skDamageHitTime__10CPatterned` store at
//     +0x448, `lbl_8041B758` accessor, `+0x34c` bit 3 and all.
// So no spelling had to be discovered: every body is one `CMysteryFlyerRel.cpp` or
// `CGrenchlerRel.cpp` already reproduces at 100%.
//
// **`fn_69_10` is not an `optional_object` template problem**, though it returns one. Retail
// *calls* the converting constructor out of line at 0x6EE4 - six words copied out of
// `r4+0x00..r4+0x14`, then `stb 1, 0x18(r3)`, i.e. six words of `CAABox` and a validity byte at
// +0x18 - and that function stays unclaimed, so it is one call by its dtk name, exactly as
// `CMysteryFlyerRel.cpp` declares `fn_45_2BBC` and `CGrenchlerRel.cpp` declares `fn_27_13C6C`.
// `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own return pointer
// is r3); the frame is `-0x30` for the 0x18-byte temporary.
//
// `fn_69_B4` is a vtable entry, not a free function: `.data:0x314` - CShrieker's own vtable -
// stores it at offset 0x3C, which calls slot 0x38, named `HealthInfo__3CAiFv` in the same table
// (and the table is the same one `CGrenchler`'s `fn_27_C8` is an entry of). So the call below is
// a member call, and that spelling is measured rather than guessed: loading the vtable by hand
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% in
// `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it
// lays a class's virtuals out the way retail's vtable is laid out - two leading words then one
// word per virtual - so thirteen virtuals put the thirteenth at 0x38. The slots are named by
// position because no header here models a CActor virtual, and none of them is defined or called
// from this file, because the only object that carries this vtable is the module's own retail
// bytes.
//
// **No dead-strip hazard, and that is measured**: `config/G2ME01/rels/Shrieker`'s
// `ldscript.lcf` puts all fourteen of `fn_69_0` .. `fn_69_B4` in its `FORCEACTIVE` block, and
// `.data:0x314` stores every one of them, so nothing here needs a `force_active:` entry in
// `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the
// module's own entry points and `fn_69_124` is called from `RELMain`, so that trio survives for
// the reason it does everywhere in this family.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218C30` is the DOL's 0x80218C30, two instructions,
//     `stw r3, gLoader_Shrieker@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218C30_text.s`),
//     immediately after `LoadShrieker__FR13CStateManagerR12CInputStreamR11CEntityInfo` at
//     0x80218C04, which is 0x2C bytes and so ends exactly at 0x80218C30. So it stores the
//     *address* of a loader slot, not a loader. `src/MetroidPrime/ScriptLoader/Shrieker.cpp` (a
//     `Matching` unit) reads it as a loader pointer to call, which is why the store below hands
//     it `&lbl_69_bss_60` and why that slot is four bytes wide. **The import name is the plain
//     `fn_80218C30`** that `config/G2ME01/symbols.txt:9492` already gives it, so no
//     `symbols.txt` rename is needed and the DOL is untouched. It is `extern "C"`: an alias
//     would be a different symbol and the call would resolve to nothing.
//   - `lbl_69_bss_60` is `.bss:0x60`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **`lbl_69_bss_60` and not `lbl_69_bss_0`** - this module's `.bss` holds nine
//     objects (`build/G2ME01/Shrieker/asm/auto_05_00000000_bss.s`) and `.bss:0x0` is a 16-byte
//     float block read and written far above the head. This unit's split claims `.text` only,
//     so dtk's `.bss` object has to define it, and a second definition under MWCC is what
//     produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC
//     and a host definition.
//
// Everything from fn_69_154 (0x154, 0x7D8), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CActor/CPatterned hierarchy this tree does not model.
// The 150-odd functions above it are CShrieker's members and stay retail.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

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
// with every function at 100% (`CMysteryFlyerRel.cpp` measured it). Only the mangled name has to
// agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_69_B4 dispatches to; see the note above on the layout.
class CShriekerDispatch {
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
CEntity* fn_69_154(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80218C30(FScriptLoader* loader);
// .text 0x6EE4, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_69_6EE4(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_69_bss_60;
#else
FScriptLoader lbl_69_bss_60 = 0;
#endif

// .text 0x124, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_69_124() {
  lbl_69_bss_60 = fn_69_154;
  fn_80218C30(&lbl_69_bss_60);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CGrenchlerRel.cpp` and `CMediumIngRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_69_154` and `fn_80218C30`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `Shrieker.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists only so
// the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_69_124(); }

void RELExit() { fn_80218C30(nullptr); }
#else
void mp_relmain_shrieker() { fn_69_124(); }

void mp_relexit_shrieker() { fn_80218C30(nullptr); }
#endif

// .text 0xB4, 0x2C bytes. Vtable entry 0x3C of CShrieker; the call it makes targets vtable slot
// 0x38, which `.data:0x314` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CShriekerDispatch*`.
void fn_69_B4(CShriekerDispatch* self) { self->Slot12(); }

// .text 0xAC, 0x08 bytes. a predicate that is always false.
bool fn_69_AC(void*) { return false; }

// .text 0xA4, 0x08 bytes. a predicate that is always true.
bool fn_69_A4(void*) { return true; }

// .text 0x9C, 0x08 bytes. the address of the member at +0x754.
void* fn_69_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x90, 0x0C bytes. a constant float out of the DOL.
float fn_69_90(void*) { return lbl_8041B758; }

// .text 0x84, 0x0C bytes. the flag at +0x34c, bit 3. dtk renders the rotate as
// `extrwi r3, r0, 1, 28`, which is the word `rlwinm r3, r0, 29, 31, 31`.
bool fn_69_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x74, 0x10 bytes. resets the unique id to the invalid value.
void fn_69_74(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_69_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_69_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_69_5C(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4C, 0x10 bytes. stores a DOL float at +0x448.
void fn_69_4C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_69_10(void* out, const CPhysicsActor* self) {
  fn_69_6EE4(out, self->GetBoundingBox());
}

// .text 0x8, 0x08 bytes. a predicate that is always true.
bool fn_69_8(void*) { return true; }

// .text 0x0, 0x08 bytes. the address of the member at +0x8C4.
void* fn_69_0(void* self) { return static_cast< char* >(self) + 0x8C4; }
}
