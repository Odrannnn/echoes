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
 * `rs_new` is plain `new`), so every `delete` in the port frees through the host heap and an object
 * allocated here has to come from it too. The file-and-line and type strings are retail's
 * allocation-tracking tags and have no host-side consumer. Not in configure.py, so it cannot affect
 * main.dol.
 */
#include <new>

extern "C" void* __nw__FUlPCcPCc(unsigned long size, const char* /*fileAndLine*/,
                                 const char* /*type*/) {
  return ::operator new(size);
}
