// CPlantScarabSwarmTail.cpp - PlantScarabSwarm's (module 49) out-of-line template tail, .text
// 0x2C00..0x2F2C: seven functions the module emitted out of line, every one of them a class's
// implicit copy constructor, a `rstl::single_ptr` teardown or a `rstl::vector` instantiation.
//
//   0x2C00 fn_49_2C00  0x54  two-word own-pointer teardown: free the pointer at +0x0, then the
//                            object itself when the deleting flag is positive
//   0x2C54 fn_49_2C54  0x58  the same teardown one level down: `__dt__10CModelDataFv(ptr, 1)`
//                            then `CMemory::Free(self)`
//   0x2CAC fn_49_2CAC  0x94  `CActorParameters`' copy constructor (0x60 bytes): the 0x3C-byte
//                            `CLightParameters` member by an out-of-line call to 0x2D40, then
//                            0x3C..0x5C copied member for member
//   0x2D40 fn_49_2D40  0x7C  `CLightParameters`' copy constructor (0x3C bytes), member for member
//   0x2DBC fn_49_2DBC  0xA4  `rstl::vector<int>`'s `reserve`, i.e. the 4-byte-element block's
//   0x2E60 fn_49_2E60  0xAC  the 0x24-byte-element block's `reserve`; both allocate, copy the
//                            live elements across, destroy the old ones, free and re-store
//   0x2F0C fn_49_2F0C  0x20  the two-pointer forwarder to the 0x24-element block's `destroy_impl`
//                            at 0x2F2C, which stays retail's
//
// The 0x2F2C..0x32A0 remainder of the module's `.text` (the `destroy_impl`, the element copy and
// the class code above them) is left unclaimed, so dtk fills it from retail; the module's sha1
// against `config/G2ME01/config.yml` holds with these seven claimed and everything else retail's.
// Every body is read off `build/G2ME01/PlantScarabSwarm/asm/auto_00_000000D8_text.s`, and every
// name is the module's own, from `config/G2ME01/rels/PlantScarabSwarm/symbols.txt`.
//
// **`mw_version="GC/2.7"` is load-bearing and measured**, the same finding `CSandBossRelTail.cpp`
// and `CLumiteRelTail.cpp` record for their modules' tails: the module default GC/1.3.2 schedules
// these copies as a plain load/store ladder, 2.7 emits retail's 2-deep "load the next member
// before storing the previous one" pipeline. `fn_49_2D40` is 0 of its 31 words under 2.7 and 28 of
// 31 under 1.3.2; nothing about the source changes between the two.
//
// **The 0x24-byte-element `reserve` is the same body as `fn_801FF5A0`** in
// `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (its DOL twin, 0xAC there against 0xAC here,
// instruction for instruction apart from the callee names), so this file's block, its one-word
// iterator and the `uninitialized_copy` note are that file's, with this module's callees. The
// 4-byte-element `reserve` is `fn_801466F4`'s twin in the DOL and `reserve__Q24rstl36vector<i,
// Q24rstl17rmemory_allocator>Fi`'s in `src/MetroidPrime/CWorld.cpp`; its copy loop is *inlined*
// there and here, which is why it takes an iterator class and an inlined `uninitialized_copy`
// rather than the raw-pointer loop - retail's four stores holding the two by-value iterator
// arguments at r1+0x8/0xc and r1+0x10/0x14 only come out of a class argument, and the
// register allocation (`in` in r5, `end` in r3, `out` in r4) follows from it. A raw-pointer loop
// compiles to the same 41 instructions minus those four stores and 0x10 of frame.
//
// **`fn_49_2CAC`'s `mr r3,r30` after the `bl`** is retail keeping `this` in r3 across the call,
// which a `void`-returning free function does not do: the function has to return the receiver
// (`return self;`), and then mwcceppc reloads r3 after the callee clobbers it and uses r4 as the
// copy's second temporary where a `void` version uses r3. That one instruction is the whole
// difference between 0 of 37 words and 15 of 37.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.
//
// The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, the arrangement
// `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` use, so listing this file in `files.cmake` adds
// no undefined reference - every call here is to a module-local symbol (`fn_49_2F2C`,
// `fn_49_2FBC`) or to a DOL global the port already links.

#include "types.h"

/** 0x802FDAB8, `symbols.txt`: `rstl::rmemory_allocator::allocate(int)`. Declared under retail's
 *  own emitted spelling so the call needs no header. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`, claimed by `Kyoto/Alloc/CMemory.cpp`
 *  in the DOL. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x800E6810, `symbols.txt`: `CModelData`'s deleting destructor, the DOL's own. This module's
 *  copy at 0x2C54 is `rstl::single_ptr<CModelData>`'s teardown, which reaches it with the
 *  deleting flag set. */
extern "C" void __dt__10CModelDataFv(void* self, int flag);

/** 0x2F2C, 0x4C: this module's own `destroy_impl` for the 0x24-byte-element block, left
 *  unclaimed. `fn_49_2F0C` forwards to it. */
extern "C" void fn_49_2F2C(char* begin, char* end);

/** One pointer of the 0x24-byte block's element iterator, built by a converting constructor so
 *  that the by-value arguments of the call below come out as retail's own two 8-byte slots. */
struct SStateIter {
  void* mCur;
  SStateIter(void* cur) : mCur(cur) {}
};

/** The 0x2FBC, 0xAC copy of `uninitialized_copy` for that block, left unclaimed. Its signature is
 *  the by-value iterator pair plus the destination, which is what fixes the argument registers
 *  (`addi r3,r1,0x14`, `addi r4,r1,0xc`, `mr r5,r31`). */
extern "C" void* fn_49_2FBC(SStateIter begin, SStateIter end, void* dst);

/** The 0x24-byte element block: `x04` the element count, `x08` the capacity, `x0c` the base.
 *  Nothing reads `x00`, so nothing is asserted about it. */
struct SBlock24 {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

/** The 4-byte element block, the same three words at the same offsets. */
struct SBlock4 {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

/** The 4-byte block's element iterator: one pointer, passed by value. */
struct SIntIter {
  int* mCur;
  SIntIter(int* cur) : mCur(cur) {}
  bool operator!=(const SIntIter& other) const { return mCur != other.mCur; }
  SIntIter& operator++() {
    ++mCur;
    return *this;
  }
  int& operator*() const { return *mCur; }
};

/** `rstl::uninitialized_copy` over that iterator, inlined at its one call site - which is the
 *  shape `include/rstl/construct.hpp:117`'s template has, and the reason the iterator class above
 *  exists at all. */
static inline int* CopyInts(SIntIter begin, SIntIter end, int* out) {
  int* tmp = out;
  SIntIter cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    *tmp = *cur;
  }
  return tmp;
}

/** `CLightParameters` (0x3C bytes) as its copy's bytes read it out. */
struct SLightParms {
  bool mCastShadow;       // x0
  float mShadowScale;     // x4
  int mShadowTesselation; // x8
  float mShadowAlpha;     // xc
  float mMaxShadowHeight; // x10
  u32 mAmbientColor;      // x14
  u8 mFlags;              // x18
  int mUseWorldLighting;  // x1c
  int mLightRecalc;       // x20
  int mUseLightSet;       // x24
  float mOffsetX;         // x28
  float mOffsetY;         // x2c
  float mOffsetZ;         // x30
  int mMaxDynamicLights;  // x34
  int mMaxAreaLights;     // x38
};

/** `CActorParameters` (0x60 bytes) as its copy's bytes read it out: the 0x3C-byte
 *  `CLightParameters` at +0x0, then 0x3C..0x5C. */
struct SActorParms {
  SLightParms mLighting; // x0
  u32 mScannable;        // x3c
  u32 mEchoModel;        // x40
  u32 mEchoSkin;         // x44
  u32 mDarkModel;        // x48
  u32 mDarkSkin;         // x4c
  u32 mVisor;            // x50
  u8 mMaxVolume;         // x54
  u8 mMaxEchoVolume;     // x55
  u8 mFlags;             // x56
  float mFadeInTime;     // x58
  float mFadeOutTime;    // x5c
};

#ifdef __MWERKS__

/** 0x2F0C, 0x20: the forwarder both pointers go across in r3/r4 with no shuffling, the shape
 *  `Carve801FF5A0.cpp`'s `fn_801FF64C` has. */
extern "C" void fn_49_2F0C(char* begin, char* end) { fn_49_2F2C(begin, end); }

/** 0x2E60, 0xAC: the 0x24-byte block's `reserve`. `capacity <= x08` returns without allocating;
 *  otherwise it allocates `capacity * 0x24`, moves the live elements across with the module's own
 *  `fn_49_2FBC`, destroys the old ones through `fn_49_2F0C`, frees the old buffer and stores the
 *  new buffer and capacity back. Both ends are re-read from `self` after the copy instead of being
 *  held in locals: keeping them costs a fifth live register, `stmw r27,28(r1)` and a 0x90-byte
 *  frame, which is the same trade `Carve801FF5A0.cpp` records for its twin. */
extern "C" void fn_49_2E60(SBlock24* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  char* const buffer =
      static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 36));
  fn_49_2FBC(SStateIter(self->x0c),
             SStateIter(static_cast< char* >(self->x0c) + self->x04 * 36), buffer);
  fn_49_2F0C(static_cast< char* >(self->x0c),
             static_cast< char* >(self->x0c) + self->x04 * 36);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}

/** 0x2DBC, 0xA4: `rstl::vector<int>`'s `reserve` - the same three words, a 4-byte stride and an
 *  inlined element copy. Retail's four stores of the two iterators on its own frame are what the
 *  by-value `SIntIter` arguments produce, and the register allocation that follows from them. */
extern "C" void fn_49_2DBC(SBlock4* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  int* const buffer = static_cast< int* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 4));
  CopyInts(SIntIter(static_cast< int* >(self->x0c)),
           SIntIter(static_cast< int* >(self->x0c) + self->x04), buffer);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}

/** 0x2D40, 0x7C: `CLightParameters`' copy constructor, the 0x3C bytes copied member for member
 *  with the byte at +0x18 copied whole - the class's three consecutive `bool : 1` fields at that
 *  offset are collapsed by the compiler into one `lbz`/`stb` pair, which a hand-written field-by-
 *  field copy of three one-bit members would not produce. */
extern "C" void fn_49_2D40(SLightParms* self, const SLightParms* other) {
  self->mCastShadow = other->mCastShadow;
  self->mShadowScale = other->mShadowScale;
  self->mShadowTesselation = other->mShadowTesselation;
  self->mShadowAlpha = other->mShadowAlpha;
  self->mMaxShadowHeight = other->mMaxShadowHeight;
  self->mAmbientColor = other->mAmbientColor;
  self->mFlags = other->mFlags;
  self->mUseWorldLighting = other->mUseWorldLighting;
  self->mLightRecalc = other->mLightRecalc;
  self->mUseLightSet = other->mUseLightSet;
  self->mOffsetX = other->mOffsetX;
  self->mOffsetY = other->mOffsetY;
  self->mOffsetZ = other->mOffsetZ;
  self->mMaxDynamicLights = other->mMaxDynamicLights;
  self->mMaxAreaLights = other->mMaxAreaLights;
}

/** 0x2CAC, 0x94: `CActorParameters`' copy constructor. The `CLightParameters` member at +0x0 is
 *  copy-constructed by an out-of-line call to 0x2D40 - not inlined, which is why the two are two
 *  functions here as in retail - and 0x3C..0x5C then go member for member.
 *
 *  **It returns the receiver, and that is load-bearing**: retail reloads r3 with `this` after the
 *  call (`mr r3,r30`) and uses r4 as the copy's second temporary because r3 is live. Spelled
 *  `void`, both are gone and 15 of 37 words differ. */
extern "C" SActorParms* fn_49_2CAC(SActorParms* self, const SActorParms* other) {
  fn_49_2D40(&self->mLighting, &other->mLighting);
  self->mScannable = other->mScannable;
  self->mEchoModel = other->mEchoModel;
  self->mEchoSkin = other->mEchoSkin;
  self->mDarkModel = other->mDarkModel;
  self->mDarkSkin = other->mDarkSkin;
  self->mVisor = other->mVisor;
  self->mMaxVolume = other->mMaxVolume;
  self->mMaxEchoVolume = other->mMaxEchoVolume;
  self->mFlags = other->mFlags;
  self->mFadeInTime = other->mFadeInTime;
  self->mFadeOutTime = other->mFadeOutTime;
  return self;
}

/** 0x2C54, 0x58: the same teardown one level down - `rstl::single_ptr<CModelData>`'s deleting
 *  destructor. The pointer at +0x0 goes through `CModelData`'s deleting destructor with the
 *  deleting flag set (no null test: retail has none), then the object itself is freed when the
 *  incoming flag is positive. The flag is a `short` (`extsh.`, not the `extsb.` a `bool` gives),
 *  and `flag > 0` sits *inside* `if (self)` so the `beq` lands on the epilogue. */
extern "C" void* fn_49_2C54(void** self, short flag) {
  if (self != 0) {
    __dt__10CModelDataFv(*self, 1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x2C00, 0x54: the two-word own-pointer teardown with a free of the pointee whose type has
 *  nothing to destroy in front of it, so the delete half is `CMemory::Free` alone. Twin
 *  `__dt__Q24rstl68single_ptr<...>Fv` at 0x802836A4 in the DOL, 0x54 bytes there too. */
extern "C" void* fn_49_2C00(void** self, short flag) {
  if (self != 0) {
    Free__7CMemoryFPCv(*self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif // __MWERKS__
