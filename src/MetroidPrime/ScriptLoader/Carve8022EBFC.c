// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9938-9940` and `:20781`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8022EBFC_text.s:8-12`, and the body below is the C those bytes
// are the compilation of.  The byte evidence is the pristine disc, not our own build:
// `python3 tools/dol_read.py 0x8022EBFC 0x8 orig/G2ME01/sys/main.dol` reads
// `90 6D 98 78 4E 80 00 20`.
//
// .text 0x8022EBFC..0x8022EC04, 0x8 = 8 bytes, 1 function:
//
//   fn_8022EBFC    0x8022EBFC  0x8    stw     r3, gLoader_DestructableBarrier@sda21(r0)
//                                          blr
//
// **What it is: DestructableBarrier's loader setter.**  It stores the pointer it is handed
// into `gLoader_DestructableBarrier`, that script object's `.sbss` slot, and returns - the
// whole shape of this family of setters.  The instruction pair is byte-for-byte the one
// `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`) has with another slot,
// and the two nearer carves of the same family - `fn_80227AF8`
// (`src/MetroidPrime/ScriptLoader/Carve80227AF8.c:109`, `gLoader_Rezbit`) and `fn_80235DCC`
// (`src/MetroidPrime/ScriptLoader/Carve80235DCC.c:91`, `gLoader_DarkSamusBattleStage`) -
// have it too.  This function never reads what it stores, so the file needs the slot's
// *address* only, not its element type.
//
// **Who calls it, and what the argument is.**  Both callers are in module 13's own listing
// (`config/G2ME01/config.yml:105` is `files/RelProd/DestructibleBarrier.rel`;
// `build/G2ME01/DestructibleBarrier/asm/auto_00_00000000_text.s`).  Neither calls it a
// setter by name, so the argument is read off them:
//
//   * `RELExit` (module `.text` 0x2C, 0x24 = 36 bytes, `li r3, 0x0` on line 27, the call on
//     line 29) passes null - the module tears its registration down on the way out.
//   * `fn_13_70` (module `.text` 0x70, 0x30 = 48 bytes, the loader registration; lines
//     52-57) does `lis r4, fn_13_A0@ha` / `lis r3, lbl_13_bss_0@ha` / `addi r0, r4,
//     fn_13_A0@l` / `stwu r0, lbl_13_bss_0@l(r3)` first, so `r3` is that record's address
//     when it calls.  `stwu` writes the cell and leaves its address in `r3`, which is the
//     same arrangement `src/MetroidPrime/ScriptObjects/CDestructibleBarrierRel.cpp:96-102`
//     spells out for this module.
//
// The record is **four bytes**, not eight:
// `build/G2ME01/DestructibleBarrier/asm/auto_05_00000000_bss.s:8-11` gives `lbl_13_bss_0`
// (`.bss:0x0`, `size:0x4`) one word holding `&fn_13_A0` - this module's own entity loader,
// `config/G2ME01/rels/DestructibleBarrier/symbols.txt:5` - and that is the *only* object in
// the module's `.bss`, so there is no CodeWarrior pointer-to-member-function in this record
// and nothing to copy.  That is the `.bss`-size check
// `docs/RUNNING_THE_DECOMP.md` tells a module head to run before copying a sibling's
// spelling: this record is the usual four bytes, so the register/argument shape transfers
// from the family unchanged.  The reader that proves the type is
// `src/MetroidPrime/ScriptLoader/DestructableBarrier.cpp`'s `LoadDestructableBarrier`, a
// `Matching` unit, which reads the DOL slot as
// `(*gLoader_DestructableBarrier.value)(mgr, input, info)` (line 19), i.e. word 0 of what
// is stored here is a `FScriptLoader`
// (`CEntity* (*)(CStateManager&, CInputStream&, CEntityInfo&)`, `include/MetroidPrime/
// ScriptLoader.hpp:23`).  So the DOL slot holds a *pointer to* a loader cell, which is why
// the argument below is a record address rather than a loader.
//
// **The slot is not claimed here.**  `gLoader_DestructableBarrier` is `.sbss 0x804195F8`,
// `symbols.txt:20781` (`size:0x8 data:4byte`), claimed and defined by
// `MetroidPrime/ScriptLoader/DestructableBarrier.cpp` (`config/G2ME01/splits.txt:1975-1977`,
// definition at its line 16 with its own two-word `SLoaderSlot`), so this unit takes it as
// `extern`: a second definition is a duplicate the moment both objects are in the link.  MWCC
// does not encode a variable's type in its name, so the store below lands on the same
// address whatever the type is spelled - `DestructableBarrier.cpp`'s `SLoaderSlot` says what
// the two words are and this file needs only the address.
//
// **The name is retail's, so the unit is `.c`.**  `symbols.txt:9939` declares
// `fn_8022EBFC = .text:0x8022EBFC; // type:function size:0x8 align:4` - there is no better
// name for it - and the module **imports that exact name** from the DOL:
//
//     strings build/G2ME01/DestructibleBarrier/DestructibleBarrier.plf | grep 8022EB
//     fn_8022EBFC
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve
// to nothing.  A `.cpp` unit would mangle `fn_8022EBFC` into something the module does not
// import; a `.c` unit is compiled with `-lang=c`, so the definition below *is* the symbol.
// `CDestructibleBarrierRel.cpp:84-88` declares it `extern "C"` inside the module's own
// `extern "C"` block for the same reason.
//
// `DestructableBarrier.cpp:6-8` says this 8-byte setter is "deliberately NOT claimed: REL
// modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
// unit".  That is half the requirement, and the same correction `Carve80227AF8.c` records
// for its neighbour: what a carve must preserve is the **name**, and reproducing the
// `fn_<addr>` symbol verbatim in a `.c` file preserves it - the unmangled `fn_8022EBFC`
// still lands in the DOL link, which is what module 13's two `bl fn_8022EBFC` resolve
// against.  What a rename would break is the name; what this carve changes is only who
// supplies the bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL,
// and a module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A
// one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ...
// link order"), and because both neighbours are already claimed units -
// `DestructableBarrier.cpp` ends at 0x8022EBFC (`LoadDestructableBarrier...`, 0x2C bytes,
// `symbols.txt:9938`) and `SwampBossStage2.cpp` starts at 0x8022EC04
// (`LoadSwampBossStage2...`, `symbols.txt:9940`).  This range is exactly the gap between
// them, 8 bytes of it, so nothing is left unclaimed either side.
//
// The directory is retail's own, taken from the nearest claimed range: this address is
// 0x2C bytes into `MetroidPrime/ScriptLoader/DestructableBarrier.cpp`, so the code is that
// unit's neighbourhood.  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `DestructableBarrier.cpp` is in the port's source list and defines
// `gLoader_DestructableBarrier`, and nothing here calls anything.

/** The first word is the `FScriptLoader` `LoadDestructableBarrier` calls; the second is the
 *  padding that makes the slot 8 bytes, as `symbols.txt:20781` records.  This file only stores
 *  into the first word, so the shape describes the retail layout rather than a layout it
 *  reads.  `DestructableBarrier.cpp` declares its own `SLoaderSlot` and defines the slot
 *  with it; C and C++ do not share a type across these two translation units, and MWCC does
 *  not encode a variable's type in its name, so the store below lands on the same address
 *  either way. */
struct SLoaderSlot {
  void* value;
  unsigned int padding;
};

/** `DestructableBarrier.cpp:16` defines this in `.sbss 0x804195F8` and reads `value` at `+0`.
 *  Declared, never defined here. */
extern struct SLoaderSlot* gLoader_DestructableBarrier;

/** DestructableBarrier's registration (module 13): `loader` is `&lbl_13_bss_0`, the module's
 *  own 4-byte record holding `&fn_13_A0`; `nullptr` is what `RELExit` passes on the way out. */
void fn_8022EBFC(struct SLoaderSlot* loader) { gLoader_DestructableBarrier = loader; }