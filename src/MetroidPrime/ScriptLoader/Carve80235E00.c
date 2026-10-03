// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:10047`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80235E00_text.s` while this range was unclaimed, and the
// body below is the C those bytes are the compilation of.
//
// .text 0x80235E00..0x80235E08, 0x8 = 8 bytes, 1 function:
//
//   fn_80235E00
//       0x80235E00  0x8 = 8 bytes   stw     r3, gLoader_DarkCommando@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the DarkCommando module's loader setter.**  It stores the pointer
// it is handed into `gLoader_DarkCommando`, the module's small-data slot, and returns - the whole
// shape of this family of setters.  The instruction pair is byte-for-byte the one
// `fn_80235DCC` (`src/MetroidPrime/ScriptLoader/Carve80235DCC.c:91`, `gLoader_DarkSamusBattleStage`)
// has, the same carve shape one address along in the same loader block and in the same dtk
// `auto_03_*` unit, and `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`,
// `gLoader_SpacePirate`) and `fn_8022A058`
// (`src/MetroidPrime/ScriptLoader/Carve8022A058.c:95`, `gLoader_PuddleSpore`) have it with other
// slots.  The 16-bit `@sda21` displacement is the only byte that differs between those copies, and
// it checks out here: `_SDA_BASE_` is 0x8041FD80, `gLoader_DarkCommando` is 0x80419660, their
// difference is 0x6720, and 0x10000 - 0x6720 is the 0x98E0 the store encodes as a signed halfword
// (`90 6D 98 E0`).
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:10047` declares
// `fn_80235E00 = .text:0x80235E00; // type:function size:0x8 align:4` - retail names none of these,
// the placeholder is all there is - and the DarkCommando module **imports that exact name** from
// the DOL:
//
//     strings build/G2ME01/DarkCommando/DarkCommando.plf | grep 80235E00
//     fn_80235E00
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80235E00` into something the module does not import; a `.c` unit is compiled with
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below *is* the
// symbol.  `src/MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp:165` declares it `extern "C"` for
// the same reason.
//
// **Both of the module's two call sites pass a plain pointer** (measured in
// `build/G2ME01/DarkCommando/asm/MetroidPrime/ScriptObjects/CDarkCommandoRel.s`):
//
//   - `RELExit` at `.text:0x128`, 0x24 bytes - materialises `li r3, 0x0` first (line 143) and
//     then `bl fn_80235E00` (line 148): a null loader, as in every other module's `RELExit` of
//     this family.
//   - `fn_3_16C` at `.text:0x16C`, 0x30 bytes, which `RELMain` at `.text:0x14C` reaches through
//     `bl fn_3_16C` - materialises `lis r4, fn_3_19C@ha` (line 172) and
//     `lis r3, lbl_3_bss_1C@ha` (line 173), adds the low half with
//     `addi r0, r4, fn_3_19C@l` (line 177), stores the module's own loader with
//     `stwu r0, lbl_3_bss_1C@l(r3)` (line 179) and hands **the cell's address** over in `r3`
//     (line 180).  So the argument is a pointer to a one-word loader cell, not a loader, which is
//     why the store below writes one pointer and why `lbl_3_bss_1C` is four bytes
//     (`build/G2ME01/DarkCommando/asm/auto_05_00000000_bss.s`: `.bss:0x1C | 0x1C | size:0x4`,
//     and `auto_05_00000000_bss.s` reports nothing else in the module's `.bss` of that width).
//
// That is the slot's shape from the other side too: the `Matching` unit
// `src/MetroidPrime/ScriptLoader/DarkCommando.cpp` declares
// `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` (lines 11-14) and its reader
// `LoadDarkCommando` calls `(*gLoader_DarkCommando.value)(mgr, input, info)` (line 19), which is
// what shows the first word holds a `FScriptLoader*` - the address of the loader, exactly what
// `fn_3_16C` passes.  The record type below is named for the parameter only; the byte this unit
// writes is a pointer whatever the type is spelled, and nothing here reads through it.
//
// **The slot belongs to `DarkCommando.cpp`, so this unit claims `.text` only.**
// `gLoader_DarkCommando` is `.sbss:0x80419660; // type:object size:0x8 data:4byte`
// (`symbols.txt:20796`) - 8 bytes, the `value` pointer plus the `padding` word - claimed and
// defined by `MetroidPrime/ScriptLoader/DarkCommando.cpp`
// (`config/G2ME01/splits.txt:2059-2061`, definition at line 16), whose `.text` ends exactly at
// 0x80235E00, where this range starts.  Declaring the slot `extern` here is what keeps it defined
// in exactly one unit; MWCC does not encode a variable's type in its name, so the `extern`
// declaration resolves to `gLoader_DarkCommando` itself whatever the type is spelled.  That unit's
// header says the setter "is deliberately NOT claimed: REL modules import it by its retail name,
// so it cannot be renamed and must stay in dtk's auto unit".  **That sentence is superseded by
// this file**, the same way `PuddleSpore.cpp`'s was superseded by
// `docs/goal-notes/carve-8022a058.md`: the import is the plain `fn_80235E00`, so defining it
// unmangled here keeps the symbol in the DOL link and both module call sites still resolve
// against it, with nothing renamed.  No other unit in `src/` defines it - `CDarkCommandoRel.cpp`
// only declares it - so there is exactly one definition.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `MetroidPrime/ScriptLoader/DarkCommando.cpp` ends at 0x80235E00 and
// `MetroidPrime/ScriptLoader/Carve802392A4.cpp` starts at 0x802392A4, whose reader is
// `fn_802392A4` at 0x802392A4 (`symbols.txt`).  What is left of the old dtk range,
// 0x80235E08..0x802392A4, is deliberately still unclaimed: it is `fn_80235E08` (0x124 bytes, the
// viewport/scissor selection), `fn_80235F2C` and the rest of that block, none of which is a copy
// of anything already matched.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `DarkCommando.cpp` is in the port's source list and defines `gLoader_DarkCommando`, and nothing
// here calls anything.

/** The loader cell the module hands over, named for the parameter only.  Declared at file scope
 *  rather than inside the parameter list, because a tag first seen in a prototype is scoped to
 *  that list and the host build then rejects the definition as a conflicting type. */
struct SDarkCommando_FuncPtrs;

/** `DarkCommando.cpp:16` defines this in `.sbss 0x80419660`; MWCC does not encode a variable's
 *  type in its name, so this references `gLoader_DarkCommando` itself whatever the type is
 *  spelled.  The declaration says pointer-to-struct where the real definition is the 8-byte
 *  `SLoaderSlot` object; that mismatch is deliberate and is the same one
 *  `Carve80235DCC.c` makes.  All it has to get right is that the store lands at offset 0 of the
 *  slot, and MWCC emits no type into the symbol name, so the link is by name only. */
extern struct SDarkCommando_FuncPtrs* gLoader_DarkCommando;

void fn_80235E00(struct SDarkCommando_FuncPtrs* loader) { gLoader_DarkCommando = loader; }