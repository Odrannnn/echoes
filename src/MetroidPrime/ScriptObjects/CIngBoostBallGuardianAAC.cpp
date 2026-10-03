// CIngBoostBallGuardianAAC.cpp - IngBoostBallGuardian's (module 30) null-guarded forwarding
// wrapper, `.text` 0xAAC..0xAD4: one function, 0x28 = 40 bytes.
//
//   0xAAC fn_30_AAC 0x28  `stwu` / `mflr` / `cmplwi r3,0` / `stw` / `beq` / `bl fn_30_F78` and the
//                           four-instruction epilogue
//
// **The whole body is a null guard and one forwarded call.** Retail is
//
//   cmplwi r3, 0x0        the test is on r3 and only on r3
//   beq   .L_00000AC4     the whole `bl` is skipped when it is zero
//   bl    fn_30_F78       no `addi`, no `mr`: r3 and r4 arrive in r3 and r4 unchanged
//
// so there is no address arithmetic to get wrong and no return value to spell - r3 is the
// destination and r4 the source on entry and neither is touched before the callee. `if (self)
// fn_30_F78(self, other);` is the whole function, and it is what makes the unit worth claiming
// (40 bytes of exact retail, one function, no data).
//
// **`fn_30_A8C` is its only caller and it forwards just as blindly**: `fn_30_A8C` (0xA8C, 0x20) is
// `stwu` / `mflr` / `bl fn_30_AAC` / `lwz` / `mtlr` / `addi` / `blr` - no argument setup at all, so
// the two-argument signature is measured from `fn_30_F78`'s own body rather than from this caller's
// shape. Nothing in this file depends on it.
//
// **The record's layout comes from `fn_30_F78`, and this function measures none of it.** Retail's
// `fn_30_F78` (0xF78, 0x74) is a straight member-wise copy of 0x38 bytes: `lfs`/`stfs` at +0x00,
// +0x04, +0x08, +0x0C, +0x10 and +0x14, `lwz`/`stw` pairs at +0x18, +0x1C, +0x20, +0x24, +0x28 and
// +0x2C, then `lfs`/`stfs` at +0x30 and +0x34. `RelRecord38` below is exactly that, in that order,
// and the member names say what each word is made of - they do not claim the class is named. Note
// that nothing at +0x00 of the *source* is a pointer and nothing in the copy is a test, so the
// callee cannot be short-circuited by anything this unit does.
//
// **`fn_30_F78` is declared, not defined.** It is `.text:0xF78 size 0x74` in
// `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:33`, it is claimed by no unit, and after
// this carve dtk starts no new auto unit at 0xF78 - the enclosing auto unit
// (`auto_00_00000B30_text.o`, whose `.text` begins at 0xB30) defines it at object offset 0x448 and
// that object is in the module's link. So the callee resolves and this unit adds no undefined
// symbol to the module. (`build/G2ME01/IngBoostBallGuardian/obj/IngBoostBallGuardian/MetroidPrime/
// ScriptObjects/CIngBoostBallGuardianF78.o` is a stale artefact of an earlier attempt at claiming
// `fn_30_F78` itself: there is no `configure.py` entry for it, so nothing links it, and it is left
// alone here. Claiming 0xF78..0xFEC is a separate unit of work.)
//
// **The function returns `void` and that is measured, not assumed.** The epilogue
// (`lwz r0,0x14(r1)` / `mtlr r0` / `addi r1,r1,0x10` / `blr`) never touches r3 after the callee, so
// whatever `fn_30_F78` returned is what leaves - and `fn_30_F78`'s own last instruction before its
// `blr` is an `stfs`, not a `mr r3,...`, so it has no return value to pass on.
//
// **mw_version is load-bearing**, with the same per-object override as the other entries in this
// module: the claim is inside `Rel("IngBoostBallGuardian", ...)` whose module default is
// `GC/1.3.2`, and this source is compiled at `GC/2.7` like the rest of this class. Setting the
// version on the `Rel(...)` block instead would recompile the other module-30 units.
//
// **No dead-strip hazard, and that is measured.** `fn_30_AAC` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `powerpc-eabi-objdump -r`
// finds an `R_PPC_REL24 fn_30_AAC` in `auto_00_00000000_text.o` and in `auto_00_00000130_text.o`
// (the call at 0xA98 inside `fn_30_A8C`), and `auto_00_00000130_text.o` is in the module's link -
// it is the unit this carve splits in two. No `force_active:` entry, no `config/G2ME01/config.yml`
// change and no `symbols.txt` rename: the claim is `.text` only, and `fn_30_*` is the name dtk's
// own objects call it.
//
// **The claim is 0xAAC..0xAD4 and not a wider run.** `fn_30_A8C` in front of it is 0x20 bytes and
// ends exactly at 0xAAC, and `fn_30_AD4` behind it is a different function (claimed by
// `CIngBoostBallGuardianAD4.cpp`), so the claim spans no unclaimed gap.
//
// Source order is **descending by retail text offset** (one function here, so the question does not
// arise) and the `extern "C"` wrapper is what keeps dtk's `fn_30_AAC` name.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.

extern "C" {

#ifdef __MWERKS__

// Retail's 0x38-byte record, laid out from `fn_30_F78`'s own disassembly (0xF78, 0x74): six floats,
// then six words, then two floats.  This unit passes pointers to it and never dereferences it.
struct RelRecord38 {
  float f00;
  float f04;
  float f08;
  float f0C;
  float f10;
  float f14;
  int w18;
  int w1C;
  int w20;
  int w24;
  int w28;
  int w2C;
  float f30;
  float f34;
};

// `fn_30_F78` by the name dtk's own objects give it; see the header comment for why it resolves.
void fn_30_F78(RelRecord38* self, const RelRecord38& other);

// .text 0xAAC, 0x28 bytes.  `self` is the destination in r3 and `other` the source in r4, both
// forwarded to the callee untouched; the only test is the null guard on `self`.
void fn_30_AAC(RelRecord38* self, const RelRecord38& other) {
  if (self) {
    fn_30_F78(self, other);
  }
}

#endif
}