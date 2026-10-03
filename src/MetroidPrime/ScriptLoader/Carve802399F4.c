// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:10088`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_802399F4_text.s:9-12` while this range was unclaimed,
// and the body below is the C those bytes are the compilation of.
//
// .text 0x802399F4..0x802399FC, 0x8 = 8 bytes, 1 function:
//
//   fn_802399F4
//       0x802399F4  0x8 = 8 bytes   stw     r3, gLoader_RubiksPuzzle@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the RubiksPuzzle module's loader setter.**  It stores the
// pointer it is handed into `gLoader_RubiksPuzzle`, the module's small-data slot, and returns -
// the whole shape of this family of setters.  The instruction pair is byte-for-byte the one
// `fn_80235DCC` (`src/MetroidPrime/ScriptLoader/Carve80235DCC.c:91`, `gLoader_DarkSamusBattleStage`)
// has, one carve of the same shape in the same loader block, and `fn_80200E3C`
// (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`, `gLoader_SpacePirate`) has it with another
// slot.  `RubiksPuzzle.cpp:6-8` already says why this range is left unclaimed on purpose.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:10088` declares
// `fn_802399F4 = .text:0x802399F4; // type:function size:0x8 align:4` - retail names it the
// `fn_<offset>` placeholder, so there is nothing better to call it - and the RubiksPuzzle module
// **imports that exact name** from the DOL:
//
//     strings build/G2ME01/RubiksPuzzle/RubiksPuzzle.plf | grep 802399
//     fn_802399F4
//
// so it cannot be renamed, and an alias would be a different symbol whose `bl` would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, and the
// mangling is not the name the module imports; a `.c` unit is compiled with `-lang=c`, so the
// definition below *is* the symbol.
//
// **Both of the module's two call sites pass a plain pointer** (measured in
// `build/G2ME01/RubiksPuzzle/asm/auto_00_00000000_text.s`):
//
//   - `RELExit` at .text:0x538, line 413 - materialises `li r3, 0x0` first (line 411), a null
//     loader, as in every other module's `RELExit` of this family.
//   - `fn_4_57C` at .text:0x57C, line 441 - the module constructor.  `lis r3, lbl_4_bss_0@ha`
//     (line 437) and `stwu r0, lbl_4_bss_0@l(r3)` (line 440) put the module's own loader in its
//     one-word `.bss` cell, and then hands **the cell's address** over (line 441).  So the
//     argument is a pointer to a one-word loader cell, not a loader, which is why the store below
//     writes one pointer and why `lbl_4_bss_0` is four bytes
//     (`build/G2ME01/RubiksPuzzle/asm/auto_05_00000000_bss.s`: `.bss:0x0 size:0x4 data:4byte`,
//     and the only object in the module's `.bss`).
//
// That is the slot's shape from the other side too: the `Matching` unit
// `src/MetroidPrime/ScriptLoader/RubiksPuzzle.cpp` declares
// `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` (lines 11-14) and its reader
// `LoadRubiksPuzzle` calls `(*gLoader_RubiksPuzzle.value)(mgr, input, info)` (line 19), which is
// what shows the first word holds a `FScriptLoader*` - the address of the loader, exactly what
// `fn_4_57C` passes.  The record type below is named for the parameter only; the byte this unit
// writes is a pointer whatever the type is spelled, and nothing here reads through it.
//
// **The slot belongs to `RubiksPuzzle.cpp`, so this unit claims `.text` only.**
// `gLoader_RubiksPuzzle` is `.sbss:0x804196C0; // type:object size:0x8 data:4byte`
// (`symbols.txt:20818`) - 8 bytes, the `value` pointer plus the `padding` word - claimed and
// defined by `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp` (`config/G2ME01/splits.txt:2074-2076`,
// definition at line 16), whose `.text` ends exactly where this range starts.  Declaring the slot
// `extern` here is what keeps it defined in exactly one unit; MWCC does not encode a variable's
// type in its name, so the `extern` declaration resolves to `gLoader_RubiksPuzzle` itself whatever
// the type is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because the range is bounded on
// both sides by other claims: `RubiksPuzzle.cpp` ends at 0x802399F4 and the rest of dtk's
// `auto_03_802399F4_text` run belongs to `MetroidPrime/ScriptLoader/Carve8023B634.c` and the
// carves after it (`splits.txt:2078+`), up to `ScriptLoader.cpp` at 0x80242894.  Nothing else
// claims this 8 bytes, so the range is a gap between two claims and not part of any neighbour.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `RubiksPuzzle.cpp` is in the port's source list and defines `gLoader_RubiksPuzzle`, and nothing
// here calls anything.

/** The loader cell the module hands over, named for the parameter only.  Declared at file scope
 *  rather than inside the parameter list, because a tag first seen in a prototype is scoped to
 *  that list and the host build then rejects the definition as a conflicting type. */
struct SRubiksPuzzle_FuncPtrs;

/** `RubiksPuzzle.cpp:16` defines this in `.sbss 0x804196C0`; MWCC does not encode a variable's
 *  type in its name, so this references `gLoader_RubiksPuzzle` itself whatever the type is
 *  spelled. */
extern struct SRubiksPuzzle_FuncPtrs* gLoader_RubiksPuzzle;

void fn_802399F4(struct SRubiksPuzzle_FuncPtrs* loader) {
  gLoader_RubiksPuzzle = loader;
}
