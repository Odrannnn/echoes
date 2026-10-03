// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:6427`, the instructions are the ones dtk itself emitted
// into `build/G2ME01/asm/auto_03_80182830_text.s:795-805` before the claim existed (the same range
// is now `build/G2ME01/asm/MetroidPrime/Carve801832E8.s`), and the body below is the C those bytes
// are the compilation of.  The bytes were re-read this run from the linked ELF with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801832E8 --stop-address=0x80183308
// build/G2ME01/main.elf`.
//
// .text 0x801832E8..0x80183308, 0x20 = 32 bytes, 1 function:
//
//   fn_801832E8    0x801832E8  0x20    8 instructions
//
// **It is a byte-shape twin of `fn_80004438`**, already matched in this tree in
// `src/MetroidPrime/Carve80004438.c:97` as `void fn_80004438(void* self) { fn_80004458(self); }`:
// both are the same eight instructions - `stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / bl /
// lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr` - with no load, no test and no return value,
// differing only in the `bl` target (0x80178AD0 here, 0x80004458 there).  So the body is a frame
// and one unconditional call with `r3`, `r4` and `r5` forwarded untouched, which is what the twin
// spells as `rstl::destroy<T>`'s `destroy_impl(in)`.
//
// **What the call is, measured rather than guessed.**  The only caller of `fn_801832E8` in retail
// (`grep -rn 'bl fn_801832E8' build/G2ME01/asm/`) is **0x80183278**, inside `fn_80183218`
// (0x80183218, 0xD0, unclaimed, in the same `auto_03_80182830_text` run), which is a stream reader
// for an `rstl::vector<CWorldSaveGameInfo::SEnvironmentVariable>`:
//
//   0x80183234  stw   r0,0x4(r3)              // zero three count/cursor words
//   0x80183254  bl    reserve<...SEnvironmentVariable...>Fi
//   0x80183264  lbz   r0,lbl_80419240@sda21(r0) ; loop head, r28 = index, r29 = count
//   0x80183268  mr    r4,r27                   // r27 is the CInputStream
//   0x80183270  addi  r3,r1,0xc               // a frame temporary
//   0x80183274  stb   r0,0x8(r1)              // the byte at lbl_80419240 into that frame word
//   0x80183278  bl    fn_801832E8              // <-- this function
//   0x80183284  mullw r0,r3,0x1c ; ...  bl push_back-shaped copy of the temporary
//
// The callee `fn_80178AD0` (0x80178AD0, 0x7C = 124 bytes, `symbols.txt:6211`, inside
// `MetroidPrime/CMemoryCard.cpp`'s claimed 0x801767BC..0x8017904C range) **clobbers `r5` with
// `addi r5,r1,0x8` before it reads anything**, so the third argument the caller sets up arrives
// here and is never used: retail's own body builds an
// `rstl::basic_string<char, ...>::basic_string(CInputStream&)` into that frame temporary and then
// reads three `uint` from the stream into `+0x10`, `+0x14` and `+0x18` of the object it was handed.
// The three arguments are declared anyway, because the call site sets all three and the signature
// should say what the caller does.
//
// `fn_80178AD0` is **above** this claim and therefore declared, never defined here: its range is
// `MetroidPrime/CMemoryCard.cpp`'s, and nothing here claims it is decompiled.  For the port link it
// is the announced empty stand-in `fn_80178AD0` in `src/MetroidPrime/PortGlobals.cpp`, the same
// trade `fn_8012862C` makes for `Carve801285DC.c`'s callee.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order is trivially satisfied;
// `python3 tools/check_decl_order.py --unit MetroidPrime/Carve801832E8` confirms it.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_801832E8v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it 0x80182830..0x801832E8 is unclaimed (`FSaveWorldFactory`, 0x80182830,
// 0x64, .. up to `fn_80183218`, 0x80183218, 0xD0), and above it 0x80183308..0x801834C8 is too
// (`fn_80183308`, 0x80183308, 0xC0, is the next function and needs its own spelling job).
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/CRainSplashGenerator.cpp` (0x80181750..0x80182830) and above is
// `MetroidPrime/Carve801834C8.cpp` (0x801834C8..0x80183648).

/** `fn_80178AD0` - retail `.text:0x80178AD0`, `symbols.txt:6211`, 0x7C = 124 bytes: the string-from-
 *  stream constructor plus the three `uint` reads described above, and the target of this file's
 *  one `bl`.  Declared, never defined here; the port link's stand-in is the announced empty
 *  `fn_80178AD0` in `src/MetroidPrime/PortGlobals.cpp`.  The third parameter is passed by retail's
 *  caller and overwritten by the callee's own `addi r5,r1,0x8`, so it is spelled but unused. */
extern void fn_80178AD0(void* self, void* stream, void* unused);

/** `fn_801832E8` - retail `.text:0x801832E8`, 0x20 = 32 bytes: a frame and one call to
 *  `fn_80178AD0` with `r3`/`r4`/`r5` forwarded, nothing else.  Its measured twin is
 *  `fn_80004438` (0x80004438, `src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight
 *  instructions with a different `bl` target. */
void fn_801832E8(void* self, void* stream, void* unused);

void fn_801832E8(void* self, void* stream, void* unused) { fn_80178AD0(self, stream, unused); }