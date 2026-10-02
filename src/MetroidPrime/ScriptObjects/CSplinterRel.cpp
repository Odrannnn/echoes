// CSplinterRel.cpp - Splinter's (module 74) head, .text 0x0..0x118: the ten accessors the REL
// loader generator emits, `fn_74_78`'s vtable call on slot 0x38, and RELExit, RELMain and the
// loader registration RELMain calls. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngRel.cpp` and
// `MetroidPrime/ScriptObjects/CMinorIngRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Splinter/symbols.txt`:
//
//   0x000 fn_74_0   0x08  addi r3,r3,0x7c4
//   0x008 fn_74_8   0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x018 fn_74_18  0x08  lbz r3, 0x44f(r3)
//   0x020 fn_74_20  0x08  li r3,0x0
//   0x028 fn_74_28  0x08  li r3,0x0
//   0x030 fn_74_30  0x10  *self = kInvalidUniqueId
//   0x040 fn_74_40  0x0C  byte at +0x34c, bit 3
//   0x04C fn_74_4C  0x08  addi r3,r3,0x754
//   0x054 fn_74_54  0x08  li r3,0x1
//   0x05C fn_74_5C  0x1C  three floats from self+0x54 -> *out
//   0x078 fn_74_78  0x2C  virtual dispatch, vtable slot 0x38
//   0x0A4 RELExit   0x24  li r3,0 / bl fn_80218C64
//   0x0C8 RELMain   0x20  bl fn_74_E8
//   0x0E8 fn_74_E8  0x30  lbl_74_bss_70 = fn_74_118 ; fn_80218C64(&lbl_74_bss_70)
//
// **The claim starts at 0x0**, like `CIngRel.cpp`'s and `CMinorIngRel.cpp`'s: Splinter's `.text`
// opens directly on the accessor block with no destructor chain below it, so dtk's
// `auto_00_00000000_text` splits once, into ours at `0x0..0x118` and retail's at `0x118..0xD4E4`.
// **This is the shortest head in the family so far: fourteen functions, where Ing's is seventeen
// and AtomicAlpha's eighteen.** That is measured off
// `build/G2ME01/Splinter/asm/auto_00_00000000_text.s` over `0x0..0x118`, not inferred from the
// `fn_<id>_<off>` names, which say nothing about which function is which. Three families of
// accessor are missing here and each is absent for a reason the block states:
//   - **no `optional_object<CAABox>` wrapper.** Splinter is not a `CPhysicsActor` in the shape the
//     wrapper needs, and no `CAABox` stand-in is needed at all because nothing in this unit names
//     one - which is why `Kyoto/Math/CAABox.hpp` is not included.
//   - **no `lbl_8041B758` accessor**, so the 0x0C slot at 0x40 is the `+0x34c` bit read and the
//     next function is the `+0x754` member address at 0x4C. Ing spends that slot on a
//     module-local `.rodata` constant instead, and MinorIng on the DOL one; Splinter has
//     neither, which is why `extern "C" const float lbl_8041B758;` appears in neither.
//   - **one leading member-address accessor** (`+0x7c4` at 0x0) where Ing, IngBoostBallGuardian
//     and AtomicAlpha each open with two, so Splinter's float store is the *second* function at
//     0x8 rather than the third.
//
// Two bodies read oddly and both are dtk's rendering rather than the source's, so they are
// spelled to match the bytes and not "improved":
//   - `fn_74_40` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`, the same word Ing's `fn_29_4C` is.
//     dtk prints the middle word `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and
//     therefore always 0; the word is opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left
//     by 32-3 masked to one bit - bit 3. The `& 8` below is that same encoding and that same bit.
//   - `fn_74_30` reloads `kInvalidUniqueId` with `lhz` and stores it with `sth` rather than
//     materialising the constant, which is what reading the DOL `.sbss` word through a
//     `TUniqueId*` gives.
//
// `fn_74_5C` interleaves its loads and its stores (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 /
// stfs f0,4 / lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores rather than as
// a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in spelling reverses the
// loads and the score falls.
//
// **The record is four bytes, and the `.bss` dump settles where it is without a choice.** The
// registration stores into `lbl_74_bss_70` at `.bss:0x70` (the name is in the disassembly, so no
// reading of the dump is needed to find it), and `lbl_74_bss_70` is `size:0x8` in
// `config/G2ME01/rels/Splinter/symbols.txt` where the family's slots are usually `size:0x4` -
// so, per the rule `RUNNING_THE_DECOMP.md` states for exactly this case, the DOL was grepped for
// every reader of `gLoader_Splinter` before the shape was decided. There is **one** reader,
// `LoadSplinter__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x80218C38, and it reads
// **word 0 only** (`lwz r6, gLoader_Splinter; lwz r12, 0x0(r6); mtctr r12; bctrl`) - no second
// reader, no `__ptmf_scall`, no pointer-to-member-function. The registration likewise stores one
// word (`stwu r0, lbl_74_bss_70@l(r3)`), not three copied out of `.data` the way
// `CSnakeWeedSwarmRel.cpp`'s and `CSplitterRelMain.cpp`'s do. So the record is a plain
// `FScriptLoader` and the 0x8 is retail's 8-byte slot for it, not a bigger record: the unit's
// split claims `.text` only, so dtk's `.bss` object supplies all 8 bytes and the declaration
// below is `extern` under MWCC with a host definition.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218C64` is the DOL's 0x80218C64, two instructions,
//     `stw r3, gLoader_Splinter@sda21(r0); blr` (`build/G2ME01/asm/auto_03_80218C64_text.s`),
//     immediately after `LoadSplinter__FR13CStateManagerR12CInputStreamR11CEntityInfo` at
//     0x80218C38, which is 0x2C bytes and so ends exactly at 0x80218C64. So it stores the
//     *address* of the record, not a loader - which is why the store below hands it
//     `&lbl_74_bss_70`. The import name is the plain `fn_80218C64` that
//     `config/G2ME01/symbols.txt` already gives it - not the long MWCC-mangled `SetLoader_...`
//     form `CAtomicAlphaRel.cpp` has to spell out for its own module - so no `symbols.txt` rename
//     is needed and the DOL is untouched. It is `extern "C"`: an alias would be a different symbol
//     and the call would resolve to nothing.
//   - `fn_74_118` is the module's entity loader, referenced only by address in the registration.
//     Its 0x724 bytes and its `mr r19, r3 / mr r24, r4 / mr r18, r5` prologue are the loader
//     signature `(CStateManager&, CInputStream&, CEntityInfo&)` that `LoadSplinter` calls
//     through the record; it is left unclaimed, and the range above it too.
//
// `fn_74_78` is a vtable entry, not a free function: `.data:0x854` - this module's own 0x26C-byte
// CPatterned vtable - stores it at offset 0x3C, which calls slot 0x38, and `.data:0x854` names
// that slot `HealthInfo__3CAiFv`. So the call below is a member call, and that spelling is
// measured rather than guessed: loading the vtable by hand -
// `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` - compiles to
// `lwz r3, 0(r3)` where retail has `lwz r12, 0(r3)`, and that one register is 99.09% on the
// function (measured in `CIngPuddleRel.cpp`, 2026-09-29, where the same bytes are `fn_32_8`).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out: two leading words (offset-to-top, then the RTTI
// pointer, both zero in this REL) and then one word per virtual. Thirteen virtuals therefore put
// the thirteenth at 0x38, and calling it gives retail's seven instructions byte for byte. The
// slots are named by position because no header here models a CPatterned virtual; none of them is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes. Slot 12's return type is float because `.data:0x854` names it
// `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes, but naming it
// anything else would misdescribe the vtable.
//
// **No dead-strip hazard, and no `force_active:` entry is needed.** This is measured off the
// module's own `build/G2ME01/Splinter/ldscript.lcf`, which lists all eleven of `fn_74_0`..
//`fn_74_78` in its FORCEACTIVE block, and `lbl_74_data_854` stores all eleven as vtable entries
// (`fn_74_78` at 0x3C, then `fn_74_8`/`fn_74_54`/`fn_74_5C`, then `fn_74_18`, then
// `fn_74_0`/`fn_74_20`, then `fn_74_28`, then `fn_74_30`/`fn_74_40`/`fn_74_4C` - see
// `build/G2ME01/Splinter/asm/auto_04_00000000_data.s`), so each one is referenced twice over.
// `RELExit` and `RELMain` are the module's own entry points, referenced by the `_epilog`/`_prolog`
// the shared `REL/REL_Setup.cpp` unit defines, and `fn_74_E8` is called from `RELMain`, so the
// trio survives for the reason it does everywhere in this family.
//
// Everything from `fn_74_118` (0x118, 0x724) up is left unclaimed: that is the module's own
// behavioural class code - CSplinter's methods and the CActor/CPatterned/CAi hierarchy this tree
// does not model - so dtk fills it from retail and the module's sha1 against
// `config/G2ME01/config.yml` still holds. `tools/audit_rel_claim.py Splinter` counts the split.
//
// Not in `files.cmake`, for the reason every other head in this family gives: the file defines
// RELMain/RELExit, which a flat host link cannot hold, and calls `fn_74_118` and `fn_80218C64`,
// which the port cannot link, so `tools/link_check.sh --strict` would fail on a growing undefined
// count. The port keeps reading `Splinter.rel` off the disc through `platform/rel.cpp`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

// The one DOL float this head reads: `skDamageHitTime__10CPatterned` is `.sdata2:0x8041AAB8` in
// `config/G2ME01/symbols.txt`. There is no `lbl_8041B758` here - see the note above.
extern "C" const float skDamageHitTime__10CPatterned;
// `kInvalidUniqueId` is declared by the `MetroidPrime/TGameTypes.hpp` include above, as
// `const TUniqueId` - not redeclared here.

// The vtable slot fn_74_78 dispatches to; see the note above on the layout. None of these is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes.
class CSplinterDispatch {
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
// .text 0x118, unclaimed: the module's own entity loader.
CEntity* fn_74_118(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80218C64(FScriptLoader* loader);

// `lbl_74_bss_70` is the module's own copy of the record, in `.bss` and not claimed by this unit,
// so it is extern here: dtk's `.bss` object defines it, and a second definition under MWCC is what
// produced mwldeppc's internal linker error on ScriptPlayerProxy.
#ifdef __MWERKS__
extern FScriptLoader lbl_74_bss_70;
#else
FScriptLoader lbl_74_bss_70 = 0;
#endif

// .text 0xE8, 0x30 bytes: the module's loader registration. Retail loads the loader out of `.text`
// and the slot address out of `.bss`, and stores the loader with `stwu` so the store writes the
// slot itself and leaves r3 holding its address for the setter call.
void fn_74_E8() {
  lbl_74_bss_70 = fn_74_118;
  fn_80218C64(&lbl_74_bss_70);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is the
// retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CIngRel.cpp` and
// `CMinorIngRel.cpp`: listing this file in `files.cmake` would make the port link `fn_74_118` and
// `fn_80218C64`, which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined
// count. So the port keeps reading `Splinter.rel` off the disc through `platform/rel.cpp`, and the
// `#else` branch exists only so the file is still a valid translation unit for
// `tools/probe_sources.sh`.
#ifdef __MWERKS__
void RELMain() { fn_74_E8(); }

void RELExit() { fn_80218C64(nullptr); }
#else
void mp_relmain_splinter() { fn_74_E8(); }

void mp_relexit_splinter() { fn_80218C64(nullptr); }
#endif

// .text 0x78, 0x2C bytes. Vtable entry 0x3C of CSplinter; the call it makes targets vtable slot
// 0x38, which `.data:0x854` names `HealthInfo__3CAiFv`. The class has no header here, so the
// object is reached as a `CSplinterDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_74_78(CSplinterDispatch* self) { self->Slot12(); }

// .text 0x5C, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_74_5C(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x54, 0x08 bytes. a predicate that is always true.
bool fn_74_54(void*) { return true; }

// .text 0x4C, 0x08 bytes. the address of the member at +0x754.
void* fn_74_4C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x40, 0x0c bytes. the flag at +0x34c - dtk's `extrwi r3, r0, 1, 28` is
// `rlwinm r3, r0, 29, 31, 31`, the third `bool : 1` of its byte.
bool fn_74_40(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x30, 0x10 bytes. resets the unique id to the invalid value.
void fn_74_30(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x28, 0x08 bytes. a predicate that is always false.
bool fn_74_28(void*) { return false; }

// .text 0x20, 0x08 bytes. a predicate that is always false.
bool fn_74_20(void*) { return false; }

// .text 0x18, 0x08 bytes. the byte at +0x44f.
unsigned char fn_74_18(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x08, 0x10 bytes. stores the default float at +0x448.
void fn_74_8(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x00, 0x08 bytes. the address of the member at +0x7c4.
void* fn_74_0(void* self) { return static_cast< char* >(self) + 0x7c4; }
}
