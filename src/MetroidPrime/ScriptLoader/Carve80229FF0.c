// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt`, the two instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80229FF0_text.s:9-11`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80229FF0..0x80229FF8, 0x8 = 8 bytes, 1 function:
//
//   fn_80229FF0    0x80229FF0  0x8    stw     r3, gLoader_StreamedMovie@sda21(r0)
//                                          blr
//
// **What it is: StreamedMovie's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`), of the matched `fn_80229EE0` in
// `Carve80229EE0.c` (IngPuddle's) and of the landed `fn_8023289C` in `Carve8023289C.c` - the whole
// shape of the family: store the argument into a loader pointer's `.sbss` slot and return.  It
// never reads back what it stores, so it needs the slot's address only, not its element type.
//
// **Who calls it, and what the argument is.**  Every call site is in a REL module that imports the
// DOL function by its retail name, so the argument is read off the module's own listing.  There is
// exactly one such module, module 67 (`config/G2ME01/config.yml:437-441`,
// `files/RelProd/ScriptStreamedMovie.rel`, sha1 `d9b45eae526aa3fb5290599843482512b257aba8`),
// and both of its call sites are in
// `build/G2ME01/ScriptStreamedMovie/asm/auto_00_00000000_text.s`:
//
//   * `RELExit` (`.text` 0xB4, 0x24 bytes) does `li r3, 0x0` (line 68) and calls it on line 70 -
//     the module tears its registration down on the way out, so the argument is `0`.
//   * `fn_67_F8` (`.text` 0xF8, 0x30 bytes) does `lis r4, fn_67_128@ha` (line 93) /
//     `lis r3, lbl_67_bss_4@ha` (line 94) / `addi r0, r4, fn_67_128@l` (line 96) /
//     `stwu r0, lbl_67_bss_4@l(r3)` (line 97) and calls it on line 98, so r3 still holds the
//     slot's address: the argument is `&lbl_67_bss_4`.  `config/G2ME01/rels/ScriptStreamedMovie/
//     symbols.txt:37` gives `lbl_67_bss_4 = .bss:0x00000004; // type:object size:0x4 data:4byte`,
//     so the DOL slot holds a *pointer to* a 4-byte loader slot, which is why
//     `StreamedMovie.cpp:18-20` reads it back as `(*gLoader_StreamedMovie.value)(mgr, input, info)`.
//
// The reader that fixes the sense of the store is `LoadStreamedMovie` at 0x80229FC4, which
// `build/G2ME01/asm/MetroidPrime/ScriptLoader/StreamedMovie.s:12-17` shows as
// `lwz r6, gLoader_StreamedMovie@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`: it
// loads the *address* of a loader slot, then dispatches through its first word.  So the argument
// is a loader slot, not a loader - which is also why `fn_67_F8` hands it `&lbl_67_bss_4`, and it
// is the same reading `Carve80229EE0.c:40-44` records for IngPuddle.
//
// The slot is `gLoader_StreamedMovie` at `.sbss 0x804195A8`, `size:0x8 data:4byte`
// (`symbols.txt:20771`), and `MetroidPrime/ScriptLoader/StreamedMovie.cpp` already claims and
// already defines it - that unit's split takes `.sbss 0x804195A8..0x804195B0`
// (`config/G2ME01/splits.txt:1903-1905`) and its listing names it at `+0`.  So this unit claims
// `.text` only and takes the pointer as `extern`: a second definition is a duplicate the moment
// both objects are in the link.  MWCC does not encode a variable's type in its name, so the store
// below lands on the same address whatever the type is spelled - `StreamedMovie.cpp`'s own
// `SLoaderSlot` says what the two words are; this file needs the address only.
//
// **No host-only block, unlike `Carve80229EAC.c`.**  That unit had to define `lbl_80419590` under
// `#ifndef __MWERKS__` because the slot it stores into is claimed by no unit of ours, so the
// port's flat link - our sources and not dtk's objects - would have lost it and
// `tools/link_check.sh --strict` would have named a new undefined symbol.  This unit's only
// reference, `gLoader_StreamedMovie`, is defined at `StreamedMovie.cpp:16`, so the port's undefined
// count cannot move.
//
// `StreamedMovie.cpp:6-8` said the setter was "deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit".  What a carve must
// preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
// preserves it: the unmangled `fn_80229FF0` still lands in the DOL link, which is what module
// 67's two `bl fn_80229FF0` resolve against.  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.  The same correction `Carve80229EE0.c:63-69`
// records for the identical sentence in `IngPuddle.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL, with the
// link still succeeding.  Only `tools/flip_test.sh` catches that.  A one-function file cannot get
// it wrong.
//
// Retail names none of these.  `symbols.txt:9834` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_80229FF0v` and both the DOL link and module 67's `bl fn_80229FF0` would pair
// nothing.  That is also why the unit is a `.c` rather than a `.cpp`.
//
// **Why its own unit, and what the split does.**  A unit may not claim two discontiguous ranges
// in one section (dtk `dol split` fails with "Cyclic dependency ... link order"), and both
// neighbours are already claimed units: `StreamedMovie.cpp` ends at 0x80229FF0
// (`splits.txt:1904`) and `IngSpiderBallGuardian.cpp` starts at 0x80229FF8 (`splits.txt:1908`).
// This range is the 8-byte gap between them and nothing else claims it.  The dtk auto unit it is
// cut from is `auto_03_80229FF0_text` (`# 0x80229FF0..0x80229FF8 | size: 0x8`, one function -
// this setter), so this carve is the **whole-unit** case: the auto unit disappears from the build
// entirely, and there is no renamed remainder and no shortened neighbour to check.
// `docs/goal-notes/carve-80229ee0.md` records the front case and `carve-801e5230.md` the middle
// one, where the tail came back under a new name.
//
// The directory is retail's own, taken from the nearest claimed range: `StreamedMovie.cpp` owns
// 0x80229FC4..0x80229FF0 and this address is the next 8 bytes, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats a
// lane picking the directory it happened to own.

/** `.sbss 0x804195A8`, `symbols.txt:20771`, `size:0x8 data:4byte`: StreamedMovie's loader
 *  slot, read by `LoadStreamedMovie` in `StreamedMovie.cpp` at `+0`.  That unit defines it;
 *  declared, never defined here. */
extern void* gLoader_StreamedMovie;

/** StreamedMovie's registration (module 67): `loader` is `&lbl_67_bss_4`, the module's own
 *  4-byte record holding `&fn_67_128`; `0` is what `RELExit` passes on the way out. */
void fn_80229FF0(void* loader) { gLoader_StreamedMovie = loader; }