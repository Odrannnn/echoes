// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9474`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218A38_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218A38..0x80218A40, 0x8 = 8 bytes, 1 function:
//
//   fn_80218A38
//       0x80218A38  0x8 = 8 bytes   stw     r3, gLoader_Grenchler@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the Grenchler module's loader setter.**  It stores the pointer
// it is handed into `gLoader_Grenchler`, the module's small-data slot, and returns - the whole
// shape of this family of setters.  The instruction pair is byte-for-byte the one
// `fn_80200E3C` has in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's setter,
// which the goal item names as the twin), and the one `fn_802188E4` has in
// `src/MetroidPrime/ScriptLoader/Carve802188E4.c`, DarkSamus's setter three carve-units
// earlier in address order.  The three differ only in the 16-bit `@sda21` displacement, because
// their slots are 8 bytes apart - `gLoader_SpacePirate` at `.sbss 0x80419358`, DarkSamus's at
// `0x804193D0` and `gLoader_Grenchler` at `0x804193F0` (`config/G2ME01/symbols.txt:20711`).
// The `4E 80 00 20` `blr` is the same in all of them.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9474` declares the
// symbol as `fn_80218A38` - retail gives this one no name - and the Grenchler module **imports
// that exact name** from the DOL:
//
//     strings build/G2ME01/Grenchler/Grenchler.plf | grep 80218A38
//     fn_80218A38
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80218A38` into something the module does not import; a `.c` unit is compiled with
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below *is* the
// symbol.  Both of the module's two call sites are in its own listing,
// `build/G2ME01/Grenchler/asm/MetroidPrime/ScriptObjects/CGrenchlerRel.s`, and neither calls it a
// setter by name, so the argument is read off them:
//
//   - `RELExit` at `.text 0xF4` (0x24 bytes) does `li r3, 0x0` and then `bl fn_80218A38` - a
//     null loader, the module tearing its loader down on the way out.
//   - `fn_27_138` at `.text 0x138` (0x30 bytes, reached from `RELMain`) does
//     `lis r4, fn_27_168@ha` / `lis r3, lbl_27_bss_40@ha` / `addi r0, r4, fn_27_168@l` /
//     `stwu r0, lbl_27_bss_40@l(r3)` and then `bl fn_80218A38` with `r3` still holding the
//     slot's address: the `stwu` writes the word and decrements `r3`, which is what leaves it
//     pointing at `lbl_27_bss_40`.  So the argument is that record's address.
//
// **The record is one loader and a padding word.**  `config/G2ME01/rels/Grenchler/symbols.txt:746`
// reads `lbl_27_bss_40 = .bss:0x00000040; // type:object size:0x8 data:4byte`, and
// `build/G2ME01/Grenchler/asm/auto_05_00000000_bss.s:28-31` is the same 8 bytes - one
// 4-byte `FScriptLoader`, `fn_27_168`, and the alignment word.  That is the smallest shape in
// this family, unlike module 72's `lbl_72_bss_24` (0x1C, `Carve80200E3C.c`) or module 47's
// `lbl_47_bss_50` (0xC, `Carve80200EFC.c`).  The type only names the parameter: this unit stores
// the record's address and never reads through it.
//
// **The slot belongs to `Grenchler.cpp`, so this unit claims `.text` only.**  `gLoader_Grenchler`
// is `.sbss 0x804193F0..0x804193F8` - 8 bytes, the `value` pointer plus the `padding` word
// `SLoaderSlot` declares (`Grenchler.cpp:11-16`) - claimed and defined by
// `MetroidPrime/ScriptLoader/Grenchler.cpp` (`config/G2ME01/splits.txt:1639-1641`, definition at
// `Grenchler.cpp:16`), whose `.text` ends exactly where this range starts and whose own header
// reserves these eight bytes: "The 8-byte setter at 0x80218A38 is deliberately NOT claimed:
// REL modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
// unit."  Its reader, `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
// 0x80218A0C, calls member 0 of what it reads out of the slot, which is what shows the slot holds
// one pointer-sized value.  Declaring the slot `extern` here is what keeps it defined in exactly
// one unit; MWCC does not encode a variable's type in its name, so the `extern` declaration
// resolves to `gLoader_Grenchler` itself whatever the type is spelled.  `CGrenchlerRel.cpp:134`
// already declares the setter `void fn_80218A38(FScriptLoader* loader)` inside its `extern "C"`
// block and calls it under that name; MWCC does not encode a parameter type in a function name,
// so the two declarations agree.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// claimed units: `Grenchler.cpp` ends at 0x80218A38 and `MetroidPrime/ScriptLoader/MediumIng.cpp`
// starts at 0x80218A40, so this run slots between them with no unclaimed gap on either side.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `Grenchler.cpp` is in the port's source list and defines `gLoader_Grenchler`, and nothing here
// calls anything.  `CGrenchlerRel.cpp` is deliberately not in the port's list - it needs
// `fn_27_168`, which the host has no definition of - so listing this file costs the flat host
// link nothing.

/** The record module 27 hands over, 8 bytes as measured above: one `FScriptLoader` and the
 *  alignment word.  It is named for the parameter only; nothing in this unit reads through it.
 *  Declared at file scope rather than inside the parameter list, because a tag first seen in a
 *  prototype is scoped to that list and the host build then rejects the definition as a
 *  conflicting type. */
struct SGrenchlerLoader {
  unsigned int loader; /* FScriptLoader - fn_27_168 */
  unsigned int padding;
};

/** `Grenchler.cpp` defines this in `.sbss 0x804193F0` and dereferences it at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_Grenchler` itself
 *  whatever the type is spelled. */
extern struct SGrenchlerLoader* gLoader_Grenchler;

void fn_80218A38(struct SGrenchlerLoader* loader) {
  gLoader_Grenchler = loader;
}