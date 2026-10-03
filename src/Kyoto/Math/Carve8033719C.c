// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8033719C_text.s`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x8033719C..0x803371A4, 0x8 = 8 bytes, 1 function:
//
//   fn_8033719C    0x8033719C  0x8    lfs f1, lbl_8041ED38@sda21(r0)
//                                          blr
//
// **What it is: a vtable slot that returns a float read out of retail's `.sdata2` pool.**  It
// is the byte-shape twin of `CParticleGen::GetGeneratorRate` at 0x800534BC
// (`src/MetroidPrime/CExplosion.cpp:44`, `lfs f1,lbl_8041A920@sda21(r0)` / `blr`), and the only
// difference between the two encodings is the 16-bit `@sda21` displacement, because the two pool
// words sit at different addresses.  Retail returns the float in `f1`, so this takes no argument
// and returns `float`, and it cannot be a folded `li f1,0` - a float zero has to come from
// memory, which is why retail loads a pool word at all.
//
// **The word is `lbl_8041ED38`, and it is not a guess.**  dtk's listing of the unclaimed data
// unit prints `# .sdata2:0x110 | 0x8041ED38 | size: 0x4` / `.obj lbl_8041ED38, global` /
// `.float 0` (`build/G2ME01/asm/auto_11_8041EC28_sdata2.s`), and the DOL agrees -
// `build/binutils/powerpc-eabi-objdump -s -j .sdata2 --start-address=0x8041ED30` on
// `build/G2ME01/main.elf` reads `3a83126f 3f4ccccd 00000000 00000000`, so the word at
// 0x8041ED38 is `0x00000000` = `0.0f` and the zero after it is the alignment pad the pool
// needs for the `double` at 0x8041ED40.
//
// **It is declared, not claimed, and that is the whole trick.**  Two alternatives were built
// and measured before this one:
//
//  * `return 0.f;`, with this unit additionally claiming `.sdata2 0x8041ED38..0x8041ED40` so
//    its own pool word landed on the right address.  mwcceppc gives a bare literal a *local*
//    pool name, so the retail name disappeared from the DOL link and the ten neighbours that
//    read the same word broke the build outright:
//    ```
//    ### mwldeppc.exe Linker Error:
//    #   undefined: 'lbl_8041ED38'
//    #   Referenced from 'fn_803371EC' in auto_03_803371A8_text.o
//    ### mwldeppc.exe Linker Error:
//    #   undefined: 'lbl_8041ED38'
//    #   Referenced from 'fn_803378E0' in auto_03_803371F8_text.o
//    ... 9 more, then:  #   Link failed.
//    ```
//    Naming the constant `lbl_8041ED38` instead does put a global definition at the claimed
//    address, but the *code* still does not load it: mwcceppc constant-folds the read and
//    points the `lfs` at its own pool copy, which lands 4 bytes later.  Measured, that object
//    had `lbl_8041ED38` at `.sdata2` offset 0 and the relocation against `@5` at offset 4, and
//    the DOL came out one byte wrong - `C0 22 C9 7C` where retail has `C0 22 C9 78`, i.e.
//    displacement `0xEFBC` against `_SDA_BASE_ = 0x8041FD80` (`powerpc-eabi-nm`) instead of
//    `0xEFB8`.  `volatile` on the constant does not change it; the folding happens anyway.
//    This is the `lbl_8041C398` failure `src/MetroidPrime/PortGlobals.cpp:993` records, and it
//    is why the twin is written `return 1.f` *inside a unit that already claims* the pool - a
//    carve may not do that.
//  * So: declare the retail symbol, exactly as `Carve8023E5A8.c` does for `lbl_8041DCB0` and
//    `Carve80229EAC.c` for `lbl_80419590`.  The matching build binds it from dtk's own
//    `auto_11_8041EC28_sdata2.o`, the `lfs` displacement comes out `0xEFB8`, the ten
//    neighbours keep the name they were already using, and this unit carries no `.sdata2` claim
//    at all.  The host-only definition below carries retail's own `0.f`, so the port's flat
//    link takes no new undefined symbol.
//
// Ten functions still reference the word, one in `auto_03_803371A8_text.s` (`fn_803371EC`,
// 0x803371EC - `lfs f1, lbl_8041ED38@sda21(r0)` / `blr`) and nine in
// `auto_03_803371F8_text.o` (`fn_8033750C`, `fn_8033758C`, `fn_8033762C`, `fn_80337794`,
// `fn_803377EC`, `fn_80337844`, `fn_8033788C`, `fn_803378E0`, and one more, all reported by
// mwldeppc when the name was missing).  None of them is claimed here; they keep resolving
// against the same address, which is what declaring rather than claiming preserves.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt:15142` carries the `fn_<addr>` placeholder
// (`fn_8033719C = .text:0x8033719C; // type:function size:0x8 align:4`) and this file reproduces
// that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init; there are no
// loops here.)
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are not this one: `Kyoto/Math/Carve80337198.c` claims
// `.text 0x80337198..0x8033719C` and `Kyoto/Math/Carve803371A4.c` claims
// `.text 0x803371A4..0x803371A8`, so 0x8033719C..0x803371A4 is the gap between two claimed
// units and cannot be added to either.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0xF58C bytes into `Kyoto/Math/CMayaSpline.cpp` (`.text start:0x80327C10`), so the code is
// that unit neighbourhood.  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.

/** `.sdata2 0x8041ED38`, `symbols.txt:25436`, `size:0x4 data:float`: retail's own pool word,
 *  claimed by no unit of ours, so the matching build takes the real bytes from dtk's
 *  `auto_11_8041EC28_sdata2.o` and this unit only loads them.  Declared, never defined here on
 *  the matching side - see the host-only definition below. */
extern const float lbl_8041ED38;

float fn_8033719C(void) { return lbl_8041ED38; }

#ifndef __MWERKS__
// Host-only definition of the one symbol this unit references that nothing else in `src/`
// defines.  The matching build does not compile this block, so `main.dol` still takes the real
// 0x8041ED38 from dtk's own `auto_11_8041EC28_sdata2.o` and the function keeps its retail bytes.
// Without it the port's flat link - which carries our sources and not dtk's objects - loses
// `lbl_8041ED38`, and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh`
// reports, names it as a new undefined symbol.  The value is retail's own, the `0x00000000`
// measured above, not a placeholder.  The guard is `__MWERKS__` rather than `TARGET_PC` for the
// reason `src/MetroidPrime/ScriptLoader/Carve80229EAC.c:90` records - the thing that must not
// happen is a second definition in the matching build, where dtk's data unit owns the word.
const float lbl_8041ED38 = 0.f;
#endif