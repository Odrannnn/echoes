// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9937` and `:20780`, the instructions are the
// ones dtk itself emitted into `build/G2ME01/asm/auto_03_8022EBC8_text.s:10-12` while this range
// was unclaimed, and the body below is the C those bytes are the compilation of.  The byte
// evidence is the pristine disc, not our own build: `python3 tools/dol_read.py 0x8022EBC8 0x8
// orig/G2ME01/sys/main.dol` gives `90 6d 98 70 4e 80 00 20`.
//
// .text 0x8022EBC8..0x8022EBD0, 0x8 = 8 bytes, 1 function:
//
//   fn_8022EBC8    0x8022EBC8  0x8    stw     r3, gLoader_EmperorIngStage3@sda21(r0)
//                                        blr
//
// **What it is: the DOL side of the EmperorIngStage3 module's loader setter.**  It stores the
// record address it is handed into `gLoader_EmperorIngStage3`, the module's small-data slot, and
// returns - the whole shape of this family of setters.  The instruction pair is byte for byte the
// one `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`, `gLoader_SpacePirate`),
// `fn_8021F9B0` (`src/MetroidPrime/ScriptLoader/Carve8021F9B0.c:105`, `gLoader_DigitalGuardian`)
// and `fn_80218BFC` (`src/MetroidPrime/ScriptLoader/Carve80218BFC.c:9`, `gLoader_Lumite`) have,
// the same carve shape at three other addresses.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9937` declares
// `fn_8022EBC8 = .text:0x8022EBC8; // type:function size:0x8 align:4`, and the EmperorIngStage3
// module **imports that exact name** from the DOL:
//
//     strings build/G2ME01/EmperorIngStage3/EmperorIngStage3.plf | grep 8022EBC8
//     fn_8022EBC8
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_8022EBC8` into something the module does not import; a `.c` unit is compiled with
// `-lang=c`, so the definition below *is* the symbol.
//
// **The argument is a record address, and both callers are in module 18's own listing**
// (`build/G2ME01/EmperorIngStage3/asm/auto_00_000000F8_text.s`):
//
//   * `RELExit` at `.text:0xC30C` (the `bl` is line 13827) materialises `li r3, 0x0` at
//     `0xC314` first and calls it - a null loader, as in every other module's `RELExit` of this
//     family.
//   * `fn_18_C350` at `.text:0xC350`, 0x30 bytes (what `RELMain` calls at `.text:0xC338`) does
//     `lis r3, lbl_18_bss_30@ha` at `0xC35C`, stores `fn_18_C380` there with
//     `stwu r0, lbl_18_bss_30@l(r3)` at `0xC368`, and calls this function at `0xC36C` (line 13855)
//     with `r3` still that address.  So the argument is the **address of the module's loader
//     record**.
//
// The record is **one word**, unlike the two-word record of the DigitalGuardian carve:
// `build/G2ME01/EmperorIngStage3/asm/auto_05_00000000_bss.s:13-15` gives `lbl_18_bss_30 size:0x4`.
// **That one word is the loader the DOL thunk calls.**  `LoadEmperorIngStage3` (retail
// `.text:0x8022EB9C`, `size:0x2C`, `symbols.txt:9936`) is `src/MetroidPrime/ScriptLoader/
// EmperorIngStage3.cpp:18-20`, whose body is `(*gLoader_EmperorIngStage3.value)(mgr, input, info)`:
// it loads the record pointer out of the slot and calls member 0, and
// `docs/research/rel_loaders.md:169` records that thunk against slot `0` of `0x804195F0`.
// `fn_18_C380` - the one word the constructor writes - is therefore the `FScriptLoader` the DOL
// jumps to, and `0` after `RELExit`, which is why the thunk is not reached during a tear-down.
//
// **The slot belongs to `EmperorIngStage3.cpp`, so this unit claims `.text` only.**
// `gLoader_EmperorIngStage3` is `.sbss 0x804195F0..0x804195F8` (`symbols.txt:20780`,
// `size:0x8`) - the `value` pointer plus the `padding` word of the `SLoaderSlot` at
// `EmperorIngStage3.cpp:11-14` - and it is claimed and defined by
// `MetroidPrime/ScriptLoader/EmperorIngStage3.cpp` (`config/G2ME01/splits.txt:1968-1970`).  The
// store below writes `+0` only, which is retail's single `stw`; the `padding` word is what keeps
// `gLoader_DestructableBarrier` at 0x804195F8 eight-byte aligned.  Declaring the slot `extern` is
// what keeps it defined in exactly one unit; MWCC does not encode a variable's type in its name,
// so the `extern` declaration resolves to `gLoader_EmperorIngStage3` itself whatever the type is
// spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order"), and because both neighbours are claimed units:
// `MetroidPrime/ScriptLoader/EmperorIngStage3.cpp` ends at 0x8022EBC8 and
// `MetroidPrime/ScriptLoader/DestructableBarrier.cpp` starts at 0x8022EBD0
// (`splits.txt:1965-1977`), so this 8-byte range is the gap between them and nothing else is
// nearby to absorb it.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `EmperorIngStage3.cpp` is in the port's source list and defines `gLoader_EmperorIngStage3`, and
// nothing here calls anything.

/** The loader the single word of the record is.  Retail's is `FScriptLoader` - `typedef CEntity*
 *  (*FScriptLoader)(CStateManager&, CInputStream&, CEntityInfo&)`
 *  (`include/MetroidPrime/ScriptLoader.hpp`) - and that header is C++, so a `.c` unit cannot
 *  include it.  `fn_18_C350` hands this function `&lbl_18_bss_30`, whose one word is
 *  `fn_18_C380` (or `0` after `RELExit`), so it is a function pointer passed by value and one
 *  word wide. */
typedef void (*SEmperorIngStage3Loader)(void);

/** The record the module hands over, 0x4 bytes as measured above.  Only its address is stored,
 *  so the field type describes the shape rather than a layout this unit reads;
 *  `EmperorIngStage3.cpp:18-20` dereferences the same word as an `FScriptLoader`. */
struct SEmperorIngStage3Loaders {
  SEmperorIngStage3Loader slot0;
};

/** The slot `EmperorIngStage3.cpp` defines, reproduced here in shape only.  Nothing here reads
 *  through it; the store below writes the first word and leaves `padding` alone, which is what
 *  retail's single `stw` does. */
struct SLoaderSlot {
  struct SEmperorIngStage3Loaders* value;
  unsigned int padding;
};

/** `EmperorIngStage3.cpp:16` defines this in `.sbss 0x804195F0`; MWCC does not encode a
 *  variable's type in its name, so this references `gLoader_EmperorIngStage3` itself whatever the
 *  type is spelled. */
extern struct SLoaderSlot gLoader_EmperorIngStage3;

void fn_8022EBC8(struct SEmperorIngStage3Loaders* loaders) {
  gLoader_EmperorIngStage3.value = loaders;
}