// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:8259`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801FBD68_text.s:1274-1279` before the claim existed
// (verified this run with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FCE34 --stop-address=0x801FCE40
// build/G2ME01/main.elf`), and the body below is the C those bytes are the compilation of.
//
// .text 0x801FCE34..0x801FCE40, 0xC = 12 bytes, 1 function:
//
//   fn_801FCE34    0x801FCE34  0xC   3 instructions
//
// **It is a byte-shape twin of `fn_80004010`**, already matched in this tree in
// `src/MetroidPrime/Carve80004010.c:98` as `void fn_80004010(int* vec) { vec[1] = 0; }`:
// `li r0,0 / stw r0,4(r3) / blr` with no other operand anywhere, so it clears the second word of
// the object it is handed and returns void.  That is the same three instructions again - retail
// emitted this body three times in this neighbourhood, and the other two are still dtk's:
// `fn_80195228` and `fn_80195234` (`config/G2ME01/symbols.txt:6708-6709`), both 0xC bytes at
// 0x80195228 and 0x80195234 inside `MetroidPrime/Enemies/CStateMachine.cpp` (a `NonMatching`
// unit, so neither is matched here).
//
// Its only caller, measured with `grep -rn 'bl fn_801FCE34' build/G2ME01/asm/`, is **0x801FCE14**
// inside `fn_801FCDAC` (0x801FCDAC, 0x88, unclaimed), whose teardown tail clears the object it
// was handed (`r29`) word by word and then calls the three clears on its members:
//
//   0x801FCDFC  stw   r0,0x48(r29)      // r0 = 0 from 0x801FCDF4
//   0x801FCE00  stw   r0,0x4(r29)
//   0x801FCE04  bl    fn_80195234       // r3 = r29+0x28
//   0x801FCE0C  bl    fn_80195228       // r3 = r29+0x18
//   0x801FCE14  bl    fn_801FCE34       // r3 = r29+0x18
//
// `fn_801FCDAC` is the reset/teardown half of a state object (it is `.4byte fn_801FCDAC` in a
// vtable at `build/G2ME01/asm/auto_07_803B7AE0_data.s:70`) and clears the element-count word of
// each member container, exactly what `fn_80004010`'s own caller does - see that file's header
// on `fn_80003F58` (0x80003F58, 0xB8): it clears the word, tests it, and on zero releases
// `+0xC` through `Free__7CMemoryFPCv`.  So the body here is the count-clear half of an
// `rstl::vector`-shaped member, the same one the twin clears.  Nothing here claims to know the
// class: the function's own argument is only ever read at `+4`, so the pointer is typed as
// `int*` exactly as the twin types it, and no struct is spelled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order is trivially satisfied;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FCE34` confirms it.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_801FCE34v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.  The file is compiled as C for the port's host build and, like every
// other source in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it 0x801FBD68..0x801FCE34 is unclaimed (`fn_801FBD68` .. up to
// `fn_801FCDAC`), and above it 0x801FCE40..0x801FD4B0 is too (`SetupFSM__17CGenericFSM2State`,
// 0x801FCE40, 0x198, is the next function and needs 0x198 bytes of its own spelling job).
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/ScriptObjects/Carve801FBC58.c` (0x801FBC58..0x801FBD68) and above is
// `MetroidPrime/ScriptObjects/Carve801FD4B0.cpp` (0x801FD4B0..0x801FD52C).

void fn_801FCE34(int* vec) { vec[1] = 0; }