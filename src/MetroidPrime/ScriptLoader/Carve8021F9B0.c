// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9608`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8021F9B0_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x8021F9B0..0x8021F9B8, 0x8 = 8 bytes, 1 function:
//
//   fn_8021F9B0
//       0x8021F9B0  0x8 = 8 bytes   stw     r3, gLoader_DigitalGuardian@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the DigitalGuardian module's loader setter.**  It stores the
// pointer it is handed into `gLoader_DigitalGuardian`, the module's small-data slot, and
// returns - the whole shape of this family of setters.  The instruction pair is byte-for-byte
// the one `fn_80200E3C` (`src/MetroidPrime/ScriptLoader/Carve80200E3C.c:67`, `gLoader_SpacePirate`)
// and `fn_80218BFC` (`src/MetroidPrime/ScriptLoader/Carve80218BFC.c:89`, `gLoader_Lumite`) have,
// the same carve shape at two other addresses.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9608` declares
// `fn_8021F9B0 = .text:0x8021F9B0; // type:function size:0x8 align:4`, and the DigitalGuardian
// module **imports that exact name** from the DOL:
//
//     strings build/G2ME01/DigitalGuardian/DigitalGuardian.plf | grep 8021F9B0
//     fn_8021F9B0
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_8021F9B0` into something the module does not import; a `.c` unit is compiled with
// `-lang=c`, so the definition below *is* the symbol.  The two callers are both in module 14's
// own listing (`build/G2ME01/DigitalGuardian/asm/auto_00_0000010C_text.s`):
//
//   - `RELExit` at `.text:0x138` materialises `li r3, 0x0` first and calls it at `.text:0x148` -
//     a null loader, as in every other module's `RELExit` of this family.
//   - `fn_14_17C` at `.text:0x17C` (what `RELMain` calls at `.text:0x168`) stores `fn_14_1B8` at
//     `lbl_14_bss_C+0` with `stwu r5, lbl_14_bss_C@l(r3)` and `fn_14_DF7C` at `lbl_14_bss_C+4`
//     with `stw r0, 0x4(r3)`, then calls it at `.text:0x1A4` with `r3` still the address
//     `lis r3, lbl_14_bss_C@ha` materialised.  So the argument is the **address of a record of
//     two loader pointers**, not a loader.  `lbl_14_bss_C` is `.bss:0xC` of size `0xC`
//     (`build/G2ME01/DigitalGuardian/asm/auto_05_00000000_bss.s:30`), so the record is 12 bytes
//     and its first two words are the ones setup fills.
//
// **The two words are slot 0 and slot 1, which is what the DOL readers read.**
// `MetroidPrime/ScriptLoader/DigitalGuardian.cpp:16-19` declares the record as
// `SDigitalGuardianLoaders { FScriptLoader slot0; FScriptLoader slot1; }` and calls member 0
// from `LoadDigitalGuardian` (line 29) and member 1 from `LoadDigitalGuardianHead` (line 33);
// retail's thunks are `lwz` + `mtctr`/`bctrl` off a vtable word at +0 and +4 of the record
// (`docs/research/rel_loaders.md:147-148` records `LoadDigitalGuardianHead` at 0x8021F958 with
// `slot` 4 and `LoadDigitalGuardian` at 0x8021F984 with `slot` 0).
//
// **The slot belongs to `DigitalGuardian.cpp`, so this unit claims `.text` only.**
// `gLoader_DigitalGuardian` is `.sbss 0x80419510..0x80419518` - 8 bytes, the `value` pointer plus
// the `padding` word `SLoaderSlot` declares (`DigitalGuardian.cpp:21-24`) - and it is claimed and
// defined by `MetroidPrime/ScriptLoader/DigitalGuardian.cpp`
// (`config/G2ME01/splits.txt:1763-1765`).  That unit's two readers read `value->slot0` and
// `value->slot1`, and the store below writes `+0` only, which is retail's single `stw`; the
// `padding` word is what keeps `gLoader_Shredder` at 0x80419518 eight-byte aligned.  Declaring
// the slot `extern` here is what keeps it defined in exactly one unit; MWCC does not encode a
// variable's type in its name, so the `extern` declaration resolves to `gLoader_DigitalGuardian`
// itself whatever the type is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `DigitalGuardian.cpp` ends at 0x8021F9B0 and
// `MetroidPrime/ScriptLoader/Shredder.cpp` starts at 0x8021F9B8 (`splits.txt:1763-1771`), whose
// reader is `LoadShredder__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x8021F9B8
// (`symbols.txt:9609`).
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `DigitalGuardian.cpp` is in the port's source list and defines `gLoader_DigitalGuardian`, and
// nothing here calls anything.

/** The loader each word of the record is.  Retail's is `FScriptLoader` - `typedef CEntity*
 *  (*FScriptLoader)(CStateManager&, CInputStream&, CEntityInfo&)`
 *  (`include/MetroidPrime/ScriptLoader.hpp`) - and that header is C++, so a `.c` unit cannot
 *  include it.  `fn_14_17C` hands this function `&lbl_14_bss_C`, whose first two words are
 *  loader addresses (`fn_14_1B8` and `fn_14_DF7C`, or `0` after `RELExit`), so each is a
 *  function pointer passed by value and one word wide. */
typedef void (*SDigitalGuardianLoader)(void);

/** The record the module hands over, 0xC bytes as measured above.  Only its address is stored,
 *  so these field types describe the shape rather than a layout this unit reads;
 *  `DigitalGuardian.cpp:16-19` is the same record in C++ and says what the two words are. */
struct SDigitalGuardianLoaders {
  SDigitalGuardianLoader slot0;
  SDigitalGuardianLoader slot1;
};

/** The slot `DigitalGuardian.cpp` defines, reproduced here in shape only.  Nothing here reads
 *  through it; the store below writes the first word and leaves `padding` alone, which is what
 *  retail's single `stw` does. */
struct SLoaderSlot {
  struct SDigitalGuardianLoaders* value;
  unsigned int padding;
};

/** `DigitalGuardian.cpp:26` defines this in `.sbss 0x80419510`; MWCC does not encode a
 *  variable's type in its name, so this references `gLoader_DigitalGuardian` itself whatever the
 *  type is spelled. */
extern struct SLoaderSlot gLoader_DigitalGuardian;

void fn_8021F9B0(struct SDigitalGuardianLoaders* loaders) {
  gLoader_DigitalGuardian.value = loaders;
}
