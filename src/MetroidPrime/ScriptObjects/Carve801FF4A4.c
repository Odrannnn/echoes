// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x801FF4A4..0x801FF4C4, 0x20 = 32 bytes, 4 functions:
//
//   fn_801FF4A4    0x801FF4A4  0x8    li        r3, 0x3
//   fn_801FF4AC    0x801FF4AC  0x8    li        r3, 0x0
//   fn_801FF4B4    0x801FF4B4  0x8    li        r3, 0x2
//   fn_801FF4BC    0x801FF4BC  0x8    li        r3, 0x1
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
// 0x6454 bytes into `MetroidPrime/ScriptObjects/CUnknown90.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
int fn_801FF4BC(void) { return 1; }

int fn_801FF4B4(void) { return 2; }

int fn_801FF4AC(void) { return 0; }

int fn_801FF4A4(void) { return 3; }
