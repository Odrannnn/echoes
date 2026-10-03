// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:70-72`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8000408C_text.s` before the claim existed (the same range
// is now `build/G2ME01/asm/MetroidPrime/Carve8000432C.s`), and the bodies below are the C++ those
// bytes are the compilation of.
//
// .text 0x8000432C..0x80004438, 0x10C = 268 bytes, 3 functions:
//
//   fn_8000432C    0x8000432C  0x84  33 instructions  the array's deleting destructor
//   fn_800043B0    0x800043B0  0x38  14 instructions  `rstl::destroy` over the range
//   fn_800043E8    0x800043E8  0x50  20 instructions  `rstl::destroy_impl` over the range
//
// **What the three are: the `rstl::vector` destructor and the two `rstl::destroy` pair it calls,
// with a 36-byte element.**  Retail names none of them, so this is read off the call edges and the
// argument registers, and each of the three has a byte-shape twin already at 100.0% in this tree
// (measured instruction by instruction, not by percentage alone):
//
//   fn_8000432C  `mr. r30,r3 / beq` guards the receiver, r4 is kept in r31 and re-tested with
//                `extsh.`, the array is destroyed, its buffer is freed, and only a positive flag
//                reaches a second `CMemory::Free` - MWCC's deleting-destructor convention.  **Exact
//                twin:** `__dt__Q24rstl49vector<12CVirtualBone,Q24rstl17rmemory_allocator>Fv`
//                (0x80310288, 0x84) in `Kyoto/Animation/DolphinCSkinRules.cpp`, 132 bytes and
//                100.0%, these 33 instructions word for word with two changes: `slwi r0,r0,5`
//                there against `mulli r0,r0,0x24` here (that is the element stride, 0x20 against
//                0x24) and its three `bl` targets.
//   fn_800043B0  a frame, two `lwz` off the two arguments and two `stw` into new stack words:
//                it copies the two iterators and forwards them, which is
//                `include/rstl/construct.hpp:111-114` - `destroy(It begin, It end)` whose whole
//                body is `destroy_impl(begin, end)`.  **Exact twin:** the `destroy` of
//                `pointer_iterator<CVirtualBone, ...>` (0x8031030C, 0x38) in the same unit, these
//                14 instructions with a different `bl` target.
//   fn_800043E8  the rotated loop `b cond / body / cond`: `It cur = begin; for (; cur != end;
//                ++cur) destroy(&*cur);` - `include/rstl/construct.hpp:100-109`.  **Exact twin:**
//                `destroy_impl<rstl::pointer_iterator<rstl::pair<...>>>` (0x802F2E3C, 0x50) in
//                `Kyoto/Particles/CSpawnSystemKeyframeData.cpp`, these 20 instructions word for
//                word with `addi r31,r31,20` there against `addi r31,r31,0x24` here and a
//                different `bl` target.
//   (Both twin units are themselves `NonMatching` - 22/23 and 36/37 - because of other functions
//   in them; the two functions named here are each at 100.0%.  The claim is about the bytes.)
//
// **The element is `CWorldState`, and the array is `rstl::vector< CWorldState >`.**  The only
// caller of the element teardown in retail is `fn_800043E8`'s own loop
// (`grep -rn 'bl fn_80004438' build/G2ME01/asm/`), and `fn_80004438` (0x80004438, 0x20) is
// `rstl::destroy< CWorldState >(T*)` - claimed and matched by `MetroidPrime/Carve80004438.c`,
// whose header states the same reading.  Its 0x24-byte element is `CWorldState`
// (`CHECK_SIZEOF(CWorldState, 0x24)`, `include/MetroidPrime/Player/CWorldState.hpp:48`), which is
// exactly the stride both loops use, and the owning member is
// `rstl::vector< CWorldState > mWorldStates` (`include/MetroidPrime/Player/CGameState.hpp:254`).
// The stride is the only thing in these bytes that fixes `sizeof(T)`: `mulli r0,r0,0x24` in the
// destructor and `addi r31,r31,0x24` in the loop, and neither is a shift.
//
// **The four stack words at +0x08..+0x17 are two copies of the two iterators, and both are
// load-bearing.**  `fn_800043B0` takes its pair **by value**, and MWCC passes a class-type by-value
// argument as a pointer to a caller-side temporary - so the call needs a temporary for each
// `begin()`/`end()` result *and* a second one for each parameter.  That is the four `stw`, and it
// is why the argument registers are `addi r3,r1,0x14` and `addi r4,r1,0xc`: the second pair.
// Naming the two iterators in locals and passing them (`It b = self->begin(); It e =
// self->end(); fn_800043B0(b, e);`) gives the same 132 bytes but a different frame - the
// parameter copies land at +0x08 and +0x0C, not +0x0C and +0x14 - so the call is written the way
// `include/rstl/vector.hpp:139-142` writes it, as the call itself.  Measured: both spellings, and
// only the call form is byte-exact.
//
// **`fn_80004438` is the one call this carve cannot own and cannot drop.**  It is 0x10 bytes above
// the claim and is claimed by `MetroidPrime/Carve80004438.c` (`Matching`, 2/2), so the `bl`
// resolves against our own object and against retail's own address.  `Free__7CMemoryFPCv` is
// 0x802CE388, `symbols.txt:12992`, size 0x64, claimed by `Kyoto/Alloc/CMemory.cpp`
// (`.text` 0x802CE224..0x802CE72C); declared, never defined here.
//
// **`~vector()`'s own `operator delete` is the second `Free`, and it is MWCC's, not ours.**  The
// shape `if (self) { <members>; if (flag > 0) Free__7CMemoryFPCv(self); } return self;` is what
// `include/rstl/vector.hpp:139-142` compiles to and what the exact twin above shows, so the flag
// test and the `return self` (the `mr r3,r30` at 0x80004398 is **after** both early exits and
// **before** the frame is torn down) are reproduced rather than invented.  The flag is a
// **`short`**: retail's tail is `extsh. r0,r31 / ble`, which an `int` would make `cmpwi r31,0`.
//
// `rstl::pointer_iterator` and the array are **declared locally**, not included.  This is what
// keeps the three bodies reading as the `rstl` ones they are while emitting nothing but them: the
// local template has no out-of-line members, so the object carries three functions and nothing
// else (measured: `nm` shows `fn_8000432C`, `fn_800043B0`, `fn_800043E8` and two undefined
// symbols, `.text` 0x10C).
//
// **`extern "C"` is what keeps the three `fn_` names unmangled** - `symbols.txt:70-72` carries
// those placeholders verbatim, and a C++ definition would mangle to `_Z<len>fn_<addr>...` and
// objdiff would pair nothing - so the unit is a `.cpp` rather than a `.c`; `extern "C"` is the
// same trade `MetroidPrime/Carve8000447C.cpp` makes for `__dt__11CWorldStateFv`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Carve8000432C` is the cheap check.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit on a unit boundary
// that a neighbouring `Matching` unit's `.text` ends on: below, 0x8000408C..0x8000432C
// (`fn_8000408C` .. `fn_8000432C`) is unclaimed and the claim below *that* is
// `MetroidPrime/Carve80004010.c` (0x80004010..0x8000408C); above, the claim is
// `MetroidPrime/Carve80004438.c` (0x80004438..0x8000447C).  The directory is retail's own, taken
// from those neighbours.  The claim starts at 0x8000432C and not at the `auto_*` unit's own start
// of 0x8000408C, which is what keeps `dtk dol split` from reporting a link-order cycle.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: retail's `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x80004438, `symbols.txt:73`, size 0x20: `rstl::destroy< CWorldState >(T*)`, the element
 *  teardown the loop below calls.  Claimed by `MetroidPrime/Carve80004438.c` (`Matching`, 2/2),
 *  so both the DOL link and the port link resolve it.  Declared, never defined here. */
extern "C" void fn_80004438(void* self);

/** The 0x24-byte element as far as these bytes read it: nothing here names it, and retail has no
 *  name for it either, so it is the anonymous 36-byte type whose size `mulli r0,r0,0x24` and
 *  `addi r31,r31,0x24` fix.  `CWorldState` is that type in retail (`CHECK_SIZEOF(CWorldState,
 *  0x24)`, `include/MetroidPrime/Player/CWorldState.hpp:48`) but it is named by nothing in this
 *  range, so the struct below stays anonymous and carries only its size. */
struct S8000432CElem {
  unsigned int x00[9];
};

struct S8000432C;

namespace rstl {

/** `include/rstl/pointer_iterator.hpp`'s `pointer_iterator`, cut to the four members these three
 *  bodies use.  One word, and the by-value pass-by-pointer convention is what the four `stw` at
 *  0x80004378..0x80004394 in the destructor are. */
template < class T, class Vec >
class pointer_iterator {
public:
  typedef long difference_type;
  pointer_iterator() : current(nullptr) {}
  pointer_iterator(T* begin) : current(begin) {}
  pointer_iterator(const Vec* owner, T* begin) : current(begin) {}
  T& operator*() const { return *current; }
  bool operator!=(const pointer_iterator& other) const { return current != other.current; }
  pointer_iterator& operator++() {
    ++current;
    return *this;
  }
  T* get_pointer() const { return current; }

protected:
  T* current;
};

} // namespace rstl

typedef rstl::pointer_iterator< S8000432CElem, S8000432C > It8000432C;

extern "C" void fn_800043E8(It8000432C begin, It8000432C end);
extern "C" void fn_800043B0(It8000432C begin, It8000432C end);
extern "C" void* fn_8000432C(S8000432C* self, short flag);

/** The array as far as these bytes read it: the count at +0x04 and the buffer at +0x0C are the
 *  two words the destructor loads, and +0x00 and +0x08 are never touched here, so they are left as
 *  the two words between them.  `begin()`/`end()` are `include/rstl/vector.hpp:27-34` verbatim,
 *  and the `T* const last` inside `end()` is what makes retail read the buffer twice
 *  (`lwz r5,0xc(r30)` at 0x80004354 and `lwz r0,0xc(r30)` at 0x80004368) - drop it and the two
 *  reads collapse into one and the function is 4 bytes short. */
struct S8000432C {
  int x00;
  int mCount;
  int x08;
  S8000432CElem* mItems;

  It8000432C begin() { return It8000432C(this, mItems); }
  It8000432C end() {
    S8000432CElem* const last = mItems + mCount;
    return It8000432C(last);
  }
};

/** `fn_800043E8` - retail `.text:0x800043E8`, 0x50 = 80 bytes: `rstl::destroy_impl` over the
 *  range.  `It cur = begin;` is the copy `lwz r31,0(r3)` at 0x800043F8, and `end` is never
 *  copied out of the caller's frame - `mr r30,r4` then `lwz r0,0(r30)` inside the loop is why the
 *  bound is re-read every iteration. */
extern "C" void fn_800043E8(It8000432C begin, It8000432C end) {
  It8000432C cur = begin;
  for (; cur != end; ++cur) {
    fn_80004438(&*cur);
  }
}

/** `fn_800043B0` - retail `.text:0x800043B0`, 0x38 = 56 bytes: `rstl::destroy` over the range,
 *  whose whole body is the `destroy_impl` call above. */
extern "C" void fn_800043B0(It8000432C begin, It8000432C end) { fn_800043E8(begin, end); }

/** `fn_8000432C` - retail `.text:0x8000432C`, 0x84 = 132 bytes.  The call is written as the call
 *  and not through named locals; see the header for the measurement behind that. */
extern "C" void* fn_8000432C(S8000432C* self, short flag) {
  if (self) {
    fn_800043B0(self->begin(), self->end());
    Free__7CMemoryFPCv(self->mItems);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}