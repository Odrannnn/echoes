// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x802B2568..0x802B2580, 0x18 = 24 bytes, 4 functions:
//
//   fn_802B2568    0x802B2568  0x8    li        r3, 0x0
//   fn_802B2570    0x802B2570  0x4    blr
//   fn_802B2574    0x802B2574  0x4    blr
//   fn_802B2578    0x802B2578  0x8    li        r3, 0x0
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0x858 bytes into `Kyoto/Animation/CSegId.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
int fn_802B2578(void) { return 0; }

void fn_802B2574(void) {}

void fn_802B2570(void) {}

int fn_802B2568(void) { return 0; }
