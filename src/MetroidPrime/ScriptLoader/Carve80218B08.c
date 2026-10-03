// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218B08_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218B08..0x80218B10, 0x8 = 8 bytes, 1 function:
//
//   fn_80218B08    0x80218B08  0x8    stw     r3, gLoader_Blogg@sda21(r0)
//                                          blr
//
// **What it is: Blogg's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's) and of the
// matched `fn_802189D0` in `src/MetroidPrime/ScriptLoader/Carve802189D0.c` (SandBoss's), which
// between them are the whole shape of the family: store the argument into the loader pointer's
// `.sbss` slot and return.  They differ in exactly the 16-bit `@sda21` displacement, because
// their slots are different addresses - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`,
// `gLoader_Blogg` at 0x80419410 is `90 6D 96 90`, and `gLoader_SandBoss` at 0x804193E0 is
// `90 6D 96 60`.  Every other byte of all three is identical, including the `4E 80 00 20`
// `blr`.
//
// Both callers are in module 7's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Blogg/asm/MetroidPrime/ScriptObjects/CBloggRel.s`):
//
//   * `RELExit` (0x94, 0x24 bytes, `li r3, 0` / `bl fn_80218B08`) passes null - the module
//     tears the loader down on the way out.
//   * `fn_7_D8` (0xD8, 0x30 bytes, reached from `RELMain` at 0xB8) does `lis r4,
//     fn_7_108@ha` , `lis r3, lbl_7_bss_10@ha` , `addi r0, r4, fn_7_108@l` , `stwu r0,
//     lbl_7_bss_10@l(r3)` and then `bl fn_80218B08` with `r3` still pointing at
//     `lbl_7_bss_10`.  So the argument is that record's address, and the record is **4 bytes**:
//     one `FScriptLoader` (`config/G2ME01/rels/Blogg/symbols.txt:393` gives `lbl_7_bss_10 =
//     .bss:0x00000010; size:0x4 data:4byte`, and `build/G2ME01/Blogg/asm/
//     auto_05_00000000_bss.s` is the same 4 bytes, the third of that module's six `.bss`
//     objects).  So the type here is the bare function pointer, as in `Carve80200F30.c`; it is
//     a struct only in module 72, whose `lbl_72_bss_24` record also holds two
//     pointers-to-member-function and is 0x1C bytes.
//
// The slot is `gLoader_Blogg` at `.sbss 0x80419410` (`config/G2ME01/symbols.txt:20715`,
// `type:object size:0x8 data:4byte`), which `Blogg.cpp` already claims and already defines -
// `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` - and whose `LoadBlogg` reads
// `value` at `+0`, which is why the store below hands it `&lbl_7_bss_10` and why that record is
// the bare pointer.  `Blogg.cpp`'s own header used to reserve these eight bytes for a separate
// unit: "The 8-byte setter at 0x80218B08 is deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit."  That is this
// file.  So this unit claims `.text` only and takes the pointer as `extern`.  REL modules
// import this function by its retail name, so it cannot be renamed; defining it here keeps the
// unmangled `fn_80218B08` in the DOL link, which is what module 7's `bl fn_80218B08` resolves
// against.  `CBloggRel.cpp` already declares it `void fn_80218B08(FScriptLoader* loader)`
// inside its `extern "C"` block and calls it under that name; MWCC does not encode a parameter
// type in a function name, so the two declarations agree.  `CBloggRel.cpp` is deliberately not
// in `files.cmake` - nothing here calls anything, and `gLoader_Blogg` is already defined by
// `Blogg.cpp`, which the port compiles, so this file adds no undefined symbol to the port link.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order").  `Blogg.cpp` ends at 0x80218B08
// and `MetroidPrime/ScriptLoader/MetroidAlpha.cpp` starts at 0x80218B3C, so this run slots
// between them; the remainder of the old auto range, 0x80218B10..0x80218B3C, is
// `CMetroidAlpha::OnDockTouch`, a real-named C++ function that stays with dtk.

/** The record module 7 hands over, 4 bytes as measured above.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_Blogg` itself
 *  whatever the type is spelled; `Blogg.cpp` spells the same slot `FScriptLoader* value` at
 *  `+0`, and this declaration describes the record the module's `.bss` holds.  Declared
 *  **above** the prototype below, not inside it: a struct named in a parameter list is scoped
 *  to that list, and the host build then rejects the definition as a conflicting type. */
struct SBloggLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `Blogg.cpp` defines this in `.sbss 0x80419410` and dereferences it at `+0`. */
extern struct SBloggLoader* gLoader_Blogg;

void fn_80218B08(struct SBloggLoader* loader) { gLoader_Blogg = loader; }