// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8021FA4C_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8021FA4C..0x8021FA54, 0x8 = 8 bytes, 1 function:
//
//   fn_8021FA4C    0x8021FA4C  0x8    stw     r3, gLoader_StoneToad@sda21(r0)
//                                          blr
//
// **What it is: StoneToad's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200EFC` in `src/MetroidPrime/ScriptLoader/Carve80200EFC.c` (`stw r3,
// gLoader_Parasite@sda21(r0)` / `blr`), which is the whole shape of this family: store the
// argument into the loader pointer's `.sbss` slot and return.  There is nothing to discover
// here and no spelling to try - the twin already compiles to these bytes.
//
// Both callers are in module 77's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/StoneToad/asm/auto_00_00000428_text.s`; module 77
// is `files/RelProd/StoneToad.rel` per `config/G2ME01/config.yml:488`):
//
//   * `RELExit` at 0x454 (0x24) passes `li r3, 0` - the module tears its loader down on the
//     way out.
//   * `fn_77_498` at 0x498 (0x30) stores the module's own `fn_77_4C8` into
//     `lbl_77_bss_0` and calls `fn_8021FA4C` with `r3 = lbl_77_bss_0`.  So the argument is
//     that record's address, and the record is **0x4 bytes - one `FScriptLoader`**:
//     `build/G2ME01/StoneToad/asm/auto_05_00000000_bss.s` gives `lbl_77_bss_0 size:0x4`.  One,
//     which is the family's usual count; the neighbour `fn_80200E3C` passes a 0x1C record
//     (one loader and two pointers-to-member-function) only because SpacePirate's module
//     registers more than a loader.
//
// The slot is `gLoader_StoneToad` at `.sbss 0x80419528`, `type:object size:0x8 data:4byte`
// (`config/G2ME01/symbols.txt:20753`).  `src/MetroidPrime/ScriptLoader/StoneToad.cpp` - a
// `Matching` unit claiming `.text 0x8021FA20..0x8021FA4C` and `.sbss 0x80419528..0x80419530`
// - already defines that slot (`struct SLoaderSlot { FScriptLoader* value; unsigned int
// padding; }`, 0x8 bytes), already calls member 0 through it in `LoadStoneToad`, and its own
// header reserves these eight bytes for a separate unit: "The 8-byte setter at 0x8021FA4C is
// deliberately NOT claimed: REL modules import it by its retail name, so it cannot be
// renamed and must stay in dtk's auto unit."  That is this file.  So this unit claims
// `.text` only and takes the pointer as `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_8021FA4C` in the DOL link, which is what module 77's
// `bl fn_8021FA4C` resolves against.  `build/goal/judge/undef.base.txt` carries the port's
// undefined names and `fn_8021FA4C` is not one of them, so listing this file costs the port
// nothing.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order
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
// are both claimed units: `StoneToad.cpp` ends at 0x8021FA4C and `Coin.cpp` starts at
// 0x8021FA54, so this run slots between them with no unclaimed gap on either side.
//
// The directory is retail's own, taken from the nearest claimed range: `StoneToad.cpp`
// claims `.text 0x8021FA20..0x8021FA4C`, which ends exactly where this begins, and
// `Coin.cpp` claims 0x8021FA54..0x8021FA80, which starts where it ends.  The whole
// neighbourhood is one loader-thunk family - DigitalGuardian, Shredder,
// FrontEndDataNetwork, StoneToad, this setter, Coin - so `MetroidPrime/ScriptLoader/` is
// where retail's own naming puts it.

/** The record module 77 hands over: one `FScriptLoader`, 0x4 bytes as measured above, padded
 *  out to the slot's 0x8.  Only its address is stored, so the field types describe the shape
 *  rather than a layout this unit reads; `StoneToad.cpp`'s `SLoaderSlot` is the same 0x8-byte
 *  record in C++ and `LoadStoneToad` there calls `value` at `+0`.  Declared **above** the
 *  prototype below, not inside it: a struct named in a parameter list is scoped to that list,
 *  and the host build then rejects the definition as a conflicting type. */
struct SStoneToadLoaderSlot {
  void* loader;           /* FScriptLoader - fn_77_4C8 */
  unsigned int padding;
};

/** `StoneToad.cpp` defines this in `.sbss 0x80419528` and reads `value` at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_StoneToad` itself
 *  whatever the type is spelled. */
extern struct SStoneToadLoaderSlot* gLoader_StoneToad;

void fn_8021FA4C(struct SStoneToadLoaderSlot* loader) { gLoader_StoneToad = loader; }