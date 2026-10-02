// CFlyerSwarmRelTail.cpp - FlyerSwarm's (module 21) rstl support block, `.text 0x1708..0x198C`:
// one contiguous run of eight functions, all eight ours, and nothing else. The module's head
// (`CFlyerSwarmRel.cpp`, 0x64..0xD8) and its `GetBoidPosition` (`CFlyerSwarm.cpp`, 0x3C..0x64) are
// separate units, and `REL/REL_Setup.cpp` claims the tail at 0x1C58; everything between this range
// and that tail stays unclaimed, so dtk fills it from retail and the module's sha1 against
// `config/G2ME01/config.yml` still holds. Sizes from
// `config/G2ME01/rels/FlyerSwarm/symbols.txt`:
//
//   0x1708 fn_21_1708  0x8C  the deleting destructor of the count+array vector whose element is
//                            the 0x24-byte record below: `destroy_elements()`, then `Free(this)`
//                            on a positive flag. **Nothing in the module references it** - it is
//                            the one function here that needs `config.yml`'s `force_active` list.
//   0x1794 fn_21_1794  0x44  that vector's copy constructor: the count first, then the element
//                            array through the copy loop below - `fn_80248E60`'s shape.
//   0x17D8 fn_21_17D8  0x68  `uninitialized_copy_n` for the 0x24-byte element - the loop entered
//                            at its bottom test, returning the end cursor.
//   0x1840 fn_21_1840  0x20  `construct`'s forwarder, one call and nothing else.
//   0x1860 fn_21_1860  0x28  `construct_impl`: `new (place) Element(src)`, whose null test is the
//                            `new` expression's own check.
//   0x1888 fn_21_1888  0x4C  the element's out-of-line copy constructor - see the rename note.
//   0x18D4 fn_21_18D4  0x58  a second deleting destructor: tears down the member at +0x18 through
//                            `fn_21_14E4` (this module's unclaimed 0x14E4, 0x50) and frees itself.
//   0x192C fn_21_192C  0x60  `CFlyerSwarm`'s deleting destructor: stores the module's own vtable
//                            `lbl_21_data_4` over the object, calls the imported base destructor
//                            `fn_80_89F4`, then frees itself on a positive flag. The vtable at
//                            `.data:0x8` holds this address, which is why it is force-active in
//                            dtk's `ldscript.lcf` already.
//
// **The whole range is compiled with `GC/2.7`, not the module's default `GC/1.3.2`, and that is
// measured rather than preferred.** Under 1.3.2 `fn_21_1794` emits retail's `lwz r0,0x0(r4)` four
// instructions late and the element copy at 0x1888 is a word copy; under 2.7 both are
// instruction for instruction retail's. That is the same finding `CLumiteRelTail.cpp` records for
// `fn_39_738` and `CSandBossRelTail.cpp` for its first two functions: this family's out-of-line
// library blocks were compiled by the later compiler.
//
// **`fn_21_1888` is renamed in `symbols.txt` to the mangled name our object emits.**
// `__ct__10SFlyerElemFRC10SFlyerElem` is a copy constructor, so it cannot be written as a free
// function named `fn_21_1888`: the same body as a plain struct assignment compiles to a *word*
// copy (nine `lwz/stw` pairs) where retail copies the six floats as floats with two registers in
// flight, and a placement `new` in its place adds the `new` expression's null test, which retail
// does not have. The out-of-line member definition below is the source those bytes come from; the
// name the module already had for it was a dtk placeholder.
//
// The element itself is not modelled by any header in this tree. It is 0x24 bytes - six floats and
// three words - and it is reached as a `void*`/offset record elsewhere in the module, so the type
// here is the copy/teardown shape and nothing more; `SFlyerVec` is only the count at +0 with the
// array behind it, which is what makes the loop at 0x1708 and the copy at 0x1794 compile.
//
// Definitions are in descending retail address order: mwcceppc emits definitions in reverse source
// order, so the class's copy constructor definition below sits between `fn_21_18D4` and
// `fn_21_1860`, exactly where its bytes belong. `python3 tools/check_decl_order.py --unit` does not
// resolve a path out of a `Rel(...)` block, so this is verified by the module's sha1.
//
// The file is listed in `files.cmake`, and **its host branch defines nothing at all** - the
// arrangement `CLumiteRelTail.cpp`, `CSandBossRelTail.cpp` and `DigitalGuardianDestroy.cpp` use:
// `fn_80_89F4` (SwarmBasics), `fn_21_14E4` and `lbl_21_data_4` are module-local names the port
// cannot resolve, so a host definition would grow the port's undefined count.

#include "Kyoto/Alloc/CMemory.hpp"
#include "types.h"

#ifdef __MWERKS__

// The 0x24-byte record the support functions below are instantiated for: six floats, then three
// words. Nothing in this tree declares it under a real name - the module reaches the same address
// as a raw pointer in `CFlyerSwarm.cpp` and in the unclaimed code at 0x16AC - so it is modelled
// only as far as the copy, the loop and the destructor need.
struct SFlyerElem {
  float x00;
  float x04;
  float x08;
  float x0C;
  float x10;
  float x14;
  u32 x18;
  u32 x1C;
  u32 x20;

  SFlyerElem(const SFlyerElem& other);
  // Empty and user-declared: `destroy_elements` below has to keep its loop, and an element whose
  // destructor the compiler can see is trivial would remove it.
  ~SFlyerElem() {}
};

// The head of the vector-shaped object: the count at +0 with the elements right behind it.
struct SFlyerVec {
  int mCount;
  SFlyerElem mData[8];

  ~SFlyerVec();
  void destroy_elements();
};

// `include/rstl/reserved_vector.hpp`'s spelling, with this element's destructor inlined.
inline void SFlyerVec::destroy_elements() {
  SFlyerElem* ptr = mData;
  for (int i = 0; i < mCount; ++i) {
    ptr[i].~SFlyerElem();
  }
}

inline SFlyerVec::~SFlyerVec() { destroy_elements(); }

extern "C" {
// The module's own unclaimed element teardown at 0x14E4 (0x50 bytes), called by fn_21_18D4 with
// the "destroy, do not free" flag. Declared, never defined here.
void fn_21_14E4(void* self, int flag);

// The imported base-class destructor, SwarmBasics's `fn_80_89F4` (module 80, 0x1B8 bytes). The
// name is the one the module's own import table carries, so no rename is needed.
void fn_80_89F4(void* self, int flag);

// The module's own vtable, `.data:0x4`, 0xC8 bytes, referenced but never defined: a class with a
// defined virtual would emit a `__vt__` of ours into `.data` and the module's sha1 would move.
extern char lbl_21_data_4[];

void* fn_21_192C(void* self, short flag);
void* fn_21_18D4(void* self, short flag);
void fn_21_1860(void* place, const SFlyerElem& src);
void fn_21_1840(void* place, const SFlyerElem& src);
SFlyerElem* fn_21_17D8(const SFlyerElem* first, int count, SFlyerElem* result);
void* fn_21_1794(void* self, const void* other);
void* fn_21_1708(void* self, short flag);

// .text 0x192C, 0x60 bytes. `CFlyerSwarm`'s deleting destructor, vtable entry 0x8 of
// `lbl_21_data_4`. The vptr store is written by hand rather than left to a `~CFlyerSwarm()` body
// for the reason `CFogOverlayRel.cpp` records: the class that defines the destructor emits the
// vtable, and an emitted `.data` object the split does not claim moves the module's sha1. The flag
// is a `short` and the return type is a pointer, the two things that decide the `extsh.` and the
// trailing `mr r3,r30`.
void* fn_21_192C(void* self, short flag) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_21_data_4;
    fn_80_89F4(self, 0);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// .text 0x18D4, 0x58 bytes. A second deleting destructor: the same chain link shape as
// `CMorphBall.cpp`'s `fn_800CD460`, with `this + 0x18` in place of the receiver.
void* fn_21_18D4(void* self, short flag) {
  if (self != nullptr) {
    fn_21_14E4(reinterpret_cast< char* >(self) + 0x18, -1);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
}

// .text 0x1888, 0x4C bytes. The element's out-of-line copy constructor, and the reason
// `symbols.txt` renames this function: six floats copied with two registers in flight, then the
// three words with the middle one through r5. It is the same body as
// `__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node` (`src/WorldFormat/CAreaOctTree_Tests.cpp`)
// instruction for instruction, which is what identifies it as the copy constructor rather than a
// free function.
SFlyerElem::SFlyerElem(const SFlyerElem& other)
: x00(other.x00)
, x04(other.x04)
, x08(other.x08)
, x0C(other.x0C)
, x10(other.x10)
, x14(other.x14)
, x18(other.x18)
, x1C(other.x1C)
, x20(other.x20) {}

extern "C" {
// .text 0x1860, 0x28 bytes. `rstl::construct_impl` for the element. The `cmplwi r3,0 / beq` is the
// `new` expression's own null test, not an explicit `if`: written as `if (place) call(place, src)`
// the same ten instructions come out, and writing it with the placement `new` is what keeps the
// call site honest about which function it reaches.
void fn_21_1860(void* place, const SFlyerElem& src) { new (place) SFlyerElem(src); }

// .text 0x1840, 0x20 bytes. `rstl::construct`'s forwarder, whose twin is `__sys_free`.
void fn_21_1840(void* place, const SFlyerElem& src) { fn_21_1860(place, src); }

// .text 0x17D8, 0x68 bytes. The element-wise copy loop, `uninitialized_copy_n` for a 0x24-byte
// element: the twin is `fn_80248EA4` (`src/WorldFormat/CMetroidAreaCollider.cpp`) with its stride,
// and the loop is entered at the bottom test so a zero count copies nothing and still returns the
// end cursor.
SFlyerElem* fn_21_17D8(const SFlyerElem* first, int count, SFlyerElem* result) {
  const SFlyerElem* it = first;
  SFlyerElem* cur = result;
  for (int remaining = count; remaining != 0; --remaining, ++it, ++cur) {
    fn_21_1840(cur, *it);
  }
  return cur;
}

// .text 0x1794, 0x44 bytes. The vector's copy constructor, the twin of `fn_80248E60` and of
// `Carve80004C4C.c`'s `fn_80004C90`. **The count is read out of the destination**, not out of the
// source value already in r0 - retail's `lwz r4,0x0(r31)` after the store is the measurement, and
// passing the source's count instead moves the load four instructions and breaks the module's
// bytes.
void* fn_21_1794(void* self, const void* other) {
  SFlyerVec* dst = static_cast< SFlyerVec* >(self);
  const SFlyerVec* from = static_cast< const SFlyerVec* >(other);
  dst->mCount = from->mCount;
  fn_21_17D8(from->mData, dst->mCount, dst->mData);
  return self;
}

// .text 0x1708, 0x8C bytes. The vector's deleting destructor: the element teardown loop, then the
// free behind the flag. **The one function in this range with no reference anywhere in the
// module** - not from the module's code and not from its `.data`, which is why `config.yml` lists
// it in FlyerSwarm's `force_active` and the ldscript's own FORCEACTIVE list does not.
void* fn_21_1708(void* self, short flag) {
  SFlyerVec* v = static_cast< SFlyerVec* >(self);
  if (v != nullptr) {
    v->destroy_elements();
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
}
#endif // __MWERKS__
