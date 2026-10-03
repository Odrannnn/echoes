// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10471-10472`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80255128_text.s:183-193` (the same range is now
// `build/G2ME01/asm/WorldFormat/Carve80255350.s`), and the body below is the C those bytes are
// the compilation of.
//
// .text 0x80255350..0x80255370, 0x20 = 32 bytes, 1 function:
//
//   fn_80255350    0x80255350  0x20    8 instructions
//
//   80255350  stwu r1, -0x10(r1)
//   80255354  mflr r0
//   80255358  stw  r0, 0x14(r1)
//   8025535c  bl   fn_80255370
//   80255360  lwz  r0, 0x14(r1)
//   80255364  mtlr r0
//   80255368  addi r1, r1, 0x10
//   8025536c  blr
//
// **It is a byte-shape twin of `fn_80004438`** (`rstl::destroy< CWorldState >`,
// `src/MetroidPrime/Carve80004438.c`, `Matching`): these 32 bytes word for word, with only the
// `bl` target differing - `bl fn_80255370` here against `bl fn_80004458` there.  That twin's
// source is one statement, `fn_80004458(self)` (`include/rstl/construct.hpp:92-95`), which is
// what this file's body is too: a frame, one unconditional tail call that touches no register,
// and an epilogue.  The 0x10-byte frame is retail's own `mflr`/`stw r0,0x14(r1)` pair, which is
// the shape every out-of-line call in this file's neighbourhood has.
//
// **What it forwards is measured from the callee, not assumed from the twin's name.**  The
// twin's `destroy_impl` takes `(T*, int)` and materialises a `-1`; `fn_80255370` here does not
// - it reads `r4` as a `CInputStream` (`lwz r4,0x8(r31)` then `lbz r4,0x0(r4)` at
// 0x802553B8/0x802553C8, after `mr r31,r4` at 0x80255384) and `r5` as the allocator it forwards
// as the third argument to `fn_802553F4` (`addi r5,r1,0x8`, `mr r30,r3`, `bl fn_802553F4`, then
// `mr r3,r30` / `bl fn_8015B780` / `bl __dt__Q24rstl36vector<f,...>Fv`, and `stb r0,0x10(r30)`
// for the stream's flag byte).  So `fn_80255370` is retail's out-of-line
// `rstl::vector<float>`-from-`CInputStream` load for one element type, and **this function is a
// plain out-of-line forwarder for it, forwarding all three arguments untouched** - not a
// `destroy`.  Written with three parameters because that is what the callee reads; the frame and
// the tail call are the same either way, and the parameters are what the header above states.
//
// **The one caller fixes the arity.**  `grep -rn "bl fn_80255350" build/G2ME01/asm/` returns
// exactly one call site, 0x8025531C inside `fn_802552E4` (`auto_03_80255128_text.s:153-181`),
// reached with `addi r3,r1,0xc` at 0x802552FC, `addi r5,r1,0x8` at 0x802552F0 and the stream
// still in `r4` - and it stores `lbl_80419714` into `0x8(r1)` (a `lbl_` of
// `size:0x1`, `symbols.txt:20845`) immediately before, which is the `rmemory_allocator`
// temporary the third argument points at.  The same shape appears twice more in this
// unclaimed run: `fn_802552A4` (0x20, `bl fn_801E1738`), `fn_802552C4` (0x20,
// `bl fn_802552E4`), `fn_802553F4` (0x24, `addi r5,r1,0x8` then
// `bl __ct__Q24rstl36vector<f,...>FR12CInputStreamRCQ24rstl17rmemory_allocator`) - four copies
// of the out-of-line tail-call idiom retail emitted for this template family.  `fn_802553F4`
// adds its own `addi r5,r1,0x8`, which is why it is 0x24 and these three are 0x20.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z11fn_80255350PvPvPv` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order cannot be wrong, and the rule
// is recorded because the next carve added to this file would break it.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// unit's `.text` ends: below the claim `fn_802552E4` (0x802552E4, 0x6C) is unclaimed, above it
// `fn_80255370` (0x80255370, 0x84) is too, and neither is trivial, so this absorbs neither side.
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `WorldFormat/CAreaRenderOctTree.cpp` (`.text` 0x80254BAC..0x80255128) and above is
// `WorldFormat/Carve802554BC.c` (0x802554BC..0x802554D0).  `tools/check_decl_order.py --unit
// WorldFormat/Carve80255350` is the cheap check.
//
// `fn_80255370` is **above** this claim and is therefore declared, never defined here; its own
// 0x84 bytes are a spelling job of its own, which is why the claim stops where it does, and its
// `bl` at 0x8025535C is retail's, so the carve cannot drop the call.  For the DOL, dtk's own
// `auto_*` object defines it and the two halves of that object do without it; for the port link
// it is the announced stand-in `stub_carve80255350_0` in
// `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_80004438_0` makes for
// `Carve80004438.c`'s callee.  Nothing here claims `fn_80255370` is decompiled.

/** 0x80255370, `symbols.txt:10472`, 0x84 = 132 bytes: the out-of-line
 *  `rstl::vector<float>`-from-`CInputStream` load this function forwards to.  Declared, never
 *  defined here; the port link's stand-in is `stub_carve80255350_0` in
 *  `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body.  The three pointers are
 *  `(this, CInputStream&, const rmemory_allocator&)`, fixed by the callee's own reads of `r4`
 *  and `r5` and by its one caller's argument setup - see the header. */
extern void fn_80255370(void* self, void* in, void* alloc);

/** `fn_80255350` - retail `.text:0x80255350`, 0x20 = 32 bytes: a 0x10-byte frame and one
 *  unconditional `bl fn_80255370` with no load, no test and no register moved in between, so the
 *  whole body is that one call with its three arguments forwarded.  Its byte-shape twin is
 *  `fn_80004438` (0x80004438, `MetroidPrime/Carve80004438.c`, `Matching`), the same eight
 *  instructions with a different `bl` target. */
void fn_80255350(void* self, void* in, void* alloc);

void fn_80255350(void* self, void* in, void* alloc) { fn_80255370(self, in, alloc); }