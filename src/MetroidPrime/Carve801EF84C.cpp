/**
 * `fn_801EF84C` - retail `.text:0x801EF84C`, `symbols.txt:7978`, 0x64 = 100 bytes: the deleting
 * destructor of a two-word `rstl::auto_ptr<T>`.  `mHas` at +0 guards the delete, the owned pointer
 * is at +4, and only a positive flag reaches `Free__7CMemoryFPCv` with the receiver returned in
 * r3.  It is a byte-shape twin of
 * `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` (0x80110D8C, 0x64,
 * `build/G2ME01/asm/MetroidPrime/Factories/CScannableObjectInfo.s:197-226`), which is these same
 * 25 instructions with `bl __dt__20CScannableObjectInfoFv` in place of this `bl fn_801EF730`.
 * `include/rstl/auto_ptr.hpp:21-25` is that shape; the flag is a **short** (`extsh. r0,r31`, not
 * the `cmpwi` an `int` gives) and `if (flag > 0)` sits **inside** `if (self)`.
 *
 * This copy's `T` is whatever `fn_801EF730` destroys, so the class is written out as the two
 * words the bytes read rather than instantiated - the same trade
 * `src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp:99-106` records for its own copy of this
 * shape, whose own header explains why `template class rstl::auto_ptr<CAnimData>;` emits no
 * `.text` at all and why an explicit specialization does not either.
 */

/**
 * `fn_801EF8B0` - retail `.text:0x801EF8B0`, 0x8 = 8 bytes: `lwz r3,0x8(r3) / blr`, the member at
 * +8 returned as a pointer.  Its byte-shape twin is
 * `GetParmDeleteIOWin__7MakeMsgFRC20CArchitectureMessage` (`src/MetroidPrime/Decode.cpp:4`,
 * 0x80048F70, `build/G2ME01/asm/MetroidPrime/Decode.s:457-461`), these same two instructions:
 * `MakeMsg::GetParmDeleteIOWin` returns a *reference* to whatever `GetParm()` points at, and
 * returning a reference compiles to handing back the pointer.
 *
 * **Who calls it, and what that fixes.**  `src/MetroidPrime/Player/CScanDisplay.cpp` calls it at
 * 0x80112854 (`build/G2ME01/asm/MetroidPrime/Player/CScanDisplay.s:314`) and the next ten
 * instructions read the result as a vector header - `lwz r0,0x4(r28)` the count,
 * `lwz r4,0xc(r28)` the items, `slwi r0,r0,3` an 8-byte element - so it returns a pointer to
 * somebody's `rstl::vector`.  Its other two call sites, 0x801ED77C and 0x801EEF04, are both in the
 * unclaimed `auto_03_801ECDCC_text`.
 */

/**
 * `fn_801EF8B8` - retail `.text:0x801EF8B8`, `symbols.txt:7980`, 0x148 = 328 bytes: the copy
 * constructor of `rstl::vector< rstl::pair< uint, uint > >`,
 * `include/rstl/vector.hpp:127-137`.  Its byte-shape twin is the COMDAT
 * `__ct__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>FRCQ24rstl55vector<Q24rstl11
 * pair<Ui,Ui>,Q24rstl17rmemory_allocator>` at 0x8018FE6C
 * (`build/G2ME01/asm/MetroidPrime/CSlideShow.s:4174-4263`, the instantiation
 * `src/MetroidPrime/CSlideShow.cpp:412-416` forces) - these same 82 instructions.
 *
 * **What this call is, from retail's own caller.**  `src/Kyoto/Animation/CAnimSourceReader.cpp`
 * calls it at 0x802A3AA0 (`build/G2ME01/asm/Kyoto/Animation/CAnimSourceReader.s:991`) with
 * `addi r3,r29,0x38` as the destination, one header past the vtable at +0 and the
 * `lwz r0,0x4(r6)`-derived count in front of it.  `include/rstl/pair.hpp:44-52` records that
 * `CAnimSourceReaderBase` keeps its POI state in exactly two `rstl::pair< uint, uint >` vectors,
 * and the `lwz r0,0x8(r4)` / `lwz r3,0x4(r4)` this reads are that header's capacity and count.
 *
 * **The body has to be written against the real `rstl` headers, and that is measured, not
 * preference.**  Written in C - the two words of the header plus a hand-written loop - the same
 * logic is 34 of 436 bytes short of retail's, all of it in the copy loop: retail keeps the count
 * in r3 and derives *both* `srwi. r0,r3,3` and `andi. r3,r3,7` from it, while the C forms put it
 * in r6 or r0 and shuffle it into r3 with an extra `mr`.  `rstl::uninitialized_copy_n`
 * (`include/rstl/construct.hpp:141-151`) takes the count as its second argument and that is what
 * carries it in the register retail uses - the finding
 * `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:8-16` already records for the byte-element
 * instantiation, whose object is byte-identical to retail's.  Four C spellings were measured (block
 * loop with `int remaining`; the same with the parameter decremented; and both again as a
 * three-argument `static`/`static inline` helper with and without `const` on the source); none
 * reaches 100, and the two that keep the count in r3 do it by outlining the helper, which puts a
 * fifth function in the object and breaks `tools/unit_fit.sh`.
 *
 * `rstl::vector`'s own copy constructor cannot be named from here either: MWCC rejects explicit
 * instantiation of a member (`template V::vector(const V&);` is a syntax error) and
 * `template class rstl::vector<...>` does not emit the COMDAT this claim needs - which is why
 * `CSlideShow.cpp:405-411` forces the instantiation through a real function instead.  Writing the
 * body out is the same approach, and it emits no symbol beyond `fn_801EF8B8` itself.
 *
 * Its three callees are declared, never defined here: `allocate__Q24rstl17rmemory_allocatorFi`
 * (0x802FDAB8, `symbols.txt:13824`) is `rstl::rmemory_allocator::allocate(int)` under CodeWarrior's
 * own mangling - the port spells it `_ZN4rstl17rmemory_allocator8allocateEi` and supplies retail's
 * as `stub_179` at `src/MetroidPrime/PortLinkStubs.cpp:847`, so calling it by retail's name is
 * both correct for the DOL and resolvable for the host - and `Free__7CMemoryFPCv` (0x802CE388) is
 * claimed by `Kyoto/Alloc/CMemory.cpp`.  So this unit claims `.text` and nothing else.
 *
 * Its own unit, and no gap is spanned: `fn_801EF7B0` (0x801EF7B0, 0x9C) below ends exactly at
 * 0x801EF84C, and 0x801EFA00 above is where the `Matching` `MetroidPrime/CStaticGeometryMap.cpp`
 * claim begins.  What is left of the run below is `auto_03_801EF598_text`, which now runs
 * 0x801EF598..0x801EF84C; `fn_801EF730` at 0x801EF730 stays inside it and is retail's.
 *
 * Source order is **descending by address** and that is load-bearing: mwcceppc emits function
 * definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
 * ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
 * `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit
 * MetroidPrime/Carve801EF84C.cpp`.
 *
 * Retail names none of these: `symbols.txt` carries the `fn_<addr>` placeholder and this file
 * reproduces those symbols verbatim, which is why the definitions are wrapped in `extern "C"` -
 * the arrangement `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34` already uses for a
 * Matching carve of exactly this shape.
 */

#include "rstl/construct.hpp"
#include "rstl/pair.hpp"
#include "rstl/rmemory_allocator.hpp"

extern "C" {

/** 0x801EF730, `symbols.txt:7976`, 0x54 = 84 bytes: the deleting destructor of whatever
 *  `fn_801EF84C`'s +4 member holds - `mr. r30,r3 / beq`, `li r4,-1`, `bl fn_80004744`, then the
 *  `extsh.` flag test and `Free__7CMemoryFPCv`.  It sits inside the unclaimed `auto_03_801EF598_text`
 *  below this claim, so nothing in the DOL defines it.  Declared, never defined here for the
 *  matching build; the port-only stand-in is at the end of this file. */
void fn_801EF730(void* self, int flag);

/** 0x802CE388, `symbols.txt:12992`, 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in the DOL, so our own tree supplies it.  Declared, never defined
 *  here.  `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the host. */
void Free__7CMemoryFPCv(const void* ptr);

/** The header `fn_801EF8B8` constructs and `fn_801EF8B0`'s callers read: `rstl::vector`'s four
 *  words, `mAllocator`/`mCount`/`mCapacity`/`mItems` (`include/rstl/vector.hpp:18-21`).  Only the
 *  three the bytes touch are named; `mAllocator` is `rstl::rmemory_allocator`, which is empty, so
 *  one word of padding is all it contributes. */
struct SPairVec {
  rstl::rmemory_allocator mAllocator;
  int mCount;
  int mCapacity;
  rstl::pair< uint, uint >* mItems;
};

/** The two words `fn_801EF84C` reads: the flag at +0 that guards the delete, the owned pointer at
 *  +4 that gets deleted.  `rstl::auto_ptr<T>`'s own layout (`include/rstl/auto_ptr.hpp:15-16`),
 *  with T whatever `fn_801EF730` destroys. */
struct SHasItem {
  bool mHas;
  void* mItem;
};

/** The header whose `+8` word `fn_801EF8B0` returns, and the pointer itself. */
struct SHasHeaderAt8 {
  int m0;
  int m4;
  void* m8;
};

void* fn_801EF8B8(SPairVec* self, const SPairVec* other);
void* fn_801EF8B0(SHasHeaderAt8* self);
void* fn_801EF84C(SHasItem* self, short flag);

void* fn_801EF8B8(SPairVec* self, const SPairVec* other) {
  self->mCount = other->mCount;
  self->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    self->mItems = nullptr;
  } else {
    rstl::rmemory_allocator::allocate(self->mItems, self->mCapacity);
    rstl::uninitialized_copy_n(other->mItems, self->mCount, self->mItems);
  }
  return self;
}

void* fn_801EF8B0(SHasHeaderAt8* self) { return self->m8; }

void* fn_801EF84C(SHasItem* self, short flag) {
  if (self != nullptr) {
    if (self->mHas) {
      fn_801EF730(self->mItem, 1);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `fn_801EF730`'s own 84 bytes are a spelling job
// of their own and nothing in this tree has claimed them.  It exists so the host link resolves the
// `bl` above, and it is the same trade `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` makes
// for `__dt__17CCameraShakerDataFv`: an announced stand-in, kept beside the one reference that
// asks for it.  Before this carve nothing in the port referenced the symbol - only dtk's `auto_*`
// objects did - so it is new to the port's link, and `tools/link_gap.py` fails the gate on a
// missing symbol that is not accounted for in `docs/research/port_link_gap.md`.  The guard is
// `__MWERKS__`, not `TARGET_PC`, to match that file: the matching build must take the symbol from
// dtk's own object of the surrounding run, and a second definition there would be a duplicate.
void fn_801EF730(void* self, int flag) {
  (void)self;
  (void)flag;
}
#endif

} // extern "C"