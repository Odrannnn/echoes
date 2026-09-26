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

// The **small** pool's index unit, in bytes. This is a third instance of the same defect as
// kAllocatorPointerBits and the guard constants: a retail protocol number written as a host
// word, so it is correct on the 32-bit decomp build and wrong on a 64-bit host. It is NOT
// `kAllocatorPointerSize`.
//
// Measured, not asserted. Three retail numbers, all in `CGameAllocator::Initialize`, pin it:
// the small pool's main data is `Alloc(0xb0000)`, its bookkeeping is `Alloc(0x16000)`, and it
// is constructed as `CSmallAllocPool(0x2c000, ...)`. `FindFree` ends its scan at
// `x4_bookKeeping + (x8_numBlocks >> 1)`, i.e. at 0x16000 - the whole bookkeeping allocation - and
// a payload is `x0_mainData + bookkeepingByteOffset * (unit * 2)`, so the pool's extent is
// `0x16000 * unit * 2`. For that to be 0xb0000 the unit is **4**. Retail's own code agrees
// independently: `Alloc` marks `(len - 2) / 2` further bookkeeping bytes and `FindFree` scans
// `len / 2`, so a payload occupies `len/2` bookkeeping bytes, and a payload of `size` bytes
// costs `round_up(size, unit)` of them - `len * unit` payload bytes for a `size`-byte request,
// which is only a fit at unit 4.
//
// The damage with `sizeof(void*)` on a 64-bit host, printed by the boot probe before this
// constant existed:
//
//   [SMALL] Alloc size=56 len=8 ptr=0x7b1eb84fe080 offset=0x0 extent(numBlocks*unit)=0x160000 unit=8
//
// **0x160000 against a 0xb0000-byte allocation** - the pool reported twice its own memory, and
// `PtrWithinPool` had the same 2x, so any free of a pointer in [0xb0000, 0x160000) was accepted
// and `CSmallAllocPool::Free` then zeroed up to `x4_bookKeeping + 0x2c000` - 0x16000 bytes past
// the end of the 0x16000-byte bookkeeping block, over the small pool object, the medium pool
// object and the medium pool's data. That is heap corruption, manufactured by the allocator
// itself, and it is only reachable now that `rstl::basic_string` buffers come from the game
// heap (see src/rstl/rstl_misc.cpp).
//
// So the number is retail's and is spelled as a number. On the decomp build it is a no-op -
// `sizeof(void*)` was already 4 - so it cannot move the DOL. It is deliberately NOT derived
// from kAllocatorPointerSize, for the reason kAllocatorPointerBits' comment gives: a derivation
// from the host is what makes the constant wrong off-target.
//
// Declared `int`, like kAllocatorPointerSize, and that is not cosmetic: `CSmallAllocPool`'s three
// index expressions divide `size_t`/`ptrdiff_t` values by it, and `size_t` would promote those
// divisions to 64-bit and change `CSmallAllocPool::Alloc`'s codegen, which is a `Matching`
// function. As an `int` every expression has the type it had before.
static const int kAllocatorSmallBlockIndexSize = 4;

#endif // _ALLOCATORCOMMON

