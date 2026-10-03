// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:10512-10514`, the instructions are retail's own, read
// this run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x802563C8
// --stop-address=0x802563E8 build/G2ME01/main.elf`, and the body below is the C those bytes are the
// compilation of.  The same bytes are also in dtk's pre-claim disassembly
// `build/G2ME01/asm/auto_03_80255D2C_text.s:507-517`, whose `.fn fn_802563C8` block is lines 508-517.
// `build/G2ME01/asm/WorldFormat/Carve802563C8.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x802563C8..0x802563E8, 0x20 = 32 bytes, 1 function:
//
//   fn_802563C8    0x802563C8  0x20    8 instructions
//
// **What it is: a frame and one unconditional `bl`, with no register touched.**  The seed's
// byte-shape twin is `fn_80004438` (0x80004438, 0x20, 8 instructions,
// `src/MetroidPrime/Carve80004438.c`, `Matching`): the same eight words apart from the `bl`
// displacement (`48 00 01 9d` against `48 00 00 15`), read as a diff of two objdump listings off
// the disc rather than recalled.  `src/WorldFormat/Carve80255A0C.c`'s `fn_80255A0C` is a third
// copy of the same eight words, and it is what fixes the spelling: a plain forwarder with no other
// statement compiles to exactly these eight instructions at `-O4,p`, with a 0x10-byte frame.
//
// **The twin's *logic* is not this function's logic, and the difference is in the arguments.**  The
// twin forwards r3 and nothing else, because its callee `fn_80004458` materialises `li r4,-1`
// itself (MWCC's "destroy, do not free me" flag - `include/rstl/construct.hpp:84-95`, which is
// what that whole four-function chain at 0x80255A0C is as well).  `fn_802563C8` materialises
// nothing: it forwards **r3 and r4 unchanged**, because its callee uses both.  Measured off
// `main.elf` (0x80256570, 0x60 bytes, `asm/auto_03_80255D2C_text.s:630-655`):
// `mr r30,r3 / mr r31,r4 / ... / lwz r5,0x8(r31) / ... / mr r4,r31 / addi r3,r30,0x14 / ... /
// sth r0,0x10(r30) / bl __ct__6CAABoxFR12CInputStream / ... / mr r3,r30` - the second argument is a
// pointer it dereferences and hands to a `CAABox(CInputStream&)`, and the result it returns is
// the first argument.
//
// **The one caller in the DOL confirms both halves.**  `grep -rn 'bl fn_802563C8' build/G2ME01/asm/`
// returns exactly one hit, 0x802561B8 inside `fn_80256158` (0x80256158, 0xA0 bytes, unclaimed and
// left in dtk's range), which sets `addi r3,r1,0xc / mr r4,r28 / mr r5,r31` immediately before it -
// two live arguments, and no `li r4` of its own anywhere in the call sequence to supply a second
// one.  So this is a two-argument forwarder that returns its callee's result, and the body below is
// written that way; a one-argument spelling would leave r4 dead and would be a different function.
//
// **The class is not named here and nothing in this claim names it.**  `fn_80256570` reads a
// stream and initialises the receiver, which is the shape of a `CAABox`-owning stream constructor,
// but retail calls it `fn_80256570` (`symbols.txt:10514`) and `symbols.txt` names the receiver's
// type nowhere that this claim reaches, so the two pointers are spelled as pointers.
//
// **The callee is above this claim and is not claimed by it.**  `fn_80256570` is 0x80256570, above
// 0x802563E8, and no unit claims it - it is still in dtk's `auto_03_80255D2C_text`, so for the DOL
// link dtk's own object supplies the bytes and retail's `bl` resolves to retail's address.  It is
// declared below, never defined here.  For the port link it is the announced empty stand-in
// `stub_carve802563c8_0` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade
// `stub_80004438_0` makes for `Carve80004438.c`'s callee.  Nothing here claims 0x80256570 is
// decompiled.
//
// Measured: `grep` for `fn_802563C8` and `fn_80256570` over `src/` and `include/` returns nothing
// outside this file, so no `src/MetroidPrime/PortLinkStubs.cpp` duplicate has to be deleted for this
// carve (the fourth trap in `docs/RUNNING_THE_DECOMP.md` §"The carve vein").
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit
// WorldFormat/Carve802563C8.c` has nothing to compare here and says so, which is **not** a check it
// performed.  With a second function ever added here it must go first.
//
// Retail names it nothing.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_802563C8v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`.
//
// Its own unit because a claim may not span an unclaimed gap, and because a unit may not claim two
// discontiguous ranges in one section.  The claim is exactly `fn_802563C8` (0x20 bytes), and
// 0x802563C8 + 0x20 = 0x802563E8, the address of the next `symbols.txt` function
// (`fn_802563E8`, 0x188 bytes), so both boundaries are function edges and nothing is split here.
// The claim starts 0x69C = 1692 bytes **after** the unit below ends
// (`WorldFormat/Carve80255C54.c`, 0x80255C54..0x80255D2C) and ends 0x934 = 2356 bytes **before** the
// one above (`WorldFormat/Carve80256D1C.c`, 0x80256D1C..0x80256D64), which is what keeps
// `dtk dol split` from reporting a link-order cycle against either neighbour.  The 26 functions
// left in the unclaimed gap around it - `fn_802563E8` (0x188) directly above and everything from
// `fn_80256158` (0x80256158) below - are not carved here.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `WorldFormat/Carve80255C54.c` (0x80255C54..0x80255D2C) and above is
// `WorldFormat/Carve80256D1C.c` (0x80256D1C..0x80256D64), so this address sits in that unit's
// neighbourhood and `WorldFormat` is not a choice made here.

/** 0x80256570, `symbols.txt:10514`, 0x60 = 96 bytes: the one callee of this file's `fn_802563C8`,
 *  which dereferences its **second** argument as a pointer and returns the first (`mr r30,r3` /
 *  `mr r31,r4` on entry, `mr r3,r30` before `blr`), so both parameters are spelled as pointers and
 *  the result as `void*`.  Claimed by no unit - it is one byte-range above this claim and still in
 *  dtk's unclaimed `auto_03_80255D2C_text`.  Declared, never defined here; the port link's stand-in
 *  is `stub_carve802563c8_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body. */
extern void* fn_80256570(void* self, void* other);

/** `fn_802563C8` - retail `.text:0x802563C8`, 0x20 = 32 bytes: a frame and one `bl` to
 *  `fn_80256570`, with both arguments and the return value passed straight through.  The byte-shape
 *  twin is `fn_80004438` (`MetroidPrime/Carve80004438.c`, `Matching`), these eight instructions
 *  word for word apart from the `bl` displacement; what differs is the logic - see the header. */
void* fn_802563C8(void* self, void* other);

void* fn_802563C8(void* self, void* other) { return fn_80256570(self, other); }