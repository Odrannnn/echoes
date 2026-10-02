#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
// A declaration under MWCC, a definition on the host - and it must stay that way round.
//
// This unit is Object(Matching) in configure.py, so mwcceppc's output for it has to
// reproduce retail's bytes exactly or the module hash breaks. A definition under MWCC
// would add a second definition of lbl_65_bss_0 to the module link: an attempt that made
// exactly that change (a plain `FScriptLoader lbl_65_bss_0 = nullptr;`) got
// "internal linker error: File: 'ELF_linker.c' Line: 5083" from mwldeppc on this module
// and on ScriptPlayerProxy - and those two are the only edited units that were Matching
// AND declared their loader `extern` at HEAD. A third thing was tried and is NOT the
// cause: their splits.txt claim a .bss range exactly as the working ScriptCannonBall's
// does, so the split is not what distinguishes them.
//
// On the host there is no dtk split object at all, so this unit has to supply the
// variable itself, and a host definition is also what keeps it out of `.text`: with no
// initialiser and no other TU defining it, the linker binds the symbol as a FUNC and
// places it in read-only .text, and the store in fn_65_CC then segfaults. That is the
// same bug as REL_loader_CannonBall, which is NonMatching and so could be fixed freely.
#ifdef __MWERKS__
extern FScriptLoader lbl_65_bss_0;
#else
FScriptLoader lbl_65_bss_0 = 0;
#endif
CEntity* fn_65_FC(CStateManager&, CInputStream&, CEntityInfo&);
void fn_80227B2C(FScriptLoader*);

void fn_65_CC() {
  lbl_65_bss_0 = fn_65_FC;
  fn_80227B2C(&lbl_65_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
void RELMain() { fn_65_CC(); }

void RELExit() { fn_80227B2C(nullptr); }
#else
void mp_relmain_rsfaudio() { fn_65_CC(); }

void mp_relexit_rsfaudio() { fn_80227B2C(nullptr); }
#endif
}
