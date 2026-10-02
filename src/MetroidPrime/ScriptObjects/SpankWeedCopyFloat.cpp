// SpankWeedCopyFloat.cpp - a carve of SpankWeed's (module 73) `.text
// 0x000039E8..0x00003ABC`: two adjacent `rstl` template instantiations, 0xD4 = 212 bytes.
//
//   fn_73_39E8  0x98  `rstl::vector<SConnection, rmemory_allocator>::reserve(int)`
//   fn_73_3A80  0x3C  the `uninitialized_copy` it calls
//
// They sit inside the module's unclaimed `auto_00_000003F4_text` run (0x3F4..0x3DA8) and are
// carved out here as their own unit, so `build/report.json` counts them instead of leaving them
// retail bytes - the same arrangement `CElitePirateVecCopy.cpp` is for one `uninitialized_copy` in
// ElitePirate.
//
// **What they are.** `reserve` grows the block: allocate `newSize` records of 12 bytes, copy the
// live ones across, free the old buffer, then store the new pointer at +0xc and the new capacity
// at +0x8. 12-byte records are trivially destructible, so `reserve`'s destroy step is empty and
// this is 0x5C bytes shorter than the 0x68-stride `fn_73_3ABC` one stride up the module, which is
// `rstl::vector<CJointCollisionDescription, rmemory_allocator>::reserve` and does have a destroy
// loop. Both readings are off the module's own disassembly
// (`build/G2ME01/SpankWeed/asm/auto_00_000003F4_text.s`), and both are instruction-for-instruction
// the DOL's, which `CEntity.cpp` and `CCollisionActorManager.cpp` already reproduce at 100%:
// `reserve__Q24rstl48vector<11SConnection,Q24rstl17rmemory_allocator>Fi` (0x800485E4,
// `symbols.txt:1356`, 25 instructions in this order) and its `uninitialized_copy` at 0x8004867C.
//
// **The block's three words are `rstl::vector`'s** (`include/rstl/vector.hpp:18-21`): `mCount` at
// +0x4, `mCapacity` at +0x8 and `mItems` at +0xc, which is the set of offsets `fn_73_39E8` reads.
// The word at +0x0 is the empty `rmemory_allocator`, kept here as `mUnused` so the offsets are the
// header's rather than a guess.
//
// **The declaration that produced the shape** (the thing to copy, per the item): the two iterators
// arrive **by value, as one-word classes**, and that is what the bytes are -
// `addi r3,r1,0x14` and `addi r4,r1,0xc` build the two temporaries that `lwz r3,0x0(r3)` and
// `lwz r0,0x0(r4)` then read, and `rstl::pointer_iterator<T, vector<T, Alloc>, Alloc>` is one word
// wide (`include/rstl/pointer_iterator.hpp:59`). `SSpankIter` below is that class reduced to its
// member; `CElitePirateVecCopy.cpp` states the same for ElitePirate's 0x68-stride copy and measures
// 100%. `fn_73_3A80`'s loop re-reads `end.mCur` once before the loop and `fn_73_3B7C`'s re-reads
// it every iteration - that difference is the compiler's, not the source's, and both spellings
// come out of the same `for`.
//
// Written here as plain functions under the module's own `fn_73_<off>` names rather than as
// template instantiations, for the reason `CElitePirateVecCopy.cpp` gives: `symbols.txt` names the
// module's text `fn_73_39E8` and `fn_73_3A80` (they are TU-local weak instantiations that dtk could
// not name), so renaming them to their mangled forms would change every relocation in the module,
// and `extern "C"` definitions keep the module's symbol table exactly as dtk split it. The two
// imports the body calls - `allocate__Q24rstl17rmemory_allocatorFi` and `Free__7CMemoryFPCv` - are
// this module's own, already imported by the retail functions around this range.
//
// Definitions are in descending retail text order, because mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim - ascending would permute
// the module's bytes with objdiff still at 100% and the module hash breaking. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/SpankWeedCopyFloat.cpp`.
//
// This unit needs `mw_version="GC/2.7"`, for the reason `CElitePirateVecCopy.cpp` gives: REL
// objects default to GC/1.3.2 and that code generator schedules the by-value iterator load
// differently from the one retail used.
//
// **No `force_active:` entry is needed.** `fn_73_39E8` is called from `fn_73_36FC` in the module's
// own unclaimed code, so it is not the orphan that dead-stripped `ScriptCoin`'s tail, and
// `fn_73_3A80` is reached from `fn_73_39E8` beside it.
//
// Listed in `files.cmake` for the reason `CElitePirateVecCopy.cpp` gives: this unit defines no
// RELMain/RELExit, so it does not collide in a flat link, and the two guest-only imports sit
// behind the guard the port cannot resolve. `powerpc-eabi-nm -u` on the host object prints
// nothing, so the port's undefined count does not move.

#include "types.h"

// Everything is behind `__MWERKS__`, as in `CElitePirateVecCopy.cpp`: the callees are guest module
// symbols with no PC-side definition, and the host build compiles this file to nothing so the
// port's link is unchanged.
#ifdef __MWERKS__

#include "Kyoto/Alloc/CMemory.hpp"

extern "C" {
/** `rstl::rmemory_allocator::allocate(int)` - the out-of-line `CMemory`-backed allocation that
 *  `rstl::rmemory_allocator::allocate(T*&, int)` calls after computing `count * sizeof(T)`. */
void* allocate__Q24rstl17rmemory_allocatorFi(int size);
/** `CMemory::Free(const void*)`, which is what `rstl::rmemory_allocator::deallocate` ends in and
 *  what `delete[]` is under mwcceppc. */
void Free__7CMemoryFPCv(const void* ptr);
} // extern "C"

namespace {

/** `rstl::pointer_iterator<T, vector<T, Alloc>, Alloc>`'s single member, `current`. */
struct SSpankIter {
  void* mCur;
  explicit SSpankIter(void* cur) : mCur(cur) {}
};

/** The three words `rstl::vector` keeps after its empty allocator. */
struct SSpankVec {
  int mUnused;
  int mCount;
  int mCapacity;
  void* mItems;
};

} // namespace

extern "C" {

// .text 0x3A80, 0x3C bytes. `rstl::uninitialized_copy` over the 12-byte records: three floats per
// element, no call - `CVector3f`'s copy *is* the three loads and three stores - and it returns the
// new end, which is what `fn_73_39E8` leaves behind.
void* fn_73_3A80(SSpankIter begin, SSpankIter end, void* out) {
  char* cur = static_cast< char* >(begin.mCur);
  float* dst = static_cast< float* >(out);
  for (; cur != static_cast< const char* >(end.mCur); cur += 12, dst += 3) {
    const float* src = reinterpret_cast< const float* >(cur);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  return dst;
}

// .text 0x39E8, 0x98 bytes. `rstl::vector<SConnection, rmemory_allocator>::reserve(int)`: grow the
// block to `newSize` records, copy the live ones across, free the old buffer, store the new pointer
// and the new capacity. `mItems` and `mCount` are re-read after the copy rather than kept in
// locals, which is what the `lwz r0,0xc(r29)` / `lwz r6,0xc(r29)` pair at +0x38 and +0x58 is.
void fn_73_39E8(SSpankVec* self, int newSize) {
  if (newSize <= self->mCapacity) {
    return;
  }
  void* newData = allocate__Q24rstl17rmemory_allocatorFi(newSize * 12);
  fn_73_3A80(SSpankIter(static_cast< char* >(self->mItems)),
             SSpankIter(static_cast< char* >(self->mItems) + self->mCount * 12), newData);
  Free__7CMemoryFPCv(self->mItems);
  self->mItems = newData;
  self->mCapacity = newSize;
}

} // extern "C"

#endif // __MWERKS__