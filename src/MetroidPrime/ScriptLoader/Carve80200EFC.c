// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80200EFC_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80200EFC..0x80200F04, 0x8 = 8 bytes, 1 function:
//
//   fn_80200EFC    0x80200EFC  0x8    stw     r3, gLoader_Parasite@sda21(r0)
//                                          blr
//
// **What it is: Parasite's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (`stw r3,
// gLoader_SpacePirate@sda21(r0)` / `blr`), which is the whole shape of this family: store
// the argument into the loader pointer's `.sbss` slot and return.  There is nothing to
// discover here and no spelling to try - the twin already compiles to these bytes.
//
// Both callers are in module 47's own listing and neither calls it a setter by name, so the
// argument is read off them (`src/MetroidPrime/ScriptObjects/CParasiteRel.cpp`, .text
// 0x0..0x148, 17/17 matched, module sha1 unchanged):
//
//   * `RELExit` at 0x0BC (0x24) passes `li r3, 0` - the module tears its loaders down on the
//     way out.
//   * `fn_47_100` at 0x100 (0x48) fills one record with three `FScriptLoader`s in reverse
//     field order and calls `fn_80200EFC(&lbl_47_bss_50)`.  So the argument is that record's
//     address, and the record is **0xC bytes - three loaders**:
//     `build/G2ME01/Parasite/asm/auto_05_00000000_bss.s:54` reads
//     `# .bss:0x50 | 0x50 | size: 0xC` / `.obj lbl_47_bss_50, global`, and
//     `CParasiteRel.cpp` records the three signatures.  Three, not the family's usual one,
//     because Parasite registers `parasite`, `brizgee` and `crystallite`.
//
// The slot is `gLoader_Parasite` at `.sbss 0x80419368`, `type:object size:0x8 data:4byte`
// (`config/G2ME01/symbols.txt:20689`).  `src/MetroidPrime/ScriptLoader/Parasite.cpp` - a
// `Matching` unit claiming `.text 0x80200E78..0x80200EFC` and `.sbss
// 0x80419368..0x80419370` - already defines that slot (`SLoaderSlot { SParasiteLoaders*
// value; unsigned int padding; }`, 0x8 bytes) and already reads all three loaders through it,
// and its own header reserves these eight bytes for a separate unit: "The 8-byte setter at
// 0x80200EFC is deliberately NOT claimed: REL modules import it by its retail name, so it
// cannot be renamed and must stay in dtk's auto unit."  That is this file.  So this unit
// claims `.text` only and takes the pointer as `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_80200EFC` in the DOL link, which is what module 47's
// `bl fn_80200EFC` resolves against.  `CParasiteRel.cpp` declares it `extern "C"` for the
// same reason and is not compiled into the flat host link, so listing this file costs the
// port nothing: `build/goal/judge/undef.base.txt` carries 285 undefined names and
// `fn_80200EFC` is not one of them.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are both claimed units: `Parasite.cpp` ends at 0x80200EFC and `PillBug.cpp` starts at
// 0x80200F04, so this run slots between them with no unclaimed gap on either side.
//
// The directory is retail's own, taken from the nearest claimed range: `Parasite.cpp` claims
// `.text` 0x80200E78..0x80200EFC, which ends exactly where this begins, and
// `PillBug.cpp` claims 0x80200F04..0x80200F30, which starts where it ends.  The whole
// neighbourhood 0x80200E10..0x80200F30 is one loader-thunk family - SpacePirate, this
// setter, Kralee, Parasite, PillBug - so `MetroidPrime/ScriptLoader/` is where retail's own
// naming puts it.

/** The record module 47 hands over: three `FScriptLoader`s, 0xC bytes as measured above.
 *  Only its address is stored, so the field types describe the shape rather than a layout
 *  this unit reads; `Parasite.cpp`'s `SParasiteLoaders` is the same record in C++ and
 *  `CParasiteRel.cpp` says which of the three is which loader function is *not* settled.
 *  Declared **above** the prototype below, not inside it: a struct named in a parameter list
 *  is scoped to that list, and the host build then rejects the definition as a conflicting
 *  type. */
struct SParasiteLoaders {
  void* slot0; /* FScriptLoader - fn_47_1568 */
  void* slot1; /* FScriptLoader - fn_47_CF8   */
  void* slot2; /* FScriptLoader - fn_47_148   */
};

/** `Parasite.cpp` defines this in `.sbss 0x80419368` and reads `value` at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_Parasite` itself
 *  whatever the type is spelled. */
extern struct SParasiteLoaders* gLoader_Parasite;

void fn_80200EFC(struct SParasiteLoaders* loader) { gLoader_Parasite = loader; }