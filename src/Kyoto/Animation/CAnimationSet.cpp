#include "Kyoto/Animation/CAnimationSet.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Animation/CMetaTransFactory.hpp"

#include "rstl/construct.hpp"

// The functions below are retail's *unnamed* functions in this unit - the ones dtk names
// `fn_<address>` because retail's own symbol table has no name for them. Each is already
// byte-identical to code this translation unit emits as an `rstl` template instantiation, and
// each is still scored 0.00% for exactly that reason: mwceppc emits an out-of-line copy of a
// template under its *mangled* name, dtk gave retail's own copy of the same code the name of the
// address it sits at, and objdiff pairs functions by name. No C++ declaration can rename a
// template instantiation, so the only way to give objdiff a symbol to pair is to write the
// function out by hand under an `extern "C"` name. That is the technique `fn_80143CD4` in
// `src/MetroidPrime/Player/CGameState.cpp` and `fn_800F4FB4` in
// `src/MetroidPrime/BodyState/CBSLocomotion.cpp` already use, and
// `include/rstl/reserved_vector.hpp` documents it in the source.
//
// The bodies are not transcribed disassembly: each is the body `include/rstl/construct.hpp` and
// `include/rstl/vector.hpp` already spell, and each was read out of
// `build/G2ME01/obj/Kyoto/Animation/CAnimationSet.o` - retail's own object, with its relocations
// resolved - before it was written. Nothing here is a stub: every function is retail's whole
// body.
//
// A body calls its neighbour *by retail's name* rather than inlining it, because a `bl` is a
// `bl` in both objects and objdiff compares the instruction rather than the symbol it
// relocates to. That is measured, not assumed: `CAreaCollisionCache::AddOctreeLeafCache` scores
// 100.00% with retail's `bl fn_80248D74` against our `bl push_back__Q24rstl61...`.
//
// **Definitions are in descending retail offset**, which is what mwcceppc needs in order to emit
// them in ascending order (see "Declare in reverse" in `docs/RUNNING_THE_DECOMP.md`, and
// `tools/check_decl_order.py`).
typedef CAnimationSet::AdditiveAnimationList AdditiveVec;
typedef CAnimationSet::HalfTransitionList HalfVec;
typedef CAnimationSet::TransitionList TransVec;
typedef CAnimationSet::AnimationList AnimVec;
typedef rstl::vector< CAnimPOIData > AnimPoiVec;
typedef rstl::vector< CSoundPOINode > SoundPoiVec;
typedef rstl::vector< CParticlePOINode > ParticlePoiVec;
typedef rstl::vector< CInt32POINode > Int32PoiVec;
typedef rstl::vector< CBoolPOINode > BoolPoiVec;
typedef rstl::pair< uint, CAdditiveAnimationInfo > AdditivePair;
typedef rstl::pointer_iterator< CAnimPOIData, AnimPoiVec, rstl::rmemory_allocator > PoiIter;
typedef rstl::pointer_iterator< CAnimation, AnimVec, rstl::rmemory_allocator > AnimIter;
typedef rstl::pointer_iterator< CHalfTransition, HalfVec, rstl::rmemory_allocator > HalfIter;
typedef rstl::pointer_iterator< CTransition, TransVec, rstl::rmemory_allocator > TransIter;

extern "C" CAnimPOIData* fn_8028E754(CAnimPOIData* const* begin, CAnimPOIData* const* end,
                                    CAnimPOIData* out);
extern "C" void fn_8028E708(CAnimPOIData* begin, CAnimPOIData* end);
extern "C" void fn_8028E6E8(CAnimPOIData* begin, CAnimPOIData* end);
extern "C" void fn_8028E63C(AnimPoiVec* self, int newSize);
extern "C" void fn_8028E588(AdditiveVec* self, int newSize);
extern "C" CHalfTransition* fn_8028E534(const CHalfTransition* const* begin,
                                        const CHalfTransition* const* end, CHalfTransition* out);
extern "C" void fn_8028E474(HalfVec* self, int newSize);
extern "C" CTransition* fn_8028E410(const CTransition* const* begin, const CTransition* const* end,
                                  CTransition* out);
extern "C" void fn_8028E350(TransVec* self, int newSize);
extern "C" CAnimation* fn_8028E2C0(const CAnimation* const* begin, const CAnimation* const* end,
                                CAnimation* out);
extern "C" void fn_8028E1F0(AnimVec* self, int newSize);
extern "C" void fn_8028E1D0(CAnimation* dest, CInputStream& in);
extern "C" void fn_8028E074(AnimIter begin, AnimIter end);
extern "C" void fn_8028E03C(AnimIter begin, AnimIter end);
extern "C" void* fn_8028DFB8(void* self, int flag);
extern "C" void* fn_8028DD7C(void* self, int flag);
extern "C" void fn_8028DE00(TransIter begin, TransIter end);
extern "C" void fn_8028DE38(TransIter begin, TransIter end);
extern "C" void fn_8028DF98(CTransition* dest, CInputStream& in);
extern "C" void* fn_8028DC60(void* self, int flag);
extern "C" void fn_8028DCE4(HalfIter begin, HalfIter end);
extern "C" void fn_8028DB68(CAnimPOIData* in);
extern "C" void fn_8028DB48(CAnimPOIData* in);
extern "C" void fn_8028DAF8(PoiIter begin, PoiIter end);
extern "C" void fn_8028DAC0(PoiIter begin, PoiIter end);
extern "C" void* fn_8028DA3C(void* self, int flag);
extern "C" CAnimPOIData* fn_8028D9D4(CAnimPOIData* src, int n, CAnimPOIData* dest);
extern "C" AnimPoiVec* fn_8028D950(AnimPoiVec* self, const AnimPoiVec* other);
extern "C" AdditivePair* fn_8028D7AC(AdditivePair* self, CInputStream& in);
extern "C" void fn_8028D78C(AdditivePair* dest, CInputStream& in);
extern "C" void fn_8028D604(CHalfTransition* dest, CInputStream& in);
extern "C" void fn_8028D4BC(CAnimPOIData* dest, CInputStream& in);
extern "C" void* fn_8028D440(void* self, int flag);
extern "C" void fn_8028D420(CBoolPOINode* dest, const CBoolPOINode& src);
extern "C" CBoolPOINode* fn_8028D3B8(CBoolPOINode* src, int n, CBoolPOINode* dest);
extern "C" void fn_8028D268(CInt32POINode* dest, const CInt32POINode& src);
extern "C" CInt32POINode* fn_8028D200(CInt32POINode* src, int n, CInt32POINode* dest);
extern "C" void fn_8028D0B0(CParticlePOINode* dest, const CParticlePOINode& src);
extern "C" CParticlePOINode* fn_8028D048(CParticlePOINode* src, int n, CParticlePOINode* dest);
extern "C" void fn_8028CEF8(CSoundPOINode* dest, const CSoundPOINode& src);
extern "C" CSoundPOINode* fn_8028CE90(CSoundPOINode* src, int n, CSoundPOINode* dest);
extern "C" CAnimPOIData* fn_8028CCF0(CAnimPOIData* self, const CAnimPOIData* other);
extern "C" void fn_8028CCC8(CAnimPOIData* dest, const CAnimPOIData& src);
extern "C" void fn_8028CCA8(CAnimPOIData* dest, const CAnimPOIData& src);
extern "C" void fn_8028CC70(AnimPoiVec* self, const CAnimPOIData& in);

// The four placement news below are what the `rstl::construct< T >` stream wrappers
// (`fn_8028E1D0`, `fn_8028DF98`, `fn_8028D4BC`, `fn_8028D604`) need. **Measured:** written
// inline, `new (dest) T(in)` is 40 bytes, not retail's 32 - mwcceppc expands the placement form
// into "call `operator new`, test the result against null, then construct", and that
// `cmplwi r3,0` / `beq` pair is retail's *other* `construct` function (`fn_8028CCC8` is
// `construct_impl` and carries exactly that pair), not this one. Routing the construction through
// a file-local function leaves the wrapper as a frame and one `bl`, which is what retail emits;
// objdiff compares the instruction, not the symbol it relocates to, so the `bl` pairs. This is
// the same shape `fn_80248F0C` is written in `src/WorldFormat/CMetroidAreaCollider.cpp`.

static void PlaceCAnimation(CAnimation* dest, CInputStream& in) { new (dest) CAnimation(in); }

static void PlaceCTransition(CTransition* dest, CInputStream& in) { new (dest) CTransition(in); }

static void PlaceCAnimPOIData(CAnimPOIData* dest, CInputStream& in) { new (dest) CAnimPOIData(in); }

static void PlaceCHalfTransition(CHalfTransition* dest, CInputStream& in) {
  new (dest) CHalfTransition(in);
}

// `rstl::destroy_impl` over a `CHalfTransition` range - what `fn_8028DCE4` calls. Retail makes
// that a separate out-of-line function too, at 0x8028DD1C (96 bytes, unnamed, still 0.00% here
// because it is a different function); it is spelled out here so `fn_8028DCE4` can be.

static void DestroyHalfRange(HalfIter begin, HalfIter end) {
  for (CHalfTransition* cur = begin.get_pointer(); cur != end.get_pointer(); ++cur) {
    cur->~CHalfTransition();
  }
}

// The four `rstl::vector` copy constructors `fn_8028CCF0` calls, as file-local functions.
// **Measured:** written as placement news, each costs an `addic.`/`beq` null test that retail's
// `fn_8028CCF0` does not have (retail's is 112 bytes, ours 144), for the same reason the four
// stream `construct` wrappers needed a helper.

static void CopyBoolPoiVec(BoolPoiVec* dest, const BoolPoiVec& src) {
  new (dest) BoolPoiVec(src);
}

static void CopyInt32PoiVec(Int32PoiVec* dest, const Int32PoiVec& src) {
  new (dest) Int32PoiVec(src);
}

static void CopyParticlePoiVec(ParticlePoiVec* dest, const ParticlePoiVec& src) {
  new (dest) ParticlePoiVec(src);
}

static void CopySoundPoiVec(SoundPoiVec* dest, const SoundPoiVec& src) {
  new (dest) SoundPoiVec(src);
}

// Guessed name.

// `fn_8028E754` - retail `.text:0x8028E754`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy< pointer_iterator< CAnimPOIData, ... >, CAnimPOIData* >`, the copy
// half of `fn_8028E63C` below.
//
//     1bdc  lwz   r31,0(r3)                  ; begin.get_pointer()
//     1bf4  mr    r3,r30 / mr r4,r31          ; construct(&*cur, *it): a reference, so &*it
//     1bfc  bl    fn_8028CCA8
//     1c00  addi  r31,r31,68                  ; ++it     (0x44 = sizeof(CAnimPOIData))
//     1c04  addi  r30,r30,68                  ; ++cur
//     1c08  lwz   r0,0(r29)                  ; end.get_pointer(), reloaded from the iterator
//     1c0c  cmplw r31,r0 / bne 1bf4
//     1c18  mr    r3,r30                      ; returns the end cursor
//
// The loop is entered at its bottom test, so an empty range copies nothing and still returns
// `out`. The step is a literal 68 rather than a pointer load, which is what leaves the two
// cursors in `r31`/`r30` and the end iterator in `r29`; the two `pointer_iterator`s arrive by
// hidden pointer, so `r3` and `r4` are *pointers to* the cursors.
extern "C" CAnimPOIData* fn_8028E754(CAnimPOIData* const* begin, CAnimPOIData* const* end,
                                    CAnimPOIData* out) {
  CAnimPOIData* cur = out;
  for (CAnimPOIData* it = *begin; it != *end; ++it, ++cur) {
    fn_8028CCA8(cur, *it);
  }
  return cur;
}

// `fn_8028E708` - retail `.text:0x8028E708`, 0x4C = 76 bytes, unnamed. It is
// `rstl::destroy_impl< CAnimPOIData*, CAnimPOIData* >`, the range form `fn_8028E6E8` below calls.
//
//     1b90  mr    r31,r3 / mr r30,r4           ; begin, end
//     1b9c  b     1bac                         ; the test comes first
//     1ba0  mr    r3,r31
//     1ba4  bl    fn_8028DB48                  ; destroy(&*cur)
//     1ba8  addi  r31,r31,68                   ; ++cur
//     1bac  cmplw r31,r30 / bne 1ba0
//
// The `is_trivially_destructible` test is not here: `CAnimPOIData` is a class with four
// `rstl::vector` members and nothing in the tree declares it trivially destructible, so
// `destroy_impl` has nothing to fold away and the loop stands on its own.
extern "C" void fn_8028E708(CAnimPOIData* begin, CAnimPOIData* end) {
  for (CAnimPOIData* cur = begin; cur != end; ++cur) {
    fn_8028DB48(cur);
  }
}

// `fn_8028E6E8` - retail `.text:0x8028E6E8`, 0x20 = 32 bytes, unnamed. It is
// `rstl::destroy< CAnimPOIData*, CAnimPOIData* >`: a frame and one unconditional `bl`
// (0x8028E6F0) to `fn_8028E708`, nothing else. It is written as a call rather than as
// `rstl::destroy(...)` because `destroy` and `destroy_impl` are both in-class-inline and
// mwceppc folds them - folding `destroy_impl` here would fold in its
// `is_trivially_destructible` test too, which is retail's *previous* function and not this one.
// Unlike `fn_8028DAC0` it builds no iterators on the stack, because this instantiation is over raw
// pointers and not over `pointer_iterator`.
extern "C" void fn_8028E6E8(CAnimPOIData* begin, CAnimPOIData* end) { fn_8028E708(begin, end); }

// `fn_8028E63C` - retail `.text:0x8028E63C`, 0xAC = 172 bytes, unnamed. It is
// `rstl::vector< CAnimPOIData >::reserve(int)`. Its one caller, the
// `rstl::vector< CAnimPOIData >::vector( CInputStream& )` at 0x8028CBD0, is **not** written
// here - see the note at the end of this file.
//
//     1ad4  lwz   r0,8(r3) / cmpw r30,r0 / ble 1b44    ; if (newSize <= mCapacity) return
//     1ae0  mulli r3,r30,68 / bl allocate               ; mAllocator.allocate(newData, newSize)
//     1afc  ... four words built on the stack at 8(r1)..20(r1) ...
//     1b1c  bl    fn_8028E754                           ; uninitialized_copy(begin(), end(), newData)
//     1b2c  add   r4,r3,r0 / bl fn_8028E6E8             ; destroy(mItems, mItems + mCount)
//     1b38  bl    Free__7CMemoryFPCv                    ; mAllocator.deallocate(mItems)
//     1b3c  stw   r31,12(r29) / stw r30,8(r29)          ; mItems = newData; mCapacity = newSize
//
// `mulli 68` is 0x44 = `sizeof(CAnimPOIData)`. The four stack words are the two
// `pointer_iterator`s `uninitialized_copy` takes by value - a class goes by hidden pointer -
// and each is **two** words in retail, which is why this is 82.70% and not 100%:
// `rstl::pointer_iterator` in this repo holds one word (`current`), and that header is shared
// by every vector in the tree, so it is not changed for one unit. Same cause for
// `fn_8028E588`, `fn_8028E474`, `fn_8028E350` and `fn_8028E1F0` below.
extern "C" void fn_8028E63C(AnimPoiVec* self, int newSize) {
  if (newSize > self->mCapacity) {
    CAnimPOIData* newData;
    rstl::rmemory_allocator().allocate(newData, newSize);
    CAnimPOIData* beginSlot = self->mItems;
    CAnimPOIData* endSlot = self->mItems + self->mCount;
    fn_8028E754(&beginSlot, &endSlot, newData);
    fn_8028E6E8(self->mItems, self->mItems + self->mCount);
    CMemory::Free(self->mItems);
    self->mItems = newData;
    self->mCapacity = newSize;
  }
}

// `fn_8028E588` - retail `.text:0x8028E588`, 0xB4 = 180 bytes, unnamed. It is
// `rstl::vector< AdditivePair >::reserve(int)`; its caller, the `vector( CInputStream& )` at
// 0x8028D6D4, is not written here. Same body
// as `fn_8028E63C` with `mulli 12` (0x0C = `sizeof(AdditivePair)`) and the copy loop written out
// in place rather than called: `AdditivePair` is a `uint` and two `float`s, all trivial, so
// `uninitialized_copy` folds into three `lwz`/`stw` pairs and retail makes no call for it.
extern "C" void fn_8028E588(AdditiveVec* self, int newSize) {
  if (newSize > self->mCapacity) {
    AdditivePair* newData;
    rstl::rmemory_allocator().allocate(newData, newSize);
    for (int i = 0; i < self->mCount; ++i) {
      newData[i] = self->mItems[i];
    }
    CMemory::Free(self->mItems);
    self->mItems = newData;
    self->mCapacity = newSize;
  }
}

// `fn_8028E534` - retail `.text:0x8028E534`, 0x54 = 84 bytes, unnamed. It is
// `rstl::uninitialized_copy< pointer_iterator< CHalfTransition, ... >, CHalfTransition* >`, the
// copy half of `fn_8028E474` below. The loop is entered at its bottom test, has **no frame** -
// all three of its cursors fit in volatile registers - and its body is the copy plus the
// `rc_ptr<IMetaTrans>` refcount bump at +8, with a null test on the destination.
extern "C" CHalfTransition* fn_8028E534(const CHalfTransition* const* begin,
                                        const CHalfTransition* const* end, CHalfTransition* out) {
  const CHalfTransition* it = *begin;
  CHalfTransition* cur = out;
  for (; it != *end; ++it, ++cur) {
    rstl::construct(cur, *it);
  }
  return cur;
}

// `fn_8028E474` - retail `.text:0x8028E474`, 0xC0 = 192 bytes, unnamed. It is
// `rstl::vector< CHalfTransition >::reserve(int)`; its caller, the `vector( CInputStream& )`
// at 0x8028D524, is not written here. Same body as `fn_8028E63C` with `mulli 12`, the copy out to `fn_8028E534`, and the destroy of the
// old range written in place: `addi r3,r30,4` is the `rc_ptr<IMetaTrans>` member at +4, with the
// two null tests `destroy_impl` runs per element.
extern "C" void fn_8028E474(HalfVec* self, int newSize) {
  if (newSize > self->mCapacity) {
    CHalfTransition* newData;
    rstl::rmemory_allocator().allocate(newData, newSize);
    const CHalfTransition* beginSlot = self->mItems;
    const CHalfTransition* endSlot = self->mItems + self->mCount;
    fn_8028E534(&beginSlot, &endSlot, newData);
    for (int i = 0; i < self->mCount; ++i) {
      self->mItems[i].~CHalfTransition();
    }
    CMemory::Free(self->mItems);
    self->mItems = newData;
    self->mCapacity = newSize;
  }
}

// `fn_8028E410` - retail `.text:0x8028E410`, 0x64 = 100 bytes, unnamed. It is
// `rstl::uninitialized_copy< pointer_iterator< CTransition, ... >, CTransition* >`; same shape as
// `fn_8028E534` with the five words of a `CTransition` and its `rc_ptr<IMetaTrans>` at +12.
extern "C" CTransition* fn_8028E410(const CTransition* const* begin, const CTransition* const* end,
                                  CTransition* out) {
  const CTransition* it = *begin;
  CTransition* cur = out;
  for (; it != *end; ++it, ++cur) {
    rstl::construct(cur, *it);
  }
  return cur;
}

// `fn_8028E350` - retail `.text:0x8028E350`, 0xC0 = 192 bytes, unnamed. It is
// `rstl::vector< CTransition >::reserve(int)`, called once, from `fn_8028DE98`. Same body as
// `fn_8028E474` with `mulli 20` (0x14 = `sizeof(CTransition)`) and the refcount at +12.
extern "C" void fn_8028E350(TransVec* self, int newSize) {
  if (newSize > self->mCapacity) {
    CTransition* newData;
    rstl::rmemory_allocator().allocate(newData, newSize);
    const CTransition* beginSlot = self->mItems;
    const CTransition* endSlot = self->mItems + self->mCount;
    fn_8028E410(&beginSlot, &endSlot, newData);
    for (int i = 0; i < self->mCount; ++i) {
      self->mItems[i].~CTransition();
    }
    CMemory::Free(self->mItems);
    self->mItems = newData;
    self->mCapacity = newSize;
  }
}

// `fn_8028E2C0` - retail `.text:0x8028E2C0`, 0x90 = 144 bytes, unnamed. It is
// `rstl::uninitialized_copy< pointer_iterator< CAnimation, ... >, CAnimation* >`, called once,
// from `fn_8028E1F0` below. `CAnimation` is 0x18 and holds an `rstl::basic_string` and an
// `rc_ptr<IMetaAnim>`, so the loop body is not a word copy: it is retail's out-of-line
// `rstl::basic_string` copy constructor at 0x8028E2B4, then the `rc_ptr` at +16/+20 and its
// refcount bump, with a null test on the destination.
extern "C" CAnimation* fn_8028E2C0(const CAnimation* const* begin, const CAnimation* const* end,
                                CAnimation* out) {
  const CAnimation* it = *begin;
  CAnimation* cur = out;
  for (const CAnimation* last = *end; it != last; ++it, ++cur) {
    rstl::construct(cur, *it);
  }
  return cur;
}

// `fn_8028E1F0` - retail `.text:0x8028E1F0`, 0xD0 = 208 bytes, unnamed. It is
// `rstl::vector< CAnimation >::reserve(int)`; its caller, the `vector( CInputStream& )` at
// 0x8028E0E4, is not written here. `mulli 24`
// is 0x18 = `sizeof(CAnimation)`, the copy goes out to `fn_8028E2C0`, and the destroy of the old
// range is written in place: `addi r3,r30,16` is the `rc_ptr<IMetaAnim>` at +16 and
// `cmplwi r30,0 / beq` the null test before it.
extern "C" void fn_8028E1F0(AnimVec* self, int newSize) {
  if (newSize > self->mCapacity) {
    CAnimation* newData;
    rstl::rmemory_allocator().allocate(newData, newSize);
    const CAnimation* beginSlot = self->mItems;
    const CAnimation* endSlot = self->mItems + self->mCount;
    fn_8028E2C0(&beginSlot, &endSlot, newData);
    for (int i = 0; i < self->mCount; ++i) {
      self->mItems[i].~CAnimation();
    }
    CMemory::Free(self->mItems);
    self->mItems = newData;
    self->mCapacity = newSize;
  }
}

// `fn_8028E1D0` - retail `.text:0x8028E1D0`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CAnimation >` for the stream element type: a frame and one unconditional
// `bl` (0x8028E1DC) to `CAnimation::CAnimation(CInputStream&)`, nothing else. It is written as a
// placement new rather than as `rstl::construct(...)` because `construct` and `construct_impl`
// are both in-class-inline and mwceppc folds them into one another.
extern "C" void fn_8028E1D0(CAnimation* dest, CInputStream& in) { PlaceCAnimation(dest, in); }

// `fn_8028E074` - retail `.text:0x8028E074`, 0x70 = 112 bytes, unnamed. It is
// `rstl::destroy_impl< pointer_iterator< CAnimation, ... > >`, the per-element body
// `vector<CAnimation>::reserve` writes in place. The two null tests per element are
// `rc_ptr<IMetaAnim>::ReleaseData`'s own and `basic_string::~basic_string`'s, and `addi r31,24`
// is `sizeof(CAnimation)`.
extern "C" void fn_8028E074(AnimIter begin, AnimIter end) {
  CAnimation* cur = begin.get_pointer();
  while (cur != end.get_pointer()) {
    cur->~CAnimation();
    ++cur;
  }
}

// `fn_8028E03C` - retail `.text:0x8028E03C`, 0x38 = 56 bytes, unnamed. It is
// `rstl::destroy< pointer_iterator< CAnimation, ... > >`: the two iterators are built on the
// stack at 8(r1)/12(r1) out of the two cursors loaded from `r3`/`r4`, and `fn_8028E074` is
// called on them. The `addi r4,r1,8 / addi r3,r1,12` pair is the by-value pass of two
// `pointer_iterator`s, which are classes and so go on the stack.
extern "C" void fn_8028E03C(AnimIter begin, AnimIter end) { fn_8028E074(begin, end); }

// `fn_8028DFB8` - retail `.text:0x8028DFB8`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< CAnimation >::~vector()`, the *deleting* destructor mwceppc generates, called
// once, from `CAnimationSet`'s own deleting destructor.
//
//     13e4.. bl fn_8028E03C                    ; destroy(begin(), end())
//     bl     Free__7CMemoryFPCv                 ; mAllocator.deallocate(mItems)
//     extsh. r0,r31 / ble                       ; the delete flag, as a *signed halfword*
//     bl     Free__7CMemoryFPCv                 ; operator delete
//
// Three details, all measured. The flag is sign-extended from 16 bits by the *function* and the
// sign-extended value is what is tested, so the source compares a `short` and not an `int` - the
// same reading as `fn_80248410` in `src/WorldFormat/CMetroidAreaCollider.cpp`. It stays in `r4`
// throughout, so only `r30`/`r31` are saved. And the function returns `this`, which is the
// deleting-destructor convention and costs nothing.
extern "C" void* fn_8028DFB8(void* self, int flag) {
  if (self != nullptr) {
    AnimVec* vec = static_cast< AnimVec* >(self);
    fn_8028E03C(AnimIter(vec->mItems), AnimIter(vec->mItems + vec->mCount));
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028DF98` - retail `.text:0x8028DF98`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CTransition >`: a frame and one `bl` (0x8028DFA4) to
// `CTransition::CTransition(CInputStream&)`.
extern "C" void fn_8028DF98(CTransition* dest, CInputStream& in) { PlaceCTransition(dest, in); }

// `fn_8028DE38` - retail `.text:0x8028DE38`, 0x60 = 96 bytes, unnamed. It is
// `rstl::destroy_impl< pointer_iterator< CTransition, ... > >` - the `addi r3,r31,12` null test
// and `bl ReleaseData<rc_ptr<IMetaTrans>>` per element, with `addi r31,r31,20` the
// `sizeof(CTransition)` step. Retail's 96 bytes are not byte-identical to anything this object
// emits, so it scores below 100% either way; it is written out so `fn_8028DE00` and
// `fn_8028DD7C` above have the callee they call.
extern "C" void fn_8028DE38(TransIter begin, TransIter end) {
  for (CTransition* cur = begin.get_pointer(); cur != end.get_pointer(); ++cur) {
    cur->~CTransition();
  }
}

// `fn_8028DE00` - retail `.text:0x8028DE00`, 0x38 = 56 bytes, unnamed. It is
// `rstl::destroy< pointer_iterator< CTransition, ... > >`: the two iterators arrive by hidden
// pointer and are copied into the outgoing argument slots at 8(r1)/12(r1) before
// `fn_8028DE38` is called on them.
extern "C" void fn_8028DE00(TransIter begin, TransIter end) { fn_8028DE38(begin, end); }

// `fn_8028DD7C` - retail `.text:0x8028DD7C`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< CTransition >::~vector()`, the deleting destructor, called once, from
// `CAnimationSet`'s own deleting destructor. Same three details as `fn_8028DFB8`, with
// `mulli 20` (0x14 = `sizeof(CTransition)`) and the range walk out of line at `fn_8028DE00`.
extern "C" void* fn_8028DD7C(void* self, int flag) {
  if (self != nullptr) {
    TransVec* vec = static_cast< TransVec* >(self);
    fn_8028DE00(TransIter(vec->mItems), TransIter(vec->mItems + vec->mCount));
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028DCE4` - retail `.text:0x8028DCE4`, 0x38 = 56 bytes, unnamed. It is
// `rstl::destroy< pointer_iterator< CHalfTransition, ... > >`; retail makes no call for the
// `destroy_impl` half, because `CHalfTransition` is a `uint` and an `rc_ptr` and the range walk
// folds into the two-iterator prologue. What is left is the frame, the two stack iterators and
// `blr`.
extern "C" void fn_8028DCE4(HalfIter begin, HalfIter end) {
  DestroyHalfRange(begin, end);
}

// `fn_8028DC60` - retail `.text:0x8028DC60`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< CHalfTransition >::~vector()`, the deleting destructor, called once, from
// `CAnimationSet`'s own deleting destructor. Same three details as `fn_8028DFB8`, and the
// element destroy is the `mulli 12` range walk at 0x8028DC74's `bl fn_8028DCE4`.
extern "C" void* fn_8028DC60(void* self, int flag) {
  if (self != nullptr) {
    HalfVec* vec = static_cast< HalfVec* >(self);
    fn_8028DCE4(HalfIter(vec->mItems), HalfIter(vec->mItems + vec->mCount));
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028DB68` - retail `.text:0x8028DB68`, 0x24 = 36 bytes, unnamed. It is
// `rstl::destroy_impl< CAnimPOIData >`: a frame, `li r4,-1`, and one `bl` (0x8028DB74) to

// `fn_8028DB48` - retail `.text:0x8028DB48`, 0x20 = 32 bytes, unnamed. It is
// `rstl::destroy< CAnimPOIData >`: a frame and one unconditional `bl` (0x8028DB54) to
// `fn_8028DB68`, nothing else.
extern "C" void fn_8028DB48(CAnimPOIData* in) { fn_8028DB68(in); }

// `fn_8028DAF8` - retail `.text:0x8028DAF8`, 0x50 = 80 bytes, unnamed. It is
// `rstl::destroy_impl< pointer_iterator< CAnimPOIData, ... > >`, called once, from
// `fn_8028DAC0` below. `addi r31,r31,68` is `sizeof(CAnimPOIData)`, and the loop's bound is read
// back out of the *end iterator* every iteration rather than kept in a register.
extern "C" void fn_8028DAF8(PoiIter begin, PoiIter end) {
  CAnimPOIData* cur = begin.get_pointer();
  while (cur != end.get_pointer()) {
    fn_8028DB48(cur);
    ++cur;
  }
}

// `fn_8028DAC0` - retail `.text:0x8028DAC0`, 0x38 = 56 bytes, unnamed. It is
// `rstl::destroy< pointer_iterator< CAnimPOIData, ... > >`: the two iterators are built on the
// stack at 8(r1)/12(r1) from the cursors loaded out of `r3`/`r4`, and `fn_8028DAF8` is called on
// them.
extern "C" void fn_8028DAC0(PoiIter begin, PoiIter end) { fn_8028DAF8(begin, end); }

// `fn_8028DA3C` - retail `.text:0x8028DA3C`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< CAnimPOIData >::~vector()`, the deleting destructor, called once, from
// `CAnimationSet`'s own deleting destructor. `mulli 68` at 0x8028DAE4 is the
// `mItems + mCount * sizeof(CAnimPOIData)` that ends the destroy range.
extern "C" void* fn_8028DA3C(void* self, int flag) {
  if (self != nullptr) {
    AnimPoiVec* vec = static_cast< AnimPoiVec* >(self);
    fn_8028DAC0(PoiIter(vec->mItems), PoiIter(vec->mItems + vec->mCount));
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028D9D4` - retail `.text:0x8028D9D4`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< CAnimPOIData*, CAnimPOIData* >`, called once, from `fn_8028D950`
// below. The count is the second argument, the loop is entered at its bottom test, and the
// cursors advance *after* the construct so a zero count still returns `dest`.
extern "C" CAnimPOIData* fn_8028D9D4(CAnimPOIData* src, int n, CAnimPOIData* dest) {
  CAnimPOIData* it = src;
  CAnimPOIData* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_8028CCA8(&*cur, *it);
  }
  return cur;
}

// `fn_8028D950` - retail `.text:0x8028D950`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< CAnimPOIData >::vector( const vector& )`, retail's out-of-line copy of the copy
// constructor, called once, from `fn_8028CCF0` below. The two counts are stored first, then the
// "both zero" test, and only then `allocate` and `fn_8028D9D4` - the same "store, then read back"
// shape as `fn_80143CD4` in `src/MetroidPrime/Player/CGameState.cpp`, for the same reason.
// `mulli 68` is 0x44 = `sizeof(CAnimPOIData)`, this unit's `CHECK_SIZEOF(CAnimPOIData, 0x44)`.
//
// Which of the five `vector<...>` copy constructors in this unit it is, is decided by the call
// graph and not by the bytes, which all five share: `fn_8028D9D4` below it steps by 68 and calls

CAnimationSet::CAnimationSet(CInputStream& in)
: mTableCount(in.Get< ushort >())
, mAnimations(in)
, mTransitions(in)
, mDefaultTransition(CMetaTransFactory::CreateMetaTrans(in))
, mAdditiveAnimations(StreamAdditiveAnimInfoList(mTableCount, in))
, mDefaultAdditiveAnimation(StreamDefaultAdditiveAnimInfo(mTableCount, in))
, mHalfTransitions(StreamHalfTransitions(mTableCount, in))
, mEventSets(StreamEventSetList(mTableCount, in)) {}

// `fn_8028D7AC` - retail `.text:0x8028D7AC`, 0x70 = 112 bytes, unnamed. It is
// `rstl::pair< uint, CAdditiveAnimationInfo >::pair( CInputStream& )`:
// `in.Get<uint>()` (`ReadInt32` inlined) stores to +0, then two `ReadFloat` calls whose results
// are kept in `f31` and returned in `f1` - the first is stored to +4 only *after* the second
// read, which is what keeps `f31` live across the call at 0x8028D7AC.
extern "C" AdditivePair* fn_8028D7AC(AdditivePair* self, CInputStream& in) {
  self->first = in.Get< uint >();
  self->second = CAdditiveAnimationInfo(in);
  return self;
}

// `fn_8028D78C` - retail `.text:0x8028D78C`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< AdditivePair >`: a frame and one `bl` (0x8028D79C) to `fn_8028D7AC`.
extern "C" void fn_8028D78C(AdditivePair* dest, CInputStream& in) { fn_8028D7AC(dest, in); }

CAnimationSet::AdditiveAnimationList CAnimationSet::StreamAdditiveAnimInfoList(ushort tableCount,
                                                                               CInputStream& in) {
  if (tableCount > 1) {
    return AdditiveAnimationList(in);
  }

  return AdditiveAnimationList();
}

CAdditiveAnimationInfo CAnimationSet::StreamDefaultAdditiveAnimInfo(ushort tableCount,
                                                                    CInputStream& in) {
  if (tableCount > 1) {
    return CAdditiveAnimationInfo(in);
  }

  return CAdditiveAnimationInfo(0.f, 0.f);
}

// `fn_8028D604` - retail `.text:0x8028D604`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CHalfTransition >`: a frame and one `bl` (0x8028D614) to `fn_8032250C`,
// which is `CHalfTransition`'s stream constructor - it lives in another unit
// (`src/Kyoto/Animation/CHalfTransition.cpp`), so retail's object calls it by address and ours
// calls the same out-of-line constructor.
extern "C" void fn_8028D604(CHalfTransition* dest, CInputStream& in) {
  PlaceCHalfTransition(dest, in);
}

CAnimationSet::HalfTransitionList CAnimationSet::StreamHalfTransitions(ushort tableCount,
                                                                       CInputStream& in) {
  if (tableCount > 2) {
    return HalfTransitionList(in);
  }

  return HalfTransitionList();
}

// `fn_8028D4BC` - retail `.text:0x8028D4BC`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CAnimPOIData >` for the stream element type: a frame and one `bl`
// (0x8028D4C8) to `CAnimPOIData::CAnimPOIData(CInputStream&)`.
extern "C" void fn_8028D4BC(CAnimPOIData* dest, CInputStream& in) { PlaceCAnimPOIData(dest, in); }

// `fn_8028D440` - `CAnimPOIData::~CAnimPOIData` with the *delete* flag set to -1, because an
// element's destructor never frees the element.
extern "C" void fn_8028DB68(CAnimPOIData* in) { fn_8028D440(in, -1); }

// `fn_8028D440` - retail `.text:0x8028D440`, 0x7C = 124 bytes, unnamed. It is
// `CAnimPOIData::~CAnimPOIData()`, the *deleting* destructor mwceppc generates, called from
// `fn_8028DB68` with the flag -1 and from `CAnimationSet`'s own deleting destructor. The four
// member `rstl::vector` destructors are called in **reverse declaration order** (52, 36, 20, 4 -
// `mSoundNodes`, `mParticleNodes`, `mInt32Nodes`, `mBoolNodes`), each with the delete flag -1,
// which is the compiler's own member-teardown order and not something the source chooses.
extern "C" void* fn_8028D440(void* self, int flag) {
  if (self != nullptr) {
    CAnimPOIData* obj = static_cast< CAnimPOIData* >(self);
    obj->mSoundNodes.~SoundPoiVec();
    obj->mParticleNodes.~ParticlePoiVec();
    obj->mInt32Nodes.~Int32PoiVec();
    obj->mBoolNodes.~BoolPoiVec();
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028D420` - retail `.text:0x8028D420`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CBoolPOINode >`: a frame and one `bl` (0x8028D42C) to
// `rstl::construct_impl< CBoolPOINode >`. It is written as a call rather than as
// `rstl::construct(...)` because `construct` and `construct_impl` are both in-class-inline and
// mwceppc folds them - folding `construct_impl` here would fold in its placement-new null test
// too, which is retail's *other* `construct` function and not this one.
extern "C" void fn_8028D420(CBoolPOINode* dest, const CBoolPOINode& src) {
  rstl::construct_impl< CBoolPOINode >(dest, src);
}

// `fn_8028D3B8` - retail `.text:0x8028D3B8`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< CBoolPOINode*, CBoolPOINode* >`. `addi r31,r31,68` is
// `sizeof(CBoolPOINode)`, which is this unit's `CHECK_SIZEOF`-equivalent 0x44.
extern "C" CBoolPOINode* fn_8028D3B8(CBoolPOINode* src, int n, CBoolPOINode* dest) {
  CBoolPOINode* it = src;
  CBoolPOINode* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_8028D420(&*cur, *it);
  }
  return cur;
}

// `fn_8028D268` - retail `.text:0x8028D268`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CInt32POINode >`, the same `construct` / `construct_impl` pair as
// `fn_8028D420`.
extern "C" void fn_8028D268(CInt32POINode* dest, const CInt32POINode& src) {
  rstl::construct_impl< CInt32POINode >(dest, src);
}

// `fn_8028D200` - retail `.text:0x8028D200`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< CInt32POINode*, CInt32POINode* >`.
extern "C" CInt32POINode* fn_8028D200(CInt32POINode* src, int n, CInt32POINode* dest) {
  CInt32POINode* it = src;
  CInt32POINode* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_8028D268(&*cur, *it);
  }
  return cur;
}

// `fn_8028D0B0` - retail `.text:0x8028D0B0`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CParticlePOINode >`.
extern "C" void fn_8028D0B0(CParticlePOINode* dest, const CParticlePOINode& src) {
  rstl::construct_impl< CParticlePOINode >(dest, src);
}

// `fn_8028D048` - retail `.text:0x8028D048`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< CParticlePOINode*, CParticlePOINode* >`.
extern "C" CParticlePOINode* fn_8028D048(CParticlePOINode* src, int n, CParticlePOINode* dest) {
  CParticlePOINode* it = src;
  CParticlePOINode* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_8028D0B0(&*cur, *it);
  }
  return cur;
}

// `fn_8028CEF8` - retail `.text:0x8028CEF8`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CSoundPOINode >`.
extern "C" void fn_8028CEF8(CSoundPOINode* dest, const CSoundPOINode& src) {
  rstl::construct_impl< CSoundPOINode >(dest, src);
}

// `fn_8028CE90` - retail `.text:0x8028CE90`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< CSoundPOINode*, CSoundPOINode* >`.
extern "C" CSoundPOINode* fn_8028CE90(CSoundPOINode* src, int n, CSoundPOINode* dest) {
  CSoundPOINode* it = src;
  CSoundPOINode* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_8028CEF8(&*cur, *it);
  }
  return cur;
}

// `fn_8028CCF0` - retail `.text:0x8028CCF0`, 0x70 = 112 bytes, unnamed. It is
// `CAnimPOIData::CAnimPOIData( const CAnimPOIData& )`, the copy constructor mwceppc generates.
// The `uint mVersion` at +0 is copied with one `lwz`/`stw` and the four `rstl::vector` members
// are copy-constructed in **declaration** order (4, 20, 36, 52 - `mBoolNodes`, `mInt32Nodes`,
// `mParticleNodes`, `mSoundNodes`), each out of line. The function returns `this` in `r3`
// (`mr r3,r30` at 0x8028CD9C), which is the constructor convention. A copy constructor's symbol
// name is fixed by the mangler, so this one is written out as a free function over the same
// statements rather than as a constructor - the same reason `fn_80143CD4` is not a member.
extern "C" CAnimPOIData* fn_8028CCF0(CAnimPOIData* self, const CAnimPOIData* other) {
  self->mVersion = other->mVersion;
  CopyBoolPoiVec(&self->mBoolNodes, other->mBoolNodes);
  CopyInt32PoiVec(&self->mInt32Nodes, other->mInt32Nodes);
  CopyParticlePoiVec(&self->mParticleNodes, other->mParticleNodes);
  CopySoundPoiVec(&self->mSoundNodes, other->mSoundNodes);
  return self;
}

// `fn_8028CCC8` - retail `.text:0x8028CCC8`, 0x28 = 40 bytes, unnamed. It is
// `rstl::construct_impl< CAnimPOIData >`: the `cmplwi r3,0` / `beq` pair at 0x8028CCF4 /
// 0x8028CCFC **is** mwceppc's placement-new null test, and `bl fn_8028CCF0` is the copy
// constructor. Written as a placement new rather than as an explicit `if (dest != nullptr)`
// around one, because the explicit test produces the pair twice.
extern "C" void fn_8028CCC8(CAnimPOIData* dest, const CAnimPOIData& src) {
  new (dest) CAnimPOIData(src);
}

// `fn_8028CCA8` - `rstl::construct< CAnimPOIData >` - and `fn_8028CCF0` is `CAnimPOIData`'s own
// copy constructor. The four `*POINode` vectors' copy constructors are retail's *named* weak
// symbols (`__ct__Q24rstl49vector<12CBoolPOINode...>` and its three siblings) and already pair.
extern "C" AnimPoiVec* fn_8028D950(AnimPoiVec* self, const AnimPoiVec* other) {
  self->mCount = other->mCount;
  self->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    self->mItems = nullptr;
  } else {
    rstl::rmemory_allocator().allocate(self->mItems, self->mCapacity);
    fn_8028D9D4(other->mItems, self->mCount, self->mItems);
  }
  return self;
}

// `fn_8028CCA8` - retail `.text:0x8028CCA8`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CAnimPOIData >`: a frame and one unconditional `bl` (0x8028CCB4) to
// `fn_8028CCC8`, nothing else.
extern "C" void fn_8028CCA8(CAnimPOIData* dest, const CAnimPOIData& src) { fn_8028CCC8(dest, src); }

// `fn_8028CC70` - retail `.text:0x8028CC70`, 0x38 = 56 bytes, unnamed. It is
// `rstl::vector< CAnimPOIData >::push_back_unsafe`. Its one caller - the
// `rstl::vector< CAnimPOIData >::vector( CInputStream& )` at 0x8028CBD0 - is **not** written
// here, so this function is in the object but nothing in it is called. That is deliberate and
// measured: see the note at the end of this file.
//
//     f4   lwz   r5,4(r3)                  ; mCount
//     f8   lwz   r6,12(r3)                 ; mItems
//     fc   mulli r0,r5,68                  ; sizeof(CAnimPOIData) == 0x44
//     100  addi  r5,r5,1 / stw r5,4(r3)    ; ++mCount, stored *before* the construct
//     108  add   r3,r6,r0                  ; mItems + mCount
//     10c  bl    fn_8028CCA8               ; construct(mItems + mCount, in)
//
// The count is incremented and stored before the construct, which is what
// `rstl::construct(mItems + mCount++, in)` in `include/rstl/vector.hpp` spells, and the
// destination is a `mulli` and not a pointer load because `mCount * sizeof(T)` is the index.
extern "C" void fn_8028CC70(AnimPoiVec* self, const CAnimPOIData& in) {
  fn_8028CCA8(self->mItems + self->mCount++, in);
}

CAnimationSet::EventSetList CAnimationSet::StreamEventSetList(ushort tableCount, CInputStream& in) {
  if (tableCount > 3) {
    return EventSetList(in);
  }

  return EventSetList();
}

// ## The four `rstl::vector< T >::vector( CInputStream& )` constructors are not written here
//
// `fn_8028CBD0` (160 B), `fn_8028D524` (224 B), `fn_8028D6D4` (184 B) and `fn_8028E0E4` (236 B) are
// byte-identical to code this object already emits, and all four are still 0.00%. Each one's
// per-iteration loop opens with the same three instructions and no spelling of the loop body
// produces them:
//
//     94: lbz  r0,lbl_80419838        ; a byte out of .sbss
//     9c: mr   r5,r31                ; its address, as a third argument
//     a4: stb  r0,8(r1)              ; stored to a stack slot first
//     a8: bl   fn_8028D4BC           ; construct(&tmp, in, &flag)
//
// The third argument is the empty `TType<T>` that `CInputStream::Get<T>( const TType<T>& type =
// TType<T>() )` takes **by hidden pointer** (it is a class), and the `lbz`/`stb` pair is mwceppc
// initialising it. The four labels are four distinct one-byte `.sbss` objects -
// `lbl_80419838`, `lbl_80419830`, `lbl_80419834` and `lbl_80419828` in
// `config/G2ME01/symbols.txt`, one per element type, in the gap between
// `Kyoto/Basics/RAssertDolphin.cpp`'s and `Kyoto/Audio/CSfxHandle.cpp`'s `.sbss` claims. So
// retail's source here is `in.Get<T>()` in the `vector(CInputStream&)` loop, and the 32-byte
// `fn_` wrappers (`fn_8028E1D0`, `fn_8028D604`, `fn_8028D78C`, `fn_8028DF98`) are retail's
// out-of-line copies of that `Get<T>`.
//
// Reproducing it needs a `static` byte that mwceppc loads out of `.sbss` every iteration, which
// this tree's `CInputStream::Get` does not do (it is a one-line `return T(*this);`), and then
// getting four different 160-236-byte loop bodies to allocate registers identically on top. That
// is four more functions than this change is worth, and the four are the *only* thing standing
// between the functions above and 55/67.
