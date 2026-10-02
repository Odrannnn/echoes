// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9465`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_802188E4_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x802188E4..0x802188EC, 0x8 = 8 bytes, 1 function:
//
//   fn_802188E4
//       0x802188E4  0x8 = 8 bytes   stw     r3, gLoader_DarkSamus@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the DarkSamus module's loader setter.**  It stores the pointer
// it is handed into `gLoader_DarkSamus`, the module's small-data slot, and returns - the whole
// shape of this family of setters.  The instruction pair is byte-for-byte the one
// `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` in
// `src/MetroidPrime/ScriptLoader/Carve8021887C.c` has, which is the same carve shape one
// address along; `SetTweaks_FuncPtrs__FP16STweaks_FuncPtrs` at 0x802187E4 has it with another
// slot, and so does `fn_80235DCC` for DarkSamusBattleStage (0x80235DCC, still in an `auto_*`
// unit - `src/MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp:46-60`).
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9465` declares the
// symbol as `fn_802188E4` - the module is one of the 526-of-535 whose retail names are
// `fn_<offset>`, so there is nothing better to call it - and the DarkSamus module **imports
// that exact name** from the DOL:
//
//     strings build/G2ME01/DarkSamus/DarkSamus.plf | grep 802188E4
//     fn_802188E4
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve
// to nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it,
// which mangles `fn_802188E4` into something the module does not import; a `.c` unit is
// compiled with `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition
// below *is* the symbol.  Both of the module's two call sites pass a plain pointer:
//
//   - `RELExit` at 0x0000CEE0 (`build/G2ME01/DarkSamus/asm/auto_00_0000CED0_text.s:14`)
//     materialises `li r3, 0` first - a null loader, as in `CSandwormRel.cpp:197`.
//   - `fn_10_CF14` at 0x0000CF30 (same file, line 42) stores a member pointer at the first
//     word of `lbl_10_bss_110`, the module's own `.bss` record, and hands its address over.
//
// **The argument is a record the module owns, spelled here after Prime 1's mangling of its
// twin.**  `config/R3ME01/symbols.txt:1665` has
// `SetSDarkSamus_FuncPtrs__FP19SDarkSamus_FuncPtrs` at 0x800747FC - the same function in the
// other game, which has a name and so spells its argument `SDarkSamus_FuncPtrs*`.  The record
// is an `FScriptLoader` plus the CodeWarrior pointers-to-member-function behind it; the pointer
// store below is what this unit actually writes, and the type only names the parameter.  It is
// an incomplete type here on purpose: nothing in this unit reads through it.
//
// **The slot belongs to `DarkSamus.cpp`, so this unit claims `.text` only.**  `gLoader_DarkSamus`
// is `.sbss 0x804193D0..0x804193D8` - 8 bytes, the `value` pointer plus the `padding` word
// `SLoaderSlot` declares (`DarkSamus.cpp:11-16`) - claimed and defined by
// `MetroidPrime/ScriptLoader/DarkSamus.cpp` (`config/G2ME01/splits.txt:1542-1544`, definition at
// `DarkSamus.cpp:16`), whose `.text` ends exactly where this range starts.  Its reader,
// `LoadDarkSamus__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x802188B8, calls member 0
// of what it reads out of the slot, which is what shows the slot holds one pointer-sized value.
// Declaring the slot `extern` here is what keeps it defined in exactly one unit; MWCC does not
// encode a variable's type in its name, so the `extern` declaration resolves to
// `gLoader_DarkSamus` itself whatever the type is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `DarkSamus.cpp` ends at 0x802188E4 and
// `MetroidPrime/ScriptLoader/Ings.cpp` starts at 0x802188EC.  The same shape sits unclaimed at
// 0x802188B0 (`fn_802188B0`, CommandPirate's setter) between `CommandPirate.cpp` and
// `DarkSamus.cpp`; that is a separate carve, not this one.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `DarkSamus.cpp` is in the port's source list and defines `gLoader_DarkSamus`, and nothing here
// calls anything.

/** The record type is named for the parameter only; the byte this unit writes is a pointer
 *  whatever the type is spelled.  Declared at file scope rather than inside the parameter list,
 *  because a tag first seen in a prototype is scoped to that list and the host build then
 *  rejects the definition as a conflicting type. */
struct SDarkSamus_FuncPtrs;

/** `DarkSamus.cpp` defines this in `.sbss 0x804193D0`; MWCC does not encode a variable's type in
 *  its name, so this references `gLoader_DarkSamus` itself whatever the type is spelled. */
extern struct SDarkSamus_FuncPtrs* gLoader_DarkSamus;

void fn_802188E4(struct SDarkSamus_FuncPtrs* loader) {
  gLoader_DarkSamus = loader;
}
