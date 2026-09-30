#ifndef _CMEMORY
#define _CMEMORY

#include "Kyoto/Alloc/CCallStack.hpp"
#include "Kyoto/Alloc/IAllocator.hpp"
#include "types.h"

class COsContext;
class CMemory {
  static IAllocator* mpAllocator;
  static bool mInitialized;

public:
  static void Startup(COsContext& ctx);
  static void Shutdown();
  static void SetAllocator(COsContext& ctx, IAllocator& allocator);
  static IAllocator* GetAllocator();
  static void* Alloc(size_t len, IAllocator::EHint hint = IAllocator::kHI_None,
                     IAllocator::EScope scope = IAllocator::kSC_Unk1,
                     IAllocator::EType type = IAllocator::kTP_Heap,
                     const CCallStack& callstack = CCallStack(-1, "??(??)"));
  static void Free(const void* ptr);
  static void SetOutOfMemoryCallback(IAllocator::FOutOfMemoryCb callback, const void* context);
  static IAllocator::SMetrics GetMetrics(bool unk1, bool unk2);
  static void OffsetFakeStatics(int);
};

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
void* operator new[](size_t sz, const char*, const char*);
// TODO remove

inline void* operator new(size_t sz) { return operator new(sz, "??(??)", nullptr); }
inline void* operator new[](size_t sz) { return operator new[](sz, "??(??)", nullptr); }

// placement new
inline void* operator new(size_t n, void* ptr) { return ptr; };
#else
// Port: the host runtime owns global new/delete, so the game's CMemory-backed
// replacements must not be declared. Declaring them next to <new> redeclares
// the standard placement new and the nothrow operator delete.
#include <new>
#endif

#ifdef __MWERKS__
inline void operator delete(void* ptr) { CMemory::Free(ptr); }
inline void operator delete[](void* ptr) { CMemory::Free(ptr); }
#define NEW new ("??(??)", nullptr)
#ifdef CMEMORY_NEW_FILE
// A translation unit whose retail object names a *specific* `operator new` placement string -
// rather than one of mwcceppc's per-translation-unit `@stringBase0` literals - defines
// `CMEMORY_NEW_FILE` to that symbol's name before including this header. The literal of our own
// would make the object emit a `.rodata` section, which the linker appends to the global string
// pool and which shifts every later pool entry; see
// `src/MetroidPrime/Factories/CStateMachineFactory.cpp`. With the macro unset - every other
// translation unit - the definition below is unchanged.
#define rs_new new (CMEMORY_NEW_FILE, nullptr)
#else
#define rs_new new ("\?\?(\?\?)", nullptr)
#endif
#else
#define NEW new
#define rs_new new
#endif

#endif // _CMEMORY
