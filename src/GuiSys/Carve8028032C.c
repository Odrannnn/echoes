// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt` and `splits.txt`, the
// instructions are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_802802B8_text.s`,
// and the body below is the C those bytes are the compilation of.
//
// .text 0x8028032C..0x80280338, 0xC = 12 bytes, 1 function:
//
//   fn_8028032C    0x8028032C  0xC    lis r3,0x424d ; addi r3,r3,0x5452 ; blr
//
// The returned word is (0x424d << 16) | 0x5452 = **0x424D5452**.  It is a byte-shape twin of
// the matched `CAuiMeter::GetWidgetTypeID() const` at 0x80274090
// (`build/G2ME01/asm/GuiSys/CAuiMeter.s:76`), which returns 'METR' and emits the identical
// `lis`/`addi`/`blr` with 0x4D455452 instead; the only difference between the two copies is
// the 16-bit halves of the constant.  Written as an integer rather than as a FourCC literal
// because retail names this function nothing, so nothing states that the word is a FourCC -
// the bytes are the claim, the reading is not.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  With one function here the order is
// vacuous, but the file has to keep descending if a second one is ever added.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial: `fn_802802B8` is 0x68 bytes and
// `MetroidPrime/Carve80280338.c` is already a separate `Matching` unit.
//
// The directory is retail's own, taken from the nearest claimed range: this address is
// 0x74 bytes past the end of `GuiSys/CAuiImagePane.cpp`'s claim (0x8027EF10..0x802802B8),
// so the code is that unit's neighbourhood.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.
//
// Claimed range is exactly this function and nothing else.  The enclosing dtk range
// (0x802802B8..0x80280338) keeps `fn_802802B8` unclaimed, so it stays an `auto_*` unit.
unsigned int fn_8028032C(void) { return 0x424D5452u; }
