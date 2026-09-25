#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

void SetLoader_RiftPortal(FScriptLoader* loader);

extern CEntity* REL_LoadRiftPortal(CStateManager& mgr, CInputStream& input,
                                   const CEntityInfo& info);

FScriptLoader gRiftPortalLoader;

void SetFuncPtrs() {
  gRiftPortalLoader = &REL_LoadRiftPortal;
  SetLoader_RiftPortal(&gRiftPortalLoader);
}

void RELMain() { SetFuncPtrs(); }

void RELExit() { SetLoader_RiftPortal(nullptr); }
