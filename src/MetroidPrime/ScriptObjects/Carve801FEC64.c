// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8316-8317`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801FEAE0_text.s` (and, once this unit is in the
// link, the same 72 bytes out of `build/G2ME01/main.elf`), and the body below is the C those
// bytes are the compilation of.
//
// .text 0x801FEC64..0x801FECAC, 0x48 = 72 bytes, 2 functions:
//
//   fn_801FEC64    0x801FEC64  0x20     8 instructions   `rstl::construct< T >(T*, T const*)`
//   fn_801FEC84    0x801FEC84  0x28     9 instructions   the null-guarded `construct` it calls
//
// **What the two are: the copy half of `rstl::construct` for one element**, both read off the
// twins the already-`Matching` `src/MetroidPrime/Player/` units hold for exactly this pair:
//
//   fn_801FEC64  is a frame and one unconditional `bl fn_801FEC84` - no load, no test, no
//                return value - which is the body `include/rstl/construct.hpp:117-120` gives the
//                in-class `construct`.  **The twin is exact**: `fn_80004D3C` (0x80004D3C, 0x20,
//                `Player/Carve80004C4C.c`, a `Matching` unit, `symbols.txt:97`) is these eight
//                instructions **8 of 8 words, the `bl` word included** (measured this run
//                against `build/G2ME01/main.elf`): both `bl`s read `48 00 00 15`, because each
//                callee sits 0x14 bytes past its own `bl` (0x801FEC70 + 0x14 = 0x801FEC84 here,
//                0x80004D48 + 0x14 = 0x80004D5C there).  The same 8 words are also
//                `fn_801FEE40`'s (`ScriptObjects/Carve801FEE40.c`, a `Matching` unit) and
//                `__sys_free`'s (0x80008A28, `src/MetroidPrime/main.cpp`), and **the word cannot
//                tell `construct` from `destroy`** - what settles this end of the element's life
//                is the callee below and the caller above.
//   fn_801FEC84  tests `r3` against zero (`cmplwi r3,0` / `beq` over the `bl`) and forwards `r4`
//                untouched, so it is `construct`'s own guard - **the twin is exact too**:
//                `fn_801FEE60` (0x801FEE60, 0x28, `ScriptObjects/Carve801FEE40.c`) is these nine
//                instructions **9 of 9 words, the `bl` word included**, and `fn_80004D5C`
//                (0x80004D5C, 0x28, `symbols.txt:98`, whose body is
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, also `Matching`) is
//                8 of 9, the one differing word being the `bl` at offset 0x14 (`4b ff fd 31` ->
//                0x80004AA0 there, `48 00 00 15` -> 0x801FECAC here).  That unit spells the same
//                body as `if (self != 0) { callee(self, src); }`, and the `beq` target 0x18 past
//                the test is the epilogue, so a null destination copies nothing and still
//                returns.
//
// **The element is 0x2C = 44 bytes, measured from retail's own caller of `fn_801FEC64`.**
// `fn_801FEBF8` (0x801FEBF8, 0x6C, unclaimed, the function directly in front of this claim)
// keeps the destination cursor in `r31`, the count in `r28` and the source in `r29`, calls
// `fn_801FEC64` once per element at 0x801FEC30-0x801FEC34 and steps the cursor by 44
// (`addi r31,r31,0x2c` at 0x801FEC38); its loop is entered at the bottom test (`b .L_801FEC3C`
// first), so a zero count constructs nothing and returns.  The `Matching` `Carve801FF8A0.cpp`
// agrees: its `fn_801FF9B8` is the same 0x2C-strided copy walk (`p += 44`, `bl fn_801FEC64` at
// 0x801FF9E8) and its `fn_801FF720.cpp` twin says the stride moved 0x24 -> 0x2C for exactly this
// range.  Nothing is asserted about the class itself: no byte of either body in this claim reads
// a member, so the pointers are passed as `void*`.
//
// **What `fn_801FECAC` is, read off its bytes, since it is outside this claim.**  It is 0x78 = 120
// bytes (`symbols.txt:8318`): a frame, then the `.data` vtable `lbl_803B7BCC`
// (0x803B7BCC, `symbols.txt:18315`) stored at +0x0 and immediately overwritten by the `.data`
// vtable `lbl_803B7BE4`, then `__ct__Q24rstl66basic_string<...>(this+4, src+4)` and
// `fn_801FE8B8(this+0x18, src+0x18)`, and `this` back in `r3`.  So it is the 0x2C-byte element's
// **copy constructor**, which is what this file's `construct` forwards to: it copies `this` from
// `src`.
//
// **This paragraph's reason for the claim stopping at 0x801FECAC was wrong, and the claim no longer
// stops there.**  It said matching those 0x78 bytes "needs two `.data` vtables and the bodies of
// that string constructor and `fn_801FE8B8` (0x801FE8B8, 0xC4), none of which any unit claims".
// Three separate errors in one sentence, each corrected by `ScriptObjects/Carve801FECAC.cpp`, which
// claims the range and is `Matching`: the string copy constructor **is** claimed and `Matching`
// (`rstl/rstl_strings.cpp`, `symbols.txt:13852`, 0x802FF134); the vtables are dtk's `.data` and a
// `Matching` unit needs their *relocations*, not their objects - `extern "C" char lbl_...[];` plus
// taking its address is enough; and `fn_801FE8B8` is a **callee**, which dtk already defines in
// `auto_03_801FEAE0_text.o`, so it needs a symbol and no body anywhere.  **A `Matching` unit needs
// its callees' symbols, not their bodies** - every "claiming X would only move the gap one function
// along" argument in this tree is about the *port*, not the DOL.
//
// The offset was also wrong: the member `fn_801FE8B8` is reached at is **+0x18**, not +0x14 -
// `addi r3,r30,24` at 0x801FECF0 is retail's own.  +0x14 is the sibling 0x24-byte element's
// member offset (`Carve801FD924.cpp`).
//
// For the port's flat link, which does not carry the `auto_*` objects, it needed a stand-in:
// `stub_226` in `src/MetroidPrime/PortLinkStubs.cpp`, **now retired**, because the new unit is in
// `files.cmake` and its `#ifndef __MWERKS__` half defines the same name as a plain `extern "C"`
// function - the host compiler would mangle a constructor the Itanium way, and the port's call site
// is this C file.
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
// Its own unit because a claim may not span an unclaimed gap.  In front of it
// `fn_801FEBF8` (0x801FEBF8, 0x6C = 108 bytes) ends exactly where this claim starts
// (0x801FEBF8 + 0x6c = 0x801FEC64) and is still unclaimed; behind it `ScriptObjects/Carve801FECAC.cpp`
// (0x801FECAC..0x801FED24) is the callee above, and its claim stops before the unclaimed
// `fn_801FED24`.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0) and `ScriptObjects/Carve801FEE40.c`
// (0x801FEE40..0x801FEE88), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood -
// which is where the item's seeder put it and where the retail caller of `fn_801FEC64` sits too.

/** 0x801FECAC, `symbols.txt:8318`, 0x78 = 120 bytes: the 0x2C-byte element's copy constructor,
 *  the function this file's `fn_801FEC84` calls when its destination is non-null.  **Claimed now**
 *  by `ScriptObjects/Carve801FECAC.cpp` (0x801FECAC..0x801FED24, `Matching`), which reproduces all
 *  30 of retail's instructions.  This file's declaration carries the constructor's mangled name
 *  because that is the symbol retail's `bl` has to resolve to, and that reference is what proves
 *  the definition reaches the DOL link.  Retail named the function `fn_801FECAC`, a placeholder;
 *  `symbols.txt:8318` is renamed to `__ct__21SCarve801FECACElementFRC21SCarve801FECACElement`
 *  because a constructor's symbol is mangled, and that rename is load bearing for the matched count
 *  rather than for the hash - see that unit's header.  Declared here, never defined: it is a C++
 *  copy constructor and this file is C. */
extern void __ct__21SCarve801FECACElementFRC21SCarve801FECACElement(void* self, const void* src);

/** `fn_801FEC84` - retail `.text:0x801FEC84`, 0x28 = 40 bytes: `rstl::construct`'s null guard on
 *  the destination, forwarding `src` in `r4` untouched.  The twin is `fn_801FEE60` (0x801FEE60,
 *  0x28, `ScriptObjects/Carve801FEE40.c`, a `Matching` unit), which is these nine instructions
 *  word for word; against `fn_80004D5C` (`Player/CGameStateBlockConstruct.cpp`, also `Matching`)
 *  only the `bl` target differs. */
void fn_801FEC84(void* self, const void* src);

void fn_801FEC84(void* self, const void* src) {
  if (self != 0) {
    __ct__21SCarve801FECACElementFRC21SCarve801FECACElement(self, src);
  }
}

/** `fn_801FEC64` - retail `.text:0x801FEC64`, 0x20 = 32 bytes: `rstl::construct` for the same
 *  element, a frame and one call to `fn_801FEC84` above and nothing else, as
 *  `include/rstl/construct.hpp:117-120` spells it.  Its measured twins are `fn_80004D3C`
 *  (`Player/Carve80004C4C.c`) and `fn_801FEE40` (`ScriptObjects/Carve801FEE40.c`): 8 of 8 words,
 *  the `bl` included - see the header. */
void fn_801FEC64(void* dest, const void* src);

void fn_801FEC64(void* dest, const void* src) { fn_801FEC84(dest, src); }
