#ifndef _ALLOCATORCOMMON
#define _ALLOCATORCOMMON

#include <stddef.h> // For size_t
#include <stdint.h>

static const int kAllocatorPointerSize = sizeof(void*);
// Retail's flag count, not the host's pointer width: 0x3F on a 64-bit host eats a real address
// bit and breaks every GetNext(). See docs/research/allocator_flag_mask.md.
static const int kAllocatorPointerBits = 32;

template < size_t PointerSize >
struct PatternExpander;

template <>
struct PatternExpander< 4 > {
  typedef unsigned int Type;
  typedef size_t MaskType;
  static const Type Multiplier = 0x01010101U;
  static const MaskType TopNybbleMask = 0xF0000000U;
};

template <>
struct PatternExpander< 8 > {
  typedef unsigned long long Type;
  typedef size_t MaskType;
  static const Type Multiplier = 0x0101010101010101ULL;
  static const MaskType TopNybbleMask = 0xF000000000000000ULL;
};

#define EXPAND_PATTERN(byte_val)                                                                   \
  (static_cast< PatternExpander< kAllocatorPointerSize >::Type >(byte_val) *                       \
   PatternExpander< kAllocatorPointerSize >::Multiplier)

static const intptr_t kAllocatorPostGuard = EXPAND_PATTERN(0xEA);
static const intptr_t kAllocatorPriorGuard = EXPAND_PATTERN(0xEF);
static const intptr_t kAllocatorPointerTopNybbleMask = PatternExpander< sizeof(void*) >::TopNybbleMask;

// The small pool's index unit is retail's 4-byte word, not the host's pointer size: with 8 the
// pool claims 0x160000 bytes of its 0xb0000 allocation and CSmallAllocPool::Free zeroes past the
// bookkeeping block. `int` so CSmallAllocPool's divisions keep their MWCC types.
static const int kAllocatorSmallBlockIndexSize = 4;
#endif // _ALLOCATORCOMMON
