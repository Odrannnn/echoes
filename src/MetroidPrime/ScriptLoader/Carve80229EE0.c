// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9829` and `:20769`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80229EE0_text.s:9-10` before this claim existed, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x80229EE0..0x80229EE8, 0x8 = 8 bytes, 1 function:
//
//   fn_80229EE0    0x80229EE0  0x8    stw     r3, gLoader_IngPuddle@sda21(r0)
//                                          blr
//
// **What it is: IngPuddle's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of the landed
// `fn_8023289C` in `Carve8023289C.c` - the whole shape of the family: store the argument into a
// loader pointer's `.sbss` slot and return.  It never reads back what it stored, so it needs the
// slot's address only, not its element type.
//
// **Who calls it, and what the argument is.**  Every call site is in a REL module that imports
// the DOL function by its retail name, so the argument is read off the module's own listing.
// Module 32 (`config/G2ME01/config.yml:171-175`, `files/RelProd/IngPuddle.rel`, sha1
// `312b87acb1dea81e5c03fccd6b87e68366f17a6f`), and both callers are in
// `build/G2ME01/IngPuddle/asm/MetroidPrime/ScriptObjects/CIngPuddleRel.s`, which is a `Matching`
// unit of ours (`src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` claims the module's head
// `.text 0x0..0xA8`):
//
//   * `RELExit` (`.text` 0x34, 0x24 bytes) does `li r3, 0x0` (line 33) and calls it on line
//     35 - the module tears its registration down on the way out, so the argument is `0`.
//   * `fn_32_78` (`.text` 0x78, 0x30 bytes) does `lis r4, fn_32_A8@ha` (line 58) /
//     `lis r3, lbl_32_bss_0@ha` (line 59) / `addi r0, r4, fn_32_A8@l` (line 61) /
//     `stwu r0, lbl_32_bss_0@l(r3)` (line 62) and calls it on line 63, so r3 still holds the
//     slot's address: the argument is `&lbl_32_bss_0`.  `config/G2ME01/rels/IngPuddle/symbols.txt:103`
//     gives `lbl_32_bss_0 size:0x4`, so the DOL slot holds a *pointer to* a 4-byte loader slot,
//     which is why `IngPuddle.cpp:19` reads it back as
//     `(*gLoader_IngPuddle.value)(mgr, input, info)`.  That call is already written out there,
//     and `src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` has no
//     `extern "C" fn_80229EE0` to reuse - none is needed, because every symbol this unit
//     references is already defined in `src/`.
//
// The reader that fixes the sense of the store is `LoadIngPuddle` at 0x80229EB4, which
// `build/G2ME01/asm/MetroidPrime/ScriptLoader/IngPuddle.s:10-16` shows as
// `lwz r6, gLoader_IngPuddle@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`: it loads the
// *address* of a loader slot, then dispatches through its first word.  So the argument is a
// loader slot, not a loader - which is also why `fn_32_78` hands it `&lbl_32_bss_0`.
//
// The slot is `gLoader_IngPuddle` at `.sbss 0x80419598`, `size:0x8 data:4byte`
// (`symbols.txt:20769`), and `MetroidPrime/ScriptLoader/IngPuddle.cpp` already claims and
// already defines it - that unit's split takes `.sbss 0x80419598..0x804195A0`
// (`config/G2ME01/splits.txt:1718-1720`).  So this unit claims `.text` only and takes the
// pointer as `extern`: a second definition is a duplicate the moment both objects are in the
// link.  MWCC does not encode a variable's type in its name, so the store below lands on the
// same address whatever the type is spelled - `IngPuddle.cpp`'s own `SLoaderSlot` says what the
// two words are; this file needs the address only.
//
// **No host-only block, unlike `Carve80227530.c`.**  That unit had to define `lbl_80419568`
// under `#ifndef __MWERKS__` because the slot it stores into is claimed by no unit of ours, so
// the port's flat link - our sources and not dtk's objects - would have lost it and
// `tools/link_check.sh --strict` would have named a new undefined symbol.  This unit's only
// reference, `gLoader_IngPuddle`, is defined at `IngPuddle.cpp:16`, so the port's undefined
// count cannot move.  (That is why the item's note that the port count "provably cannot move"
// holds: it is not a prediction, it is the absence of any other reference.)
//
// `IngPuddle.cpp:6-8` said the setter was "deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit".  What a carve must
// preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
// preserves it: the unmangled `fn_80229EE0` still lands in the DOL link, which is what module
// 32's two `bl fn_80229EE0` resolve against.  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.  The same correction `Carve8023289C.c` records
// for the identical sentence in `AtomicBeta.cpp`, and `Carve80232868.c` for `MysteryFlyer.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a module
// hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A one-function file
// cannot get it wrong.
//
// Retail names none of these.  `symbols.txt:9829` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// already claimed units: `IngPuddle.cpp` ends at 0x80229EE0 (`splits.txt:1718`) and
// `FlyerSwarm.cpp` starts at 0x80229F90 (`splits.txt:1723`).  This range is the first 8 bytes of
// the gap between them and nothing else claims it.  The dtk auto unit it is cut from is
// `auto_03_80229EE0_text` (`# 0x80229EE0..0x80229F90 | size: 0xB0`, two functions: this setter
// and `fn_80229EE8` at 0x80229EE8, 0xA8).  This carve is the **front** case: it takes the head,
// so the remainder comes back under a new name, `auto_03_80229EE8_text` covering
// 0x80229EE8..0x80229F90 (0xA8 = 168 bytes, 1 function), and it needs no `configure.py` entry.
// `docs/goal-notes/carve-8023289c.md` records the same front case and
// `docs/goal-notes/carve-80045160.md` the middle one, where the auto unit keeps its start.
//
// The directory is retail's own, taken from the nearest claimed range: `IngPuddle.cpp` owns
// 0x80229EB4..0x80229EE0 and this address is the next 8 bytes, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is.

/** `.sbss 0x80419598`, `symbols.txt:20769`, `size:0x8 data:4byte`: IngPuddle's loader slot,
 *  read by `LoadIngPuddle` in `IngPuddle.cpp` at `+0`.  That unit defines it; declared,
 *  never defined here. */
extern void* gLoader_IngPuddle;

/** IngPuddle's registration (module 32): `loader` is `&lbl_32_bss_0`, the module's own 4-byte
 *  record holding `&fn_32_A8`; `0` is what `RELExit` passes on the way out. */
void fn_80229EE0(void* loader) { gLoader_IngPuddle = loader; }