// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8311-8312`, the instructions are retail's own at
// `.text 0x801FEA98..0x801FEAE0`, read out of the disc
// (`orig/G2ME01/sys/main.dol`) with `python3 tools/dol_read.py 0x801FEA98 0x48` this run and
// **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link.  The
// body below is the C those bytes are the compilation of.
//
// .text 0x801FEA98..0x801FEAE0, 0x48 = 72 bytes, 2 functions:
//
//   fn_801FEA98    0x801FEA98  0x20     8 instructions   `rstl::construct< T >(T*)`, a forwarder
//   fn_801FEAB8    0x801FEAB8  0x28    10 instructions   `rstl::construct< T >(T*, T const&)`
//
// **What the two are: `rstl::construct`'s two halves for the 0x24-byte script-object element whose
// copy constructor is `fn_801FEAE0`.**  Retail names neither, so both are read off their bytes and
// off the twins the already-`Matching` `src/MetroidPrime/Player/Carve80004C4C.c` and
// `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp` hold for exactly this pair:
//
//   fn_801FEA98  is a frame and one unconditional `bl fn_801FEAB8` - no load, no test, no return
//                value - which is the forwarder half of `rstl::construct`, spelled in the twins as
//                `construct(self, src)` with nothing but the call in the body.  **The twin is
//                exact**: `fn_80004D3C` (0x80004D3C, 0x20, `Player/Carve80004C4C.c`, a `Matching`
//                unit, where it is documented as `rstl::construct< SGameStateBlock >`'s forwarder)
//                is these eight instructions **8 of 8 words, the `bl` word included** (both disc
//                reads this run): both `bl`s read `48000015`, because each callee sits 0x14 bytes
//                past its own `bl` (0x801FEAA4 + 0x14 = 0x801FEAB8 here, 0x80004D48 + 0x14 =
//                0x80004D5C there).  `fn_80004C4C` (the same file's `destroy` forwarder) and
//                `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) are the same eight
//                instructions with a different `bl`, so the word alone does not settle the
//                direction of the element's life - the null test below and the callers above do.
//   fn_801FEAB8  is the null-guarded half: `cmplwi r3,0x0` / `beq` over the destination and then
//                one call with **both arguments untouched** - no `li r4,-1` anywhere, which is what
//                separates `construct` from `destroy_impl` (the twins' `fn_80004C6C` / `fn_801FD900`
//                both materialise the `-1` "do not free me" flag before their call).  That is
//                `new (dest) T(src)` as `include/rstl/construct.hpp:53-56` spells it.  **The twin is
//                exact too**: `fn_80004D5C` (0x80004D5C, 0x28, `Player/CGameStateBlockConstruct.cpp`,
//                a `Matching` unit) is these ten instructions **9 of 10** word for word, the one
//                differing word being the `bl` at offset 0x14 (`4bfffd31` -> 0x80004AA0 there,
//                `48000015` -> 0x801FEAE0 here).
//
// **The element is 0x24 = 36 bytes and the call takes `(destination, source)`, measured from the
// two retail callers of `fn_801FEA98`**, each of which steps its destination cursor by 36:
//   fn_801FEA2C (0x801FEA2C, 0x6C, unclaimed) - a count-first loop that `bl fn_801FEA98` at
//     0x801FEA64 with `mr r3,r31` / `mr r4,r29`, then `addi r30,r30,0x1` / `addi r31,r31,0x24` at
//     0x801FEA68..0x801FEA6C - `uninitialized_copy_n`'s shape, `construct` once per element.
//   fn_801FF838 (0x801FF838, in `ScriptObjects/Carve801FF720.cpp`, a `Matching` unit) - `bl
//     fn_801FEA98` at 0x801FF868 with `mr r3,r30` / `mr r4,r31` at 0x801FF860..0x801FF864 and both
//     cursors stepped by 36 afterwards (`addi r31,r31,36` / `addi r30,r30,36`).  That unit's own
//     comment calls this symbol "the 0x24-byte element's copy, called with the destination first".
//   The callee settles it as well: `fn_801FEAE0` (0x801FEAE0, 0x68, unclaimed) stores the `.data`
//     vtable pair `lbl_803B7BCC` / `lbl_803B7BF0` (0x803B7BCC, 0x803B7BF0) into +0x0, copies the
//     `rstl::basic_string` at +0x4 through
//     `__ct__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>`, runs
//     `fn_801FE8B8` (0x801FE8B8, 0xC4) over the member at +0x14 and returns the destination - a copy
//     constructor, i.e. the `T(src)` this file's guard calls.
// Nothing is asserted here about the class itself: no member of it is read or written in either
// body of this claim, so no struct is spelled and both pointers are passed as `void*`.
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
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FEA2C`
// (0x801FEA2C, 0x6C) ends exactly where this claim starts (0x801FEA2C + 0x6C = 0x801FEA98) and is
// still unclaimed; behind it `fn_801FEAE0` (0x801FEAE0, 0x68) starts exactly where the claim ends
// and is left to dtk, because matching its bytes needs the `.data` vtable pair above plus the
// `basic_string` copy constructor and `fn_801FE8B8`'s 0xC4 bytes, none of them claimed.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `ScriptObjects/Carve801FEE40.c`
// (0x801FEE40..0x801FEE88), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood - which
// is where the item's seeder put it and where the two retail callers of `fn_801FEA98` sit as well.

/** 0x801FEAE0, `symbols.txt:8313`, 0x68 = 104 bytes: the 0x24-byte element's copy constructor, the
 *  function this file's `fn_801FEAB8` calls.  Outside this claim and unclaimed, so it stays
 *  retail's: dtk's own `auto_*` object defines it in the DOL.  Its bytes are this element's shape -
 *  the vtable pair 0x803B7BCC / 0x803B7BF0 into +0x0, the `rstl::basic_string` at +0x4 copied
 *  through `__ct__Q24rstl66basic_string<...>`, the member at +0x14 built by `fn_801FE8B8`
 *  (0x801FE8B8, 0xC4) - and matching its 0x68 bytes needs the `.data` pair as well as that string
 *  copy and `fn_801FE8B8`'s body.  Declared here, never defined. */
extern void fn_801FEAE0(void* self, const void* src);

/** `fn_801FEAB8` - retail `.text:0x801FEAB8`, 0x28 = 40 bytes: `rstl::construct` for the 0x24-byte
 *  element, i.e. the placement-new guard `if (dest != 0) T(dest, src)`.  The twin is `fn_80004D5C`
 *  (0x80004D5C, 0x28, `Player/CGameStateBlockConstruct.cpp`), which is these ten instructions word
 *  for word apart from the `bl` target. */
void fn_801FEAB8(void* dest, const void* src);

void fn_801FEAB8(void* dest, const void* src) {
  if (dest != 0) {
    fn_801FEAE0(dest, src);
  }
}

/** `fn_801FEA98` - retail `.text:0x801FEA98`, 0x20 = 32 bytes: the in-class `construct` forwarder
 *  for the same element, a frame and one call to `fn_801FEAB8` above and nothing else, as
 *  `include/rstl/construct.hpp:74-77` spells it.  Its measured twin is `fn_80004D3C`
 *  (0x80004D3C, 0x20, `Player/Carve80004C4C.c`): 8 of 8 words, the `bl` included - see the
 *  header. */
void fn_801FEA98(void* dest, const void* src);

void fn_801FEA98(void* dest, const void* src) { fn_801FEAB8(dest, src); }
