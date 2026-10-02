// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8279-8280`, the instructions are retail's own at
// `.text 0x801FD8E0..0x801FD924`, read out of the disc
// (`orig/G2ME01/sys/main.dol`, text section 1: file offset 0x640 at address 0x80003840, size
// 0x3A1C60) and **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in
// the link - the built DOL's 68 bytes at this address are identical to the disc's, measured this
// run.  The body below is the C those bytes are the compilation of.
//
// .text 0x801FD8E0..0x801FD924, 0x44 = 68 bytes, 2 functions:
//
//   fn_801FD8E0    0x801FD8E0  0x20     8 instructions   `rstl::destroy< T >(T*)`, a forwarder
//   fn_801FD900    0x801FD900  0x24     9 instructions   `rstl::destroy_impl< T >(T*)`
//
// **What the two are: `rstl::destroy`'s two halves for one 0x24-byte element.**  Retail names
// neither, so both are read off their bytes and off the twins the already-`Matching`
// `src/MetroidPrime/Player/Carve80004C4C.c` holds for exactly this pair:
//
//   fn_801FD8E0  is a frame and one unconditional `bl fn_801FD900` - no load, no test, no return
//                value - which is exactly the body `include/rstl/construct.hpp:92-95` gives
//                `destroy`: `destroy_impl(in)`.  **The twin is exact**: `fn_80004D3C`
//                (0x80004D3C, 0x20, `Player/Carve80004C4C.c`, a `Matching` unit) is these eight
//                instructions **8 of 8 words, the `bl` word included** (measured this run against
//                the disc): both `bl`s read `48000015`, because each callee sits 0x14 bytes past
//                its own `bl`.  `fn_80004C4C` (0x80004C4C, the same file's other forwarder) is
//                also 8 of 8, and `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) is the
//                same eight instructions apart from its `bl` (`482c5955`).  **The word cannot tell
//                `destroy` from `construct`** - the same eight bytes are both - so what settles
//                this end of the element's life is the callee below and the callers above: the
//                callee is the deleting destructor and the callers walk 0x24-strided elements.
//                Ours at 0x801FD8EC is `48000015`, i.e. 0x801FD8EC + 0x14 = **0x801FD900**, the
//                next function in this claim.
//   fn_801FD900  materialises `li r4,-1` between its `mflr` and its `stw`, and calls `fn_801FD924`
//                taking nothing else from its own argument - that is `destroy_impl`'s `in->~T()`,
//                and the `-1` is MWCC's "destroy, but do not free me afterwards" flag, so
//                `fn_801FD924` is the *deleting* destructor of the element.  **The twin is exact
//                too**: `fn_80004C6C` (0x80004C6C, 0x24, same file) is these nine instructions
//                **8 of 9** word for word, the one differing word being the `bl` at offset 0x10
//                (`4bfffdd1` -> 0x80004A4C there, `48000015` -> 0x801FD924 here).  The `-1` is an
//                `int`, not a `short`: the twin's `li r4,-1` is the same instruction either way,
//                and the callee's own definition takes an `int`.
//
// **The element is 0x24 = 36 bytes, measured from the two retail callers of `fn_801FD8E0`**, each
// of which steps its cursor by 36 and passes one pointer:
//   fn_801FD890 (0x801FD890, 0x50, unclaimed) - `lwz r31,0(r3)` / `mr r30,r4` / `b` to the bottom
//     test / `mr r3,r31` / `bl fn_801FD8E0` at 0x801FD8B4 / `addi r31,r31,0x24` / `lwz r0,0(r30)` /
//     `cmplw r31,r0` / `bne` - the element walk, `destroy_elements()`.  `r4` is copied to r30 and
//     never read again, so the call takes one argument.
//   fn_801FF7EC (0x801FF7EC, in `ScriptObjects/Carve801FF720.cpp`, a `Matching` unit) - the same
//     0x24 stride at 0x801FF7F0..0x801FF81C, `bl fn_801FD8E0` at 0x801FF810, `addi
//     r31,r31,0x24`, and again `r4` dead at the site.  Those two fix the element size at 0x24 and
//     the argument list at one pointer, even though `r4` survives untouched.
// Nothing is asserted here about the class itself: the receiver never appears in either body of
// this claim, so no struct is spelled and the pointer is passed as `void*`.
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
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FD890`
// (0x801FD890, 0x50) ends exactly where this claim starts (0x801FD890 + 0x50 = 0x801FD8E0) and is
// still unclaimed; behind it `fn_801FD924` (0x801FD924, 0x74 = 116 bytes) is the callee above and
// is left to dtk, because it is a real destructor body rather than a forwarder.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C) and `ScriptObjects/Carve801FDB5C.c`
// (0x801FDBE0..0x801FDC88), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood - which
// is where the item's seeder put it and where the two retail callers of `fn_801FD8E0` sit as well.

/** 0x801FD924, `symbols.txt:8281`, 0x74 = 116 bytes: the 0x24-byte element's deleting destructor,
 *  the function this file's `fn_801FD900` calls with the `-1` flag.  Outside this claim and
 *  unclaimed, so it stays retail's: dtk's own `auto_*` object defines it in the DOL.  Its bytes
 *  are this element's shape - the vtable 0x803B7BF0 into +0x0, the member at +0x14 destroyed by
 *  `fn_801FD6F0` (with the same `li r4,-1`), the `rstl::basic_string` at +0x4 released through
 *  `internal_dereference__Q24rstl66basic_string<...>`, and `Free__7CMemoryFPCv(self)` only when the
 *  caller's flag is positive (`extsh. r0,r31 / ble`) - and matching its 0x74 bytes needs the
 *  `.data` vtable as well as the bodies of `fn_801FD6F0` (0x801FD6F0, 0x84) and that string
 *  release.  Declared here, never defined. */
extern void fn_801FD924(void* self, int flag);

/** `fn_801FD900` - retail `.text:0x801FD900`, 0x24 = 36 bytes: `rstl::destroy_impl` for the
 *  0x24-byte element, i.e. the element's destructor called with the "do not free me" flag.  The
 *  twin is `fn_80004C6C` (0x80004C6C, 0x24, `Player/Carve80004C4C.c`), which is these nine
 *  instructions word for word apart from the `bl` target. */
void fn_801FD900(void* self);

void fn_801FD900(void* self) { fn_801FD924(self, -1); }

/** `fn_801FD8E0` - retail `.text:0x801FD8E0`, 0x20 = 32 bytes: `rstl::destroy` for the same
 *  element, a frame and one call to `fn_801FD900` above and nothing else, as
 *  `include/rstl/construct.hpp:92-95` spells it.  Its measured twin is `fn_80004D3C`
 *  (0x80004D3C, 0x20, same file): 8 of 8 words, the `bl` included - see the header. */
void fn_801FD8E0(void* self);

void fn_801FD8E0(void* self) { fn_801FD900(self); }
