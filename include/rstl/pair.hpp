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

template <>
struct is_trivially_destructible< pair< int, float > > {
  enum { value = true };
};

inline void construct_impl(void* dest, const pair< int, float >& src) {
  *static_cast< pair< int, float >* >(dest) = src;
}

// `CScriptActorRotate` is the only user of `pair<TUniqueId, CTransform4f>`, and retail copies it
// as one 52-byte block: the out-of-line `CTransform4f` copy (`fn_800E88FC`, 48 bytes) plus the
// trailing word (`CScriptActorRotate::UpdateActors`, 0x8010A568). A placement `new` of the pair's
// copy constructor emits the same 48 bytes member-wise instead, so this pair goes through
// assignment, like the two pairs above.
template < typename T >
inline void construct_impl(void* dest, const pair< T, CTransform4f >& src) {
  *static_cast< pair< T, CTransform4f >* >(dest) = src;
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
