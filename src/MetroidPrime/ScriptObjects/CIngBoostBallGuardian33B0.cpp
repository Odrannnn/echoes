// CIngBoostBallGuardian33B0.cpp - IngBoostBallGuardian's (module 30) record copy,
// `.text` 0x33B0..0x340C: one leaf function, 0x5C bytes.
//
//   0x33B0 fn_30_33B0 0x5C  eleven member copies, 8 floats, 2 words, 1 byte
//
// **Leaf**, measured on `build/G2ME01/IngBoostBallGuardian/obj/auto_00_000020A0_text.o`: 22
// loads and stores and one `blr`, and its `.rela.text` holds nothing between section offsets
// 0x1310 and 0x136C (retail 0x33B0..0x340C), so the bytes are the whole of the claim and there
// is no callee to declare. `CIngBoostBallGuardian10B90.cpp` spells the same kind of function one
// range up the module; this one has eleven members instead of eighteen.
//
// **The layout is retail's and it is load-bearing.** Eight floats at +0x00..+0x1F, then two words
// at +0x20 and +0x24, then a byte at +0x28 - read straight off the loads: every float is `lfs`
// (`+0x00, +0x04, ... +0x1C`), the two words are `lwz` at +0x20 and +0x24, and +0x28 is `lbz`,
// so it is `unsigned char` and not `bool`. The record's own size is *not* measurable from these
// bytes - 0x29 is read and the last member is a byte - so the local type is named for the extent
// the copy reaches, and the members say what each word is made of, not what the record is called.
//
// **Who calls it, and what that fixes about the signature.** `fn_30_324C` (0x324C, 0x164) calls
// it at retail 0x3334 (`R_PPC_REL24` at section offset 0x1294 of the object above) with
// `addi r3,r1,104` / `addi r4,r1,56` - two locals of its own frame, destination at r1+0x68 and
// source at r1+0x38. So the record is copied as a lvalue through r3/r4 rather than constructed
// through a hidden return pointer, which is why the parameters are spelled `self` and `other`
// and why the body is member-by-member rather than a constructor initialising `self`.
//
// **mw_version is what produces retail's schedule, and the override is per-object.** Retail keeps
// two float temporaries live and issues every load two instructions ahead of its store
// (`f1`/`f0`, then two stores, then the next two loads). Under the module default `GC/1.3.2` the
// same source compiles to plain pairs - one live register, source order - and only **31 of the 92
// bytes** are retail's (measured with `tools/probe_cc.sh` on this source against
// `orig/G2ME01/files/RelProd/IngBoostBallGuardian.rel`; the body is right, the schedule is not, so
// the size is the same and every percentage-based check has to be told the difference). Under
// `GC/2.7` it is byte-identical to retail, 92 of 92. That is why the `Object(...)` entry in
// configure.py carries a per-object `mw_version="GC/2.7"` - the same override the 388C, 10B90 and
// 108D4 entries use - rather than the module-wide one, which would recompile the other module-30
// units.
//
// **The two word copies are written +0x24 before +0x20, against the declaration order, and that
// is the whole of the difference between 88 and 92 bytes.** Retail issues
// `lwz r0,0x24(r4)` / `lwz r5,0x20(r4)` - the later member first - interleaved with the last
// `stfs` and the `lbz`, so the two words sit in different registers (r0 and r5) rather than one
// reused `r0`. MWCC honours the *statement* order here. Three other spellings were measured on
// this source at `GC/2.7` against retail, all **88 of 92 bytes** and none of them retail's: the
// eleven copies in declaration order; the two words as a nested 2-word struct assigned whole; and
// the whole-object `*self = other`. That last one is worth stating because it is the spelling
// `CIngBoostBallGuardian388C.cpp` uses successfully at ten members - at eleven it still inlines
// (one function, 92 bytes, no out-of-line `__as__`), so the inlining threshold noted there is not
// what is at work here; what is at work is the statement order, and all three spellings agree on
// it because they all write +0x20 first. Only the reversed pair gives retail's registers.
//
// **No dead-strip hazard, and that is measured.** `fn_30_33B0` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list (unlike `fn_30_3790`),
// but `powerpc-eabi-objdump -r` shows an `R_PPC_REL24` naming it in
// `auto_00_00000000_text.o` (section offset 0x3334) as well as in
// `auto_00_000020A0_text.o` (0x1294) - the two dtk objects that define it today - so a reference
// is in the link once the carve removes their copies. No `force_active:` entry, no
// `config/G2ME01/config.yml` change and no `symbols.txt` rename.
//
// **The claim is 0x33B0..0x340C and not a wider run.** `fn_30_340C` (0x6C) behind it is a
// different function and `fn_30_324C` (0x164) in front of it is the ray-intersection builder that
// calls this one, so neither is claimed here.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's record, member types read off the loads in fn_30_33B0. Only 0x29 bytes are touched by
// the copy, so this type's own size is a convenience and not a measurement.
struct RelRecord2C {
  float f00;
  float f04;
  float f08;
  float f0C;
  float f10;
  float f14;
  float f18;
  float f1C;
  unsigned int w20;
  unsigned int w24;
  unsigned char b28;
};

// .text 0x33B0, 0x5C bytes. `self` is the destination in r3, `other` the source in r4, and the
// body is the eleven member copies `GC/2.7` compiles to retail's two-deep schedule, in retail's
// statement order rather than the declaration's - see the note above.
void fn_30_33B0(RelRecord2C* self, const RelRecord2C& other) {
  self->f00 = other.f00;
  self->f04 = other.f04;
  self->f08 = other.f08;
  self->f0C = other.f0C;
  self->f10 = other.f10;
  self->f14 = other.f14;
  self->f18 = other.f18;
  self->f1C = other.f1C;
  // +0x24 before +0x20: retail loads them in this order, into r0 and r5, and MWCC follows the
  // statement order. Written the other way round this is 88 of 92 bytes, not retail's.
  self->w24 = other.w24;
  self->w20 = other.w20;
  self->b28 = other.b28;
}

#endif
}
