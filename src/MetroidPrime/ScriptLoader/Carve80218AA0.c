// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9478`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218AA0_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218AA0..0x80218AA8, 0x8 = 8 bytes, 1 function:
//
//   fn_80218AA0
//       0x80218AA0  0x8 = 8 bytes   stw     r3, gLoader_MinorIng@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the MinorIng module's loader setter.**  It stores the pointer it
// is handed into `gLoader_MinorIng`, the module's small-data slot, and returns - the whole shape
// of this family of setters.  The instruction pair is byte-for-byte the one its three nearest
// relatives in the DOL have, one with another slot each: `fn_80218918` in
// `src/MetroidPrime/ScriptLoader/Carve80218918.c` (`stw r3, gLoader_Ings@sda21(r0)`, 0x188 bytes
// above this one), `fn_802188E4` in `src/MetroidPrime/ScriptLoader/Carve802188E4.c`
// (`stw r3, gLoader_DarkSamus@sda21(r0)`) and `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (`stw r3, gLoader_SpacePirate@sda21(r0)`),
// which is the one this unit's shape comes from.  Same store, same return; only the slot
// address differs.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9478` declares the
// symbol as `fn_80218AA0` - the MinorIng module is one of the many whose retail name is
// `fn_<offset>` - so there is nothing better to call it - and the module **imports that exact
// name** from the DOL:
//
//     strings build/G2ME01/MinorIng/MinorIng.plf | grep 80218AA0
//     fn_80218AA0
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80218AA0` into something the module does not import; a `.c` unit is compiled with
// `-lang=c`, so the definition below *is* the symbol.  Both of the module's two call sites pass
// a plain pointer:
//
//   - `RELExit` at module `.text:0x9C` (`build/G2ME01/MinorIng/asm/auto_00_00000000_text.s:101`)
//     materialises `li r3, 0x0` first (line 99) - a null loader, as in `CSandwormRel.cpp:197`.
//   - `fn_44_E0` at module `.text:0xE0` (same file, line 129) stores `fn_44_110` - the module's
//     entity loader, `.text:0x110, 0x844` bytes - into the module's own `lbl_44_bss_84` and hands
//     that slot's address over.  The slot is four bytes wide
//     (`build/G2ME01/MinorIng/asm/auto_05_00000000_bss.s:55-57` gives
//     `lbl_44_bss_84` `.skip 0x4`), and `src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp:157-159`
//     spells the same reading out: `fn_44_E0` sets `lbl_44_bss_84 = fn_44_110` and then calls
//     `fn_80218AA0(&lbl_44_bss_84)`.
//
// **The argument is that record's address, and the slot holds a pointer to an `FScriptLoader`,
// not a loader.**  `src/MetroidPrime/ScriptLoader/MinorIng.cpp` is the one reader, and it is a
// `Matching` unit: `LoadMinorIng` reads the slot as `(*gLoader_MinorIng.value)(mgr, input, info)`
// - so it dereferences what was stored here and calls through it, which is consistent with the
// module handing over the address of a slot holding one loader pointer.  The type below names the
// parameter and describes that shape; nothing in this unit reads through it.
//
// **The slot belongs to `MinorIng.cpp`, so this unit claims `.text` only.**  `gLoader_MinorIng`
// is `.sbss 0x80419400..0x80419408` (`config/G2ME01/symbols.txt:20713` gives the address and an
// 8-byte size) - the `value` pointer plus the `padding` word `SLoaderSlot` declares
// (`MinorIng.cpp:11-16`) - claimed and defined by `MetroidPrime/ScriptLoader/MinorIng.cpp`
// (`config/G2ME01/splits.txt:1653-1655`, definition at `MinorIng.cpp:16`), whose `.text` ends
// exactly where this range starts.  Declaring the slot `extern` here is what keeps it defined in
// exactly one unit; MWCC does not encode a variable's type in its name, so the `extern`
// declaration resolves to `gLoader_MinorIng` itself whatever the type is spelled, exactly as
// `Carve80218918.c` does for `gLoader_Ings`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because the range is bounded on both sides by claimed units, so the claim is one
// contiguous run with no gap to span: `MinorIng.cpp` ends at 0x80218AA0 (below) and
// `MetroidPrime/ScriptLoader/ElitePirate.cpp` starts at 0x80218AA8 (above, `splits.txt:1657`),
// whose `.sbss 0x80419408..0x80419410` is the next slot along.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol: `MinorIng.cpp`
// is in the port's source list (twice: `files.cmake:1200` and `:1625`) and defines
// `gLoader_MinorIng`, and nothing here calls anything.  It also answers the one `fn_80218AA0`
// reference the tree already had, the `extern "C"` declaration at `CMinorIngRel.cpp:146`.

/** The record type is named for the parameter only; the word this unit writes is a pointer
 *  whatever the type is spelled, and the record it points at holds one loader pointer.  Declared
 *  at file scope rather than inside the parameter list, because a tag first seen in a prototype
 *  is scoped to that list and the host build then rejects the definition as a conflicting type. */
struct SMinorIngLoaderRecord;

/** `MinorIng.cpp` defines this in `.sbss 0x80419400`; MWCC does not encode a variable's type in
 *  its name, so this references `gLoader_MinorIng` itself whatever the type is spelled. */
extern struct SMinorIngLoaderRecord* gLoader_MinorIng;

void fn_80218AA0(struct SMinorIngLoaderRecord* loader) { gLoader_MinorIng = loader; }