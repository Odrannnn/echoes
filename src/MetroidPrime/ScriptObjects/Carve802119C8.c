// Carved out of an unclaimed dtk `auto_*` range by goal item `carve-802119c8` (lane `6`).
// Every number here is measured: the address and size come from `config/G2ME01/symbols.txt`,
// the instructions are the ones dtk itself emitted into `build/G2ME01/asm/
// auto_03_80210990_text.s` (lines 1148-1152), and the body below is the C those bytes are the
// compilation of.
//
// .text 0x802119C8..0x802119D0, 0x8 = 8 bytes, 1 function:
//
//   fn_802119C8    0x802119C8  0x8    stw r4, 0x38(r3) ; blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still
// saying "fits", the link still succeeding, and a broken DOL.  Only `tools/flip_test.sh`
// catches that.  With one function the order is trivially right; the rule is recorded because
// the next run to extend this claim may not have one.
//
// Retail names none of these.  `symbols.txt:8527` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the functions on either
// side of this run are not trivial: below, `fn_80211864` (0x80211864, 0x164) ends with
// `addi r1,r1,0x30 / blr`, and above, `fn_802119D0` (0x802119D0, 0x9C) opens with
// `stwu r1,-0x20(r1)` and builds a `CCallStack` from the `lbl_803ACBB4` string.  Neither
// boundary is another unit's boundary, so no link-order cycle is at risk (`docs/
// RUNNING_THE_DECOMP.md`, "The carve vein") - proximate carves link, a carve that *starts*
// where an existing unit's `.text` ends does not.
//
// The directory is retail's own, taken from the nearest claimed ranges: `MetroidPrime/
// ScriptObjects/Carve80210980.c` ends 0x1038 bytes below this claim and
// `MetroidPrime/ScriptObjects/Carve80212278.c` starts 0x8A8 bytes above its end, so both
// neighbours of the address are in `ScriptObjects/`.  The named retail function inside this
// same `auto_*` run is `__ct__9CScanTreeFv` (0x80211C3C), and `CScanTreeInventory.cpp` -
// `MetroidPrime/ScriptObjects/CScanTreeInventory.cpp`, 0x8020EBF0..0x8020EE18 - is the claimed
// range below.  For an anonymous function that is the only evidence there is, and it beats a
// lane picking the directory it happened to own.

/** The one thing the bytes name: the receiver's word at `+0x38`, which `fn_802119C8` writes
 *  and nothing in this claim reads.  The 0x38 bytes in front of it are padding so the store
 *  lands on retail's displacement; what they hold is not this unit's business.
 *
 *  **What the receiver is, as far as its neighbours say.**  The single caller is
 *  `fn_80212EB0` (0x80212EB0, `auto_03_80212A2C_text`), and it hands `r27` - its own second
 *  argument - to this function at 0x80212EF8, immediately after reading a 32-bit word off the
 *  stream its first argument keeps at `+8` (`lwz r3,0x8(r3) / addi r0,r3,4 / stw r0,8(r26) /
 *  lwz r4,0x0(r4)`), so the value is an `int` and the receiver is the object being read into.
 *  That object also owns the vector `fn_80211A6C` (0x80211A6C, same `auto_*` run) resizes,
 *  which passes `addi r3,r3,0x28` to the 8-byte-element `reserve` carved as
 *  `MetroidPrime/ScriptObjects/Carve80213320.cpp`; that vector is `rstl::vector`'s four words
 *  at `+0x28..+0x34`, so the int at `+0x38` is the word immediately after it.  Nothing past
 *  `+0x3B` is asserted here and nothing reads it; the rest of retail's class is another
 *  unit's business and this file does not restate it. */
struct SCarve802119C8 {
  char x00[0x38];
  int x38;
};

/** The setter: the store of the second argument into the receiver's `+0x38` word, and the
 *  return.  It is byte-for-byte the shape of a symbol retail *does* name elsewhere in this
 *  DOL, `CardStat::SetCommentAddr(int)` (0x80309710, `Matching` in
 *  `src/Kyoto/DolphinCMemoryCardSys.cpp`), whose body is
 *  `CARDSetCommentAddress(&mStat, addr)` and `include/dolphin/card.h:43`'s
 *  `#define CARDSetCommentAddress(stat, addr) ((stat)->commentAddr = (u32)(addr))` - a `u32`
 *  at `CARDStat`'s `+0x38` (`include/dolphin/card.h:112`), the same two instructions and the
 *  same displacement.  That is the twin: it is evidence for the shape, not a result of this
 *  one; `tools/flip_test.sh` is what says so. */
void fn_802119C8(struct SCarve802119C8* self, int value) { self->x38 = value; }