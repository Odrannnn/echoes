#include "REL/REL_Setup.h"
#include "dolphin/os.h"

#ifdef __MWERKS__
#define REL_EXTERN __declspec(section ".init") extern
#else
#define REL_EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*VoidFunc)(void);

#ifdef __MWERKS__
// Retail: the module's static constructors live in the `.init` section and the
// linker synthesises the `_ctors` table, null-terminated. mwldeppc emits it.
REL_EXTERN VoidFunc _ctors[];
REL_EXTERN VoidFunc _dtors[];
#else
// Host: the GameCube's linker-generated `_ctors`/`_dtors` do not exist in an ELF
// link, and referencing them made the port fail to link with two undefined
// symbols. GCC's crtbegin/crtend provide the same tables as counted ranges
// instead, so walk those. The retail branch above is what MWCC compiles and is
// untouched, so this unit still matches the retail object byte for byte.
extern VoidFunc __init_array_start[];
extern VoidFunc __init_array_end[];
extern VoidFunc __fini_array_start[];
extern VoidFunc __fini_array_end[];
#endif

#ifdef __cplusplus
}
#endif

void ModuleConstructors(void) {
#ifdef __MWERKS__
  VoidFunc* func;
  for (func = _ctors; *func != nullptr; ++func) {
    (*func)();
  }
#else
  for (VoidFunc* func = __init_array_start; func != __init_array_end; ++func) {
    (*func)();
  }
#endif
}

void ModuleDestructors(void) {
#ifdef __MWERKS__
  VoidFunc* func;
  for (func = _dtors; *func != nullptr; ++func) {
    (*func)();
  }
#else
  for (VoidFunc* func = __fini_array_start; func != __fini_array_end; ++func) {
    (*func)();
  }
#endif
}

REL_EXPORT void _prolog(void) {
  ModuleConstructors();
  RELMain();
}

REL_EXPORT void _epilog(void) {
  ModuleDestructors();
  RELExit();
}

REL_EXPORT void _unresolved(void) {
  u32 i, s;

  OSReport("\nError: Unlinked function called in module %s.\n", __FILE__);
  OSReport("Address:      Back Chain    LR Save\n");

  for (i = 0, s = OSGetStackPointer(); s != 0 && s != 0xFFFFFFFF && i++ < 16;
       s = *(const u32*)(s)) {
    const u32* p = (const u32*)(s);
    OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
  }

  OSReport("\n");
}
