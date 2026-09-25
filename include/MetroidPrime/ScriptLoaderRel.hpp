#ifndef _SCRIPTLOADERREL
#define _SCRIPTLOADERREL

#include "MetroidPrime/ScriptLoader.hpp"

#include "Kyoto/Math/CTransform4f.hpp"
#include "MetroidPrime/TGameTypes.hpp"

class CVector3f;
class CDamageInfo;
class CFinalInput;

struct SGuiWidget_FuncPtrs {
  FScriptLoader guiWidget;
  FScriptLoader guiScreen;
  FScriptLoader guiSlider;
  FScriptLoader guiMenu;
  FScriptLoader guiPlayerJoinManager;
};
struct GUILoaders;
extern "C" void ScriptGUI_SetPtrs__FP10GUILoaders(GUILoaders*);

struct SSafeZone_FuncPtrs {
  FScriptLoader safeZone;
  FScriptLoader safeZoneCrystal;
  void (CEntity::*method)(CStateManager& mgr);
};
struct SafeCrystalLoaders;
void SetLoader_SafeZone(SafeCrystalLoaders*);

struct SFishCloud_FuncPtrs {
  FScriptLoader fishCloud;
  FScriptLoader fishCloudModifier;
};
struct FishCloudLoaders;
void SetLoader_FishCloud(FishCloudLoaders*);

struct SSnakeWeedSwarm_FuncPtrs {
  FScriptLoader swarm;
  void (CEntity::*method)(CVector3f, const CDamageInfo&, CStateManager&);
};
struct SnakeWeedLoaders;
void SetLoader_SnakeWeedSwarm(SnakeWeedLoaders*);

struct SPlayerActor_FuncPtrs {
  FScriptLoader loader;
  void (CEntity::*method)(CStateManager& mgr);
};
struct PlayerActorFunctions;
void SetLoader_PlayerActor(PlayerActorFunctions*);

struct SPlayerTurret_FuncPtrs {
  FScriptLoader loader;
  CTransform4f (CEntity::*GetTransform1)(CStateManager&);
  CTransform4f (CEntity::*GetTransform2)(CStateManager&);
  void (CEntity::*SendSomeMsg)(CStateManager&);
  void (CEntity::*CheckInput)(float, CFinalInput&, CStateManager&);
  TUniqueId (CEntity::*GetSomeId)();
};
struct PlayerTurretFunctions;
void SetLoader_PlayerTurret(PlayerTurretFunctions*);

struct SScriptForgottenObject_FuncPtrs {
  FScriptLoader loader;
};
void SetSScriptForgottenObject_FuncPtrs(SScriptForgottenObject_FuncPtrs*);

void SetLoader_CannonBall(FScriptLoader* loader);

struct STweaks_FuncPtrs {
  void (*Loader)(CInputStream&);
  void (*CreateGlobals)();
  void (*FreeTweaks)();
};
void SetTweaks_FuncPtrs(STweaks_FuncPtrs*);

#endif // _SCRIPTLOADERREL
