// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7319-7320`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801C13F8_text.s:1584-1665` before the claim
// existed (the same range is now `build/G2ME01/asm/MetroidPrime/Carve801C2BA8.s`), and the bodies
// below are the C++ those bytes are the compilation of.  `tools/flip_test.sh` is what says so.
//
// .text 0x801C2BA8..0x801C2CBC, 0x114 = 276 bytes, 2 functions:
//
//   fn_801C2BA8    0x801C2BA8  0xB8  46 instructions  the block's `reserve`
//   fn_801C2C60    0x801C2C60  0x5C  23 instructions  `rstl::uninitialized_copy`
//
// **Both are byte-shape twins of `Matching` functions in this tree, read out of their own source
// rather than guessed.**  The seed named them; comparing the two `.fn` blocks instruction by
// instruction confirms it.  The comparison is of the two `.text` ranges in `main.elf` with
// `build/binutils/powerpc-eabi-objdump -d --start-address=... --stop-address=...`, and it is exact:
//
//   fn_801C2BA8  is `reserve__Q24rstl56vector<19SScanHistoryWidgets,Q24rstl17rmemory_allocator>Fi`
//                (0x80116954, 0xB8, in `src/MetroidPrime/Player/CScanDisplay.cpp`, `Matching`).
//                46 instructions each, and **exactly two words differ**: the `bl` at 0x801C2BD8 and
//                the `bl` at 0x801C2C38, which reach `allocate__Q24rstl17rmemory_allocatorFi` and
//                `Free__7CMemoryFPCv` from *this* copy and the same two symbols from the twin's
//                address - the same callees, a different displacement field.  The third `bl`, to
//                the out-of-line copy helper, is byte-identical by coincidence: retail's
//                `fn_801C2C60` and the twin's `uninitialized_copy` are each the *next* function
//                after the caller, 0x50 away.  Everything else, including the four stores that
//                materialise the two by-value iterators (0x801C2BF8-0x801C2C0C against
//                0x801169A4-0x801169B8), is the same word.
//   fn_801C2C60  is `uninitialized_copy<Q24rstl132pointer_iterator<19SScanHistoryWidgets,...>,
//                P19SScanHistoryWidgets>` (0x80116A0C, 0x5C, same unit and file): 23 instructions
//                each and **not one word differs**, down to the per-element null test and the
//                six-word copy.
//
// **What the two are, read off the twin's own source** (`include/rstl/vector.hpp:166-179` and
// `include/rstl/construct.hpp:116-127`):
//
//   void vector<T,Alloc>::reserve(int newSize) {
//     if (newSize <= mCapacity) return;
//     T* newData;  mAllocator.allocate(newData, newSize);
//     uninitialized_copy(begin(), end(), newData);
//     destroy(mItems, mItems + mCount);
//     mAllocator.deallocate(mItems);
//     mItems = newData;  mCapacity = newSize;
//   }
//
// The three pieces of that are visible in retail's bytes: the **signed** `cmpw` against `+0x8`
// (the capacity) and its early return (0x801C2BCC-0x801C2BD0), `mulli` by the element size
// (0x801C2BD4), the out-of-line `uninitialized_copy` at 0x801C2C10, the element walk at
// 0x801C2C28-0x801C2C34, the `bl Free__7CMemoryFPCv` at 0x801C2C38 (that is
// `rmemory_allocator::deallocate`, `include/rstl/rmemory_allocator.hpp:44-52`) and the two stores
// back into `self` (0x801C2C3C-0x801C2C40).
//
// **The element is 0x18 = 24 bytes and `SScanHistoryWidgets` is it**, both measured: the stride
// literal is `0x18` in three places here (0x801C2BD4, 0x801C2CA4, 0x801C2C2C) and
// `CHECK_SIZEOF(SScanHistoryWidgets, 0x18)` is `include/MetroidPrime/HUD/CScanHistory.hpp:49` -
// six pointers, which is also the six `lwz`/`stw` pairs `fn_801C2C60` copies per element.  The
// copy is `construct(tmp, *cur)` reaching the **primary** `rstl::construct_impl` (placement new),
// not the trivially-constructible specialisations of `construct.hpp`, and that is what the
// per-element `cmplwi r5,0 / beq` at 0x801C2C6C-0x801C2C70 is: a null-destination test the
// specialisations do not emit.
//
// **The destroy walk has no body and that is retail's, not an omission.**  `destroy(mItems,
// mItems + mCount)` is `construct.hpp:101-109`, which returns early for a trivially destructible
// `T` and otherwise walks `destroy(&*cur)`; `SScanHistoryWidgets` is a bare struct of six pointers
// with no destructor, so the walk survives with an empty body - 0x801C2C28-0x801C2C34 is the
// whole of it, four instructions, and `fn_801C2C60`'s caller keeps its own copy of the same walk
// shape at 0x801C2C24.
//
// **Both iterators are parameters by value, and that is load-bearing**, the same point
// `Carve801FF5A0.cpp:47-55` and `Carve801C2D74.cpp:43-50` make: a class goes by hidden pointer, so
// the callee reads them out of the caller's frame (`lwz r6,0x0(r3)` / `lwz r0,0x0(r4)`,
// 0x801C2C60-0x801C2C64) and the caller builds both on its own stack (`addi r3,r1,0x14 / addi
// r4,r1,0xc`, 0x801C2BE8/0x801C2BF4).  The 8-byte slot per argument that holds the pointer
// **twice** is the same in both this unit and the twin - and `end`'s slot is the *low* one
// (0x8/0xc) while `begin`'s is the high one (0x10/0x14), with `end` written first.
//
// **Retail re-reads `mItems` and `mCount` from `self` after the copy** (0x801C2C14-0x801C2C20),
// which is what the `destroy` walk's bound needs and is also why the walk cannot be hoisted above
// the call: the compiler has no reason to know the call left `self` alone.  Nothing here holds
// either in a local across it.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `python3 tools/check_decl_order.py --unit Carve801C2BA8` is
// the cheap check (the argument is matched against the report's unit names, so the extension does
// not go with it: `--unit MetroidPrime/Carve801C2BA8.cpp` checks nothing and still prints `ok`).
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so every definition is `extern "C"` - a C++ definition without
// it would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  **The unit is a `.cpp`
// and not a `.c` anyway**, and not for a choice: the two by-value iterator arguments are what the
// bytes are made of (see `SCarveIter` below), and mwcceppc's C mode cannot build them - measured,
// the C spelling of `fn_801C2BA8` is 45 instructions with the argument slots interleaved and one
// read of `x0c_items` instead of retail's two.  This is `Carve801FF5A0.cpp:75-78`'s arrangement
// and its reason: `Carve801C2D74.cpp`, `Carve801FF5A0.cpp` and a dozen other `Carve*.cpp` units are
// `extern "C"` for the same reason, and `powerpc-eabi-nm` on this object shows two
// `T fn_801C2<addr>` and two `U`, which is the thing the `extern "C"` protects.
//
// Its own unit because a claim may not span an unclaimed gap.  This run sits inside the
// 0x801C13F8..0x801C2D74 hole that dtk covers with `auto_03_801C13F8_text`, so carving splits that
// object into 0x801C13F8..0x801C2BA8, this one, and 0x801C2CBC..0x801C2D74.  Below the claim
// `fn_801C2B5C` (0x801C2B5C, `symbols.txt:7318`) ends at 0x801C2BA8 and stays retail's; above it
// `fn_801C2CBC` (0x801C2CBC, 0xB8) begins - the same `reserve` shape again, one element type
// over, and unclaimed - and the next claimed range is `MetroidPrime/Carve801C2D74.cpp` at
// 0x801C2D74.  The directory is retail's own, taken from the nearest claimed range: below is
// `MetroidPrime/Carve801C13F4.c` (0x801C13F4..0x801C13F8) and above is
// `MetroidPrime/Carve801C2D74.cpp`.  For an anonymous function that is the only evidence there is,
// and it beats a lane picking the directory it happened to own.

/** `rstl::rmemory_allocator::allocate(int)` and `rstl::rmemory_allocator::deallocate(T*)`, by
 *  their mangled names rather than by including `include/rstl/rmemory_allocator.hpp`: the header
 *  spells the first as a template over `T` and would bring the second in as an inline `delete[]`
 *  rather than the `bl Free__7CMemoryFPCv` these bytes carry.  Both names are retail's own
 *  (`bl` at 0x801C2BD8 and 0x801C2C38) and both are defined elsewhere - in the port by
 *  `stub_179` in `src/MetroidPrime/PortLinkStubs.cpp` for the allocator, which is the trade
 *  `PortLinkStubs.cpp:864-888` records - so this unit defines neither. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** One pointer of the block's element iterator, `current`.  What these bytes need of the class is
 *  exactly this: **one word, a converting constructor and nothing else**, passed by value, and
 *  that is what puts the caller-built temporaries behind `r3` and `r4` in `fn_801C2C60` and makes
 *  the loop read them out of the caller's frame.  `rstl::pointer_iterator` is reduced to that one
 *  member here for the reason `Carve801C2D74.cpp:62-65` gives -
 *  `include/rstl/pointer_iterator.hpp` includes `rstl/construct.hpp`, and this object must not see
 *  that definition.
 *
 *  **The converting constructor is load-bearing, and it is why this file is C++.**  Written as
 *  plain C - measured - `fn_801C2BA8` comes out 45 instructions instead of 46 and the argument
 *  temporaries land *interleaved* (`begin` at `r1+0xc`, `end` at `r1+0x8`, each slot holding one
 *  word of each) rather than in the two contiguous 8-byte slots retail builds at `0x10/0x14` and
 *  `0x8/0xc`, and C mode common-subexpressions the two reads of `x0c_items` into one `lwz r6`,
 *  where retail reads it twice (0x801C2BE4 and 0x801C2C00).  The class has no default constructor
 *  on purpose: it is what makes mwcceppc build the temporary and then copy it into the argument
 *  slot, and the two writes are the two `stw`s of one pointer retail has per slot.  This is
 *  `Carve801FF5A0.cpp:47-78`'s finding, on the same shape and the same 8-byte slots, and its twin
 *  measured the same two C-mode failures (42 instructions instead of 43, interleaved slots). */
struct SCarveIter {
  void* mCur;

  SCarveIter(void* cur) : mCur(cur) {}
};

/** `SScanHistoryWidgets` as far as these 276 bytes need it, and no further: the six pointers of
 *  `include/MetroidPrime/HUD/CScanHistory.hpp:32-48`, in retail's own names and retail's own order,
 *  whose `CHECK_SIZEOF` (`:49`) is the `0x18` stride and whose memberwise copy is the six
 *  `lwz`/`stw` pairs `fn_801C2C60` emits.  So `*out = *in` is the copy, spelled the way retail's
 *  `construct_impl` reaches it.  Nothing is asserted here about what the six pointers point at -
 *  only their number, their order and their size.
 *
 *  **The aggregate assignment is exact in C++ and not in C** - measured both ways: with `-lang=c++`
 *  `*out = *in` gives all 23 instructions, and with `-lang=c` mwceppc reorders the same source into
 *  two loads then two stores (9 of 23 instructions differ, `r4` and `r3` alternating instead of
 *  `r3` alone).  The six stores are individually written out in neither file; see
 *  `SCarveIter` below for why this one is C++. */
struct SCarveElem {
  void* mRoot;
  void* mHistory;
  void* mNumber;
  void* mPercent;
  void* mFlash;
  void* mDouble;
};

/** The block, as far as these bytes read it: `+0x4` the element count and `+0x8` the capacity,
 *  because the test at 0x801C2BCC is signed against `+0x8` and the copy's bound is
 *  `x0c + x04 * 0x18`; `+0xc` the base pointer.  Nothing reads `+0`, so nothing is asserted about
 *  it.  These are the same three words at the same offsets as `rstl::vector`'s own
 *  `mCount`/`mCapacity`/`mItems` (`include/rstl/vector.hpp`, private section), which is the
 *  `vector<SScanHistoryWidgets, rmemory_allocator>` the twin's symbol names. */
struct SCarveBlock {
  unsigned int x00_allocator;
  unsigned int x04_count;
  unsigned int x08_capacity;
  void* x0c_items;
};

/** 0x801C2C60, 92 bytes: `rstl::uninitialized_copy< It, T* >` - the loop
 *  `include/rstl/construct.hpp:116-127` writes, with the destination's null test the primary
 *  `construct_impl` emits.  Returns the new end, which `fn_801C2BA8` ignores. */
extern "C" void* fn_801C2C60(SCarveIter begin, SCarveIter end, void* dst);

/** 0x801C2BA8, 184 bytes: the block's `reserve`. */
extern "C" void fn_801C2BA8(SCarveBlock* self, int capacity);

extern "C" void* fn_801C2C60(SCarveIter begin, SCarveIter end, void* dst) {
  SCarveElem* in = static_cast< SCarveElem* >(begin.mCur);
  SCarveElem* out = static_cast< SCarveElem* >(dst);
  for (; in != static_cast< SCarveElem* >(end.mCur); ++in, ++out) {
    if (out != 0) {
      *out = *in;
    }
  }
  return out;
}

extern "C" void fn_801C2BA8(SCarveBlock* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08_capacity)) {
    return;
  }
  SCarveElem* const buffer =
      static_cast< SCarveElem* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 24));
  fn_801C2C60(SCarveIter(self->x0c_items),
              SCarveIter(static_cast< SCarveElem* >(self->x0c_items) + self->x04_count), buffer);
  /** `destroy(mItems, mItems + mCount)`.  **The walk is load-bearing and so is the empty body**:
   *  `SScanHistoryWidgets` is a struct of six pointers with no destructor, so
   *  `rstl::destroy_impl`'s loop (`include/rstl/construct.hpp:101-107`) survives with nothing in
   *  it - 0x801C2C28-0x801C2C34 is the whole of it, four instructions.  Both ends are re-read
   *  from `self` rather than carried across the `uninitialized_copy`, because a call may leave
   *  memory alone and the compiler has no reason to think otherwise. */
  SCarveElem* elem = static_cast< SCarveElem* >(self->x0c_items);
  SCarveElem* const limit = elem + self->x04_count;
  while (elem != limit) {
    ++elem;
  }
  Free__7CMemoryFPCv(self->x0c_items);
  self->x0c_items = buffer;
  self->x08_capacity = static_cast< unsigned int >(capacity);
}
