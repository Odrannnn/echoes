// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218D8C_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218D8C..0x80218D94, 0x8 = 8 bytes, 1 function:
//
//   fn_80218D8C    0x80218D8C  0x8    stw     r3, gLoader_WispTentacle@sda21(r0)
//                                          blr
//
// The raw encodings dtk printed are `90 6D 96 D8` and `4E 80 00 20`; the first is
// `stw r3, 0x96D8(r13)`, and dtk renders the `@sda21` relocation as `(r0)`.  With G2ME01's
// `_SDA_BASE_` at 0x8041FD80 (`tools/sda.py`), 0x8041FD80 - 0x80419458 = 0x6928 and the
// 16-bit displacement is 0x10000 - 0x6928 = 0x96D8, which is `gLoader_WispTentacle`, so the
// annotation and the bytes agree.
//
// **What it is: WispTentacle's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's), of the
// matched `fn_802189D0` in `Carve802189D0.c` (SandBoss's), of `fn_80218B68` in
// `Carve80218B68.c` (MetroidAlpha's), of `fn_80218D58` in `Carve80218D58.c` (Tryclops's) and
// of `fn_80218DF4` in `Carve80218DF4.c` (DarkTrooper's), which between them are the whole
// shape of the family: store the argument into the loader pointer's `.sbss` slot and return.
// Those five differ only in the 16-bit `@sda21` displacement, i.e. only in which slot they
// name - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_SandBoss` at
// 0x804193E0 is `90 6D 96 60`, `gLoader_MetroidAlpha` at 0x80419418 is `90 6D 96 98`,
// `gLoader_Tryclops` at 0x80419450 is `90 6D 96 D0`, `gLoader_WispTentacle` at 0x80419458 is
// `90 6D 96 D8` and `gLoader_DarkTrooper` at 0x80419468 is `90 6D 96 E8`.  Every other byte of
// all six is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 86's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/WispTentacle/asm/auto_00_000003C8_text.s`):
//
//   * `RELExit` (`.text:0x2C`, 0x3F4..0x418, 0x24 bytes) does `li r3, 0x0` and then
//     `bl fn_80218D8C` - the module tears the loader down on the way out.
//   * `fn_86_438` (`.text:0x70`, 0x438..0x468, 0x30 bytes, which `RELMain` at `.text:0x50`
//     reaches through `bl fn_86_438`) does `lis r4, fn_86_468@ha` , `lis r3,
//     lbl_86_bss_0@ha` , `addi r0, r4, fn_86_468@l` , `stwu r0, lbl_86_bss_0@l(r3)` and then
//     `bl fn_80218D8C` with r3 still pointing at `lbl_86_bss_0`.  So the argument is that
//     record's address, and the record is **4 bytes**: one `FScriptLoader` -
//     `config/G2ME01/rels/WispTentacle/symbols.txt:167` gives
//     `lbl_86_bss_0 = .bss:0x00000000; size:0x4 data:4byte`, and
//     `build/G2ME01/WispTentacle/asm/auto_05_00000000_bss.s` is that module's only `.bss`
//     object, the same bare 4 bytes `Carve802189D0.c` and `Carve80218DF4.c` find in modules
//     55 and 12.  `fn_86_468`, the word it stores, takes r3/r4/r5 and reads `0x8(r4)`, which
//     is the `(CStateManager&, CInputStream&, CEntityInfo&)` FScriptLoader signature.  (Only
//     the multi-word records are structs: `Carve80200E3C.c`'s is 0x1C bytes because module
//     72's also holds two pointers-to-member-function.)
//
// The slot is `gLoader_WispTentacle` at `.sbss 0x80419458..0x80419460`
// (`config/G2ME01/symbols.txt:20724`, `type:object size:0x8 data:4byte`), which
// `WispTentacle.cpp` already claims and already defines - `SLoaderSlot { FScriptLoader*
// value; unsigned int padding; }` - and whose `LoadWispTentacle` reads `value` at `+0`, which
// is why the store below hands it `&lbl_86_bss_0` and why that record is the bare pointer.
// `WispTentacle.cpp`'s own header used to reserve these eight bytes for a separate unit: "The
// 8-byte setter at 0x80218D8C is deliberately NOT claimed: REL modules import it by its retail
// name, so it cannot be renamed and must stay in dtk's auto unit."  That is this file.  So
// this unit claims `.text` only and takes the pointer as `extern`.  REL modules import this
// function by its retail name - `fn_80218D8C` is in module 86's own import table
// (`build/G2ME01/WispTentacle/WispTentacle.preplf`) alongside the mangled retail names, and
// `auto_00_000003C8_text.o` carries `R_PPC_REL24 fn_80218D8C` on both call sites - so it
// cannot be renamed; defining it here keeps the unmangled `fn_80218D8C` in the DOL link, which
// is what those two calls resolve against.  There is no `src/MetroidPrime/ScriptObjects/
// CWispTentacleRel.cpp` in the tree (unlike `CGrenchlerRel.cpp` / `CSandBossRel.cpp`), so
// there is no second declaration of the name to reconcile; MWCC does not encode a parameter
// type in a function name, so it would agree with the one below anyway.
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
// are claimed units: `WispTentacle.cpp` ends at 0x80218D8C and
// `MetroidPrime/ScriptLoader/SpankWeed.cpp` starts at 0x80218D94, so this run slots between
// them with no unclaimed gap on either side.

/** The record module 86 hands over, 4 bytes as measured above.  Only its address is stored,
 *  so the field type describes the shape rather than a layout this unit reads; MWCC does not
 *  encode a variable's type in its name, so the `extern` below references
 *  `gLoader_WispTentacle` itself whatever the type is spelled, and `WispTentacle.cpp` spells
 *  the same slot `FScriptLoader* value` at `+0`.  Declared **above** the prototype below, not
 *  inside it: a struct named in a parameter list is scoped to that list, and the host build
 *  then rejects the definition as a conflicting type. */
struct SWispTentacleLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `WispTentacle.cpp` defines this in `.sbss 0x80419458` and dereferences it at `+0`. */
extern struct SWispTentacleLoader* gLoader_WispTentacle;

void fn_80218D8C(struct SWispTentacleLoader* loader) { gLoader_WispTentacle = loader; }