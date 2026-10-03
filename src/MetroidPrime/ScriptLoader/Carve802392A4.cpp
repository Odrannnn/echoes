// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10079-10080`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80235E00_text.s:3693-3769`, and the bodies
// below are the C those bytes are the compilation of.
//
// .text 0x802392A4..0x802393A8, 0x104 = 260 bytes, 2 functions:
//
//   fn_802392A4  0x802392A4  0xB8 = 184 bytes  46 instructions  a 16-byte-element `reserve`
//   fn_8023935C  0x8023935C  0x4C =  76 bytes  19 instructions  the `uninitialized_copy` it calls
//
// **What the two are, read off the twins.**  Retail names neither, so the twins identify them,
// and this pair is a byte-shape twin pair of two functions this tree already holds, both in
// `src/MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp` (a `Matching` unit, measured in
// `build/G2ME01/SnakeWeedSwarm/asm/MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.s:73-155`).
// That file is the same code with a 0x24 = 36-byte element; this one has a 0x10 = 16-byte element,
// which is the whole of the difference - `mulli r3,r30,152` there is `slwi r3,r30,4` here,
// `mulli r0,r0,152` is `slwi r0,r0,4`, and the out-of-line element copy `fn_71_B50` that
// `fn_71_3BC0` calls is inlined here as eight `lwz`/`stw` pairs under a null guard.
//
//   fn_802392A4  is `fn_71_3B08` (`CSnakeWeedSwarmVecTail.cpp:212-225`), the same 46
//                instructions in the same order: `stwu r1,-48(r1)` / `mflr` / `stw r0,52(r1)` /
//                `stw r31,44` / `stw r30,40` / `mr r30,r4` / `stw r29,36` / `mr r29,r3` /
//                `lwz r0,8(r3)` / `cmpw r30,r0` / `ble` to the epilogue, then `slwi r3,r30,4` /
//                `bl allocate__Q24rstl17rmemory_allocatorFi`, then the four outgoing-argument
//                stores and `bl fn_8023935C`, then the empty destroy walk (`mr r4,r3` /
//                `add r0,r3,r0` / `b` / `addi r4,r4,16` / `cmplw r4,r0` / `bne`), then
//                `bl Free__7CMemoryFPCv` and the two stores back into the block.  It is
//                `include/rstl/vector.hpp:167-179`'s `reserve` with `mAllocator` four bytes wide,
//                so the count is at +0x4, the capacity at +0x8 and the base at +0xc.
//   fn_8023935C  is `fn_71_3BC0` (`CSnakeWeedSwarmVecTail.cpp:201-207`) and, for the element copy,
//                the twin of `CopyWords` (the same file, line 145), which is
//                `include/rstl/construct.hpp:117-127`'s `uninitialized_copy` over this tree's
//                `rstl::pointer_iterator`: a null-guarded destination test (`cmplwi r5,0` /
//                `beq`) and four `lwz`/`stw` pairs, the loop entered through a `b` to the compare,
//                and the advanced destination handed back (`mr r3,r5` before the `blr`).
//
// **The four outgoing-argument stores are the ABI, not decoration.**  `lwz r6,0(r3)` /
// `lwz r0,0(r4)` as the callee's first two instructions mean both iterators arrive *as addresses
// of one-word objects* (`addi r3,r1,20` / `addi r4,r1,12` at the call), and MWCC passes a by-value
// 4-byte class parameter by pointer.  Each argument is therefore written twice: once into the
// caller's temporary and once into the outgoing argument slot - `stw r6,12(r1)` + `stw r6,8(r1)`
// for `end` and `stw r0,20(r1)` + `stw r0,16(r1)` for `begin`, in the order this run measured.
// **That is why this file is a `.cpp` with `extern "C"` and not a `.c`**: in C a 4-byte struct
// passed by value arrives in a register, and the whole shape above is unreachable.  The name stays
// unmangled because of `extern "C"`, exactly as `Carve80233A90.cpp` in this same directory
// records; a C++ definition without it would mangle to `_Z<len>fn_802392A4...` and objdiff would
// pair nothing.
//
// **The iterators need their converting constructor for the same reason.**  A plain one-word POD
// returned by value comes back in `r3`, and the call site then passes it straight through instead
// of materialising the temporary; `Carve80233A90.cpp`'s header measures that exact failure on its
// own body (19 instructions / 76 bytes become 14 / 56).  A class with a constructor is not
// trivially constructible, so the copy goes through memory and the four stores above are what
// MWCC emits for it.
//
// **Who calls it.**  One call site, in dtk's still-unclaimed
// `build/G2ME01/asm/auto_03_80235E00_text.s:1173`: `fn_802392A4` is called at 0x80236EA8 with
// `addi r3,r1,0x60` on line 1172 and `mr r4,r3` off a `bctrl` on line 1171 - a block built on the
// caller's stack and a capacity out of a virtual call.  So nothing of ours needs the symbol and no
// count depends on it.  `fn_8023935C` is called only by `fn_802392A4`, inside this claim.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function, a broken DOL, and a module hash
// that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  `fn_8023935C` (0x8023935C)
// is the *higher* address and therefore comes first here, so the object leads with
// `fn_802392A4` at offset 0, which is where retail has it.  `python3
// tools/check_decl_order.py --unit main/MetroidPrime/ScriptLoader/Carve802392A4` is the cheap
// check; `tools/flip_test.sh` is the one that decides.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts exactly at `fn_802392A4` and stops exactly where `fn_802393A8`
// (0x802393A8, `symbols.txt:10081`, 0x198 bytes) begins, so nothing above the item's two
// functions is taken.  Neither neighbour is a unit boundary: the nearest claimed range below is
// `MetroidPrime/ScriptLoader/DarkCommando.cpp` (`.text` 0x80235DD4..0x80235E00) and above is
// `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp` (0x802399C8..0x802399F4), and this claim sits
// 0x34A4 bytes into dtk's `auto_03_80235E00_text` (0x80235E00..0x802398C4) - the "proximity to
// another carve is fine, proximity to a unit boundary is not" case `RUNNING_THE_DECOMP.md`
// records for `0x80335A14` and not the cycle it records for `0x80302BAC`.
//
// The directory is retail's own, taken from the nearest claimed ranges: both neighbours are
// `MetroidPrime/ScriptLoader/` units.  For an anonymous function that is the only evidence there
// is, and it beats a lane picking the directory it happened to own.
//
// The three callees are declared, never defined here.  `allocate__Q24rstl17rmemory_allocatorFi`
// (0x802FDAB8) and `Free__7CMemoryFPCv` (0x802CE388) are claimed by other DOL units and are
// defined for the host link elsewhere in `src/`, and `fn_8023935C` is in this file, so the unit
// adds no undefined symbol to either link and needs no `#ifndef __MWERKS__` stand-in.

#include "types.h"

/** 0x802FDAB8, `config/G2ME01/symbols.txt`: `rstl::rmemory_allocator::allocate(int)`, declared
 *  under retail's own emitted spelling so the call needs no header. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** 0x802CE388, `config/G2ME01/symbols.txt`: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** `rstl::pointer_iterator`'s one member, `include/rstl/pointer_iterator.hpp:59`.  Only the word
 *  at offset 0 is ever read - `lwz r6,0(r3)` at the callee's first instruction - so one word is
 *  the whole of it.  The constructor is load-bearing: see the header. */
struct SHintLocIter {
  void* mCur;
  SHintLocIter(void* cur) : mCur(cur) {}
  bool operator!=(const SHintLocIter& other) const { return mCur != other.mCur; }
  void operator++() { mCur = static_cast< char* >(mCur) + 16; }
};

/** The block's element, named only for its size: 0x10 = 16 bytes, four words, and nothing else
 *  about it is knowable from these bytes.  Four `int` members rather than a `char[16]` because
 *  the copy below is a member-wise one - retail's eight `lwz`/`stw` pairs, not the `lmw`/`stmw`
 *  pair `-use_lmw_stmw on` would emit for an aggregate assignment. */
struct SHintLocation {
  int x0_mMlvlId;
  int x4_mMreaId;
  int x8_mAreaId;
  int xc_mStringId;
};

/** This tree's `rstl::vector` layout with `mAllocator` four bytes wide, which is what puts the
 *  element count at +0x4 rather than at +0x8: `x00` is the allocator and is never read. */
struct SBlock16 {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

/** `rstl::destroy` over a range of an element with nothing to tear down.  The body is empty and
 *  the loop is still emitted - retail's `addi r4,r4,16` with nothing between it and the
 *  `cmplw`/`bne` - because the compiler is counting on the destructor call, not on any work. */
template < class T >
static inline void DestroyWords(T* begin, T* end) {
  for (T* p = begin; p != end; ++p) {
  }
}

extern "C" void fn_802392A4(struct SBlock16* self, int capacity);
extern "C" void* fn_8023935C(SHintLocIter begin, SHintLocIter end, void* out);

/** `fn_8023935C` - retail `.text:0x8023935C`, 0x4C = 76 bytes, 19 instructions, no frame:
 *  `rstl::uninitialized_copy` over this element, i.e.
 *  `include/rstl/construct.hpp:117-127` with `construct`'s placement-new guard still in it
 *  (`cmplwi r5,0` / `beq` over the copy).  The element copy is inlined here as four
 *  `lwz`/`stw` pairs, and the advanced destination is returned in `r3`. */
extern "C" void* fn_8023935C(SHintLocIter begin, SHintLocIter end, void* out) {
  char* tmp = static_cast< char* >(out);
  for (SHintLocIter cur = begin; cur != end; ++cur, tmp = tmp + 16) {
    if (tmp != 0) {
      SHintLocation* dst = reinterpret_cast< SHintLocation* >(tmp);
      const SHintLocation* src = static_cast< const SHintLocation* >(cur.mCur);
      dst->x0_mMlvlId = src->x0_mMlvlId;
      dst->x4_mMreaId = src->x4_mMreaId;
      dst->x8_mAreaId = src->x8_mAreaId;
      dst->xc_mStringId = src->xc_mStringId;
    }
  }
  return tmp;
}

/** `fn_802392A4` - retail `.text:0x802392A4`, 0xB8 = 184 bytes: this block's `reserve`.  A
 *  capacity at or below the stored one returns without allocating; otherwise it allocates
 *  `capacity * 16`, moves the live elements across, walks the old range with nothing to destroy,
 *  frees the old base and stores the new base and capacity back.  Both ends are re-read from
 *  `self` after the copy rather than held in locals, which is the trade
 *  `CSnakeWeedSwarmVecTail.cpp` records for the same function. */
extern "C" void fn_802392A4(struct SBlock16* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  char* const buffer = static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 16));
  fn_8023935C(SHintLocIter(self->x0c),
              SHintLocIter(static_cast< char* >(self->x0c) + self->x04 * 16), buffer);
  DestroyWords(static_cast< SHintLocation* >(self->x0c),
               static_cast< SHintLocation* >(self->x0c) + self->x04);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}