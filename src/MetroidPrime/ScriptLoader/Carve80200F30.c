// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80200F30_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80200F30..0x80200F38, 0x8 = 8 bytes, 1 function:
//
//   fn_80200F30    0x80200F30  0x8    stw     r3, gLoader_PillBug@sda21(r0)
//                                          blr
//
// **What it is: PillBug's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` and of the matched
// `fn_80200E70` in `src/MetroidPrime/ScriptLoader/Carve80200E70.c`, which between them are the
// whole shape of the family: store the argument into the loader pointer's `.sbss` slot and
// return.  The three differ in exactly the 16-bit `@sda21` displacement, because their slots
// are 8 bytes apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`,
// `gLoader_Kralee` at 0x80419360 is `90 6D 95 E0`, Parasite's own `fn_80200EFC` at 0x80419368
// is `90 6D 95 E8` and `gLoader_PillBug` at 0x80419370 is `90 6D 95 F0`.  Every other byte of
// all four is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 48's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/PillBug/asm/auto_00_00000000_text.s`):
//
//   * `RELExit` (0xBC, 0x24 bytes, `li r3, 0` / `bl fn_80200F30`) passes null - the module
//     tears the loader down on the way out.
//   * `fn_48_100` (0x100, 0x30 bytes, reached from `RELMain`) does `lis r4, fn_48_130@ha` ,
//     `lis r3, lbl_48_bss_0@ha` , `addi r0, r4, fn_48_130@l` , `stwu r0, lbl_48_bss_0@l(r3)`
//     and then `bl fn_80200F30` with `r3` still pointing at `lbl_48_bss_0`.  So the argument
//     is that record's address, and the record is **4 bytes**: one `FScriptLoader`
//     (`config/G2ME01/rels/PillBug/symbols.txt:147` gives `lbl_48_bss_0 = .bss:0x00000000;
//     size:0x4 data:4byte`, and `build/G2ME01/PillBug/asm/auto_05_00000000_bss.s` is the same
//     4 bytes).  So the type here is the bare function pointer, as in `Carve80200E70.c`; it is
//     a struct only in module 72, whose `lbl_72_bss_24` record also holds two
//     pointers-to-member-function and is 0x1C bytes.
//
// The slot is `gLoader_PillBug` at `.sbss 0x80419370` (size 0x8), which `PillBug.cpp` already
// claims and already defines - `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` -
// and whose `LoadPillBug` reads `value` at `+0`, which is why the store below hands it
// `&lbl_48_bss_0` and why that record is the bare pointer.  This unit claims `.text` only and
// takes the pointer as `extern`.  REL modules import this function by its retail name, so it
// cannot be renamed; defining it here keeps the unmangled `fn_80200F30` in the DOL link, which
// is what module 48's `bl fn_80200F30` resolves against.  `CPillBugRel.cpp` already declares
// it `void fn_80200F30(FScriptLoader* loader)` inside its `extern "C"` block and calls it
// under that name; MWCC does not encode a parameter type in a function name, so the two
// declarations agree.
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
// neighbours are claimed units: `PillBug.cpp` ends at 0x80200F30 and
// `MetroidPrime/ScriptLoader/Carve80201418.c` starts at 0x80201418.

/** The record module 48 hands over, 4 bytes as measured above.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_PillBug` itself
 *  whatever the type is spelled; `PillBug.cpp` spells the same slot `FScriptLoader* value` at
 *  `+0`, and this declaration describes the record the module's `.bss` holds. */
struct SPillBugLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `PillBug.cpp` defines this in `.sbss 0x80419370` and dereferences it at `+0`. */
extern struct SPillBugLoader* gLoader_PillBug;

void fn_80200F30(struct SPillBugLoader* loader) { gLoader_PillBug = loader; }