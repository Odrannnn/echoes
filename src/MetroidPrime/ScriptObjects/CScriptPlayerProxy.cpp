#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

void SetLoader_PlayerController(FScriptLoader* loader);

extern "C" {
extern FScriptLoader lbl_62_bss_0;
CEntity* fn_62_188(CStateManager&, CInputStream&, const CEntityInfo&);

void fn_62_158() {
  lbl_62_bss_0 = fn_62_188;
  SetLoader_PlayerController(&lbl_62_bss_0);
}

void RELMain() { fn_62_158(); }

void RELExit() { SetLoader_PlayerController(nullptr); }
}
