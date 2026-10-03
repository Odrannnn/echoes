// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:61`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80003BE8_text.s:247-252`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80003EFC..0x80003F08, 0xC = 12 bytes, 1 function:
//
//   fn_80003EFC    0x80003EFC  0xC     3 instructions
//
// **It is a byte-shape twin of `fn_80004010`**, the function already matched in
// `src/MetroidPrime/Carve80004010.c` (0x80004010..0x8000401C, 3/3 at 100.00%), which is itself the
// twin of `fn_80038D4C` (`src/MetroidPrime/CStateManager.cpp:105`, 100%): the three bytes' worth
// of shape `li r0,0 / stw r0,4(r3) / blr` is `vec[1] = 0`, i.e. it clears the second word of the
// object it is handed, so the twin's spelling is the spelling here.
//
// Its only caller is retail's own `rstl::vector<rstl::pair<unsigned int, unsigned int>,
// rstl::rmemory_allocator>::operator=(const rstl::vector<...>&)`
// (`__as__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>FRCQ24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>`,
// `build/G2ME01/asm/auto_03_80003BE8_text.s:192-245`), which `bl`s it at 0x80003E6C with `r30` -
// the destination vector - and then tests `+4` of the **source** at 0x80003E74 and, when that word
// is zero, frees `+0xC` of the destination through `Free__7CMemoryFPCv` at 0x80003E80 before
// clearing `+4`/`+8`/`+0xC` with `li r0,0`.  That is the capacity half of an `rstl::vector`
// assignment: the call under test is the same "the element count is zero, so drop the buffer" test
// that `fn_80003F58` makes with `fn_80004010` at 0x80003F80, hence the twin pair.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A single function is descending by construction, but the
// file is written in that shape so that a later extension of the claim can extend downward.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it, 0x80003BE8..0x80003EFC (`fn_80003BE8`, `fn_80003C44`,
// `__as__12CGameOptionsFRC12CGameOptions`, `fn_80003DA0` and the 0xB8-byte
// `rstl::vector<pair<u32,u32>>::operator=`) is unclaimed, and above it,
// 0x80003F08..0x80004010 (`__as__18CPersistentOptionsFRC18CPersistentOptions`,
// `fn_80003F58`) is too.  The directory is retail's own, taken from the nearest claimed ranges:
// below is `MetroidPrime/CMainResetGameState.cpp` (0x80003A48..0x80003BE8) and above is
// `MetroidPrime/Carve80004010.c` (0x80004010..0x8000408C).
//
// The function is a leaf, so nothing above or below the claim needs declaring and this unit has
// no `extern` at all.  For the DOL, dtk's own `auto_*` object defined `fn_80003EFC` and now the two
// halves of that object do without it; for the port link, `src/MetroidPrime/Carve80003EFC.c` is
// the definition and no `PortLinkStubs.cpp` stand-in is needed or wanted - `fn_80003EFC` was not
// stubbed before this carve because no port-linked object referenced it.
//
// The parameter is modelled as the plain `int*` the twin uses.  Only its **second** word is
// written (`stw r0,4(r3)`), so the callee's real argument is an object with a live word at `+4` -
// the count half of the `rstl::vector` the caller has in `r30` - and nothing above `+4` is touched,
// which is why nothing here needs the element type.

void fn_80003EFC(int* vec) { vec[1] = 0; }