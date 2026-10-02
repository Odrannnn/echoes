// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8271-8272`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801FA3CC_text.s` before the claim existed (verified this
// run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FD638
// --stop-address=0x801FD67C build/G2ME01/main.elf`), and the body below is the C those bytes are
// the compilation of.
//
// .text 0x801FD638..0x801FD67C, 0x44 = 68 bytes, 2 functions:
//
//   fn_801FD638    0x801FD638  0x20     8 instructions   `rstl::destroy< T >(T*)`, a forwarder
//   fn_801FD658    0x801FD658  0x24     9 instructions   `rstl::destroy_impl< T >(T*)`
//
// **What the two are: `rstl::destroy`'s two halves for one element type.**  Retail calls them
// nowhere inside this claim and neither body touches its argument beyond passing it on, so both
// are read off the twin pair rather than off their own code:
//
//   fn_801FD638  is a frame and one unconditional `bl fn_801FD658` - no load, no test, no return
//                value - which is exactly the body `include/rstl/construct.hpp:95-99` gives
//                `destroy`: `destroy_impl(in)`.  **The twin is exact**: `fn_80004D3C` (0x80004D3C,
//                0x20, `src/MetroidPrime/Player/Carve80004C4C.c`, a `Matching` unit) is these
//                eight instructions byte for byte with `bl fn_80004D5C` in place of the `bl`, and
//                so is `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) apart from its own
//                `bl`.
//   fn_801FD658  materialises `li r4,-1` between its `mflr` and its `stw`, and calls
//                `fn_801FD67C` taking nothing else from its own argument - that is
//                `destroy_impl`'s `in->~T()`, and the `-1` is MWCC's "destroy, but do not free me
//                afterwards" flag, so `fn_801FD67C` is the *deleting* destructor of the element.
//                **The twin is exact too**: `fn_80004C6C` (0x80004C6C, 0x24, same file) is these
//                nine instructions word for word with `bl fn_80004A4C` in place of the `bl`.  The
//                `-1` is an `int`, not a `short`: the twin's `li r4,-1` is the same instruction
//                either way, and the callee's own definition takes an `int`.
//
// **The element is 0x24 = 36 bytes, measured from two retail loops that call only
// `fn_801FD638`** and each step their cursor by 36:
//   fn_801FD5E8 (0x801FD5E8, 0x50, unclaimed) - `lwz r31,0(r3)` / `b 0x801FD614` /
//     `mr r3,r31` / `bl fn_801FD638` at 0x801FD60C / `addi r31,r31,36` / `lwz r0,0(r30)` /
//     `cmplw r31,r0` / `bne`.
//   fn_801FF66C (0x801FF66C, already ported in `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`,
//     a `Matching` unit) - the same nine instructions at 0x801FF68C..0x801FF69C, `bl fn_801FD638`
//     at 0x801FF690 and `addi r31,r31,36`.
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
// casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FD5E8`
// (0x801FD5E8, 0x50) ends exactly where this claim starts (0x801FD5E8 + 0x50 = 0x801FD638) and is
// still unclaimed; behind it `fn_801FD67C` (0x801FD67C, 0x74 = 116 bytes) is the element's real
// destructor and is left to dtk.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88) and `ScriptObjects/Carve801FEEF0.c`
// (0x801FEEF0..0x801FEEF8), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood - which
// is where the item's seeder put it and where the two retail callers of `fn_801FD638` sit as well.

/** 0x801FD67C, `symbols.txt:8273`, 0x74 = 116 bytes: the element's deleting destructor, the
 *  function this file's `fn_801FD658` calls with the `-1` flag.  Outside this claim and
 *  unclaimed, so it stays retail's: dtk's own `auto_*` object defines it in the DOL.  Its bytes
 *  are this element's shape - the vtable 0x803B7BFC into +0x0, a `rstl::basic_string` at +0x4
 *  (released through `internal_dereference__Q24rstl66basic_string<...>`), a member at +0x14
 *  destroyed by `fn_801FD6F0`, and `Free__7CMemoryFPCv(self)` only when the caller's flag is
 *  positive - and matching its 0x74 bytes needs the `.data` vtable as well as the bodies of
 *  `fn_801FD6F0` and `fn_801FD774`.  Declared here, never defined. */
extern void fn_801FD67C(void* self, int flag);

/** `fn_801FD658` - retail `.text:0x801FD658`, 0x24 = 36 bytes: `rstl::destroy_impl` for the
 *  36-byte element, i.e. the element's destructor called with the "do not free me" flag.  The
 *  twin is `fn_80004C6C` (0x80004C6C, 0x24, `Player/Carve80004C4C.c`), which is these nine
 *  instructions word for word. */
void fn_801FD658(void* self);

void fn_801FD658(void* self) { fn_801FD67C(self, -1); }

/** `fn_801FD638` - retail `.text:0x801FD638`, 0x20 = 32 bytes: `rstl::destroy` for the same
 *  element, a frame and one call to `fn_801FD658` above and nothing else, as
 *  `include/rstl/construct.hpp:95-99` spells it.  Its measured twin is `fn_80004D3C`
 *  (0x80004D3C, 0x20, same file), byte for byte apart from the `bl` target. */
void fn_801FD638(void* self);

void fn_801FD638(void* self) { fn_801FD658(self); }
