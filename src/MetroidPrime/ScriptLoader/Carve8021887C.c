// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9461`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8021887C_text.s` while this range was unclaimed, and the
// body below is the C those bytes are the compilation of.
//
// .text 0x8021887C..0x80218884, 0x8 = 8 bytes, 1 function:
//
//   SetLoader_Sandworm__FP18SSandworm_FuncPtrs
//       0x8021887C  0x8 = 8 bytes   stw     r3, gLoader_Sandworm@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the Sandworm module's loader setter.**  It stores the pointer it
// is handed into `gLoader_Sandworm`, the module's small-data slot, and returns - the whole shape
// of this family of setters.  The instruction pair is byte-for-byte the one the matched
// `SetLoader_WallWalker__FPPFR13CStateManagerR12CInputStreamRC11CEntityIn` in
// `src/MetroidPrime/ScriptLoaderRel.cpp` has, and the one its two nearest relatives in the DOL
// have with another slot: `SetTweaks_FuncPtrs__FP16STweaks_FuncPtrs` at 0x802187E4 (`stw r3,
// gLoader_Tweaks@sda21(r0)`) and `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's, the same carve shape).
//
// **Retail names it, so there is nothing to rename.**  `config/G2ME01/symbols.txt:9461` already
// carries `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` at this address, and the `.c` rule passes
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below is the symbol
// itself.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which is
// the same name and one more file for one line of code.
//
// **The slot belongs to `Sandworm.cpp`, so this unit claims `.text` only.**  `gLoader_Sandworm`
// is `.sbss 0x804193C0..0x804193C8` - 8 bytes, the `value` pointer plus the `padding` word
// `SLoaderSlot` declares (`Sandworm.cpp:11-16`) - claimed and defined by
// `MetroidPrime/ScriptLoader/Sandworm.cpp` (`config/G2ME01/splits.txt:1495-1497`, definition at
// `Sandworm.cpp:16`), whose `.text` ends exactly where this range starts.  Its reader,
// `LoadSandworm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218850 (the function
// immediately before this one), touches the slot with the same small-data displacement this store
// uses - `lwz r6, -27072(r13)` at 0x8021885C - which is what shows the slot holds one
// pointer-sized value.  Declaring the slot `extern` here is what keeps it defined in exactly one
// unit.
//
// **The argument is the record `CSandwormRel.cpp` already models.**  Retail's own mangling says
// `SSandworm_FuncPtrs*` (`P18SSandworm_FuncPtrs`); `src/MetroidPrime/ScriptObjects/
// CSandwormRel.cpp:73-88` is that struct - an `FScriptLoader` plus two 12-byte CodeWarrior
// pointers-to-member-function, 0x1C bytes - and the module's two calls are both consistent with a
// plain pointer store: `RELExit` hands over a null (`CSandwormRel.cpp:197`) and the registration
// `fn_56_70` hands over `&lbl_56_bss_38`, the module's own `.bss:0x38` record
// (`CSandwormRel.cpp:181`).  REL modules import this symbol by its retail name, so the name is
// not this file's to choose.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `Sandworm.cpp` ends at 0x8021887C and
// `MetroidPrime/ScriptLoader/CommandPirate.cpp` starts at 0x80218884.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol: `Sandworm.cpp`
// is in the port's source list and defines `gLoader_Sandworm`, and nothing here calls anything.

/** The record type is named for the parameter only; the byte this unit writes is a pointer
 *  whatever the type is spelled.  Declared at file scope rather than inside the parameter list,
 *  because a tag first seen in a prototype is scoped to that list and the host build then rejects
 *  the definition as a conflicting type. */
struct SSandworm_FuncPtrs;

/** `Sandworm.cpp` defines this in `.sbss 0x804193C0`; MWCC does not encode a variable's type in
 *  its name, so this references `gLoader_Sandworm` itself whatever the type is spelled. */
extern struct SSandworm_FuncPtrs* gLoader_Sandworm;

void SetLoader_Sandworm__FP18SSandworm_FuncPtrs(struct SSandworm_FuncPtrs* loader) {
  gLoader_Sandworm = loader;
}
