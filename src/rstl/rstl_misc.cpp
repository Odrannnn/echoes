#include "rstl/rmemory_allocator.hpp"

#ifdef TARGET_PC
/**
 * The port's `rstl::basic_string` heap buffer, and **this is the fix for the boot's
 * SIGSEGV in `CGameAllocator::FreeNormalAllocation`.**
 *
 * Retail's out-of-line body (0x802FDAB8, 0x3C) is
 *
 *     802fdac0: cmpwi r3,0 ; bne ; li r3,0 ; b            <- size == 0 -> nullptr
 *     802fdad4: lis r4,-32709 ; li r5,0 ; addi r4,r4,-1360
 *     802fdae0: bl 802ce224 <__nwa__FUlPCcPCc>           <- operator new[](size,"??(??)",0)
 *
 * and `__nwa__FUlPCcPCc` is `CMemory::Alloc(len, kHI_None, kSC_Unk1, kTP_Array,
 * CCallStack(-1, file, type))`. So on retail **both** ends of every `rstl::string` live in the
 * game's heap: the buffer is allocated by `CMemory::Alloc` and released by
 * `rstl::basic_string::internal_dereference`, which is `if (x4_cow && --x4_cow->x4_refCount
 * == 0) CMemory::Free(x4_cow);` (src/rstl/rstl_strings.cpp:172).
 *
 * **On the host the `rs_new` half of that line silently degrades to the global `operator
 * new[]`, so the two ends stopped being a pair**: `CMemory.hpp`'s `operator new[]` overloads
 * live behind `#if defined(__MWERKS__) || defined(CLANGD)`, so the port takes the `#else` branch
 * (`#include <new>`) and `rs_new` is `new`. Every string buffer became a *host malloc block*
 * that `CMemory::Free` - the game's allocator - was then asked to release. Measured, not
 * inferred: the pointer the boot handed to `CGameAllocator::Free` was `0x63eca31d3390`, which is
 * 21 MB into the executable image and **not one byte inside the game heap**
 * (`xc_first = 0x7c9452803040`, `x10_last = 0x7c9453ffef60`), and the 8 bytes the
 * allocator read as a block header were `[0x0b][0x00]` followed by `"NoARAM.pak"` - i.e. the
 * `control` block (capacity 11, refcount 0) of a `malloc`'d string. The full path, from the
 * probe's own backtrace:
 *
 *     CGameGlobalObjects::PostInitialize -> CGameGlobalObjects::AddPaksAndFactories
 *       -> CResLoader::AddPakFileAsync  (+0x98, the inlined ~basic_string)
 *         -> CMemory::Free -> CGameAllocator::Free -> CGameAllocator::FreeNormalAllocation
 *
 * `FreeNormalAllocation` then computed a "length" of 0x76652f7365007965 - the ASCII
 * `"ey\0esfv"` - out of a string constant, and the block-list merge that follows dereferenced
 * it. **The allocator was right to refuse; a crash in `Free` is the allocator working.**
 *
 * The branch below restores the pair, and it is the *allocation* end that is wrong rather
 * than the free end: four other call sites pair `rmemory_allocator::allocate` with
 * `CMemory::Free` by hand - `CGameStateBlockReserve.cpp:19/26`,
 * `CGameStateBlockCopyCtor.cpp:25`, `CStateManager.cpp:127` and `PortTweakGlobals.cpp:141` - so
 * the host `new[]` was mismatching every one of them too, not just the strings. Only
 * `allocate(int)` and `allocate2<T>` are out of line; `allocate2<T>` pairs with
 * `rmemory_allocator::deallocate`'s `delete[]` and is left alone, because *that* pair is
 * consistent on the host.
 *
 * `#ifdef TARGET_PC` is what keeps this unit `Matching`: mwcceppc does not define TARGET_PC,
 * so the retail branch below is compiled exactly as it was before and 0x802FDAB8 does not move.
 */
void* rstl::rmemory_allocator::allocate(int size) {
  if (size == 0) {
    return nullptr;
  }
  return CMemory::Alloc(static_cast< size_t >(size), IAllocator::kHI_None, IAllocator::kSC_Unk1,
                        IAllocator::kTP_Array, CCallStack(-1, "??(??)", nullptr));
}
#else
void* rstl::rmemory_allocator::allocate(int size) {
  return size == 0 ? nullptr : rs_new uchar[size];
}
#endif // TARGET_PC
