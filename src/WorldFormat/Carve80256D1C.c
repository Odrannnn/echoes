// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:10528-10529`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80255B28_text.s:1359-1384` before the claim
// existed (the same bytes are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x80256D1C --stop-address=0x80256D64
// build/G2ME01/main.elf`), and the bodies below are the C those bytes are the compilation of.
// `build/G2ME01/asm/WorldFormat/Carve80256D1C.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x80256D1C..0x80256D64, 0x48 = 72 bytes, 2 functions:
//
//   fn_80256D1C    0x80256D1C  0x20    8 instructions
//   fn_80256D3C    0x80256D3C  0x28   10 instructions
//
// **Both are wrappers, and the pair is a copy constructor for one array element.**  Retail names
// neither of them, so this is read off the call edges and the argument registers, not off a name:
//
//   fn_80256D3C  is a frame, `cmplwi r3,0`, a branch over one call, and nothing else: it is
//                "if the destination exists, construct it", the null test
//                `rstl::construct` performs before running a copy constructor.  Its measured
//                twin is `fn_80004D5C` (0x80004D5C, `src/MetroidPrime/Player/
//                CGameStateBlockConstruct.cpp`, `Matching`), these ten instructions word for
//                word - including the `cmplwi` landing *before* the `stw r0,0x14(r1)`, which is
//                the order mwcceppc uses for this shape - with `bl fn_80004AA0` in place of
//                `bl fn_80256D64`.  That twin's body is literally `if (self != 0) { copy
//                ctor(self, src); }`, which is what is written here.
//   fn_80256D1C  is a frame and one unconditional `bl fn_80256D3C` - no load, no test, no return
//                value - so it is the pair's outer half, whose whole body is the call above.
//                Its measured twin is `fn_80004438` (0x80004438, `src/MetroidPrime/
//                Carve80004438.c`, `Matching`), the same eight instructions with a different
//                `bl` target.
//
// **The two parameters are `(dst, src)`, measured from every call site.**  `fn_80256D1C` has
// three callers in retail (`grep -n 'bl fn_80256D1C' build/G2ME01/asm/`) and all three load two
// registers and walk both by 0x20 per iteration: 0x80256CE4 in `fn_80256CB4` (`mr r3,r30` /
// `mr r4,r31`), 0x80256E94 in `fn_80256E70`, 0x80257008 in `fn_80256FD8`.  So r4 arrives live
// and is forwarded untouched - which costs nothing in the bytes because mwcceppc never emits
// `mr r4,r4`.  **The element is 0x20-strided and that is retail's, not this file's**: the
// caller above them, `fn_80256C30` (0x80256C30), allocates with `slwi r3,r0,5` (count * 32)
// and calls `fn_80256CB4` to fill the array, so the stride is a fact about the range around this
// claim rather than about the copy below.
//
// `fn_80256D64` (0x80256D64, 0x4C, `symbols.txt:10530`) is **above** this claim and is therefore
// declared, never defined here.  Its 19 instructions are a straight member-wise copy of a 0x1E =
// 30-byte aggregate: six `float`s at +0x00..+0x14 and three `short`s at +0x18/+0x1A/+0x1C, from
// r4 to r3, ending in `blr` with no frame.  That is the copy constructor the null test above
// guards, and it is a spelling job of its own, which is why the claim stops where it does.  For
// the DOL, dtk's own `auto_03_80256D64_text` object defines it and the `bl` resolves to retail's
// address; for the port link it is the announced empty stand-in `stub_carve80256d1c_0` in
// `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_80004438_0` makes for
// `Carve80004438.c`'s callee.  **Nothing here claims `fn_80256D64` is decompiled**, and its class
// is not guessed: retail names it nothing, so `void*` is only what the bytes read.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Checked first with
// `python3 tools/check_decl_order.py --unit WorldFormat/Carve80256D1C.c`.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: the range came out of
// `auto_03_80255B28_text`, which starts at 0x80255B28 where `WorldFormat/Carve80255A0C.c` stops,
// so the split leaves `0x80255B28..0x80256D1C` and `0x80256D64..0x80257A14` to dtk's own two
// `auto_*` objects and this file claims the 0x48 bytes between them only.  The directory is
// retail's own, taken from the nearest claimed ranges: below is `WorldFormat/Carve80255A0C.c`
// (0x80255A0C..0x80255B28), above is `WorldFormat/CCollisionPrimitiveData.cpp` (0x80257A14).

/** 0x80256D64, `symbols.txt:10530`, size:0x4C = 76 bytes: the 30-byte aggregate copy
 *  constructor this file's `fn_80256D3C` null-tests and calls.  Declared, never defined here;
 *  the port link's stand-in is `stub_carve80256d1c_0` in `src/MetroidPrime/PortLinkStubs.cpp`,
 *  an announced empty body.  `const void*` for the source because every load in its 19
 *  instructions is a read of r4 and every store is a write of r3 - the bytes read that way and the
 *  pointee type is retail's own naming, which it does not have. */
extern void fn_80256D64(void* self, const void* src);

void fn_80256D3C(void* self, const void* src);
void fn_80256D1C(void* self, const void* src);

/** `fn_80256D3C` - retail `.text:0x80256D3C`, 0x28 = 40 bytes: the null-guarded construct of one
 *  array element, whose only work is the call to `fn_80256D64` above.  The measured twin is
 *  `fn_80004D5C` (`Player/CGameStateBlockConstruct.cpp`, `Matching`), these ten instructions
 *  apart from the `bl` target. */
void fn_80256D3C(void* self, const void* src) {
  if (self != 0) {
    fn_80256D64(self, src);
  }
}

/** `fn_80256D1C` - retail `.text:0x80256D1C`, 0x20 = 32 bytes: the outer half of that pair, a
 *  frame and one call to `fn_80256D3C` and nothing else.  The measured twin is `fn_80004438`
 *  (`src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight instructions with a
 *  different `bl` target.  `src` is forwarded but never read here, which is why retail spends no
 *  instruction on it: see the header's note on the three call sites. */
void fn_80256D1C(void* self, const void* src) { fn_80256D3C(self, src); }