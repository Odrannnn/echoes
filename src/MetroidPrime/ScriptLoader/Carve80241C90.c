// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10220-10221`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8023C998_text.s:6209-6234` while this range was
// still unclaimed, and the bodies below are the C those bytes are the compilation of.
//
// .text 0x80241C90..0x80241CD8, 0x48 = 72 bytes, 2 functions:
//
//   fn_80241C90    0x80241C90  0x3C = 60 bytes  15 instructions  deleting-destructor shape
//   fn_80241CCC    0x80241CCC  0x0C = 12 bytes   3 instructions  default-constructor shape
//
// **What the two are.**  Retail names neither, so the twins identify them, and both twins are
// exact - same instructions, same operand schedule, only the `bl` displacement differs:
//
//   fn_80241C90  is `__dt__5CMainFv` (0x800087DC, 0x3C = 60 bytes, `src/MetroidPrime/main.cpp:517`
//                `CMain::~CMain() {}`) word for word, `bl Free__7CMemoryFPCv` included: the
//                receiver is tested (`mr. r31,r3` / `beq`), the flag is re-tested as a **short**
//                (`extsh. r0,r4` / `ble` - a `bool` would give `extsb.`) and `if (flag > 0)` sits
//                **inside** `if (self)`, so the receiver's `beq` lands on the epilogue.  That
//                epilogue returns the receiver in r3 (`mr r3,r31`), and that is what says the
//                return is `void*` and not `void`: as `void` mwcceppc drops the move and the body
//                compiles to 14 instructions / 56 bytes, so the function is 12 bytes short and
//                does not fit the claim.
//   fn_80241CCC  is `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC = 12 bytes, spelled
//                `mFogMode = kRFM_None;` at `src/MetroidPrime/CGameArea.cpp:1592`, the member
//                `ERglFogMode mFogMode` first at `include/MetroidPrime/CGameArea.hpp:120`)
//                exactly: `li r0,0` / `stw r0,0(r3)` / `blr` - store a zero word at +0 and
//                return.  With twelve call sites that is the shape of a **default constructor**,
//                not of a clear: every caller materialises the object in place and then writes it,
//                so nothing observable says more.  The type of the word is not observable: a
//                constant store needs no type, and the twin is an enum.
//
// **Who calls them, and with what.**  `fn_80241C90` has 13 call sites - eleven in dtk's unclaimed
// `auto_*` units (0x8009FE54, 0x801DEB6C, 0x801DEB78, 0x801DF9F0, 0x801DFA08, 0x801E05CC,
// 0x801E05FC, 0x801E0614, 0x801EA180, 0x801EA18C, 0x801FB44C), one in our own
// `MetroidPrime/ScriptLoader/Carve80220394.c` (0x802203C0, on `this + 4`) and one in
// `MetroidPrime/ScriptObjects/CScriptEffect.cpp`'s retail object (0x80080F84, on `this + 0xb8`).
// **Every one of the 13 passes `li r4,-1` immediately before the call**, so the
// `extsh. r0,r4` / `ble` pair always takes the skip and the free is dead at all of them: `-1` is
// MWCC's "destroy, do not free me afterwards", so what retail uses these for is the member's
// destructor walk, not a deallocation.  The free is live in the *source* - which is why the
// branch is written - and dead at every call site this tree can see.
// `fn_80241CCC` has twelve call sites, also all in unclaimed `auto_*` units plus 0x80220478 (our
// own `Carve80220394.c`'s constructor chain calls it on `this + 4`).
//
// **Its own unit because a claim may not span an unclaimed gap, and because this range is
// strictly interior to one.**  The whole of 0x8023C998..0x80242894 is still dtk's
// `auto_03_8023C998_text`, and it is one contiguous unclaimed run, so 0x80241C90..0x80241CD8 is
// the whole writable part of it that this item claims and nothing else in it is touched.  The
// nearest claimed boundary below is `MetroidPrime/ScriptLoader/Carve8023C950.c` ending at
// 0x8023C998 and the one above is `MetroidPrime/ScriptLoader.cpp` starting at 0x80242894, so this
// range is not adjacent to either - which is the case that creates a dtk link-order cycle
// ("Cyclic dependency ... link order", see `docs/RUNNING_THE_DECOMP.md`).  Neither neighbouring
// function is twin-shaped and writable: 0x80241BE8 above ends the run below this one with a
// plain `blr`, and `fn_80241CD8` (0x80241CD8) is a loop with a `lis r4,32760` string-address
// materialisation, not twin-shaped.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying "fits"
// and a broken DOL.  Only `tools/flip_test.sh` catches that.  `fn_80241CCC` (0x80241CCC) is the
// *higher* address and therefore comes first here; the object then leads with `fn_80241C90` at
// offset 0, which is where retail has it.  `tools/check_decl_order.py --unit
// MetroidPrime/ScriptLoader/Carve80241C90.c` is the cheap check for the order.
//
// Retail names none of these, so `symbols.txt:10220-10221` carries the `fn_<addr>` placeholder and
// this file reproduces those symbols verbatim, which is why the definitions have to stay C: a C++
// one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the
// unit is a `.c` rather than a `.cpp`.
//
// The directory is retail's own, taken from the nearest claimed range: the `ScriptLoader/` carves
// own the range below this one (`Carve8023C950.c` ends at 0x8023C998) and `ScriptLoader.cpp` the
// range above it.  For an anonymous function that is the only evidence there is, and it beats a
// lane picking the directory it happened to own.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes), is
// claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined under this name for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp:38`, so this unit needs no `#ifndef __MWERKS__` stand-in and
// adds no undefined symbol: it declares the symbol and never defines it.
//
// **Two duplicate definitions this claim removes.**  Claiming the range moves `fn_80241C90` from
// dtk's auto object into ours, so `src/MetroidPrime/ScriptLoader/Carve80220394.c`'s
// `#ifdef TARGET_PC` stand-in for it had to go - `link_gap.py` counts what is *missing* and
// cannot see a symbol defined twice, and the boot probe cannot either, because it links with the
// reach stubs; `tools/gate.sh`'s `port link dups` step is what catches it.  `fn_80241CCC` had no
// host definition at all, so listing this unit resolves one more name for the port link.

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:38`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_80241C90(void* self, short flag);
void fn_80241CCC(int* self);

/** `fn_80241CCC` - retail `.text:0x80241CCC`, 0xC = 12 bytes: store the zero word at +0.  The
 *  twin is `DisableFog__Q29CGameArea8CAreaFogFv`, three instructions, and this is the same three.
 *  With twelve call sites in place this is a default constructor.  Reached from our own
 *  `Carve80220394.c` at `this + 4` (0x80220478) and from ten unclaimed `auto_*` units. */
void fn_80241CCC(int* self) { *self = 0; }

/** `fn_80241C90` - retail `.text:0x80241C90`, 0x3C = 60 bytes: a deleting-destructor-shaped
 *  step.  `self` is tested and kept for the return, the flag is a **short**, and only a positive
 *  flag reaches the free; the receiver is returned in r3 whatever the flag was.  The twin
 *  `__dt__5CMainFv` is these 15 instructions word for word, and all thirteen of its call sites
 *  (0x80080F84, 0x8009FE54, 0x801DEB6C, 0x801DEB78, 0x801DF9F0, 0x801DFA08, 0x801E05CC,
 * 0x801E05FC, 0x801E0614, 0x801EA180, 0x801EA18C, 0x801FB44C, 0x802203C0) pass `li r4,-1`, so the
 *  free is dead at every one of them. */
void* fn_80241C90(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
