// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_802189D0_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x802189D0..0x802189D8, 0x8 = 8 bytes, 1 function:
//
//   fn_802189D0    0x802189D0  0x8    stw     r3, gLoader_SandBoss@sda21(r0)
//                                          blr
//
// **What it is: SandBoss's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's) and of the
// matched `fn_80200F30` in `src/MetroidPrime/ScriptLoader/Carve80200F30.c` (PillBug's), which
// between them are the whole shape of the family: store the argument into the loader pointer's
// `.sbss` slot and return.  The three differ in exactly the 16-bit `@sda21` displacement,
// because their slots are 0x128 bytes apart - `gLoader_SpacePirate` at 0x80419358 is
// `90 6D 95 D8`, `gLoader_PillBug` at 0x80419370 is `90 6D 95 F0`, and `gLoader_SandBoss` at
// 0x804193E0 is `90 6D 96 60`.  Every other byte of all three is identical, including the
// `4E 80 00 20` `blr`.
//
// Both callers are in module 55's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/SandBoss/asm/auto_00_00000000_text.s`):
//
//   * `RELExit` (0x104, 0x24 bytes, `li r3, 0` / `bl fn_802189D0`) passes null - the module
//     tears the loader down on the way out.
//   * `fn_55_148` (0x148, 0x30 bytes, reached from `RELMain`) does `lis r4, fn_55_178@ha` ,
//     `lis r3, lbl_55_bss_4@ha` , `addi r0, r4, fn_55_178@l` , `stwu r0, lbl_55_bss_4@l(r3)`
//     and then `bl fn_802189D0` with `r3` still pointing at `lbl_55_bss_4`.  So the argument
//     is that record's address, and the record is **4 bytes**: one `FScriptLoader`
//     (`config/G2ME01/rels/SandBoss/symbols.txt:549` gives `lbl_55_bss_4 = .bss:0x00000004;
//     size:0x4 data:4byte`, and `build/G2ME01/SandBoss/asm/auto_05_00000000_bss.s` is the
//     same 4 bytes, the middle of that module's three `.bss` objects).  So the type here is the
//     bare function pointer, as in `Carve80200F30.c`; it is a struct only in module 72, whose
//     `lbl_72_bss_24` record also holds two pointers-to-member-function and is 0x1C bytes.
//
// The slot is `gLoader_SandBoss` at `.sbss 0x804193E0` (`config/G2ME01/symbols.txt:20709`,
// `type:object size:0x8 data:4byte`), which `SandBoss.cpp` already claims and already defines -
// `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` - and whose `LoadSandBoss`
// reads `value` at `+0`, which is why the store below hands it `&lbl_55_bss_4` and why that
// record is the bare pointer.  `SandBoss.cpp`'s own header used to reserve these eight bytes
// for a separate unit: "The 8-byte setter at 0x802189D0 is deliberately NOT claimed: REL
// modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
// unit."  That is this file.  So this unit claims `.text` only and takes the pointer as
// `extern`.  REL modules import this function by its retail name, so it cannot be renamed;
// defining it here keeps the unmangled `fn_802189D0` in the DOL link, which is what module 55's
// `bl fn_802189D0` resolves against.  `CSandBossRel.cpp` already declares it `void
// fn_802189D0(FScriptLoader* loader)` inside its `extern "C"` block and calls it under that
// name; MWCC does not encode a parameter type in a function name, so the two declarations
// agree.  It is not in `files.cmake`'s module list either - nothing here calls anything, and
// `gLoader_SandBoss` is already defined by `SandBoss.cpp`, which the port compiles, so this
// file adds no undefined symbol.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the two
// neighbours are claimed units: `SandBoss.cpp` ends at 0x802189D0 and
// `MetroidPrime/ScriptLoader/FlyingPirate.cpp` starts at 0x802189D8, so this run slots between
// them with no unclaimed gap on either side.

/** The record module 55 hands over, 4 bytes as measured above.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_SandBoss` itself
 *  whatever the type is spelled; `SandBoss.cpp` spells the same slot `FScriptLoader* value` at
 *  `+0`, and this declaration describes the record the module's `.bss` holds.  Declared
 *  **above** the prototype below, not inside it: a struct named in a parameter list is scoped
 *  to that list, and the host build then rejects the definition as a conflicting type. */
struct SSandBossLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `SandBoss.cpp` defines this in `.sbss 0x804193E0` and dereferences it at `+0`. */
extern struct SSandBossLoader* gLoader_SandBoss;

void fn_802189D0(struct SSandBossLoader* loader) { gLoader_SandBoss = loader; }