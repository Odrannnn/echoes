#include "MetroidPrime/ScriptLoader.hpp"

extern "C" CEntity* REL_LoadPuffer(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" {
// The initialiser is host-only. Under `-common on` the retail form is a common symbol, and
// this unit's split claims the module's .bss slot for it; turning it into an initialised
// definition under MWCC is a change to the matching object, so MWCC keeps the retail form.
// On the host the `= 0` keeps the loader an ordinary writable object (see
// REL_loader_CannonBall in CScriptCannonBall.cpp).
#ifdef __MWERKS__
FScriptLoader lbl_52_bss_0;
#else
FScriptLoader lbl_52_bss_0 = 0;
#endif
}

void SetLoader_Puffer(FScriptLoader* loader);

extern "C" void RegisterPufferLoader() {
  lbl_52_bss_0 = REL_LoadPuffer;
  SetLoader_Puffer(&lbl_52_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELMain() { RegisterPufferLoader(); }

extern "C" void RELExit() { SetLoader_Puffer(0); }
#else
extern "C" void mp_relmain_puffer() { RegisterPufferLoader(); }

extern "C" void mp_relexit_puffer() { SetLoader_Puffer(0); }
#endif
