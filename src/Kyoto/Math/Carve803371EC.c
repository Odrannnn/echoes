// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_803371A8_text.s`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x803371EC..0x803371F4, 0x8 = 8 bytes, 1 function:
//
//   fn_803371EC    0x803371EC  0x8    lfs f1, lbl_8041ED38@sda21(r0)
//                                          blr
//
// **What it is: the second of the two byte-shape twins of `CParticleGen::GetGeneratorRate`
// at 0x800534BC (`src/MetroidPrime/CExplosion.cpp:44`, `lfs f1,lbl_8041A920@sda21(r0)` /
// `blr`) that sit either side of this address.**  A vtable slot that returns a float read out
// of retail's `.sdata2` pool: no argument, returns `float` in `f1`.  It cannot be a folded
// `li f1,0` - a float zero has to come from memory, which is why retail loads a pool word at
// all - and the only difference between this encoding and its twin's is the 16-bit `@sda21`
// displacement, because the two pool words sit at different addresses.
//
// **The word is `lbl_8041ED38`, and it is not a guess.**  `config/G2ME01/symbols.txt:25436`
// carries `lbl_8041ED38 = .sdata2:0x8041ED38; // type:object size:0x4 align:4 data:float`, dtk's
// listing of the unclaimed data unit prints `# .sdata2:0x110 | 0x8041ED38 | size: 0x4` /
// `.obj lbl_8041ED38, global` / `.float 0` (`build/G2ME01/asm/auto_11_8041EC28_sdata2.s`), and
// the DOL agrees - `build/binutils/powerpc-eabi-objdump -s -j .sdata2
// --start-address=0x8041ED30` on `build/G2ME01/main.elf` reads
// `3a83126f 3f4ccccd 00000000 00000000`, so the word at 0x8041ED38 is `0x00000000` = `0.0f`
// and the zero after it is the alignment pad the pool needs for the `double` at 0x8041ED40.
// `build/binutils/powerpc-eabi-objdump -s -j .text --start-address=0x803371EC` reads
// `c022c978 4e800020`, which is the two instructions above and nothing else.
//
// **It is declared, not claimed, and that is the whole trick.**  The alternative - claiming
// `.sdata2 0x8041ED38..0x8041ED40` so this unit's own pool word landed on the right address -
// was built and measured on the twin at 0x8033719C and it breaks the DOL link outright:
// mwcceppc gives a bare literal a *local* pool name, so the retail name disappears and every
// other function that reads the same word fails to link.  Nine more functions read it -
// `fn_8033750C`, `fn_8033758C`, `fn_8033762C`, `fn_80337794`, `fn_803377EC`, `fn_80337844`,
// `fn_8033788C` and `fn_803378E0` in `auto_03_803371F8_text.s`, plus this one.  `extern const
// float` keeps the name in the link while the pool word stays with dtk's own
// `auto_11_8041EC28_sdata2.o`, so the matching build binds it from there and the `lfs`
// displacement comes out `0xEFB8` against `_SDA_BASE_ = 0x8041FD80`.  The full evidence is in
// `src/Kyoto/Math/Carve8033719C.c`, which is the same function written at the address 0x50
// bytes below and is `Matching` today.
//
// **The host-only definition of `lbl_8041ED38` lives in that twin, not here.**  Only one unit
// may define it - a second `const float lbl_8041ED38 = 0.f;` under `#ifndef __MWERKS__` is a
// duplicate symbol in the port's flat link - so this file only declares it and the
// `#ifndef __MWERKS__` block below is empty on purpose.  The value the host sees is retail's
// own `0.f`, measured above, not a placeholder.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt:15150` carries the `fn_<addr>` placeholder
// (`fn_803371EC = .text:0x803371EC; // type:function size:0x8 align:4`) and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init; there are
// no loops here.)
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are not this one: `Kyoto/Math/Carve803371E0` does not exist as a
// unit, the six `stb`-into-`this` thunks below it are still unclaimed in dtk's
// `auto_03_803371A8_text`, and `Kyoto/Math/Carve803371F4.c` claims the 4 bytes above it, so
// 0x803371EC..0x803371F4 is a gap between a claimed unit and a claimed unit and cannot be
// added to either.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0xF5DC bytes into `Kyoto/Math/CMayaSpline.cpp` (`.text start:0x80327C10`), so the code is
// that unit neighbourhood.  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.

/** `.sdata2 0x8041ED38`, `symbols.txt:25436`, `size:0x4 data:float`: retail's own pool word,
 *  claimed by no unit of ours, so the matching build takes the real bytes from dtk's
 *  `auto_11_8041EC28_sdata2.o` and this unit only loads them.  Declared, never defined here -
 *  `Kyoto/Math/Carve8033719C.c` is the unit that carries the host-only definition, so that the
 *  port's flat link has exactly one. */
extern const float lbl_8041ED38;

float fn_803371EC(void) { return lbl_8041ED38; }