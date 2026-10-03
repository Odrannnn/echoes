// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9610` and `:20751`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8021F9E4_text.s` while this range was
// unclaimed, and the body below is the C those bytes are the compilation of.
//
// .text 0x8021F9E4..0x8021F9EC, 0x8 = 8 bytes, 1 function:
//
//   fn_8021F9E4    0x8021F9E4  0x8    stw     r3, gLoader_Shredder@sda21(r0)
//                                          blr
//
// **What it is: the DOL side of the Shredder module's loader setter.**  It stores the pointer it
// is handed into `gLoader_Shredder`, the module's small-data slot, and returns - the whole shape of
// this family of setters.  The instruction pair is byte-for-byte the one the matched
// `SetLoader_WallWalker__FPPFR13CStateManagerR12CInputStreamRC11CEntityIn` in
// `src/MetroidPrime/ScriptLoaderRel.cpp` has, and the one `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` has (SpacePirate's, the same carve shape).  The
// nearest copy in the DOL is the immediately preceding 8 bytes, `fn_8021F9B0` at 0x8021F9B0, which
// is DigitalGuardian's and differs only in the `@sda21` displacement: its `stw r3,
// gLoader_DigitalGuardian@sda21(r0)` is `90 6D 97 90` against this one's `90 6D 97 98`, because
// the two slots are 8 bytes apart (`gLoader_DigitalGuardian` at `.sbss 0x80419510`,
// `gLoader_Shredder` at `0x80419518`).  The `blr` is `4E 80 00 20` in both.
//
// **Retail names it only as a placeholder, so the `fn_` name has to be reproduced verbatim.**
// `config/G2ME01/symbols.txt:9610` carries `fn_8021F9E4 = .text:0x8021F9E4; // type:function
// size:0x8 align:4`, and the Shredder REL module imports it under that exact name
// (`build/G2ME01/Shredder/asm/auto_00_000000C8_text.s`, two `bl fn_8021F9E4`), so the name is not
// this file's to choose.  A C++ definition would mangle to `_Z10fn_8021F9E4P1x` and objdiff would
// pair nothing, which is why the unit is a `.c` rather than a `.cpp`.
//
// **The slot belongs to `Shredder.cpp`, so this unit claims `.text` only.**  `gLoader_Shredder` is
// `.sbss 0x80419518 size:0x8` (`symbols.txt:20751`), which is the 8-byte `SLoaderSlot { value;
// padding; }` that `MetroidPrime/ScriptLoader/Shredder.cpp` defines and claims
// (`config/G2ME01/splits.txt:1773-1775`), and whose `.text` 0x8021F9B8..0x8021F9E4 ends exactly
// where this range starts.  Its reader, `LoadShredder` in the same file, is the function
// immediately before this one and calls member 0 of that slot.  Declaring the slot `extern` here is
// what keeps it defined in exactly one unit.
//
// **The argument is read off the module's two calls**
// (`build/G2ME01/Shredder/asm/auto_00_000000C8_text.s`), because retail does not name the record:
// `RELExit` (0x24 bytes) does `li r3, 0` and passes null - the module tears the loader down on the
// way out - and the registration `fn_68_138` (0x30 bytes, reached from `RELMain`) does `lis r4,
// fn_68_168@ha` , `addi r0, r4, fn_68_168@l` , `lis r3, lbl_68_bss_0@ha` , `stwu r0,
// lbl_68_bss_0@l(r3)` and then calls `fn_8021F9E4` with `r3` still pointing at `lbl_68_bss_0`.  So
// the argument is that record's address, and the record is **4 bytes**: one `FScriptLoader`
// (`build/G2ME01/Shredder/asm/auto_05_00000000_bss.s` gives `lbl_68_bss_0 size:0x4`, and the
// module's whole `.bss` is 0x4).  Shredder loads one entity, so this is the bare function pointer
// and the parameter is that pointer - the same shape as `fn_80200E70` in
// `src/MetroidPrime/ScriptLoader/Carve80200E70.c`, Kralee's, whose module record is also 4 bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap and because both neighbours are
// claimed units: `Shredder.cpp` ends at 0x8021F9E4 and
// `MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp` starts at 0x8021F9EC.
//
// The port compiles this file too (`files.cmake`), and it adds no undefined symbol: `Shredder.cpp`
// is in the port's source list and defines `gLoader_Shredder`, and nothing here calls anything.

/** The record the module hands over, 4 bytes as measured above.  Only its address is stored, so
 *  the field type describes the shape rather than a layout this unit reads; `Shredder.cpp` spells
 *  the same 4 bytes `FScriptLoader* value` at `+0`.  Declared at file scope rather than inside the
 *  parameter list, because a tag first seen in a prototype is scoped to that list and the host
 *  build then rejects the definition as a conflicting type. */
struct SShredderLoader {
  unsigned int loader; /* FScriptLoader */
};

/** `Shredder.cpp` defines this in `.sbss 0x80419518` and dereferences it at `+0`; MWCC does not
 *  encode a variable's type in its name, so this references `gLoader_Shredder` itself whatever the
 *  type is spelled. */
extern struct SShredderLoader* gLoader_Shredder;

void fn_8021F9E4(struct SShredderLoader* loader) { gLoader_Shredder = loader; }