#include "MetroidPrime/ScriptLoaderRel.hpp"

// Retail gives every loader pointer an 8-byte .sbss slot (symbols.txt sizes 0x8 at
// 0x80419480..0x804194F0), so each is a pointer plus a pad word, not a plain pointer.
template < typename T >
struct SLoaderSlot {
  T* value;
  uint padding;

  operator T*() const { return value; }
  SLoaderSlot& operator=(T* ptr) {
    value = ptr;
    return *this;
  }
};

SLoaderSlot< FScriptLoader > gLoader_IngSnatchingSwarm;
SLoaderSlot< SSnakeWeedSwarm_FuncPtrs > gLoader_SnakeWeed;
SLoaderSlot< SFishCloud_FuncPtrs > gLoader_FishCloud;
SLoaderSlot< FScriptLoader > gLoader_AtomicAlpha;
SLoaderSlot< FScriptLoader > gLoader_Ripper;
SLoaderSlot< FScriptLoader > gLoader_Puffer;
SLoaderSlot< FScriptLoader > gLoader_Metaree;
SLoaderSlot< SPlayerActor_FuncPtrs > gLoader_PlayerActor;
SLoaderSlot< SPlayerTurret_FuncPtrs > gLoader_PlayerTurret;
SLoaderSlot< FScriptLoader > gLoader_RiftPortal;
SLoaderSlot< SSafeZone_FuncPtrs > gLoader_SafeZone;
SLoaderSlot< GUILoaders > gLoader_GUI;
SLoaderSlot< FScriptLoader > gLoader_PlayerController;
SLoaderSlot< FScriptLoader > gLoader_WallWalker;

void SetLoader_WallWalker(FScriptLoader* loader) { gLoader_WallWalker = loader; }
CEntity* Load_WallWalker(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_WallWalker.value)(mgr, input, info);
}
void SetLoader_PlayerController(FScriptLoader* loader) { gLoader_PlayerController = loader; }
CEntity* LoadPlayerController(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_PlayerController.value)(mgr, input, info);
}
void ScriptGUI_SetPtrs(GUILoaders* loaders) { gLoader_GUI = loaders; }

CEntity* LoadGuiWidget(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GUI.value->guiWidget(mgr, input, info);
}
CEntity* LoadGuiScreen(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GUI.value->guiScreen(mgr, input, info);
}
CEntity* LoadGuiSlider(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GUI.value->guiSlider(mgr, input, info);
}
CEntity* LoadGuiMenu(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_GUI.value->guiMenu(mgr, input, info);
}
CEntity* LoadGuiPlayerJoinManager(CStateManager& mgr, CInputStream& input,
                                  const CEntityInfo& info) {
  return gLoader_GUI.value->guiPlayerJoinManager(mgr, input, info);
}

void SetSSafeZone_FuncPtrs(SSafeZone_FuncPtrs* loader) { gLoader_SafeZone = loader; }
CEntity* LoadSafeZone(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_SafeZone.value->safeZone(mgr, input, info);
}
void SafeZone_ActOn(CEntity& entity, CStateManager& mgr) {
  (entity.*(gLoader_SafeZone.value->method))(mgr);
}

CEntity* LoadSafeZoneCrystal(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_SafeZone.value->safeZoneCrystal(mgr, input, info);
}
void SetLoader_RiftPortal(FScriptLoader* loader) { gLoader_RiftPortal = loader; }
CEntity* LoadRiftPortal(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_RiftPortal.value)(mgr, input, info);
}

void SetLoader_PlayerTurret(SPlayerTurret_FuncPtrs* loader) { gLoader_PlayerTurret = loader; }
CEntity* LoadPlayerTurret(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_PlayerTurret.value->loader(mgr, input, info);
}
CTransform4f PlayerTurret_GetTransform1(CEntity& entity, CStateManager& mgr) {
  return (entity.*(gLoader_PlayerTurret.value->GetTransform1))(mgr);
}

CTransform4f PlayerTurret_GetTransform2(CEntity& entity, CStateManager& mgr) {
  return (entity.*(gLoader_PlayerTurret.value->GetTransform2))(mgr);
}

void PlayerTurret_SendSomeMsg(CEntity& entity, CStateManager& mgr) {
  return (entity.*(gLoader_PlayerTurret.value->SendSomeMsg))(mgr);
}

void PlayerTurret_CheckInput(CEntity& entity, float dt, CFinalInput& input, CStateManager& mgr) {
  return (entity.*(gLoader_PlayerTurret.value->CheckInput))(dt, input, mgr);
}

TUniqueId PlayerTurret_GetSomeId(CEntity& entity) {
  return (entity.*(gLoader_PlayerTurret.value->GetSomeId))();
}

void SetLoader_PlayerActor(SPlayerActor_FuncPtrs* loader) { gLoader_PlayerActor = loader; }
CEntity* LoadPlayerActor(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_PlayerActor.value->loader(mgr, input, info);
}
void TouchPlayerActor(CEntity& ent, CStateManager& mgr) {
  return (ent.*(gLoader_PlayerActor.value->method))(mgr);
}

void SetLoader_Metaree(FScriptLoader* loader) { gLoader_Metaree = loader; }
CEntity* LoadMetaree(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Metaree.value)(mgr, input, info);
}
void SetLoader_Puffer(FScriptLoader* loader) { gLoader_Puffer = loader; }
CEntity* LoadPuffer(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Puffer.value)(mgr, input, info);
}
void SetLoader_Ripper(FScriptLoader* loader) { gLoader_Ripper = loader; }
CEntity* LoadRipper(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Ripper.value)(mgr, input, info);
}
void SetLoader_AtomicAlpha(FScriptLoader* loader) { gLoader_AtomicAlpha = loader; }
CEntity* LoadAtomicAlpha(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_AtomicAlpha.value)(mgr, input, info);
}

void SetLoader_FishCloud(SFishCloud_FuncPtrs* loader) { gLoader_FishCloud = loader; }
CEntity* LoadFishCloud(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_FishCloud.value->fishCloud(mgr, input, info);
}
CEntity* LoadFishCloudModifier(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_FishCloud.value->fishCloudModifier(mgr, input, info);
}

void SetLoader_SnakeWeedSwarm(SSnakeWeedSwarm_FuncPtrs* loader) { gLoader_SnakeWeed = loader; }
CEntity* LoadSnakeWeedSwarm(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_SnakeWeed.value->swarm(mgr, input, info);
}

void SnakeWeedAlt_8021BA94(CEntity& ent, const CVector3f& v, const CDamageInfo& dmgInfo, CStateManager& mgr) {
  return (ent.*(gLoader_SnakeWeed.value->method))(v, dmgInfo, mgr);
}

void SetLoader_IngSnatchingSwarm(FScriptLoader* loader) { gLoader_IngSnatchingSwarm = loader; }
CEntity* LoadIngSnatchingSwarm(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_IngSnatchingSwarm.value)(mgr, input, info);
}

