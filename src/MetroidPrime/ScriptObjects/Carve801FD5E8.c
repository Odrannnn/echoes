// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8270`, the instructions are retail's own, read this
// run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FD5E8
// --stop-address=0x801FD638 build/G2ME01/main.elf`, and the body below is the C those bytes are
// the compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_801FA3CC_text.s`, which is the listing that unit still carries.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FD5E8.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x801FD5E8..0x801FD638, 0x50 = 80 bytes, 1 function:
//
//   fn_801FD5E8    0x801FD5E8  0x50    20 instructions
//
// **What it is: `rstl::destroy_impl< It, It >` for one element type**, written out as C.  Retail
// names it only as the `fn_<addr>` placeholder, so it is read off a twin rather than off its own
// code - and the twin is exact.  `fn_801FDC18` (0x801FDC18, 0x50 = 80 bytes,
// `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`, a `Matching` unit) is these twenty
// instructions word for word, with exactly two differences measured this run by disassembling
// both ranges of `build/G2ME01/main.elf` and comparing the decoded words:
//   0x801FD60C  `bl 0x801FD638`  vs  0x801FDC3C  `bl 0x801FDC68`   (the callee)
//   0x801FD610  `addi r31,r31,36` vs  0x801FDC40  `addi r31,r31,48`  (the element stride)
// The remaining eighteen words - frame, the `lwz r31,0(r3)` / `mr r30,r4` cursor setup, the
// `b` over the loop head, the `lwz r0,0(r30)` / `cmplw r31,r0` / `bne` bound, and the whole
// epilogue - are identical, including the two branch displacements, which is what fixes the shape.
//
// **The `It` arguments are load-bearing, and they are why this is a struct at all.**  MWCC passes
// a struct parameter by reference and copies it into the callee's own frame: that is why this body
// dereferences r3 and r4, why it keeps r30 = r4 and reloads `lwz r0,0(r30)` on every iteration,
// and why the begin cursor is loaded once into r31 and walked with `addi r31,r31,0x24`.  Spelled
// with plain `char*` parameters instead, the body is still 20 instructions but a different shape -
// `fn_801FF66C` (0x801FF66C, 0x4C, `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`, a
// `Matching` unit) is that spelling, and it differs from this one in six words, all of it the
// cursor never being spilled: `mr r31,r3` and `mr r30,r4` in place of `lwz r31,0(r3)` and
// `stw r30,8(r1)`, and `cmplw r31,r30` in place of the reload.  So the by-value one-pointer struct
// is not decoration; it is what produces these bytes.
//
// **The element is 0x24 = 36 bytes**, measured twice independently and both times off this very
// callee: this loop's own `addi r31,r31,36` at 0x801FD610, and `fn_801FF66C`'s `addi r31,r31,36`
// at 0x801FF694, which calls the same `fn_801FD638`.  Nothing is asserted about the class: the
// receiver never appears in this body, so no struct is spelled and the pointer is passed as
// `void*`.
//
// **Its callee is real, not a stand-in.**  `fn_801FD638` (0x801FD638, 0x20) is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FD638.c`, a `Matching` unit, and that file's own header
// records how that element's size was measured from this function and from `fn_801FF66C`.  So
// `bl 0x801FD60C` lands on a definition we wrote, in retail's bytes, and nothing in
// `src/MetroidPrime/PortLinkStubs.cpp` stands in for either symbol.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  With one function
// the rule cannot be violated here, and `tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/Carve801FD5E8` says so, but it is the rule that keeps a later second
// function in this unit from breaking it.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other
// source in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the
// explicit casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  Its single function ends exactly
// where the next claim starts (0x801FD5E8 + 0x50 = 0x801FD638 =
// `ScriptObjects/Carve801FD638.c`), so nothing is split here; behind that claim `fn_801FD67C`
// (0x801FD67C, 0x74 = 116 bytes) is the element's real destructor and is left to dtk, and in
// front of it `fn_801FD5B0` (0x801FD5B0, 0x38 = 56 bytes, the retail caller at 0x801FD5D4) is
// unclaimed and stays retail's.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `ScriptObjects/Carve801FD638.c`
// (0x801FD638..0x801FD67C), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood, and
// the twin that fixes the shape lives there too.

/** The one pointer of `rstl::pointer_iterator`: `current`, protected in retail's C++ and named
 *  there too.  Retail names the iterator's class only inside mangled twin symbols elsewhere in
 *  the DOL; what these bytes need of it is exactly this, that it is a struct of one pointer
 *  passed by value. */
struct SCarve801FD5E8Iterator {
  void* current;
};

/** `fn_801FD638` - retail `.text:0x801FD638`, 0x20 = 32 bytes: `rstl::destroy` for the 36-byte
 *  element, defined for real by `src/MetroidPrime/ScriptObjects/Carve801FD638.c`. */
extern void fn_801FD638(void* self);

void fn_801FD5E8(struct SCarve801FD5E8Iterator begin, struct SCarve801FD5E8Iterator end) {
  char* cur = (char*)begin.current;
  while (cur != (char*)end.current) {
    fn_801FD638(cur);
    cur += 0x24;
  }
}