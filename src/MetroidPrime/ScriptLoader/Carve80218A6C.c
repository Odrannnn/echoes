// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9476`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218A6C_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218A6C..0x80218A74, 0x8 = 8 bytes, 1 function:
//
//   fn_80218A6C    0x80218A6C  0x8    stw     r3, gLoader_MediumIng@sda21(r0)
//                                          blr
//
// **What it is: MediumIng's loader setter.**  It stores the pointer it is handed into
// `gLoader_MediumIng`, the module's small-data slot, and returns - the whole shape of this
// family.  The instruction pair is byte-for-byte the one the matched `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` has (SpacePirate's, the same carve shape
// with another slot), and the one the two neighbours have with their own slots:
// `fn_802188E4` in `src/MetroidPrime/ScriptLoader/Carve802188E4.c` (DarkSamus's) and
// `fn_80232834` in `src/MetroidPrime/ScriptLoader/Carve80232834.c` (FogOverlay's).
// It never reads back what it stored, so it needs the slot's address only, not its element
// type.
//
// **The name is retail's, so the unit is `.c`.**  `symbols.txt:9476` carries the `fn_<addr>`
// placeholder - the module is one of the 526-of-535 whose retail names are `fn_<offset>` - and
// the MediumIng module imports that exact name from the DOL:
//
//     strings build/G2ME01/MediumIng/MediumIng.plf | grep 80218A6C
//     fn_80218A6C
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve
// to nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it,
// which mangles `fn_80218A6C` into something the module does not import; a `.c` unit is
// compiled with `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition
// below *is* the symbol.  That is also why the unit is a `.c` rather than a `.cpp`.
//
// **Who calls it, and what the argument is.**  Both call sites are in module 41
// (`config/G2ME01/config.yml:287-290`, `files/RelProd/MediumIng.rel`, sha1
// `4ff29124bdf74564c3d33e6ae5948071479d9e61`, which still matches
// `orig/G2ME01/files/RelProd/MediumIng.rel`) and neither is in a listing that names it, so the
// argument is read off the module's own listing
// (`build/G2ME01/MediumIng/asm/MetroidPrime/ScriptObjects/CMediumIngRel.s`):
//
//   * `RELExit` (`.text` 0xDC) materialises `li r3, 0x0` at 0xE4 (line 115) and calls on
//     line 117's `bl fn_80218A6C` - a null loader, the module tearing the registration down.
//   * `fn_41_120` (`.text` 0x120, 0x30 bytes; the loader registration `RELMain` calls at 0x10C,
//     line 129) does `lis r4, fn_41_150@ha` / `lis r3, lbl_41_bss_10@ha` (lines 140-141) and
//     `addi r0, r4, fn_41_150@l` / `stwu r0, lbl_41_bss_10@l(r3)` (lines 143-144) - a store
//     *through* the slot address, which writes the module's own record and leaves r3 holding
//     that record's address - then `bl fn_80218A6C` on line 145.
//
// So the argument is `&lbl_41_bss_10`, the module's `.bss:0x10` record, and
// `build/G2ME01/MediumIng/asm/auto_05_00000000_bss.s` gives it `size:0x4` - **one** word, unlike
// SpacePirate's and DarkSamus's 0x1C-byte `*_FuncPtrs` records.  It is that module's single
// `FScriptLoader`, which `src/MetroidPrime/ScriptObjects/CMediumIngRel.cpp:149` already writes
// out as `fn_80218A6C(&lbl_41_bss_10)` against a `void fn_80218A6C(FScriptLoader*)`
// declaration at `:134` and an `FScriptLoader lbl_41_bss_10` at `:139`/`:141`; that is where
// this reading comes from, and its `#else` branch at `:169` is the same call with a null.
// The type below names the parameter only; nothing in this unit reads through it.
//
// **The slot belongs to `MediumIng.cpp`, so this unit claims `.text` only.**  `gLoader_MediumIng`
// is `.sbss 0x804193F8`, `size:0x8 data:4byte` (`symbols.txt:20712`) - the `value` pointer plus
// the `padding` word `SLoaderSlot` declares (`MediumIng.cpp:12-17`) - and it is claimed and
// defined by `MetroidPrime/ScriptLoader/MediumIng.cpp`
// (`config/G2ME01/splits.txt:1646-1648`), whose `.text` ends exactly where this range starts.
// Its reader, `LoadMediumIng` at 0x80218A40 (`MediumIng.cpp:19-21`), loads the slot with the
// same small-data displacement this store uses - `lwz r6, gLoader_MediumIng@sda21(r0)` at
// 0x80218A4C - and calls member 0 of what it read, which is what shows the slot holds one
// pointer-sized value.  Declaring the slot `extern` here is what keeps it defined in exactly
// one unit: a second definition is a duplicate the moment both objects are in the link, and
// `link_gap.py` counts only what is *missing*, so it cannot see one.  MWCC does not encode a
// variable's type in its name, so this `extern` references `gLoader_MediumIng` itself whatever
// the type is spelled.
//
// **No host-only block, unlike `Carve80227530.c`.**  That unit had to define the slot itself
// because no unit of ours claimed it, so the port's flat link would have lost it and
// `tools/link_check.sh --strict` would have named a new undefined symbol.  Every symbol this
// unit references is already defined in `src/`: `MediumIng.cpp` is in `files.cmake` twice (in
// the source list at :1197 and in the "every remaining DOL object" append at :1624) and defines
// `gLoader_MediumIng`, and nothing here calls anything.  So the port's undefined count cannot
// move.  `CMediumIngRel.cpp` is deliberately *not* in `files.cmake` - it is one of the 17
// module-entry files excluded because each defines a `RELMain`/`RELExit` pair - so nothing in
// the host build sees this definition and nothing in it can conflict with one.
//
// `MediumIng.cpp:6-8` said the setter is "deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit".  What a carve
// must preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
// preserves it: the unmangled `fn_80218A6C` still lands in the DOL link, which is what module
// 41's two `bl fn_80218A6C` resolve against.  What a rename would break is the name; what
// this carve changes is only who supplies the bytes.  The same correction is recorded in
// `Carve80200E3C.c` for the identical sentence in `SpacePirate.cpp`, in `Carve802188E4.c` for
// the one in `DarkSamus.cpp`, and in `Carve80232834.c` for the one in `FogOverlay.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// already claimed units: `MediumIng.cpp` ends at 0x80218A6C and
// `MetroidPrime/ScriptLoader/MinorIng.cpp` starts at 0x80218A74 (`splits.txt:1653-1655`).
// This range is exactly the gap between them, 8 bytes, and nothing else claims it.
//
// The directory is retail's own, taken from the nearest claimed range: `MediumIng.cpp` owns
// 0x80218A40..0x80218A6C and this address is the next 8 bytes, so the code is that unit's
// neighbourhood, and the whole `ScriptLoader` block here is one family.  For an anonymous
// function that is the only evidence there is.  `docs/research/rel_loaders.md` lists every
// loader in the family - it indexes the `Load*` thunks rather than these setters, so it needs
// no row for this range.

/** The record module 41 hands over: `.bss:0x10`, `size:0x4`, one `FScriptLoader`.  Named for
 *  the parameter only; nothing in this unit reads through it.  Declared at file scope rather
 *  than inside the parameter list, because a tag first seen in a prototype is scoped to that
 *  list and the host build then rejects the definition as a conflicting type. */
struct SMediumIngFuncPtr;

/** `.sbss 0x804193F8`, `symbols.txt:20712`, `size:0x8 data:4byte`: MediumIng's loader slot,
 *  read by `LoadMediumIng` in `MediumIng.cpp` at `+0`.  That unit defines it; declared, never
 *  defined here. */
extern struct SMediumIngFuncPtr* gLoader_MediumIng;

/** MediumIng's registration (module 41): `loader` is `&lbl_41_bss_10`, the module's own
 *  4-byte record holding `&fn_41_150`; a null is what `RELExit` passes on the way out. */
void fn_80218A6C(struct SMediumIngFuncPtr* loader) { gLoader_MediumIng = loader; }