#include "MetroidPrime/ScriptLoaderRel.hpp"

class CInputStream;
class CStateManager;
class CEntityInfo;

extern "C" CEntity* fn_60_6FF0(CStateManager&, CInputStream&, CEntityInfo&);
extern "C" CEntity* fn_60_A30(CStateManager&, CInputStream&, CEntityInfo&);
extern "C" CEntity* fn_60_8E90(CStateManager&, CInputStream&, CEntityInfo&);
extern "C" CEntity* fn_60_7D20(CStateManager&, CInputStream&, CEntityInfo&);
extern "C" CEntity* fn_60_B8(CStateManager&, CInputStream&, CEntityInfo&);

extern "C" {
GUILoaders gGUILoaders;
}

// Every REL module defines RELMain/RELExit, and this one and CScriptRiftPortal.cpp both
// define SetFuncPtrs; a flat host link cannot hold any of them twice, so on the host they
// take distinct names and platform/compiled_modules.cpp calls the entry points. The MWCC
// branch is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
void SetFuncPtrs() {
  gGUILoaders.guiWidget = &fn_60_6FF0;
  gGUILoaders.guiScreen = &fn_60_A30;
  gGUILoaders.guiSlider = &fn_60_8E90;
  gGUILoaders.guiMenu = &fn_60_7D20;
  gGUILoaders.guiPlayerJoinManager = &fn_60_B8;
  ScriptGUI_SetPtrs(&gGUILoaders);
}

extern "C" void RELMain() { SetFuncPtrs(); }

extern "C" void RELExit() { ScriptGUI_SetPtrs(nullptr); }
#else
static void mp_setfuncptrs_scriptguisetup() {
  gGUILoaders.guiWidget = &fn_60_6FF0;
  gGUILoaders.guiScreen = &fn_60_A30;
  gGUILoaders.guiSlider = &fn_60_8E90;
  gGUILoaders.guiMenu = &fn_60_7D20;
  gGUILoaders.guiPlayerJoinManager = &fn_60_B8;
  ScriptGUI_SetPtrs(&gGUILoaders);
}

extern "C" void mp_relmain_scriptguisetup() { mp_setfuncptrs_scriptguisetup(); }

extern "C" void mp_relexit_scriptguisetup() { ScriptGUI_SetPtrs(nullptr); }
#endif
