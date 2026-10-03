// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80335A0C_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80335A0C..0x80335A14, 0x8 = 8 bytes, 1 function:
//
//   fn_80335A0C    0x80335A0C  0x8    lfs      f1, lbl_8041ECE8@sda21(r0)
//                                          blr
//
// **What it is: a float constant getter.**  It is the byte-shape twin of the matched
// `CParticleGen::GetGeneratorRate()` in `src/MetroidPrime/CExplosion.cpp`
// (`.text 0x800534BC`, `lfs f1, lbl_8041A920@sda21(r0)` / `blr`) - load one `.sdata2` float
// into the return register and return.  The two differ only in which literal they load:
// `lbl_8041A920` is CExplosion's own pool word, `lbl_8041ECE8` is not.
//
// **Why the constant is `extern` and not a literal.**  The twin can write `return 1.f;`
// because its literal is inside its own claim - `config/G2ME01/splits.txt` gives
// `MetroidPrime/CExplosion.cpp` the range `.sdata2 start:0x8041A900 end:0x8041A928`, and the
// compiler's R_PPC_EMB_SDA21 relocation at `GetGeneratorRate` names `lbl_8041A920` inside
// it.  `lbl_8041ECE8` has no such owner: `config/G2ME01/symbols.txt:25423` types it
// `data:float` at `.sdata2:0x8041ECE8`, but **no unit in `splits.txt` claims that
// address** - it sits inside the unclaimed dtk run `auto_11_8041EC28_sdata2`
// (`.sdata2 0x8041EC28..0x8041EE08`, dtk's own object, `build/G2ME01/asm/
// auto_11_8041EC28_sdata2.s:185`, which is where the link gets the word from and where
// `powerpc-eabi-nm build/G2ME01/main.elf` reports it as `8041ec28 D lbl_8041EC28`).
// So `return 0.f;` would be wrong twice over: it would make this object emit its own pool
// word that no claim places, and it would return the wrong value.  Declaring the word
// `extern` reproduces retail's relocation exactly and claims nothing extra.  The same
// arrangement is in `Kyoto/Graphics/Carve802C2534.cpp` (`extern float lbl_8041E508;`, also
// an unclaimed `auto_11_*` word).
//
// Its value, for the record and not needed by the codegen: `.float 0` in
// `auto_11_8041EC28_sdata2.s`, i.e. 0x00000000 in the linked `.sdata2`.  Its only other
// reader is `fn_803367F8` in `build/G2ME01/asm/auto_03_80335B5C_text.s`, which loads it as
// the default before a switch - so it is a zero default, which is what `return 0.f;` would
// have meant.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object's `.text`
// order verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a
// broken DOL, and a hash that fails on a few bytes.  Only `tools/flip_test.sh` catches
// that.  A one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and both neighbours are
// already claimed units - `Kyoto/Math/Carve803359F4.c` ends exactly at 0x80335A0C and
// `Kyoto/Math/Carve80335A14.c` starts at 0x80335A14, this 8-byte gap being dtk's
// `auto_03_80335A0C_text`.
//
// The directory is retail's own, taken from the nearest claimed range below: this address
// is 0xDDE4 bytes into `Kyoto/Math/CMayaSpline.cpp`, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
//
// A carve is four files or it is not a carve: this source, `configure.py`,
// `config/G2ME01/splits.txt` and `files.cmake`, each entry in address order.
extern float lbl_8041ECE8;

float fn_80335A0C(void) { return lbl_8041ECE8; }