// CAtomicAlphaRel.cpp - AtomicAlpha's (module 2) head, .text 0x0..0x13C: the eighteen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/AtomicAlpha/symbols.txt`:
//
//   0x00  fn_2_0   0x08   addi r3,r3,0x8c8 / blr
//   0x08  fn_2_8   0x08   addi r3,r3,0x7d8 / blr
//   0x10  fn_2_10  0x10   lbl_8041AAB8 -> *((float*)(self + 0x448))
//   0x20  fn_2_20  0x08   lbz r3, 0x44f(r3)
//   0x28  fn_2_28  0x08   li r3,0
//   0x30  fn_2_30  0x08   li r3,0
//   0x38  fn_2_38  0x10   *self = kInvalidUniqueId
//   0x48  fn_2_48  0x0C   byte at +0x34c, bit 3
//   0x54  fn_2_54  0x0C   lbl_8041B758
//   0x60  fn_2_60  0x08   addi r3,r3,0x754
//   0x68  fn_2_68  0x08   li r3,1
//   0x70  fn_2_70  0x08   li r3,0
//   0x78  fn_2_78  0x08   li r3,0
//   0x80  fn_2_80  0x1C   three floats from self+0x54 -> *out
//   0x9C  fn_2_9C  0x2C   virtual dispatch, vtable slot 0x38
//   0xC8  RELExit  0x24   li r3,0 / bl SetLoader_AtomicAlpha
//   0xEC  RELMain  0x20   bl fn_2_10C
//   0x10C fn_2_10C 0x30   lbl_2_bss_0 = fn_2_13C ; SetLoader_AtomicAlpha(&lbl_2_bss_0)
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_2_13C (0x13C, 0x420), the module's own entity loader: behavioural class code, and it needs
// the CActor/CPatterned hierarchy this tree does not model.
//
// **Twelve of the fourteen accessors are the bodies
// `src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp` already reproduces at 100% as a
// `Matching` unit - but the two blocks are NOT byte for byte identical, and it is worth being
// exact about why.** AtomicBeta's `.text 0x0..0x9C` and this module's `.text 0x0..0x9C` are both
// fourteen accessors reading the same three DOL relocations (`lbl_8041AAB8`, `kInvalidUniqueId`,
// `lbl_8041B758` - all three in the DOL, which is why one body serves every module), in the same
// store/load/predicate/flag/float/member/vector-copy order. The difference is at the front: this
// module's two *leading* accessors are extra, `fn_2_0` at +0x8C8 and `fn_2_8` at +0x7D8, where
// AtomicBeta opens with the float store at 0x0 - and in exchange this module carries two fewer
// `li r3,0; blr` predicates after the byte read (0x28/0x30 are ours; AtomicBeta's 0x18 and 0x20
// have no counterpart here). So the number that survives checking against the bytes is twelve of
// fourteen, and the reusable advice is "read AtomicBetaAccessors.cpp and diff", not "copy it".
// Even so no spelling had to be discovered: each body below is the one AtomicBetaAccessors.cpp or
// CIngPuddleRel.cpp carries and each was measured there. Two of them read oddly and both are
// dtk's rendering, not the source's:
//
//   - `fn_2_48` is `lbz r0, 0x34c(r3) / 54 03 EF FE / blr`. dtk prints the middle word
//     `extrwi r3, r0, 1, 28`, which would be bit 28 of a byte and therefore always 0; the word is
//     opcode 21, i.e. `rlwinm r3, r0, 29, 31, 31`, a rotate left by 32-3 masked to one bit - bit 3.
//     AtomicBetaAccessors.cpp's `& 8` is the same encoding and the same bit.
//   - `fn_2_80` loads and stores *interleaved* (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4
//     / lfs f0,0x5c / stfs f0,8`), so it is written as three subscript stores. Do not "improve" it
//     into a `CVector3f` copy: `CMetareeSwarmRel.cpp` records that the built-in spelling reverses
//     the loads and the score falls.
//
// **fn_2_0, fn_2_8 and fn_2_9C are vtable entries, not free functions.** dtk puts all three in the
// module's FORCEACTIVE list and `.data:0xC4` - AtomicAlpha's own vtable - stores them, so they are
// kept by the link and none of them is a dead-stripping hazard. `fn_2_0` and `fn_2_8` hand back a
// member 0x8C8 / 0x7D8 bytes into the object; `fn_2_9C` calls whatever sits in vtable slot 0x38,
// which `.data:0xC4` names as `HealthInfo__3CAiFv` and which for this object is retail's own.
//
// So the call below is a member call, and that spelling is measured rather than guessed. Loading
// the vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on
// the function (measured in `CIngPuddleRel.cpp`, 2026-09-29, where the same bytes are `fn_32_8`).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals
// out the way retail's vtable is laid out: two leading words (offset-to-top, then the RTTI pointer,
// both zero in this REL) and then one word per virtual. Thirteen virtuals therefore put the last
// one at 0x38, and calling it gives retail's seven instructions byte for byte. The slots are named
// by position because no header here models a CPatterned virtual; none of them is defined or
// called from this file, because the only object that carries this vtable is the module's own
// retail bytes. Slot 12's return type is float because `.data:0xC4` names it
// `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes, but naming it
// anything else would misdescribe the vtable.
//
// The two callees are named by what they are, not invented:
//   - `SetLoader_AtomicAlpha` is the DOL's 0x8021BB9C, two instructions,
//     `stw r3, gLoader_AtomicAlpha; blr`. So it stores the *address* of a loader slot, not a loader.
//     `LoadAtomicAlpha` at 0x8021BB74 reads it as
//     `lwz r6, gLoader_AtomicAlpha; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the store
//     below hands it `&lbl_2_bss_0` and why that slot is four bytes wide. Its body already lives
//     in `src/MetroidPrime/ScriptLoaderRel.cpp` (42/42, `SetLoader_AtomicAlpha` at 100.00%), so it
//     is declared here in C++ so `mwcceppc` mangles it to the retail name, exactly as
//     `CScriptPlayerProxy.cpp` does for `SetLoader_PlayerController`.
//   - `lbl_2_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" const float lbl_8041AAB8;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// `SetLoader_AtomicAlpha` is a C++ function, not a C one, and that is the whole point of the
// declaration: `mwcceppc` mangles it to
// `SetLoader_AtomicAlpha__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`, the name
// `config/G2ME01/symbols.txt` gives the DOL's 0x8021BB9C, and the same trick
// `CScriptPlayerProxy.cpp` uses on `SetLoader_PlayerController`. The three modules whose setter
// is still an unnamed `fn_80xxxxxx` in the DOL had to declare that name instead; do not copy that
// spelling here, because an alias is a different symbol and the call would resolve to nothing.
void SetLoader_AtomicAlpha(FScriptLoader* loader);

// The vtable slot fn_2_9C dispatches to; see the note above on the layout.
class CAtomicAlphaDispatch {
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
CEntity* fn_2_13C(CStateManager&, CInputStream&, const CEntityInfo&);

#ifdef __MWERKS__
extern FScriptLoader lbl_2_bss_0;
#else
FScriptLoader lbl_2_bss_0 = 0;
#endif

void fn_2_10C() {
  lbl_2_bss_0 = fn_2_13C;
  SetLoader_AtomicAlpha(&lbl_2_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp` and `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_2_13C`, which it cannot, and `tools/link_check.sh --strict` fails on a growing
// undefined count. So the port keeps reading `AtomicAlpha.rel` off the disc through
// `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid translation
// unit.
#ifdef __MWERKS__
void RELMain() { fn_2_10C(); }

void RELExit() { SetLoader_AtomicAlpha(nullptr); }
#else
void mp_relmain_atomicalpha() { fn_2_10C(); }

void mp_relexit_atomicalpha() { SetLoader_AtomicAlpha(nullptr); }
#endif

// .text 0x9C, 0x2C bytes. Vtable entry 0x3C of AtomicAlpha; the call it makes targets vtable slot
// 0x38, which `.data:0xC4` names `HealthInfo__3CAiFv`. The class has no header here, so the object
// is reached as a `CAtomicAlphaDispatch*` - see the CIngPuddle section of
// `docs/research/raw_offsets.md` for why the hand-loaded vtable does not match.
void fn_2_9C(CAtomicAlphaDispatch* self) { self->Slot12(); }

// .text 0x80, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_2_80(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x78, 0x08 bytes. a predicate that is always false.
bool fn_2_78(void*) { return false; }

// .text 0x70, 0x08 bytes. a predicate that is always false.
bool fn_2_70(void*) { return false; }

// .text 0x68, 0x08 bytes. a predicate that is always true.
bool fn_2_68(void*) { return true; }

// .text 0x60, 0x08 bytes. the address of the member at +0x754.
void* fn_2_60(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x54, 0x0c bytes. a constant float out of the DOL.
float fn_2_54(void*) { return lbl_8041B758; }

// .text 0x48, 0x0c bytes. the flag at +0x34c - dtk's `extrwi r3, r0, 1, 28` is
// `rlwinm r3, r0, 29, 31, 31`, the third `bool : 1` of its byte.
bool fn_2_48(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x38, 0x10 bytes. resets the unique id to the invalid value.
void fn_2_38(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x30, 0x08 bytes. a predicate that is always false.
bool fn_2_30(void*) { return false; }

// .text 0x28, 0x08 bytes. a predicate that is always false.
bool fn_2_28(void*) { return false; }

// .text 0x20, 0x08 bytes. the byte at +0x44f.
unsigned char fn_2_20(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x10, 0x10 bytes. stores the default float at +0x448.
void fn_2_10(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}

// .text 0x08, 0x08 bytes. the address of the member at +0x7D8.
void* fn_2_8(void* self) { return static_cast< char* >(self) + 0x7D8; }

// .text 0x00, 0x08 bytes. the address of the member at +0x8C8.
void* fn_2_0(void* self) { return static_cast< char* >(self) + 0x8C8; }
}
