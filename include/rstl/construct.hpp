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

/**
 * `inline` only where the object retail built this header for needed it, which is measured and
 * is exactly one object: `Kyoto/CPakFile.o`. Fifteen of the sixteen `reserve` instantiations in
 * the DOL call the out-of-line copy - `reserve<vector<SConnection>>` at 0x800485E4 is
 * `stwu r1,-48` + `bl allocate(int)` + `bl uninitialized_copy<...>` + `bl CMemory::Free`, byte
 * for byte what this header produces by default - and only
 * `reserve<vector<CPakFile::SResInfo>>` (0x80324A64) carries the loop inlined, as
 *
 *     1ac8: lwz r0,0(r26) ; addi r3,r27,4 ; addi r4,r26,4 ; li r5,7 ; stw r0,0(r27)
 *     1adc: bl __copy                 <- the 7-byte tail of an 11-byte element
 *     1ae0: addi r26,r26,11 ; addi r27,r27,11 ; cmplw r26,r29 ; bne
 *
 * The discriminator is the object, not the element type: the out-of-line set covers element
 * sizes 1, 2, 4, 8, 12, 16, 20, 24, 28, 32 and 40, and the inlined one is 11. Marking this
 * `inline` unconditionally is not free either - it deletes four `uninitialized_copy`
 * instantiations that are at 100% today (`CStaticAudioPlayer` for `vector<auto_ptr<CDvdRequest>>`
 * at 68 B, `CEntity` for `vector<SConnection>` at 60 B, and `CRuleSet` for
 * `vector<CRuleAction>` and `vector<CRuleSetRule>` at 104 B each) because objdiff then has no
 * symbol to pair them with, and `CRuleSet::reserve<vector<CRuleAction>>` and
 * `reserve<vector<CRuleSetRule>>` get *worse* (66.72 -> 35.37 and 59.74 -> 22.60) because
 * retail does not inline them either. Scoped to `Kyoto/CPakFile.cpp` the movement is one
 * function, 33.84% -> 99.74%, and nothing else moves.
 *
 * See `rstl/rmemory_allocator.hpp` for the same decision on `allocate`, and for why the two
 * are the same decision: it is one object compiled against a second revision of the rstl
 * headers, not two independent codegen accidents.
 */
#ifdef RSTL_INLINE_RESERVE_HELPERS
template < typename It, typename T >
static inline T uninitialized_copy(It begin, It end, T out) {
#else
template < typename It, typename T >
static T uninitialized_copy(It begin, It end, T out) {
#endif
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
