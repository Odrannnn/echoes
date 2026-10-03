// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8163`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801F7AD0_text.s:968-974`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x801F8818..0x801F8828, 0x10 = 16 bytes, 1 function:
//
//   fn_801F8818    0x801F8818  0x10   stw       r4, 0x0(r3)
//                                        li        r0, 0x0
//                                        stw       r0, 0x4(r3)
//                                        blr
//
// It is the byte-shape twin of the matched `CPASAnimInfo::CPASAnimInfo(int)`
// (`src/Kyoto/Animation/CPASAnimInfo.cpp:3`, `Matching`), whose body is the same store of the
// parameter at +0 - so the spelling below is the twin's own, `this[0] = arg`, widened by the
// `li r0, 0` / `stw r0, 0x4(r3)` pair the matched `fn_80004010`
// (`src/MetroidPrime/Carve80004010.c:98`, `vec[1] = 0`) also emits.  Neither call is made: no
// callee is `bl`-ed, so there is nothing to declare `extern` and no
// `.data`/`.rodata`/`.sbss` is claimed.
//
// What it is of, as far as the disassembly goes: the function named just below it is
// `CPathFindPointSearchFilter::CPathFindPointSearchFilter(float, unsigned int, int)`
// (0x801F8828, 0x10, `symbols.txt:8164`), whose three stores land at +0, +4 and +8 of its `this`,
// and the function named just above it is
// `CPathFindPointSearch::FindClosestPhysicalPoint(const CVector3f&, int&, const
// CPathFindPointSearchFilter&) const` (0x801F86F4, 0x124, `symbols.txt:8162`).  So the word
// cleared here is the first member of the object this call constructs, and the value stored at +0
// is a pointer or flag the constructor is handed - the shape of a constructor that takes a base
// pointer and initialises the first member, which is also the shape of
// `CPathFindPointSearch`'s own inlined base construction at 0x801F8A30 (`lwz r4, 0x0(r31)`,
// `addi r3, r31, 0x4`, `bl fn_801F8A60`).
//
// **No caller was found.** `grep -rn 'bl fn_801F8818' build/G2ME01/asm/` returns nothing and the
// linked disassembly (`.tmp-full-dis.txt`) carries no `bl` to 0x801F8818, so this copy is reached
// indirectly - through a vtable or a relocated call the unclaimed asm does not show - or not at
// all.  Nothing is claimed here about which class it belongs to: retail does not name it, and the
// anonymous-function rule is that the bytes are the claim and the identity is not.
//
// Retail names none of this.  `symbols.txt:8163` carries the `fn_801F8818` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_801F8818v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the order is trivially satisfied here; it
// is stated because a second function must go above this one.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below, 0x801F7AD0..0x801F8818 is unclaimed, and above,
// 0x801F8828..0x801F8A54 is too (its first function is the three-argument
// `CPathFindPointSearchFilter` constructor, 0xA4 bytes).  The directory is retail's own, taken
// from the nearest claimed ranges: below is `MetroidPrime/Carve801F7AC8.c`
// (0x801F7AC8..0x801F7AD0) and above is `MetroidPrime/Carve801F8A54.c` (0x801F8A54..0x801F8A60);
// the named code on both sides is `CPathFindPointSearch`, whose other members live in
// `src/MetroidPrime/PathFinding/`.
void fn_801F8818(int* self, int arg) {
  self[0] = arg;
  self[1] = 0;
}