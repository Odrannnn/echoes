// CGrenchler8F34.cpp - Grenchler's (module 27) .text 0x8F34..0x9018: five of CGrenchler's class
// methods, four read straight off the module's vtables and one destroyed in place.
//
//   0x8F34 fn_27_8F34  0x5C  the deleting destructor of a class whose `rstl::string` at +0x2C is
//                            the only non-trivial member: `~basic_string()`, then
//                            `Free__7CMemoryFPCv(self)` for a positive flag
//   0x8F90 fn_27_8F90  0x0C  the bit at +0x90C that MWCC packs second in the byte (0x40)
//   0x8F9C fn_27_8F9C  0x18  the bit at +0x420 that it packs first (0x80), returned as a `bool`
//   0x8FB4 fn_27_8FB4  0x0C  the bit at +0xA24 that it packs fourth (0x10)
//   0x8FC0 fn_27_8FC0  0x58  `CPatterned::GetDamageVulnerability`'s result handed to the module's
//                            own `fn_27_990C`, then the bit at +0xA24 that packs first is cleared
//
// The five are contiguous - `config/G2ME01/rels/Grenchler/symbols.txt:171-176` gives 0x8F34 size
// 0x5C, 0x8F90 size 0xC, 0x8F9C size 0x18, 0x8FB4 size 0xC and 0x8FC0 size 0x58, so
// 0x8F34 + 0x5C = 0x8F90, + 0xC = 0x8F9C, + 0x18 = 0x8FB4, + 0xC = 0x8FC0, + 0x58 = 0x9018 is
// `fn_27_9018` - so the claim covers them and nothing else. Everything around them, including the
// rest of dtk's `auto_00_00000168_text`, stays unclaimed and dtk fills it from retail, which is
// what keeps the module's sha1 at `config/G2ME01/config.yml`'s
// `de128c91f467b5b61b4de4a2ed6b217044a975c5`.
//
// **The bitfield byte is real and it is why these functions are not a mask test each.** All four
// predicaies read one bit of a byte and every one of them compiles to `lbz` + `extrwi` (+ the
// `bool` conversion's `neg`/`or`/`srwi` in `fn_27_8F9C`), not to `lbz` + `andi.`. `fn_27_8FB4`
// reads bit 27 of the loaded word (0x10 of the byte) and `fn_27_8FC0` *clears* bit 24 (0x80) with
// `li r3,0` + `rlwimi r0,r3,7,24,24` + `stb` - the shape MWCC emits for a one-bit field store, and
// the same shape `fn_27_1A2C` has at +0x90C. So the byte is modelled as `SGrenchlerBits`, eight
// one-bit fields in declaration order = `mBit0` at 0x80 down to `mBit7` at 0x01.
//
// **Three of the five are named by the module's vtables, measured rather than guessed.**
// `tools/rel_class_map.py Grenchler` reads `.data:0xA54`'s relocated slots and reports `fn_27_8F90`
// at slot 81, `fn_27_8F9C` at slot 82 and `fn_27_8FB4` at slot 112 - virtuals this module defines
// with no DOL name to inherit, which is why they carry no base-class spelling here and why the
// module's `ldscript.lcf` FORCEACTIVE lists them. `fn_27_8F34` and `fn_27_8FC0` are ordinary
// methods: `fn_27_8FC0` is called from the module's own bytes and `fn_27_8F34` is a destructor the
// module reaches from its class code. Nothing here needs a `force_active:` entry - measured: the
// build links the module to retail's exact 121468 bytes with none added.
//
// **The class is modelled with padding, not raw offsets.** Every member is named, so
// `tools/check_raw_offsets.py` counts no raw offset in this file.
//
// The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, the arrangement
// `CLumiteRelTail.cpp` and `CPlantScarabSwarmTail.cpp` use: `check_files_cmake.py` requires every
// configure.py `Matching` object to be in `files.cmake`, and only a RELMain/RELExit unit is
// exempt - but a host body would make the port link `fn_27_990C`,
// `GetDamageVulnerability__10CPatternedCFv` and the module's `fn_27_B8F0`-style locals, which the
// port does not have.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

#include "types.h"

#include "rstl/string.hpp"

/** The one-bit fields MWCC packs into a byte, most significant first: `mBit0` is 0x80 of the
 *  byte. `tools/rel_class_map.py Grenchler` names four of the module's virtuals that read one of
 *  these; the field numbers below are the bit positions the bytes show. */
struct SGrenchlerBits {
  u8 mBit0 : 1;
  u8 mBit1 : 1;
  u8 mBit2 : 1;
  u8 mBit3 : 1;
  u8 mBit4 : 1;
  u8 mBit5 : 1;
  u8 mBit6 : 1;
  u8 mBit7 : 1;
};

/** 0x8F34's range: the `rstl::string` at +0x2C is the class's only non-trivial member, and it is
 *  the only thing this destructor touches. The class is CGrenchler, whose full layout this tree
 *  does not model, so the gap is opaque padding of the measured size rather than invented
 *  members. */
struct SGrenchlerStringAt2C {
  uchar mUnknown00[0x2C];
  rstl::string mName; // +0x2C
};

/** 0x8F90's range: the byte at +0x90C. */
struct SGrenchlerFlags90C {
  uchar mUnknown00[0x90C];
  SGrenchlerBits mFlags90c; // +0x90C
};

/** 0x8F9C's range: the byte at +0x420. */
struct SGrenchlerFlags420 {
  uchar mUnknown00[0x420];
  SGrenchlerBits mFlags420; // +0x420
};

/** 0x8FB4's and 0x8FC0's range: the byte at +0xA24. */
struct SGrenchlerFlagsA24 {
  uchar mUnknown00[0xA24];
  SGrenchlerBits mFlagsA24; // +0xA24
};

/** 0x802CE388, `symbols.txt`, claimed by `Kyoto/Alloc/CMemory.cpp` in both builds: declared under
 *  retail's own emitted spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x800742F8, `symbols.txt:2167`, size 0x68: `CPatterned::GetDamageVulnerability() const`,
 *  reached through the mangled name the module's own `bl` uses. */
extern "C" const void* GetDamageVulnerability__10CPatternedCFv(const void* self);

/** 0x990C, 0x1C8, unclaimed: this module's own method, called by `fn_27_8FC0` with the receiver
 *  in r3, the second argument in r4 and the damage-vulnerability pointer in r5. It stays inside
 *  dtk's auto unit, so the link takes it from retail. */
extern "C" void fn_27_990C(void* self, void* arg, const void* vuln);

#ifdef __MWERKS__

// 0x8FC0, 0x58 bytes. No body is inlined here: `GetDamageVulnerability` is called with the
// receiver untouched, its result is moved to the third argument's register and the module's own
// `fn_27_990C` is called with (self, arg, vuln), exactly as the three `mr`s before the `bl` show.
// The last four instructions are the one-bit field clearing `mBit0` - which is `rlwimi r0,r3,7,
// 24,24` with r3 materialised as zero, not a mask, which is what a `& ~0x80` would give.
extern "C" void fn_27_8FC0(SGrenchlerFlagsA24* self, void* arg) {
  fn_27_990C(self, arg, GetDamageVulnerability__10CPatternedCFv(self));
  self->mFlagsA24.mBit0 = 0;
}

// 0x8FB4, 0x0C bytes. Two instructions: the byte load and the fourth bit of it.
extern "C" u8 fn_27_8FB4(const SGrenchlerFlagsA24* self) { return self->mFlagsA24.mBit3; }

// 0x8F9C, 0x18 bytes. The same shape as the two below except that the extracted bit goes through
// the `neg`/`or`/`srwi` `bool` conversion, so the function returns a `bool` (retail's word 2 is
// the record-form `extrwi`, word 3 the conversion).
extern "C" bool fn_27_8F9C(const SGrenchlerFlags420* self) { return self->mFlags420.mBit0 != 0; }

// 0x8F90, 0x0C bytes. Two instructions: the byte load and the second bit of it.
extern "C" u8 fn_27_8F90(const SGrenchlerFlags90C* self) { return self->mFlags90c.mBit1; }

// 0x8F34, 0x5C bytes. A deleting destructor, the shape the whole port uses (`mr. r30,r3 / beq`
// guards the receiver, `extsh. r0,r31 / ble` reaches `Free__7CMemoryFPCv` only for a positive
// flag, and the result is the receiver). The `addic. r0,r30,0x2c / beq` in front of the call is
// the address test MWCC emits for the explicit member destructor: `~rstl::string` is the inline
// `{ internal_dereference(); }`, so naming the member's destructor is what produces both the test
// and the module's out-of-line `internal_dereference__Q24rstl66basic_string<...>Fv` call.
extern "C" void* fn_27_8F34(SGrenchlerStringAt2C* self, short flag) {
  if (self != 0) {
    self->mName.~basic_string();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif // __MWERKS__
