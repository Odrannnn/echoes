// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218DF4_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218DF4..0x80218DFC, 0x8 = 8 bytes, 1 function:
//
//   fn_80218DF4    0x80218DF4  0x8    stw     r3, gLoader_DarkTrooper@sda21(r0)
//                                          blr
//
// The raw encodings dtk printed are `90 6D 96 E8` and `4E 80 00 20`; the first is
// `stw r3, 0x96E8(r13)`, and dtk renders the `@sda21` relocation as `(r0)`.  With G2ME01's
// `_SDA_BASE_` at 0x8041FD80 (`tools/sda.py`), 0x8041FD80 - 0x96E8 = 0x80419468, which is
// `gLoader_DarkTrooper` - so the annotation and the bytes agree.
//
// **What it is: DarkTrooper's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's), of the
// matched `fn_802189D0` in `Carve802189D0.c` (SandBoss's) and of the matched `fn_80218B68` in
// `Carve80218B68.c` (MetroidAlpha's), which between them are the whole shape of the family:
// store the argument into the loader pointer's `.sbss` slot and return.  Those four differ
// only in the 16-bit `@sda21` displacement, because their slots are 8 bytes or a little more
// apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_SandBoss` at
// 0x804193E0 is `90 6D 96 60`, `gLoader_MetroidAlpha` at 0x80419418 is `90 6D 96 98`, and
// `gLoader_DarkTrooper` at 0x80419468 is `90 6D 96 E8`.  Every other byte of all four is
// identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 12's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/DarkTrooper/asm/auto_00_00000000_text.s`):
//
//   * `RELExit` (`.text:0xB8`, 0x24 bytes) does `li r3, 0x0` at 0xC0 and then
//     `bl fn_80218DF4` at 0xC8 - the module tears the loader down on the way out.
//   * `fn_12_FC` (`.text:0xFC`, 0x30 bytes, which `RELMain` reaches through `bl fn_12_FC` at
//     0xE8) does `lis r4, fn_12_12C@ha` , `lis r3, lbl_12_bss_0@ha` , `addi r0, r4,
//     fn_12_12C@l` , `stwu r0, lbl_12_bss_0@l(r3)` and then `bl fn_80218DF4` at 0x118 with r3
//     still pointing at `lbl_12_bss_0`.  So the argument is that record's address, and the
//     record is **4 bytes**: one `FScriptLoader` - `config/G2ME01/rels/DarkTrooper/symbols.txt`
//     gives `lbl_12_bss_0 = .bss:0x00000000; size:0x4 data:4byte`, the same bare pointer
//     `Carve802189D0.c` finds in module 55.  (Only the multi-word records are structs:
//     `Carve80200E3C.c`'s is 0x1C bytes because module 72's also holds two
//     pointers-to-member-function.)  `src/MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp:109`
//     declares this very function `void fn_80218DF4(FScriptLoader* loader)` inside its
//     `extern "C"` block and calls it under that name at both sites above; MWCC does not
//     encode a parameter type in a function name, so the two declarations agree.
//
// The slot is `gLoader_DarkTrooper` at `.sbss 0x80419468..0x80419470`
// (`config/G2ME01/symbols.txt:20726`, `type:object size:0x8 data:4byte`), which
// `DarkTrooper.cpp` already claims and already defines - `SLoaderSlot { FScriptLoader* value;
// unsigned int padding; }` - and whose `LoadDarkTrooper` reads `value` at `+0`, which is why
// the store below hands it `&lbl_12_bss_0` and why that record is the bare pointer.
// `DarkTrooper.cpp`'s own header used to reserve these eight bytes for a separate unit: "The
// 8-byte setter at 0x80218DF4 is deliberately NOT claimed: REL modules import it by its retail
// name, so it cannot be renamed and must stay in dtk's auto unit."  That is this file.  So
// this unit claims `.text` only and takes the pointer as `extern`.  REL modules import this
// function by its retail name - `fn_80218DF4` is in module 12's own import table
// (`build/G2ME01/DarkTrooper/DarkTrooper.preplf`) - so it cannot be renamed; defining it here
// keeps the unmangled `fn_80218DF4` in the DOL link, which is what module 12's two
// `bl fn_80218DF4` calls resolve against.  It is not in `files.cmake`'s module list either -
// nothing here calls anything, and `gLoader_DarkTrooper` is already defined by
// `DarkTrooper.cpp`, which the port compiles, so this file adds no undefined symbol.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `DarkTrooper.cpp` ends at 0x80218DF4 and
// `MetroidPrime/ScriptLoader/GlowBug.cpp` starts at 0x80218DFC, so this run slots between them
// with no unclaimed gap on either side.

/** The record module 12 hands over, 4 bytes as measured above.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_DarkTrooper`
 *  itself whatever the type is spelled; `DarkTrooper.cpp` spells the same slot
 *  `FScriptLoader* value` at `+0`, and this declaration describes the record the module's
 *  `.bss` holds.  Declared **above** the prototype below, not inside it: a struct named in a
 *  parameter list is scoped to that list, and the host build then rejects the definition as a
 *  conflicting type. */
struct SDarkTrooperLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `DarkTrooper.cpp` defines this in `.sbss 0x80419468` and dereferences it at `+0`. */
extern struct SDarkTrooperLoader* gLoader_DarkTrooper;

void fn_80218DF4(struct SDarkTrooperLoader* loader) { gLoader_DarkTrooper = loader; }