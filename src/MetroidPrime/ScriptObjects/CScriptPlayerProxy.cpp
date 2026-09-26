#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

void SetLoader_PlayerController(FScriptLoader* loader);

extern "C" {
// A declaration under MWCC, a definition on the host; see CScriptRsfAudio.cpp. This unit's
// split claims only .text, so the module's .bss slot is dtk's to define, and a second
// definition here is the suspected cause of mwldeppc's internal linker error on this module.
#ifdef __MWERKS__
extern FScriptLoader lbl_62_bss_0;
#else
FScriptLoader lbl_62_bss_0 = 0;
#endif
CEntity* fn_62_188(CStateManager&, CInputStream&, const CEntityInfo&);

void fn_62_158() {
  lbl_62_bss_0 = fn_62_188;
  SetLoader_PlayerController(&lbl_62_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
void RELMain() { fn_62_158(); }

void RELExit() { SetLoader_PlayerController(nullptr); }
#else
void mp_relmain_playerproxy() { fn_62_158(); }

void mp_relexit_playerproxy() { SetLoader_PlayerController(nullptr); }
#endif
}
