#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

void SetLoader_RiftPortal(FScriptLoader* loader);

extern CEntity* REL_LoadRiftPortal(CStateManager& mgr, CInputStream& input,
                                   const CEntityInfo& info);

// Host-only initialiser; see CScriptPufferRel.cpp. MWCC keeps the retail common symbol.
#ifdef __MWERKS__
FScriptLoader gRiftPortalLoader;
#else
FScriptLoader gRiftPortalLoader = 0;
#endif

// Every REL module defines RELMain/RELExit, and this one and ScriptGuiSetup.cpp both define
// SetFuncPtrs; a flat host link cannot hold any of them twice, so on the host they take
// distinct names and platform/compiled_modules.cpp calls the entry points. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
void SetFuncPtrs() {
  gRiftPortalLoader = &REL_LoadRiftPortal;
  SetLoader_RiftPortal(&gRiftPortalLoader);
}

void RELMain() { SetFuncPtrs(); }

void RELExit() { SetLoader_RiftPortal(nullptr); }
#else
static void mp_setfuncptrs_riftportal() {
  gRiftPortalLoader = &REL_LoadRiftPortal;
  SetLoader_RiftPortal(&gRiftPortalLoader);
}

extern "C" void mp_relmain_riftportal() { mp_setfuncptrs_riftportal(); }

extern "C" void mp_relexit_riftportal() { SetLoader_RiftPortal(nullptr); }
#endif
