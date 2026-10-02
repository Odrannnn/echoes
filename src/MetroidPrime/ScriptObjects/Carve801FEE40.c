// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8321-8322`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801FDC88_text.s` (and, once this unit is in the
// link, the same 72 bytes out of `build/G2ME01/main.elf`), and the body below is the C those
// bytes are the compilation of.
//
// .text 0x801FEE40..0x801FEE88, 0x48 = 72 bytes, 2 functions:
//
//   fn_801FEE40    0x801FEE40  0x20     8 instructions   `rstl::construct< T >(T*, T const*)`
//   fn_801FEE60    0x801FEE60  0x28     9 instructions   the null-guarded `construct` it calls
//
// **What the two are: the copy half of `rstl::construct` for one element**, both read off the
// twins the already-`Matching` `src/MetroidPrime/Player/` units hold for exactly this pair:
//
//   fn_801FEE40  is a frame and one unconditional `bl fn_801FEE60` - no load, no test, no
//                return value - which is the body `include/rstl/construct.hpp:117-120` gives the
//                in-class `construct`.  **The twin is exact**: `fn_80004D3C` (0x80004D3C, 0x20,
//                `Player/Carve80004C4C.c`, a `Matching` unit) is these eight instructions
//                **8 of 8 words, the `bl` word included** (measured this run against
//                `build/G2ME01/main.elf`): both `bl`s read `48 00 00 15`, because each callee
//                sits 0x14 bytes past its own `bl` (0x801FEE4C + 0x14 = 0x801FEE60 here,
//                0x80004D48 + 0x14 = 0x80004D5C there).  `fn_80004C4C` (0x80004C4C, the same
//                file's `destroy` forwarder) and `__sys_free` (0x80008A28,
//                `src/MetroidPrime/main.cpp`) are the same eight instructions too, and **the
//                word cannot tell `construct` from `destroy`** - what settles this end of the
//                element's life is the callee below and the caller above: the callee tests its
//                destination and the caller is a copy walk.
//   fn_801FEE60  tests `r3` against zero (`cmplwi r3,0` / `beq` over the `bl`) and forwards `r4`
//                untouched, so it is `construct`'s own guard - **the twin is exact too**:
//                `fn_80004D5C` (0x80004D5C, 0x28, `symbols.txt:98`, whose body is
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, also `Matching`) is these
//                nine instructions **8 of 9 words**, the one differing word being the `bl` at
//                offset 0x14 (`4b ff fd 31` -> 0x80004AA0 there, `48 00 00 15` -> 0x801FEE88 here).
//                That unit spells the same body as `if (self != 0) { callee(self, src); }`, and
//                the `beq` target 0x18 past the test is the epilogue, so a null destination
//                copies nothing and still returns.
//
// **The element is 0x24 = 36 bytes, measured from retail's own caller of `fn_801FEE40`.**
// `fn_801FF6B8` (0x801FF6B8, in `ScriptObjects/Carve801FF5A0.cpp`, a `Matching` unit) sets
// `mr r3,r30` / `mr r4,r31` and `bl fn_801FEE40` at 0x801FF6E0-0x801FF6E8, then steps both
// cursors by 36 (`addi` 0x801FF6F0-0x801FF6F8) - the `uninitialized_copy` walk of a 0x24-byte
// element.  `fn_801FEE88`'s own body agrees: it copies an `rstl::basic_string` at +0x4 (0x14
// bytes) and calls `fn_801FE8B8` on +0x14, which is where a 0x24-byte object ends.  Nothing is
// asserted about the class itself: no byte of either body in this claim reads a member, so the
// pointers are passed as `void*`.
//
// **What `fn_801FEE88` is, read off its bytes, since it is outside this claim.**  It is 0x68 =
// 104 bytes (`symbols.txt:8323`): a frame, then the `.data` vtable `lbl_803B7BCC`
// (0x803B7BCC, `symbols.txt:18343`) stored at +0x0 and immediately overwritten by the
// `.data` vtable `lbl_803B7BFC` (0x803B7BFC, `symbols.txt:18347`) - the base-class constructor
// inside the derived one - then `__ct__Q24rstl66basic_string<...>(this+4, src+4)` and
// `fn_801FE8B8(this+0x14, src+0x14)`, and `this` back in `r3`.  So it is the element's **copy
// constructor**, which is what `construct` is being spelled over: it copies `this` from `src`.
// It is not claimed here - matching its 0x68 bytes needs two `.data` vtables and the bodies of
// that string constructor and `fn_801FE8B8` (0x801FE8B8, 0xC4), none of which any unit claims -
// so it stays retail's and dtk emits it from `auto_03_801FDC88_text.o`.  For the port's flat
// link, which does not carry the `auto_*` objects, it needs a stand-in:
// `stub_200` in `src/MetroidPrime/PortLinkStubs.cpp`.  The claim stops at 0x801FEE88 rather than
// taking it, and that is the trade `stub_199` already makes one function along for
// `Carve801FDAA4.c`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*`, which is compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FEDD4`
// (0x801FEDD4, 0x6C = 108 bytes) ends exactly where this claim starts (0x801FEDD4 + 0x6c =
// 0x801FEE40) and is still unclaimed; behind it `fn_801FEE88` is the callee above.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `ScriptObjects/Carve801FEEF0.c`
// (0x801FEEF0..0x801FEEF8), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood -
// which is where the item's seeder put it and where the retail caller of `fn_801FEE40` sits too.

/** 0x801FEE88, `symbols.txt:8323`, 0x68 = 104 bytes: the 0x24-byte element's copy constructor,
 *  the function this file's `fn_801FEE60` calls when its destination is non-null.  Outside this
 *  claim and unclaimed, so it stays retail's: dtk's own `auto_03_801FDC88_text.o` defines it in
 *  the DOL.  Its bytes are the element's shape (two `.data` vtables into +0x0, the
 *  `rstl::basic_string` at +0x4, `fn_801FE8B8` on the member at +0x14) and matching them needs
 *  both vtables and those two bodies, themselves unclaimed.  Declared here, never defined. */
extern void fn_801FEE88(void* self, const void* src);

/** `fn_801FEE60` - retail `.text:0x801FEE60`, 0x28 = 40 bytes: `rstl::construct`'s null guard on
 *  the destination, forwarding `src` in `r4` untouched.  The twin is `fn_80004D5C` (0x80004D5C,
 *  0x28, `symbols.txt:98`, `Player/CGameStateBlockConstruct.cpp`, a `Matching` unit), which is
 *  these nine instructions word for word apart from the `bl` target. */
void fn_801FEE60(void* self, const void* src);

void fn_801FEE60(void* self, const void* src) {
  if (self != 0) {
    fn_801FEE88(self, src);
  }
}

/** `fn_801FEE40` - retail `.text:0x801FEE40`, 0x20 = 32 bytes: `rstl::construct` for the same
 *  element, a frame and one call to `fn_801FEE60` above and nothing else, as
 *  `include/rstl/construct.hpp:117-120` spells it.  Its measured twin is `fn_80004D3C`
 *  (0x80004D3C, 0x20, `Player/Carve80004C4C.c`): 8 of 8 words, the `bl` included - see the
 *  header. */
void fn_801FEE40(void* dest, const void* src);

void fn_801FEE40(void* dest, const void* src) { fn_801FEE60(dest, src); }
