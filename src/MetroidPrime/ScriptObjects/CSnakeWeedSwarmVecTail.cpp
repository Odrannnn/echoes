// CSnakeWeedSwarmVecTail.cpp - SnakeWeedSwarm's (module 71) out-of-line template tail,
// .text 0x3A34..0x3D44: six functions the module emitted out of line at the top of its
// unclaimed middle - three `rstl::vector<T>::reserve` instantiations, the two element-copy
// routines two of them call, and one `rstl::rc_ptr`'s `ReleaseData`. A third unit in the same
// module and the same arrangement as `CPlantScarabSwarmTail.cpp`: the head,
// `CSnakeWeedSwarmRel.cpp`, claims `.text 0x0..0xDC` and `REL/REL_Setup.cpp` claims
// 0x3D44..0x3EE8, this one claims 0x3A34..0x3D44, and everything else stays unclaimed so `dtk`
// fills it from retail and the module's sha1 against `config/G2ME01/config.yml` holds.
//
// Ranges and names from `config/G2ME01/rels/SnakeWeedSwarm/symbols.txt`, bodies from
// `build/G2ME01/SnakeWeedSwarm/asm/auto_00_000000DC_text.s`:
//
//   0x3A34 fn_71_3A34  0x98  `reserve` for the 0xC-stride block; copies through fn_71_3ACC and
//                            has no element-destroy loop at all (12 bytes need no teardown)
//   0x3ACC fn_71_3ACC  0x3C  that block's out-of-line `uninitialized_copy`: three floats per
//                            element, no prologue, and it returns the advanced destination
//   0x3B08 fn_71_3B08  0xB8  `reserve` for the 0x24-stride block; copies through fn_71_3BC0 and
//                            then walks the old range with an empty body before the free
//   0x3BC0 fn_71_3BC0  0x68  that block's out-of-line `uninitialized_copy`, one out-of-line
//                            element copy (fn_71_B50) per element
//   0x3C28 fn_71_3C28  0xCC  `reserve` for the 4-stride block; its element copy is *inlined*
//                            and null-tests the destination each iteration
//   0x3CF4 fn_71_3CF4  0x50  `rstl::rc_ptr<rstl::vector<int> >::ReleaseData`
//
// **The three copies take their two iterators by value**, in a one-word class with a
// converting constructor, exactly as `CPlantScarabSwarmTail.cpp` spells `fn_49_2E60`'s call.
// That is what puts the four stores at r1+0x8/0xc/0x10/0x14 and the argument registers at
// `addi r3,r1,0x14` / `addi r4,r1,0xc` / `mr r5,r31`, and it is measurable: the same three
// `reserve`s written over raw pointers lose all four stores and 0x10 of frame.
// `tools/twin_scan.py` pairs the three with `reserve__Q24rstl48vector<11SConnection,...>Fi`
// (CEntity.cpp), `reserve__Q24rstl59vector<w,CGlyph>,...>Fi` (CRasterFont.cpp) and
// `reserve__Q24rstl60vector<CGameArea::ELayerPhase,...>Fi` (CGameArea.cpp) in the DOL; the
// bodies here are those templates', with this module's strides and this module's callees.
//
// **`mw_version="GC/2.7"` is load-bearing and measured**, the same finding
// `CPlantScarabSwarmTail.cpp` and `CLumiteRelTail.cpp` record for their modules' tails. Under
// the module default GC/1.3.2 this file compiles all six identically except `fn_71_3BC0`, which
// sits at 92.31% (24 of its 26 words): 1.3.2 hoists `lwz r31,0x0(r3)` - the load of `begin` -
// to *after* `mr r29,r4`, where retail has it before, and 2.7 emits it in retail's order. The
// other five are at 100% under both, so this is one function's two words and not a spelling
// difference: the body was not changed between the two measurements.
//
// **fn_71_B50 is left unclaimed.** It is this module's element copy for the 0x24 block -
// `cmplwi dest,0 / beqlr` and then 0x24 bytes of member-by-member float copy, which is the
// null test `fn_71_3C28`'s inlined copy also has - and it is called from `fn_71_3BC0`, so the
// claim does not need it: mwldeppc keeps what a kept section calls, and the module's data
// references `fn_71_3B08` as well (0x133C calls all three `reserve`s), so nothing here needs a
// `force_active:` entry either.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty**, the arrangement
// `CPlantScarabSwarmTail.cpp` and `CLumiteRelTail.cpp` use: every callee here is either a DOL
// import or a symbol of this module's own unclaimed middle (`fn_71_B50`), which a flat host link
// does not have, and `tools/check_files_cmake.py` requires this file in `files.cmake` all the
// same. Definitions are in descending retail text order (mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim, so ascending would
// permute the module's bytes with objdiff still at 100% and only the module's sha1 would catch
// it).

#include "types.h"

/** 0x802FDAB8, `config/G2ME01/symbols.txt`: `rstl::rmemory_allocator::allocate(int)`, declared
 *  under retail's own emitted spelling so the call needs no header. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`, claimed by `Kyoto/Alloc/CMemory.cpp`
 *  in the DOL. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x800E6810, `symbols.txt`: `CModelData`'s deleting destructor. This is `rstl::rc_ptr`'s
 *  `delete GetPtr()`, so it is reached with the deleting flag set. */
extern "C" void __dt__10CModelDataFv(void* self, int flag);

/** 0xB50, 0x54: this module's own copy of one element of the 0x24 block - a null test and then
 *  0x24 bytes copied member for member. Left unclaimed, above. */
extern "C" void fn_71_B50(void* dest, const void* src);

/** One word of the 0x24 block's element iterator, built by a converting constructor so that the
 *  by-value arguments of `fn_71_3B08`'s call come out as retail's own two 8-byte slots. The
 *  three operators are what `fn_71_3BC0`'s loop is written against - the shape
 *  `include/rstl/construct.hpp:118`'s `uninitialized_copy` has, which this function is. */
struct SStateIter {
  void* mCur;
  SStateIter(void* cur) : mCur(cur) {}
  bool operator!=(const SStateIter& other) const { return mCur != other.mCur; }
  void operator++() { mCur = static_cast< char* >(mCur) + 36; }
  void* operator*() const { return mCur; }
};

/** The same one word for the 0xC block, over three floats. */
struct SVec3Iter {
  void* mCur;
  SVec3Iter(void* cur) : mCur(cur) {}
};

/** The 4-byte block's element iterator, the same one word, passed by value: `operator*` returns
 *  the word itself and the increment steps by one. */
struct SIntIter {
  int* mCur;
  SIntIter(int* cur) : mCur(cur) {}
  bool operator!=(const SIntIter& other) const { return mCur != other.mCur; }
  SIntIter& operator++() {
    ++mCur;
    return *this;
  }
  int& operator*() const { return *mCur; }
};

/** All three blocks are this tree's `rstl::vector` layout: `x00` unread, `x04` the element
 *  count, `x08` the capacity and `x0c` the base pointer (`mAllocator` is four bytes here, which
 *  is what puts the count at 0x4 rather than 0x8). */
struct SBlock4 {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

/** The 0x24 block's element, named only for its size: nothing here reads a member, and
 *  `fn_71_B50` (left unclaimed) is the one that knows the layout. Naming the element is what
 *  keeps the destroy loop below a plain pointer walk - over `char*` MWCC strength-reduces the
 *  empty loop to a `mtctr`/`bdnz` countdown, and retail's is `addi`/`cmplw`/`bne`. */
struct SState {
  unsigned char mBytes[36];
};

/** Three floats, as `fn_71_3ACC` reads and writes them. */
struct SVec3 {
  float x;
  float y;
  float z;
};

/** `rstl::rc_ptr`'s two words, read through a same-layout view rather than through the class:
 *  the refcount is a separate four-byte `CMemory` allocation and both members are private. */
struct SPairRcPtr {
  void* x0_ptr;
  int* x4_refCount;
};

/** `rstl::uninitialized_copy` over the 4-stride block's iterator, inlined at its one call site -
 *  the shape `include/rstl/construct.hpp`'s template has. The destination null test is not
 *  decoration: this module's element copy for the 0x24 block (`fn_71_B50`, left unclaimed) is
 *  `cmplwi dest,0 / beqlr` and then the member copy, and retail's inlined copy here is the
 *  same test before the same `lwz`/`stw`. */
static inline int* CopyWords(SIntIter begin, SIntIter end, int* out) {
  int* tmp = out;
  SIntIter cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    if (tmp != 0) {
      *tmp = *cur;
    }
  }
  return tmp;
}

/** `rstl::destroy` over a range of a type with nothing to tear down. The body is empty and the
 *  loop is still emitted - retail's `addi r4,r4,4` with nothing between it and the condition -
 *  because the element's destructor call is what the compiler is counting on, not any work. */
template < class T >
static inline void DestroyWords(T* begin, T* end) {
  for (T* p = begin; p != end; ++p) {
  }
}

#ifdef __MWERKS__

/** 0x3CF4, 0x50: `rstl::rc_ptr<rstl::vector<int> >::ReleaseData`. The count is decremented and
 *  stored before it is tested (`subic. r0,r3,1 / stw r0,0(r4) / bgt`), which is
 *  `if (--*mRefCount <= 0)`, and only then does the object go through `CModelData`'s deleting
 *  destructor and the refcount through `CMemory::Free`. Twin
 *  `ReleaseData__Q24rstl53rc_ptr<Q24rstl36vector<i,...>>Fv` (src/MetroidPrime/main.cpp) is the
 *  same body with a different pointee. */
extern "C" void fn_71_3CF4(SPairRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    __dt__10CModelDataFv(self->x0_ptr, 1);
    Free__7CMemoryFPCv(self->x4_refCount);
  }
}

/** 0x3C28, 0xCC: the 4-stride block's `reserve`. `capacity <= x08` returns without allocating,
 *  otherwise it allocates `capacity * 4`, moves the live words across, walks the old range with
 *  nothing to destroy, frees the old buffer and stores the new base and capacity back. Both ends
 *  are re-read from `self` after the copy rather than held in locals, which is the trade
 *  `CPlantScarabSwarmTail.cpp` records for `fn_49_2E60`. */
extern "C" void fn_71_3C28(SBlock4* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  int* const buffer = static_cast< int* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 4));
  CopyWords(SIntIter(static_cast< int* >(self->x0c)),
            SIntIter(static_cast< int* >(self->x0c) + self->x04), buffer);
  DestroyWords(static_cast< int* >(self->x0c), static_cast< int* >(self->x0c) + self->x04);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}

/** 0x3BC0, 0x68: the 0x24 block's `uninitialized_copy`, one out-of-line element copy per
 *  element. The two iterators are read at offset 0 only, which is what the one-word iterator
 *  class gives. */
extern "C" void* fn_71_3BC0(SStateIter begin, SStateIter end, void* out) {
  void* tmp = out;
  for (SStateIter cur = begin; cur != end; ++cur, tmp = static_cast< char* >(tmp) + 36) {
    fn_71_B50(tmp, *cur);
  }
  return tmp;
}

/** 0x3B08, 0xB8: the 0x24-stride block's `reserve`. The old range is walked after the copy and
 *  before the free, with a body that destroys nothing - retail's `addi r4,r4,0x24` and its
 *  condition, and nothing between them. */
extern "C" void fn_71_3B08(SBlock4* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  char* const buffer =
      static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 36));
  fn_71_3BC0(SStateIter(self->x0c),
             SStateIter(static_cast< char* >(self->x0c) + self->x04 * 36), buffer);
  DestroyWords(static_cast< SState* >(self->x0c),
               static_cast< SState* >(self->x0c) + self->x04);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}

/** 0x3ACC, 0x3C: the 0xC block's `uninitialized_copy` - three floats per element, no prologue,
 *  and it hands back the advanced destination (`mr r3,r5` before the `blr`). */
extern "C" void* fn_71_3ACC(SVec3Iter begin, SVec3Iter end, SVec3* out) {
  SVec3* dst = out;
  SVec3* cur = static_cast< SVec3* >(begin.mCur);
  SVec3* const last = static_cast< SVec3* >(end.mCur);
  while (cur != last) {
    dst->x = cur->x;
    dst->y = cur->y;
    dst->z = cur->z;
    ++cur;
    ++dst;
  }
  return dst;
}

/** 0x3A34, 0x98: the 0xC-stride block's `reserve`. No destroy loop: retail frees the old base
 *  straight after the copy. */
extern "C" void fn_71_3A34(SBlock4* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  SVec3* const buffer =
      static_cast< SVec3* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 12));
  fn_71_3ACC(SVec3Iter(self->x0c), SVec3Iter(static_cast< SVec3* >(self->x0c) + self->x04),
             buffer);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}

#endif // __MWERKS__