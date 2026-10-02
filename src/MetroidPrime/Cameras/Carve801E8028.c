// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7867-7868`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801E7CB0_text.s:285-305` while this range was
// still unclaimed, and the bodies below are the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/Cameras/Carve801E8028.s` is this unit's own generated listing,
// not retail's - it can confirm, never establish, what retail had.
//
// .text 0x801E8028..0x801E8070, 0x48 = 72 bytes, 2 functions:
//
//   fn_801E8028    0x801E8028  0x20     8 instructions
//   fn_801E8048    0x801E8048  0x28     10 instructions
//
// **What the two are: the two-step `rstl::construct` chain for one element of
// `CCameraShakeManager`'s shake array.**  Retail names neither of them, so both are read off
// their call edges and their argument registers, and each one's twin is measured:
//
//   fn_801E8028  a frame, one unconditional `bl fn_801E8048` with r3 and r4 forwarded
//                untouched, and an epilogue - no load, no test, no return value.  That is the
//                outer half of the null-guarded `construct`
//                (`include/rstl/construct.hpp:117-120`), whose whole body is the inner call.
//                **Its twin is `fn_80004D3C`** (0x80004D3C, 0x20 = 32 bytes,
//                `src/MetroidPrime/Player/Carve80004C4C.c:88`), the same eight instructions with
//                `bl fn_80004D5C` in place of this `bl`; that unit is `Matching` and its own
//                generated listing `build/G2ME01/asm/MetroidPrime/Player/Carve80004C4C.s:87-96`
//                has the identical schedule `stwu / mflr / stw / bl / lwz / mtlr / addi / blr`.
//   fn_801E8048  a frame, `cmplwi r3,0`, the `stw r0,0x14(r1)`, `beq` to the epilogue and one
//                `bl fn_801E8070` - the null test on the destination, then the call.  **The twin
//                is exact**: `fn_80004D5C` (0x80004D5C, 0x28 = 40 bytes,
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, a `Matching` unit) is
//                these ten instructions word for word, including the `cmplwi` *before* the
//                `stw`, with `bl fn_80004AA0` in place of `bl fn_801E8070` - see
//                `build/G2ME01/asm/MetroidPrime/Player/CGameStateBlockConstruct.s:8-19`.
//
// **What the chain is called with.**  The one call site, 0x801E8004 in `fn_801E7FE0`
// (0x801E7FE0, 0x48 = 72 bytes), computes `r3 = self + count * 0x104 + 4` from
// `lwz r0,0x0(r31)` / `mulli r0,r0,0x104` / `add r3,r31,r0` / `addi r3,r3,0x4` and passes the
// stack temporary at `r1+0xFC` in r4; on return it does `lwz r3,0x0(r31)` / `addi r0,r3,0x1`
// / `stw r0,0x0(r31)`.  So the pair is `construct(&self->elements[self->count], &copy)` followed
// by `++self->count` - one element appended - and the 0x104 stride is the element size.
// `fn_801E8070` (0x801E8070, 0x64 = 100 bytes), the callee, is that element's copy constructor:
// it copies the three words at +0/+4/+8, `bl __ct__17CCameraShakerDataFRC17CCameraShakerData`
// on `addi r3,r30,0xc` / `addi r4,r31,0xc`, and then the byte at +0x100 - so the element is a
// `CCameraShakerData` at +0xC (size 0xF4, `include/MetroidPrime/Cameras/CCameraShakerData.hpp`)
// inside a 0x104-byte class, the same object the 0x104-stride walks in `fn_801E7D88` and
// `fn_801E7FE0` step through.  **It is not claimed here**, and that is deliberate: this unit
// claims only the two functions the item names, and `fn_801E8070` stays where the seeder found
// it.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range, `fn_801E7FE0`
// (0x801E7FE0..0x801E8028) begins exactly where this claim begins and no unit claims it; above
// it, `fn_801E8070` (0x801E8070..0x801E80D4) begins exactly where this claim ends and no unit
// claims it either.  Both sit inside `auto_03_801E7CB0_text` (0x801E7CB0..0x801E8AEC), which
// splits into three runs around this claim.  The nearest claimed range below is
// `MetroidPrime/Cameras/Carve801E7C14.c` (0x801E7C14..0x801E7CB0), the destructor chain for the
// same 0x104-stride class, so this address is in the `MetroidPrime/Cameras/` neighbourhood -
// which is where this file sits.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither function, so `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim; the definitions therefore have to stay C, because a C++
// one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the
// unit is a `.c` rather than a `.cpp`.  The file is compiled as C for the port's host build and,
// like every other source in `files.cmake`, is syntax-checked as C++ by
// `tools/probe_sources.sh` - hence the explicit casts, which are compile-time only and leave the
// object byte-identical.

/** 0x801E8070, `symbols.txt:7869`, 0x64 = 100 bytes: the 0x104-stride shake element's copy
 *  constructor, the callee of `fn_801E8048` below.  It belongs to `auto_03_801E7CB0_text` and
 *  no unit claims it, so it is declared here and never defined for the matching build.  The
 *  host link needs a definition; see the port-only stand-in at the end of this file. */
extern void fn_801E8070(void* self, const void* src);

/** `fn_801E8048` - retail `.text:0x801E8048`, 0x28 = 40 bytes: the null-guarded `construct` for
 *  one shake element.  The whole body is the test on the destination and the call; the test is
 *  `self != 0` spelled that way because its measured twin `fn_80004D5C` is written that way in
 *  `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp` and compiles to retail's exact
 *  instruction order. */
void fn_801E8048(void* self, const void* src);

void fn_801E8048(void* self, const void* src) {
  if (self != 0) {
    fn_801E8070(self, src);
  }
}

/** `fn_801E8028` - retail `.text:0x801E8028`, 0x20 = 32 bytes: the outer half of the same
 *  `construct`, a frame and one call to `fn_801E8048` with both arguments forwarded untouched
 *  and nothing else.  Its measured twin is `fn_80004D3C`
 *  (`src/MetroidPrime/Player/Carve80004C4C.c:88`), these eight instructions exactly. */
void fn_801E8028(void* self, const void* src);

void fn_801E8028(void* self, const void* src) { fn_801E8048(self, src); }

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `fn_801E8070` is the 0x104-stride shake
// element's copy constructor, which this item does not claim, and writing it is a separate job -
// it is 25 instructions whose only callee, `__ct__17CCameraShakerDataFRC17CCameraShakerData`,
// is itself `scope:weak` inside a claim the host does not build.  This block exists so the host
// link resolves the `bl` in `fn_801E8048`, and it is the same trade
// `src/MetroidPrime/Cameras/Carve801E7C14.c:135` makes for `__dt__17CCameraShakerDataFv` and
// `src/MetroidPrime/PortLinkStubs.cpp` makes for its symbols: an announced stand-in, kept beside
// the one reference that asks for it.  Measured with `tools/link_check.sh --strict` in this tree:
// with this block the port reports `unique undefined symbols 291`, matching the 291 recorded in
// `docs/research/port_link_baseline.txt`; with the block's guard forced false the same run
// reports 292 `(GREW)` and names `NEW  fn_801E8070`.
//
// The guard is `__MWERKS__`, not `TARGET_PC`, for the reason the same block in
// `Carve801E7C14.c` gives: the matching build must not get a second definition, because the
// matching build's link resolves `fn_801E8070` out of `auto_03_801E7CB0_text`'s own object.
void fn_801E8070(void* self, const void* src) {
  (void)self;
  (void)src;
}
#endif