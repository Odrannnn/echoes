// CIngBoostBallGuardianF78.cpp - IngBoostBallGuardian's (module 30) 0x38-byte record copy,
// `.text` 0xF78..0xFEC: one function, 0x74 = 116 bytes.
//
//   0xF78 fn_30_F78 0x74  fourteen `lfs`/`lwz` loads out of r4 interleaved with fourteen
//                      `stfs`/`stw` stores into r3, then `blr`
//
// **It is the callee of `fn_30_AAC`**, which is what makes the range claimable one function at a
// time: `CIngBoostBallGuardianAAC.cpp` (`.text` 0xAAC, 0x28) declares it `extern "C"` and does
// `if (self) fn_30_F78(self, other);`, so before this carve the module linked with the symbol
// defined by dtk's own bytes and this unit turns that reference into a definition. Four objects in
// the module's link still reference it after the carve - `powerpc-eabi-objdump -r` finds an
// `R_PPC_REL24 fn_30_F78` four times in `auto_00_00000000_text.o`, three times in
// `auto_00_00000130_text.o`, once in `auto_00_0001466C_text.o` and once in
// `CIngBoostBallGuardianAAC.o` - so the callee is still reached and the module still hashes to
// `config/G2ME01/config.yml`'s `956265e8ccf3f489e9cb3a70ec22d0357ce17cca`.
//
// **The body is a member-wise copy of 0x38 bytes and nothing else**, read off
// `build/G2ME01/IngBoostBallGuardian/asm/auto_00_00000000_text.s:1169-1199`: six `lfs`/`stfs` pairs
// at +0x00, +0x04, +0x08, +0x0C, +0x10 and +0x14, six `lwz`/`stw` pairs at +0x18 .. +0x2C, then two
// more `lfs`/`stfs` at +0x30 and +0x34. Fourteen members, fourteen stores, no `addi`, no `li`, no
// `bl`, no relocation anywhere in `.rela.text` for this range, and no call. `RelRecord38` below is
// exactly that, in that order; the member names say what each word is made of and do not claim the
// class is named anything. `CIngBoostBallGuardianAAC.cpp` declares the same 0x38-byte record under
// the same name and passes pointers to it without dereferencing it, which is where this layout was
// first read off; the two declarations are per translation unit and the link sees one `extern "C"`
// symbol, so they must agree and do.
//
// **`*self = other;` does NOT produce this code, and that is measured, not assumed.** Written that
// way, mwcceppc emits `fn_30_F78` as a 32-byte wrapper - `stwu` / `mflr` / `stw` / `bl` / `lwz` /
// `mtlr` / `addi` / `blr` - that calls the implicit copy-assignment operator it generated for the
// record, `__as__11RelRecord38FRC11RelRecord38`, and *that* operator is retail's 116-byte body. The
// unit then defines 148 bytes where retail has 116, and the module stops matching: the linked
// `IngBoostBallGuardian.rel` came out **123404 bytes against retail's 123372**, its `.text` section
// 0x17DA4 against retail's 0x17D84 (exactly the 32 extra bytes), with the wrapper landing at
// `.text` 0xF68 and every following address moved up by 0x20 - measured on this source at `GC/2.7`
// before the members were spelled out one by one. So the fourteen assignments below are load-
// bearing, not a stylistic choice, and they are the same spelling `CIngBoostBallGuardianAD4.cpp`
// uses for its own record copy.
//
// **The function returns `void` and that is measured too.** The last instruction before the `blr`
// is the `stfs` at 0xFE4, and nothing after the final store moves r3, so there is no return value
// to spell - unlike `fn_30_AD4`, which does put its destination back in r3 and is typed to return
// the pointer. `fn_30_AAC`'s epilogue likewise never touches r3 after the `bl`.
//
// **mw_version is load-bearing**, with the same per-object override as the other module-30 units:
// the `Rel("IngBoostBallGuardian", ...)` block this entry sits in defaults to `GC/1.3.2`, and on
// this source (measured by compiling it once per version and counting the `.text` bytes equal to
// retail's 116) `GC/1.3.2` gives **37 of 116** and `GC/3.0a5` 48 of 116, while `GC/2.0`, `2.0p1`,
// `2.5`, `2.6` and `2.7` are all **116 of 116**. Setting the version on the `Rel(...)` block
// instead would recompile the other module-30 units.
//
// **The claim is 0xF78..0xFEC and spans no unclaimed gap.** `fn_30_F30` (0xF30) is 0x48 bytes and
// ends exactly at 0xF78 (`config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:32`), and
// `fn_30_FEC` (0xFEC, 0x8C) starts exactly at 0xFEC, so the carve takes one whole function and
// dtk starts the next auto unit at 0xFEC (`auto_00_00000FEC_text.o`). `total_functions` stays
// 28465 and the module's function count stays 318: the auto unit this split divides goes 22 -> 5
// functions, the new `auto_00_00000FEC_text` takes 16 and this unit contributes 1.
//
// **No dead-strip hazard, and that is measured rather than assumed.** `fn_30_F78` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list (`grep -c` returns 0), but
// the four objects named above all carry a relocation naming it, and the module still comes out
// `cmp`-identical to `orig/G2ME01/files/RelProd/IngBoostBallGuardian.rel`. No `force_active:`
// entry, no `config/G2ME01/config.yml` change and no `symbols.txt` rename: the split claims `.text`
// only, and keeping the `fn_30_*` name matters because dtk's own objects call it that way.
//
// Source order is **descending by retail text offset** (one function here, so the question does not
// arise) and the `extern "C"` wrapper is what keeps dtk's `fn_30_F78` name.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.

extern "C" {

#ifdef __MWERKS__

// Retail's 0x38-byte record, laid out from this function's own disassembly (0xF78, 0x74): six
// floats, then six words, then two floats.  `CIngBoostBallGuardianAAC.cpp` declares the same
// record and passes pointers to it.
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

// .text 0xF78, 0x74 bytes.  `self` is the destination in r3 and `other` the source in r4, and the
// fourteen assignments below are the whole body - see the header comment for why they cannot be
// written as one `*self = other;`.
void fn_30_F78(RelRecord38* self, const RelRecord38& other) {
  self->f00 = other.f00;
  self->f04 = other.f04;
  self->f08 = other.f08;
  self->f0C = other.f0C;
  self->f10 = other.f10;
  self->f14 = other.f14;
  self->w18 = other.w18;
  self->w1C = other.w1C;
  self->w20 = other.w20;
  self->w24 = other.w24;
  self->w28 = other.w28;
  self->w2C = other.w2C;
  self->f30 = other.f30;
  self->f34 = other.f34;
}

#endif
}
