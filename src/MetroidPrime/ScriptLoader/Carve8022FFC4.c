// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8022FFC4_text.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x8022FFC4..0x8022FFCC, 0x8 = 8 bytes, 1 function:
//
//   fn_8022FFC4    0x8022FFC4  0x8    stw     r3, gLoader_IngBoostBallGuardian@sda21(r0)
//                                          blr
//
// The raw encodings dtk printed are `90 6D 98 90` and `4E 80 00 20`; the first is
// `stw r3, 0x9890(r13)`, and dtk renders the `@sda21` relocation as `(r0)`.  With G2ME01's
// `_SDA_BASE_` at 0x8041FD80 (`tools/sda.py`), 0x8041FD80 - 0x80419610 = 0x6770 and the
// 16-bit displacement is 0x10000 - 0x6770 = 0x9890, which is `gLoader_IngBoostBallGuardian`,
// so the annotation and the bytes agree.
//
// **What it is: IngBoostBallGuardian's loader setter.**  It is the byte-shape twin of the
// matched `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's) and
// of the landed `fn_80227AF8` in `Carve80227AF8.c` (Rezbit's), which is the whole shape of the
// family: store the argument into the loader pointer's `.sbss` slot and return.  Those two
// differ from this one only in the 16-bit `@sda21` displacement, i.e. only in which slot they
// name.  Every other byte of all three is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 30's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/IngBoostBallGuardian/asm/auto_00_00000000_text.s`,
// module ID 30 = `files/RelProd/IngBoostBallGuardian.rel`, `config/G2ME01/config.yml:221`):
//
//   * `RELExit` (`.text:0xBC`, 0x24 bytes) does `li r3, 0x0` and then `bl fn_8022FFC4` - the
//     module tears the loader down on the way out.
//   * `fn_30_100` (`.text:0x100`, 0x30 bytes, which `RELMain` at `.text:0xE0` reaches through
//     `bl fn_30_100`) does `lis r4, fn_30_130@ha`, `lis r3, lbl_30_bss_6C@ha`, `addi r0, r4,
//     fn_30_130@l`, `stwu r0, lbl_30_bss_6C@l(r3)` and then `bl fn_8022FFC4` with r3 still
//     pointing at `lbl_30_bss_6C`.  So the argument is that record's address, and the record is
//     **4 bytes**: one `FScriptLoader` - `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:648`
//     gives `lbl_30_bss_6C = .bss:0x0000006C; type:object size:0x4 data:4byte`, and
//     `build/G2ME01/IngBoostBallGuardian/asm/auto_05_00000000_bss.s:77-79` is that module's
//     own `.bss` object at the same size.  It is `lbl_30_bss_6C` and not `lbl_30_bss_0` because
//     this module's `.bss:0x0` is a different 0x8-byte object.  The reader that proves the type
//     is `src/MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp`'s `LoadIngBoostBallGuardian`,
//     a `Matching` unit, which reads the DOL slot as `(*gLoader_IngBoostBallGuardian.value)(mgr,
//     input, info)`, i.e. word 0 of what is stored here is an `FScriptLoader`
//     (`include/MetroidPrime/ScriptLoader.hpp`).  So the DOL slot holds a *pointer to* a loader
//     slot, which is why the argument below is a record address rather than a loader.  (Only
//     the multi-word records are structs: `Carve80200E3C.c`'s is 0x1C bytes because module 72's
//     also holds two pointers-to-member-function.)
//
// The slot is `gLoader_IngBoostBallGuardian` at `.sbss 0x80419610..0x80419618`
// (`config/G2ME01/symbols.txt:20784`, `type:object size:0x8 data:4byte`), which
// `IngBoostBallGuardian.cpp` already claims and already defines - `SLoaderSlot { FScriptLoader*
// value; unsigned int padding; }` - and whose `LoadIngBoostBallGuardian` reads `value` at `+0`,
// which is why the store below hands it `&lbl_30_bss_6C` and why that record is the bare
// pointer.  So this unit claims `.text` only and takes the pointer as `extern`: a second
// definition is a duplicate the moment both objects are in the link.  MWCC does not encode a
// variable's type in its name, so the store lands on the same address whatever the type is
// spelled.
//
// `IngBoostBallGuardian.cpp`'s own header used to reserve these eight bytes for a separate
// unit: "The 8-byte setter at 0x8022FFC4 is deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit."  That is half
// the requirement, and this file is the other half: REL module 30 imports this function by
// that exact retail name - `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000000_text.o`
// carries `R_PPC_REL24 fn_8022FFC4` at both call sites (section offsets 0xcc and 0x11c) and the
// plain name is in `build/G2ME01/IngBoostBallGuardian/IngBoostBallGuardian.preplf` - so it
// cannot be renamed; defining it here keeps the unmangled `fn_8022FFC4` in the DOL link, which
// is what those two calls resolve against.  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.  `src/MetroidPrime/ScriptObjects/
// CIngBoostBallGuardianRel.cpp:133` declares the same name `extern "C"` for the same reason,
// and that file is not in `files.cmake`, so the two are never in one host link.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a
// module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A
// one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `IngBoostBallGuardian.cpp` ends at 0x8022FFC4 and
// `MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp` starts at 0x8022FFCC, so this run slots
// between them with no unclaimed gap on either side and the 8 bytes are the whole gap.
//
// The directory is retail's own, taken from the nearest claimed range: this address is 0x2C
// bytes into `MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp`, so the code is that unit's
// neighbourhood.

/** The record module 30 hands over, 4 bytes as measured above.  Only its address is stored, so
 *  the record type here describes the shape rather than a layout this unit reads.  Declared
 *  **above** the prototype below, not inside it: a struct named in a parameter list is scoped to
 *  that list, and the host build then rejects the definition as a conflicting type. */
struct SIngBoostBallGuardianLoader;

/** `IngBoostBallGuardian.cpp` defines this in `.sbss 0x80419610` and dereferences it at `+0`;
 *  MWCC does not encode a variable's type in its name, so this references
 *  `gLoader_IngBoostBallGuardian` itself whatever the type is spelled. */
extern struct SIngBoostBallGuardianLoader* gLoader_IngBoostBallGuardian;

/** IngBoostBallGuardian's registration (module 30): `loader` is `&lbl_30_bss_6C`, the module's
 *  own 4-byte record holding `&fn_30_130`; `nullptr` is what `RELExit` passes on the way out. */
void fn_8022FFC4(struct SIngBoostBallGuardianLoader* loader) {
  gLoader_IngBoostBallGuardian = loader;
}
