// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:635`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80023FFC_text.s:787-836`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80024B70..0x80024C18, 0xA8 = 168 bytes, 1 function:
//
//   fn_80024B70    0x80024B70  0xA8    42 instructions
//
// **It is `rstl::vector<TToken<CCharLayoutInfo>>::~vector()` in MWCC's deleting-destructor form**:
// `mr. r28,r3` / `beq` guards the receiver, `extsh. r0,r29` / `ble` tests the incoming 16-bit flag
// so that only a positive one reaches the final `Free__7CMemoryFPCv(self)` - the call sites pass
// `li r4,-1`, meaning "do not free me afterwards" - and `mr r3,r28` in the epilogue returns the
// receiver.  `src/MetroidPrime/Carve800045A0.c:13-17` documents that convention on a fourth copy,
// in this same neighbourhood.
//
// Read off the 42 instructions:
//
//   - `lwz r0,4(self)` is the count and `lwz r30,0xc(self)` the item array.
//     `include/rstl/vector.hpp:18-21` is `mAllocator`, `mCount`, `mCapacity`, `mItems`, so the
//     bytes read `+0x4` and `+0xC` of that header.
//   - `slwi r0,r0,3` is `mItems + mCount` with `sizeof(T) == 8`, so the element is 8 bytes and
//     the loop walks in strides of 8 (`addi r30,r30,0x8`).  `TToken<T>` adds no member to
//     `CToken`, which is `CObjectReference* mObjRef` plus `bool mLockHeld`
//     (`include/Kyoto/CToken.hpp:33-36`), 8 bytes - hence the `vector<TToken<CCharLayoutInfo>>`
//     this is instantiated for.
//   - the loop's `cmplwi r30,0` / `beq` before `mr r3,r30 / li r4,0 / bl __dt__6CTokenFv` is
//     `destroy_impl`'s null test: an empty `TToken` has no object reference, so the destructor is
//     not reached.  Retail's own `__dt__6CTokenFv` (0x8030154C) carries the same
//     `mr. rX,r3 / beq` guard and the same `extsh.` flag test.
//   - the first `bl Free__7CMemoryFPCv` is `mAllocator.deallocate(mItems)` and the second is the
//     flag-guarded release of the receiver.
//   - **the four stores at `r1+0x14/0x10/0x0C/0x08` that read like dead writes are the inliner's
//     two by-value `pointer_iterator` home slots**, one copy for `destroy`'s parameters and one
//     for `destroy_impl`'s, each holding the pointer twice.  Removing them is 16 bytes short and
//     does not match - measured, and the same four are described at
//     `src/MetroidPrime/Carve800045A0.c:26-30`.
//
// This body is a byte-shape twin of an already-matched function, which is what makes the operands
// above readable rather than guessed.  Disassembled beside retail's range it is
// instruction-for-instruction identical apart from the two `bl` displacements, which are
// address-relative:
//
//   __dt__Q24rstl62vector<25TToken<15CCharLayoutInfo>,Q24rstl17rmemory_allocator>Fv
//   at 0xBdc of build/G2ME01/src/MetroidPrime/Factories/CCharacterFactory.o
//
// Both callees are declared, never defined here.  `Free__7CMemoryFPCv` (0x802CE388,
// `symbols.txt:12992`, size 0x64) is claimed by `Kyoto/Alloc/CMemory.cpp` (`.text`
// 0x802CE224..0x802CE72C) and `__dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`, size 0x68) by
// `Kyoto/CToken.cpp` (`.text` 0x803013D0..0x80301710), both real units, so the three `bl`s below
// resolve inside our own tree.  So this unit claims `.text` and nothing else.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Check it first with
// `python3 tools/check_decl_order.py --unit Carve80024B70`.
//
// **Retail names none of this.**  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_80024B70v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.  One definition, so the descending-source-order rule holds trivially;
// only `tools/flip_test.sh` decides.
//
// Its own unit because a claim may not span an unclaimed gap: `fn_80024C18` (0x80024C18) begins the
// next claim, `MetroidPrime/Carve80024C9C.c`.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/CCredits.cpp` (`.text` 0x8001FF7C..0x80023FFC) and above is
// `MetroidPrime/CAnimData.cpp` (0x80025D3C..0x8002F7A8), so this address sits in the
// `MetroidPrime/` neighbourhood.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x8030154C, `symbols.txt:13922`, size 0x68: `CToken::~CToken()` in MWCC's deleting form - the
 *  `mr. r3 / beq` receiver guard and the `extsh.` flag test are visible in its own 0x68 bytes.
 *  Claimed by `Kyoto/CToken.cpp` (`.text` 0x803013D0..0x80301710).  Declared, never defined here. */
extern void* __dt__6CTokenFv(void* self, short flag);

/** The 8-byte element this walks in strides of 8: `TToken<CCharLayoutInfo>`, whose only members
 *  are `CToken`'s (`include/Kyoto/CToken.hpp:33-36`).  Only the two words are modelled; nothing
 *  here reads them - the loop only calls `CToken`'s destructor on the address. */
struct SToken {
  void* mObjRef;
  int mLockHeld;
};

/** The header in front of the array: `rstl::vector`'s `mAllocator`, `mCount`, `mCapacity`,
 *  `mItems` (`include/rstl/vector.hpp:18-21`).  Only the two fields the bytes read are modelled -
 *  `+0x4` is the count, which is what `slwi` scales, and `+0xC` the item array, which is what the
 *  first `Free__7CMemoryFPCv` releases.  The words at +0x0 and +0x8 are padding here and exist
 *  only so the fields land on retail's displacements. */
struct SVector {
  int mAllocator;
  int mCount;
  int mCapacity;
  struct SToken* mItems;
};

/** One 4-byte `rstl::pointer_iterator` (`include/rstl/pointer_iterator.hpp:58` is its single
 *  `T* current`), named only so the by-value parameter list of the `destroy` chain - which is
 *  what emits the four home-slot stores - can be written in C. */
struct SIterator {
  struct SToken* current;
};

/** `rstl::destroy_impl(begin, end)`, inlined: the `if (cur)` is retail's `cmplwi r30,0` / `beq`
 *  at 0x80024BBC/0x80024BC0, and the empty `SToken` case is why it is there at all. */
static inline void DestroyImpl(struct SIterator begin, struct SIterator end) {
  struct SToken* cur = begin.current;
  struct SToken* last = end.current;
  for (; cur != last; ++cur) {
    if (cur) {
      (void)__dt__6CTokenFv(cur, 0);
    }
  }
}

static inline void Destroy(struct SIterator begin, struct SIterator end) {
  DestroyImpl(begin, end);
}

void* fn_80024B70(struct SVector* self, short flag);

void* fn_80024B70(struct SVector* self, short flag) {
  if (self) {
    struct SIterator begin;
    struct SIterator end;
    begin.current = self->mItems;
    end.current = self->mItems + self->mCount;
    Destroy(begin, end);
    Free__7CMemoryFPCv(self->mItems);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
