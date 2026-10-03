// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9490`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218BFC_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218BFC..0x80218C04, 0x8 = 8 bytes, 1 function:
//
//   fn_80218BFC
//       0x80218BFC  0x8 = 8 bytes   stw     r3, gLoader_Lumite@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the Lumite module's loader setter.**  It stores the pointer it
// is handed into `gLoader_Lumite`, the module's small-data slot, and returns - the whole shape
// of this family of setters.  The instruction pair is byte-for-byte the one `fn_80200E3C`
// (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`, `gLoader_SpacePirate`)
// and `fn_802188E4` (`src/MetroidPrime/ScriptLoader/Carve802188E4.c:84`,
// `gLoader_DarkSamus`) have, the same carve shape at two other addresses.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9490` declares
// `fn_80218BFC = .text:0x80218BFC; // type:function size:0x8 align:4`, and the Lumite module
// **imports that exact name** from the DOL:
//
//     strings build/G2ME01/Lumite/Lumite.plf | grep 80218BFC
//     fn_80218BFC
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80218BFC` into something the module does not import; a `.c` unit is compiled with
// `-lang=c`, so the definition below *is* the symbol.  `src/MetroidPrime/ScriptObjects/
// CLumiteRel.cpp:144` already declares it `extern "C"` for the same reason.
//
// **Both of the module's two call sites pass a plain pointer** (measured in
// `build/G2ME01/Lumite/asm/auto_00_00000000_text.s`):
//
//   - `RELExit` at 0x11C, line 141 - materialises `li r3, 0x0` first (line 138), a null loader,
//     as in every other module's `RELExit` of this family.
//   - `fn_39_160` at 0x160, line 169 - stores `fn_39_190` at `lbl_39_bss_40` with
//     `stwu r0, lbl_39_bss_40@l(r3)` (line 166) and hands its **address** over.  So the
//     argument is the address of a one-word loader cell, not a loader, which is why the store
//     below writes one pointer and why `lbl_39_bss_40` is four bytes
//     (`src/MetroidPrime/ScriptObjects/CLumiteRel.cpp:157`).
//
// **The slot belongs to `Lumite.cpp`, so this unit claims `.text` only.**  `gLoader_Lumite` is
// `.sbss 0x80419428..0x80419430` - 8 bytes, the `value` pointer plus the `padding` word
// `SLoaderSlot` declares (`Lumite.cpp:11-16`) - and it is claimed and defined by
// `MetroidPrime/ScriptLoader/Lumite.cpp` (`config/G2ME01/splits.txt:1685-1687`).  That unit's
// reader, `LoadLumite` at 0x80218BD0, calls member 0 of what it reads out of the slot
// (`Lumite.cpp:19`), which is what shows the slot holds one pointer-sized value; retail's
// reader is the two instructions `lwz` + `mtctr`/`bctrl`, and the slot's `padding` word is what
// keeps `gLoader_Shrieker` at 0x80419430 eight-byte aligned.  Declaring the slot `extern` here is
// what keeps it defined in exactly one unit; MWCC does not encode a variable's type in its name,
// so the `extern` declaration resolves to `gLoader_Lumite` itself whatever the type is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `Lumite.cpp` ends at 0x80218BFC and
// `MetroidPrime/ScriptLoader/Shrieker.cpp` starts at 0x80218C04 (`splits.txt:1689-1691`), whose
// reader is `LoadShrieker__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C04
// (`symbols.txt:9491`).
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `Lumite.cpp` is in the port's source list and defines `gLoader_Lumite`, and nothing here calls
// anything.

/** The loader this setter is handed, spelled by its shape rather than its real signature.
 *  Retail's is `FScriptLoader` - `typedef CEntity* (*FScriptLoader)(CStateManager&,
 *  CInputStream&, CEntityInfo&)` (`include/MetroidPrime/ScriptLoader.hpp`) - and that header is
 *  C++, so a `.c` unit cannot include it.  `fn_39_160` hands this function `&lbl_39_bss_40`,
 *  whose contents are a loader address (`fn_39_190`, or `0` after `RELExit`), so the argument
 *  is a function pointer passed by value and one word wide. */
typedef void (*SLumiteLoader)(void);

/** The slot `Lumite.cpp` defines, reproduced here in shape only.  Nothing here reads through
 *  it; the store below writes the first word and leaves `padding` alone, which is what retail's
 *  single `stw` does. */
struct SLumiteLoaderSlot {
  SLumiteLoader value;
  unsigned int padding;
};

/** `Lumite.cpp:16` defines this in `.sbss 0x80419428`; MWCC does not encode a variable's type in
 *  its name, so this references `gLoader_Lumite` itself whatever the type is spelled. */
extern struct SLumiteLoaderSlot gLoader_Lumite;

void fn_80218BFC(SLumiteLoader loader) { gLoader_Lumite.value = loader; }
