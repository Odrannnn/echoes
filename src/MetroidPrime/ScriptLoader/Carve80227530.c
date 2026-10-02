// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9752-9753` and `:20790-20791`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80227530_text.s:9-18`, and the bodies below are the C those bytes
// are the compilation of.
//
// .text 0x80227530..0x80227540, 0x10 = 16 bytes, 2 functions:
//
//   fn_80227530    0x80227530  0x8    stw     r3, gLoader_AIMannedTurret@sda21(r0)
//                                          blr
//   fn_80227538    0x80227538  0x8    stw     r3, lbl_80419568@sda21(r0)
//                                          blr
//
// **What they are: two loader-slot setters of the ScriptLoader family.**  Both are byte-shape
// twins of the matched `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of `fn_8022756C` in the landed
// `Carve8022756C.c` 0x2C bytes above this range - the whole shape of the family: store the
// argument into a loader pointer's `.sbss` slot and return.  Neither one reads what it stores,
// so neither needs the slot's element type, only its address.
//
// **Who calls them, and what the argument is.**  Every call site is in a REL module that
// imports the DOL function by its retail name, so the argument is read off the module's own
// listing:
//
//   * `fn_80227530` is module 1's (`config/G2ME01/config.yml:17`,
//     `files/RelProd/AIMannedTurret.rel`).  Both callers are in
//     `build/G2ME01/AIMannedTurret/asm/auto_00_00000018_text.s`: `RELExit` passes `li r3, 0`
//     (line 38, call on line 40) - the module tears its registration down on the way out - and
//     `fn_1_A4` (call on line 68) does `lis r4, fn_1_D4@ha` / `addi r0, r4, fn_1_D4@l` /
//     `stwu r0, lbl_1_bss_90@l(r3)` (lines 60-66) and so passes `&lbl_1_bss_90`: the address of
//     the module's own 4-byte slot, whose word 0 is its entity loader `fn_1_D4`.  So the DOL
//     slot holds a *pointer to* a loader slot, which is why `AIMannedTurret.cpp:19` reads it
//     back as `(*gLoader_AIMannedTurret.value)(mgr, input, info)`.
//   * `fn_80227538` is module 50's (`config/G2ME01/config.yml:262`,
//     `files/RelProd/PirateRagDoll.rel`).  Both callers are in
//     `build/G2ME01/PirateRagDoll/asm/MetroidPrime/ScriptObjects/CPirateRagDollRel.s`:
//     `RELExit` passes `li r3, 0` (line 12, call on line 14) and `fn_50_44` (call on line 42)
//     follows `lbl_50_bss_188 = fn_50_74` with `&lbl_50_bss_188`.
//     `src/MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp:22-27` already describes that call
//     site and these two instructions.
//
// **The two slots are not claimed here, and they are not the same case.**
//
//   * `gLoader_AIMannedTurret` is `.sbss 0x80419560`, `size:0x8 data:4byte`
//     (`symbols.txt:20790`), and `MetroidPrime/ScriptLoader/AIMannedTurret.cpp` already claims
//     and defines it (that unit's `.text` ends exactly where this one starts).  It is taken as
//     `extern` here: a second definition is a duplicate the moment both objects are in the
//     link.  MWCC does not encode a variable's type in its name, so the store below lands on
//     the same address whatever the type is spelled - `AIMannedTurret.cpp`'s own `SLoaderSlot`
//     says what the two words are; this file needs the address only.
//   * `lbl_80419568` is `.sbss 0x80419568`, `size:0x8 data:4byte` (`symbols.txt:20791`).  No
//     unit of ours claims it - it sits in the `.sbss` gap between `AIMannedTurret.cpp`'s end
//     0x80419568 and `EmperorIngStage1.cpp`'s start 0x80419570 - so the matching build takes it
//     from dtk's own `auto_10_80419568_sbss.o`, and `grep -rn lbl_80419568 build/G2ME01/asm/`
//     finds no reader anywhere in the DOL: the store below is its only reference.  The port,
//     which links our sources and not dtk's objects, therefore loses it, and the host-only
//     definition at the end of this file is what keeps the port's undefined count where it
//     was.
//
// `AIMannedTurret.cpp:6-8` says this 8-byte setter is "deliberately NOT claimed: REL modules
// import it by its retail name, so it cannot be renamed and must stay in dtk's auto unit".
// What a carve must preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim
// in a `.c` file preserves it: the unmangled `fn_80227530` still lands in the DOL link, which
// is what module 1's `bl fn_80227530` resolves against (the same correction `Carve8022756C.c`
// records for the sentence below its range).  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a
// module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// already claimed units: `AIMannedTurret.cpp` ends at 0x80227530 and `EmperorIngStage1.cpp`
// starts at 0x80227540.  This range is exactly the gap between them.
//
// The directory is retail's own, taken from the nearest claimed range: this address is 0x2C
// bytes past the start of `MetroidPrime/ScriptLoader/AIMannedTurret.cpp`, so the code is that
// unit's neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.

/** `.sbss 0x80419560`, `symbols.txt:20790`, `size:0x8 data:4byte`: AIMannedTurret's loader
 *  slot, read by `LoadAIMannedTurret` in `AIMannedTurret.cpp` at `+0`.  That unit defines it;
 *  declared, never defined here. */
extern void* gLoader_AIMannedTurret;

/** `.sbss 0x80419568`, `symbols.txt:20791`, `size:0x8 data:4byte`: PirateRagDoll's loader
 *  slot.  Claimed by no unit of ours, so the matching build takes it from dtk's own
 *  `auto_10_80419568_sbss.o`; this file only stores into it. */
extern void* lbl_80419568;

/** PirateRagDoll's registration (module 50): `loader` is `&lbl_50_bss_188`, the module's own
 *  4-byte record holding its entity loader; `nullptr` is what `RELExit` passes on the way out. */
void fn_80227538(void* loader) { lbl_80419568 = loader; }

/** AIMannedTurret's registration (module 1): `loader` is `&lbl_1_bss_90`, the module's own
 *  4-byte record holding `&fn_1_D4`; `nullptr` is what `RELExit` passes on the way out. */
void fn_80227530(void* loader) { gLoader_AIMannedTurret = loader; }

#ifndef __MWERKS__
// Host-only definition of the one symbol this unit references that nothing else in `src/`
// defines.  The matching build does not compile this block, so `main.dol` still takes the real
// 0x80419568 from dtk's auto object above and both functions keep their retail bytes.  Without
// it the port's flat link - which carries our sources and not dtk's objects - loses
// `lbl_80419568`, and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh`
// reports, names it as a new undefined symbol.  Zero-initialised on purpose and unreferenced by
// the port: nothing in `src/` reads it.  The guard is `__MWERKS__` rather than `TARGET_PC` for
// the reason `src/MetroidPrime/Cameras/Carve801E7C14.c` records: the thing that must not happen
// is a second definition in the matching build, where dtk's `auto_10_80419568_sbss.o` owns the
// slot.
void* lbl_80419568 = 0;
#endif
