// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8021FA18_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8021FA18..0x8021FA20, 0x8 = 8 bytes, 1 function:
//
//   fn_8021FA18    0x8021FA18  0x8    stw     r3, gLoader_FrontEndDataNetwork@sda21(r0)
//                                          blr
//
// **What it is: FrontEndDataNetwork's loader setter.**  It is the byte-shape twin of the
// matched `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's),
// of `fn_80218D24` in `Carve80218D24.c` (ChozoGhost's) and of `fn_80213CB8` in
// `Carve80213CB8.c` (Sporb's): store the argument into the loader pointer's `.sbss` slot
// and return.  Those differ from this one only in the 16-bit `@sda21` displacement, because
// the slots sit 8 bytes apart in the same run of `.sbss` - `gLoader_SpacePirate` at
// 0x80419358 is `90 6D 95 D8`, `gLoader_ChozoGhost` at 0x80419448 is `90 6D 96 C8`, and
// `gLoader_FrontEndDataNetwork` at 0x80419520 is `90 6D 97 A0`.  Every other byte is the
// same, including the `4E 80 00 20` `blr`.  The slot's own arithmetic checks out:
// `_SDA_BASE_` = 0x8041FD80 minus 0x5860 is 0x80419520, so the store is 0x97A0 as a signed
// halfword.
//
// Both callers are in module 59 (`ScriptFrontEndDataNetwork`) and neither calls it a setter
// by name, so the argument is read off them
// (`build/G2ME01/ScriptFrontEndDataNetwork/asm/MetroidPrime/ScriptObjects/CFrontEndDataNetworkRel.s`):
//
//   * `RELExit` (`.text:0x380`, 0x24 bytes) does `li r3, 0x0` and then `bl fn_8021FA18` -
//     the module tears the loader down on the way out.
//   * `fn_59_3C4` (`.text:0x3C4`, 0x30 bytes, which `RELMain` at `.text:0x3A4` reaches
//     through `bl fn_59_3C4`) fills the record in place first: `lis r3, lbl_59_bss_4@ha`,
//     then `stwu r0, lbl_59_bss_4@l(r3)` with `r0 = fn_59_3F4`, and only then calls
//     `fn_8021FA18` with `r3` still on `lbl_59_bss_4`.
//
// So the argument is that word's address, and the record is **0x4 bytes**: a bare
// `FScriptLoader` with no pointers-to-member-function after it.
// `config/G2ME01/rels/ScriptFrontEndDataNetwork/symbols.txt:149` gives `lbl_59_bss_4 =
// .bss:0x00000004; // type:object size:0x4 data:4byte`, and dtk's own
// `auto_05_00000000_bss.s` says the same thing (`lbl_59_bss_4 size:0x4` against
// `lbl_59_bss_0 size:0x4`).  The DOL says it too: `LoadFrontEndDataNetwork` at 0x8021F9EC
// loads the slot and calls word 0 of it and nothing else, so there is no second field for
// anything to occupy.  That is what distinguishes this from its wider relatives in the
// family - SpacePirate's record (`fn_80200E3C`) is 0x1C and Metroid's (`fn_80218B68`) is
// 0x10 - and it makes this the same shape as ChozoGhost's.
//
// The slot is `gLoader_FrontEndDataNetwork` at `.sbss 0x80419520..0x80419528`, which
// `MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp` already claims and already defines
// (`SLoaderSlot { FScriptLoader* value; unsigned int padding; }`, read at `+0`), so this
// unit claims `.text` only and takes the pointer as `extern`.  That unit's header says the
// setter "is deliberately NOT claimed: REL modules import it by its retail name, so it
// cannot be renamed and must stay in dtk's auto unit".  The carve is what retires that
// exception without renaming anything: the import is the plain `fn_8021FA18` (checked in
// module 59's own `build/G2ME01/ScriptFrontEndDataNetwork/ScriptFrontEndDataNetwork.preplf`
// import table, and named by `config/G2ME01/symbols.txt:9612` in the DOL), so defining it
// unmangled here keeps the symbol in the DOL link and module 59's two `bl fn_8021FA18`
// still resolve against it.  `src/MetroidPrime/ScriptObjects/CFrontEndDataNetworkRel.cpp`
// declares it `extern "C"` and never defines it, so there is exactly one definition.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two
// neighbours are claimed units whose boundaries are exactly this gap:
// `FrontEndDataNetwork.cpp` ends at 0x8021FA18 and `StoneToad.cpp` starts at 0x8021FA20.
// So the claim spans no unclaimed gap.  The third unit of the same trio is
// `CannonBallLoaderSet.cpp` at 0x8021FAB4, whose `SetLoader_CannonBall` is this shape with
// a *mangled* retail name, which is why that one is a `.cpp` and this one is not.

/** The record module 59 hands over, 0x4 bytes as measured above.  Only its address is
 *  stored, so the field type describes the shape rather than a layout this unit reads;
 *  `FrontEndDataNetwork.cpp`'s `SLoaderSlot` reads word 0 of the same value.  Declared
 *  **above** the prototype below, not inside it: a struct named in a parameter list is
 *  scoped to that list, and the host build then rejects the definition as a conflicting
 *  type. */
struct SFrontEndDataNetworkLoader {
  void* loader; /* FScriptLoader */
};

/** `FrontEndDataNetwork.cpp` defines this in `.sbss 0x80419520` and reads `value` at `+0`;
 *  MWCC does not encode a variable's type in its name, so this references
 *  `gLoader_FrontEndDataNetwork` itself whatever the type is spelled. */
extern struct SFrontEndDataNetworkLoader* gLoader_FrontEndDataNetwork;

void fn_8021FA18(struct SFrontEndDataNetworkLoader* loader) { gLoader_FrontEndDataNetwork = loader; }