// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:969-972`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80032674_text.s:333-435`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x80032A98..0x80032BE8, 0x150 = 336 bytes, 4 functions:
//
//   fn_80032A98    0x80032A98  0x54    21 instructions
//   fn_80032AEC    0x80032AEC  0x54    21 instructions
//   fn_80032B40    0x80032B40  0x54    21 instructions
//   fn_80032B94    0x80032B94  0x54    21 instructions
//
// **All four are the same function four times, and it is the deleting destructor of a
// `rstl::single_ptr<T>`.**  Each is byte-identical to the matched
// `__dt__Q24rstl68single_ptr<PFRC27CInternalCollisionStructureR18CCollisionInfoList_b>Fv` at
// 0x802836A4 in `build/G2ME01/main.elf` (`src/Collision/CCollisionPrimitive.cpp`, demangled
// `rstl::single_ptr<bool (*)(const CInternalCollisionStructure&, CCollisionInfoList&)>::
// ~single_ptr()`) apart from the two `bl` displacements, which are address-relative - so the
// instructions below are read straight off that twin and only the operands change:
//
//   `lwz r3,0(r30)` then `bl Free__7CMemoryFPCv` is the member teardown: `single_ptr<T>`'s only
//   member is `T* mPtr` at +0 (`include/rstl/single_ptr.hpp:17`), and there is no null test on
//   it, which is what `delete mPtr` compiles to for this instantiation (the twin's bytes prove
//   it, not the source).
//   `mr. r30,r3 / beq` guards the receiver and `extsh. r0,r31 / ble` re-tests the second
//   argument, with `Free(self)` reached only when it is positive: MWCC's deleting-destructor
//   convention, the same shape `docs/RUNNING_THE_DECOMP.md` describes under "The carve vein".
//   `mr r3,r30` after both early exits is the return value, so every path returns the receiver.
//
// **Which four they are is not a guess: the `.ctors` entry at 0x803A54C0 hands each of them to
// `__register_global_object` as the destructor of a named tweak global.**  That initialiser is
// `fn_800324A4` (`build/G2ME01/asm/auto_fn_800324A4_text.s:9-126`, in the unclaimed auto unit
// `auto_fn_800324A4_text`), and it registers `fn_80032A98` with `gpTweakGuiColors`, `fn_80032AEC`
// with `gpTweakGui`, `fn_80032B40` with `gpTweakGame` and `fn_80032B94` with `gpTweakBall` (lines
// 45-50, 38-43, 31-36 and 24-29).  Those four globals are `rstl::single_ptr<CTweakGuiColors>`,
// `<CTweakGui>`, `<CTweakGame>` and `<CTweakBall>` (`include/MetroidPrime/Tweaks/
// CTweakGuiColors.hpp:190`, `CTweakGui.hpp:290`, `CTweakGame.hpp:34`, `CTweakBall.hpp:108`), which
// is what makes the shape above a `single_ptr` destructor rather than a guess.  The addresses
// are passed as data (`lis`/`addi`), so no `bl` in the DOL calls these four; the global
// destructor chain does, at shutdown.
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
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour is one of
// these four: the run below ends at `fn_80032A00` (0x80032A00, 0x98 bytes), a *different*
// destructor - it null-tests its member and destroys three `rstl::basic_string` sub-objects at
// +4, +0x14 and +0x24 (0x80032A38, 0x80032A4C, 0x80032A5C) before freeing it - and the one above
// is `fn_80032BE8` (0x80032BE8, 0x54 bytes), a fifth copy of this same shape, left for the next
// carve.  The claim therefore starts at 0x80032A98 and stops at 0x80032BE8.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/Factories/CAssetFactory.cpp` (`.text` 0x80031E60..0x800324A4) and the
// one above is `MetroidPrime/Weapons/CGameProjectile.cpp` (`.text` 0x80032C3C..0x80036200), so
// this address sits in the Factories neighbourhood.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.  The claim
// starts and ends in the middle of `auto_03_80032674_text` (0x80032674..0x80032C3C), which is
// what keeps `dtk dol split` from reporting a link-order cycle against a neighbouring unit.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  It sits
 *  inside `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C, `splits.txt:2246`), which is
 *  `Matching` (`configure.py:902`), so our own object for that unit supplies these bytes in the
 *  DOL link - `build/binutils/powerpc-eabi-nm build/G2ME01/src/Kyoto/Alloc/CMemory.o` lists
 *  `Free__7CMemoryFPCv` - and both `bl`s below resolve to 0x802CE388.  Declared, never defined
 *  here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it under that name for the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_80032B94(void* self, short flag);
void* fn_80032B40(void* self, short flag);
void* fn_80032AEC(void* self, short flag);
void* fn_80032A98(void* self, short flag);

void* fn_80032B94(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80032B40(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80032AEC(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80032A98(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
