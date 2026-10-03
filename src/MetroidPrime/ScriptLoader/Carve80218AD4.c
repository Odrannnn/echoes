// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9480`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80218AD4_text.s` while this range was unclaimed, and
// the body below is the C those bytes are the compilation of.
//
// .text 0x80218AD4..0x80218ADC, 0x8 = 8 bytes, 1 function:
//
//   fn_80218AD4
//       0x80218AD4  0x8 = 8 bytes   stw     r3, gLoader_ElitePirate@sda21(r0)
//                                  blr
//
// **What it is: the DOL side of the ElitePirate module's loader setter.**  It stores the pointer
// it is handed into `gLoader_ElitePirate`, the module's small-data slot, and returns - the whole
// shape of this family of setters.  The instruction pair is byte-for-byte the one
// `fn_80218918` in `src/MetroidPrime/ScriptLoader/Carve80218918.c` has (Ing's, the same carve
// shape), which is itself byte-for-byte what `fn_802188E4` in
// `src/MetroidPrime/ScriptLoader/Carve802188E4.c` (DarkSamus's) and `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's) have; each differs from its
// neighbours only in the `sda21` slot it names.
//
// **The name is retail's, so the unit is `.c`.**  `config/G2ME01/symbols.txt:9480` declares the
// symbol as `fn_80218AD4` - ElitePirate is one of the 526-of-535 whose retail names are
// `fn_<offset>`, so there is nothing better to call it - and the ElitePirate module **imports
// that exact name** from the DOL:
//
//     strings build/G2ME01/ElitePirate/ElitePirate.plf | grep 80218AD4
//     fn_80218AD4
//
// so it cannot be renamed, and an alias would be a different symbol whose call would resolve to
// nothing.  A `.cpp` unit would have to write the signature and let mwcceppc mangle it, which
// mangles `fn_80218AD4` into something the module does not import; a `.c` unit is compiled with
// `-lang=c` (see `src/MetroidPrime/Carve800069AC.c:36-40`), so the definition below *is* the
// symbol.  Both of the module's two call sites pass a plain pointer, measured in
// `build/G2ME01/ElitePirate/asm/auto_00_00000000_text.s`:
//
//   - `RELExit` at module `.text:0x104` (same file, line 143) materialises `li r3, 0` first - a
//     null loader, as in `CSandwormRel.cpp:197` and `CIngRel.cpp`.
//   - `fn_15_148` at module `.text:0x148` (same file, line 171) stores `fn_15_178` at the first
//     word of `lbl_15_bss_0` with `stwu r0, lbl_15_bss_0@l(r3)` - so the store writes the slot
//     itself and leaves r3 holding its address - and hands that address over.  The record is
//     four bytes (`config/G2ME01/rels/ElitePirate/symbols.txt:467` gives
//     `lbl_15_bss_0 = .bss:0x00000000 size:0x4 data:4byte`), one loader pointer and no more,
//     and `src/MetroidPrime/ScriptObjects/CElitePirateRel.cpp:140-147` spells the same reading
//     out in C++.
//
// **The argument is a record the module owns, spelled here after Prime 1's mangling of its
// twin.**  `config/R3ME01/symbols.txt:1783` has
// `SetSElitePirate_FuncPtrs__FP21SElitePirate_FuncPtrs` at 0x8007C9EC, size 0x8 - the same
// function in the other game, which has a name and so spells its argument
// `SElitePirate_FuncPtrs*`.  (That is an inference from the name, the offset's neighbourhood
// and the size; this tree holds no R3ME01 listing, so nothing here claims to have read its
// bytes.)  It is an incomplete type on purpose: nothing in this unit reads through it.
//
// **The slot belongs to `ElitePirate.cpp`, so this unit claims `.text` only.**  `gLoader_ElitePirate`
// is `.sbss 0x80419408..0x80419410` - 8 bytes, the `value` pointer plus the `padding` word
// `SLoaderSlot` declares (`ElitePirate.cpp:11-16`) - claimed and defined by
// `MetroidPrime/ScriptLoader/ElitePirate.cpp` (`config/G2ME01/splits.txt:1659-1661`, definition at
// `ElitePirate.cpp:16`), whose `.text` ends exactly where this range starts.  Its reader,
// `LoadElitePirate` at 0x80218AA8, calls member 0 of what it reads out of the slot, which is
// what shows the slot holds one pointer-sized value.  Declaring the slot `extern` here is what
// keeps it defined in exactly one unit; MWCC does not encode a variable's type in its name, so
// the `extern` declaration resolves to `gLoader_ElitePirate` itself whatever the type is spelled,
// exactly as `Carve802188E4.c` does for `gLoader_DarkSamus`.
//
// `ElitePirate.cpp`'s header records that this setter was deliberately left in dtk's `auto_*`
// unit because "REL modules import it by its retail name, so it cannot be renamed"; that is the
// same reason as for `fn_80218918`, and the answer is the same - it cannot be renamed, so it is
// defined **unrenamed** rather than left retail.  The claim moves from dtk's object to this one
// and the bytes are identical, which is what `tools/flip_test.sh` checks.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `ElitePirate.cpp` ends at 0x80218AD4 and
// `MetroidPrime/ScriptLoader/Blogg.cpp` starts at 0x80218ADC.
//
// **The shape is common - 41 of these setters are still sitting in `auto_03_*_text.s`** (every
// `stw r3, gLoader_...@sda21(r0) ; blr` in `build/G2ME01/asm/`, measured), each one the 0x2C
// after its own thunk, so each is its own carve and each is 8 bytes.  In this block the ones
// next door are 0x802188B0 (CommandPirate), 0x802189D0 (SandBoss), 0x80218A04 (FlyingPirate),
// 0x80218A38 (Grenchler), 0x80218A6C (MediumIng) and 0x80218AA0 (MinorIng); the last five are
// queued as their own `carve-*` items, and none of them is claimed here.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol:
// `ElitePirate.cpp` is in the port's source list (`files.cmake:1181` and `:1630`) and defines
// `gLoader_ElitePirate`, and nothing here calls anything.  It also answers the one
// `fn_80218AD4` reference the tree already had - the declaration at
// `src/MetroidPrime/ScriptObjects/CElitePirateRel.cpp:129`, in a file that is deliberately not
// in the port's list (its header says why, at lines 150-156), so the port never linked it.

/** The record type is named for the parameter only; the word this unit writes is a pointer
 *  whatever the type is spelled.  Declared at file scope rather than inside the parameter list,
 *  because a tag first seen in a prototype is scoped to that list and the host build then
 *  rejects the definition as a conflicting type. */
struct SElitePirate_FuncPtrs;

/** `ElitePirate.cpp` defines this in `.sbss 0x80419408`; MWCC does not encode a variable's type
 *  in its name, so this references `gLoader_ElitePirate` itself whatever the type is spelled. */
extern struct SElitePirate_FuncPtrs* gLoader_ElitePirate;

void fn_80218AD4(struct SElitePirate_FuncPtrs* loader) {
  gLoader_ElitePirate = loader;
}