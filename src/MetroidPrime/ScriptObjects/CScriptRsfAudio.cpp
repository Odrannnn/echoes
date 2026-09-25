#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
extern FScriptLoader lbl_65_bss_0;
CEntity* fn_65_FC(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80227B2C(FScriptLoader*);

void fn_65_CC() {
  lbl_65_bss_0 = fn_65_FC;
  fn_80227B2C(&lbl_65_bss_0);
}

void RELMain() { fn_65_CC(); }

void RELExit() { fn_80227B2C(nullptr); }
}
