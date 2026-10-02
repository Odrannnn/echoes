// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:77-80`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80003BE8_text.s:769-896`, and the bodies below are the C
// those bytes are the compilation of.
//
// .text 0x800045A0..0x80004744, 0x1A4 = 420 bytes, 4 functions:
//
//   fn_800045A0    0x800045A0  0x54    21 instructions
//   fn_800045F4    0x800045F4  0x84    33 instructions
//   fn_80004678    0x80004678  0x58    22 instructions
//   fn_800046D0    0x800046D0  0x74    29 instructions
//
// **It is one deleting-destructor chain, each step calling the next.**  All four are the shape
// `if (self) { <member teardown>; if (flag > 0) Free__7CMemoryFPCv(self); } return self;`, that is
// MWCC's deleting-destructor convention: `mr. r30,r3 / beq` guards the receiver, `li r4,-1` is
// the "do not free me afterwards" flag handed to the member's own destructor, and only a positive
// `flag` reaches `Free__7CMemoryFPCv`.  Read off the call edges and the argument registers:
//
//   fn_800045A0  destroys the object at +0 by calling fn_800045F4 with `li r4,-1`.
//   fn_800045F4  is `rstl::vector<T>::~vector()` for a 12-byte `T`: `lwz r0,4(self)` is the count
//                and `lwz r3,0xc(self)` the item array (`include/rstl/vector.hpp:18-21` is
//                `mAllocator, mCount, mCapacity, mItems`), `mulli ...,0xc` is the element stride,
//                the empty `cmplw r4,r0` loop is `destroy(begin(), end())` for a trivially
//                destructible `T` (`include/rstl/construct.hpp:100-112` - the loop survives even
//                though its body inlines to nothing), and the first `bl Free__7CMemoryFPCv` is
//                `mAllocator.deallocate(mItems)`.  The four stores at `r1+0x14/0x08/0x10/0x0C`
//                that look like dead writes are the inliner's two by-value `pointer_iterator`
//                home slots: each holds the pointer twice, once for `destroy`'s parameter and
//                once for `destroy_impl`'s.  Removing them (a plain pointer loop) is 16 bytes
//                short and does not match - measured.
//   fn_80004678  destroys the object at +4 by calling fn_800046D0 with `li r4,-1`.
//   fn_800046D0  is the 3-node binary tree's destructor (`rstl::map`/`red_black_tree`): the
//                `lwz r4,0x10(r30) / cmplwi / beq` is the root (`+0x10`), the call is
//                `bl fn_80008D68` - whose caller list in `src/MetroidPrime/main.cpp:254-255`
//                names this very address, 0x80004700, as one of its two call sites - and the four
//                `li r0,0 / stw` pairs clear the root and the tree's three counters in retail's
//                order `+0x10, +0x8, +0xC, +0x4`.  The doubled `beq` at 0x800046EC/0x800046F0 is
//                the receiver guard followed by the implicit member destructor's own guard, the
//                same dead pair `src/MetroidPrime/Carve80193E08.c:27` describes.
//
// Two of the four are named by an outside caller, and it agrees with the shapes above:
// `src/MetroidPrime/Player/CGameStateStreamCtor.cpp` records, in its own table of the REL24
// slots of `fn_80144140` (`CGameState::CGameState(CInputStream&, int)`), call slots at `+0x384`
// to `fn_800045A0` and at `+0x42C` to `fn_80004678` (lines 85 and 90), and its line 234 calls
// them "Three destructors of 0x14-byte stack temporaries: `~T(t, -1)`" (line 235 declares
// `fn_800045A0`, line 237 `fn_80004678`).  That file is **not registered** - not in
// `files.cmake`, not in `configure.py` - so it is evidence, not a link participant.  The objects
// it says these two destroy are 0x14 bytes of stack: `fn_800045A0`'s is a `rstl::vector`
// (`mAllocator`/`mCount`/`mCapacity`/`mItems` = 0x10) plus one word, and `fn_80004678`'s is a
// tree header at +4.
//
// Each body is a byte-shape twin of an already-matched function of the same shape, and the twins
// are why the operands above are readable rather than guessed: fn_800045A0 is
// `__dt__19CStaticInterferenceFv` (0x80009460, `src/MetroidPrime/main.cpp:2210`), fn_800045F4 is
// `__dt__Q24rstl42vector<6CRelay,Q24rstl17rmemory_allocator>Fv` (0x800501F4,
// `src/MetroidPrime/CWorld.cpp`), fn_80004678 is
// `__dt__Q24rstl38bit_vector<Q24rstl17rmemory_allocator>Fv` (0x80009124, same file) and
// fn_800046D0 is `__dt__Q24rstl209map<...>Fv` (0x801F09D0,
// `src/MetroidPrime/CRELFileManager.cpp`).  Disassembled beside retail's range each pair differs
// only in the `bl` displacements, which are address-relative.
//
// The two callees are declared, never defined here: `fn_80008D68` is inside `main.cpp`'s claim
// (0x800053B8..0x80009880) and is written at `src/MetroidPrime/main.cpp:301`, and
// `Free__7CMemoryFPCv` (0x802CE388) is claimed by `Kyoto/Alloc/CMemory.cpp`.  So the eight `bl`s
// below resolve to our own objects' symbols, and this unit claims `.text` and nothing else.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour belongs to
// this chain: `fn_80004510` (0x80004510, 0x90) below is a *different* destructor - it walks a
// count at +0 in strides of 8 calling `ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv` on each
// element, which is an inlined `rstl::vector<rc_ptr<T>>::~vector()` - and `fn_80004744`
// (0x80004744, 0x54) above has no element walk at all: it frees the pointer at +0xC, so it is a
// teardown of a different object.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/CMainResetGameState.cpp` (`.text` 0x80003A48..0x80003BE8) and the one
// above is `MetroidPrime/Player/CGameStateBlockDtor.cpp` (0x80004A4C..0x80004AA0), so this
// address sits in the `MetroidPrime/` neighbourhood.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x80008D68, `symbols.txt:180`, size 0x80: `rstl::red_black_tree`'s recursive node teardown.
 *  Written at `src/MetroidPrime/main.cpp:301` as `extern "C" void fn_80008D68(void* self,
 *  SNode* node)`; declared, never defined here. */
extern void fn_80008D68(void* self, void* node);

/** The 12-byte element `fn_800045F4` walks in strides of 0xC, and the array header it walks.
 *  Only the two fields the bytes read are modelled: `+0x4` is the count (`mulli ...,0xC` and the
 *  `cmplw` bound) and `+0xC` the item array, which is `rstl::vector`'s `mItems` after its
 *  `mAllocator`/`mCount`/`mCapacity` (`include/rstl/vector.hpp:18-21`).  The element's own size is
 *  what the stride fixes; nothing here reads its fields. */
struct SElem12 {
  int m0;
  int m4;
  int m8;
};

struct SVec12 {
  int mAllocator;
  int mCount;
  int mCapacity;
  struct SElem12* mItems;
};

/** One 4-byte `rstl::pointer_iterator` (`include/rstl/pointer_iterator.hpp:58` is its single
 *  `T* current`), named only so the by-value parameter list of the `destroy` chain - which is
 *  what emits the four home-slot stores - can be written in C. */
struct SIt {
  struct SElem12* current;
};

static inline void DestroyElem(struct SElem12* p) {}

static inline void DestroyImpl(struct SIt begin, struct SIt end) {
  struct SElem12* cur = begin.current;
  struct SElem12* last = end.current;
  for (; cur != last; ++cur) {
    DestroyElem(cur);
  }
}

static inline void Destroy(struct SIt begin, struct SIt end) { DestroyImpl(begin, end); }

/** The tree object `fn_800046D0` tears down: the root at `+0x10` and the three counters the
 *  bytes clear.  Its node layout is `src/MetroidPrime/main.cpp:278-285`'s `SNode`. */
struct STree {
  int m0;
  int m4;
  int m8;
  int mC;
  void* mRoot;
};

void* fn_800046D0(struct STree* self, short flag);
void* fn_80004678(void* self, short flag);
void* fn_800045F4(struct SVec12* self, short flag);
void* fn_800045A0(void* self, short flag);

void* fn_800046D0(struct STree* self, short flag) {
  if (self) {
    if (self) {
      if (self->mRoot) {
        fn_80008D68(self, self->mRoot);
      }
      self->mRoot = 0;
      self->m8 = 0;
      self->mC = 0;
      self->m4 = 0;
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80004678(void* self, short flag) {
  if (self) {
    fn_800046D0((struct STree*)((char*)self + 4), -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_800045F4(struct SVec12* self, short flag) {
  if (self) {
    struct SIt begin;
    struct SIt end;
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

void* fn_800045A0(void* self, short flag) {
  if (self) {
    fn_800045F4((struct SVec12*)self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
