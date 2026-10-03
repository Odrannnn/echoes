// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218D58_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218D58..0x80218D60, 0x8 = 8 bytes, 1 function:
//
//   fn_80218D58    0x80218D58  0x8    stw     r3, gLoader_Tryclops@sda21(r0)
//                                          blr
//
// **What it is: Tryclops' loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's), of
// `fn_80218B68` in `Carve80218B68.c` (MetroidAlpha's, same carve shape) and of `fn_80213CB8`
// in `Carve80213CB8.c` (Sporb's): store the argument into the loader pointer's `.sbss` slot
// and return.  The four differ only in the 16-bit `@sda21` displacement, because their slots
// are 8 bytes apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_SporbBase`
// at 0x804193A8 is `90 6D 96 28`, `gLoader_MetroidAlpha` at 0x80419418 is `90 6D 96 98`, and
// `gLoader_Tryclops` at 0x80419450 is `90 6D 96 D0`.  Every other byte of the four is
// identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 81's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Tryclops/asm/MetroidPrime/ScriptObjects/
// CTryclopsRel.s`):
//
//   * `RELExit` (`.text:0x104`, 0x24 bytes) does `li r3, 0x0` and then `bl fn_80218D58` - the
//     module tears the loader down on the way out.
//   * `fn_81_148` (`.text:0x148`, 0x30 bytes, the loader registration `RELMain` reaches) does
//     `lis r3, lbl_81_bss_30@ha`, materialises `fn_81_178` with `lis r4` + `addi r0`, stores it
//     with `stwu r0, lbl_81_bss_30@l(r3)`, and only then calls `fn_80218D58` with `r3` still
//     pointing at `lbl_81_bss_30`.  So the argument is that record's address, and the record is
//     **0x4 bytes**: one 4-byte `FScriptLoader` and nothing else
//     (`config/G2ME01/rels/Tryclops/symbols.txt:225` gives
//     `lbl_81_bss_30 = .bss:0x30 size:0x4 data:4byte`).  That is the narrowest form of this
//     family - SpacePirate's is 0x1C and MetroidAlpha's 0x10, both because their module passes a
//     CodeWarrior pointer-to-member-function after the loader - and the DOL says so twice over:
//     `LoadTryclops` at 0x80218D2C reads word 0 as the loader (`lwz r6,
//     gLoader_Tryclops@sda21(r0); lwz r12, 0x0(r6); mtctr r12; bctrl`,
//     `build/G2ME01/asm/MetroidPrime/ScriptLoader/Tryclops.s`), and nothing in the DOL reads
//     `+4` off this slot, where MetroidAlpha's `OnDockTouch__13CMetroidAlpha` does
//     `addi r12, r5, 0x4; bl __ptmf_scall` on it.
//
// The slot is `gLoader_Tryclops` at `.sbss 0x80419450..0x80419458`, which `Tryclops.cpp` already
// claims and already defines (`SLoaderSlot { FScriptLoader* value; unsigned int padding; }`),
// so this unit claims `.text` only and takes the pointer as `extern`.  That unit's header
// records why the setter was deliberately left unclaimed - "REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit" - and the carve is
// what removes that exception without renaming anything: the import name is the plain
// `fn_80218D58` that `config/G2ME01/symbols.txt:9502` already gives it (line 281 of
// `strings build/G2ME01/Tryclops/Tryclops.preplf`), so defining it here keeps the unmangled
// `fn_80218D58` in the DOL link and module 81's two `bl fn_80218D58` calls still resolve
// against it.  `CTryclopsRel.cpp:124` already declares the same plain C symbol and calls it
// from `fn_81_148` and from `RELExit`, so this change moves a definition and renames nothing.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `Tryclops.cpp` ends at 0x80218D58 and
// `MetroidPrime/ScriptLoader/WispTentacle.cpp` starts at 0x80218D60.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `Tryclops.cpp` is in the port's source list and defines `gLoader_Tryclops`, and nothing here
// calls anything.

/** The record module 81 hands over, 0x4 bytes as measured above: a bare `FScriptLoader`, with
 *  no member pointer after it.  Only its address is stored, so the field types describe the
 *  shape rather than a layout this unit reads, and `CTryclopsRel.cpp`'s `lbl_81_bss_30` is the
 *  same slot spelled `FScriptLoader`.  Declared **above** the prototype below, not inside it: a
 *  struct named in a parameter list is scoped to that list, and the host build then rejects the
 *  definition as a conflicting type. */
struct STryclopsLoaderSlot {
  void* loader; /* FScriptLoader */
};

/** `Tryclops.cpp` defines this in `.sbss 0x80419450` and reads `value` at `+0`; MWCC does not
 *  encode a variable's type in its name, so this references `gLoader_Tryclops` itself whatever
 *  the type is spelled. */
extern struct STryclopsLoaderSlot* gLoader_Tryclops;

void fn_80218D58(struct STryclopsLoaderSlot* record) { gLoader_Tryclops = record; }