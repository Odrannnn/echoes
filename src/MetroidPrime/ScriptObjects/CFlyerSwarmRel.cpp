#include "MetroidPrime/ScriptLoader.hpp"

extern "C" CEntity* REL_LoadFlyerSwarm(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" void fn_80229FBC(FScriptLoader* loader);

extern "C" {
FScriptLoader REL_loader_FlyerSwarm = 0;
}

static void SetRelLoaderFunctionToLoader() {
  REL_loader_FlyerSwarm = REL_LoadFlyerSwarm;
  fn_80229FBC(&REL_loader_FlyerSwarm);
}

extern "C" void RELMain() { SetRelLoaderFunctionToLoader(); }
extern "C" void RELExit() { fn_80229FBC(0); }
