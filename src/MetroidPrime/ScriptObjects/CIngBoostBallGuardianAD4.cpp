// CIngBoostBallGuardianAD4.cpp - IngBoostBallGuardian's (module 30) 0x30-byte record copy
// assignment, `.text` 0xAD4..0xB30: one function, 0x5C = 92 bytes.
//
//   0xAD4 fn_30_AD4 0x5C  `bl __copy` (21 bytes), two word copies, `bl fn_30_B84`
//
// **It is the second callee of `fn_30_1056C`**, which is what makes it claimable:
// `CIngBoostBallGuardian1056C.cpp` emits `R_PPC_REL24 fn_30_AD4` at object offset 0x28 of
// `build/G2ME01/src/IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardian1056C.o`
// for the member at +0x29C. Before that entry landed, `fn_30_AD4` was an `extern "C"`
// declaration that five of the module's own objects still referenced -
// `powerpc-eabi-objdump -r` finds the relocation in `auto_00_00000000_text.o` four times and in
// `auto_00_00000130_text.o`, `auto_00_0000D2E0_text.o`, `auto_00_00010C24_text.o` and
// `auto_00_0001466C_text.o` once each - so the module linked with it undefined in our objects and
// still does for anything this file does not define.
//
// **The layout is retail's and it is what the body is written from.** Three regions, read off the
// disassembly at `build/G2ME01/IngBoostBallGuardian/asm/auto_00_00000000_text.s`:
//
//   +0x00  0x15 bytes  `li r5,0x15` then `bl __copy` with r3 = r30 and r4 = r31 and *no* `addi`
//   +0x15  3 bytes     never touched: not read, not written, not copied
//   +0x18  0x08 bytes  `lwz r5,0x18(r31)` / `lwz r0,0x1c(r31)` and the two matching `stw`s
//   +0x20  0x10 bytes  `addi r3,r30,0x20` / `addi r4,r31,0x20` then `bl fn_30_B84`
//
// - **21 bytes, not 24: the size is `li r5,0x15` and the argument is a byte count**
//   (`src/Runtime/CPlusLibPPC.cpp`'s `__copy(char*, char*, size_t)`), so +0x15..+0x17 are padding
//   and this unit does not copy them. `CIngBoostBallGuardian1056C.cpp` sizes this same record as
//   `RelRecord30` with `unsigned char sub00[0x18]`, which agrees: 0x18 + 8 + 0x10 = 0x30.
// - **The head sub-record is at offset 0**, which is why the `__copy` gets bare `r30`/`r31` and no
//   address arithmetic - the same reasoning the 1056C entry gives for its `fn_30_10694` call.
// - **The last 0x10 bytes are a callee, not inline loads**, so nothing here measures their layout;
//   `RelSub10` below is sized from `fn_30_B84`'s own disassembly and no more.
//
// **The three padding bytes are alignment, and that is measured rather than asserted.** Declaring
// them as a member - `unsigned char b[0x15]; unsigned char pad15[3];` - makes the implicit
// assignment copy the pad too, and the object comes out **108 bytes against retail's 92**, with
// only 49 of the first 92 equal to retail's (measured on this source at `GC/2.7` against
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000000_text.o`; the same 108-against-92 failure
// is recorded for `CDamageVulnerability` in `include/MetroidPrime/CDamageVulnerability.hpp`).
// `__attribute__((aligned(4)))` on the 21-byte array makes those bytes padding, which is what
// retail has. It is redundant *as written* - the `int w18` behind it already pads the array to
// 0x18, and dropping the attribute is still 92 of 92 bytes, also measured - and it is kept
// because it says so rather than leaving it to the next member's alignment.
//
// **`fn_30_B84` is declared, not defined, and so is `__copy`.** After this carve dtk starts a new
// auto unit at 0xB30, and `fn_30_B84` (`.text:0xB84 size 0x130`) is defined by
// `auto_00_00000B30_text.o`, which is in the module's link, so the callee resolves. `__copy` is the
// compiler's own name for the head sub-record's block copy and needs no declaration here; it was
// already an undefined REL symbol before this carve (`powerpc-eabi-nm -u` found it in
// `auto_00_00000130_text.o`, an object the module linked), so this unit adds no new one. Both
// callees are `extern "C"` because that is the name dtk's objects use.
//
// **It returns `self`.** The `mr r3,r30` at 0xB18 puts the destination back in r3, which is what a
// copy assignment returns, so the return type is the pointer and not `void`.
//
// **mw_version is load-bearing**, with the same per-object override as the other five entries in
// this module: at the module default `GC/1.3.2` the same source is still 92 bytes but only **82 of
// the 92 are retail's** (the schedule differs from object offset 0x25 on), at `GC/2.7` it is
// byte-identical, `GC/2.0`/`2.0p1`/`2.5`/`2.6` agree with `GC/2.7`, and `GC/3.0a5` emits 240 bytes
// (all measured on this source). Setting the version on the `Rel(...)` block instead would
// recompile the other module-30 units.
//
// **No dead-strip hazard, and that is measured.** `fn_30_AD4` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but the five relocations
// named above are in dtk's own objects and the 1056C entry names it as well, so the reference
// survives the carve. No `force_active:` entry, no `config/G2ME01/config.yml` change and no
// `symbols.txt` rename: the split claims `.text` only, and keeping the `fn_30_*` name matters
// because dtk's objects call it that way.
//
// **The claim is 0xAD4..0xB30 and not a wider run.** `fn_30_AAC` (0x28) in front of it ends
// exactly at 0xAD4 and `fn_30_B30` behind it is a different function.
//
// The member names say what each word is made of, not what the class is called; a copy assignment
// says what it is made of and nothing else, and these 92 bytes occur nowhere else in the DOL to
// anchor a name.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// The 21 data bytes at +0x00, which retail copies with one `__copy` and never fills with anything
// else, and the three padding bytes at +0x15 that it does not copy. `aligned(4)` is what makes the
// tail padding; see the header comment above.
struct RelSub21 {
  unsigned char b[0x15] __attribute__((aligned(4)));
};

// The 0x10-byte record at +0x20, the type `fn_30_B84` is the copy assignment of. Its layout is
// read off `fn_30_B84`'s own disassembly (0xB84, 0x130) and is *not* measured by this function,
// which only contributes the two `addi`s and the `bl`; what the disassembly shows is that it reads
// +0x04 and +0x08, keeps both, and when either is nonzero allocates `+0x08 * 12` bytes into +0x0C
// before copying `+0x04 >> 2` twelve-byte `lfs`/`lwz`/`lbz` triples out of the source's +0x0C.
// Nothing at +0x00 is read.
struct RelSub10 {
  int w00;
  int w04;
  int w08;
  void* p0C;
};

// Retail's 0x30-byte record at +0x29C of the 0x321-byte class: the 21 copied bytes and their
// padding at +0x00, two words at +0x18 and +0x1C, and the callee's 0x10 bytes at +0x20. Same size
// and same member offsets as `RelRecord30` in `CIngBoostBallGuardian1056C.cpp`, which is the
// member this function is the copy assignment of; the two layouts agree by construction because
// that object is byte-identical to retail and passes `&self->rec29C` here.
struct RelRecord30 {
  RelSub21 sub00;
  int w18;
  int w1C;
  RelSub10 sub20;
};

// `fn_30_B84` by the name dtk's own object gives it; `__copy` is generated by the compiler for the
// head sub-record's assignment and is not declared here.
void fn_30_B84(RelSub10* self, const RelSub10& other);

// .text 0xAD4, 0x5C bytes. `self` is the destination in r3 and `other` the source in r4. The body
// is the three regions the header comment above lists, in that order, and returns `self`.
RelRecord30* fn_30_AD4(RelRecord30* self, const RelRecord30& other) {
  self->sub00 = other.sub00;
  self->w18 = other.w18;
  self->w1C = other.w1C;
  fn_30_B84(&self->sub20, other.sub20);
  return self;
}

#endif
}