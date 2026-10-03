// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:11058`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80278C7C_text.s:451-455`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80279244..0x80279250, 0xC = 12 bytes, 1 function:
//
//   fn_80279244    0x80279244  0xC     3 instructions
//
// **It is a byte-shape twin of `GetWidgetTypeID__13CAuiImagePaneCFv`**, retail 0x8027EFA0
// (`symbols.txt:11219`, 0xC bytes), whose whole body is the inline
// `FourCC GetWidgetTypeID() const override { return 'IMGP'; }` at
// `include/GuiSys/CAuiImagePane.hpp:17`.  That twin is measured at **100.00%** in
// `build/report.json` under `main/GuiSys/CAuiImagePane` (11 of its unit's 12 functions at 100,
// the unit itself still `NonMatching` - which is all a twin has to be).  Retail's three
// instructions are `lis r3,0x494d / addi r3,r3,0x4750 / blr`
// (`build/G2ME01/asm/auto_03_8027EF10_text.s:50-56`) and ours are the same three with one immediate
// changed: `lis r3,0x5442 / addi r3,r3,0x4750 / blr`.  So the twin's spelling is the spelling here,
// and the only thing this copy changes is the constant: 0x54424750 instead of 0x494D4750, i.e. the
// FourCC `'TBGP'` rather than `'IMGP'` (both end in `'G'`+`'P'` = 0x4750).
//
// Retail names neither the function nor its class.  `symbols.txt:11058` carries the `fn_<addr>`
// placeholder and this file reproduces that symbol verbatim, so the definition has to stay C: a
// C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why
// the unit is a `.c` rather than a `.cpp`.
//
// It *is* a virtual override, and that is measured rather than guessed: `fn_80279244` is the
// fourth word of the vtable-shaped object `lbl_803B8FA8` (0x803B8FA8, 0x40 bytes) in
// `build/G2ME01/asm/auto_07_803B8F68_data.s:32-34`, right after `fn_80279D90`, and that object is
// laid out exactly like its neighbour `lbl_803B8F68` (0x803B8F68) with `fn_8027906C`,
// `GetIsActive__10CGuiWidgetCFv`, `GetIsVisible__10CGuiWidgetCFv`, `Update__10CGuiWidgetFf`,
// `Draw__10CGuiWidgetCFRC19CGuiWidgetDrawParms`, `GetWorkerWidget__10CGuiWidgetFi` and then two
// `CGuiCompoundWidget` entries in the same order.  It has **no caller** - `grep -rn 'bl
// fn_80279244' build/G2ME01/asm/` returns nothing - because the dispatch is the vtable, and that
// data range is still dtk's unclaimed `auto_07_803B8F68_data`, so the pointer stays retail's.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A single function is descending by construction, but the
// file is written in that shape so that a later extension of the claim can extend downward.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it, 0x80278C7C..0x80279244 is still unclaimed (the rest of dtk's
// `auto_03_80278C7C_text`), and above it, 0x80279250..0x80279260 is
// `MetroidPrime/Carve80279250.c`.  This address is 0x3699C bytes into that same unclaimed run, so
// the claim is a single 0xC-byte island with `auto_*` on both sides - which is exactly the shape
// `Carve80279250.c` and `Carve80278C74.c` already take in this directory.  The directory is
// retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve80278C74.c` (0x80278C74..0x80278C7C) and above is
// `MetroidPrime/Carve80279250.c` (0x80279250..0x80279260), so `MetroidPrime/` is where the code
// lives and no lane picked the directory it happened to own.
//
// The function is a leaf, so nothing above or below the claim needs declaring and this unit has no
// `extern` at all.  For the DOL, dtk's own `auto_*` object defined `fn_80279244` and now the two
// halves of that object do without it; for the port link, `src/MetroidPrime/Carve80279244.c` is
// the definition and no `PortLinkStubs.cpp` stand-in is needed or wanted - `fn_80279244` was not
// stubbed before this carve because no port-linked object referenced it.
//
// The return type is the twin's own: a FourCC, i.e. the 32-bit `uint` of
// `include/Kyoto/SObjectTag.hpp:11`.  0x54424750 is positive and its low half is non-negative, so
// this is the one shape mwcc spells `lis`+`addi`; a value with the low half above 0x7FFF would
// come out as `lis`+`ori` instead, which is why the constant is spelled as the FourCC character
// literal `'TBGP'` rather than split across two halves.
int fn_80279244(void) { return 'TBGP'; }