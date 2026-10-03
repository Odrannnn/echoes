// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9467`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218918_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218918..0x80218920, 0x8 = 8 bytes, 1 function:
//
//   fn_80218918
//       0x80218918  0x8 = 8 bytes   stw     r3, gLoader_Ings@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the Ing module's loader setter.**  It stores the pointer it is
// handed into `gLoader_Ings`, the module's small-data slot, and returns - the whole shape of
// this family of setters.  The instruction pair is byte-for-byte the one its two nearest
// relatives in the DOL have, one with another slot each: `fn_802188E4` in
// `src/MetroidPrime/ScriptLoader/Carve802188E4.c` (DarkSamus's, the same carve shape 0x34 bytes
// below this one) and `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` in
// `src/MetroidPrime/ScriptLoader/Carve8021887C.c` (`stw r3, gLoader_Sandworm@sda21(r0)`).
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9467` declares the
// symbol as `fn_80218918` - Ing is one of the 526-of-535 whose retail names are `fn_<offset>`,
// so there is nothing better to call it - and the Ing module **imports that exact name** from
// the DOL:
//
//     strings build/G2ME01/Ing/Ing.plf | grep 80218918
//     fn_80218918
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80218918` into something the module does not import; a `.c` unit is compiled with
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below *is* the
// symbol.  Both of the module's two call sites pass a plain pointer:
//
//   - `RELExit` at module `.text:0xBC` (`build/G2ME01/Ing/asm/auto_00_00000000_text.s:117`)
//     materialises `li r3, 0` first - a null loader, as in `CSandwormRel.cpp:197`.
//   - `fn_29_100` at module `.text:0x100` (same file, line 145) stores a pointer to `fn_29_130`
//     into the module's own `.bss:0x6C` record and hands that record's address over.  The record
//     is four bytes (`build/G2ME01/Ing/asm/auto_05_00000000_bss.s:76-79` gives
//     `lbl_29_bss_6C size:0x4`), and `src/MetroidPrime/ScriptObjects/CIngRel.cpp:22,48-52` spells
//     the same reading out: `fn_29_100` writes `lbl_29_bss_6C = fn_29_130` and then calls
//     `fn_80218918(&lbl_29_bss_6C)`.
//
// **The argument is that record's address, and the slot holds a pointer to an `FScriptLoader`,
// not a loader.**  `src/MetroidPrime/ScriptObjects/CIngRel.cpp:51-52` measures the one reader:
// `LoadIngs`, in the `Matching` unit `src/MetroidPrime/ScriptLoader/Ings.cpp`, reads the slot as
// `(*gLoader_Ings.value)(mgr, input, info)` - so it dereferences what was stored here and calls
// through it, which is consistent with the module handing over the address of a record holding
// one loader pointer.  The type below names the parameter and describes that shape; nothing in
// this unit reads through it.
//
// **The slot belongs to `Ings.cpp`, so this unit claims `.text` only.**  `gLoader_Ings` is
// `.sbss 0x804193D8..0x804193E0` (`config/G2ME01/symbols.txt:20708` gives the address and an
// 8-byte size) - the `value` pointer plus the `padding` word `SLoaderSlot` declares
// (`Ings.cpp:11-16`) - claimed and defined by `MetroidPrime/ScriptLoader/Ings.cpp`
// (`config/G2ME01/splits.txt:1618-1620`, definition at `Ings.cpp:16`), whose `.text` ends exactly
// where this range starts.  Declaring the slot `extern` here is what keeps it defined in exactly
// one unit; MWCC does not encode a variable's type in its name, so the `extern` declaration
// resolves to `gLoader_Ings` itself whatever the type is spelled, exactly as
// `Carve802188E4.c` does for `gLoader_DarkSamus`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because the range below is a
// claimed unit: `Ings.cpp` ends at 0x80218918.  What is above is **not** a claimed unit but an
// unclaimed gap - `__ct__12CIngSpotDataFUiUiUiUifffffRC20CDamageVulnerabilityUsUsUsUsUs` at
// 0x80218920..0x802189A4, 0x84 bytes, the last unclaimed stretch before
// `MetroidPrime/ScriptLoader/SandBoss.cpp` starts at 0x802189A4 - and it stays that way.  That
// constructor is behavioural class code for `CIngSpotData`, a class this tree does not model, so
// it is a separate item.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol: `Ings.cpp` is
// in the port's source list (twice: `files.cmake:1185` and `:1611`) and defines `gLoader_Ings`,
// and nothing here calls anything.  It also answers the one `fn_80218918` reference the tree
// already had, the `extern "C"` declaration at `CIngRel.cpp:142`.

/** The record type is named for the parameter only; the word this unit writes is a pointer
 *  whatever the type is spelled, and the record it points at holds one loader pointer.  Declared
 *  at file scope rather than inside the parameter list, because a tag first seen in a prototype
 *  is scoped to that list and the host build then rejects the definition as a conflicting type. */
struct SIngLoaderRecord;

/** `Ings.cpp` defines this in `.sbss 0x804193D8`; MWCC does not encode a variable's type in its
 *  name, so this references `gLoader_Ings` itself whatever the type is spelled. */
extern struct SIngLoaderRecord* gLoader_Ings;

void fn_80218918(struct SIngLoaderRecord* loader) {
  gLoader_Ings = loader;
}