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
#include "Kyoto/CToken.hpp"
#include "Kyoto/Text/CFontImageDef.hpp"

extern "C" void* __nw__FUlPCcPCc(unsigned long size, const char* /*fileAndLine*/,
                                 const char* /*type*/) {
  return ::operator new(size);
}

// Retail's own mangled name for the free itself, which `Matching` units reproduce by calling it
// through `extern "C"` - the same mechanism as `__nw__FUlPCcPCc` above.
// `src/WorldFormat/Carve80255A0C.c` (retail 0x80255A0C..0x80255B28) is the unit that needs it:
// its three free steps are `__sys_free`-shaped, `rstl::destroy_impl`-shaped and the
// deleting-destructor convention, and the last two reach `CMemory::Free` by that name. It is
// unclaimed by any unit (`symbols.txt:12992`, 0x802CE388, 0x64 bytes), so `dtk`'s own object
// supplies the bytes in the DOL link and this is only what a host link binds to.
extern "C" void Free__7CMemoryFPCv(const void* ptr) { CMemory::Free(ptr); }

// Retail's own mangled name for `CToken::~CToken()`, reached the same way.  `src/MetroidPrime/
// Carve80024B70.c` (retail 0x80024B70..0x80024C18) is the unit that needs it: that is
// `rstl::vector<TToken<CCharLayoutInfo>>::~vector()`, whose element loop calls
// `bl __dt__6CTokenFv` with `li r4,0` once per token.  Like `Free__7CMemoryFPCv` above, retail
// defines it (`symbols.txt:13922`, 0x8030154C, 0x68 bytes, claimed by `Kyoto/CToken.cpp`), so the
// DOL link resolves it and this is only what a host link binds to.  The 16-bit second argument is
// MWCC's deleting-destructor flag, which the host's C++ destructor does not take; the flag is
// always 0 from this call site, so it is accepted and ignored and the work is `CToken`'s own
// destructor above.
extern "C" void __dt__6CTokenFv(void* self, short) {
  static_cast< CToken* >(self)->~CToken();
}

// Retail's own mangled name for `CFontImageDef::~CFontImageDef()`, reached the same way.
// `src/MetroidPrime/Carve80024D24.c` (retail 0x80024D24..0x80024D68) is the unit that needs it:
// that is `rstl::destroy_impl< CFontImageDef >`, whose whole body is `bl __dt__13CFontImageDefFv`
// with `li r4,-1`.  Unlike the two names above, **no unit in this tree claims the bytes** - retail's
// `__dt__13CFontImageDefFv` (0x80024D68, `symbols.txt:641`, 0x58 bytes, `scope:weak`) sits in the
// same unclaimed `auto_*` range as the carve, one function above it, so dtk's own object supplies
// them in the DOL link and this is only what a host link binds to.  The 16-bit second argument is
// MWCC's deleting-destructor flag, which the host's C++ destructor does not take; the only call site
// passes -1, so it is accepted and ignored and the work is `CFontImageDef`'s own destruction - the
// texture vector at `+0x04`, whose host destructor `Kyoto/Text/CFontImageDef.cpp` already
// instantiates as a weak symbol in this same link.
extern "C" void __dt__13CFontImageDefFv(void* self, short) {
  static_cast< CFontImageDef* >(self)->~CFontImageDef();
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
