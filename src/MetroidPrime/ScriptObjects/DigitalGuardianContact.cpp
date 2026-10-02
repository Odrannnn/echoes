// DigitalGuardianContact.cpp - a carve of DigitalGuardian's .text 0x0001AB50..0x0001AB98:
// `fn_14_1AB50`, the module's "copy this contact record if it is flagged" accessor.
//
// The class is unnamed in retail, as in `DigitalGuardianAccessors.cpp`, so nothing here is evidence
// about a class declaration that does not exist: the whole body is member accesses through the two
// pointers the ABI puts in r3 and r4, which is what keeps the unit layout-immune. The name is
// retail's own `fn_14_<off>` string, so `config/G2ME01/rels/DigitalGuardian/symbols.txt` needs no
// rename, and there is no relocation at all: `powerpc-eabi-nm -u` on this object prints nothing,
// which is the same reason `DigitalGuardianAccessors.cpp` is in `files.cmake`.
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.

// **The two 8-byte members are `union`s of two `u32`, and that is measured, not stylistic.**
// Retail copies the record as 8+4+8+4 bytes:
//
//     lwz r5,0x718(r4); lwz r0,0x71c(r4); stw r5,0x0(r3); stw r0,0x4(r3);
//     lwz r0,0x720(r4); stw r0,0x8(r3);
//     lwz r5,0x724(r4); lwz r0,0x728(r4); stw r5,0xc(r3); stw r0,0x10(r3);
//     lwz r0,0x72c(r4); stw r0,0x14(r3);
//
// i.e. two loads and two stores per 8 bytes, in ascending offset order. Four other spellings were
// compiled with this unit's exact MWCC command line and none produces that shape: six named `u32`
// members copy one word at a time through `r0`; a whole-struct assignment does the same and then
// re-copies the flag; a two-`u32` nested struct and a `u32[2]` member both do the same; and a
// `s64`/`u64` member loads the pair into `r0` and `r5` but stores them **descending**
// (`stw r5,4(r3)` before `stw r0,0(r3)`), which is the one instruction order that differs. Only
// the 8-byte union stores ascending. A union with a `u64` alternative member was also measured and
// is *wrong* - it compiles to `lfd`/`stfd`. So the union here is the one declaration that
// reproduces retail, and its two members are the shape retail's own record has at 0x718.

#include "types.h"

namespace {

union UDGWordPair {
  u32 w[2];
};

// The record the accessor writes at +0 of `this`: two 8-byte groups with a `u32` between them and
// the flag byte last. The offsets are retail's, and `UDGContact` reproduces them exactly - the
// flag lands at +0x18, which is the `stb r0,0x18(r3)` in the first instruction pair.
struct UDGContact {
  UDGWordPair x0_first;
  u32 x8_second;
  UDGWordPair xC_third;
  u32 x14_fourth;
  u8 x18_flag;
};

// The same record as a member of the *other* object, at +0x718 of it. The two types are separate
// because retail reads the flag at +0x730 of the argument and writes it at +0x18 of `this`, and one
// type cannot be at two offsets at once.
struct UDGSource {
  u8 x0_pad[0x718];
  UDGContact x718_contact;
};

} // namespace

extern "C" {

// .text 0x0001AB50, 0x48 bytes. Copies the flag unconditionally, the rest of the record only when
// it is set, and leaves `this` in r3 - so it is a member function returning `*this`.
//
// The flag is re-read after the store because the compiler cannot prove `this` and `other` are
// distinct objects, and that reload is what makes the second `lbz r0,0x730(r4)` part of retail's
// body; it falls out of the plain spelling rather than being asked for.
UDGContact* fn_14_1AB50(UDGContact* out, const UDGSource* self) {
  out->x18_flag = self->x718_contact.x18_flag;
  if (self->x718_contact.x18_flag != 0) {
    out->x0_first = self->x718_contact.x0_first;
    out->x8_second = self->x718_contact.x8_second;
    out->xC_third = self->x718_contact.xC_third;
    out->x14_fourth = self->x718_contact.x14_fourth;
  }
  return out;
}

} // extern "C"