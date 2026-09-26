#ifndef _RSTL_HASH_MAP
#define _RSTL_HASH_MAP

#include "types.h"

#include "rstl/list.hpp"
#include "rstl/pair.hpp"
#include "rstl/red_black_tree.hpp"
#include "rstl/rmemory_allocator.hpp"
#include "rstl/vector.hpp"

namespace rstl {
template < typename K, typename P, int unk, typename Select, typename Hash, typename Equal,
           typename Alloc = rmemory_allocator >
class hash_table {
public:
  typedef rstl::list< P, Alloc > bucket_type;
  typedef rstl::vector< bucket_type, Alloc > bucket_vector;

  // The bucket array, for the one user that walks it itself (the port's `CSimplePool`, in
  // src/Kyoto/CSimplePoolPort.cpp). Inline and unused by every mwcceppc unit, so nothing emits it.
  bucket_vector& buckets() { return x; }
  const bucket_vector& buckets() const { return x; }

private:
  bucket_vector x;
};

template < typename K, typename V, typename Hash, typename Equal,
           typename Alloc = rmemory_allocator >
class hash_map {
public:
  typedef rstl::pair< K, V > Pair;
  typedef hash_table< K, Pair, 0, select1st< Pair >, Hash, Equal, Alloc > table_type;

  table_type& get_table() { return table; }
  const table_type& get_table() const { return table; }

private:
  table_type table;
};
} // namespace rstl

#endif // _RSTL_HASH_MAP
