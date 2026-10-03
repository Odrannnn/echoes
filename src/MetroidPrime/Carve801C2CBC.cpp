// Carved out of dtk's unclaimed `auto_03_801C2CBC_text`.  Every number here is measured: the
// address and size come from `config/G2ME01/symbols.txt:7321`, the 46 instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_801C2CBC_text.s` before this claim existed
// (the same range is now `build/G2ME01/asm/MetroidPrime/Carve801C2CBC.s`), and the body below is
// the C++ those bytes are the compilation of.  `tools/flip_test.sh` is what says so.
//
// .text 0x801C2CBC..0x801C2D74, 0xB8 = 184 bytes, 1 function:
//
//   fn_801C2CBC    0x801C2CBC  0xB8  46 instructions  a `reserve`, the same one as the two below
//
// **It is a byte-shape twin of `fn_801C2BA8`, which is already `Matching` in this tree
// (`src/MetroidPrime/Carve801C2BA8.cpp:197-219`), and the comparison was made instruction by
// instruction rather than assumed.**  46 instructions each, and **exactly seven words differ**, in
// four kinds that are all accounted for below: the three `mulli`/`addi` strides, the two `bl`
// displacements into `allocate` and `Free`, and the callee of the copy - `fn_801C2D74` here
// against `fn_801C2C60` there, which is retail's out-of-line copy sitting 0x50 away in both.
//
//   idx  retail                                        ours
//   11  801C2BD4  1C7E0018  mulli r3, r30, 0x18       801C2CE8  1C7E0044  mulli r3, r30, 0x44
//   12  801C2BD8  4813AEE1  bl allocate...           801C2CEC  4813ADCD  bl allocate...   (same callee)
//   17  801C2BEC  1C000018  mulli r0, r0, 0x18        801C2D00  1C000044  mulli r0, r0, 0x44
//   26  801C2C10  48000051  bl fn_801C2C60            801C2D24  48000051  bl fn_801C2D74
//   29  801C2C1C  1C000018  mulli r0, r0, 0x18        801C2D30  1C000044  mulli r0, r0, 0x44
//   33  801C2C2C  38840018  addi r4, r4, 0x18        801C2D3C  38840044  addi r4, r4, 0x44
//   36  801C2C38  4810B751  bl Free__7CMemoryFPCv    801C2D4C  4810B63D  bl Free__7CMemoryFPCv
//
// **So this is the same `rstl::vector<T,Alloc>::reserve` one element type over**
// (`include/rstl/vector.hpp:166-179`, read out of the twin's own file at
// `src/MetroidPrime/Player/CScanDisplay.cpp`), and the element is `CRagDoll::CRagDollParticle`:
// the stride is the literal `0x44` in four places here, and
// `NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)` is `include/MetroidPrime/CRagDoll.hpp:197`.
// The copy callee settles it independently and exactly: `fn_801C2D74` is
// `rstl::uninitialized_copy<pointer_iterator<CRagDoll::CRagDollParticle,...>,
// CRagDoll::CRagDollParticle*>` (`src/MetroidPrime/Carve801C2D74.cpp`, `Matching`, `symbols.txt:7322`),
// it steps `addi ..,0x44` too, and it is retail's only caller on this side - `grep -rn 'bl
// fn_801C2D74' build/G2ME01/asm/` returns this instruction and no other.
//
// The three pieces of `reserve` are all visible in the bytes: the **signed** `cmpw` against `+0x8`
// (the capacity) and its early return (0x801C2CE0-0x801C2CE4), the `mulli` by the element size
// (0x801C2CE8), the out-of-line copy at 0x801C2D24, the element walk at
// 0x801C2D28-0x801C2D48, `bl Free__7CMemoryFPCv` at 0x801C2D4C (that is
// `rmemory_allocator::deallocate`, `include/rstl/rmemory_allocator.hpp:44-52`) and the two stores
// back into `self` (0x801C2D50-0x801C2D54).
//
// **The destroy walk has no body and that is retail's, not an omission**, the same point
// `Carve801C2BA8.cpp:205-215` makes for the twin: `destroy(mItems, mItems + mCount)` is
// `construct.hpp:101-109`, which keeps its loop for a class that is not trivially destructible, and
// `CRagDoll::CRagDollParticle` is a struct of scalars with no destructor to call, so
// 0x801C2D28-0x801C2D48 is the whole of it.
//
// **Both iterators are parameters by value, and that is load-bearing** - the same point
// `Carve801C2BA8.cpp:68-74` and `Carve801C2D74.cpp:43-50` make.  A class goes by value behind a
// hidden pointer, so the caller builds both temporaries on its own stack (`addi r3,r1,0x14 /
// addi r4,r1,0xc`, 0x801C2CFC/0x801C2D08) in two contiguous 8-byte slots at `0x10/0x14` and
// `0x8/0xc`, `end` written first, and each slot holds its pointer twice.  The converting
// constructor below is what makes mwcceppc build the temporary and then copy it into the slot;
// measured, the plain-C spelling of the same bodies interleaves the slots and loses an
// instruction (`Carve801C2BA8.cpp:128-137`), which is why this unit is a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `python3 tools/check_decl_order.py --unit Carve801C2CBC` is
// the cheap check (the argument is matched as a substring of the report's unit names, so the
// extension does not go with it: `--unit MetroidPrime/Carve801C2CBC.cpp` checks nothing and still
// prints `ok`).
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition is `extern "C"` - a C++ definition without it
// would mangle to `_Z<len>fn_801C2CBCv` and objdiff would pair nothing.  That is also why the unit
// is a `.cpp` rather than a `.c`, not for a choice: the two by-value iterator arguments are what
// the bytes are made of, and mwcceppc's C mode cannot build them.
//
// Its own unit because a claim may not span an unclaimed gap.  Carving this 0xB8 removes
// `auto_03_801C2CBC_text` outright rather than splitting it - 0x801C2CBC..0x801C2D74 was all of it,
// which `config/G2ME01/splits.txt` now says in place of the auto object.  Below the claim
// `MetroidPrime/Carve801C2BA8.cpp` ends at 0x801C2CBC; above it `MetroidPrime/Carve801C2D74.cpp`
// begins at 0x801C2D74.  The directory is retail's own, taken from the nearest claimed range on
// both sides, and for an anonymous function that is the only evidence there is.
//
// The body is inside `#ifdef __MWERKS__` for the reason `Carve801C2D74.cpp:74-79` and its
// `files.cmake` entry give: `fn_801C2D74` - this function's only other callee - is itself defined
// only in the DOL branch of that file, so a host compilation here would add one undefined symbol
// to the port's link for a function no host source calls.  The host branch is empty by design; the
// DOL branch is the whole file.

#ifdef __MWERKS__

/** `rstl::rmemory_allocator::allocate(int)` and `rstl::rmemory_allocator::deallocate(T*)`, by
 *  their mangled names rather than by including `include/rstl/rmemory_allocator.hpp`: the header
 *  spells the first as a template over `T` and would bring the second in as an inline `delete[]`
 *  rather than the `bl Free__7CMemoryFPCv` these bytes carry.  Both names are retail's own (`bl`
 *  at 0x801C2CEC and 0x801C2D4C) and both are defined elsewhere - in the port by `stub_179` in
 *  `src/MetroidPrime/PortLinkStubs.cpp:864-888` for the allocator, which is the trade those lines
 *  record - so this unit defines neither. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** One pointer of the block's element iterator, `current`.  What these bytes need of the class is
 *  exactly this: **one word, a converting constructor and nothing else**, passed by value.
 *  `rstl::pointer_iterator` is reduced to that one member here for the reason
 *  `Carve801C2D74.cpp:62-65` gives - `include/rstl/pointer_iterator.hpp` includes
 *  `rstl/construct.hpp`, and nothing in this translation unit may see `construct_impl`'s inline
 *  definition.
 *
 *  **The converting constructor is load-bearing, and it is why this file is C++.**  Written as
 *  plain C - measured on the twin, `Carve801C2BA8.cpp:128-137` - the argument temporaries land
 *  *interleaved* (`begin` at `r1+0xc`, `end` at `r1+0x8`, each slot holding one word of each)
 *  rather than in the two contiguous 8-byte slots retail builds at `0x10/0x14` and `0x8/0xc`, one
 *  instruction is lost, and the two reads of the base pointer common-subexpress into one `lwz`
 *  where retail reads it twice (0x801C2CF8 and 0x801C2D14).  The class has no default constructor
 *  on purpose: that is what makes mwcceppc build the temporary and then copy it into the argument
 *  slot, and the two writes are the two `stw`s of one pointer retail has per slot. */
struct SCarveIter {
  void* mCur;

  SCarveIter(void* cur) : mCur(cur) {}
};

/** `CRagDoll::CRagDollParticle` as far as these 184 bytes need it, and no further.  The real class
 *  is `include/MetroidPrime/CRagDoll.hpp:44-77` and this range never reads a member of it: the
 *  walk steps a pointer by `sizeof` and nothing else, and the element's copy is retail's own
 *  `fn_801C2D74` in another unit.  **What is reproduced here is the size**, because the four
 *  `0x44` strides (0x801C2CE8, 0x801C2D00, 0x801C2D30, 0x801C2D3C) are
 *  `NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)` (`CRagDoll.hpp:197`) and the callee's
 *  name spells `Q28CRagDoll16CRagDollParticle`.  Nothing here claims anything about the layout
 *  below that size. */
struct SCarveElem {
  unsigned char x00_payload[0x44];
};

/** The block, as far as these bytes read it: `+0x4` the element count and `+0x8` the capacity,
 *  because the test at 0x801C2CE0 is signed against `+0x8` and the copy's bound is
 *  `x0c + x04 * 0x44`; `+0xc` the base pointer.  Nothing reads `+0`, so nothing is asserted about
 *  it.  These are the same three words at the same offsets as `rstl::vector`'s own
 *  `mCount`/`mCapacity`/`mItems` (`include/rstl/vector.hpp`, private section), which is the
 *  `vector<CRagDoll::CRagDollParticle, rmemory_allocator>` the callee's mangled type spells. */
struct SCarveBlock {
  unsigned int x00_allocator;
  unsigned int x04_count;
  unsigned int x08_capacity;
  void* x0c_items;
};

/** 0x801C2D74, 0x68 bytes, defined and `Matching` in `src/MetroidPrime/Carve801C2D74.cpp`: the
 *  `rstl::uninitialized_copy< It, T* >` of `include/rstl/construct.hpp:116-127` over this element,
 *  taking its two cursors by value and returning the new end cursor.  **The declaration is the
 *  load-bearing part of the call**: two by-value class arguments is what puts the two
 *  caller-built 8-byte slots behind `r3`/`r4` and makes retail write `end` into the low slot
 *  first.  Declared `extern "C"`, so this is retail's own symbol name and nothing here needs its
 *  template arguments; the return value is discarded, which is what the caller does. */
extern "C" void* fn_801C2D74(SCarveIter begin, SCarveIter end, SCarveElem* dst);

/** 0x801C2CBC, 184 bytes: the block's `reserve`. */
extern "C" void fn_801C2CBC(SCarveBlock* self, int capacity);

extern "C" void fn_801C2CBC(SCarveBlock* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08_capacity)) {
    return;
  }
  SCarveElem* const buffer =
      static_cast< SCarveElem* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 68));
  fn_801C2D74(SCarveIter(self->x0c_items),
              SCarveIter(static_cast< SCarveElem* >(self->x0c_items) + self->x04_count), buffer);
  /** `destroy(mItems, mItems + mCount)`.  **The walk is load-bearing and so is the empty body**:
   *  `CRagDoll::CRagDollParticle` is a struct of scalars with no destructor, so
   *  `rstl::destroy_impl`'s loop (`include/rstl/construct.hpp:101-107`) survives with nothing in
   *  it - 0x801C2D28-0x801C2D48 is the whole of it.  Both ends are re-read from `self` rather
   *  than carried across the copy, because a call may leave memory alone and the compiler has no
   *  reason to think otherwise. */
  SCarveElem* elem = static_cast< SCarveElem* >(self->x0c_items);
  SCarveElem* const limit = elem + self->x04_count;
  while (elem != limit) {
    ++elem;
  }
  Free__7CMemoryFPCv(self->x0c_items);
  self->x0c_items = buffer;
  self->x08_capacity = static_cast< unsigned int >(capacity);
}

#endif // __MWERKS__