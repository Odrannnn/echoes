#include "MetroidPrime/ScriptLoader.hpp"

extern "C" CEntity* REL_LoadFlyerSwarm(CStateManager&, CInputStream&, CEntityInfo&);
extern "C" void fn_80229FBC(FScriptLoader* loader);

extern "C" {
FScriptLoader REL_loader_FlyerSwarm = 0;
}

static void SetRelLoaderFunctionToLoader() {
  REL_loader_FlyerSwarm = REL_LoadFlyerSwarm;
  fn_80229FBC(&REL_loader_FlyerSwarm);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELMain() { SetRelLoaderFunctionToLoader(); }
extern "C" void RELExit() { fn_80229FBC(0); }
#else
extern "C" void mp_relmain_swarm() { SetRelLoaderFunctionToLoader(); }
extern "C" void mp_relexit_swarm() { fn_80229FBC(0); }
#endif
