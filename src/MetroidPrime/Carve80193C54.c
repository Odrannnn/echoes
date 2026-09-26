// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80193C54..0x80193C7C, 0x28 = 40 bytes, 6 functions:
//
//   fn_80193C54    0x80193C54  0x8    li        r3, 0x0
//   fn_80193C5C    0x80193C5C  0x8    li        r3, 0x0
//   fn_80193C64    0x80193C64  0x8    li        r3, 0x0
//   fn_80193C6C    0x80193C6C  0x4    blr
//   fn_80193C70    0x80193C70  0x8    li        r3, 0x0
//   fn_80193C78    0x80193C78  0x4    blr
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
// 0x9C80 bytes into `MetroidPrime/CDamageInfo.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
void fn_80193C78(void) {}

int fn_80193C70(void) { return 0; }

void fn_80193C6C(void) {}

int fn_80193C64(void) { return 0; }

int fn_80193C5C(void) { return 0; }

int fn_80193C54(void) { return 0; }
