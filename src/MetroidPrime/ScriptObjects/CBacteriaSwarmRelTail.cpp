// CBacteriaSwarmRelTail.cpp - BacteriaSwarm's (module 6) out-of-line template tail, .text
// 0x4150..0x42E8: six functions the module's own translation units emitted out of line, all of them
// the machinery one element array of 0x24-byte records goes through:
//
//   0x4150 fn_6_4150  0x44  the container's copy constructor: the count at +0x0 stored from the
//                              source, then the element array copied by fn_6_4194
//   0x4194 fn_6_4194  0x68  the element-wise copy loop (count first, both cursors by 0x24), returning
//                              the end cursor
//   0x41FC fn_6_41FC  0x20  `rstl::construct` for one element: a frame and one call, nothing else
//   0x421C fn_6_421C  0x28  `rstl::construct_impl` for one element: the destination null test and
//                              the record copy
//   0x4244 fn_6_4244  0x4C  the record's copy constructor: six floats at +0x0..+0x14 and three words
//                              at +0x18, +0x1C and +0x20, i.e. `CAreaOctTree::Node`
//   0x4290 fn_6_4290  0x58  the record container's deleting destructor: teardown of the member at
//                              +0x18 with the -1 flag, then `CMemory::Free(self)` on the flag
//
// **The claim starts at 0x4150, not at 0x40C4 where the seeder's run starts.** `fn_6_40C4`
// (0x40C4, 0x8C) is a `~reserved_vector()` instantiation and is a measured wall: this compiler
// reproduces it instruction for instruction and gives the loop's induction variable and peeled trip
// count the other two registers (r5/r3 against retail's r3/r5), and MW's allocator does not move
// between the eight `GC/*` compilers or across the ten source spellings measured in
// `docs/goal-notes/progress-twin-rel-sandworm.md`. Claiming a range the object does not reproduce
// takes those bytes out of the module and breaks its sha1, so `fn_6_40C4` and `fn_6_3570`'s tail
// stay retail's, and dtk's `auto_00_000000A0_text` is cut in two around this claim - the
// arrangement `ScriptCoin`, `Metaree` and `CRipperForwarders.cpp` already use.
//
// **Names are the module's own and are reproduced verbatim, so every definition has to stay C**:
// `config/G2ME01/rels/BacteriaSwarm/symbols.txt` carries the `fn_<addr>` placeholders, and the
// linked module's symbol table is part of the file the sha1 covers, so a C++ definition here would
// mangle to `_Z<len>fn_6_4150...` and put a name where retail has none. That is the arrangement
// `CSandBossRelTail.cpp`, `CSandwormRelTail.cpp` and `Player/Carve80004C4C.c` use, and it is why
// `fn_6_40C4` could not have been written as the template instantiation it is in retail: the
// mangled name would not match.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses.
// `fn_6_4150` and `fn_6_4290` are called from the module's own retail bytes (0x409C, 0x38E0,
// 0x39A8, 0x3AE8, 0x51B4, 0x51C4), so their names have to be the ones those bytes reference, and
// every call this object makes - `fn_6_4194`, `fn_6_41FC`, `fn_6_421C`, `fn_6_4244` inside this
// unit, `fn_6_3EA0` and `Free__7CMemoryFPCv` outside it - is a module-local or imported name a host
// build would not have.
//
// **The six are in the module's `force_active` list** (`config/G2ME01/config.yml`): `fn_6_4194`,
// `fn_6_41FC`, `fn_6_421C` and `fn_6_4244` are reachable only from this object, and without the
// list mwldeppc dead-strips them and the module links short. Same arrangement, and same reason, as
// `CSandBossRelTail.cpp`'s two and `CSandwormRelTail.cpp`'s six.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it. Every body
// is read off `build/G2ME01/BacteriaSwarm/asm/auto_00_000000A0_text.s`.

#ifdef __MWERKS__

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`, the DOL's own function. Declared under
 *  retail's emitted spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x3EA0, 0x50: the record's own teardown, `fn_6_4290` calls it with the receiver's word at
 *  +0x18 and the literal -1. It is retail's, so it stays in `auto_00_000000A0_text`; declared and
 *  never defined here. Its twin is `__dt__80004B9C` in `src/MetroidPrime/Player/Carve80004B9C.c`. */
extern "C" void fn_6_3EA0(void* self, int deleting);

/** The 0x24-byte record the whole range copies: six floats and three words, which is
 *  `CAreaOctTree::Node` - `NESTED_CHECK_SIZEOF(CAreaOctTree, Node, 0x24)` in
 *  `include/WorldFormat/CAreaOctTree.hpp`. The members are named by offset rather than by retail's
 *  names: the tree declares `CAABox mAabb; const void* mPtr; const CAreaOctTree& mOwner;
 *  ETreeType mNodeType;`, and spelling them out would drag a `CAreaOctTree` in for a function that
 *  only copies them, so the shape that matters - six `lfs`/`stfs` pairs in retail's two-deep
 *  pipeline, then three `lwz`/`stw` - is written on offsets. `CAreaOctTree::Node`'s own copy
 *  constructor is the measured twin of `fn_6_4244` and is emitted implicitly in
 *  `src/WorldFormat/CAreaOctTree_Tests.cpp` (`__ct__Q212CAreaOctTree4NodeFRCQ212CAreaOctTree4Node`,
 *  weak), so there is no hand-written source to copy; `fn_55_106E0` in
 *  `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp` is the same member-for-member copy over
 *  another 0x2C record and matches byte for byte, which is where the statement order comes from. */
struct SFn6_4244 {
  float f00;
  float f04;
  float f08;
  float f0c;
  float f10;
  float f14;
  unsigned int w18;
  unsigned int w1c;
  unsigned int w20;
};

/** 0x4290, 0x58: the deleting destructor of the record's owner - receiver guard, the record's
 *  teardown at +0x18 with the literal -1, the sign-extended flag test, then
 *  `CMemory::Free(self)`; it returns the receiver. This is `fn_800CD460` in
 *  `src/MetroidPrime/Player/CMorphBall.cpp:501` word for word, the callee being `fn_6_3EA0` here and
 *  `fn_800CD4B8` there, so it is written as that body is written: **the flag is a `short`**
 *  (retail's test is `extsh.`) and the flag test sits **inside** `if (self)`, or the `beq` lands on
 *  the `extsh.` instead of on the epilogue. */
extern "C" void* fn_6_4290(void* self, short deleting) {
  if (self) {
    fn_6_3EA0(static_cast< char* >(self) + 24, -1);
    if (deleting > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x4244, 0x4C: the record's copy constructor. 19 instructions, no frame and no saved registers -
 *  the whole body is the six floats and the three words, loaded and stored in the order retail's
 *  two-deep load/store pipeline uses. */
extern "C" void fn_6_4244(SFn6_4244* self, const SFn6_4244* other) {
  self->f00 = other->f00;
  self->f04 = other->f04;
  self->f08 = other->f08;
  self->f0c = other->f0c;
  self->f10 = other->f10;
  self->f14 = other->f14;
  self->w18 = other->w18;
  self->w1c = other->w1c;
  self->w20 = other->w20;
}

/** 0x421C, 0x28: `rstl::construct_impl` for one element - the null test is on the **destination**
 *  (`cmplwi r3,0x0`), and the record copy is called on it. The twin is
 *  `construct_impl<CPASAnimState>`, i.e. `rstl::construct_impl< CPASAnimState >(dest, src)` folded
 *  into `fn_8002E4B8` at `src/MetroidPrime/CAnimData.cpp:145`, and `fn_80248DDC` in
 *  `src/WorldFormat/CMetroidAreaCollider.cpp` is the same nine instructions written as
 *  `if (dest) { ... }`. */
extern "C" void fn_6_421C(void* dest, const SFn6_4244& src) {
  if (dest) {
    fn_6_4244(static_cast< SFn6_4244* >(dest), &src);
  }
}

/** 0x41FC, 0x20: `rstl::construct` for one element - a frame and one unconditional call, no load
 *  and no test. The shape is `fn_80004C4C` in `src/MetroidPrime/Player/Carve80004C4C.c` (and
 *  `__sys_free`, 0x80008A28, in `src/MetroidPrime/main.cpp`), eight instructions apart from the
 *  `bl` target. */
extern "C" void fn_6_41FC(void* dest, const SFn6_4244& src) { fn_6_421C(dest, src); }

/** 0x4194, 0x68: the element-wise copy, `rstl::uninitialized_copy_n` for the 0x24-byte element:
 *  `(first, n, result)` in r3/r4/r5, the three cursors in r31/r30/r29, the loop entered at its
 *  **bottom** test, the cursors advanced after the construct, and the **end cursor returned**. The
 *  twin is `fn_80248EA4` (`src/WorldFormat/CMetroidAreaCollider.cpp:915`) and
 *  `fn_80004CD4` (`src/MetroidPrime/Player/Carve80004C4C.c`), the same 26 instructions with a
 *  different stride; a zero count copies nothing and still returns `result`. */
extern "C" void* fn_6_4194(const void* first, int n, void* result) {
  const unsigned char* it = static_cast< const unsigned char* >(first);
  unsigned char* cur = static_cast< unsigned char* >(result);
  for (int remaining = n; remaining != 0; --remaining, it += 0x24, cur += 0x24) {
    fn_6_41FC(cur, *reinterpret_cast< const SFn6_4244* >(it));
  }
  return cur;
}

/** The container of the array: the element count at +0x0 and the elements themselves at +0x4.
 *  `fn_6_4194` above is `rstl::reserved_vector`'s `uninitialized_copy_n` over that array; only the
 *  one word at +0x0 is read here, so the array is reached by the `+ 4` below rather than with a
 *  member - spelling it as a `SFn6_4244` array would drag the element type into a function whose
 *  only data is the count. */
struct SFn6_4150 {
  int mCount;
};

/** 0x4150, 0x44: the container's copy constructor, `rstl::reserved_vector`'s own: the same 17
 *  instructions as `fn_80248E60` (`src/WorldFormat/CMetroidAreaCollider.cpp:950`) and
 *  `fn_80004C90` (`src/MetroidPrime/Player/Carve80004C4C.c`), which are this function over other
 *  element types. **The bound is `self->mCount`, not `other->mCount`**: retail loads the source's
 *  word once, stores it, and then *re-loads it from the destination* (`lwz r4,0(r31)`) to pass as
 *  the count - and that reload is what puts `self` in r31 and the two cursors in r3/r5. */
extern "C" SFn6_4150* fn_6_4150(SFn6_4150* self, const SFn6_4150* other) {
  self->mCount = other->mCount;
  fn_6_4194(reinterpret_cast< const unsigned char* >(other) + 4, self->mCount,
            reinterpret_cast< unsigned char* >(self) + 4);
  return self;
}

#endif