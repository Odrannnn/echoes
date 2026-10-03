// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80200E70_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80200E70..0x80200E78, 0x8 = 8 bytes, 1 function:
//
//   fn_80200E70    0x80200E70  0x8    stw     r3, gLoader_Kralee@sda21(r0)
//                                          blr
//
// **What it is: Kralee's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`, which is the whole shape
// of the family: store the argument into the loader pointer's `.sbss` slot and return.  The
// two differ in exactly the 16-bit `@sda21` displacement, because their slots are 8 bytes
// apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_Kralee` at 0x80419360
// is `90 6D 95 E0`, and Parasite's own `fn_80200EFC` at 0x80419368 is `90 6D 95 E8`.  Every
// other byte of the three is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 37's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Kralee/asm/auto_00_0000009C_text.s`):
//
//   * `RELExit` (0x24 bytes, `li r3, 0`) passes null - the module tears the loader down on
//     the way out.
//   * `fn_37_10C` (0x30 bytes, reached from `RELMain`) does `lis r4, fn_37_13C@ha` ,
//     `addi r0, r4, fn_37_13C@l` , `lis r3, lbl_37_bss_8@ha` , `stwu r0, lbl_37_bss_8@l(r3)`
//     and then `bl fn_80200E70` with `r3` still pointing at `lbl_37_bss_8`.  So the argument
//     is that record's address, and the record is **4 bytes**: one `FScriptLoader`
//     (`auto_05_00000000_bss.s` gives `lbl_37_bss_8 size:0x4`, and the module's whole `.bss`
//     is only 0xC).  That is the difference from module 72, whose `lbl_72_bss_24` record is
//     0x1C bytes and is why `Carve80200E3C.c` types its argument as a struct; Kralee loads one
//     entity, so its record is the bare function pointer and the type here is that pointer.
//
// The slot is `gLoader_Kralee` at `.sbss 0x80419360`, which `Kralee.cpp` already claims and
// already defines - `SLoaderSlot { FScriptLoader* value; unsigned int padding; }`, 8 bytes,
// `.sbss 0x80419360..0x80419368` - and whose `LoadKralee` reads `value` at `+0`.  This unit
// claims `.text` only and takes the pointer as `extern`.  REL modules import this function by
// its retail name, so it cannot be renamed; defining it here keeps the unmangled
// `fn_80200E70` in the DOL link, which is what module 37's `bl fn_80200E70` resolves against.
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
// neighbours are claimed units: `Kralee.cpp` ends at 0x80200E70 and
// `MetroidPrime/ScriptLoader/Parasite.cpp` starts at 0x80200E78.

/** The record module 37 hands over, 4 bytes as measured above.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_Kralee` itself
 *  whatever the type is spelled; `Kralee.cpp` spells the same slot `FScriptLoader* value` at
 *  `+0`, and this declaration describes the record the module's `.bss` holds. */
struct SKraleeLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `Kralee.cpp` defines this in `.sbss 0x80419360` and dereferences it at `+0`. */
extern struct SKraleeLoader* gLoader_Kralee;

void fn_80200E70(struct SKraleeLoader* loader) { gLoader_Kralee = loader; }