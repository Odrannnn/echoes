#ifndef _ALLOCATORCOMMON
#define _ALLOCATORCOMMON

#include <stddef.h> // For size_t
#include <stdint.h>

static const int kAllocatorPointerSize = sizeof(void*);

// How many low bits of a block pointer are FLAGS rather than address.
//
// This is a property of retail's allocator, not of the host's pointer width, and
// conflating the two is a bug that only shows up off-target. Retail's block
// pointers are 0x20-aligned, so the 0x20 bit is always clear and retail's
// "pointer bits" (sizeof(void*) * 8 == 32) is a correct description of the flag
// field: the mask is 0x1F, five bits, and bit 5 is address.
//
// Deriving the mask from the host instead gives 0x3F on a 64-bit host, six bits.
// Bit 5 is then read as a flag, but with a 0x40 block stride it is *address*, and
// every accessor silently truncates it. The setters look safe - they OR the old
// low bits back in - so the damage is invisible until a read: `GetNext()` is
// `x14_next & ~0x3F`, so a successor at 0x...120 comes back as 0x...100.
// Measured on the boot path: with x4_len = 32 the walk stored next = block+0x60
// and read back block+0x40, the block's own payload rather than its successor's
// header. The walk stepped 32 bytes short, the "next block" it found was payload,
// its guard words were absent and a code address sat where the length belongs,
// and FindFreeBlock then rejected a good 180220-byte block for a 135168-byte
// request, reading its length out of what was really payload.
//
// So the count is fixed at retail's value. It is deliberately NOT derived from
// kAllocatorPointerSize: that derivation is what makes the mask host-dependent,
// and the fix has to hold on any host rather than on the one being debugged.
// On the decomp build this is a no-op - MWCC pointers are 32-bit, so
// sizeof(void*) * 8 was already 32 - which is why it cannot move the DOL.
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
#endif // _ALLOCATORCOMMON
