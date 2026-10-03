// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218D24_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218D24..0x80218D2C, 0x8 = 8 bytes, 1 function:
//
//   fn_80218D24    0x80218D24  0x8    stw     r3, gLoader_ChozoGhost@sda21(r0)
//                                          blr
//
// **What it is: ChozoGhost's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's), of
// `fn_80213CB8` in `Carve80213CB8.c` (Sporb's) and of `fn_80218B68` in `Carve80218B68.c`
// (Metroid's): store the argument into the loader pointer's `.sbss` slot and return.  Those
// four differ from this one only in the 16-bit `@sda21` displacement, because the slots sit 8
// bytes apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_Kralee` at
// 0x80419360 is `90 6D 95 E0`, `gLoader_SporbBase` at 0x804193A8 is `90 6D 96 28`,
// `gLoader_MetroidAlpha` at 0x80419418 is `90 6D 96 98`, and `gLoader_ChozoGhost` at 0x80419448
// is `90 6D 96 C8`.  Every other byte of the five is identical, including the `4E 80 00 20`
// `blr`.
//
// Both callers are in module 8's own listing and neither calls it a setter by name, so the
// argument is read off them
// (`build/G2ME01/ChozoGhost/asm/MetroidPrime/ScriptObjects/CChozoGhostRel.s`):
//
//   * `RELExit` (`.text:0x414`, 0x24 bytes) does `li r3, 0x0` and then `bl fn_80218D24` - the
//     module tears the loader down on the way out.
//   * `fn_8_458` (`.text:0x458`, 0x30 bytes, which `RELMain` at `.text:0x438` reaches through
//     `bl fn_8_458`) fills the record in place first: `lis r4, fn_8_488@ha`, then
//     `addi r0, r4, fn_8_488@l`, then `stwu r0, lbl_8_bss_C@l(r3)`, and only then calls
//     `fn_80218D24` with `r3` still on `lbl_8_bss_C`.
//
// So the argument is that word's address, and the record is **0x4 bytes**: a bare
// `FScriptLoader` with no pointers-to-member-function after it.  `auto_05_00000000_bss.s` gives
// `lbl_8_bss_C size:0x4` in the module (against `lbl_8_bss_0 size:0xC` and `lbl_8_bss_10
// size:0x10`), and the DOL says the same thing: `LoadChozoGhost` at 0x80218CF8 loads the slot
// and calls word 0 of it and nothing else - `lwz r6, gLoader_ChozoGhost@sda21(r0); lwz r12,
// 0x0(r6); mtctr r12; bctrl` in
// `build/G2ME01/asm/MetroidPrime/ScriptLoader/ChozoGhost.s` - so there is no second field for
// anything to occupy.  That is what distinguishes it from its neighbours in the family, which
// are wider: Parasite's record (`fn_80200EFC`) is 0xC and Metroid's (`fn_80218B68`) is 0x10.
//
// The slot is `gLoader_ChozoGhost` at `.sbss 0x80419448..0x80419450`, which `ChozoGhost.cpp`
// already claims and already defines (`SLoaderSlot { FScriptLoader* value; unsigned int
// padding; }`), so this unit claims `.text` only and takes the pointer as `extern`.  That
// unit's header says the setter "is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit".  The carve is what
// retires that exception without renaming anything: the import is the plain `fn_80218D24`
// (checked in module 8's own `build/G2ME01/ChozoGhost/ChozoGhost.preplf` import table), so
// defining it unmangled here keeps the symbol in the DOL link and module 8's two
// `bl fn_80218D24` still resolve against it.
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
// are claimed units: `ChozoGhost.cpp` ends at 0x80218D24 and `Tryclops.cpp` starts at
// 0x80218D2C.  So this is exactly the gap between them, as `Carve80218B68.c` is the gap
// between `MetroidAlpha.cpp` and `GunTurretBase.cpp`; that geometry splits and links cleanly,
// it is not the cycle.

/** The record module 8 hands over, 0x4 bytes as measured above.  Only its address is stored,
 *  so the field type describes the shape rather than a layout this unit reads;
 *  `ChozoGhost.cpp`'s `SLoaderSlot` reads word 0 of the same value.  Declared **above** the
 *  prototype below, not inside it: a struct named in a parameter list is scoped to that list,
 *  and the host build then rejects the definition as a conflicting type. */
struct SChozoGhostLoader {
  void* loader; /* FScriptLoader */
};

/** `ChozoGhost.cpp` defines this in `.sbss 0x80419448` and reads `value` at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_ChozoGhost` itself
 *  whatever the type is spelled. */
extern struct SChozoGhostLoader* gLoader_ChozoGhost;

void fn_80218D24(struct SChozoGhostLoader* loader) { gLoader_ChozoGhost = loader; }