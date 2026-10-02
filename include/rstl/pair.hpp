#ifndef _RSTL_PAIR
#define _RSTL_PAIR

#include "rstl/construct.hpp"
#include "rstl/functional.hpp"
#include "types.h"

class CInputStream;
class COutputStream;
class CTransform4f;

namespace rstl {
template < typename L, typename R >
class pair {
public:
  pair() {}
  pair(CInputStream& in);
  pair(const L& first, const R& second) : first(first), second(second) {}
  void PutTo(COutputStream& out) const;

  bool operator==(const pair& other) const {
    return first == other.first && second == other.second;
  }

  bool operator!=(const pair& other) const {
    return first != other.first || second != other.second;
  }

  bool operator<(const pair& other) const {
    return first < other.first || (first == other.first && second < other.second);
  }

  L first;
  R second;
};

template <>
struct is_trivially_destructible< pair< uint, uint > > {
  enum { value = true };
};

inline void construct_impl(void* dest, const pair< uint, uint >& src) {
  *static_cast< pair< uint, uint >* >(dest) = src;
}

// The two pairs `CAnimSourceReaderBase` keeps its POI state in. Retail's out-of-line
// `rstl::vector` destructor for them walks no elements before freeing the storage - the bool
// one is the 0x54-byte `fn_802A2E14` and the int one the 0x54-byte `fn_801ED894`, both just
// `CMemory::Free(mItems)` and the deleting-flag tail - while the third
// `pair<uint, CParticleData::EParentedMode>` destructor in the same unit, `fn_802A2E68`, is
// 0x84 bytes and does run the element loop. Marking only these two keeps that difference, and
// with it the 0x54-byte destructor bodies. Neither gets a `construct_impl` from this: retail
// copies them through a placement new, which is the 0xB0-byte `fn_802A3AD0` loop.
template <>
struct is_trivially_destructible< pair< uint, bool > > {
  enum { value = true };
};

template <>
struct is_trivially_destructible< pair< uint, int > > {
  enum { value = true };
};

// Retail copies this pair by assignment and not by a placement new of `pair`'s copy constructor:
// the out-of-line copy of `CAnimSourceReaderBase::mBoolStates`, `fn_802A3B80` (0x802A3B80, 0x148
// bytes), stores each element with `lwz`/`stw` at +0 and `lbz`/`stb` at +4 in a loop unrolled eight
// times, where the same loop for `mParticleStates` (`fn_802A3AD0`, 0xB0 bytes) is not unrolled at
// all and keeps a null check on the destination. `construct_impl` is the same lever the three
// pairs above use, and the only reason this pair is treated differently from `pair<uint, int>`
// above is the byte member: the int pair's copy is a plain `lwz`/`stw` pair in the same loop.
inline void construct_impl(void* dest, const pair< uint, bool >& src) {
  *static_cast< pair< uint, bool >* >(dest) = src;
}

// `fn_802A4930` (0x802A4930, 0xEC bytes), `rstl::vector<pair<uint, int>>::resize`'s fill loop, is
// unrolled eight times exactly like the bool pair's `fn_802A4A1C` one instruction for instruction,
// with a bare `stw` at +4 where the bool pair has `stb`. A placement-new of `pair`'s copy
// constructor gives the 0x98-byte unrolled-free loop this unit built instead, so the int pair is
// copied by assignment here too - the same conclusion the `fn_802A3B80` note above reached for the
// out-of-line copy.
inline void construct_impl(void* dest, const pair< uint, int >& src) {
  *static_cast< pair< uint, int >* >(dest) = src;
}

template <>
struct is_trivially_destructible< pair< int, float > > {
  enum { value = true };
};

inline void construct_impl(void* dest, const pair< int, float >& src) {
  *static_cast< pair< int, float >* >(dest) = src;
}

// `CMayaSpline` keeps its segment extrema in a `reserved_vector<pair<float, float>, 2>` and pushes
// through it (`CMayaSpline::FindSegmentExtrema`, `CMayaSpline::FindMaximumAmplitude`). Retail
// stores each pair with a bare `stfs`/`stfs` pair and keeps `mCount` in a register across the
// push, with no placement-new null check and no element loop in `clear()`, so this pair is
// trivially destructible and copies by assignment, like the pairs above.
template <>
struct is_trivially_destructible< pair< float, float > > {
  enum { value = true };
};

inline void construct_impl(void* dest, const pair< float, float >& src) {
  *static_cast< pair< float, float >* >(dest) = src;
}

// `CScriptActorRotate` is the only user of `pair<TUniqueId, CTransform4f>`, and retail copies it
// as one 52-byte block, split at 48: `rstl::vector<pair<TUniqueId, CTransform4f>>::reserve`'s loop
// (0x8010B278) is `mr r3,dst / mr r4,src / bl fn_800E88FC` - the out-of-line `CTransform4f` copy,
// called on the *pair's* base - followed by `lwz r0,48(src) / stw r0,48(dst)`, and
// `UpdateActors` (0x8010A568) builds the pair the same way. Neither a placement `new` of the pair's
// copy constructor nor its implicit `operator=` emits that: the first outlines the whole loop and
// the second calls `__as__12CTransform4fFRC12CTransform4f` at the member's own offset 4. So the
// block copy is spelled out here, in the two pieces retail uses, and the two together are the
// pair's whole object representation - no member is dropped. `fn_800E88FC` is declared in
// `Kyoto/Math/CTransform4f.hpp` and redeclared here only so this header needs not include it.
extern "C" void fn_800E88FC(CTransform4f* self, const CTransform4f& other);

template < typename T >
inline void construct_impl(void* dest, const pair< T, CTransform4f >& src) {
  fn_800E88FC(static_cast< CTransform4f* >(dest), *reinterpret_cast< const CTransform4f* >(&src));
  reinterpret_cast< uint* >(dest)[12] = reinterpret_cast< const uint* >(&src)[12];
}



// A pair whose second member is a pointer copies by assignment, like the two pairs above, so it
// must not go through the placement new in `construct`'s generic overload. `red_black_tree`'s node
// fills its raw value storage through `construct`, and the two shapes that follows from that are
// both visible in retail: `create_node` for `rstl::map<int, CFactoryFnReturn (*)(...)>` copies
// straight to `this + 0x10` with no guard, while the `basic_string`, `SObjectTag` and `CPrimitive`
// instantiations guard a placement new with `addic.`/`beq`. Only a pointer second member is
// claimed here; `pair<int, auto_ptr<T>>` and `pair<int, TEditorId>` have non-trivial copies and
// keep the generic path.
template < typename T >
inline void construct_impl(void* dest, const pair< int, T* >& src) {
  *static_cast< pair< int, T* >* >(dest) = src;
}

// `rstl::map<TUniqueId, CGameModeListener*>` is `pair<uint, T*>` rather than `pair<int, T*>`, so
// the overload above does not reach it, and its `create_node` (retail `fn_80197910`, 0x80197910,
// 104 bytes) keeps the guard the one above removes: `addic. r5,r3,16` / `beq` around the value
// copy, 112 bytes instead of 104. Same lever, same two-word copy, claimed for the unsigned first
// member as well.
template < typename T >
inline void construct_impl(void* dest, const pair< uint, T* >& src) {
  *static_cast< pair< uint, T* >* >(dest) = src;
}

template < typename P >
struct select1st : unary_function< P, P > {
  const P& operator()(const P& it) const { return it; }
};

template < typename K, typename V >
struct select1st< pair< K, V > > : unary_function< pair< K, V >, K > {
  typedef K value_type;

  const K& operator()(const pair< K, V >& it) const { return it.first; }
};

} // namespace rstl

#endif // _RSTL_PAIR
