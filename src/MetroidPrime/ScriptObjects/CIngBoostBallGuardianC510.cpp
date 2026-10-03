// CIngBoostBallGuardianC510.cpp - IngBoostBallGuardian's (module 30) "the id I was told to watch
// is not the one I am watching" pair, `.text` 0xC510..0xC5A4: two contiguous functions,
// 0x94 = 148 bytes.
//
//   0xC510 fn_30_C510 0x58  fn_30_C568(self, mgr) && self->+0x108A != self->+0x1088
//   0xC568 fn_30_C568 0x3C  mgr.GetObjectById(self->+0x1088) != nullptr
//
// **The claim spans no gap.** `fn_30_C4E0` (0xC4E0, 0x30) ends exactly at 0xC510 -
// `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:206` - and `fn_30_C5A4` (0xC5A4, 0xE8)
// starts exactly where this claim stops (`symbols.txt:209`), so both neighbours are somebody else's
// range and nothing between them is left to retail. After the carve dtk starts a new auto unit at
// 0xC5A4 (`obj/auto_00_0000C5A4_text.o`), which is the proof the split took.
//
// **`GetObjectById` takes its `TUniqueId` by value and still passes its address, and that is
// measured here rather than assumed.** `include/MetroidPrime/CStateManager.hpp:178` declares
// `const CEntity* GetObjectById(TUniqueId uid) const` - a by-value two-byte class - and the mangle
// `GetObjectById__13CStateManagerCF9TUniqueId` is exactly what that declaration produces, so the
// call below is an ordinary call of the tree's own declaration and not an `extern "C"` fiction.
// MWCC's PPC ABI hands a by-value class parameter of this size over **by reference**, which is
// retail's 0xC574..0xC584:
//
//   lhz r0, 0x1088(r3)   the member, into r0
//   mr  r3, r4           the CStateManager, into `this`
//   addi r4, r1, 0x8     the address of the frame's argument slot
//   sth r0, 0x8(r1)      the member, stored there
//   bl  GetObjectById__13CStateManagerCF9TUniqueId
//
// The frame slot is the ABI's copy, not a choice in the source: passing the member's address
// explicitly (`mgr.GetObjectById(*&self->x1088)`) compiles to the same 148 bytes, while making the
// copy a named local of its own (`TUniqueId id = self->x1088; mgr.GetObjectById(id);`) costs four
// bytes and **28 of 37 words** are retail's. The identical shape is reproduced by
// `CIngSnatchingSwarmAi.cpp`'s `fn_33_3678` - a `Matching` unit in module 33
// (`build/G2ME01/IngSnatchingSwarm/asm/auto_00_00000000_text.s`, `lhz r0,0x412(r3)` /
// `addi r4,r1,0x8` / `sth r0,0x8(r1)` / `bl` at 0x368C..0x369C), so the header's spelling is the
// one retail used.
//
// **`!= nullptr` and not `== nullptr`, which is worth four bytes.** `!= nullptr` is the
// `neg r0,r3` / `or r0,r0,r3` / `srwi r3,r0,31` idiom - the same `!= 0` form
// `CIngBoostBallGuardianPredicates.cpp` tabulates - and the function comes out at retail's 148
// bytes; `== nullptr` is 144 bytes with 31 of 37 words retail's, the `cmplwi r3,0` and inverted
// branch `fn_33_3678` uses instead.
//
// **`fn_30_C510`'s `&&` is written as a flag, and that choice is free.** Retail keeps the
// accumulator in `r31`: `li r31,0` before the call, `clrlwi. r0,r3,24` / `beq` for the callee's
// result (its low byte, the `bool` MWCC returns), the `lhz`/`cmplw`/`beq` for the second test, and
// `li r31,1` on the path that reaches it, both failures landing on one join with `mr r3,r31` in
// the epilogue. `bool result = false; if (a && b) result = true; return result;` is that shape,
// and so is `return a && b;` - both compile to retail's 148 bytes. What is *not* free is lifting the
// call out of the second operand (`bool found = fn_30_C568(self, mgr); if (found) ...`): that is
// 140 bytes and 7 of 37 words, so the call has to stay inside the `&&`'s guard.
// `self` still has to be copied out of r3 into the callee-saved r30 across the call either way.
//
// **The receiver's two halfwords are adjacent, and both are `TUniqueId`.** Retail reads them with
// `lhz` - `+0x1088` in `fn_30_C568`, and `+0x108A` then `+0x1088` in `fn_30_C510` - and compares
// them word for word with `cmplw`, which is `TUniqueId`'s `operator!=`
// (`include/MetroidPrime/TGameTypes.hpp:73-74`, two `ushort`s) and not a four-byte member read.
// **The operand order is load-bearing**: written `self->x1088 != self->x108A` the two `lhz` come out
// the other way round and 35 of 37 words are retail's, so the source compares them in retail's
// order. Comparing `.value` instead of using the operator makes no difference - both are the same
// two `lhz` and one `cmplw`.
//
// `CIngBoostBallGuardianIds` below therefore models the object down to the two members this unit
// reads, with the rest of the object's size spelled as `char` padding, the arrangement
// `CIngSnatchingSwarmBounds.cpp` uses for the same reason (this tree models none of module 30's
// entity code: `fn_30_130`, the module's own entity loader, is still retail's). Nothing here is a
// raw offset, so `docs/research/raw_offsets.md` gains no section for this file.
//
// **No dead-strip hazard, and that is measured.** Both functions are in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list (lines 124 and 125) *and*
// `powerpc-eabi-nm -u` over every object dtk writes into `build/G2ME01/IngBoostBallGuardian/obj/`
// finds both named as undefined by `auto_04_00000000_data.o`, the module's own vtable store. Two
// independent reasons, so no `force_active:` entry, no `config/G2ME01/config.yml` change and no
// `symbols.txt` rename: `fn_30_*` is the name dtk's own objects already call these by.
//
// **mw_version is the module default and no per-object override is needed.** Compiled with this
// tree's flags, this source is byte-identical to retail at `GC/1.3.2` - the
// `Rel("IngBoostBallGuardian", ...)` default - and also at `GC/2.0`, `GC/2.0p1`, `GC/2.5`,
// `GC/2.6` and `GC/2.7`; `GC/3.0a5` rejects these flags outright (`rstl/single_ptr.hpp`), so
// nothing is claimed for it. Setting `mw_version="GC/2.7"` on the `Object(...)` entry - which the
// record-copy units in this module need - would be a second thing to keep true for no gain.
//
// Definitions are in **descending** retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so `fn_30_C568` is written first
// and `fn_30_C510` second - which is also the order that lets `fn_30_C510` call it. Ascending would
// permute the module's bytes with objdiff still at 100% and the module's hash broken; only
// `tools/flip_test.sh` catches that, and here the module's sha1 against
// `config/G2ME01/config.yml` is the test. (`tools/check_decl_order.py` reports
// `0 unit(s) checked` for REL units, as it does for every REL unit in this tree, so the order is
// read off `nm -S` on the built object instead: `fn_30_C510` at +0x00 size 0x58 and `fn_30_C568` at
// +0x58 size 0x3C.)

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TGameTypes.hpp"

// The object these two functions are written over, reduced to the members they touch. Every offset
// is measured: `lhz` at `+0x1088` and at `+0x108A`, nothing else. The type's own size is the
// convenience the padding gives it, not a measurement of the object.
class CIngBoostBallGuardianIds {
private:
  char x_pad0[0x1088];

public:
  // +0x1088 is the id `fn_30_C568` hands to `GetObjectById` and the right-hand side of `fn_30_C510`'s
  // comparison; +0x108A is the left-hand side of it, the id the object currently holds.
  TUniqueId x1088;
  TUniqueId x108A;
};

extern "C" {

#ifdef __MWERKS__

// .text 0xC568, 0x3C bytes. `self` in r3, the CStateManager in r4; the callee sees `mgr` as `this`
// and the value as a pointer to the frame's argument slot, and the test on its result is the
// `!= 0` idiom (see the header).
bool fn_30_C568(CIngBoostBallGuardianIds* self, CStateManager& mgr) {
  return mgr.GetObjectById(self->x1088) != nullptr;
}

// .text 0xC510, 0x58 bytes. `self` is copied into r30 before the call and the result accumulated in
// r31, so both operands survive it; the second test loads +0x108A before +0x1088.
bool fn_30_C510(CIngBoostBallGuardianIds* self, CStateManager& mgr) {
  bool result = false;
  if (fn_30_C568(self, mgr) && self->x108A != self->x1088) {
    result = true;
  }
  return result;
}

#endif
}