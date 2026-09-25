#include "MetroidPrime/ScriptLoader.hpp"

class CInputStream;
class CStateManager;
class CEntityInfo;

struct GUILoaders {
  FScriptLoader guiWidget;
  FScriptLoader guiScreen;
  FScriptLoader guiSlider;
  FScriptLoader guiMenu;
  FScriptLoader guiPlayerJoinManager;
};

extern "C" void ScriptGUI_SetPtrs__FP10GUILoaders(GUILoaders* loaders);

extern "C" CEntity* fn_60_6FF0(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" CEntity* fn_60_A30(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" CEntity* fn_60_8E90(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" CEntity* fn_60_7D20(CStateManager&, CInputStream&, const CEntityInfo&);
extern "C" CEntity* fn_60_B8(CStateManager&, CInputStream&, const CEntityInfo&);

extern "C" {
GUILoaders gGUILoaders;
}

void SetFuncPtrs() {
  gGUILoaders.guiWidget = &fn_60_6FF0;
  gGUILoaders.guiScreen = &fn_60_A30;
  gGUILoaders.guiSlider = &fn_60_8E90;
  gGUILoaders.guiMenu = &fn_60_7D20;
  gGUILoaders.guiPlayerJoinManager = &fn_60_B8;
  ScriptGUI_SetPtrs__FP10GUILoaders(&gGUILoaders);
}

extern "C" void RELMain() { SetFuncPtrs(); }

extern "C" void RELExit() { ScriptGUI_SetPtrs__FP10GUILoaders(0); }
