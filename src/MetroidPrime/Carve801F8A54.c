// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8170-8171`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801F7AD0_text.s:1143-1148`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x801F8A54..0x801F8A60, 0xC = 12 bytes, 1 function:
//
//   fn_801F8A54    0x801F8A54  0xC    li        r0, 0x0
//                                        stw       r0, 0x4(r3)
//                                        blr
//
// It is the byte-shape twin of the matched `fn_80004010`
// (`src/MetroidPrime/Carve80004010.c:98`, `Matching`, 100.00%) - the same three instructions -
// so the spelling below is the twin's own: `vec[1] = 0`, clearing the second word of the object
// the function is handed.  That unit's header records what that copy is for (the
// element-count half of an `rstl::vector` clear, whose caller frees `+0xC` once the word it just
// cleared tests zero); nothing of the sort is claimed here, because this copy's only caller is
// the destructor of the object it clears.
//
// `fn_801F8A54` has **one** caller, measured with `grep -rn 'bl fn_801F8A54' build/G2ME01/asm/`:
// 0x801F8A3C inside `fn_801F89E0` (0x801F89E0, 0x74), itself an anonymous function of the same
// unclaimed `auto_03_801F7AD0_text` run.  It passes `addi r3, r31, 0x14`, and `mr r31, r3` at
// 0x801F89FC makes r31 the object it was handed, so what is cleared is the word at **+0x18** of
// that object.  `fn_801F89E0` builds a 0x14-byte stack record first (a `0.0f` pair from
// `lbl_8041D640@sda21`, plus the packed byte at 0x18(r1)), hands it to `fn_801F8A60`, and only
// then calls this - a reset, and the zeroing of a trailing flag, which is why a claim that is
// only 12 bytes is worth its own unit.
//
// There are **no callees and no data references**: the three instructions above touch nothing but
// r3, so there is nothing to declare `extern` and no `.data`/`.rodata`/`.sbss` is claimed.  The
// pointer is spelled `int*` because the twin's is, and MWCC does not encode a variable's type in
// its name, so the three bytes of `.text` retail emits are the only thing this spelling has to
// reproduce.
//
// Retail names none of this.  `symbols.txt:8170` carries the `fn_801F8A54` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a
// `.c` rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the order is trivially satisfied here; it
// is stated because a second function must go above this one.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below, 0x801F7AD0..0x801F8A54 is unclaimed, and above,
// 0x801F8A60..0x801F9050 (`fn_801F8A60`, 0xA4) is too.  The directory is retail's own, taken from
// the nearest claimed ranges: below is `MetroidPrime/Carve801F7AC8.c` (0x801F7AC8..0x801F7AD0)
// and above is `MetroidPrime/ScriptObjects/CUnknown90.cpp` (0x801F9050..0x801F9190).
void fn_801F8A54(int* vec) { vec[1] = 0; }