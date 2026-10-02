// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80200E3C_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80200E3C..0x80200E44, 0x8 = 8 bytes, 1 function:
//
//   fn_80200E3C    0x80200E3C  0x8    stw     r3, gLoader_SpacePirate@sda21(r0)
//                                          blr
//
// **What it is: SpacePirate's loader setter.**  It is the byte-shape twin of the matched
// `SetLoader_WallWalker__FPPFR13CStateManagerR12CInputStreamRC11CEntityIn` in
// `src/MetroidPrime/ScriptLoaderRel.cpp` (`stw r3, gLoader_WallWalker@sda21(r0)` / `blr`),
// which is the whole shape of the family: store the argument into the loader pointer's
// `.sbss` slot and return.
//
// Both callers are in module 72's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/SpacePirate/asm/MetroidPrime/ScriptObjects/
// CSpacePirateRel.s`):
//
//   * `RELExit` passes `li r3, 0` - the module tears the loader down on the way out.
//   * `fn_72_D4` (the module constructor, 0x6C bytes) copies one `FScriptLoader` and two
//     CodeWarrior pointers-to-member-function into `lbl_72_bss_24` word for word and then
//     calls `fn_80200E3C` with `r3 = lbl_72_bss_24`.  So the argument is that record's
//     address, and the record is 0x1C bytes: one 4-byte function pointer and two 12-byte
//     pointers-to-member-function (`auto_05_00000000_bss.s` gives `lbl_72_bss_24 size:0x1C`,
//     and `src/MetroidPrime/ScriptObjects/CSpacePirateRel.cpp` records the two signatures).
//
// The slot is `gLoader_SpacePirate` at `.sbss 0x80419358`, which `SpacePirate.cpp` already
// claims and already defines, so this unit claims `.text` only and takes the pointer as
// `extern`.  REL modules import this function by its retail name, so it cannot be renamed;
// defining it here keeps the unmangled `fn_80200E3C` in the DOL link, which is what
// module 72's `bl fn_80200E3C` resolves against.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the two
// neighbours are claimed units: `SpacePirate.cpp` ends at 0x80200E3C and
// `MetroidPrime/ScriptLoader/Kralee.cpp` starts at 0x80200E44.

/** The record module 72 hands over, 0x1C bytes as measured above.  Only its address is
 *  stored, so the field types describe the shape rather than a layout this unit reads;
 *  `CSpacePirateRel.cpp`'s `SSpacePirate_FuncPtrs` is the same record in C++ and says what
 *  the three pointers are.  Declared **above** the prototype below, not inside it: a struct
 *  named in a parameter list is scoped to that list, and the host build then rejects the
 *  definition as a conflicting type. */
struct SSpacePirateFuncPtrs {
  void* spacePirate;          /* FScriptLoader */
  unsigned int setTarget[3];  /* bool (CEntity::*)(const TUniqueId*) - 3 words */
  unsigned int clearTarget[3];/* void (CEntity::*)() - 3 words */
};

/** `SpacePirate.cpp` defines this in `.sbss 0x80419358` and reads `value` at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_SpacePirate`
 *  itself whatever the type is spelled. */
extern struct SSpacePirateFuncPtrs* gLoader_SpacePirate;

void fn_80200E3C(struct SSpacePirateFuncPtrs* loader) { gLoader_SpacePirate = loader; }