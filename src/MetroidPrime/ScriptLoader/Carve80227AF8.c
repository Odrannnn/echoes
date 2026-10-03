// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9771` and `:20766`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80227AF8_text.s:8-12`, and the body below is the C those bytes
// are the compilation of.
//
// .text 0x80227AF8..0x80227B00, 0x8 = 8 bytes, 1 function:
//
//   fn_80227AF8    0x80227AF8  0x8    stw     r3, gLoader_Rezbit@sda21(r0)
//                                          blr
//
// **What it is: Rezbit's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of `fn_8022756C` in the landed
// `src/MetroidPrime/ScriptLoader/Carve8022756C.c` (`EmperorIngStage1`'s, same shape), which
// is the whole shape of the family: store the argument into a loader pointer's `.sbss` slot
// and return.  This function never reads what it stores, so the file needs the slot's
// *address* only, not its element type.
//
// **Who calls it, and what the argument is.**  Both callers are in module 53's own listing
// (`config/G2ME01/config.yml:356` is `files/RelProd/Rezbit.rel`; module ID 53,
// `build/G2ME01/Rezbit/asm/auto_00_00000000_text.s`).  Neither calls it a setter by name, so
// the argument is read off them:
//
//   * `RELExit` (module `.text` 0xF4, 0x24 bytes, line 127 `li r3, 0x0`, call on line 131)
//     passes null - the module tears its registration down on the way out.
//   * `fn_53_138` (module `.text` 0x138, 0x30 bytes, the loader registration, call on line
//     159) does `lis r4, fn_53_168@ha` / `lis r3, lbl_53_bss_0@ha` /
//     `addi r0, r4, fn_53_168@l` / `stwu r0, lbl_53_bss_0@l(r3)` first, so `r3` is that
//     record's address when it calls.  `stwu` writes the slot and leaves its address in `r3`,
//     which is the same arrangement `src/MetroidPrime/ScriptObjects/CRezbitRel.cpp` spells
//     out for this module.
//
// The record is **four bytes**, not eight:
// `build/G2ME01/Rezbit/asm/auto_05_00000000_bss.s:7-10` gives `lbl_53_bss_0`
// (`.bss:0x0`, `size:0x4`) one word holding `&fn_53_168`, this module's entity loader - and
// `auto_05_00000000_bss.s:12-16` gives `lbl_53_bss_4` `size:0xC`, so the module has no
// CodeWarrior pointer-to-member-function in that record.  That is the check
// `docs/RUNNING_THE_DECOMP.md` tells a module head to run before copying a sibling's
// spelling: this record is the usual four bytes, so the register/argument shape transfers from
// the family and no pmf is copied.  The reader that proves the type is
// `src/MetroidPrime/ScriptLoader/Rezbit.cpp`'s `LoadRezbit`, a `Matching` unit, which reads
// the DOL slot as `(*gLoader_Rezbit.value)(mgr, input, info)`, i.e. word 0 of what is stored
// here is a `FScriptLoader`
// (`CEntity* (*)(CStateManager&, CInputStream&, CEntityInfo&)`, `include/MetroidPrime/ScriptLoader.hpp:23`).
// So the DOL slot holds a *pointer to* a loader slot, which is why the argument below is a
// record address rather than a loader.
//
// **The slot is not claimed here.**  `gLoader_Rezbit` is `.sbss 0x80419580`,
// `symbols.txt:20766` (`size:0x8 data:4byte`), which `Rezbit.cpp` already claims
// (`.sbss start:0x80419580 end:0x80419588`) and already defines, so this unit takes it as
// `extern`: a second definition is a duplicate the moment both objects are in the link.  MWCC
// does not encode a variable's type in its name, so the store below lands on the same address
// whatever the type is spelled - `Rezbit.cpp`'s own two-word `SLoaderSlot` says what the two
// words are and this file needs only the address.
//
// `Rezbit.cpp:6-8` says this 8-byte setter is "deliberately NOT claimed: REL modules import
// it by its retail name, so it cannot be renamed and must stay in dtk's auto unit".  That is
// half the requirement, and the same correction `Carve8022756C.c` and `Carve8022A570.c`
// record for their neighbours: what a carve must preserve is the **name**, and reproducing the
// `fn_<addr>` symbol verbatim in a `.c` file preserves it - the unmangled `fn_80227AF8` still
// lands in the DOL link, which is what module 53's two `bl fn_80227AF8` resolve against
// (`configure.py:2940-2943` already says the same of this import, and
// `CRezbitRel.cpp` declares it `extern "C"` for the same reason: an alias would be a
// different symbol and the call would resolve to nothing).  What a rename would break is the
// name; what this carve changes is only who supplies the bytes.  The neighbour's header is
// left as it is, exactly as the earlier carves in this family left theirs.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL,
// and a module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A
// one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt:9771` carries the `fn_80227AF8` placeholder and
// this file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_80227AF8v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Its own unit for the reason every carve has one: a unit may not claim two discontiguous
// ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link order"), and
// both neighbours are already claimed units - `Rezbit.cpp` ends at 0x80227AF8 and
// `RsfAudio.cpp` starts at 0x80227B00 (`symbols.txt:9772` gives
// `LoadRsfAudio__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x80227B00, 0x2C bytes).
// This range is exactly the gap between them.
//
// The directory is retail's own, taken from the nearest claimed range: this address is 0x2C
// bytes into `MetroidPrime/ScriptLoader/Rezbit.cpp`, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats
// a lane picking the directory it happened to own.

/** The first word is the `FScriptLoader` `LoadRezbit` calls; the second is the padding that
 *  makes the slot 8 bytes, as `symbols.txt:20766` records.  This file only stores into the
 *  first word, so the shape describes the retail layout rather than a layout it reads.
 *  `Rezbit.cpp` declares its own `SLoaderSlot` and defines the slot with it; C and C++ do
 *  not share a type across these two translation units, and MWCC does not encode a
 *  variable's type in its name, so the store below lands on the same address either way. */
struct SLoaderSlot {
  void* value;
  unsigned int padding;
};

/** `Rezbit.cpp` defines this in `.sbss 0x80419580` and reads `value` at `+0`.  Declared,
 *  never defined here. */
extern struct SLoaderSlot* gLoader_Rezbit;

/** Rezbit's registration (module 53): `loader` is `&lbl_53_bss_0`, the module's own 4-byte
 *  record holding `&fn_53_168`; `nullptr` is what `RELExit` passes on the way out. */
void fn_80227AF8(struct SLoaderSlot* loader) { gLoader_Rezbit = loader; }