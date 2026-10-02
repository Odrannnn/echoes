// CMinorIngRel.cpp - MinorIng's (module 44) head, .text 0x0..0x110: the twelve accessors the
// REL loader generator emits, plus RELExit, RELMain and the loader registration RELMain calls.
// Same arrangement as `MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` and
// `MetroidPrime/ScriptObjects/CChozoGhostRel.cpp`, and the ranges come from
// `config/G2ME01/rels/MinorIng/symbols.txt`:
//
//   0x000 fn_44_0   0x08  addi r3,r3,0x960
//   0x008 fn_44_8   0x08  addi r3,r3,0xa4c
//   0x010 fn_44_10  0x08  li r3,0x1
//   0x018 fn_44_18  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x028 fn_44_28  0x08  lbz r3, 0x44f(r3)
//   0x030 fn_44_30  0x08  li r3,0x0
//   0x038 fn_44_38  0x10  *self = kInvalidUniqueId
//   0x048 fn_44_48  0x0C  byte at +0x34c, bit 3
//   0x054 fn_44_54  0x0C  lbl_8041B758
//   0x060 fn_44_60  0x08  addi r3,r3,0x754
//   0x068 fn_44_68  0x08  li r3,0x1
//   0x070 fn_44_70  0x2C  virtual dispatch, vtable slot 0x38
//   0x09C RELExit   0x24  li r3,0 / bl fn_80218AA0
//   0x0C0 RELMain   0x20  bl fn_44_E0
//   0x0E0 fn_44_E0  0x30  lbl_44_bss_84 = fn_44_110 ; fn_80218AA0(&lbl_44_bss_84)
//
// **The claim starts at 0x0**, unlike `CChozoGhostRel.cpp`'s, which starts at 0x350 because the
// destructor chain below its accessors needs the actor hierarchy this tree does not model. MinorIng
// has no such range: its `.text` opens directly on the accessor block, so the head is the first
// fourteen functions of the module and dtk's `auto_00_00000000_text` splits once, into ours at
// `0x0..0x110` and retail's at `0x110..0xB484`. `Metaree`, `AtomicAlpha`, `Rezbit` and the rest
// already do this; a claim beginning at the module's first function is the ordinary arrangement.
//
// **The block is AtomicAlpha's with four measured differences, and copying
// `CAtomicAlphaRel.cpp` without diffing gets two of them wrong.** Read against
// `build/G2ME01/MinorIng/asm/auto_00_00000000_text.s` over `0x0..0x9C` and
// `CAtomicAlphaRel.cpp` over `0x0..0x9C`: this module's two leading accessors are
// `addi r3,r3,0x960` and `addi r3,r3,0xa4c` where AtomicAlpha's are `0x8C8` and `0x7D8`; this
// module's third function is `li r3,1` where AtomicAlpha's third is the float store at 0x448
// (AtomicBeta opens on the float store, so the order is not even fixed inside the family); and
// after the byte read this module runs **one** `li r3,0` predicate where AtomicAlpha runs two. It
// also has **no** `li r3,0` pair at 0x70/0x78, **no** interleaved three-float copy and **no**
// `optional_object<CAABox>` wrapper, so the head is fifteen functions and stops at 0x110 rather
// than AtomicAlpha's 0x13C. Every body below is nevertheless one `CAtomicAlphaRel.cpp` already
// carries, and every one was measured there - no spelling had to be found for this run.
//
// Two of those bodies read oddly and both are dtk's rendering, not the source's, so they are
// spelled to match the bytes and not "improved":
//   - `fn_44_48` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`, the same word AtomicAlpha's `fn_2_48`
//     is. dtk prints it `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always
//     0; the word is opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to
//     one bit - bit 3. The `& 8` below is that same encoding and that same bit.
//   - `fn_44_38` reloads `kInvalidUniqueId` with `lhz` and stores it with `sth` rather than
//     materialising the constant, which is what reading the DOL `.sbss` word through a
//     `const unsigned short*` gives.
//
// **No dead-strip hazard, and no `force_active:` entry is needed.** `build/G2ME01/MinorIng/ldscript.lcf`
// already lists all twelve of `fn_44_0` .. `fn_44_70` in its FORCEACTIVE block, and
// `lbl_44_data_620` - this module's own 0x148-byte CPatterned vtable at `.data:0x620` - stores all
// twelve as entries, so each one is referenced twice over (see
// `build/G2ME01/MinorIng/asm/auto_04_00000000_data.s`: `fn_44_18` and `fn_44_68` and `fn_44_10` at
// entries 0x24/0x28/0x2C, `fn_44_70` at 0x3C, `fn_44_28` at 0x50, `fn_44_0` and `fn_44_8` at
// 0x68/0x6C, then `fn_44_30`/`fn_44_38`/`fn_44_48`/`fn_44_54` and `fn_44_60` at 0x74..0x80). That
// also identifies the block without relying on the `fn_<id>_<off>` names, which say nothing about
// which function is which. `RELMain` and `RELExit` are the module's own entry points and `fn_44_E0`
// is called from `RELMain`, so the trio survives for the reason it does everywhere in this family.
//
// `fn_44_70` is vtable entry 0x3C and the call it makes targets slot 0x38, which `.data:0x620`
// names `HealthInfo__3CAiFv`. So the call below is a member call, and that spelling is measured
// rather than guessed: loading the vtable by hand - `void* const* vt = *(void* const* const*)self;`
// and calling `vt[14]` - compiles to `lwz r3, 0(r3)` where retail has `lwz r12, 0(r3)`, and that
// one register is 99.09% on the function (measured in `CIngPuddleRel.cpp`, 2026-09-29, where the
// same bytes are `fn_32_8`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and
// it lays a class's virtuals out the way retail's vtable is laid out: two leading words
// (offset-to-top, then the RTTI pointer, both zero in this REL) and then one word per virtual.
// Thirteen virtuals therefore put the last one at 0x38, and calling it gives retail's seven
// instructions byte for byte. The slots are named by position because no header here models a
// CPatterned virtual; none of them is defined or called from this file, because the only object
// that carries this vtable is the module's own retail bytes. Slot 12's return type is float because
// `.data:0x620` names it `HealthInfo__3CAiFv`; the call discards it, so this does not affect the
// bytes, but naming it anything else would misdescribe the vtable.
//
// The three callees are named by what they are, not invented:
//   - `fn_80218AA0` is the DOL's 0x80218AA0, two instructions,
//     `stw r3, gLoader_MinorIng@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218AA0_text.s`),
//     immediately after `LoadMinorIng__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
//     0x80218A74, which is 44 bytes and so ends exactly at 0x80218AA0. So it stores the *address*
//     of a loader slot, not a loader. `src/MetroidPrime/ScriptLoader/MinorIng.cpp` - a `Matching`
//     unit, and the file that already records why this setter is deliberately not claimed in the
//     DOL - reads the slot as `(*gLoader_MinorIng.value)(mgr, input, info)`, which is why the store
//     below hands it `&lbl_44_bss_84` and why that slot is four bytes wide. **The import name is
//     the plain `fn_80218AA0`** that `config/G2ME01/symbols.txt` already gives it - not the long
//     MWCC-mangled `SetLoader_...` form `CAtomicAlphaRel.cpp` has to spell out for its own module -
//     so no `symbols.txt` rename is needed and the DOL is untouched. It is `extern "C"`: an alias
//     would be a different symbol and the call would resolve to nothing.
//   - `lbl_44_bss_84` is `.bss:0x84`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer, and it is **`lbl_44_bss_84` and not `lbl_44_bss_0`** - this module's `.bss` holds
//     nine objects and `.bss:0x0` is a 0x18-byte one read and written by `fn_44_110`'s code far
//     above the head (see `build/G2ME01/MinorIng/asm/auto_05_00000000_bss.s`), which is why the
//     name below is the one it is. `lbl_44_bss_84` is also the module's only `data:4byte` slot,
//     which is what marks it as the one `fn_44_E0` stores through
//     (`lis r3, lbl_44_bss_84@ha; ... stwu r0, lbl_44_bss_84@l(r3)`). This unit's split claims
//     `.text` only, so dtk's `.bss` object has to define it, and a second definition under MWCC is
//     what produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC
//     and a host definition.
//   - `fn_44_110` is the module's entity loader, referenced only by address in the registration.
//     Its 0x844 bytes and its `mr r23, r3 / mr r24, r4 / mr r25, r5` prologue are the loader
//     signature `(CStateManager&, CInputStream&, const CEntityInfo&)` that `LoadMinorIng` calls
//     through the slot; it is left unclaimed, and the range above it too.
//
// Everything from `fn_44_110` (0x110, 0x844) up is left unclaimed: that is the module's own
// behavioural class code - CMinorIng's methods and the CActor/CPatterned/CAi hierarchy this tree
// does not model - so dtk fills it from retail and the module's sha1 against
// `config/G2ME01/config.yml` still holds. The module has 217 text symbols in all, which is what
// `tools/audit_rel_claim.py MinorIng` counts: 15 ours, 5 `REL_Setup`, 2
// `global_destructor_chain` and 195 unclaimed.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// The vtable slot fn_44_70 dispatches to; see the note above on the layout.
class CMinorIngDispatch {
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
// .text 0x110, unclaimed: the module's own entity loader.
CEntity* fn_44_110(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218AA0(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_44_bss_84;
#else
FScriptLoader lbl_44_bss_84 = 0;
#endif

// .text 0xE0, 0x30 bytes: the module's loader registration. Retail loads the loader out of `.text`
// and the slot address out of `.bss`, and stores the loader with `stwu` so the store writes the slot
// itself and leaves r3 holding its address for the setter call.
void fn_44_E0() {
  lbl_44_bss_84 = fn_44_110;
  fn_80218AA0(&lbl_44_bss_84);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is the
// retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CAtomicAlphaRel.cpp` and
// `CChozoGhostRel.cpp`: listing this file in `files.cmake` would make the port link `fn_44_110` and
// `fn_80218AA0`, which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined
// count. So the port keeps reading `MinorIng.rel` off the disc through `platform/rel.cpp`, and the
// `#else` branch exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_44_E0(); }

void RELExit() { fn_80218AA0(nullptr); }
#else
void mp_relmain_minoring() { fn_44_E0(); }

void mp_relexit_minoring() { fn_80218AA0(nullptr); }
#endif

// .text 0x70, 0x2C bytes. Vtable entry 0x3C of CMinorIng; the call it makes targets vtable slot
// 0x38, which `.data:0x620` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CMinorIngDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_44_70(CMinorIngDispatch* self) { self->Slot12(); }

// .text 0x68, 0x08 bytes. a predicate that is always true.
bool fn_44_68(void*) { return true; }

// .text 0x60, 0x08 bytes. the address of the member at +0x754.
void* fn_44_60(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x54, 0x0c bytes. a constant float out of the DOL.
float fn_44_54(void*) { return lbl_8041B758; }

// .text 0x48, 0x0c bytes. the flag at +0x34c - dtk's `extrwi r3, r0, 1, 28` is
// `rlwinm r3, r0, 29, 31, 31`, the third `bool : 1` of its byte.
bool fn_44_48(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x38, 0x10 bytes. resets the unique id to the invalid value.
void fn_44_38(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x30, 0x08 bytes. a predicate that is always false.
bool fn_44_30(void*) { return false; }

// .text 0x28, 0x08 bytes. the byte at +0x44f.
unsigned char fn_44_28(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x18, 0x10 bytes. stores the default float at +0x448.
void fn_44_18(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x10, 0x08 bytes. a predicate that is always true.
bool fn_44_10(void*) { return true; }

// .text 0x08, 0x08 bytes. the address of the member at +0xA4C.
void* fn_44_8(void* self) { return static_cast< char* >(self) + 0xA4C; }

// .text 0x00, 0x08 bytes. the address of the member at +0x960.
void* fn_44_0(void* self) { return static_cast< char* >(self) + 0x960; }
}
