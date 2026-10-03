// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8292`, the instructions are retail's own, read from
// the disc (`orig/G2ME01/sys/main.dol`, text section 1: file offset 0x640 at address 0x80003840)
// and **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link -
// the built DOL's 36 bytes at this address are identical to the disc's, measured this run before
// the claim existed.  They are also still readable in
// `build/G2ME01/asm/auto_03_801FDC88_text.s:9-18`, and `build/G2ME01/asm/MetroidPrime/
// ScriptObjects/Carve801FDC88.s` is this unit's own generated listing, not the retail one - it is
// our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x801FDC88..0x801FDCAC, 0x24 = 36 bytes, 1 function:
//
//   fn_801FDC88    0x801FDC88  0x24     9 instructions
//
// **What it is: `rstl::destroy_impl< T >(T*)`** - a frame, `li r4,-1`, one call taking nothing
// else from its own argument, and the standard epilogue.  That is `in->~T()` with the `-1` being
// MWCC's "destroy, but do not free me afterwards" flag, whose spelling is `include/rstl/
// construct.hpp:84-90` (`destroy_impl`, `in->~T()` at :89).  Retail names the function nothing, so
// this is read off its call edge and its argument registers, and the identification rests on a
// **byte-for-byte twin** that retail *does* name, elsewhere in the DOL:
//
//   fn_80004458  (0x80004458, 0x24, `MetroidPrime/Carve80004438.c`, a `Matching` unit measured
//                2/2 matched functions in `build/report.json`) is these nine instructions **9 of 9
//                words, the `bl` word included**.  Both `bl`s read `48000015` because in each case
//                the callee sits 0x14 bytes past its own `bl` - 0x801FDC98 + 0x14 = 0x801FDCAC here,
//                0x80004468 + 0x14 = 0x8000447C there - so not one byte of the two objects
//                differs.  (Measured this run with
//                `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FDC88
//                --stop-address=0x801FDCAC build/G2ME01/main.elf` against the same command at
//                0x80004458.)  `fn_80004C6C` (0x80004C6C, `Player/Carve80004C4C.c`) is the same nine
//                instructions again, and `fn_801FDAC4` (0x801FDAC4,
//                `ScriptObjects/Carve801FDAA4.c`, `Matching` 2/2) is this same `destroy_impl` shape
//                0x50 bytes below this claim.
//
// **The caller fixes which half of the pair this is.**  `fn_801FDC68` (0x801FDC68, 0x20,
// `ScriptObjects/Carve801FDB5C.c`, a `Matching` unit, 3/3) is a frame and one unconditional
// `bl fn_801FDC88` at 0x801FDC74 and nothing else - no load, no test, no return value - which is
// `rstl::destroy`, whose whole body is `destroy_impl(in)` (`include/rstl/construct.hpp:92-95`).
// `fn_801FDC18` (0x801FDC18, 0x50, same file) walks a block in strides of `addi r31,r31,0x30`
// calling `fn_801FDC68` on each element, so the element is 0x30 = 48 bytes and its destructor is
// this file's callee.  Nothing is asserted here about the class: the receiver never appears in
// either body, so no struct is spelled and the pointer is passed as `void*`.
//
// **`fn_801FDCAC` (0x801FDCAC, `symbols.txt:8293`, 0x94 = 148 bytes) is the element's deleting
// destructor, and it is left to dtk.**  Its bytes are this element's shape: the vtable
// `lbl_803B7BD8` into +0x0, a `CToken` destructor at +0x24 (`__dt__6CTokenFv`, called only when
// `lbz r0,0x2c(r30)` is non-zero), `fn_801FD6F0` at +0x14 with the same `li r4,-1`, an
// `rstl::basic_string` at +0x4 released through `internal_dereference__Q24rstl66basic_string<...>`,
// and `Free__7CMemoryFPCv(self)` only when the caller's flag is positive (`extsh. r0,r31 / ble`) -
// the last of those is what makes it *deleting*, and the `extsh` is why the flag is an `int` and
// not a `short`.  Its +0x24 member and its 0x30 stride agree, but matching its 148 bytes needs
// that `basic_string` release and `fn_801FD6F0` as well, which is a job of its own; the claim
// stops here rather than span it.  For the DOL nothing is needed - dtk's own
// `auto_03_801FDC88_text.o` defines it.  For the port link it is the announced stand-in
// `stub_carve801fdc88_0` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_178` made
// for the symbol this file now claims for real (that stand-in is deleted).  Nothing here claims
// `fn_801FDCAC` is decompiled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order is trivially satisfied, and
// `python3 tools/check_decl_order.py` is the check either way.
//
// Retail names this function nothing.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*`, which is compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it
// `ScriptObjects/Carve801FDB5C.c` claims 0x801FDBE0..0x801FDC88 and ends exactly where this
// claim starts; behind it `fn_801FDCAC`..`fn_801FEA98` is unclaimed and stays retail's.  The claim
// is therefore exactly this one function's 0x24 bytes and nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/ScriptObjects/Carve801FDB5C.c` (0x801FDBE0..0x801FDC88), above is
// `MetroidPrime/ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0), and the two retail
// callers of `fn_801FDC88`/`fn_801FDCAC` sit in the same `ScriptObjects/` neighbourhood.

/** 0x801FDCAC, `symbols.txt:8293`, 0x94 = 148 bytes: the 0x30-byte element's deleting
 *  destructor, the function this file's `fn_801FDC88` calls with the `-1` flag.  Outside this
 *  claim and unclaimed, so it stays retail's: dtk's own `auto_03_801FDC88_text.o` defines it in
 *  the DOL, and the port link's stand-in is `stub_carve801fdc88_0` in
 *  `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body.  `int`, not `short`: the flag
 *  word is read with `extsh`, so it is the low half of the register `li r4,-1` wrote, and the
 *  twin's own `li r4,-1` is the same instruction either way. */
extern void fn_801FDCAC(void* self, int flag);

/** `fn_801FDC88` - retail `.text:0x801FDC88`, 0x24 = 36 bytes, 9 instructions: `destroy_impl` for
 *  the 0x30-byte element this unit's caller walks, i.e. the element's destructor called with the
 *  "do not free me" flag.  The measured twin is `fn_80004458` (0x80004458, 0x24,
 *  `MetroidPrime/Carve80004438.c`, `Matching`), which is these nine instructions 9 of 9 words -
 *  see the header. */
void fn_801FDC88(void* self);

void fn_801FDC88(void* self) { fn_801FDCAC(self, -1); }