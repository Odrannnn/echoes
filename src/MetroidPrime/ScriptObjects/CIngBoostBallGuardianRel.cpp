// CIngBoostBallGuardianRel.cpp - IngBoostBallGuardian's (module 30) head, .text 0x0..0x130: the
// seventeen functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp` and
// `MetroidPrime/ScriptObjects/CMetroidRel.cpp`, and the ranges come from
// `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt`:
//
//   0x000 fn_30_0   0x08  addi r3,r3,0xaec
//   0x008 fn_30_8   0x08  addi r3,r3,0xbd8
//   0x010 fn_30_10  0x08  li r3,1
//   0x018 fn_30_18  0x0C  lbl_30_rodata_64, this module's own .rodata:0x64
//   0x024 fn_30_24  0x08  lbz r3, 0x44f(r3)
//   0x02C fn_30_2C  0x08  li r3,0
//   0x034 fn_30_34  0x08  li r3,0
//   0x03C fn_30_3C  0x10  *self = kInvalidUniqueId
//   0x04C fn_30_4C  0x0C  byte at +0x34c, bit 3
//   0x058 fn_30_58  0x0C  lbl_8041B758
//   0x064 fn_30_64  0x08  addi r3,r3,0x754
//   0x06C fn_30_6C  0x08  li r3,1
//   0x074 fn_30_74  0x1C  three floats from self+0x54 -> *out
//   0x090 fn_30_90  0x2C  virtual dispatch, vtable slot 0x38
//   0x0BC RELExit   0x24  li r3,0 / bl fn_8022FFC4
//   0x0E0 RELMain   0x20  bl fn_30_100
//   0x100 fn_30_100 0x30  lbl_30_bss_6C = fn_30_130 ; fn_8022FFC4(&lbl_30_bss_6C)
//
// **The accessor block is IngSpaceJumpGuardian's with the family differences, and that is measured
// rather than read off the `fn_<id>_<off>` names.** Diffing dtk's `auto_00_00000000_text.s` over
// the 0x130 this claims against `CIngSpaceJumpGuardianRel.cpp`'s 0x170: the `0x44f` byte, the
// `kInvalidUniqueId` store, the `+0x34c` bit test, the `lbl_8041B758` accessor, the `+0x754`
// address, the three-float copy and the closing vtable-0x38 dispatch are all byte-identical to the
// bodies those files already reproduce at 100%, and the differences are the four below, each read
// off the disassembly rather than assumed from a sibling:
//   - the two leading eight-byte accessors are **both address accessors** - `addi r3,r3,0xaec`
//     and then `addi r3,r3,0xbd8`, one word apart - where IngSpaceJumpGuardian opens
//     `addi r3,r3,0x8d0` and `li r3,1` and Metroid opens `addi r3,r3,0x8c8` and `li r3,1`; so
//     this module covers two more members at the front than the rest of the family;
//   - there is **no** `GetBoundingBox` wrapper and **no** `lbl_8041AAB8` store at +0x448, so
//     `lbl_8041B758` is the only DOL global this head's relocations name;
//   - it runs **two** `li r3,0` predicates (`fn_30_2C`, `fn_30_34`) where the family runs two or
//     three, and two `li r3,1` (`fn_30_10`, `fn_30_6C`) where Metroid has one; and
//   - the module-local constant accessor is at 0x18 rather than 0x10, and it reads
//     `lbl_30_rodata_64` (`.rodata:0x64`, `.float 1`).
// So no spelling had to be discovered for anything except the ordering above, and every body
// below is the one `CIngSpaceJumpGuardianRel.cpp` and `CMetroidRel.cpp` already reproduce at
// 100%. The point is worth keeping, because `CMediumIngRel.cpp` records the opposite result for
// module 41: **a module head is not a sibling's head until the bytes say so.** The three-float
// copy must also stay spelled as subscript stores: `CMetareeSwarmRel.cpp` records that the
// built-in `CVector3f` spelling reverses the loads and the score falls.
//
// One accessor reads oddly in dtk's rendering, and it is dtk's rendering rather than the source's:
// `fn_30_4C` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
// `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
// opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
// `fn_30_0`, `fn_30_8` and `fn_30_64` return the *address* of a member, so they are `void*` and
// the offsets are literal.
//
// **No dead-strip hazard, and that is measured**: `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`
// puts all fourteen of `fn_30_0` .. `fn_30_90` in its FORCEACTIVE list, so nothing here needs a
// `force_active:` entry in `config/G2ME01/config.yml` (the trap `CGeomBlobV2` hit). `.data:0x9C0` -
// CIngBoostBallGuardian's own vtable, 0x148 bytes = two leading words (offset-to-top and the
// RTTI pointer, both zero in this REL) and then one word per virtual - stores `fn_30_90` at
// offset 0x3C, whose target slot 0x38 the same table names `HealthInfo__3CAiFv`, and stores
// eight of the other accessors besides. `RELMain` and `RELExit` are the module's own entry points
// and `fn_30_100` is called from `RELMain`, so that trio survives for the reason it does
// everywhere in this family.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022FFC4` is the DOL's 0x8022FFC4, two instructions,
//     `stw r3, gLoader_IngBoostBallGuardian@sda21(r0); blr`
//     (`build/G2ME01/asm/auto_03_8022FFC4_text.s`), and `config/G2ME01/symbols.txt:9963` already
//     gives it that name, so no `symbols.txt` rename and no DOL change. It is `extern "C"`: an
//     alias would be a different symbol and the call would resolve to nothing. So it stores the
//     *address* of a loader slot, not a loader - the same shape as IngSpaceJumpGuardian's
//     `fn_8021DC2C`.
//   - `lbl_30_bss_6C` is `.bss:0x6C`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **`lbl_30_bss_6C` and not `lbl_30_bss_0`** - this module's `.bss:0x0` is a
//     different 0x8-byte object read and written by `fn_30_130`'s code above the head - which is
//     why the name below is the one it is. This unit's split claims `.text` only, so dtk's
//     `.bss` object has to define it, and a second definition under MWCC is what produced
//     mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and a host
//     definition.
//
// Everything from fn_30_130 (0x130, 0x4B4), the module's own entity loader, is left unclaimed:
// behavioural class code that needs the CIngBoostBallGuardian/CActor/CPatterned/CAi hierarchy
// this tree does not model. The 301 functions above it are the module's methods and stay retail,
// and dtk fills them, so the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// **Nothing calls the host pair of RELMain/RELExit, and that is deliberate**, exactly as in
// `CMetroidRel.cpp` and `CIngSpaceJumpGuardianRel.cpp`: listing this file in `files.cmake` would
// make the port link `fn_30_130` and `fn_8022FFC4`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `IngBoostBallGuardian.rel` off the disc through `platform/rel.cpp`, and the `#else` branch
// exists only so the file is still a valid translation unit.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%, `unit_fit.sh` still saying "fits" and the link still
// succeeding. Only `tools/flip_test.sh` catches that.

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

// The one DOL float this head's relocations name: `fn_30_58`'s accessor. Both
// `fn_30_4C`'s flag byte and the family store are absent here, so nothing else needs declaring.
extern "C" const float lbl_8041B758;
// `kInvalidUniqueId` comes from MetroidPrime/TGameTypes.hpp, which declares it as the `TUniqueId`
// retail has, so `fn_30_3C` is spelled `*id = kInvalidUniqueId` on a `TUniqueId*`.
// .rodata:0x64 of this module: `.float 1`. See the note above; the split claims .text only, so
// this stays defined in dtk's `.rodata` object and the reference is an ordinary cross-object
// relocation, exactly as the DOL global beside it is.
extern "C" const float lbl_30_rodata_64;

// The vtable slot fn_30_90 dispatches to; see the note above on the layout.
class CIngBoostBallGuardianDispatch {
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
CEntity* fn_30_130(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_8022FFC4(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_30_bss_6C;
#else
FScriptLoader lbl_30_bss_6C = 0;
#endif

// .text 0x100, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_30_100() {
  lbl_30_bss_6C = fn_30_130;
  fn_8022FFC4(&lbl_30_bss_6C);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
#ifdef __MWERKS__
void RELMain() { fn_30_100(); }

void RELExit() { fn_8022FFC4(nullptr); }
#else
void mp_relmain_ingboostballguardian() { fn_30_100(); }

void mp_relexit_ingboostballguardian() { fn_8022FFC4(nullptr); }
#endif

// .text 0x90, 0x2C bytes. Vtable entry 0x3C of CIngBoostBallGuardian (`.data:0x9C0`, word 15);
// the call it makes targets slot 0x38, which the same table names `HealthInfo__3CAiFv`. The
// class has no header here, so the object is reached as a `CIngBoostBallGuardianDispatch*` - see
// the CIngPuddle section of `docs/research/raw_offsets.md` for why the hand-loaded vtable does
// not match. `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a
// class's virtuals out the way retail's vtable is laid out - two leading words (offset-to-top,
// then the RTTI pointer, both zero in this REL) and then one word per virtual - so thirteen
// virtuals put the thirteenth at 0x38. Slot 12's return type is float because the table names it
// `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes.
void fn_30_90(CIngBoostBallGuardianDispatch* self) { self->Slot12(); }

// .text 0x74, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_30_74(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x6C, 0x08 bytes. a predicate that is always true.
bool fn_30_6C(void*) { return true; }

// .text 0x64, 0x08 bytes. the address of the member at +0x754.
void* fn_30_64(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x58, 0x0C bytes. a constant float out of the DOL.
float fn_30_58(void*) { return lbl_8041B758; }

// .text 0x4C, 0x0C bytes. the flag at +0x34c, bit 3.
bool fn_30_4C(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x3C, 0x10 bytes. resets the unique id to the invalid value.
void fn_30_3C(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x34, 0x08 bytes. a predicate that is always false.
bool fn_30_34(void*) { return false; }

// .text 0x2C, 0x08 bytes. a predicate that is always false.
bool fn_30_2C(void*) { return false; }

// .text 0x24, 0x08 bytes. the byte at +0x44f.
unsigned char fn_30_24(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x18, 0x0C bytes. a constant float out of this module's own `.rodata`.
float fn_30_18(void*) { return lbl_30_rodata_64; }

// .text 0x10, 0x08 bytes. a predicate that is always true.
bool fn_30_10(void*) { return true; }

// .text 0x8, 0x08 bytes. the address of the member at +0xBD8.
void* fn_30_8(void* self) { return static_cast< char* >(self) + 0xBD8; }

// .text 0x0, 0x08 bytes. the address of the member at +0xAEC.
void* fn_30_0(void* self) { return static_cast< char* >(self) + 0xAEC; }
}
