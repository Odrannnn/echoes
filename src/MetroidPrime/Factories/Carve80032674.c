// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:959-961`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80032674_text.s:8-85`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80032674..0x80032774, 0x100 = 256 bytes, 3 functions:
//
//   fn_80032674    0x80032674  0x54    21 instructions
//   fn_800326C8    0x800326C8  0x54    21 instructions
//   fn_8003271C    0x8003271C  0x58    22 instructions
//
// **All three are `rstl::single_ptr<T>` deleting destructors**, and which one is which is read
// off the two matched twins rather than guessed:
//
//   `fn_80032674` and `fn_800326C8` are byte-identical to the matched `fn_80032B94`
//   (0x80032B94, 0x54) in `src/MetroidPrime/Factories/Carve80032A98.c` - which is itself
//   `Matching` (`configure.py:708`) at 100.00% - apart from the two `bl` displacements, which
//   are address-relative.  Both `bl`s in both functions target `Free__7CMemoryFPCv`
//   (0x802CE388), so the body was read straight off that twin and only the operands change.
//
//   `fn_8003271C` is byte-identical to the matched
//   `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` (0x80006AE0, 0x58 = 88 bytes,
//   `src/MetroidPrime/main.cpp`, `build/report.json` fuzzy_match_percent 100.0) apart from the
//   two `bl` displacements again.  That twin is the same 21 instructions with `li r4,1 /
//   bl <T's own destructor>` in place of the first `Free`, and this copy's callee is
//   `fn_80032774` (0x80032774) - the `1` is the deleting flag, the same spelling `main.cpp:1152`
//   records for `~single_ptr<CMemoryCard>()`.
//
// The three shapes are the MWCC deleting-destructor convention the sibling units
// (`src/MetroidPrime/Carve800045A0.c`, `Carve80032A98.c`) already describe: `mr. r30,r3` /
// `beq` guards the receiver, `extsh. r0,r31` / `ble` re-tests the incoming `short`, `Free(self)`
// is reached only for a positive flag, and `mr r3,r30` returns the receiver on every path.
// `if (flag > 0)` therefore has to stay **inside** `if (self)`, so the `beq` lands on the
// epilogue and not on the `extsh.`.
//
// **Which three they are is measured, not inferred.**  `fn_800324A4`
// (`build/G2ME01/asm/auto_fn_800324A4_text.s:101-120`, itself in the unclaimed
// `auto_fn_800324A4_text`, 0x800324A4..0x80032674) hands each of them to
// `__register_global_object` as a tweak global's destructor: `fn_8003271C` with
// `gpTweakPlayerRes` (lines 101-106), `fn_800326C8` with `gpTweakSlideShow` (108-113) and
// `fn_80032674` with `gpTweakTargeting` (115-120).  All three globals are declared
// `rstl::single_ptr<T>` (`include/MetroidPrime/Tweaks/CTweakPlayerRes.hpp:64`,
// `CTweakSlideShow.hpp:17`, `CTweakTargeting.hpp:128`), which is what makes the shape above a
// `single_ptr` destructor rather than a guess.  `T`'s own destructor is `fn_80032774`
// (0x80032774, 0x88), just above this claim: it destroys five `CMayaSpline`s at +0x104, +0x148,
// +0x18C, +0x1D0 and +0x214, a stride of 0x44 = `CHECK_SIZEOF(CMayaSpline, 0x44)`, and
// 0x214 + 0x44 = 0x258 is where `CTweakPlayerRes::mData` sits - `CHECK_SIZEOF(CTweakPlayerRes,
// 0x25c)`, whose five spline members are `SLdrSpline`, a `typedef` of `CMayaSpline`
// (`include/Kyoto/Math/CMayaSpline.hpp:109`).  The addresses are passed to
// `__register_global_object` as data (`lis`/`addi`), so no `bl` in the DOL calls these three;
// the global destructor chain does, at shutdown.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the neighbours are
// different code: the run below is `fn_800324A4` (0x800324A4, 0x1D0), the tweak-global
// *constructor* that names these three, and the one above is `fn_80032774` (0x80032774, 0x88),
// the `CTweakPlayerRes` destructor that `fn_8003271C` calls - five `CMayaSpline` teardowns, left
// for a later carve.  The claim is therefore exactly 0x80032674..0x80032774.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/Factories/CAssetFactory.cpp` (`.text` 0x80031E60..0x800324A4,
// `splits.txt:133-136`) and the one above is `MetroidPrime/Factories/Carve80032A98.c`
// (0x80032A98..0x80032BE8), so this address sits in the Factories neighbourhood.  For an
// anonymous function that is the only evidence there is, and it beats a lane picking the
// directory it happened to own.  The claim starts and ends inside
// `auto_03_80032674_text` (0x80032674..0x80032C3C), which is what keeps `dtk dol split` from
// reporting a link-order cycle against a neighbouring unit.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  It sits
 *  inside `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), which is `Matching`, so
 *  our own object for that unit supplies these bytes in the DOL link.  Declared, never defined
 *  here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it under that name for the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x80032774, `symbols.txt:962`, size 0x88: `CTweakPlayerRes::~CTweakPlayerRes()` - the
 *  deleting destructor of `T` for `gpTweakPlayerRes`, so it is what `fn_8003271C` calls with the
 *  deleting flag `1`.  **Not claimed by any unit of ours** - it is the byte above this claim and
 *  stays in dtk's `auto_03_80032674_text.o`, which the matching build links.  Declared, never
 *  defined here. */
extern void* fn_80032774(void* self, short flag);

void* fn_8003271C(void* self, short flag) {
  if (self) {
    fn_80032774(*(void**)self, 1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_800326C8(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80032674(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifdef TARGET_PC
// Port-only stand-in for the one symbol in this file that nothing else under `src/` defines.
// The matching build does not compile this block (`PORT_NOTES.md`, "TARGET_PC, and the rule for
// port edits"), so `main.dol` still takes the real 0x80032774 from dtk's auto object above and
// `fn_8003271C` keeps its retail bytes.
//
// The host's flat link carries our sources only, not dtk's objects, so without a definition the
// link loses `fn_80032774`; with it the undefined count does not grow.  The other callee needs
// nothing here: `src/Kyoto/Alloc/PortMwccNew.cpp:34` already defines `Free__7CMemoryFPCv` for
// the host, and a second definition would be a duplicate (`link_check: duplicate definitions`).
//
// Empty body on purpose: a stand-in that returns something plausible is worse than one that
// announces itself.  Nothing in the port calls `fn_80032774` (its only caller in retail is
// `fn_8003271C`, a tweak-global destructor reached from the DOL's global destructor chain, which
// the host port does not run), so this cannot change what the game does.
void* fn_80032774(void* self, short flag) {
  (void)self;
  (void)flag;
  return self;
}
#endif