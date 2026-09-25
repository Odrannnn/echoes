#ifndef _RSTL_CONSTRUCT
#define _RSTL_CONSTRUCT

#include "types.h"
#include "rstl/iterator.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

namespace rstl {
template < typename T >
struct is_trivially_destructible {
  enum { value = false };
};

template <>
struct is_trivially_destructible< float > {
  enum { value = true };
};

template < typename T >
struct is_trivially_destructible< T* > {
  enum { value = true };
};

// A vector of these has nothing to destroy, so `destroy(begin(), end())` in vector<T>::clear and
// ~vector<T> must compile away to nothing. Without it every clear() of such a vector keeps a live
// element loop: retail's clear<vector<unsigned int>> is 12 bytes (li / stw / blr) where ours was 68.
//
// Scoped to these two types on purpose - each one has to be measured on its own, because the trait
// is not uniform in retail. Adding `unsigned short` here sends
// CStateManager::__dt__13CStateManagerFv from 18.39% to 13.07%, and `bool`/`char`/`signed char`/
// `short`/`int`/`long`/`unsigned long` are untested. Widen one type at a time and re-gate.
#define RSTL_TRIVIALLY_DESTRUCTIBLE_ARITHMETIC( type )                                                 \
  template <>                                                                                         \
  struct is_trivially_destructible< type > {                                                           \
    enum { value = true };                                                                            \
  }
RSTL_TRIVIALLY_DESTRUCTIBLE_ARITHMETIC( unsigned int );
RSTL_TRIVIALLY_DESTRUCTIBLE_ARITHMETIC( unsigned char );
#undef RSTL_TRIVIALLY_DESTRUCTIBLE_ARITHMETIC

template < typename T >
static inline void construct(void* dest, const T& src) {
  new (dest) T(src);
}

template < typename T >
static inline void construct(T** dest, T* const& src) {
  *dest = src;
}

template < typename T >
static inline void destroy(T* in) {
  in->~T();
}

template < typename It >
static inline void destroy_range(It begin, It end) {
  It cur = begin;
  for (; cur != end; ++cur) {
    destroy(&*cur);
  }
}
template < typename It >
static inline void destroy(It begin, It end) {
  if (is_trivially_destructible< typename iterator_traits< It >::value_type >::value) {
    return;
  }
  destroy_range(begin, end);
}

template < typename It, typename T >
static T uninitialized_copy(It begin, It end, T out) {
  T tmp = out;
  It cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    construct(tmp, *cur);
  }

  return tmp;
}

template < typename S, typename T >
static inline T uninitialized_copy(S* begin, S* end, T out) {
  T tmp = out;
  S* cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    construct(tmp, *cur);
  }

  return tmp;
}

template < typename S, typename D >
static inline D uninitialized_copy_n(S src, int n, D dest) {
  S it = src;
  D cur = dest;
  int count = n;
  for (; count != 0; ++it, ++cur, --count) {
    construct(&*cur, *it);
  }

  return cur;
}

template < typename D, typename S >
static inline void uninitialized_fill_n(D dest, int n, const S& value) {
  D cur = dest;
  for (int i = 0; i < n; ++i, ++cur) {
    construct(&*cur, value);
  }
}
} // namespace rstl

#endif // _RSTL_CONSTRUCT
