// DigitalGuardianTouchBounds.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x0000D774..0x0000D7DC: two adjacent functions, `fn_14_D774` (`GetTouchBounds`) and
// `fn_14_D7BC` (`GetDamageVulnerability`).
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:224-225` carry
// `fn_14_D774 = .text:0x0000D774; size:0x48` and `fn_14_D7BC = .text:0x0000D7BC; size:0x20`,
// the first ending exactly where the second begins and the second exactly where `fn_14_D7DC`
// (0xD7DC, 0xC0) begins.  The function in front of the claim is `fn_14_D704`, which ends
// where it starts.  Both neighbours of the range are unclaimed, so dtk fills them from retail
// and the module's sha1 still holds.  Two functions, one contiguous range, one file - the
// arrangement `DigitalGuardianDoorWrappers.cpp` uses for the same reason.
//
// **fn_14_D7BC is a pure forward**, eight instructions - prologue, one call, epilogue - and
// it is `DigitalGuardianVulnerability.cpp` verbatim with a different name:
//
//   0xD7BC stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1)
//   0xD7C8 bl PassThruVulnerability__20CDamageVulnerabilityFv
//   0xD7CC lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// **fn_14_D774 is the copy-the-record-if-flagged accessor again**, 0x48 bytes, and the same
// shape as `fn_14_1AB50` in `DigitalGuardianContact.cpp` (0x1AB50, 0x48 bytes) with this
// module's second offset set - 0x164 instead of 0x718, because this is the other of the
// module's two classes:
//
//   0xD774 lbz r0,0x17c(r4) / stb r0,0x18(r3)
//   0xD77C lbz r0,0x17c(r4) / cmplwi r0,0 / beqlr
//   0xD788 lwz r5,0x164(r4) / lwz r0,0x168(r4) / stw r5,0x0(r3) / stw r0,0x4(r3)
//   0xD798 lwz r0,0x16c(r4) / stw r0,0x8(r3)
//   0xD7A0 lwz r5,0x170(r4) / lwz r0,0x174(r4) / stw r5,0xc(r3) / stw r0,0x10(r3)
//   0xD7B0 lwz r0,0x178(r4) / stw r0,0x14(r3) / blr
//
// Two loads and two stores per 8 bytes, in ascending offset order, which is what MWCC emits
// for two 8-byte `union`s of two `u32` and for nothing else that was measured - the same
// measurement, and the same conclusion, that `DigitalGuardianContact.cpp` records for
// `fn_14_1AB50`.  The two types are separate because retail reads the flag at +0x17C of the
// argument and writes it at +0x18 of `this`, and one type cannot be at two offsets at once.
// The flag is re-read after the store because the compiler cannot prove the two objects are
// distinct, and that reload is what makes the second `lbz` part of the body.
//
// Note there is no frame at all here: `fn_14_D774` is a leaf that keeps everything in
// registers, and the `beqlr` on the zero flag is what lets it skip the whole tail without a
// branch back.  That falls out of the plain spelling - the copy is guarded by the flag - and
// is not asked for.
//
// The classes are unnamed in retail, so both definitions below are free functions carrying
// the pointers the ABI puts in r3 and r4, exactly as in `DigitalGuardianContact.cpp`.  Nothing
// here asserts what either object is beyond the offsets retail's own instructions read.
//
// The names are retail's own, so `symbols.txt` needs no rename.  The one relocation is
// `fn_14_D7BC`'s call to `PassThruVulnerability__20CDamageVulnerabilityFv`, a DOL symbol the
// port does not define, so the bodies are behind the `#ifdef __MWERKS__` guard that
// `DigitalGuardianDestroy.cpp` uses and the host object defines nothing.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim), so
// `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/DigitalGuardianTouchBounds.cpp` has nothing to reorder.

#include "types.h"

extern "C" {

#ifdef __MWERKS__

namespace {

union UDGWordPair {
  u32 w[2];
};

// The record fn_14_D774 writes at +0 of `this`: two 8-byte groups with a `u32` between them and
// the flag byte last, which is what the `stb r0,0x18(r3)` at the head of the body lands on.
struct UDGTouch {
  UDGWordPair x0_first;
  u32 x8_second;
  UDGWordPair xC_third;
  u32 x14_fourth;
  u8 x18_flag;
};

// The same record as a member of the *other* object, at +0x164 of it.
struct UDGTouchSource {
  u8 x0_pad[0x164];
  UDGTouch x164_touch;
};

} // namespace

// What the ABI carries in r3 on fn_14_D7BC's return: the address of the shared vulnerability
// record.  A one-word stand-in, unnamed in retail.
struct SDGDamageVulnerability {
  const void* record;
};

// .text 0xDBB50 (module 0), 0x10 bytes: the DOL's
// `CDamageVulnerability::PassThruVulnerability()`.  Declared, never defined here.
const SDGDamageVulnerability* PassThruVulnerability__20CDamageVulnerabilityFv();

// .text 0xD7BC, 0x20 bytes.  The receiver is passed through untouched and never read.
const SDGDamageVulnerability* fn_14_D7BC(void* self) {
  return PassThruVulnerability__20CDamageVulnerabilityFv();
}

// .text 0xD774, 0x48 bytes.  Copies the flag unconditionally, the rest of the record only when
// it is set, and leaves `this` in r3 - so it is a member function returning `*this`.
UDGTouch* fn_14_D774(UDGTouch* out, const UDGTouchSource* self) {
  out->x18_flag = self->x164_touch.x18_flag;
  if (self->x164_touch.x18_flag != 0) {
    out->x0_first = self->x164_touch.x0_first;
    out->x8_second = self->x164_touch.x8_second;
    out->xC_third = self->x164_touch.xC_third;
    out->x14_fourth = self->x164_touch.x14_fourth;
  }
  return out;
}

#endif

} // extern "C"