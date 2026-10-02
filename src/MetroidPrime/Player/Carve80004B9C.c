// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:91-92`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80004B9C_text.s` before this claim existed (dtk no longer
// regenerates that file once the range is claimed, so it is the surviving record of retail's
// bytes; dtk now emits ours to `build/G2ME01/asm/MetroidPrime/Player/Carve80004B9C.s`), and the
// body below is the C those bytes are the compilation of.
//
// .text 0x80004B9C..0x80004C4C, 0xB0 = 176 bytes, 2 functions:
//
//   __dt__80004B9C  0x80004B9C  0x50    20 instructions
//   fn_80004BEC     0x80004BEC  0x60    24 instructions
//
// **What the pair is: the saved-game block's own deleting destructor and the element walk it
// calls.**  The object is `SGameStateSlots` in `include/MetroidPrime/Player/CGameStateBlocks.hpp` -
// `{ int x00_count; SGameStateBlock x04_blk[3]; }`, 0x34 bytes - whose 16-byte element the four
// `Matching` units either side of this claim already own:
//
//   0x80004A4C..0x80004AA0  Player/CGameStateBlockDtor.cpp       `fn_80004A4C`,  the element's deleting destructor
//   0x80004AA0..0x80004B9C  Player/CGameStateBlockCopyCtor.cpp   `fn_80004AA0`,  the element copy
//   0x80004B9C..0x80004C4C  this file                           `__dt__80004B9C`, `fn_80004BEC`
//   0x80004C4C..0x80004D5C  Player/Carve80004C4C.c               5 functions, `fn_80004C4C` = `rstl::destroy< SGameStateBlock >`
//   0x80004D5C..0x80004D84  Player/CGameStateBlockConstruct.cpp  `fn_80004D5C`,  the element's `construct`
//
// So `fn_80004BEC` is the one step of that chain that releases the three elements in a row, and
// `__dt__80004B9C` is the head that calls it and then the object itself.
//
// **What `__dt__80004B9C` is, read off its own 20 instructions** (`asm/auto_03_80004B9C_text.s:9-28`):
//
//   80004b9c  stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / stw r31,0xc(r1)
//   80004bac  mr   r31,r4          ; the flag, live from entry
//   80004bb0  stw  r30,0x8(r1)
//   80004bb4  mr.  r30,r3          ; `this`
//   80004bb8  beq  0x80004bd0      ; if (this == 0) return
//   80004bbc  bl   fn_80004BEC      ; destroy_elements(this)
//   80004bc0  extsh. r0,r31         ; the flag, sign-extended to 16
//   80004bc4  ble  0x80004bd0      ; if (flag <= 0) return
//   80004bc8  mr   r3,r30
//   80004bcc  bl   Free__7CMemoryFPCv
//   80004bd0  epilogue, `mr r3,r30` ; returns `this`
//
// **Its twin is exact, and that is what identifies it.**  `fn_80004A4C` (0x80004A4C, 0x54 = 84
// bytes, 21 instructions, `src/MetroidPrime/Player/CGameStateBlockDtor.cpp`) is these twenty
// instructions with one inserted - the `lwz r3,0xc(r30)` at 0x80004A6C, which loads the pointer it
// frees - so this is that function's twin one step up the chain, and the file says so itself
// (line 37-40).  `fn_80004744` (0x80004744, 0x54, `src/MetroidPrime/Carve80004744.c`, `Matching`)
// is a third copy of the same shape.  Three details carry over unchanged and are what fix the
// registers: the receiver is tested **once** (`mr. r30,r3 ; beq`) and the epilogue's `mr r3,r30`
// is the return, the flag is compared as **16-bit** (`extsh.` is the halfword sign-extend, so the
// parameter is a `short`), and the flag never leaves `r4` - the only call takes its argument in
// `r3` - so the frame saves `r30` and `r31` and nothing else.
//
// **The return type is measured, not assumed.**  Retail's epilogue is `mr r3,r30`, and spelled
// with a `void` return mwcceppc drops it: the object comes out 19 instructions / 76 bytes and every
// instruction from 0x80004BD4 on is shifted.  So the function returns its receiver, and the two
// declarations elsewhere in the tree that spell it `void` - `src/MetroidPrime/main.cpp:1996` and
// `src/MetroidPrime/CMainResetGameState.cpp:294`, both `extern "C"`, both discarding the result -
// are wrong on that one word.  Nothing detects it and nothing breaks: the symbol has C linkage, so
// neither mwcceppc nor the port's g++ ever sees a definition and a declaration in the same
// translation unit, and a call that discards a returned pointer is well-formed at the call site.
// They are left alone on purpose, because both are `Matching` units and this item has no business
// editing them.
//
// **What `fn_80004BEC` is.**  `include/rstl/reserved_vector.hpp:97-105` spells `destroy_elements()`
// as `T* ptr = data(); for (int i = 0; i < mCount; ++i) destroy(&ptr[i]);`, and these 24
// instructions are that loop with the two things this block does differently: the cursor starts at
// `self + 4` (not at a heap pointer - the three elements are inline in the 0x34 object) and the
// stride is 0x10.  It is entered at its **bottom** test (`b .L_80004C24`), so a zero count calls
// nothing and still returns.
//
// Two spellings decide the register allocation, and both were measured with
// `tools/carve_diff.sh 80004B9C B0 ...`:
//
//   - **The cursor is declared before the index.**  `unsigned char* it; int i;` puts the index in
//     `r30` and the cursor in `r31`, which is retail's `li r30,0` / `addi r31,r29,0x4`.  Declaring
//     them the other way round is the same arithmetic and swaps both registers: 8 of the 24
//     instructions differ.
//   - **The increment order is `it += 16, ++i`.**  Retail emits `addi r31,r31,0x10` and then
//     `addi r30,r30,1`; `++i, it += 16` emits them the other way round - 2 instructions differ.
//
// And the count is **re-read from the receiver on every iteration** (`lwz r0,0(r29)` at the bottom
// test, `cmpw r30,r0`), so the loop test has to be spelled on `self->mCount` and not on a saved
// copy.  That is the one place this loop differs from the copy loop beside it: `fn_80004CD4`
// (0x80004CD4, `Player/Carve80004C4C.c`) takes its count in `r4` and keeps it in a register for the
// whole loop, which is why its body is spelled with a `remaining` local and this one is not.
//
// **The two callees are declared, never defined here, and both are already ours**, so this object
// adds no name to the port's undefined set and closes one: `fn_80004C4C` by
// `src/MetroidPrime/Player/Carve80004C4C.c` and `Free__7CMemoryFPCv` by `src/Kyoto/Alloc/CMemory.cpp`
// (0x802CE388, `symbols.txt:12992`; `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host).
//
// **Its own unit, and the claim is the whole of what was unclaimed.**  A claim may not span an
// unclaimed gap, and 0x80004B9C..0x80004C4C is exactly dtk's `auto_03_80004B9C_text` with nothing
// left in it: below is `Player/CGameStateBlockCopyCtor.cpp` (which ends at 0x80004B9C) and above is
// `Player/Carve80004C4C.c` (which starts at 0x80004C4C), a boundary the two `Matching` units above
// already share at 0x80004AA0.  The item named both functions, and both are claimed.
//
// The directory is retail's own, taken from the nearest claimed ranges - all four neighbours are
// in `MetroidPrime/Player/`, so this address is in that neighbourhood too.
//
// **Source order is descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names these `__dt__80004B9C` (a dtor-shaped name dtk guessed from the call sites) and the
// `fn_` placeholder.  This file reproduces both symbols verbatim, so the definitions have to stay C:
// a C++ spelling would mangle `__dt__80004B9C` to `_Z13__dt__80004B9Cv` and objdiff would pair
// nothing.  That is also why the unit is a `.c` rather than a `.cpp`.  The file is compiled as C
// for the port's host build and, like every other source in `files.cmake`, is syntax-checked as C++
// by `tools/probe_sources.sh` - hence the explicit casts, which are compile-time only and leave the
// object byte-identical.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x80004C4C, `symbols.txt:93`, 0x20 = 32 bytes: `rstl::destroy< SGameStateBlock >` for one
 *  element, the one-element teardown this file's loop calls.  Defined by
 *  `src/MetroidPrime/Player/Carve80004C4C.c`, a `Matching` unit whose range starts exactly where
 *  this one ends.  Declared, never defined here. */
extern void fn_80004C4C(void* self);

/** The object both functions below tear down: the element count at +0x00 with the three 16-byte
 *  elements at +0x04 - `SGameStateSlots` in `include/MetroidPrime/Player/CGameStateBlocks.hpp`, of
 *  which only the one word is read here.  The array is reached by the `+ 4` below rather than with
 *  a member, because spelling the latter as a `SGameStateBlock` array would drag in the element
 *  type and the count is the only word these bytes touch. */
struct SBlockHead {
  int mCount;
};

/** `fn_80004BEC` - retail `.text:0x80004BEC`, 0x60 = 96 bytes: `destroy_elements` over the three
 *  inline elements, entered at its bottom test.  The declaration matches
 *  `src/MetroidPrime/Player/CGameState.cpp:35`, the one caller already in the tree
 *  (`fn_80142944`, 0x80142944, which releases the destination's elements before copying over them).
 */
void fn_80004BEC(struct SBlockHead* self);

void fn_80004BEC(struct SBlockHead* self) {
  unsigned char* it;
  int i;
  for (it = (unsigned char*)self + 4, i = 0; i < self->mCount; it += 16, ++i) {
    fn_80004C4C(it);
  }
}

/** `__dt__80004B9C` - retail `.text:0x80004B9C`, 0x50 = 80 bytes: the deleting destructor of
 *  `SGameStateSlots`, twenty instructions that are `fn_80004A4C`'s with the member load replaced by
 *  the element walk.  All five callers in the DOL pass `-1`
 *  (`asm/auto_03_8000408C_text.s:159,162`, `asm/MetroidPrime/CMainResetGameState.s:108`,
 *  `asm/MetroidPrime/main.s:133,139`), so as in the rest of the family the free-through-to-self is
 *  dead at every call site - it is not dead in the source. */
void* __dt__80004B9C(struct SBlockHead* self, short flag);

void* __dt__80004B9C(struct SBlockHead* self, short flag) {
  if (self) {
    fn_80004BEC(self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
