#include "Kyoto/Animation/CCharacterSet.hpp"

#include "Kyoto/Streams/CInputStream.hpp"

#include "rstl/construct.hpp"

typedef rstl::pair< int, CCharacterInfo > TCharacterEntry;
typedef rstl::vector< TCharacterEntry > TCharacterList;

// Declared up front only so the chain can be written caller-after-callee in the order below; a
// declaration emits no code, so it does not disturb `.text`'s order.
extern "C" TCharacterEntry* fn_80293FDC(TCharacterList::iterator begin, TCharacterList::iterator end,
                                        TCharacterEntry* out);
extern "C" void fn_80293F90(TCharacterEntry* first, TCharacterEntry* last);
extern "C" void fn_80293F70(TCharacterEntry* first, TCharacterEntry* last);
extern "C" void fn_80293EC4(TCharacterList* self, int size);
extern "C" void fn_80293EA4(TCharacterEntry* in);
extern "C" void fn_80293E84(CCharacterInfo* dest, CInputStream& in);
extern "C" void fn_80293E18(TCharacterEntry* self, CInputStream& in);
extern "C" void fn_80293DD8(void* dest, const TCharacterEntry& src);
extern "C" void fn_80293DB0(void* dest, const TCharacterEntry& src);
extern "C" void fn_80293D90(void* dest, const TCharacterEntry& src);
extern "C" void fn_80293D58(TCharacterList* self, const TCharacterEntry& in);

/**
 * `.text 0x80293D58`..`0x80294044` - the eleven unnamed routines retail's linker leaves between this
 * constructor and `CMetaAnimBlend`.
 *
 * None of them is a `CCharacterSet` member. They are the out-of-line copies the `rstl` headers emit
 * for `TCharacterList`'s two out-of-line members and for the `rstl` construct/destroy family over
 * `TCharacterEntry`, which is what the constructor's own body needs:
 *
 *   0x80293D58  56 B `TCharacterList::push_back_unsafe`        - the constructor's push
 *   0x80293D90  32 B `rstl::construct<TCharacterEntry>`        - 8-instruction forwarder
 *   0x80293DB0  40 B `rstl::construct_impl<TCharacterEntry>`   - `cmplwi r3,0` / `beq` null test
 *   0x80293DD8  64 B `TCharacterEntry::TCharacterEntry(const&)` - member-wise, `first` then `second`
 *   0x80293E18 108 B `TCharacterEntry::TCharacterEntry(CInputStream&)` - reads `first`, then
 *                                                            `second` through `CInputStream::Get`
 *   0x80293E84  32 B `CCharacterInfo::CCharacterInfo(CInputStream&)` - 8-instruction forwarder
 *   0x80293EA4  32 B `rstl::destroy<TCharacterEntry>`          - 8-instruction forwarder
 *   0x80293EC4 172 B `TCharacterList::reserve`                 - the constructor's reserve
 *   0x80293F70  32 B `rstl::destroy<TCharacterEntry>(first, last)` - 8-instruction forwarder
 *   0x80293F90  76 B `rstl::destroy_impl<TCharacterEntry*>`    - stride 0xFC loop
 *   0x80293FDC 104 B `rstl::uninitialized_copy`                - stride 0xFC loop
 *
 * They are spelled out by hand for the reason `MetroidPrime/CAnimData.cpp` spells out its own
 * `rstl::construct` chain: retail's map leaves all eleven unnamed, so `config/G2ME01/symbols.txt`
 * carries the `fn_<addr>` placeholder for each, and that spelling is kept - which is also what
 * makes them `extern "C"`, because a C++ definition of a template would mangle
 * (`push_back_unsafe__Q24rstl68vector<...>`) and objdiff pairs functions by name, so it would pair
 * nothing. The bodies are the ones the headers already spell, in the order the headers spell them:
 * `push_back_unsafe` is `construct(mItems + mCount++, in)` (the count is stored before the call, as
 * retail has it), `construct` forwards to `construct_impl`, `construct_impl` is `new (dest) T(src)`
 * and so keeps the placement-new null test, `reserve` is `vector::reserve` unchanged, and the two
 * `destroy` overloads are the forwarder/loop pair over `is_trivially_destructible`. Each forwarder is
 * written as a call to the routine retail's forwarder calls - `fn_80293D58` calls `fn_80293D90`,
 * `fn_80293D90` calls `fn_80293DB0`, `fn_80293F70` calls `fn_80293F90` - which is the same device
 * `fn_80026F28` -> `fn_80026F68` uses in `CAnimData`.
 *
 * Two bodies are spelled as loops rather than through the header, because mwcceppc's deferred
 * inliner leaves both `destroy_impl` and `uninitialized_copy` outlined at this call site
 * (`fn_80293F90` came out as a thunk to `destroy_impl__4rstl`), and an outlined call is 32 bytes
 * where retail has the loop. Spelled out, both reproduce retail's registers as well as its
 * instructions: `destroy_impl` keeps the induction variable in r31 and the limit in r30, and
 * `uninitialized_copy` reloads the limit from the caller's iterator (`lwz r0, 0x0(r29)`) instead of
 * holding it in a register - so `fn_80293FDC` takes the two iterators by value, which mwcceppc
 * passes by address.
 *
 * Three of the eleven are still short, all for the same reason
 * (`include/Collision/CCollisionInfo.hpp` records it): retail's copies end in a bare `bl` because
 * the constructor they call is a function the compiler has no body for, while mwcceppc expands
 * `new (dest) T(src)` into "call `operator new`, test the result against null, then construct", and
 * that `addic.`/`beq` pair survives inlining. `fn_80293DD8` and `fn_80293E84` want
 * `__ct__14CCharacterInfoFRC14CCharacterInfo` / `__ct__14CCharacterInfoFR12CInputStream` out of
 * line, which means declaring `CCharacterInfo`'s constructors in its header - shared, and its copy
 * constructor is what `MetroidPrime/CAnimData.cpp` already matches, so not this file's change to
 * make. `fn_80293E18` additionally wants the `bool` member retail's `TType<T>` carries
 * (`lbz r0, lbl_80419860` / `stb r0, 0x8(r1)`); ours is an empty tag.
 *
 * Declared in reverse retail order, which is the order mwldeppc needs this unit's `.text` in:
 * mwcceppc emits definitions in reverse source order.
 */
extern "C" TCharacterEntry* fn_80293FDC(TCharacterList::iterator begin, TCharacterList::iterator end,
                                        TCharacterEntry* out) {
  TCharacterList::iterator cur = begin;
  TCharacterEntry* tmp = out;
  for (; cur != end; ++cur, ++tmp) {
    fn_80293D90(tmp, *cur);
  }
  return tmp;
}

extern "C" void fn_80293F90(TCharacterEntry* first, TCharacterEntry* last) {
  TCharacterEntry* cur = first;
  for (; cur != last; ++cur) {
    fn_80293EA4(cur);
  }
}

extern "C" void fn_80293F70(TCharacterEntry* first, TCharacterEntry* last) {
  fn_80293F90(first, last);
}

// `vector<T, Alloc>::reserve` unchanged, spelled in place so it carries this name.
extern "C" void fn_80293EC4(TCharacterList* self, int size) {
  if (size <= self->capacity()) {
    return;
  }

  TCharacterEntry* newData;
  self->mAllocator.allocate(newData, size);
  rstl::uninitialized_copy(self->begin(), self->end(), newData);
  rstl::destroy(self->data(), self->data() + self->size());
  self->mAllocator.deallocate(self->mItems);
  self->mItems = newData;
  self->mCapacity = size;
}

extern "C" void fn_80293EA4(TCharacterEntry* in) { rstl::destroy(in); }

extern "C" void fn_80293E84(CCharacterInfo* dest, CInputStream& in) {
  new (dest) CCharacterInfo(in);
}

extern "C" void fn_80293E18(TCharacterEntry* self, CInputStream& in) {
  new (self) TCharacterEntry(in);
}

// `pair<int, CCharacterInfo>::pair(const pair&)`, member-wise in declaration order. The second
// member is copy-constructed through `out + 4` rather than through `&d->second` so that the
// compiler computes it as a plain `addic. r5, r3, 4`; the `beq` that follows is the placement-new
// null test, which retail's copy constructor does not have - see the note above.
extern "C" void fn_80293DD8(void* dest, const TCharacterEntry& src) {
  char* out = static_cast< char* >(dest);
  *reinterpret_cast< int* >(out) = src.first;
  new (out + 4) CCharacterInfo(src.second);
}

// `rstl::construct_impl<TCharacterEntry>`: `new (dest) T(src)` and so the null test. Written as the
// explicit test rather than through placement new because it must forward to `fn_80293DD8` by name.
extern "C" void fn_80293DB0(void* dest, const TCharacterEntry& src) {
  if (dest != nullptr) {
    fn_80293DD8(dest, src);
  }
}

extern "C" void fn_80293D90(void* dest, const TCharacterEntry& src) { fn_80293DB0(dest, src); }

extern "C" void fn_80293D58(TCharacterList* self, const TCharacterEntry& in) {
  fn_80293D90(&self->data()[self->mCount++], in);
}

CCharacterSet::CCharacterSet(CInputStream& in) : mTableCount(in.Get< ushort >()) {
  const int count = in.Get< int >();
  mCharacters.reserve(count);
  for (int i = 0; i < count; ++i) {
    mCharacters.push_back_unsafe(rstl::pair< int, CCharacterInfo >(in));
  }
}