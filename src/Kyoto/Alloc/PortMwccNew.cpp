/**
 * Port-only: `__nw__FUlPCcPCc`, mwcceppc's mangling of `operator new(unsigned long, const char*,
 * const char*)`, retail 0x802CE278, 0x54 (the body is `src/Kyoto/Alloc/CMemory.cpp`'s).
 *
 * Units written against retail's bytes call it by that mangled name through `extern "C"`, which is
 * what reproduces their call sites under mwcceppc: `CMainFlowDtor.cpp` (`CMainFlow::SetGameState`,
 * reached on the first frame), `Player/CGameStateCtor.cpp` and `ScriptObjects/CScriptSkyRipple.cpp`.
 * On a host link that name binds to nothing, so this defines it.
 *
 * It forwards to the **host** `operator new`, not CMemory's: on `TARGET_PC` the host runtime owns
 * global new/delete (`CMemory.hpp` declares the CMemory-backed operators for `__MWERKS__` only, and
 * `rs_new` is plain `new`), so a `delete` of anything outside the game heap frees through the host
 * heap (see `PortDelete` below) and an object allocated here has to come from it too. The file-and-line and type strings are retail's
 * allocation-tracking tags and have no host-side consumer. Not in configure.py, so it cannot affect
 * main.dol.
 */
#include <cstdlib>
#include <new>

#include "Kyoto/Alloc/CMemory.hpp"

extern "C" void* __nw__FUlPCcPCc(unsigned long size, const char* /*fileAndLine*/,
                                 const char* /*type*/) {
  return ::operator new(size);
}

// The other half of the same split. Retail's `operator delete` is `CMemory::Free`, and `Matching`
// units rely on it: `CResLoader::AddGroupCache` takes its buffer from `CMemory::Alloc` and gives it
// to an `rstl::auto_ptr`, whose `delete` on the host would hand a game-heap pointer to glibc
// (`munmap_chunk(): invalid pointer`, first hit when the memory card read its first MLVL). So the
// host's `operator delete` looks at the address: inside the arena the game heap was carved from
// (`platform/main.cpp` records it) it is CMemory's, anywhere else it is malloc's.
void* gPortGameHeapLo = nullptr;
void* gPortGameHeapHi = nullptr;

static inline void PortDelete(void* ptr) noexcept {
  if (ptr >= gPortGameHeapLo && ptr < gPortGameHeapHi) {
    CMemory::Free(ptr);
  } else {
    std::free(ptr);
  }
}

void operator delete(void* ptr) noexcept { PortDelete(ptr); }
void operator delete[](void* ptr) noexcept { PortDelete(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { PortDelete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { PortDelete(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { PortDelete(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { PortDelete(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept { PortDelete(ptr); }
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept { PortDelete(ptr); }
