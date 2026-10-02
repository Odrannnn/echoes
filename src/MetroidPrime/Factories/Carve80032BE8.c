// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:973`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80032BE8_text.s:10-31`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80032BE8..0x80032C3C, 0x54 = 84 bytes, 1 function:
//
//   fn_80032BE8    0x80032BE8  0x54    21 instructions
//
// **It is the deleting destructor of a `rstl::single_ptr<T>`, and the bytes say so rather than
// this comment.**  Retail's own copy of this shape is already matched at 0x802836A4 in
// `build/G2ME01/main.elf` - `__dt__Q24rstl68single_ptr<PFRC27CInternalCollisionStructureR18C
// CollisionInfoList_b>Fv`, in `src/Collision/CCollisionPrimitive.cpp` - and the two functions are
// byte-identical except for the two `bl` displacements, which are address-relative:
//
//   `lwz r3,0(r30)` then `bl Free__7CMemoryFPCv` (0x802CE388) is the member teardown:
//   `single_ptr<T>`'s only member is `T* mPtr` at +0 (`include/rstl/single_ptr.hpp:17`), and
//   there is no null test on the loaded pointer, which is what `delete mPtr` compiles to for
//   this instantiation.  The twin's bytes prove it, not the source.
//   `mr. r30,r3 / beq .L_80032C20` guards the receiver and `extsh. r0,r31 / ble .L_80032C20`
//   re-tests the second argument, with `Free(self)` reached only when that flag is positive:
//   MWCC's deleting-destructor convention.  `mr r3,r30` after both early exits is the return
//   value, so every path returns the receiver.
//
// **What type it is is measured, not guessed: the `.ctors` entry at 0x803A54C0 hands this exact
// address to `__register_global_object` as a named tweak global's destructor.**  That
// initialiser is `fn_800324A4` (`build/G2ME01/asm/auto_fn_800324A4_text.s:9-126`, itself still
// in the unclaimed unit `auto_fn_800324A4_text`); lines 14-19 load `fn_80032BE8@ha`/`@l` into
// r4 and `gpTweakAutoMapper@sda21` into r3 before the call at line 19, so r4 is the destructor
// and r3 the object.  That global is a `rstl::single_ptr<CTweakAutoMapper>`
// (`include/MetroidPrime/Tweaks/CTweakAutoMapper.hpp:96`), which is what makes the shape above a
// `single_ptr` destructor.  The address is passed as data (`lis`/`addi`), so no `bl` in the DOL
// calls this function; the global destructor chain does, at shutdown.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function and a broken DOL on a few
// bytes.  Only `tools/flip_test.sh` catches that.  With one function in the unit the ordering
// cannot go wrong here, but the file is written the same way as the rest of the vein.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_80032BE8v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour is this
// function.  Below, `MetroidPrime/Factories/Carve80032A98.c` claims 0x80032A98..0x80032BE8 and
// holds four *different* copies of this same shape; above, `MetroidPrime/Weapons/
// CGameProjectile.cpp` starts at 0x80032C3C.  0x80032BE8 + 0x54 = 0x80032C3C exactly, so this
// claim spans no unclaimed gap at either end: it is the whole of dtk's `auto_03_80032BE8_text`
// (0x80032BE8..0x80032C3C, 1 function, per `build/report.json`) and nothing more.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/Factories/Carve80032A98.c` and the one above is
// `MetroidPrime/Weapons/CGameProjectile.cpp`, so this address sits in the Factories
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats a
// lane picking the directory it happened to own.  The claim starts and ends inside the old
// `auto_03_80032674_text` (0x80032674..0x80032C3C), which is what keeps `dtk dol split` from
// reporting a link-order cycle against a neighbouring unit.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  It sits
 *  inside `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C, `splits.txt:2271`), which is
 *  `MatchingFor("G2ME01")` (`configure.py:910`), so our own object for that unit supplies these
 *  bytes in the DOL link - `build/binutils/powerpc-eabi-nm build/G2ME01/src/Kyoto/Alloc/
 *  CMemory.o` lists `Free__7CMemoryFPCv` - and both `bl`s above resolve to 0x802CE388.  Declared,
 *  never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it under that name for the
 *  host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_80032BE8(void* self, short flag);

void* fn_80032BE8(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}