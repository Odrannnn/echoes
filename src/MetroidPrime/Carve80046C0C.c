// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:1319-1320`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80045CDC_text.s:1141-1232` while this range was
// still unclaimed, and the body below is the C those bytes are the compilation of.
//
// .text 0x80046C0C..0x80046D44, 0x138 = 312 bytes, 2 functions:
//
//   fn_80046C0C    0x80046C0C  0xC0    48 instructions
//   fn_80046CCC    0x80046CCC  0x78    30 instructions
//
// **What the two are: a `rstl::vector::reserve(int)` for a 12-byte element and the
// `rstl::uninitialized_copy` it calls.**  Retail names neither of them, so this is read off the
// call edges and the argument registers, and it is confirmed instruction for instruction against
// two already-matched instantiations of the *same two* `include/rstl` templates:
//
//   fn_80046C0C  `include/rstl/vector.hpp:167-179` is
//                `reserve(newSize) { if (newSize <= mCapacity) return; T* newData;
//                mAllocator.allocate(newData, newSize); uninitialized_copy(begin(), end(),
//                newData); destroy(mItems, mItems + mCount); mAllocator.deallocate(mItems);
//                mItems = newData; mCapacity = newSize; }`, and those 48 instructions are the
//                *same instructions, in the same order*, as the matched
//                `reserve__Q24rstl65vector<28TCachedToken<12CStringTable>,
//                Q24rstl17rmemory_allocator>Fi` at 0x80116A68 (0xC0, `symbols.txt:4825`,
//                `MetroidPrime/Player/CScanDisplay.cpp` claims 0x801123F8..0x80116C74) with only
//                its four `bl` displacements different.
//   fn_80046CCC  `include/rstl/construct.hpp:117-127` is `uninitialized_copy(It begin, It end,
//                T out) { T tmp = out; It cur = begin; for (; cur != end; ++cur, ++tmp)
//                construct(tmp, *cur); return tmp; }`, and those 30 instructions are the same as
//                the matched
//                `uninitialized_copy<rstl::pointer_iterator<TCachedToken<CStringTable>,...>,
//                TCachedToken<CStringTable>*>__4rstlF...` at 0x80116B28 (0x78, `symbols.txt:4826`),
//                again with only the one `bl` displacement different.
//
// So this is a different *instantiation* of the same two templates over a different element type,
// which is what makes the claim possible at all: retail gives the pair here the `fn_<addr>`
// placeholders and gives the pair in `CScanDisplay.cpp` its own names, so the two cannot be one
// unit, and neither can be written in C++ here (see "Retail names none of this" below).
//
// The element is 12 bytes and is `TCachedToken< CStringTable >`, which is what fixes both copies:
// `mulli r0,r0,0xc` in `reserve` and `addi r31,r31,0xc` in `uninitialized_copy` are multiplies
// and adds, not shifts.  The per-element work is `bl __ct__6CTokenFRC6CToken` (0x803015B4,
// `symbols.txt:13923`) followed by `lwz r0,0x8(r31) / stw r0,0x8(r30)`, i.e. the implicit copy of
// `TCachedToken`'s own `T* mItem` at +8 on top of `CToken`'s out-of-line copy constructor
// (`include/Kyoto/CToken.hpp` has `CObjectReference* mObjRef` at +0 and `bool mLockHeld` at +4).
// The matching teardown is `bl __dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`) with `li r4,0` -
// the base destructor only, because `~TToken` and `~TCachedToken` are implicit and their only
// member (`T* mItem`) needs no destruction.
//
// **Three spellings in `fn_80046C0C` below are load-bearing, and each was measured by changing
// only that one thing.**  (i) `uninitialized_copy` is called with two by-value iterators built
// from compound literals, not through named locals: that is what produces retail's four home-slot
// stores at `r1+0x08..0x14` and its argument registers `addi r3,r1,0x14` and `addi r4,r1,0x0C`
// (the matched `src/MetroidPrime/Carve800047E0.c` records both spellings).  (ii) `last` is
// `cur + self->mCount`, not `self->mItems + self->mCount`: written the other way retail's two
// instructions become three, `mr r30,r3` appearing between the load and the add.  (iii) The three
// locals are **declared `last`, `cur`, `newData`**, in that order, and the teardown carries the
// **nested** `if (cur) if (cur)`; the declaration order is what puts `newData` in `r29`, `cur` in
// `r30` and `last` in `r31` as retail does (mwcceppc hands out `r29..r31` in declaration order
// here, so declaring `newData` first - the shape `include/rstl/vector.hpp:172` has - swaps
// `newData` and `last` and costs one register), and the second null test is retail's second
// `beq .L_80046C9C` at 0x80046C8C.  One null test instead of two is 4 bytes short.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Carve80046C0C` is the cheap check.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>...` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.  **mwcceppc's C mode is C89**: a declaration after a statement in a block
// is rejected, a `const` local cannot be assigned after its declaration, and it does not convert a
// struct to a struct pointer implicitly - all three cost a build here.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// `Matching` unit's `.text` ends: below is `MetroidPrime/Carve80045CD4.c`
// (0x80045CD4..0x80045CDC), and above, 0x80046D44 is the *named*
// `do_erase__Q24rstl43list<9TUniqueId,Q24rstl17rmemory_allocator>FPQ34rstl43list<9TUniqueId,
// Q24rstl17rmemory_allocator>4node` (`symbols.txt:1321`), which needs its own mangling and a
// `.cpp`; `MetroidPrime/Carve80046DB8.c` then claims 0x80046DB8..0x80046F88.  The claim starts at
// 0x80046C0C and not at the `auto_*` unit's own 0x80045CDC, which is what keeps `dtk dol split`
// from reporting a link-order cycle.  The directory is retail's own, taken from those neighbours:
// this address sits in the `MetroidPrime/` neighbourhood.

/** 0x802FDAB8, `symbols.txt:13824`, size 0x3C: `rstl::rmemory_allocator::allocate(int)` under
 *  **MWCC's own mangling**.  Claimed by `rstl/rstl_misc.cpp` (`.text` 0x802FDAB8..0x802FDAF4), so
 *  our own tree supplies it.  Declared, never defined here.  For a host link it binds to
 *  `stub_179()` in `src/MetroidPrime/PortLinkStubs.cpp`, as it does for the other carves. */
extern void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** 0x802CE388, `symbols.txt:12992`, size 0x64: retail's `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x8030154C, `symbols.txt:13922`, size 0x68: `CToken::~CToken()` under **MWCC's own mangling**.
 *  Claimed by `Kyoto/CToken.cpp` (`.text` 0x803013D0..0x80301710), so our own tree supplies it.
 *  Declared, never defined here.  The 16-bit second argument is MWCC's deleting-destructor flag,
 *  and `include/rstl/construct.hpp:84-90`'s `destroy_impl(T* in)` always passes 0;
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:50` defines it for the host. */
extern void __dt__6CTokenFv(void* self, short deleting);

/** 0x803015B4, `symbols.txt:13923`, size 0x5C: `CToken::CToken(const CToken&)` under **MWCC's own
 *  mangling**, the base of the implicit `TCachedToken` copy that
 *  `include/rstl/construct.hpp:53-56`'s `construct_impl` places.  Claimed by `Kyoto/CToken.cpp`
 *  (`.text` 0x803013D0..0x80301710), so our own tree supplies it.  Declared, never defined here;
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host, next to the other retail-mangled
 *  `CToken` names in that file. */
extern void __ct__6CTokenFRC6CToken(void* self, const void* src);

/** `TCachedToken< CStringTable >` as far as these two bodies read it: `CToken` at +0
 *  (`include/Kyoto/CToken.hpp`: `mObjRef`, `mLockHeld`) and `T* mItem` at +8
 *  (`include/Kyoto/TToken.hpp`), 12 bytes in total.  The `CToken` members are never touched here -
 *  retail copies and destroys them through the two out-of-line calls above - so only their size
 *  is modelled. */
struct SElem80046C0C {
  void* mObjRef; /* +0 */
  int mLockHeld; /* +4 */
  void* mItem;   /* +8 */
};
typedef struct SElem80046C0C SElem80046C0C;

/** One 4-byte `rstl::pointer_iterator< T, Vec, Alloc >` - `include/rstl/pointer_iterator.hpp:62`
 *  derives it from `const_pointer_iterator`, which holds the single `T* current` it inherits and
 *  adds no member, so the iterator is one word - named so the by-value parameter list of
 *  `uninitialized_copy` can be written in C.  MWCC passes a by-value class argument as a pointer
 *  to a caller-side temporary: retail's `lwz r31,0x0(r3)` reads `begin`'s home slot, `mr r29,r4`
 *  keeps the *address* of `end`'s home slot and re-reads it (`lwz r0,0x0(r29)`) in every loop
 *  test, which is why the declaration below is by value and not a pointer. */
struct SIt80046C0C {
  SElem80046C0C* current;
};
typedef struct SIt80046C0C SIt80046C0C;

/** The array header as far as these bytes read it: `+0x04` is the count, `+0x08` the capacity and
 *  `+0x0C` the item array, which is `rstl::vector`'s `mCount`/`mCapacity`/`mItems` after its
 *  empty `mAllocator` (`include/rstl/vector.hpp`). */
struct SVec80046C0C {
  void* mAllocator;  /* +0, empty class: no bytes of its own */
  int mCount;        /* +4 */
  int mCapacity;     /* +8 */
  SElem80046C0C* mItems; /* +0xC */
};
typedef struct SVec80046C0C SVec80046C0C;

SElem80046C0C* fn_80046CCC(SIt80046C0C begin, SIt80046C0C end, SElem80046C0C* out);
void fn_80046C0C(SVec80046C0C* self, int newSize);

/** `fn_80046CCC` - retail `.text:0x80046CCC`, 0x78 = 120 bytes: `rstl::uninitialized_copy` over the
 *  range, whose per-element body is `include/rstl/construct.hpp:53-56`'s placement-new copy behind
 *  its own `cmplwi r30,0x0` null test.  The `+8` store after the copy constructor call is
 *  `TCachedToken`'s own `mItem`, which the implicit copy constructor initialises from the source
 *  after the base has been constructed.  `cur` is declared **before** `tmp`, which is what puts
 *  `cur` in `r31` and `tmp` in `r30` as retail does - the other order swaps them and the function
 *  is otherwise identical. */
SElem80046C0C* fn_80046CCC(SIt80046C0C begin, SIt80046C0C end, SElem80046C0C* out) {
  SElem80046C0C* cur = begin.current;
  SElem80046C0C* tmp = out;
  for (; cur != end.current; cur += 1, tmp += 1) {
    if (tmp) {
      __ct__6CTokenFRC6CToken(tmp, cur);
      tmp->mItem = cur->mItem;
    }
  }
  return tmp;
}

/** `fn_80046C0C` - retail `.text:0x80046C0C`, 0xC0 = 192 bytes: the array's `reserve`.  The
 *  declaration order, the shape of `last` and the doubled null test are load-bearing and measured;
 *  see the header for all three. */
void fn_80046C0C(SVec80046C0C* self, int newSize) {
  SElem80046C0C* last;
  SElem80046C0C* cur;
  SElem80046C0C* newData;
  if (newSize <= self->mCapacity) {
    return;
  }

  newData = (SElem80046C0C*)allocate__Q24rstl17rmemory_allocatorFi(newSize * 12);
  fn_80046CCC((SIt80046C0C){self->mItems}, (SIt80046C0C){self->mItems + self->mCount}, newData);

  cur = self->mItems;
  last = cur + self->mCount;
  for (; cur != last; cur += 1) {
    if (cur) {
      if (cur) {
        __dt__6CTokenFv(cur, 0);
      }
    }
  }
  Free__7CMemoryFPCv(self->mItems);
  self->mItems = newData;
  self->mCapacity = newSize;
}
