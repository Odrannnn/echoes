// ScriptGuiTail.cpp - ScriptGui's (module 60) teardown group, .text 0x9CC8..0x9E6C plus the
// 0x84 bytes of `.rodata` at 0x170 that `_unresolved` reports from. Five functions, in the order
// the module holds them:
//
//   0x9CC8 _unresolved          0xC4  the unlinked-function report: two format strings, the
//                                     module file name, the back-chain walk and its per-frame line
//   0x9D8C _epilog              0x24  ModuleDestructors(); RELExit();
//   0x9DB0 _prolog              0x24  ModuleConstructors(); RELMain();
//   0x9DD4 ModuleDestructors    0x4C  walks `_dtors`, the null-terminated table the linker builds
//   0x9E20 ModuleConstructors   0x4C  the same over `_ctors`
//
// **This is the shared "REL" lib's `REL/REL_Setup.cpp` under a module-unique name.** Retail
// compiled one copy of that file into every module, and 68 of the 86 carry it verbatim as the unit
// `REL/REL_Setup.cpp` (Metaree 0x1F80..0x2124, Lumite 0x75E0..0x7784, SandBoss 0x11B30..0x11CD4).
// ScriptGui's copy was left as the `NonMatching` scaffold claim `ScriptGuiTail.cpp` with no
// source, so the five functions sat at 0%. The name has to stay module-unique rather than
// `REL/REL_Setup.cpp`: with the shared name the same ranges break this module's hash (the GOT
// grows 40 bytes and the `bl` in `_epilog`/`_prolog` gets a real displacement where retail holds
// a placeholder) - measured on ScriptCoin, and the reason `CScriptCoinTail.cpp` exists. So the
// bodies are the shared file's, the file is this module's, and the claim is this module's.
//
// **`ModuleDestructors` and `ModuleConstructors` are named in `symbols.txt` where retail left
// them `fn_60_9DD4`/`fn_60_9E20`, and `RELExit`/`RELMain` gain `scope:global`.** Both are load
// bearing for the hash: unnamed, the reference from `_epilog`/`_prolog` stays unresolved and the
// module grows 48 bytes of relocations. `tools/wire_rel_setup.py` makes the same two edits.
//
// **`"REL_Setup.cpp"` is spelled out rather than written `__FILE__`.** Retail passed `__FILE__`
// here, which MWCC expands to the path it was handed, and every module's copy reads the same
// 12 bytes - .rodata 0x1A0..0x1AC, immediately after the first format string, which is where
// `OSReport`'s `%s` argument has to sit. This file is not named `REL_Setup.cpp`, so `__FILE__`
// would emit a longer string and push every byte after it; the literal is retail's own text.
//
// The `.rodata` claim is what makes that possible: the five strings - the module report, the file
// name, the column header, the back-chain format and the trailing newline - are the module's last
// `.rodata` bytes (0x170..0x1F4 of 0x1F4) and this object emits all of them. Without the claim
// they would sit in dtk's `auto_*` object while `_unresolved` referenced them from here, which is
// the arrangement no other module uses.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the
// arrangement `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` use. The host link already has
// `ModuleConstructors`, `ModuleDestructors`, `_prolog`, `_epilog` and `_unresolved` from the one
// `REL/REL_Setup.cpp` object the shared lib compiles, and a flat link cannot hold them twice.
// `OSReport` and `OSGetStackPointer` are the DOL's, so a host body would add nothing but two
// undefined references.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/ScriptGuiTail.cpp`.

#include "types.h"

#include "REL/REL_Setup.h"
#include "dolphin/os.h"

#ifdef __MWERKS__
#define SCRIPTGUITAIL_EXTERN __declspec(section ".init") extern
#define SCRIPTGUITAIL_EXPORT __declspec(export)
#else
#define SCRIPTGUITAIL_EXTERN extern
#define SCRIPTGUITAIL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*VoidFunc)(void);

#ifdef __MWERKS__
// Retail: the module's static constructors live in the `.init` section and the linker synthesises
// the `_ctors`/`_dtors` tables, null-terminated. mwldeppc emits them.
SCRIPTGUITAIL_EXTERN VoidFunc _ctors[];
SCRIPTGUITAIL_EXTERN VoidFunc _dtors[];
#endif

// 0x9E20, 0x4C: the null-terminated `_ctors` walk. `mtctr r12` / `bctrl` is the indirect call, and
// the pointer walks the table forward before the test, so the load is in the loop's tail.
#ifdef __MWERKS__
void ModuleConstructors(void) {
  VoidFunc* func;
  for (func = _ctors; *func != nullptr; ++func) {
    (*func)();
  }
}

// 0x9DD4, 0x4C: the same walk over `_dtors`, reached only from `_epilog`.
void ModuleDestructors(void) {
  VoidFunc* func;
  for (func = _dtors; *func != nullptr; ++func) {
    (*func)();
  }
}

// 0x9DB0, 0x24: bring the module up - its static constructors, then its entry point.
SCRIPTGUITAIL_EXPORT void _prolog(void) {
  ModuleConstructors();
  RELMain();
}

// 0x9D8C, 0x24: and down, in that order. Both are one frame, two calls and the epilogue; retail
// makes no frame for either call.
SCRIPTGUITAIL_EXPORT void _epilog(void) {
  ModuleDestructors();
  RELExit();
}

// 0x9CC8, 0xC4: the report for a call into an unresolved import. Two header lines, then up to
// sixteen back-chain frames, then a blank line. `i` counts in `r30` and the chain pointer walks in
// `r29`; the loop tests the frame address, then the chain against 0xFFFF, then the count.
SCRIPTGUITAIL_EXPORT void _unresolved(void) {
  u32 i, s;

  OSReport("\nError: Unlinked function called in module %s.\n", "REL_Setup.cpp");
  OSReport("Address:      Back Chain    LR Save\n");

  for (i = 0, s = OSGetStackPointer(); s != 0 && s != 0xFFFFFFFF && i++ < 16;
       s = *(const u32*)(s)) {
    const u32* p = (const u32*)(s);
    OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
  }

  OSReport("\n");
}
#endif

#ifdef __cplusplus
}
#endif