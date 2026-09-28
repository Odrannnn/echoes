#ifndef _SCRIPTLOADERREL
#define _SCRIPTLOADERREL

#include "MetroidPrime/ScriptLoader.hpp"

#include "Kyoto/Math/CTransform4f.hpp"
#include "MetroidPrime/TGameTypes.hpp"

class CVector3f;
class CDamageInfo;
class CFinalInput;

// Retail's setter is ScriptGUI_SetPtrs__FP10GUILoaders, so the struct it takes has to be spelled
// GUILoaders: a typedef of a differently named class would mangle as the underlying class.
// Upstream's ScriptLoaderRel.cpp calls the same setter SetSGuiWidget_FuncPtrs.
struct GUILoaders {
  FScriptLoader guiWidget;
  FScriptLoader guiScreen;
  FScriptLoader guiSlider;
  FScriptLoader guiMenu;
  FScriptLoader guiPlayerJoinManager;
};
void ScriptGUI_SetPtrs(GUILoaders*);

struct SSafeZone_FuncPtrs {
  FScriptLoader safeZone;
  FScriptLoader safeZoneCrystal;
  void (CEntity::*method)(CStateManager& mgr);
};
void SetSSafeZone_FuncPtrs(SSafeZone_FuncPtrs*);

struct SFishCloud_FuncPtrs {
  FScriptLoader fishCloud;
  FScriptLoader fishCloudModifier;
};
void SetSFishCloud_FuncPtrs(SFishCloud_FuncPtrs*);

struct SSnakeWeedSwarm_FuncPtrs {
  FScriptLoader swarm;
  // Retail copies the three words of the position into a caller-side temporary and passes
  // its address, so the pmf takes the vector by value, not by reference.
  void (CEntity::*method)(CVector3f, const CDamageInfo&, CStateManager&);
};
void SetSSnakeWeedSwarm_FuncPtrs(SSnakeWeedSwarm_FuncPtrs*);

struct SPlayerActor_FuncPtrs {
  FScriptLoader loader;
  void (CEntity::*method)(CStateManager& mgr);
};
void SetSPlayerActor_FuncPtrs(SPlayerActor_FuncPtrs*);

struct SPlayerTurret_FuncPtrs {
  FScriptLoader loader;
  CTransform4f (CEntity::*GetTransform1)(CStateManager&);
  CTransform4f (CEntity::*GetTransform2)(CStateManager&);
  void (CEntity::*SendSomeMsg)(CStateManager&);
  void (CEntity::*CheckInput)(float, CFinalInput&, CStateManager&);
  TUniqueId (CEntity::*GetSomeId)();
};
void SetSPlayerTurret_FuncPtrs(SPlayerTurret_FuncPtrs*);

struct SScriptForgottenObject_FuncPtrs {
  FScriptLoader loader;
};
void SetSScriptForgottenObject_FuncPtrs(SScriptForgottenObject_FuncPtrs*);

// Defined in src/MetroidPrime/ScriptLoader/CannonBall{,LoaderSet}.cpp because retail keeps each in
// a unit of its own. `gLoader_CannonBall` is not declared here: it is an 8-byte slot (retail
// .sbss 0x80419538..0x80419540), a file-local struct in both units, not a plain pointer.
void SetLoader_CannonBall(FScriptLoader* loader);
CEntity* LoadCannonBall(CStateManager& mgr, CInputStream& input, const CEntityInfo& info);

struct STweaks_FuncPtrs {
  void (*Loader)(CInputStream&);
  void (*CreateGlobals)();
  void (*FreeTweaks)();
};
void SetTweaks_FuncPtrs(STweaks_FuncPtrs*);

#endif // _SCRIPTLOADERREL
