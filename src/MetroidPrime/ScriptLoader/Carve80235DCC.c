// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:10045`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80235DCC_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80235DCC..0x80235DD4, 0x8 = 8 bytes, 1 function:
//
//   fn_80235DCC
//       0x80235DCC  0x8 = 8 bytes   stw     r3, gLoader_DarkSamusBattleStage@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the DarkSamusBattleStage module's loader setter.**  It stores
// the pointer it is handed into `gLoader_DarkSamusBattleStage`, the module's small-data slot,
// and returns - the whole shape of this family of setters.  The instruction pair is byte-for-byte
// the one `fn_802188E4` (`src/MetroidPrime/ScriptLoader/Carve802188E4.c:84`, `gLoader_DarkSamus`)
// has, the same carve shape for the module one address along in the same loader block, and
// `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`, `gLoader_SpacePirate`) and
// `fn_8021887C` (`SetLoader_Sandworm__FP18SSandworm_FuncPtrs`) have it with other slots.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:10045` declares
// `fn_80235DCC = .text:0x80235DCC; // type:function size:0x8 align:4` - the module is one of the
// 526-of-535 whose retail names are `fn_<offset>`, so there is nothing better to call it - and
// the DarkSamusBattleStage module **imports that exact name** from the DOL:
//
//     strings build/G2ME01/DarkSamusBattleStage/DarkSamusBattleStage.plf | grep 80235D
//     fn_80235DCC
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80235DCC` into something the module does not import; a `.c` unit is compiled with
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below *is* the
// symbol.  It is `extern "C"` for the same reason:
// `src/MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp:80` declares it that way.
//
// **Both of the module's two call sites pass a plain pointer** (measured in
// `build/G2ME01/DarkSamusBattleStage/asm/auto_00_00000000_text.s`):
//
//   - `fn_11_0` at .text:0x00, line 14 - materialises `li r3, 0x0` first (line 12), a null
//     loader, as in every other module's `RELExit` of this family.
//   - `fn_11_44` at .text:0x44, line 42 - materialises `lis r4, fn_11_74@ha` (line 37) and
//     `addi r0, r4, fn_11_74@l` (line 40), stores the module's own loader at `lbl_11_bss_0`
//     with `stwu r0, lbl_11_bss_0@l(r3)` (line 41) and hands **the cell's address** over
//     (line 42).  So the argument is a pointer to a one-word loader cell, not a loader, which
//     is why the store below writes one pointer and why `lbl_11_bss_0` is four bytes
//     (`build/G2ME01/DarkSamusBattleStage/asm/auto_05_00000000_bss.s`: `.bss:0x0 size:0x4
//     data:4byte`, and the only object in the module's `.bss`).
//
// That is the slot's shape from the other side too: the `Matching` unit
// `src/MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp` declares
// `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` (lines 11-14) and its reader
// `LoadDarkSamusBattleStage` calls `(*gLoader_DarkSamusBattleStage.value)(mgr, input, info)`
// (line 19), which is what shows the first word holds a `FScriptLoader*` - the address of the
// loader, exactly what `fn_11_44` passes.  The record type below is named for the parameter only;
// the byte this unit writes is a pointer whatever the type is spelled, and nothing here reads
// through it.
//
// **The slot belongs to `DarkSamusBattleStage.cpp`, so this unit claims `.text` only.**
// `gLoader_DarkSamusBattleStage` is `.sbss:0x80419658; // type:object size:0x8 data:4byte`
// (`symbols.txt:20795`) - 8 bytes, the `value` pointer plus the `padding` word - claimed and
// defined by `MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp`
// (`config/G2ME01/splits.txt:2003-2005`, definition at line 16), whose `.text` ends exactly
// where this range starts.  Declaring the slot `extern` here is what keeps it defined in exactly
// one unit; MWCC does not encode a variable's type in its name, so the `extern` declaration
// resolves to `gLoader_DarkSamusBattleStage` itself whatever the type is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches it.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `DarkSamusBattleStage.cpp` ends at 0x80235DCC and
// `MetroidPrime/ScriptLoader/DarkCommando.cpp` starts at 0x80235DD4 (`splits.txt:2007-2008`),
// whose reader is `LoadDarkCommando__FR13CStateManagerR12CInputStreamR11CEntityInfo` at
// 0x80235DD4 (`symbols.txt:10046`).
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `DarkSamusBattleStage.cpp` is in the port's source list and defines
// `gLoader_DarkSamusBattleStage`, and nothing here calls anything.

/** The loader cell the module hands over, named for the parameter only.  Declared at file scope
 *  rather than inside the parameter list, because a tag first seen in a prototype is scoped to
 *  that list and the host build then rejects the definition as a conflicting type. */
struct SDarkSamusBattleStage_FuncPtrs;

/** `DarkSamusBattleStage.cpp:16` defines this in `.sbss 0x80419658`; MWCC does not encode a
 *  variable's type in its name, so this references `gLoader_DarkSamusBattleStage` itself
 *  whatever the type is spelled. */
extern struct SDarkSamusBattleStage_FuncPtrs* gLoader_DarkSamusBattleStage;

void fn_80235DCC(struct SDarkSamusBattleStage_FuncPtrs* loader) {
  gLoader_DarkSamusBattleStage = loader;
}