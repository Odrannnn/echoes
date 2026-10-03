// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80213CB8_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80213CB8..0x80213CC0, 0x8 = 8 bytes, 1 function:
//
//   fn_80213CB8    0x80213CB8  0x8    stw     r3, gLoader_SporbBase@sda21(r0)
//                                          blr
//
// **What it is: Sporb's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's, same
// carve shape) and of `fn_80200E70` in `Carve80200E70.c` (Kralee's): store the argument into
// the loader pointer's `.sbss` slot and return.  The three differ only in the 16-bit
// `@sda21` displacement, because their slots are 8 bytes apart - `gLoader_SpacePirate` at
// 0x80419358 is `90 6D 95 D8`, `gLoader_Kralee` at 0x80419360 is `90 6D 95 E0`, and
// `gLoader_SporbBase` at 0x804193A8 is `90 6D 96 28`.  Every other byte of the three is
// identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 76's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Sporb/asm/auto_00_000026A8_text.s`):
//
//   * `RELExit` (0x24 bytes, `li r3, 0`) passes null - the module tears the loader down on
//     the way out.
//   * `fn_76_2804` (0x54 bytes, reached from `RELMain`) does `lis r3, lbl_76_bss_28@ha` and
//     then four stores into it - `stwu r7` at `+0`, `stw r0` at `+4`, `stw r6` at `+8`,
//     `stw r5` at `+0xC` - before `bl fn_80213CB8` with `r3` still pointing at
//     `lbl_76_bss_28`.  So the argument is that record's address, and the record is
//     **0x10 bytes**: four `FScriptLoader`s (`config/G2ME01/rels/Sporb/symbols.txt:517` gives
//     `lbl_76_bss_28 size:0x10 data:4byte`), which is exactly the four-slot struct
//     `SporbBase.cpp` already models - `SSporbBaseLoaders { FScriptLoader slot0..slot3; }` -
//     and the reason that unit has four thunks in it.
//
// The slot is `gLoader_SporbBase` at `.sbss 0x804193A8..0x804193B0`, which `SporbBase.cpp`
// already claims and already defines (`SLoaderSlot { SSporbBaseLoaders* value; unsigned int
// padding; }`), so this unit claims `.text` only and takes the pointer as `extern`.  REL
// modules import this function by its retail name, so it cannot be renamed; defining it here
// keeps the unmangled `fn_80213CB8` in the DOL link, which is what module 76's two
// `bl fn_80213CB8` calls resolve against.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt:8626` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `SporbBase.cpp` ends at 0x80213CB8 and
// `MetroidPrime/Tweaks/CTweakTargeting.cpp` starts at 0x80213CC0.

/** The record module 76 hands over, 0x10 bytes as measured above.  Only its address is stored,
 *  so the field types describe the shape rather than a layout this unit reads;
 *  `SporbBase.cpp:16-21` is the same record in C++ and says what the four slots are.
 *  Declared **above** the prototype below, not inside it: a struct named in a parameter list
 *  is scoped to that list, and the host build then rejects the definition as a conflicting
 *  type. */
struct SSporbBaseLoaders {
  unsigned int loader[4]; /* FScriptLoader slot0..slot3 */
};

/** `SporbBase.cpp` defines this in `.sbss 0x804193A8` and reads `value` at `+0`; MWCC does not
 *  encode a variable's type in its name, so this references `gLoader_SporbBase` itself whatever
 *  the type is spelled. */
extern struct SSporbBaseLoaders* gLoader_SporbBase;

void fn_80213CB8(struct SSporbBaseLoaders* loaders) { gLoader_SporbBase = loaders; }