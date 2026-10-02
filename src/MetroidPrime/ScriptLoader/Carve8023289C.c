// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9994` and `:20793`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8023289C_text.s`, and the body below is the C those bytes are the
// compilation of.
//
// .text 0x8023289C..0x802328A4, 0x8 = 8 bytes, 1 function:
//
//   fn_8023289C    0x8023289C  0x8    stw     r3, gLoader_AtomicBeta@sda21(r0)
//                                          blr
//
// **What it is: AtomicBeta's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of the landed
// `fn_80232868` in `Carve80232868.c` - the whole shape of the family: store the argument into a
// loader pointer's `.sbss` slot and return.  It never reads back what it stored, so it needs the
// slot's address only, not its element type.
//
// **Who calls it, and what the argument is.**  Every call site is in a REL module that imports
// the DOL function by its retail name, so the argument is read off the module's own listing.
// Module 5 (`config/G2ME01/config.yml:37-40`, `files/RelProd/AtomicBeta.rel`, sha1
// `c5ea789254e2f3efb4616aea172050f41312d81e`), and both callers are in
// `build/G2ME01/AtomicBeta/asm/auto_00_0000009C_text.s`:
//
//   * `fn_5_C8` (`.text` 0xC8, 0x24 bytes) does `li r3, 0x0` (line 27) and calls it on line
//     29 - the module tears its registration down on the way out, so the argument is `0`.
//   * `fn_5_10C` (`.text` 0x10C, 0x30 bytes) does `lis r4, fn_5_13C@ha` (line 52) /
//     `lis r3, lbl_5_bss_0@ha` (line 53) / `addi r0, r4, fn_5_13C@l` (line 55) /
//     `stwu r0, lbl_5_bss_0@l(r3)` (line 56) and calls it on line 57, so r3 still holds the
//     slot's address: the argument is `&lbl_5_bss_0`.  `auto_05_00000000_bss.s` gives
//     `lbl_5_bss_0 size:0x4`, so the DOL slot holds a *pointer to* a 4-byte loader slot, which
//     is why `AtomicBeta.cpp:19` reads it back as
//     `(*gLoader_AtomicBeta.value)(mgr, input, info)`.  That call is already written out there,
//     and `src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp` (the module's own
//     reimplementation, which claims `.text 0x00000000..0x0000009C`) has no
//     `extern "C" fn_8023289C` to reuse - none is needed, because every symbol this unit
//     references is already defined in `src/`.
//
// The slot is `gLoader_AtomicBeta` at `.sbss 0x80419648`, `size:0x8 data:4byte`
// (`symbols.txt:20793`), and `MetroidPrime/ScriptLoader/AtomicBeta.cpp` already claims and
// already defines it - that unit's split takes `.sbss 0x80419648..0x80419650`
// (`config/G2ME01/splits.txt:1814`).  So this unit claims `.text` only and takes the pointer
// as `extern`: a second definition is a duplicate the moment both objects are in the link.
// MWCC does not encode a variable's type in its name, so the store below lands on the same
// address whatever the type is spelled - `AtomicBeta.cpp`'s own `SLoaderSlot` says what the
// two words are; this file needs the address only.
//
// **No host-only block, unlike `Carve80227530.c`.**  That unit had to define `lbl_80419568`
// under `#ifndef __MWERKS__` because the slot it stores into is claimed by no unit of ours, so
// the port's flat link - our sources and not dtk's objects - would have lost it and
// `tools/link_check.sh --strict` would have named a new undefined symbol.  Every symbol this
// unit references is already defined in `src/`, so the port's undefined count cannot move.
//
// `AtomicBeta.cpp:6-8` said the setter was "deliberately NOT claimed: REL modules import it by
// its retail name, so it cannot be renamed and must stay in dtk's auto unit".  What a carve must
// preserve is the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
// preserves it: the unmangled `fn_8023289C` still lands in the DOL link, which is what module
// 5's two `bl fn_8023289C` resolve against.  What a rename would break is the name; what this
// carve changes is only who supplies the bytes.  The same correction `Carve80232868.c` records
// for the identical sentence in `MysteryFlyer.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a module
// hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A one-function file
// cannot get it wrong.
//
// Retail names none of these.  `symbols.txt:9994` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle
// to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// already claimed units: `AtomicBeta.cpp` ends at 0x8023289C (`splits.txt:1813`) and
// `EyeBall.cpp` starts at 0x80234900 (`splits.txt:1817`).  This range is exactly the gap
// between them, 8 bytes, and nothing else claims it.  The tail, 0x802328A4..0x80234900
// (0x205C, 32 functions), is the rest of the auto unit this range was cut from; it reappears
// as its own `auto_03_802328A4_text` unit and needs no `configure.py` entry.
//
// The directory is retail's own, taken from the nearest claimed range: `AtomicBeta.cpp` owns
// 0x80232870..0x8023289C and this address is the next 8 bytes, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is.

/** `.sbss 0x80419648`, `symbols.txt:20793`, `size:0x8 data:4byte`: AtomicBeta's loader slot,
 *  read by `LoadAtomicBeta` in `AtomicBeta.cpp` at `+0`.  That unit defines it; declared,
 *  never defined here. */
extern void* gLoader_AtomicBeta;

/** AtomicBeta's registration (module 5): `loader` is `&lbl_5_bss_0`, the module's own 4-byte
 *  record holding `&fn_5_13C`; `0` is what `fn_5_C8` passes on the way out. */
void fn_8023289C(void* loader) { gLoader_AtomicBeta = loader; }