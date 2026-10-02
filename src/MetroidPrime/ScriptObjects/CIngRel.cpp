// CIngRel.cpp - Ing's (module 29) head, .text 0x0..0x130: the seventeen functions above the
// module's class code. Same arrangement as `MetroidPrime/ScriptObjects/CRezbitRel.cpp`,
// `CMediumIngRel.cpp`, `CMetroidRel.cpp` and `CGrenchlerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Ing/symbols.txt`:
//
//   0x000 fn_29_0   0x08  addi r3,r3,0x9dc
//   0x008 fn_29_8   0x08  addi r3,r3,0xac8
//   0x010 fn_29_10  0x08  li r3,1
//   0x018 fn_29_18  0x0C  lbl_29_rodata_64, **this module's own** .rodata:0x64
//   0x024 fn_29_24  0x08  lbz r3, 0x44f(r3)
//   0x02C fn_29_2C  0x08  li r3,0
//   0x034 fn_29_34  0x08  li r3,0
//   0x03C fn_29_3C  0x10  *self = kInvalidUniqueId
//   0x04C fn_29_4C  0x0C  byte at +0x34c, bit 3
//   0x058 fn_29_58  0x0C  lbl_8041B758
//   0x064 fn_29_64  0x08  addi r3,r3,0x754
//   0x06C fn_29_6C  0x08  li r3,1
//   0x074 fn_29_74  0x1C  three floats from self+0x54 -> *out
//   0x090 fn_29_90  0x2C  virtual dispatch, vtable slot 0x38
//   0x0BC RELExit   0x24  li r3,0 / bl fn_80218918
//   0x0E0 RELMain   0x20  bl fn_29_100
//   0x100 fn_29_100 0x30  lbl_29_bss_6C = fn_29_130 ; fn_80218918(&lbl_29_bss_6C)
//
// **The block is the family in an order no sibling has, and that is measured rather than assumed
// from the `fn_<id>_<off>` names, which say nothing about which function is which.** Diffing
// `build/G2ME01/Ing/asm/auto_00_00000000_text.s` over the 0x130 this claims against
// `CRezbitRel.cpp`'s 0x168 and `CMediumIngRel.cpp`'s 0x150 instruction for instruction, the
// bodies themselves are all ones a sibling already reproduces, but the arrangement differs three
// ways:
//   - it opens with **two** member-address accessors (`+0x9dc` then `+0xac8`) where Rezbit,
//     MediumIng, Metroid and Grenchler each open with at most one, so the `li r3,1` lands third
//     at 0x10;
//   - its 0x0C slot at 0x18 holds a **module-local `.rodata` constant** (`lbl_29_rodata_64`,
//     `.float 1`) rather than a DOL one - the same distinction `CDarkCommandoRel.cpp`'s
//     `lbl_3_rodata_0` and `CChozoGhostRel.cpp`'s `lbl_8_rodata_0` already carry, so the spelling
//     is an ordinary `extern "C"` declaration and nothing more;
//   - it has **neither** the `GetBoundingBox` wrapper **nor** the `skDamageHitTime__10CPatterned` store at +0x448,
//     so the three-float copy sits at 0x74 and the claim ends 0x38 below Rezbit's.
//
// Two accessors read oddly in dtk's rendering, and they are dtk's rendering rather than the
// source's. `fn_29_4C` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`; dtk prints the middle word
// `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0, where the word
// is opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31` - a rotate left by 32-3 masked to one bit, bit 3.
// `fn_29_74` interleaves its loads and its stores, so it is written as subscript stores rather
// than as a `CVector3f` copy (`CMetareeSwarmRel.cpp` records that the built-in spelling reverses
// the loads and the score falls).
//
// The record is **four bytes at `.bss:0x6C`**, and the position is measured rather than assumed:
// `build/G2ME01/Ing/asm/auto_05_00000000_bss.s` gives `lbl_29_bss_6C` `size:0x4` at 0x6C, while
// this module's `.bss:0x0` is a different 8-byte object its own code reads and writes far above
// the head. The only reader of the slot is `LoadIngs` in the `Matching` unit
// `src/MetroidPrime/ScriptLoader/Ings.cpp`, which reads it as `(*gLoader_Ings.value)(mgr, input,
// info)`, so unlike `CMetroidRel.cpp`'s MetroidAlpha there is no second reader, no
// `__ptmf_scall` and no pointer-to-member-function in the record. That is why no
// `CAABox`/`CPhysicsActor` stand-in is needed at all here, and why `Kyoto/Math/CAABox.hpp` is
// not included: nothing in this unit names a `CAABox`.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218918` is the DOL's 0x80218918, two instructions,
//     `stw r3, gLoader_Ings@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218918_text.s`),
//     immediately after `LoadIngs__FR13CStateManagerR12CInputStreamR11CEntityInfo` at
//     0x802188EC, which is 0x2C bytes and so ends exactly at 0x80218918. So it stores the
//     *address* of a loader slot, not a loader. The import name is the plain `fn_80218918` that
//     `config/G2ME01/symbols.txt` already gives it - not the long MWCC-mangled `SetLoader_...`
//     form `CIngSnatchingSwarmRel.cpp` had to spell out - so no `symbols.txt` rename is needed
//     and the DOL is untouched. It is `extern "C"`: an alias would be a different symbol and the
//     call would resolve to nothing.
//   - `lbl_29_bss_6C` is this unit's split claims `.text` only, so dtk's `.bss` object has to
//     define it, and a second definition under MWCC is what produced mwldeppc's internal linker
//     error on ScriptPlayerProxy (`CRezbitRel.cpp` measured it). Hence extern under MWCC and a
//     host definition.
//
// `fn_29_90` is a vtable entry, not a free function: `.data:0xA24` - CIng's own vtable, 0x148
// bytes = 82 words, the first two being offset-to-top and the RTTI pointer, both zero in this
// REL - stores it at offset 0x3C, which calls slot 0x38, and `.data:0xA24` names that slot
// `HealthInfo__3CAiFv`. So the call below is a member call, and that spelling is measured rather
// than guessed: loading the vtable by hand - `void* const* vt = *(void* const* const*)self;` and
// calling `vt[14]` - compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one
// register is 99.09% on the function (measured in `CIngPuddleRel.cpp`, 2026-09-29).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out - two leading words then one word per virtual - so
// thirteen virtuals put the thirteenth at 0x38. The slots are named by position because no header
// here models a CActor/CPatterned virtual, and none of them is defined or called from this file,
// because the only object that carries this vtable is the module's own retail bytes. Slot 12
// returns float because the table names it `HealthInfo__3CAiFv`; the call discards it.
//
// **No dead-strip hazard, and that is measured**: `build/G2ME01/Ing/ldscript.lcf` lists all
// fourteen of `fn_29_0`..`fn_29_90` in its FORCEACTIVE block, and `.data:0xA24` stores every one
// of them (`fn_29_90` at offset 0x3C), so nothing here needs a `force_active:` entry in
// `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `RELMain` and `RELExit` are the
// module's own entry points and `fn_29_100` is called from `RELMain`, so that trio survives for
// the reason it does everywhere in this family.
//
// Everything from fn_29_130 (0x130, 0xFC8), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CIng/CActor/CPatterned/CAi hierarchy this tree does not
// model. The 250 functions above it are CIng's members and stay retail, so dtk fills them and the
// module's sha1 against `config/G2ME01/config.yml` still holds.
//
// Not in `files.cmake`, for the reason every other head in this family gives: the file defines
// RELMain/RELExit, which a flat host link cannot hold, and calls `fn_29_130` and `fn_80218918`,
// which the port cannot link, so `tools/link_check.sh --strict` would fail on a growing undefined
// count. The port keeps reading `Ing.rel` off the disc through `platform/rel.cpp`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

// This module's own `.rodata:0x64` (`.float 1`), not a DOL constant. Defined by dtk's `.rodata`
// object, which this unit's split does not claim, so the reference is an ordinary extern. Note
// that `REL/REL_Setup.cpp` claims `.rodata 0x838..0x8BC` only, so 0x64 is unclaimed.
extern "C" const float lbl_29_rodata_64;
// The one DOL float this head reads: `lbl_8041B758` is `.sdata2:0x8041B758` in
// `config/G2ME01/symbols.txt`.
extern "C" const float lbl_8041B758;

// The vtable slot fn_29_90 dispatches to; see the note above on the layout. None of these is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes.
class CIngDispatch {
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
CEntity* fn_29_130(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80218918(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_29_bss_6C;
#else
FScriptLoader lbl_29_bss_6C = 0;
#endif

// .text 0x100, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_29_100() {
  lbl_29_bss_6C = fn_29_130;
  fn_80218918(&lbl_29_bss_6C);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
#ifdef __MWERKS__
void RELMain() { fn_29_100(); }

void RELExit() { fn_80218918(nullptr); }
#else
void mp_relmain_ing() { fn_29_100(); }

void mp_relexit_ing() { fn_80218918(nullptr); }
#endif

// .text 0x90, 0x2C bytes. Vtable entry 0x3C of CIng; the call it makes targets vtable slot 0x38,
// which `.data:0xA24` names `HealthInfo__3CAiFv`. The class has no header here, so the object is
// reached as a `CIngDispatch*` - see the CIngPuddle section of `docs/research/raw_offsets.md` for
// why the hand-loaded vtable does not match.
void fn_29_90(CIngDispatch* self) { self->Slot12(); }

// .text 0x74, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_29_74(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x6C, 0x08 bytes. a predicate that is always true.
bool fn_29_6C(void*) { return true; }

// .text 0x64, 0x08 bytes. the address of the member at +0x754.
void* fn_29_64(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x58, 0x0C bytes. a constant float out of the DOL.
float fn_29_58(void*) { return lbl_8041B758; }

// .text 0x4C, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_29_4C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x3C, 0x10 bytes. resets the unique id to the invalid value.
void fn_29_3C(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x34, 0x08 bytes. a predicate that is always false.
bool fn_29_34(void*) { return false; }

// .text 0x2C, 0x08 bytes. a predicate that is always false.
bool fn_29_2C(void*) { return false; }

// .text 0x24, 0x08 bytes. the byte at +0x44f.
unsigned char fn_29_24(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x18, 0x0C bytes. this module's own `lbl_29_rodata_64`, `.float 1`.
float fn_29_18(void*) { return lbl_29_rodata_64; }

// .text 0x10, 0x08 bytes. a predicate that is always true.
bool fn_29_10(void*) { return true; }

// .text 0x8, 0x08 bytes. the address of the member at +0xac8.
void* fn_29_8(void* self) { return static_cast< char* >(self) + 0xAC8; }

// .text 0x0, 0x08 bytes. the address of the member at +0x9dc.
void* fn_29_0(void* self) { return static_cast< char* >(self) + 0x9DC; }
}
