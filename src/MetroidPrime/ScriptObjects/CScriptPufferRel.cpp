#include "MetroidPrime/ScriptLoader.hpp"

extern "C" CEntity* REL_LoadPuffer(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" {
FScriptLoader lbl_52_bss_0;
}

void SetLoader_Puffer(FScriptLoader* loader);

extern "C" void RegisterPufferLoader() {
  lbl_52_bss_0 = REL_LoadPuffer;
  SetLoader_Puffer(&lbl_52_bss_0);
}

extern "C" void RELMain() { RegisterPufferLoader(); }

extern "C" void RELExit() { SetLoader_Puffer(0); }
