// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:64-67`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80003BE8_text.s:333-375`, and the bodies below are the C
// those bytes are the compilation of.
//
// .text 0x80004010..0x8000408C, 0x7C = 124 bytes, 3 functions:
//
//   fn_80004010    0x80004010  0xC     3 instructions
//   fn_8000401C    0x8000401C  0x40   16 instructions
//   fn_8000405C    0x8000405C  0x30   12 instructions
//
// **It is one copy-assignment chain of a `u32`-first pair**, each of the three a byte-shape twin
// of a function already matched in this tree - the same instructions apart from call targets - so
// the spellings below are the twins' own:
//
//   fn_80004010  twin of `fn_80038D4C` (`src/MetroidPrime/CStateManager.cpp:105`, 100%).
//                `li r0,0 / stw r0,4(r3) / blr` is `vec[1] = 0`: it clears the second word of the
//                object it is handed.  Its only caller, measured with
//                `grep -rn 'bl fn_80004010' build/G2ME01/asm/`, is 0x80003F80 inside
//                `fn_80003F58` (0x80003F58, 0xB8), which frees `+0xC` through
//                `Free__7CMemoryFPCv` once the word it just cleared tests zero - the
//                element-count half of an `rstl::vector` clear.
//   fn_8000401C  twin of `fn_80026F28` (`src/MetroidPrime/CAnimData.cpp:1257`, 100%), an
//                `rstl::pair` copy-assign: copy the first word, forward the second member's
//                address to the member's own copy, return the destination.  Four callers,
//                measured the same way: 0x80003F24 in the same `auto_*` range, and
//                0x80143120 / 0x80144560 / 0x80145910, the three `CGameState` stream constructors
//                (`build/G2ME01/asm/MetroidPrime/Player/CGameState.s:1282, 2714, 4165`).
//   fn_8000405C  twin of `fn_800391B4` (`src/MetroidPrime/CStateManager.cpp:747`, 100%), the
//                member's copy-assign forwarder: `bl fn_8000408C`, then return the receiver.  Its
//                only caller is 0x80004040 inside `fn_8000401C` above; likewise `fn_8000408C` is
//                called from 0x80004070 and nowhere else.
//
// `fn_8000408C` (0x8000408C, 0xC8) is **above** this claim and is therefore declared, never
// defined here.  It is the copy-assign of the same tree-shaped member `fn_8000405C` forwards to:
// it tears the destination's root at `+0x10` down through `fn_80008D68`, clears `+0x10/+8/+0xC/+4`
// in that order, clones the source's root with `fn_80008C28` and re-links the clone's two chains
// into `+8`/`+0xC`.  Its own 0xC8 bytes are a spelling job of their own, which is why the claim
// stops where it does, and its `bl fn_8000408C` at 0x80004070 is retail's, so the carve cannot
// drop the call.  For the DOL dtk's own `auto_*` object defines it; for the port link it is the
// announced stand-in `stub_186` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_182`
// makes for the neighbouring `Carve800045A0.c`.  Nothing here claims `fn_8000408C` is decompiled.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges: below it, 0x80003BE8..0x80004010 (`fn_80003BE8` .. `fn_80003F58`) is
// unclaimed, and above it, 0x8000408C..0x800045A0 (`fn_8000408C`, `fn_80004154`,
// `__dt__10CGameStateFv`, ...) is too.  The directory is retail's own, taken from the nearest
// claimed ranges: below is `MetroidPrime/CMainResetGameState.cpp` (0x80003A48..0x80003BE8) and
// above is `MetroidPrime/Carve800045A0.c` (0x800045A0..0x80004744).

/** The 0x14-byte member at +4 of the pair.  Only its **address** is used by `fn_8000401C` and
 *  `fn_8000405C`, so the fields are modelled after the displacements its own copy-assign
 *  `fn_8000408C` reads: a counter at +4, two chain ends at +8/+0xC and a root at +0x10.  The
 *  neighbouring `Carve800045A0.c` documents the same shape (`struct STree`) for the object its
 *  `fn_800046D0` destroys, root at +0x10 through the same `fn_80008D68`. */
struct SPairSecond {
  int m0;
  int m4;
  int m8;
  int mC;
  int m10;
};

/** The pair these bytes copy: a `u32` at +0 (`lwz r0,0(r4)` / `stw r0,0(r31)` in `fn_8000401C`)
 *  followed by the member above at +4 (`addi r3,r31,4` / `addi r4,r4,4`). */
struct SPair {
  unsigned int mFirst;
  struct SPairSecond mSecond;
};

void fn_8000408C(struct SPairSecond* self, const struct SPairSecond* other);

void* fn_8000405C(struct SPairSecond* self, const struct SPairSecond* other);
void* fn_8000401C(struct SPair* dest, const struct SPair* src);
void fn_80004010(int* vec);

void* fn_8000405C(struct SPairSecond* self, const struct SPairSecond* other) {
  fn_8000408C(self, other);
  return self;
}

void* fn_8000401C(struct SPair* dest, const struct SPair* src) {
  dest->mFirst = src->mFirst;
  fn_8000405C(&dest->mSecond, &src->mSecond);
  return dest;
}

void fn_80004010(int* vec) { vec[1] = 0; }
