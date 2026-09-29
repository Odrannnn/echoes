#ifndef _RSTL_RMEMORY_ALLOCATOR
#define _RSTL_RMEMORY_ALLOCATOR

#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"

namespace rstl {
struct rmemory_allocator {
  rmemory_allocator() {}
  rmemory_allocator(const rmemory_allocator&) {}
  static void* allocate(int size);

  template < typename T >
  static void allocate(T*& out, int count) {
    int size = count * sizeof(T);
    out = reinterpret_cast< T* >(allocate(size));
  }
  // TODO: this fixes a regswap in vector::reserve
  template < typename T >
  static T* allocate2(int count) {
    int size = count * sizeof(T);
    if (size == 0) {
      return nullptr;
    } else {
#ifdef TARGET_PC
      // `new uchar[]` is the out-of-line `allocate(int)` under mwcceppc; the host calls it too.
      return reinterpret_cast< T* >(allocate(size));
#else
      return reinterpret_cast< T* >(new uchar[size]);
#endif
    }
  }
  template < typename T >
  static void deallocate(T* ptr) {
#ifdef TARGET_PC
    // Under mwcceppc `delete[]` is CMemory.hpp's inline `CMemory::Free`; on the host it is
    // glibc's `free`, while the buffer came from `CMemory::Alloc`. Keep the pair on the host.
    CMemory::Free(ptr);
#else
    delete[] reinterpret_cast< uchar* >(ptr);
#endif
  }
};

struct aligned_allocator {
  aligned_allocator() {}
  aligned_allocator(const aligned_allocator&) {}

  template < typename T >
  static void allocate(T*& out, int count) {
    const int size = count * sizeof(T);
    out = size == 0 ? nullptr
                    : static_cast< T* >(CMemory::Alloc(size, IAllocator::kHI_RoundUpLen));
  }

  template < typename T >
  static void deallocate(T* ptr) {
#ifdef TARGET_PC
    // Under mwcceppc `delete[]` is CMemory.hpp's inline `CMemory::Free`; on the host it is
    // glibc's `free`, while the buffer came from `CMemory::Alloc`. Keep the pair on the host.
    CMemory::Free(ptr);
#else
    delete[] reinterpret_cast< uchar* >(ptr);
#endif
  }
};
} // namespace rstl

#endif // _RSTL_RMEMORY_ALLOCATOR
