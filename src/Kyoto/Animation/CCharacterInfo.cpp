#include "Kyoto/Animation/CCharacterInfo.hpp"

#include "Kyoto/Streams/CInputStream.hpp"

typedef rstl::vector< rstl::pair< rstl::string, rstl::vector< CEffectComponent > > > TEffectList;
typedef TEffectList::value_type TEffectEntry;
typedef rstl::pair< uint, CAABox > TIdAabbEntry;
typedef rstl::vector< TIdAabbEntry > TIdAabbVector;

/**
 * `.text 0x802925C4`-`0x80293C98`: the `rstl` container helpers behind
 * `CCharacterInfo`'s members. `config/G2ME01/symbols.txt` has a name for the four template
 * instantiations at the bottom of the range (0x802925C4-0x8029265C, `destroy`/`construct` over
 * `TEffectList`'s element) and for the two constructors. Nineteen more were `fn_<addr>` -
 * retail's map never named them - and are now named after the weak instantiations this file
 * already gets byte-for-byte (see the table in `CCharacterInfo.hpp`).
 *
 * The bodies still spelled out below under `fn_` are the ones mwccceppc does **not** emit for us:
 * either it inlines them at the call site instead of keeping a weak copy, or retail's shape is a
 * raw-pointer loop rather than a template call. Each is written with `extern "C"` under retail's
 * own name, which is what gives objdiff a partner to score; a function left as a `static`
 * placeholder emits no symbol and reads as 0.00%. `MetroidPrime/CAnimData.cpp` and
 * `MetroidPrime/CTargetReticles.cpp` do the same for their ranges.
 *
 * The order is **descending by retail offset**, which is what mwccceppc needs to emit `.text`
 * in ascending order; every callee here sits at a *lower* address than its caller, so the
 * forwarders at the top of each pair are declared before they are defined.
 */

// `fn_80293C30` (0x80293C30, 0x68): `rstl::uninitialized_copy` over `TEffectList`'s element.
// Both range endpoints arrive as pointers-to-pointers and `*last` is re-read on every
// iteration (`lwz r0,0(r29)` sits inside the loop test at 0x80293C6C), which needs `first` to
// be `T* const*` and `last` `T**`; the loop is rotated, its body ends with `++src; ++dst`, and
// the advanced destination comes back in r3. `fn_80292C30` below is the same loop over plain
// element pointers. Same spelling as `fn_800B2FD8` in `MetroidPrime/CTargetReticles.cpp`.
extern "C" void fn_80292A1C(TIdAabbVector& vec);

extern "C" TEffectEntry* fn_80293C30(TEffectEntry* const* first, TEffectEntry** last,
                                    TEffectEntry* dst) {
  const TEffectEntry* it = *first;
  while (it != *last) {
    rstl::construct(dst, *it);
    ++it;
    ++dst;
  }
  return dst;
}

extern "C" void fn_80293BE4(TEffectEntry* first, TEffectEntry* last);

extern "C" void fn_80293BC4(TEffectEntry* first, TEffectEntry* last) { fn_80293BE4(first, last); }

// `fn_80293BE4` (0x80293BE4, 0x4C) is `rstl::destroy`'s loop over the same element: the loop
// test comes first (`b` to 0x80293C10), each iteration calls the out-of-line
// `rstl::destroy<TEffectEntry>` at 0x802925FC and steps by 0x20, and nothing is returned.
extern "C" void fn_80293BE4(TEffectEntry* first, TEffectEntry* last) {
  TEffectEntry* it = first;
  for (; it != last; ++it) {
    rstl::destroy(it);
  }
}

// The copy inside `fn_80293A44`, kept as its own `static inline` so that mwccceppc inlines it:
// retail's reserve carries the seven-word element copy in its own body, with the two
// `rstl::pointer_iterator` range endpoints materialised in the four words at 8/12/16/20(r1)
// that a by-value aggregate argument occupies. Reaching `rstl::uninitialized_copy` directly
// emits its out-of-line instantiation instead (this unit has a second caller for it) and the
// loop never appears inline.
static inline TIdAabbEntry* copy_id_aabb_entries(TIdAabbVector::iterator first,
                                                TIdAabbVector::iterator last, TIdAabbEntry* out) {
  TIdAabbVector::iterator it = first;
  for (; it != last; ++out, ++it) {
    *out = *it;
  }
  return out;
}

// `fn_80293A44` (0x80293A44, 0xD4) is `rstl::vector<pair<uint, CAABox>>::reserve`, called out of
// line by both `fn_8029293C` and `fn_80292A28`. It re-reads `mCount`/`mItems` *after* the
// allocation (0x80293A78/0x80293A80), so the copy comes after `allocate`, and it stores
// `mCapacity` last.
extern "C" void fn_80293A44(TIdAabbVector* vec, int newSize) {
  if (newSize <= vec->mCapacity) {
    return;
  }
  TIdAabbEntry* newData;
  rstl::rmemory_allocator::allocate(newData, newSize);
  copy_id_aabb_entries(vec->begin(), vec->end(), newData);
  rstl::rmemory_allocator::deallocate(vec->mItems);
  vec->mItems = newData;
  vec->mCapacity = newSize;
}

// `mAabbs`' three element helpers. All three are the same copy of a
// `rstl::pair<rstl::string, CAABox>` - the string's copy constructor, then the `CAABox`'s six
// floats one at a time - and retail keeps that copy **inline in each of them**
// (0x802931C4 at +0x34, 0x8029302C at +0x2c, 0x802939A4 at +0x30), so none of the three has
// a `bl` to a shared copy. Reaching `rstl::construct` from here does not get that:
// `construct<pair<string, CAABox>>` is an out-of-line instantiation mwccceppc keeps
// whenever the unit has more than one caller for it, and this unit has four. A file-local
// `static inline` has one caller per use, so the copy lands in the loop body - the same
// lever `copy_id_aabb_entries` below uses for `fn_80293A44`.
typedef rstl::pair< rstl::string, CAABox > TAabbEntry;

static inline void construct_aabb_entry(TAabbEntry* dest, const TAabbEntry& src) {
  new (dest) TAabbEntry(src);
}

// `fn_802931C4` (0x802931C4, 0x84) is `vector<pair<string, CAABox>>::push_back_unsafe`: the
// count is incremented before the address is formed and the element is constructed in place.
extern "C" void fn_802931C4(rstl::vector< TAabbEntry >* vec, const TAabbEntry& in) {
  construct_aabb_entry(&vec->mItems[vec->mCount++], in);
}

// `fn_8029302C` (0x8029302C, 0x9C) is `rstl::uninitialized_copy` over `pair<string, CAABox>`
// with plain element pointers, the one `vector<pair<string, CAABox>>::operator=` calls.
extern "C" TAabbEntry* fn_8029302C(const TAabbEntry* first, const TAabbEntry* last,
                                   TAabbEntry* dst) {
  TAabbEntry* out = dst;
  for (const TAabbEntry* it = first; it != last; ++it, ++out) {
    construct_aabb_entry(out, *it);
  }
  dst = out;
  return dst;
}

// `fn_802939A4` (0x802939A4, 0xA0) is the same loop over `rstl::pointer_iterator`s, the one
// `vector<pair<string, CAABox>>::reserve` calls. Both range endpoints arrive as
// pointers-to-pointers, `*first` is read once in the prologue and `*last` inside the loop
// test, so the destination cursor has to be named *after* the source cursor to get retail's
// `r31`/`r30` - the reverse order puts them the other way round.
extern "C" TAabbEntry* fn_802939A4(TAabbEntry* const* first, TAabbEntry** last,
                                   TAabbEntry* dst) {
  const TAabbEntry* it = *first;
  TAabbEntry* out = dst;
  for (; it != *last; ++it, ++out) {
    construct_aabb_entry(out, *it);
  }
  dst = out;
  return dst;
}

CCharacterInfo::CParticleResData::CParticleResData(CInputStream& in, ushort tableCount)
: mPart(in), mSwhc(in), mElscB(in) {
  if (tableCount > 5) {
    mElscA = rstl::vector< CAssetId >(in);
  }
  if (tableCount > 8) {
    mSpsc = rstl::vector< CAssetId >(in);
    mSrsc = rstl::vector< CAssetId >(in);
  }
}

// `mAabbs`' two element helpers are **not** spelled out here any more. Retail's `0x80293248`
// (`Get<pair<string, CAABox>>`) and `0x80293268` (`pair<string, CAABox>::pair(CInputStream&, ...)`)
// are byte-for-byte what this file already gets from `rstl::vector<pair<string, CAABox>>(in)` -
// including the six `lfs`/`stfs` that move the `CAABox` out of its temporary - so
// `config/G2ME01/symbols.txt` names those two retail addresses after the weak instantiations
// the compiler emits for them (see `CCharacterInfo.hpp`), and a hand-written `extern "C"`
// duplicate would only add two symbols retail's map does not have.

// `fn_80292D94` (0x80292D94, 0x38) is `TEffectList`'s `push_back_unsafe`: the count is
// incremented **before** the address is formed (`addi r5,r6,1` / `slwi r0,r6,5` / `stw r5,4(r3)`
// / `add r3,r7,r0`), which is what a post-increment subscript says. It then calls the
// out-of-line `rstl::construct<TEffectEntry>` retail keeps at 0x8029261C.
extern "C" void fn_80292D94(TEffectList* vec, const TEffectEntry& in) {
  rstl::construct(&vec->mItems[vec->mCount++], in);
}

// `fn_80292C94` (0x80292C94, 0x60) is `TEffectList`'s "destroy the elements, keep the
// storage": the four words at 8/12/16/20(r1) are the two `rstl::pointer_iterator`s that
// `rstl::destroy(vec.begin(), vec.end())` takes, and `mCount` is zeroed after the call. Retail
// calls the out-of-line `rstl::destroy` instantiation at 0x802925C4 rather than inlining it -
// the same 0x38-byte symbol this unit already matches.
extern "C" void fn_80292C94(TEffectList* vec) {
  rstl::destroy(vec->begin(), vec->end());
  vec->mCount = 0;
}

extern "C" TEffectEntry* fn_80292C30(const TEffectEntry* first, const TEffectEntry* last,
                                    TEffectEntry* dst);

// `fn_80292C30` (0x80292C30, 0x64) is the same `uninitialized_copy` loop as `fn_80293C30` with
// plain element pointers: `mr r30,r3` / `mr r29,r4` in the prologue and a direct `cmplw` of the
// two cursors, with no reload of an end pointer.
extern "C" TEffectEntry* fn_80292C30(const TEffectEntry* first, const TEffectEntry* last,
                                    TEffectEntry* dst) {
  TEffectEntry* out = dst;
  const TEffectEntry* it = first;
  for (; it != last; ++it, ++out) {
    rstl::construct(out, *it);
  }
  dst = out;
  return dst;
}

// `mAnimBoundsById`'s two element helpers are named in `config/G2ME01/symbols.txt` the same
// way: retail's `0x80292B00` is `rstl::pair<uint, CAABox>`'s `CInputStream::Get`, and
// `0x80292B20` is its `pair(CInputStream&, const Alloc&)`, both byte-identical to the weak
// instantiations `TIdAabbVector(in)` already emits.

// `fn_8029293C` (0x8029293C, 0xE0) is `rstl::vector<pair<uint, CAABox>>::operator=` for
// `CCharacterInfo::mAnimBoundsById`: a self-assignment guard (`cmplw` with no `cmp`),
// `fn_80292A1C` to drop the destination's count, then either release the storage outright when
// the source is empty or size the destination and copy the elements. The copy is spelled as a
// raw-pointer loop because retail keeps the seven-word element copy in this body with no
// iterator operands materialised on the stack, unlike `fn_80293A44`.
extern "C" TIdAabbVector* fn_8029293C(TIdAabbVector* dst, const TIdAabbVector& src) {
  if (dst != &src) {
    fn_80292A1C(*dst);
    if (src.mCount == 0) {
      rstl::rmemory_allocator::deallocate(dst->mItems);
      dst->mCount = 0;
      dst->mCapacity = 0;
      dst->mItems = nullptr;
    } else {
      fn_80293A44(dst, src.mCount);
      const TIdAabbEntry* in = src.mItems;
      const TIdAabbEntry* const stop = in + src.mCount;
      TIdAabbEntry* out = dst->mItems;
      for (; in != stop; ++in, ++out) {
        *out = *in;
      }
      dst->mCount = src.mCount;
    }
  }
  return dst;
}

// `fn_80292A1C` (0x80292A1C, 0xC): retail emits the "release the elements but keep the storage"
// step of `mAnimBoundsById`'s copy assignment as its own out-of-line function, so the caller at
// 0x80292964 does not have to spell the store.
extern "C" void fn_80292A1C(TIdAabbVector& vec) { vec.mCount = 0; }

CCharacterInfo::CCharacterInfo(CInputStream& in)
: mTableCount(in.Get< ushort >())
, mName(in)
, mCmdl(in.Get< CAssetId >())
, mCksr(in.Get< CAssetId >())
, mCinf(in.Get< CAssetId >())
, mAnimInfo(in)
, mPasDatabase(in.Get< CPASDatabase >(TGetType(mPasDatabase)))
, mPartRes(in, mTableCount)
, xa4_(in.Get< uint >())
, mCmdlOverlay(kInvalidAssetId)
, mCksrOverlay(kInvalidAssetId)
, mSpatialPrimitiveId(kInvalidAssetId)
, mAnimatedScale(false) {
  if (mTableCount > 1) {
    mAabbs = rstl::vector< rstl::pair< rstl::string, CAABox > >(in);
  }
  if (mTableCount > 2) {
    mEffects = TEffectList(in);
  }
  if (mTableCount > 3) {
    mCmdlOverlay = in.Get< CAssetId >();
    mCksrOverlay = in.Get< CAssetId >();
  } else {
    mCmdlOverlay = 0;
    mCksrOverlay = 0;
  }
  if (mTableCount > 4) {
    mAnimIdxs = rstl::vector< int >(in);
  }
  if (mTableCount > 6) {
    mSpatialPrimitiveId = in.Get< CAssetId >();
  }
  if (mTableCount > 7) {
    mAnimatedScale = in.Get< bool >();
  }
  if (mTableCount > 9) {
    mAnimBoundsById = TIdAabbVector(in);
  }
}