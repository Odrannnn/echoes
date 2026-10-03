/**
 * `fn_80256FD8` - retail `.text:0x80256FD8`, `symbols.txt:10536`, 0x68 = 104 bytes, 26
 *  instructions: retail's own instantiation of `rstl::uninitialized_copy<It, T>`
 *  (`include/rstl/construct.hpp:116-127`), the copy half of the `reserve` below.  `It` is
 *  `rstl::pointer_iterator`, so `begin` and `end` arrive **by value as a one-pointer class** -
 *  `lwz r31,0x0(r3)` at 0x80256FE8 is `begin.current` copied into the loop cursor, `mr r29,r4`
 *  at 0x80256FF8 is the address of `end`, and `lwz r0,0x0(r29)` at 0x80257014 re-reads
 *  `end.current` on every iteration, which is why the bound must not be hoisted.  The return is
 *  `tmp`, the destination block's end pointer (`mr r3,r30` at 0x80257024).
 *
 *  This is a **byte-shape twin** of `fn_801FF6B8` (0x801FF6B8, 0x68,
 *  `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FF5A0.s:94-122`, `Matching`): these same
 *  26 instructions word for word, including the `b` that jumps to the condition rather than
 *  testing on entry, with `bl fn_80256D1C` where the twin has `bl fn_801FEE40` and 0x20 where it
 *  has 0x24.  That twin's source, `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp:144-151`, is
 *  where the body below is read from rather than guessed.
 *
 *  `fn_80256D1C` (0x80256D1C, `symbols.txt:10528`, 0x20) is the `construct` this calls.  It is
 *  declared, never defined here: it is claimed and matched by `WorldFormat/Carve80256D1C.c`
 *  (`.text` 0x80256D1C..0x80256D64), the claim immediately below this one.
 */

/**
 * `fn_80256F20` - retail `.text:0x80256F20`, `symbols.txt:10535`, 0xB8 = 184 bytes, 46
 *  instructions: `rstl::vector< T, Alloc >::reserve(int)` (`include/rstl/vector.hpp:166-179`) for
 *  this copy's 0x20-byte element.  `newSize <= mCapacity` returns without allocating (`cmpw r30,r0
 *  / ble` at 0x80256F44); otherwise it allocates `newSize << 5` (`slwi r3,r30,5` at 0x80256F4C),
 *  moves the live elements across with `fn_80256FD8`, walks the old block with `destroy`, frees
 *  it, and stores the new block and capacity back.
 *
 *  This is a **byte-shape twin** of `fn_801FF5A0` (0x801FF5A0, 0xAC,
 *  `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FF5A0.s:9-54`, `Matching`): the same 46
 *  instructions, with `slwi`/`slwi` for 0x20 where the twin has `mulli ...,0x24` for 0x24, an
 *  empty `destroy` loop here (this copy's element is trivially destructible, so the loop keeps
 *  only its `b`/`addi`/`cmplw`/`bne` and has no body - where the twin calls `fn_801FF64C`),
 *  `bl fn_80256FD8` where the twin has `bl fn_801FF6B8`, and address-relative displacements.
 *
 *  **Both ends are re-read from `self` after the copy instead of being held in locals** - retail
 *  reloads `mCount` and `mItems` at 0x80256F8C/0x80256F90 for the `destroy` walk, and that is the
 *  spelling, not an accident: keeping them costs a fifth live register and a larger frame, the
 *  same trade `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp:165-170` records for its own
 *  byte-identical twin.
 */

/** `allocate__Q24rstl17rmemory_allocatorFi` (0x802FDAB8, `symbols.txt:13824`) is
 *  `rstl::rmemory_allocator::allocate(int)` under CodeWarrior's own mangling - the port spells it
 *  `_ZN4rstl17rmemory_allocator8allocateEi` and supplies retail's as `stub_179` at
 *  `src/MetroidPrime/PortLinkStubs.cpp:888`, so calling it by retail's name is both correct for
 *  the DOL and resolvable for the host.  Taken by its mangled name rather than by including
 *  `rstl/rmemory_allocator.hpp`, because that header would bring an inline copy of the callee in
 *  and this object must define exactly the two functions above.  Declared, never defined here. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is `CMemory::Free(void const*)`.  It is
 *  claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL, so our own tree supplies it.  Declared, never
 *  defined here; `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x80256D1C, 0x20 bytes: the per-element `construct` `fn_80256FD8` calls at 0x80257008, with the
 *  destination first (`mr r3,r30` then `mr r4,r31`).  Claimed and matched by
 *  `WorldFormat/Carve80256D1C.c`, the claim immediately below this one.  `const void*` for the
 *  source because every load in its callee is a read of r4 and every store a write of r3. */
extern "C" void fn_80256D1C(void* dst, const void* src);

/** The one pointer of `rstl::pointer_iterator`, `current`.  What the bytes need of the class is
 *  exactly this: a struct of one pointer passed **by value**, and built by a converting
 *  constructor, because that is what makes `fn_80256F20`'s two argument temporaries come out as
 *  the stores at 0x80256F74/0x80256F7C/0x80256F80/0x80256F84.  It is retail's own
 *  `rstl::pointer_iterator` (`include/rstl/pointer_iterator.hpp:62-99`) narrowed to the member
 *  the twelve instructions of `fn_80256FD8` read. */
struct SAreaSurfaceIter {
  void* mCurrent;
  SAreaSurfaceIter(void* cur) : mCurrent(cur) {}
};

/** The header `fn_80256F20` grows: `rstl::vector`'s four words
 *  (`include/rstl/vector.hpp:18-21`).  `mAllocator` is `rstl::rmemory_allocator`, which is empty,
 *  so the leading word is padding and the three the bytes touch are `mCount` at +4, `mCapacity`
 *  at +8 and `mItems` at +0xC.  The element type is unnamed in retail, so `mItems` is `void*` and
 *  the 0x20 stride is written out where retail writes it; that the element is 0x20 bytes is
 *  stated twice by retail itself, `slwi r3,r30,5` at 0x80256F4C and `addi r4,r4,0x20` at
 *  0x80256FA4. */
struct SAreaSurfaceVec {
  int mAllocatorPad;
  int mCount;
  int mCapacity;
  void* mItems;
};

extern "C" void* fn_80256FD8(SAreaSurfaceIter begin, SAreaSurfaceIter end, void* dst);
extern "C" void fn_80256F20(SAreaSurfaceVec* self, int newSize);

/** `uninitialized_copy`: one 0x20-strided walk that copy-constructs each element and returns the
 *  new end, which is what `fn_80256F20` leaves in r3.  The bound is re-read from `end` every
 *  iteration, so it must not be hoisted into a register. */
extern "C" void* fn_80256FD8(SAreaSurfaceIter begin, SAreaSurfaceIter end, void* dst) {
  char* in = static_cast< char* >(begin.mCurrent);
  char* out = static_cast< char* >(dst);
  for (; in != static_cast< char* >(end.mCurrent); in += 32, out += 32) {
    fn_80256D1C(out, in);
  }
  return out;
}

/** `vector::reserve`.  `newSize <= mCapacity` returns without allocating; otherwise it allocates
 *  `newSize * 0x20`, moves the live elements across with `fn_80256FD8`, walks the old block,
 *  frees it and stores the new block and capacity back.  The `destroy` walk has no body because
 *  this copy's element is trivially destructible, and its shape - `b` to the condition, `addi` the
 *  stride, `cmplw`, `bne` - is retail's, so the loop is written out rather than delegated. */
extern "C" void fn_80256F20(SAreaSurfaceVec* self, int newSize) {
  if (newSize <= self->mCapacity) {
    return;
  }
  char* const buffer =
      static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(newSize * 32));
  fn_80256FD8(SAreaSurfaceIter(self->mItems),
              SAreaSurfaceIter(static_cast< char* >(self->mItems) + self->mCount * 32), buffer);
  for (char* p = static_cast< char* >(self->mItems);
       p != static_cast< char* >(self->mItems) + self->mCount * 32; p += 32) {
  }
  Free__7CMemoryFPCv(self->mItems);
  self->mItems = buffer;
  self->mCapacity = newSize;
}
