// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9755` and `:20792`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8022756C_text.s:10-13`, and the body below is the C those bytes
// are the compilation of.
//
// .text 0x8022756C..0x80227574, 0x8 = 8 bytes, 1 function:
//
//   fn_8022756C    0x8022756C  0x8    stw     r3, gLoader_EmperorIngStage1@sda21(r0)
//                                          blr
//
// **What it is: EmperorIngStage1's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`), which is the whole shape of the family:
// store the argument into a loader pointer's `.sbss` slot and return.
//
// **Who calls it, and what the argument is.**  Both callers are in module 16's own listing
// (`build/G2ME01/EmperorIngStage1/asm/`, and `config/G2ME01/config.yml:92` is
// `files/RelProd/EmperorIngStage1.rel`, module ID 16).  Neither calls it a setter by name, so
// the argument is read off them:
//
//   * `fn_16_A270` (0x30 bytes, `auto_00_00000000_text.s:11544-11561`) is the registration
//     path: `lis r3, lbl_16_bss_0@ha` / `lis r4, fn_16_A2A0@ha` /
//     `stwu r0, lbl_16_bss_0@l(r3)` and then calls it, so `r3` is that record's address and
//     the record is the 4 bytes `auto_05_00000000_bss.s:8-11` gives `lbl_16_bss_0`
//     (`.bss:0x0 size:0x4`): one word holding `&fn_16_A2A0`, this module's entity loader.  It
//     is reached from `_prolog` through `fn_16_A250` (`auto_00_0000C2A4_text.s:83`).
//   * `fn_16_A22C` (0x24 bytes, `auto_00_00000000_text.s:11519-11528`) is the teardown path:
//     `li r3, 0` and then the same call, reached from `_epilog`
//     (`auto_00_0000C2A4_text.s:70`).
//
// The word at +0 of the slot is a `FScriptLoader`, i.e.
// `CEntity* (*)(CStateManager&, CInputStream&, const CEntityInfo&)`
// (`include/MetroidPrime/ScriptLoader.hpp:13`), which is what the reader uses: the claim
// immediately below is `MetroidPrime/ScriptLoader/EmperorIngStage1.cpp`
// (.text 0x80227540..0x8022756C), whose `LoadEmperorIngStage1` is
// `(*gLoader_EmperorIngStage1.value)(mgr, input, info)` - it loads the slot's word and calls
// it.  This function never reads what it stores, so the file needs the slot's *address*
// only and takes the type as `struct SLoaderSlot*`, the two-word shape every other unit in
// this family uses.
//
// **The slot is not claimed here.**  `gLoader_EmperorIngStage1` is `.sbss 0x80419570`,
// `symbols.txt:20792` (`size:0x8`), which `EmperorIngStage1.cpp` already claims and already
// defines, so this unit references it as `extern` - the arrangement
// `src/MetroidPrime/ScriptLoader/Carve8022A570.c` uses for
// `gLoader_EmperorIngStage2Tentacle`, and the same reason: a second definition of it is a
// duplicate the moment both objects are in the link.
//
// `EmperorIngStage1.cpp`'s header says this 8-byte setter is "deliberately NOT claimed:
// REL modules import it by its retail name, so it cannot be renamed and must stay in dtk's
// auto unit".  That is half the requirement, and the same correction `Carve8022A570.c`
// records for the two setters around `EmperorIngStage2Tentacle`: what a carve must preserve
// is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file preserves
// it - the unmangled `fn_8022756C` lands in the DOL link, which is what module 16's
// `bl fn_8022756C` resolves against.  What a rename would break is the name; what this carve
// changes is only who supplies the bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL,
// and a module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A
// one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit for the reason every carve has one: a unit may not claim two discontiguous
// ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link order"), and
// both neighbours are already claimed units - `EmperorIngStage1.cpp` ends at 0x8022756C and
// `OctopedeSegment.cpp` starts at 0x80227574.  This range is exactly the gap between them.
// The 8-byte gap at 0x80227530..0x80227540 above it is a separate unclaimed range and is
// deliberately not claimed here.
//
// The directory is retail's own, taken from the nearest claimed range: this address is 0x2C
// bytes into `MetroidPrime/ScriptLoader/EmperorIngStage1.cpp`, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats
// a lane picking the directory it happened to own.

/** The first word is the `FScriptLoader` `LoadEmperorIngStage1` calls; the second is the
 *  padding that makes the slot 8 bytes, as `symbols.txt:20792` records.  This file only
 *  stores into the first word, so the shape describes the retail layout rather than a layout
 *  it reads.  `EmperorIngStage1.cpp` declares its own `SLoaderSlot` and defines the slot with
 *  it; C and C++ do not share a type across these two translation units, and MWCC does not
 *  encode a variable's type in its name, so the store below lands on the same address
 *  either way. */
struct SLoaderSlot {
  void* value;
  unsigned int padding;
};

/** `EmperorIngStage1.cpp` defines this in `.sbss 0x80419570` and reads `value` at `+0`.
 *  Declared, never defined here. */
extern struct SLoaderSlot* gLoader_EmperorIngStage1;

/** EmperorIngStage1's registration: `loader` is `&lbl_16_bss_0`, the module's own 4-byte
 *  record holding `&fn_16_A2A0`; `nullptr` is what `fn_16_A22C` passes on the way out. */
void fn_8022756C(struct SLoaderSlot* loader) { gLoader_EmperorIngStage1 = loader; }
