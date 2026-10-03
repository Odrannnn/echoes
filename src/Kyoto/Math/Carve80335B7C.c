// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80335B5C_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80335B7C..0x80335B84, 0x8 = 8 bytes, 1 function:
//
//   fn_80335B7C    0x80335B7C  0x8    lwz r3, 0x1c(r3) ; blr
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
// 0xDF6C bytes into `Kyoto/Math/CMayaSpline.cpp` (which holds `.text 0x80327C10..0x8032AFC8`
// in `config/G2ME01/splits.txt`), so the code is that unit neighbourhood.  For an anonymous
// function that is the only evidence there is, and it beats a lane picking the directory it
// happened to own.
//
// **The function is virtual.**  `build/G2ME01/asm/auto_07_803BBB68_data.s` lists
// `fn_80335B7C` as a vtable entry of `lbl_803BBB68` (`symbols.txt:18732`, `size:0x1F0`), and
// `auto_07_803BB288_data.s` repeats it in three further tables, so it is reached through a
// vtable slot rather than called by name - which is why the tree has no caller to read a
// signature off and the two instructions below are the whole of the evidence for it.
//
// **The structure is by offset, not by a class.**  Retail's class for this slot is unnamed
// in `symbols.txt`, so the word is declared in a local struct whose only claim is the offset
// the instruction names, exactly as `src/Kyoto/Math/Carve80335B48.c` does for the accessor
// pair below it and `src/MetroidPrime/Tweaks/Carve80216D2C.c` does for `CTweakGame`.
// Nothing between +0x0 and +0x1B is asserted, and nothing reads it.  A future run that
// identifies the owning class can replace the local struct with the real header without
// touching the body.
//
// **Port.**  Nothing in `src/` or `include/` names this symbol - it is reached only through
// the `.data` vtables above, which retail's own unclaimed `auto_07_*` objects supply and the
// host link does not carry - so no `TARGET_PC` arm is needed and the body below is the
// DOL's, offsets and all.
typedef struct {
  char x00[0x1c];
  int x1c; /* 0x1C - fn_80335B7C's load */
} SCarve80335B7C;

int fn_80335B7C(SCarve80335B7C* self) { return self->x1c; }
