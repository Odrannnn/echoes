// DigitalGuardianScannable.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x00009DBC..0x00009DFC: `fn_14_9DBC`, the module's own `GetScannableObjectInfo` override.
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:180` carries
// `fn_14_9DBC = .text:0x00009DBC; // type:function size:0x40`, and `fn_14_9DFC` (0x9DFC, 0x8C)
// begins exactly where this claim ends; `fn_14_9CA4` (0x9CA4, 0x118) ends exactly where this
// claim starts.  Both neighbours are unclaimed, so dtk fills them from retail and the
// module's sha1 still holds.
//
// **What it is.**  0x40 bytes, a two-armed select on one flag bit:
//
//   0x9DBC stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1)
//   0x9DC8 lbz r0,0x128c(r3)
//   0x9DCC extrwi. r0,r0,1,25          ; one bit of the byte at +0x128C
//   0x9DD0 beq .L_00009DE8
//   0x9DD4 lwz r4,0xe68(r3)
//   0x9DD8 cmplwi r4,0x0 / beq .L_00009DE8
//   0x9DE0 lwz r3,0x8(r4)
//   0x9DE4 b .L_00009DEC
//   .L_00009DE8 bl GetScannableObjectInfo__10CPatternedCFv
//   .L_00009DEC lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// so: if the flag bit is clear, **or** the pointer at +0xE68 is null, the module calls its
// base implementation `CPatterned::GetScannableObjectInfo()`; otherwise it returns the word
// at +8 of the object that pointer names, which is the scan target's own link field.  Both
// tests branch *forward to the call*, and the module's answer is reached over one `b` past
// it - that is the shape of a nested `if` whose body is the early return, not of a
// `||`-guard in front of one return, which compiles to the branches the other way round.
// Measured, with this unit's exact MWCC command line and the retail 0x40 bytes to compare:
//
//   nested `if (bit) { if (ptr) { return ptr->x8_link; } } return base();`  byte-exact (this)
//   the same as `if (!bit || !ptr) { return base(); } return ptr->x8_link;`  same
//                                                                             instructions, the
//                                                                             two `beq` become
//                                                                             `bne` + `b` in the
//                                                                             other order
//   the flag as one `u8` bitfield instead of a packed byte                  the `lbz` becomes
//                                                                             `lbz` at a
//                                                                             different offset, or
//                                                                             `rlwinm` with the
//                                                                             wrong rotate
//   one leading `u32` ahead of the scannable's `x8_link` member             `lwz r3,0x4(r4)`
//                                                                             instead of
//                                                                             `lwz r3,0x8(r4)` -
//                                                                             one byte of the
//                                                                             module's sha1, and
//                                                                             100.00% fuzzy
//
// **The flag is a packed byte, not a word and not a mask.**  Retail loads it with `lbz` - a
// byte - and then extracts one bit, so the tested field is a member of a one-byte unit.  A
// one-byte unit with the tested bit as its **second** declared `bool : 1` is what compiles to
// this exact `lbz` + `extrwi. r0,r0,1,25`; measured positions give `extrwi` shifts of 27 (third
// `bool`), 28 (fourth), 30 (fifth), 31 (sixth) and a `clrlwi` for the seventh, and a `u32`
// storage unit at the same offset gives `lwz 0x1290` instead of `lbz 0x128c` because the
// struct is then 4-aligned.  This is the same encoding `CLumiteRel.cpp` records for the
// `+0x34C` flag family (`extrwi r3,r0,1,28`, the fourth `bool : 1` of its byte), one position
// along.
// `GetScannableObjectInfo` is vtable slot 27, which the module's class overrides.
//
// The class is unnamed in retail - nothing in these 0x40 bytes names it - so the definition
// below is a free function carrying the pointer the ABI puts in r3, exactly as in
// `DigitalGuardianContact.cpp`, and the three types involved are local stand-ins with the
// fields these instructions read and nothing else.  Nothing here asserts what any of them is
// beyond those offsets.
//
// The name is retail's own `fn_14_9DBC`, so `symbols.txt` needs no rename.  The one relocation
// is the call to `GetScannableObjectInfo__10CPatternedCFv`, a DOL symbol (0x8007427C,
// `config/G2ME01/symbols.txt:2165`, 0x50 bytes, whose own body opens with the identical
// `IsIngPossessed` / `+0x39C` pointer / `+8` select) the port does not define, so the body is
// behind the `#ifdef __MWERKS__` guard that `DigitalGuardianDestroy.cpp` uses and the host
// object defines nothing.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim); there is one function here,
// so `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/DigitalGuardianScannable.cpp` has nothing to reorder.

#include "types.h"

extern "C" {

#ifdef __MWERKS__

namespace {

// The byte at +0x128C.  `x1_tested` is the second declared `bool : 1`, which is what makes
// the read `lbz 0x128c` + `extrwi. r0,r0,1,25`; the other two members exist only to give the
// unit somewhere to sit, and neither is ever read.
struct SDGFlagByte {
  bool x0_lower : 1;
  bool x1_tested : 1;
  u8 x2_upper : 6;
};

// The object at +0xE68 of the receiver.  Retail reads the word at +8 and returns it, so the
// two leading words here are padding retail's own layout also has - one is not enough, and a
// single `u32` member ahead of `x8_link` puts the read at +4.
struct SDGScannable {
  u32 x0_pad[2];
  u32 x8_link;
};

// The receiver: the pointer at +0xE68 and the flag byte at +0x128C, in the order retail
// reads them.
struct SDGScannableSelf {
  u32 x0_pad[0xE68 / 4];
  SDGScannable* xE68_scannable;
  u8 xE6C_pad[0x128C - 0xE6C];
  SDGFlagByte x128C_flag;
};

} // namespace

// .text 0x7427C (module 0), 0x50 bytes: the DOL's `CPatterned::GetScannableObjectInfo()`.
// Declared, never defined here - the DOL supplies it, so the object carries a relocation to a
// symbol the port does not define.
u32 GetScannableObjectInfo__10CPatternedCFv(void* self);

// .text 0x9DBC, 0x40 bytes.  The module's own answer is the word at +8 of the scannable the
// receiver points at; every other case falls through to the base implementation.
u32 fn_14_9DBC(SDGScannableSelf* self) {
  if (self->x128C_flag.x1_tested) {
    if (self->xE68_scannable != nullptr) {
      return self->xE68_scannable->x8_link;
    }
  }
  return GetScannableObjectInfo__10CPatternedCFv(self);
}

#endif

} // extern "C"