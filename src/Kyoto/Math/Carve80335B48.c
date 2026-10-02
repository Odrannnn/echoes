// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80335B48_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80335B48..0x80335B58, 0x10 = 16 bytes, 2 functions:
//
//   fn_80335B48    0x80335B48  0x8    stfs f1, 0x4(r3) ; blr
//   fn_80335B50    0x80335B50  0x8    stw  r4, 0x1c(r3) ; blr
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
// functions on either side of this run are not trivial.  The claim fills the 0x10-byte gap
// between the two carves that already bracket it, `Kyoto/Math/Carve80335B38.c`
// (0x80335B38..0x80335B48) below and `Kyoto/Math/Carve80335B58.c` (0x80335B58..0x80335B5C)
// above, so it spans no unclaimed gap and is entered in `splits.txt` in address order
// between them.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0xDF38 bytes into `Kyoto/Math/CMayaSpline.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
//
// **Both functions are virtual.**  `build/G2ME01/asm/auto_07_803BBB68_data.s` lists them as
// consecutive vtable entries (`lbl_803BBB68`, `symbols.txt:18732`, `size:0x1F0`), and
// `auto_07_803BB288_data.s` repeats the pair in three further tables, so each is reached
// through a vtable slot rather than called by name - which is why the tree has no caller to
// read a signature off and the two words below are the whole of the evidence for it.  They
// are setters, one storing the incoming float at `self + 4` and one the incoming int at
// `self + 0x1C`, and that is read off the instructions, not assumed.
//
// **The structure is by offset, not by a class.**  Retail's class for these slots is
// unnamed in `symbols.txt`, and the tree's own classes do not agree on a single layout that
// puts a float at +0x4 and an int at +0x1C - so the words are declared in a local struct
// whose only claims are the two offsets the instructions name, exactly as
// `src/MetroidPrime/Tweaks/Carve80216D2C.c` does for `CTweakGame`.  Nothing between +0x8
// and +0x1B is asserted, and nothing reads it.
//
// **Port.**  Nothing in `src/` or `include/` names either symbol - they are reached only
// through the `.data` vtables above, which retail's own unclaimed `auto_07_*` objects
// supply and the host link does not carry - so no `TARGET_PC` arm is needed and the
// bodies below are the DOL's, offsets and all.
typedef struct {
  char x00[4];
  float x04; /* 0x04 - fn_80335B48's store target */
  char x08[0x14];
  int x1c; /* 0x1C - fn_80335B50's store target */
} SCarve80335B48;

void fn_80335B50(SCarve80335B48* self, int value) { self->x1c = value; }

void fn_80335B48(SCarve80335B48* self, float value) { self->x04 = value; }