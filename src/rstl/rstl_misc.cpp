#include "rstl/rmemory_allocator.hpp"

#ifdef TARGET_PC
// Retail's body is `rs_new uchar[size]`, and `__nwa__FUlPCcPCc` is `CMemory::Alloc(size,
// kHI_None, kSC_Unk1, kTP_Array, CCallStack(-1, "??(??)", 0))`. On the host `rs_new` degrades to
// the global `new`, so every rstl buffer became a malloc block that `CMemory::Free` was later
// asked to release (the boot died in `CGameAllocator::FreeNormalAllocation` from
// `CResLoader::AddPakFileAsync`). Spell out retail's call so both ends are the game heap.
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
