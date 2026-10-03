// CIngBoostBallGuardian10694.cpp - IngBoostBallGuardian's (module 30) whole-sub-object copy,
// `.text` 0x10694..0x108D4: one function, 0x240 = 576 bytes.
//
//   0x10694 fn_30_10694 0x240  sixty head members, a call at +0xF0, two members, a call at +0x140
//
// **It is the function the two units behind it in the module are the callees of.** Retail emits a
// `R_PPC_REL24` to `fn_30_10B90` at object offset 0x35B8 and to `fn_30_108D4` at 0x35D4 of
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` (the object that begins at module
// `.text` 0xD2E0, so object offset + 0xD2E0 is the module offset), and both of those are units of
// their own now - `CIngBoostBallGuardian10B90.cpp` (0x10B90, the 0x48-byte record) and
// `CIngBoostBallGuardian108D4.cpp` (0x108D4, the 0x15C-byte record) - which is what makes this one
// claimable at all.
//
// **The layout is retail's and it is load-bearing.** Sixty loads and sixty stores off r31 (`other`)
// into r30 (`self`), plus the two address pairs for the calls: the sixty member offsets are
// 0x00..0xEF, then 0x138 and 0x13C, and every load's type (`lwz`/`lfs`) is the store's type at the
// same offset. Nothing is narrower than a word and nothing is packed: a byte member would change
// the instruction and the byte count. **There is no repeating stride in the head** - the per-member
// type sequence `W F F F F F F W W W W W W W W W W W W W F W F F F W W W W W F F F F F W F F F W
// F F F F F W F F F F W W F F F W W W F F F` is aperiodic for every period up to 30 (checked over
// all 62 members, script not by eye), which is the question `CIngBoostBallGuardian108D4.cpp` says to
// ask before spelling a copy of this shape. So this one is flat member-by-member, where that one is
// one assignment per 12-byte triple.
//
// **The two gaps are the two callees, and the offsets pin the members.** +0xF0 is the head's end
// rounded up to the 0x48-byte record's start, and 0xF0 + 0x48 = 0x138 is exactly where the inline
// copy resumes; 0x140 is 0x13C + 4 and 0x140 + 0x15C = 0x29C. Retail's own record layouts (0x48 with
// no padding, the 29 `float, int, unsigned char` triples at stride 12) reproduce both boundaries, so
// the sub-object this function copies is 0x29C bytes and its last store is +0x29B. The module's
// *whole* class is larger than that - `fn_30_1056C` (0x1056C) calls this function and then
// initialises from +0x29C on - which is why `fn_30_10694` reads as a base sub-object's copy
// assignment, and why the two callee records are spelled by dtk as functions of their own.
//
// **The calls are spelled as calls, and `self->rec0F0 = other.rec0F0` cannot be used.** Writing the
// assignment would make this object define its own out-of-line `__as__` for the 0x48-byte record
// (measured on `CIngBoostBallGuardian10B90.cpp`: eighteen members is where `GC/2.7` stops inlining
// it) - two functions where retail defines one, which is what `tools/unit_fit.sh` reports and what
// makes a unit un-promotable however good it looks. Retail calls a *named* function instead, so the
// declarations are `extern "C"` and the bodies are two calls, matching the two relocations.
//
// **The two record types are declared here as well as in their own units, identically.** That is
// not duplication by accident: the ABI here is the struct layout, and this function's bytes depend
// on those offsets being the ones `CIngBoostBallGuardian10B90.cpp` and
// `CIngBoostBallGuardian108D4.cpp` compile for their own copies. It is checked by measurement, not
// by inspection - all three objects are byte-identical to retail, so all three agree.
//
// **mw_version is load-bearing**, for the same reason and with the same per-object override as the
// two units it calls: retail keeps two temporaries live and issues each load two instructions ahead
// of its store (`r0`/`r3` for the words, `f0`/`f1` for the floats, `r5` for the word at +0xDC that
// the first call would otherwise clobber), and at the module default `GC/1.3.2` the same source
// gives plain load/store pairs in source order instead.
//
// **No dead-strip hazard, and that is measured.** `fn_30_10694` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `powerpc-eabi-objdump -r`
// shows `R_PPC_REL24`s naming it at module 0x10588 (in `fn_30_1056C`, inside
// `auto_00_0000D2E0_text.o` at object offset 0x32A8) and at module 0x10C40 (in `fn_30_10C24`,
// inside `auto_00_00010C24_text.o` at object offset 0x1C), so dtk's own objects hold the reference
// and this unit's `.text` survives the link. Those two call sites are each named twice, by the head
// object `auto_00_00000000_text.o` as well, because dtk's auto units overlap - `fn_30_EC6C` is in
// both `auto_00_00000000_text.o` and `auto_00_0000D2E0_text.o` - so the four relocation records are
// two instructions, not four. No `force_active:` entry, no `config/G2ME01/config.yml` change and no
// `symbols.txt` rename: the split claims `.text` only, and keeping the `fn_30_*` name matters
// because dtk's objects name it that way.
//
// **The claim is 0x10694..0x108D4 and not a wider run.** `fn_30_1056C` in front of it (0x1056C,
// 0x128) ends exactly at 0x10694, so the claim spans no unclaimed gap, and `fn_30_108D4` behind it is
// the 108D4 unit. `total_functions` stays 28465 because retail's function boundaries do not move.
//
// **It returns `self`.** The epilogue's `mr r3,r30` (0x35dc) puts the destination back in r3, which
// is what a copy assignment returns, so the return type is the pointer and not `void`.
//
// The member names say what each word is *made of*, not what the sub-object is called; a copy
// assignment says what it is made of and nothing else.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's 0x48-byte record at +0xF0, the type `fn_30_10B90` copies: six words, nine floats and
// three words, 0x48 with no padding. Same layout as in `CIngBoostBallGuardian10B90.cpp`.
struct RelRecord48 {
  int w00;
  int w04;
  int w08;
  int w0C;
  int w10;
  int w14;
  float f18;
  float f1C;
  float f20;
  float f24;
  float f28;
  float f2C;
  float f30;
  float f34;
  float f38;
  int w3C;
  int w40;
  int w44;
};

// Retail's 0x15C-byte record at +0x140, the type `fn_30_108D4` copies: 29 copies of this 12-byte
// triple. Same layout as in `CIngBoostBallGuardian108D4.cpp`.
struct RelTriple {
  float f;
  int w;
  unsigned char b;
};

struct RelRecord15C {
  RelTriple t[29];
};

// The two callees, by the names dtk's own object gives them, each with the signature its own unit
// compiles: `self` in r3, `other` in r4, and the body is a member-by-member copy.
void fn_30_10B90(RelRecord48* self, const RelRecord48& other);
void fn_30_108D4(RelRecord15C* self, const RelRecord15C& other);

// Retail's sub-object: the sixty-member head at +0x00, the 0x48-byte record at +0xF0, a word and a
// float at +0x138, and the 0x15C-byte record at +0x140 - 0x29C bytes. Member types and offsets are
// read off the loads and stores in `fn_30_10694`; the header comment above is load-bearing on why.
struct RelRecord29C {
  int w00;
  float f04;
  float f08;
  float f0C;
  float f10;
  float f14;
  float f18;
  int w1C;
  int w20;
  int w24;
  int w28;
  int w2C;
  int w30;
  int w34;
  int w38;
  int w3C;
  int w40;
  int w44;
  int w48;
  float f4C;
  int w50;
  float f54;
  float f58;
  float f5C;
  int w60;
  int w64;
  int w68;
  int w6C;
  int w70;
  float f74;
  float f78;
  float f7C;
  float f80;
  float f84;
  int w88;
  float f8C;
  float f90;
  float f94;
  int w98;
  float f9C;
  float fA0;
  float fA4;
  float fA8;
  float fAC;
  int wB0;
  float fB4;
  float fB8;
  float fBC;
  float fC0;
  int wC4;
  int wC8;
  float fCC;
  float fD0;
  float fD4;
  int wD8;
  int wDC;
  int wE0;
  float fE4;
  float fE8;
  float fEC;
  RelRecord48 rec0F0;
  int w138;
  float f13C;
  RelRecord15C rec140;
};

// .text 0x10694, 0x240 bytes. `self` is the destination in r3 and `other` the source in r4; the
// sixty head copies and the two at +0x138 are spelled out one member at a time - which is what
// `GC/2.7` compiles to retail's two-deep schedule, each load issued two instructions ahead of its
// store - and the two records at +0xF0 and +0x140 are the calls retail makes. Returns `self`.
RelRecord29C* fn_30_10694(RelRecord29C* self, const RelRecord29C& other) {
  self->w00 = other.w00;
  self->f04 = other.f04;
  self->f08 = other.f08;
  self->f0C = other.f0C;
  self->f10 = other.f10;
  self->f14 = other.f14;
  self->f18 = other.f18;
  self->w1C = other.w1C;
  self->w20 = other.w20;
  self->w24 = other.w24;
  self->w28 = other.w28;
  self->w2C = other.w2C;
  self->w30 = other.w30;
  self->w34 = other.w34;
  self->w38 = other.w38;
  self->w3C = other.w3C;
  self->w40 = other.w40;
  self->w44 = other.w44;
  self->w48 = other.w48;
  self->f4C = other.f4C;
  self->w50 = other.w50;
  self->f54 = other.f54;
  self->f58 = other.f58;
  self->f5C = other.f5C;
  self->w60 = other.w60;
  self->w64 = other.w64;
  self->w68 = other.w68;
  self->w6C = other.w6C;
  self->w70 = other.w70;
  self->f74 = other.f74;
  self->f78 = other.f78;
  self->f7C = other.f7C;
  self->f80 = other.f80;
  self->f84 = other.f84;
  self->w88 = other.w88;
  self->f8C = other.f8C;
  self->f90 = other.f90;
  self->f94 = other.f94;
  self->w98 = other.w98;
  self->f9C = other.f9C;
  self->fA0 = other.fA0;
  self->fA4 = other.fA4;
  self->fA8 = other.fA8;
  self->fAC = other.fAC;
  self->wB0 = other.wB0;
  self->fB4 = other.fB4;
  self->fB8 = other.fB8;
  self->fBC = other.fBC;
  self->fC0 = other.fC0;
  self->wC4 = other.wC4;
  self->wC8 = other.wC8;
  self->fCC = other.fCC;
  self->fD0 = other.fD0;
  self->fD4 = other.fD4;
  self->wD8 = other.wD8;
  self->wDC = other.wDC;
  self->wE0 = other.wE0;
  self->fE4 = other.fE4;
  self->fE8 = other.fE8;
  self->fEC = other.fEC;
  fn_30_10B90(&self->rec0F0, other.rec0F0);
  self->w138 = other.w138;
  self->f13C = other.f13C;
  fn_30_108D4(&self->rec140, other.rec140);
  return self;
}

#endif
}