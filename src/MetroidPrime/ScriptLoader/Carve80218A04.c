// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9472`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218A04_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218A04..0x80218A0C, 0x8 = 8 bytes, 1 function:
//
//   fn_80218A04
//       0x80218A04  0x8 = 8 bytes   stw     r3, gLoader_FlyingPirate@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the FlyingPirate module's loader setter.**  It stores the
// pointer it is handed into `gLoader_FlyingPirate`, the module's small-data slot, and returns -
// the whole shape of this family of setters.  The instruction pair is byte-for-byte the one
// `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` in
// `src/MetroidPrime/ScriptLoader/Carve8021887C.c` has, and the one `fn_802188E4` in
// `src/MetroidPrime/ScriptLoader/Carve802188E4.c` has with another slot (DarkSamus's, sixteen
// bytes below this range).
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9472` declares the
// symbol as `fn_80218A04` - the module is one of the 526-of-535 whose retail names are
// `fn_<offset>`, so there is nothing better to call it - and the FlyingPirate module **imports
// that exact name** from the DOL:
//
//     strings build/G2ME01/FlyingPirate/FlyingPirate.plf | grep 80218A04
//     fn_80218A04
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  The `.c` rule passes `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so
// the definition below *is* the symbol.
//
// **Both of the module's two call sites pass a plain pointer**
// (`build/G2ME01/FlyingPirate/asm/MetroidPrime/ScriptObjects/CFlyingPirateRel.s`):
//
//   - `RELExit` at 0x00000570 (line 127) materialises `li r3, 0` first - a null loader, as in
//     `CSandwormRel.cpp:197`.
//   - `fn_22_5A4` at 0x000005C0 (line 155) is the registration: `lis r4, fn_22_5D4@ha` /
//     `lis r3, lbl_22_bss_58@ha` / `stwu r0, lbl_22_bss_58@l(r3)` / `bl fn_80218A04`, so r3
//     holds the *address* of the module's own copy of the loader, not the loader.
//
// `lbl_22_bss_58` is `.bss:0x58`, `size:0x4 data:4byte`
// (`build/G2ME01/FlyingPirate/asm/auto_05_00000000_bss.s:48-51`), so in *this* game the record
// the pointer addresses is only its first word - the loader itself - which is what
// `src/MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp:162-168` measures and spells as
// `lbl_22_bss_58 = fn_22_5D4; fn_80218A04(&lbl_22_bss_58);`.  The sibling modules hand over the
// address of a 0x1C-byte `SFoo_FuncPtrs` record instead (`Carve8021887C.c`, `Carve802188E4.c`,
// `Carve80200E3C.c`), and retail's own mangling of this very function in the other game calls its
// parameter a record - `config/R3ME01/symbols.txt:2015` and `config/R32J01/symbols.txt:2012` both
// have `SetSFlyingPirate_FuncPtrs__FP22SFlyingPirate_FuncPtrs`, 0x8 bytes - so that is the tag
// below.  The store is one word for any pointer type and nothing in this unit reads through the
// parameter, so the tag names the parameter's role and not a layout this unit depends on.
//
// **The slot belongs to `FlyingPirate.cpp`, so this unit claims `.text` only.**  `gLoader_
// FlyingPirate` is `.sbss 0x804193E8..0x804193F0` - 8 bytes, the `value` pointer plus the
// `padding` word `SLoaderSlot` declares (`FlyingPirate.cpp:11-16`) - claimed and defined by
// `MetroidPrime/ScriptLoader/FlyingPirate.cpp` (`config/G2ME01/splits.txt:1633-1635`, definition
// at `FlyingPirate.cpp:16`), whose `.text` ends exactly where this range starts.  Its reader,
// `LoadFlyingPirate__FR13CStateManagerR12CInputStreamR11CEntityInfo` at 0x802189D8, the function
// immediately before this one, reads the slot with the same small-data displacement this store
// writes (`lwz r6, -27032(r13)` at 0x802189E4) and calls member 0 of what it finds
// (`lwz r12, 0(r6)`), which is what shows the slot holds one pointer-sized value.  Declaring the
// slot `extern` here is what keeps it defined in exactly one unit; MWCC does not encode a
// variable's type in its name, so this references `gLoader_FlyingPirate` itself whatever the type
// is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `FlyingPirate.cpp` ends at 0x80218A04 and
// `MetroidPrime/ScriptLoader/Grenchler.cpp` starts at 0x80218A0C.  The same shape sits unclaimed
// at 0x80218A38 (`fn_80218A38`, Grenchler's setter) between `Grenchler.cpp` and
// `MediumIng.cpp`; that is a separate carve, not this one.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `FlyingPirate.cpp` is in the port's source list and defines `gLoader_FlyingPirate`, and nothing
// here calls anything.

/** The record type is named for the parameter only; the word this unit writes is a pointer
 *  whatever the type is spelled.  Declared at file scope rather than inside the parameter list,
 *  because a tag first seen in a prototype is scoped to that list and the host build then rejects
 *  the definition as a conflicting type. */
struct SFlyingPirate_FuncPtrs;

/** `FlyingPirate.cpp` defines this in `.sbss 0x804193E8`; MWCC does not encode a variable's type
 *  in its name, so this references `gLoader_FlyingPirate` itself whatever the type is spelled. */
extern struct SFlyingPirate_FuncPtrs* gLoader_FlyingPirate;

void fn_80218A04(struct SFlyingPirate_FuncPtrs* loader) {
  gLoader_FlyingPirate = loader;
}
