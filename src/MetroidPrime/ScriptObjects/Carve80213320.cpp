// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt` (lines 8602-8604), the
// instructions are the ones dtk emitted into `build/G2ME01/asm/auto_03_80212A2C_text.s` before
// the claim existed, and are still readable with `build/binutils/powerpc-eabi-objdump -d
// --start-address=0x80213320 --stop-address=0x8021348C build/G2ME01/main.elf`.  The body below
// is the C++ those bytes are the compilation of, and `tools/flip_test.sh` is what says so.
//
// .text 0x80213320..0x8021348C, 0x16C = 364 bytes, 3 functions:
//
//   fn_80213320    0x80213320  0xBC  47 instructions   the 8-byte-element block's `reserve`
//   fn_802133DC    0x802133DC  0x4C  19 instructions   uninitialized_copy(begin, end, dst)
//   fn_80213428    0x80213428  0x64  25 instructions   the element's ReleaseData
//
// **What the three are: one `rstl::vector` of 8-byte refcounted pointers, reallocated.**  The
// element stride is in the bytes twice (`slwi r3,r28,3` on the allocation size at 0x80213344,
// `slwi r0,r0,3` on the count at 0x8021335C); `fn_802133DC` copies two words and bumps the
// second word's pointee (`lwz r4,4(r5)` / `lwz r3,0(r4)` / `addi r3,r3,1` / `stw r3,0(r4)`), and
// `fn_80213428` is `if (--*mRefCount <= 0) { delete mPtr; delete mRefCount; }` with the
// **vtable** `delete` (`lwz r12,0(r3)` / `li r4,1` / `lwz r12,8(r12)` / `mtctr r12` / `bctrl`,
// 0x8021345C-0x8021346C), so the pointee is polymorphic.  That is `rstl::rc_ptr<T>`, and the
// destroy loop's callee is this copy's own `fn_8020D278` (`symbols.txt:8466`), not the weak
// `ReleaseData__Q24rstl23rc_ptr<...>Fv` some other instantiation emits.
//
// Retail names none of these, so each is byte-for-byte the shape of a symbol retail *does*
// name, elsewhere in this same DOL:
//
//   fn_80213320  twin of `reserve__Q24rstl69vector<Q24rstl25ncrc_ptr<13CAnimTreeNode>,
//                Q24rstl17rmemory_allocator>Fi` (0x8029A238, Matching in
//                `src/Kyoto/Animation/CSequenceHelper.cpp`) - 47 instructions against 47,
//                differing in **3 words, all `bl`**: `allocate__Q24rstl17rmemory_allocatorFi` and
//                `Free__7CMemoryFPCv` are the same absolute targets this copy calls, at a
//                different displacement, and the third is the destroy loop's callee
//                (`fn_8020D278` here, `ReleaseData__Q24rstl23rc_ptr<13CAnimTreeNode>Fv` there).
//                The `bl fn_802133DC` word is identical because both copies call the function
//                that follows them.
//   fn_802133DC  twin of `uninitialized_copy<Q24rstl144pointer_iterator<Q24rstl18rc_ptr<
//                9IMetaAnim>,...>,PQ24rstl18rc_ptr<9IMetaAnim>>` (0x8006D384, Matching in
//                `src/MetroidPrime/CAnimationDatabaseGame.cpp`) - 19 against 19, differing in
//                **0** words.
//   fn_80213428  twin of `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` (0x80008FA4, Matching in
//                `src/MetroidPrime/main.cpp`) - 25 against 25, differing only in the `bl Free`.
//
// **The unit is C++ even though retail names none of its functions, and `fn_80213320` is why.**
// Retail's two iterator arguments to `fn_802133DC` arrive **by address** and the caller
// materialises a two-word temporary per argument:
//
//   80213354  lwz  r4,12(r27)     ; begin
//   80213358  addi r3,r1,20       ; &begin
//   80213364  add  r6,r4,r0       ; end
//   80213368  addi r4,r1,12       ; &end
//   8021336c  stw  r6,12(r1)      ; end
//   80213370  lwz  r0,12(r27)     ; begin again (not CSEd across the store)
//   80213374  stw  r6,8(r1)       ; end
//   80213378  stw  r0,16(r1)      ; begin
//   8021337c  stw  r0,20(r1)      ; begin
//   80213380  bl   fn_802133DC
//
// MWCC passes a one-word class with a user-defined constructor by address, which is what
// produces those two words and the reload; mwcceppc's C front end has no class type to ask for
// it.  `Carve801FF5A0.cpp` in this same directory is the `.c`/`.cpp` sibling of this exact
// shape and records the same ABI (two 8-byte argument slots, `addi r3,r1,0x14` / `addi r4,r1,0xc`).
//
// The definitions are `extern "C"` so the `fn_<addr>` names stay unmangled: `symbols.txt`
// carries the placeholder and objdiff pairs by name, so a C++ definition without it would
// mangle to `_Z<len>fn_<addr>v` and pair nothing.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are not trivial: `fn_802132C0` (0x802132C0, 0x60) sits directly below
// the claim and `fn_8021348C` (0x8021348C, 0x1C) directly above it.
//
// The directory is retail's own, taken from the nearest claimed units: `ScriptObjects/` is where
// both of them live - `ScriptObjects/Carve80212A24.c` is 0x8FC below the claim and the `auto_*` run
// this range is cut out of (`auto_03_80212A2C_text`) begins right after it, and
// `MetroidPrime/CRelFile.cpp` starts 0x1C4 above the claim's end (0x80213650).  For an anonymous
// function that is the only evidence there is, and it beats a lane picking the directory it happened
// to own.

#include "rstl/construct.hpp"

/** `rstl::rmemory_allocator::allocate` and `CMemory::Free`, taken by their mangled names rather
 *  than by including the headers: this object must define exactly the three functions above and
 *  nothing else, and either header would bring an inline copy of the callee in with it.  Both
 *  addresses are retail's own (`symbols.txt`, and the `bl` at 0x80213348 / 0x802133BC). */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x8020D278, 0x64 bytes: this copy's own out-of-line `ReleaseData` - the destroy loop calls it
 *  (`bl` at 0x802133A8) rather than the weak `ReleaseData__Q24rstl23rc_ptr<...>Fv` the header
 *  would emit for its own instantiation.  It is retail code in another unit's range and stays
 *  undefined here; the port links it through `PortLinkStubs.cpp`. */
extern "C" void fn_8020D278(void* self);

/** The polymorphic pointee, as far as the `delete` needs it: a vtable at +0 and the deleting
 *  destructor at +8, nothing else.  Declared and not defined, because the call is virtual - the
 *  vtable comes from the object, and this unit must not define a destructor it was not asked
 *  for. */
class SCarve80213320Poly {
public:
  virtual ~SCarve80213320Poly();
};

/** One refcounted pointer: `rstl::rc_ptr<T>`'s two words at +0 and +4.  A same-layout view
 *  rather than the class, because this unit needs the offsets and not `rstl/rc_ptr.hpp`'s
 *  `CRefData` control block. */
struct SCarve80213320Pair {
  void* x0_ptr;
  int* x4_refCount;
};

/** The element's `ReleaseData`: `if (--*mRefCount <= 0) { delete mPtr; delete mRefCount; }`.
 *  The `delete` is the null-guarded virtual one (`cmplwi r3,0` at 0x80213454) and the refcount
 *  word is freed through `CMemory::Free` (`bl` at 0x80213474), which is what `delete (int*)`
 *  compiles to. */
extern "C" void fn_80213428(SCarve80213320Pair* self) {
  if (--*self->x4_refCount <= 0) {
    delete static_cast< SCarve80213320Poly* >(self->x0_ptr);
    delete self->x4_refCount;
  }
}

/** The element, as far as this unit reads it: `rstl::rc_ptr<T>`'s two words, a copy
 *  constructor that adds a reference, and a destructor that releases it through this copy's own
 *  `fn_8020D278`.  Retail's element is a `ncrc_ptr`-shaped derived class - `rc_ptr`'s two words
 *  under a class with no destructor of its own.
 *
 *  **The derived type is what makes the destroy loop's second null test.**
 *  `fn_80213320`'s loop is `cmplwi r30,0` / `beq` / `beq` / `mr r3,r30` / `bl` (0x80213398) -
 *  two branches on one comparison - and MWCC emits that only for the implicit destructor of a
 *  derived class: the same loop over `SCarve80213320ElemBase` alone is one `beq`, measured in
 *  scratch compiles of the two spellings.
 *
 *  **The copy side is spelled over the base on purpose.**  Copy-constructing the *derived* type
 *  (`construct(tmp, *cur)` in `fn_802133DC`, or the same `new (dest) Elem(src)` by hand) makes
 *  MWCC also emit a weak out-of-line copy of `~SCarve80213320ElemBase` - 0x50 bytes in this
 *  object's `.text`, measured, and retail's range has no such function.  Over the base the copy
 *  is the same nine instructions and nothing else is emitted.  `powerpc-eabi-nm` on the built
 *  object is the check: three `T fn_80213<addr>` and three `U`, nothing else. */
struct SCarve80213320ElemBase {
  void* x0_ptr;
  int* x4_refCount;

  SCarve80213320ElemBase(const SCarve80213320ElemBase& other)
  : x0_ptr(other.x0_ptr), x4_refCount(other.x4_refCount) {
    ++*x4_refCount;
  }
  ~SCarve80213320ElemBase() { fn_8020D278(this); }
};

struct SCarve80213320Elem : SCarve80213320ElemBase {};

/** The one-pointer iterator `uninitialized_copy` takes **by address**, which is what makes
 *  `fn_80213320` build its two-word temporaries.  What the bytes need of the class is exactly
 *  this: one pointer and a converting constructor. */
struct SCarve80213320Iter {
  SCarve80213320ElemBase* mCur;
  SCarve80213320Iter(SCarve80213320ElemBase* cur) : mCur(cur) {}
  SCarve80213320Iter& operator++() {
    mCur += 1;
    return *this;
  }
  SCarve80213320ElemBase& operator*() const { return *mCur; }
  bool operator!=(const SCarve80213320Iter& other) const { return mCur != other.mCur; }
};

extern "C" SCarve80213320ElemBase* fn_802133DC(SCarve80213320Iter begin,
                                               SCarve80213320Iter end,
                                               SCarve80213320ElemBase* out);

/** The block, as far as these functions read it: `x04` is the live count, `x08` the capacity
 *  `fn_80213320` compares against with a **signed** `cmpw` and writes the requested capacity
 *  back into, `x0c` the base pointer, and `x00` is never read - so nothing is asserted about
 *  it.  Same four words at the same offsets as `rstl::vector`'s (`include/rstl/vector.hpp`),
 *  whose `reserve` this is. */
struct SCarve80213320Block {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  SCarve80213320Elem* x0c;
};

/** `rstl::uninitialized_copy`: one 8-byte-strided walk that copy-constructs each element and
 *  returns the new end, which is what `fn_80213320` leaves in r3.  The placement new's null
 *  test is `construct`'s (`cmplwi r5,0` / `beq`, 0x802133E8-0x802133EC), and the element it
 *  copies is the base - see the note on the element types above. */
extern "C" SCarve80213320ElemBase* fn_802133DC(SCarve80213320Iter begin,
                                               SCarve80213320Iter end,
                                               SCarve80213320ElemBase* out) {
  SCarve80213320ElemBase* tmp = out;
  SCarve80213320Iter cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    rstl::construct(tmp, *cur);
  }
  return tmp;
}

/** The block's `reserve`, the header's `vector<T,Alloc>::reserve` with this copy's callees: a
 *  capacity that is not larger returns without allocating, otherwise it allocates
 *  `newSize * 8`, moves the live elements across with `fn_802133DC`, destroys the old ones
 *  with the inline `rstl::destroy` loop, frees the old buffer and stores the new buffer and
 *  capacity back. */
extern "C" void fn_80213320(SCarve80213320Block* self, int newSize) {
  if (newSize <= static_cast< int >(self->x08)) {
    return;
  }
  SCarve80213320Elem* const newData = static_cast< SCarve80213320Elem* >(
      allocate__Q24rstl17rmemory_allocatorFi(newSize * 8));
  fn_802133DC(SCarve80213320Iter(self->x0c), SCarve80213320Iter(self->x0c + self->x04), newData);
  rstl::destroy(self->x0c, self->x0c + self->x04);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = newData;
  self->x08 = static_cast< unsigned int >(newSize);
}
