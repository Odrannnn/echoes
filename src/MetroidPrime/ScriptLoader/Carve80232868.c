// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9992` and `:20820`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80232868_text.s`, and the body below is the C those bytes are the
// compilation of.
//
// .text 0x80232868..0x80232870, 0x8 = 8 bytes, 1 function:
//
//   fn_80232868    0x80232868  0x8    stw     r3, gLoader_MysteryFlyer@sda21(r0)
//                                          blr
//
// **What it is: MysteryFlyer's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of `fn_80227530` in the landed
// `Carve80227530.c` - the whole shape of the family: store the argument into a loader
// pointer's `.sbss` slot and return.  It never reads back what it stored, so it needs the
// slot's address only, not its element type.
//
// **Who calls it, and what the argument is.**  Every call site is in a REL module that imports
// the DOL function by its retail name, so the argument is read off the module's own listing.
// Module 45 (`config/G2ME01/config.yml:236-237`, `files/RelProd/MysteryFlyer.rel`), and both
// callers are in `build/G2ME01/MysteryFlyer/asm/MetroidPrime/ScriptObjects/CMysteryFlyerRel.s`:
//
//   * `RELExit` (`.text` 0x0FC) does `li r3, 0` and calls it on line 137 - the module tears
//     its registration down on the way out.
//   * `fn_45_140` (`.text` 0x140, 0x30 bytes) does `lis r3, lbl_45_bss_0@ha` /
//     `addi r0, r4, fn_45_170@l` / `stwu r0, lbl_45_bss_0@l(r3)` (lines 161-164) and calls it
//     on line 165, so r3 still holds the slot's address: the argument is `&lbl_45_bss_0`.
//     `auto_05_00000000_bss.s` gives `lbl_45_bss_0 size:0x4`, so the DOL slot holds a *pointer
//     to* a 4-byte loader slot, which is why `MysteryFlyer.cpp:19` reads it back as
//     `(*gLoader_MysteryFlyer.value)(mgr, input, info)`.  That call is already written out at
//     `src/MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp:100` (the declaration) and `:115`
//     (the store), which is where this reading comes from.
//
// The slot is `gLoader_MysteryFlyer` at `.sbss 0x80419640`, `size:0x8 data:4byte`
// (`symbols.txt:20820`), and `MetroidPrime/ScriptLoader/MysteryFlyer.cpp` already claims and
// already defines it - that unit's split takes `.sbss 0x80419640..0x80419648`
// (`config/G2ME01/splits.txt:1625`).  So this unit claims `.text` only and takes the pointer
// as `extern`: a second definition is a duplicate the moment both objects are in the link.
// MWCC does not encode a variable's type in its name, so the store below lands on the same
// address whatever the type is spelled - `MysteryFlyer.cpp`'s own `SLoaderSlot` says what the
// two words are; this file needs the address only.
//
// **No host-only block, unlike `Carve80227530.c`.**  That unit had to define `lbl_80419568`
// under `#ifndef __MWERKS__` because the slot it stores into is claimed by no unit of ours, so
// the port's flat link - our sources and not dtk's objects - would have lost it and
// `tools/link_check.sh --strict` would have named a new undefined symbol.  Every symbol this
// unit references is already defined in `src/`, so the port's undefined count cannot move.
//
// `MysteryFlyer.cpp:6-8` says the setter is "deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit".  What a carve must
// preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
// preserves it: the unmangled `fn_80232868` still lands in the DOL link, which is what module
// 45's two `bl fn_80232868` resolve against.  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.  The same correction `Carve80200E3C.c` records
// for the identical sentence in `SpacePirate.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a module
// hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A one-function file
// cannot get it wrong.
//
// Retail names none of these.  `symbols.txt:9992` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// already claimed units: `MysteryFlyer.cpp` ends at 0x80232868 (`splits.txt:1624`) and
// `AtomicBeta.cpp` starts at 0x80232870 (`splits.txt:1628`).  This range is exactly the gap
// between them, 8 bytes, and nothing else claims it.
//
// The directory is retail's own, taken from the nearest claimed range: `MysteryFlyer.cpp` owns
// 0x8023283C..0x80232868 and this address is the next 8 bytes, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is.

/** `.sbss 0x80419640`, `symbols.txt:20820`, `size:0x8 data:4byte`: MysteryFlyer's loader
 *  slot, read by `LoadMysteryFlyer` in `MysteryFlyer.cpp` at `+0`.  That unit defines it;
 *  declared, never defined here. */
extern void* gLoader_MysteryFlyer;

/** MysteryFlyer's registration (module 45): `loader` is `&lbl_45_bss_0`, the module's own
 *  4-byte record holding `&fn_45_170`; `nullptr` is what `RELExit` passes on the way out. */
void fn_80232868(void* loader) { gLoader_MysteryFlyer = loader; }