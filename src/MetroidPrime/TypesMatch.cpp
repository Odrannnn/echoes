#include "MetroidPrime/CEntity.hpp"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPickup.hpp"
#include "MetroidPrime/ScriptObjects/CScriptActor.hpp"
#include "MetroidPrime/ScriptObjects/CScriptEffect.hpp"
#include "MetroidPrime/ScriptObjects/CScriptForgottenObject.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSequenceTimer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpawnPoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptStreamedMusic.hpp"

// Three names here are inferred rather than read from a symbol table or another
// version's config: id 93 CScriptTriggerEllipsoid (sits between the trigger
// classes), id 104 CWallCrawler (its only vtable reference is WallCrawler.rel) and
// id 109 CBeamProjectile (Prime 1 has the same GameProjectile -> BeamProjectile ->
// PlasmaProjectile chain). Everything else was read from the Trilogy configs in
// config/R3ME01, R3MP01 and R32J01, or from the EEntityType enum.
//
// Classes without headers yet, declared just far enough to define their overrides. Each parent is
// the class whose override the retail function calls.
#define TYPES_MATCH_CLASS(cls, parent)                                                           \
  class cls : public parent {                                                                    \
  public:                                                                                        \
    ~cls();                                                                                      \
    CEntity* TypesMatch(int typeId) const;                                                       \
  };

TYPES_MATCH_CLASS(CAi, CPhysicsActor)
TYPES_MATCH_CLASS(CPatterned, CAi)
TYPES_MATCH_CLASS(CGameCamera, CActor)
TYPES_MATCH_CLASS(CWeapon, CActor)
TYPES_MATCH_CLASS(CEffect, CActor)
TYPES_MATCH_CLASS(CGameProjectile, CWeapon)
TYPES_MATCH_CLASS(CScriptWaypoint, CActor)
TYPES_MATCH_CLASS(CScriptGuiWidget, CEntity)
TYPES_MATCH_CLASS(CBomb, CWeapon)
TYPES_MATCH_CLASS(CBouncingBomb, CWeapon)
TYPES_MATCH_CLASS(CBouncyGrenade, CPhysicsActor)
TYPES_MATCH_CLASS(CCinematicCamera, CGameCamera)
TYPES_MATCH_CLASS(CCollisionActor, CPhysicsActor)
TYPES_MATCH_CLASS(CEnergyProjectile, CGameProjectile)
TYPES_MATCH_CLASS(CScattershotProjectile, CWeapon)
TYPES_MATCH_CLASS(CExplosion, CEffect)
TYPES_MATCH_CLASS(CFishCloud, CActor)
TYPES_MATCH_CLASS(CGameLight, CActor)
TYPES_MATCH_CLASS(CHUDBillboardEffect, CEffect)
TYPES_MATCH_CLASS(CIngPuddle, CPhysicsActor)
TYPES_MATCH_CLASS(CIngSnatchingSwarm, CActor)
TYPES_MATCH_CLASS(CScriptActorKeyframe, CEntity)
TYPES_MATCH_CLASS(CScriptAIHint, CActor)
TYPES_MATCH_CLASS(CScriptAiJumpPoint, CActor)
TYPES_MATCH_CLASS(CScriptAIWaypoint, CScriptWaypoint)
TYPES_MATCH_CLASS(CScriptCameraShaker, CEntity)
TYPES_MATCH_CLASS(CScriptCamera, CActor)
TYPES_MATCH_CLASS(CScriptColorModulate, CEntity)
TYPES_MATCH_CLASS(CScriptCounter, CEntity)
TYPES_MATCH_CLASS(CScriptCoverPoint, CActor)
TYPES_MATCH_CLASS(CScriptDamageableTrigger, CActor)
TYPES_MATCH_CLASS(CScriptDarkSamusBattleStage, CEntity)
TYPES_MATCH_CLASS(CScriptDestructibleBarrier, CPhysicsActor)
TYPES_MATCH_CLASS(CScriptDock, CPhysicsActor)
TYPES_MATCH_CLASS(CScriptDoor, CPhysicsActor)
TYPES_MATCH_CLASS(CScriptDynamicLight, CGameLight)
TYPES_MATCH_CLASS(CScriptGrapplePoint, CActor)
TYPES_MATCH_CLASS(CScriptGuiMenu, CScriptGuiWidget)
TYPES_MATCH_CLASS(CScriptGuiScreen, CActor)
TYPES_MATCH_CLASS(CScriptGuiSlider, CScriptGuiWidget)
TYPES_MATCH_CLASS(CScriptLayerController, CEntity)
TYPES_MATCH_CLASS(CScriptPlayerProxy, CActor)
TYPES_MATCH_CLASS(CScriptPlatform, CPhysicsActor)
TYPES_MATCH_CLASS(CScriptPortalTransition, CEntity)
TYPES_MATCH_CLASS(CScriptRelay, CEntity)
TYPES_MATCH_CLASS(CScriptRepulsor, CActor)
TYPES_MATCH_CLASS(CScriptRiftPortal, CActor)
TYPES_MATCH_CLASS(CScriptSound, CActor)
TYPES_MATCH_CLASS(CScriptSpecialFunction, CActor)
TYPES_MATCH_CLASS(CScriptSwitch, CEntity)
TYPES_MATCH_CLASS(CScriptTargetingPoint, CActor)
TYPES_MATCH_CLASS(CScriptTeamAiMgr, CEntity)
TYPES_MATCH_CLASS(CScriptTextPane, CActor)
TYPES_MATCH_CLASS(CScriptTrigger, CActor)
TYPES_MATCH_CLASS(CScriptTriggerEllipsoid, CScriptTrigger)
TYPES_MATCH_CLASS(CScriptTriggerOrientated, CScriptTrigger)
TYPES_MATCH_CLASS(CScriptSafeZone, CScriptTriggerEllipsoid)
TYPES_MATCH_CLASS(CScriptWater, CScriptTrigger)
TYPES_MATCH_CLASS(CScriptWorldTeleporter, CEntity)
TYPES_MATCH_CLASS(CSnakeWeedSwarm, CActor)
TYPES_MATCH_CLASS(CSwarmBasics, CActor)
TYPES_MATCH_CLASS(CFlyerSwarm, CSwarmBasics)
TYPES_MATCH_CLASS(CWallCrawler, CPatterned)
TYPES_MATCH_CLASS(CBacteriaSwarm, CActor)
TYPES_MATCH_CLASS(CMetareeSwarm, CSwarmBasics)
TYPES_MATCH_CLASS(CIngBlobSwarm, CSwarmBasics)
TYPES_MATCH_CLASS(CPlantScarabSwarm, CSwarmBasics)
TYPES_MATCH_CLASS(CBeamProjectile, CGameProjectile)
TYPES_MATCH_CLASS(CPlasmaProjectile, CBeamProjectile)
TYPES_MATCH_CLASS(CDarkSamus, CPatterned)
TYPES_MATCH_CLASS(CDigitalGuardian, CPatterned)
TYPES_MATCH_CLASS(CDigitalGuardianHead, CPatterned)
TYPES_MATCH_CLASS(CElitePirate, CPatterned)
TYPES_MATCH_CLASS(CGrenchler, CPatterned)
TYPES_MATCH_CLASS(CIng, CPatterned)
TYPES_MATCH_CLASS(CIngBoostBallGuardian, CPatterned)
TYPES_MATCH_CLASS(CIngSpaceJumpGuardian, CPatterned)
TYPES_MATCH_CLASS(CIngSpiderballGuardian, CPatterned)
TYPES_MATCH_CLASS(CLumite, CPatterned)
TYPES_MATCH_CLASS(CMetaree, CPatterned)
TYPES_MATCH_CLASS(CMetroid, CPatterned)
TYPES_MATCH_CLASS(CBabyMetroid, CMetroid)
TYPES_MATCH_CLASS(CParasite, CWallCrawler)
TYPES_MATCH_CLASS(CPillBug, CWallCrawler)
TYPES_MATCH_CLASS(CPuffer, CPatterned)
TYPES_MATCH_CLASS(CRezbit, CPatterned)
TYPES_MATCH_CLASS(CRipper, CPatterned)
TYPES_MATCH_CLASS(CSandBoss, CPatterned)
TYPES_MATCH_CLASS(CSandworm, CPatterned)
TYPES_MATCH_CLASS(CSandwormEye, CActor)
TYPES_MATCH_CLASS(CSpacePirate, CPatterned)
TYPES_MATCH_CLASS(CSpankWeed, CPatterned)
TYPES_MATCH_CLASS(CSplitterMainChassis, CPatterned)
TYPES_MATCH_CLASS(CSplitterCommandModule, CPatterned)
TYPES_MATCH_CLASS(CWispTentacle, CPatterned)
TYPES_MATCH_CLASS(CScriptPlayerTurret, CActor)
TYPES_MATCH_CLASS(CGunTurretBase, CPatterned)
TYPES_MATCH_CLASS(CGunTurretTop, CPatterned)
TYPES_MATCH_CLASS(CKralee, CWallCrawler)
TYPES_MATCH_CLASS(CGlowbug, CPatterned)
TYPES_MATCH_CLASS(CSporbBase, CPatterned)
TYPES_MATCH_CLASS(CSporbNeedle, CPhysicsActor)
TYPES_MATCH_CLASS(CSporbTop, CPatterned)
TYPES_MATCH_CLASS(CSporbProjectile, CPatterned)
TYPES_MATCH_CLASS(CMinorIng, CPatterned)
TYPES_MATCH_CLASS(CBoostBallGuardian, CPhysicsActor)
TYPES_MATCH_CLASS(CBlogg, CPatterned)
TYPES_MATCH_CLASS(CWallWalker, CWallCrawler)
TYPES_MATCH_CLASS(CShredder, CPatterned)
TYPES_MATCH_CLASS(CAIMannedTurret, CAi)
TYPES_MATCH_CLASS(CStoneToad, CPatterned)
TYPES_MATCH_CLASS(CScriptFrontEndDataNetwork, CActor)
TYPES_MATCH_CLASS(CPowerBomb, CWeapon)
TYPES_MATCH_CLASS(CKrocuss, CPatterned)
TYPES_MATCH_CLASS(COctapedeSegment, CWallCrawler)
TYPES_MATCH_CLASS(CPuddleSpore, CPatterned)

#undef TYPES_MATCH_CLASS

#define TYPES_MATCH_IMPL(cls, parent, id)                                                        \
  CEntity* cls::TypesMatch(int typeId) const {                                                   \
    if (typeId == id) {                                                                          \
      return const_cast< cls* >(this);                                                           \
    }                                                                                            \
    if (typeId > id) {                                                                           \
      return nullptr;                                                                            \
    }                                                                                            \
    return parent::TypesMatch(typeId);                                                           \
  }

TYPES_MATCH_IMPL(CScriptForgottenObject, CEntity, kET_ScriptForgottenObject)
TYPES_MATCH_IMPL(CPuddleSpore, CPatterned, kET_PuddleSpore)
TYPES_MATCH_IMPL(COctapedeSegment, CWallCrawler, kET_OctapedeSegment)
TYPES_MATCH_IMPL(CKrocuss, CPatterned, kET_Krocuss)
TYPES_MATCH_IMPL(CPowerBomb, CWeapon, kET_PowerBomb)
TYPES_MATCH_IMPL(CScriptFrontEndDataNetwork, CActor, kET_ScriptFrontEndDataNetwork)
TYPES_MATCH_IMPL(CStoneToad, CPatterned, kET_StoneToad)
TYPES_MATCH_IMPL(CAIMannedTurret, CAi, kET_AIMannedTurret)
TYPES_MATCH_IMPL(CShredder, CPatterned, kET_Shredder)
TYPES_MATCH_IMPL(CWallWalker, CWallCrawler, kET_WallWalker)
TYPES_MATCH_IMPL(CBlogg, CPatterned, kET_Blogg)
TYPES_MATCH_IMPL(CBoostBallGuardian, CPhysicsActor, kET_BoostBallGuardian)
TYPES_MATCH_IMPL(CMinorIng, CPatterned, kET_MinorIng)
TYPES_MATCH_IMPL(CSporbProjectile, CPatterned, kET_SporbProjectile)
TYPES_MATCH_IMPL(CSporbTop, CPatterned, kET_SporbTop)
TYPES_MATCH_IMPL(CSporbNeedle, CPhysicsActor, kET_SporbNeedle)
TYPES_MATCH_IMPL(CSporbBase, CPatterned, kET_SporbBase)
TYPES_MATCH_IMPL(CGlowbug, CPatterned, kET_Glowbug)
TYPES_MATCH_IMPL(CKralee, CWallCrawler, kET_Kralee)
TYPES_MATCH_IMPL(CGunTurretTop, CPatterned, kET_GunTurretTop)
TYPES_MATCH_IMPL(CGunTurretBase, CPatterned, kET_GunTurretBase)
TYPES_MATCH_IMPL(CScriptPlayerTurret, CActor, kET_ScriptPlayerTurret)
TYPES_MATCH_IMPL(CWispTentacle, CPatterned, kET_WispTentacle)
TYPES_MATCH_IMPL(CSplitterCommandModule, CPatterned, kET_SplitterCommandModule)
TYPES_MATCH_IMPL(CSplitterMainChassis, CPatterned, kET_SplitterMainChassis)
TYPES_MATCH_IMPL(CSpankWeed, CPatterned, kET_SpankWeed)
TYPES_MATCH_IMPL(CSpacePirate, CPatterned, kET_SpacePirate)
TYPES_MATCH_IMPL(CSandwormEye, CActor, kET_SandwormEye)
TYPES_MATCH_IMPL(CSandworm, CPatterned, kET_Sandworm)
TYPES_MATCH_IMPL(CSandBoss, CPatterned, kET_SandBoss)
TYPES_MATCH_IMPL(CRipper, CPatterned, kET_Ripper)
TYPES_MATCH_IMPL(CRezbit, CPatterned, kET_Rezbit)
TYPES_MATCH_IMPL(CPuffer, CPatterned, kET_Puffer)
TYPES_MATCH_IMPL(CPillBug, CWallCrawler, kET_PillBug)
TYPES_MATCH_IMPL(CParasite, CWallCrawler, kET_Parasite)
TYPES_MATCH_IMPL(CBabyMetroid, CMetroid, kET_BabyMetroid)
TYPES_MATCH_IMPL(CMetroid, CPatterned, kET_Metroid)
TYPES_MATCH_IMPL(CMetaree, CPatterned, kET_Metaree)
TYPES_MATCH_IMPL(CLumite, CPatterned, kET_Lumite)
TYPES_MATCH_IMPL(CIngSpiderballGuardian, CPatterned, kET_IngSpiderballGuardian)
TYPES_MATCH_IMPL(CIngSpaceJumpGuardian, CPatterned, kET_IngSpaceJumpGuardian)
TYPES_MATCH_IMPL(CIngBoostBallGuardian, CPatterned, kET_IngBoostBallGuardian)
TYPES_MATCH_IMPL(CIng, CPatterned, kET_Ing)
TYPES_MATCH_IMPL(CGrenchler, CPatterned, kET_Grenchler)
TYPES_MATCH_IMPL(CElitePirate, CPatterned, kET_ElitePirate)
TYPES_MATCH_IMPL(CDigitalGuardianHead, CPatterned, kET_DigitalGuardianHead)
TYPES_MATCH_IMPL(CDigitalGuardian, CPatterned, kET_DigitalGuardian)
TYPES_MATCH_IMPL(CDarkSamus, CPatterned, kET_DarkSamus)
TYPES_MATCH_IMPL(CPlasmaProjectile, CBeamProjectile, kET_PlasmaProjectile)
TYPES_MATCH_IMPL(CBeamProjectile, CGameProjectile, kET_BeamProjectile)
TYPES_MATCH_IMPL(CPlantScarabSwarm, CSwarmBasics, kET_PlantScarabSwarm)
TYPES_MATCH_IMPL(CIngBlobSwarm, CSwarmBasics, kET_IngBlobSwarm)
TYPES_MATCH_IMPL(CMetareeSwarm, CSwarmBasics, kET_MetareeSwarm)
TYPES_MATCH_IMPL(CBacteriaSwarm, CActor, kET_BacteriaSwarm)
TYPES_MATCH_IMPL(CWallCrawler, CPatterned, kET_WallCrawler)
TYPES_MATCH_IMPL(CFlyerSwarm, CSwarmBasics, kET_FlyerSwarm)
TYPES_MATCH_IMPL(CSwarmBasics, CActor, kET_SwarmBasics)
TYPES_MATCH_IMPL(CSnakeWeedSwarm, CActor, kET_SnakeWeedSwarm)
TYPES_MATCH_IMPL(CScriptWorldTeleporter, CEntity, kET_ScriptWorldTeleporter)
TYPES_MATCH_IMPL(CScriptWater, CScriptTrigger, kET_ScriptWater)
TYPES_MATCH_IMPL(CScriptSafeZone, CScriptTriggerEllipsoid, kET_ScriptSafeZone)
TYPES_MATCH_IMPL(CScriptTriggerOrientated, CScriptTrigger, kET_ScriptTriggerOrientated)
TYPES_MATCH_IMPL(CScriptTriggerEllipsoid, CScriptTrigger, kET_ScriptTriggerEllipsoid)
TYPES_MATCH_IMPL(CScriptTrigger, CActor, kET_ScriptTrigger)
TYPES_MATCH_IMPL(CScriptTextPane, CActor, kET_ScriptTextPane)
TYPES_MATCH_IMPL(CScriptTeamAiMgr, CEntity, kET_ScriptTeamAi)
TYPES_MATCH_IMPL(CScriptTargetingPoint, CActor, kET_ScriptTargetingPoint)
TYPES_MATCH_IMPL(CScriptSwitch, CEntity, kET_ScriptSwitch)
TYPES_MATCH_IMPL(CScriptStreamedMusic, CEntity, kET_ScriptStreamedMusic)
TYPES_MATCH_IMPL(CScriptSpecialFunction, CActor, kET_ScriptSpecialFunction)
TYPES_MATCH_IMPL(CScriptSpawnPoint, CEntity, kET_ScriptSpawnPoint)
TYPES_MATCH_IMPL(CScriptSound, CActor, kET_ScriptSound)
TYPES_MATCH_IMPL(CScriptRiftPortal, CActor, kET_ScriptRiftPortal)
TYPES_MATCH_IMPL(CScriptRepulsor, CActor, kET_ScriptRepulsor)
TYPES_MATCH_IMPL(CScriptRelay, CEntity, kET_Relay)
TYPES_MATCH_IMPL(CScriptPortalTransition, CEntity, kET_ScriptPortalTransition)
TYPES_MATCH_IMPL(CScriptPlatform, CPhysicsActor, kET_ScriptPlatform)
TYPES_MATCH_IMPL(CScriptPlayerProxy, CActor, kET_ScriptPlayerProxy)
TYPES_MATCH_IMPL(CScriptPickup, CActor, kET_ScriptPickup)
TYPES_MATCH_IMPL(CScriptLayerController, CEntity, kET_ScriptLayerController)
TYPES_MATCH_IMPL(CScriptGuiSlider, CScriptGuiWidget, kET_ScriptGuiSlider)
TYPES_MATCH_IMPL(CScriptGuiScreen, CActor, kET_ScriptGuiScreen)
TYPES_MATCH_IMPL(CScriptGuiMenu, CScriptGuiWidget, kET_ScriptGuiMenu)
TYPES_MATCH_IMPL(CScriptGrapplePoint, CActor, kET_ScriptGrapplePoint)
TYPES_MATCH_IMPL(CScriptEffect, CActor, kET_ScriptEffect)
TYPES_MATCH_IMPL(CScriptDynamicLight, CGameLight, kET_ScriptDynamicLight)
TYPES_MATCH_IMPL(CScriptDoor, CPhysicsActor, kET_ScriptDoor)
TYPES_MATCH_IMPL(CScriptDock, CPhysicsActor, kET_ScriptDock)
TYPES_MATCH_IMPL(CScriptDestructibleBarrier, CPhysicsActor, kET_ScriptDestructibleBarrier)
TYPES_MATCH_IMPL(CScriptDarkSamusBattleStage, CEntity, kET_DarkSamusBattleStage)
TYPES_MATCH_IMPL(CScriptDamageableTrigger, CActor, kET_ScriptDamageableTrigger)
TYPES_MATCH_IMPL(CScriptCoverPoint, CActor, kET_ScriptCoverPoint)
TYPES_MATCH_IMPL(CScriptCounter, CEntity, kET_ScriptCounter)
TYPES_MATCH_IMPL(CScriptColorModulate, CEntity, kET_ScriptColorModulate)
TYPES_MATCH_IMPL(CScriptCamera, CActor, kET_ScriptCamera)
TYPES_MATCH_IMPL(CScriptCameraShaker, CEntity, kET_ScriptCameraShaker)
TYPES_MATCH_IMPL(CScriptAIWaypoint, CScriptWaypoint, kET_ScriptAIWaypoint)
TYPES_MATCH_IMPL(CScriptAiJumpPoint, CActor, kET_ScriptAiJumpPoint)
TYPES_MATCH_IMPL(CScriptAIHint, CActor, kET_ScriptAIHint)
TYPES_MATCH_IMPL(CScriptActorKeyframe, CEntity, kET_ScriptActorKeyframe)
TYPES_MATCH_IMPL(CScriptActor, CPhysicsActor, kET_ScriptActor)
TYPES_MATCH_IMPL(CPlayer, CPhysicsActor, kET_Player)
TYPES_MATCH_IMPL(CIngSnatchingSwarm, CActor, kET_IngSnatchingSwarm)
TYPES_MATCH_IMPL(CIngPuddle, CPhysicsActor, kET_IngPuddle)
TYPES_MATCH_IMPL(CHUDBillboardEffect, CEffect, kET_HUDBillboardEffect)
TYPES_MATCH_IMPL(CGameLight, CActor, kET_GameLight)
TYPES_MATCH_IMPL(CFishCloud, CActor, kET_FishCloud)
TYPES_MATCH_IMPL(CExplosion, CEffect, kET_Explosion)
TYPES_MATCH_IMPL(CScattershotProjectile, CWeapon, kET_ScattershotProjectile)
TYPES_MATCH_IMPL(CEnergyProjectile, CGameProjectile, kET_EnergyProjectile)
TYPES_MATCH_IMPL(CCollisionActor, CPhysicsActor, kET_CollisionActor)
TYPES_MATCH_IMPL(CCinematicCamera, CGameCamera, kET_CinematicCamera)
TYPES_MATCH_IMPL(CBouncyGrenade, CPhysicsActor, kET_BouncyGrenade)
TYPES_MATCH_IMPL(CBouncingBomb, CWeapon, kET_BouncingBomb)
TYPES_MATCH_IMPL(CBomb, CWeapon, kET_Bomb)
TYPES_MATCH_IMPL(CScriptSequenceTimer, CEntity, kET_ScriptSequenceTimer)
TYPES_MATCH_IMPL(CScriptGuiWidget, CEntity, kET_ScriptGuiWidget)
TYPES_MATCH_IMPL(CScriptWaypoint, CActor, kET_ScriptWaypoint)
TYPES_MATCH_IMPL(CGameProjectile, CWeapon, kET_GameProjectile)
TYPES_MATCH_IMPL(CEffect, CActor, kET_Effect)
TYPES_MATCH_IMPL(CWeapon, CActor, kET_Weapon)
TYPES_MATCH_IMPL(CGameCamera, CActor, kET_GameCamera)
TYPES_MATCH_IMPL(CPatterned, CAi, kET_Patterned)
TYPES_MATCH_IMPL(CAi, CPhysicsActor, kET_Ai)
TYPES_MATCH_IMPL(CPhysicsActor, CActor, kET_PhysicsActor)
TYPES_MATCH_IMPL(CActor, CEntity, kET_Actor)

#undef TYPES_MATCH_IMPL

CEntity* CEntity::TypesMatch(int typeId) const {
  return typeId == kET_Entity ? const_cast< CEntity* >(this) : nullptr;
}

CEntity* TryCast(CEntity* entity, int typeId) {
  if (entity != nullptr) {
    return entity->TypesMatch(typeId);
  }
  return nullptr;
}

#define CAST_TO_PTR_IMPL(cls, id)                     \
  template <>                                        \
  cls* TCastToPtr< cls >(CEntity* entity) {          \
    return static_cast< cls* >(TryCast(entity, id)); \
  }

CAST_TO_PTR_IMPL(CEntity, kET_Entity)
CAST_TO_PTR_IMPL(CScriptSequenceTimer, kET_ScriptSequenceTimer)
CAST_TO_PTR_IMPL(CPlayer, kET_Player)
CAST_TO_PTR_IMPL(CScriptActor, kET_ScriptActor)
CAST_TO_PTR_IMPL(CScriptEffect, kET_ScriptEffect)
CAST_TO_PTR_IMPL(CScriptPickup, kET_ScriptPickup)
CAST_TO_PTR_IMPL(CScriptSpawnPoint, kET_ScriptSpawnPoint)
CAST_TO_PTR_IMPL(CScriptStreamedMusic, kET_ScriptStreamedMusic)
CAST_TO_PTR_IMPL(CScriptForgottenObject, kET_ScriptForgottenObject)

#undef CAST_TO_PTR_IMPL

#define CAST_TO_REF_IMPL(cls, id)                              \
  template <>                                                  \
  cls* TCastToPtr< cls >(CEntity& entity) {                    \
    return static_cast< cls* >(entity.TypesMatch(id));        \
  }

CAST_TO_REF_IMPL(CEntity, kET_Entity)
CAST_TO_REF_IMPL(CScriptSequenceTimer, kET_ScriptSequenceTimer)
CAST_TO_REF_IMPL(CPlayer, kET_Player)
CAST_TO_REF_IMPL(CScriptActor, kET_ScriptActor)
CAST_TO_REF_IMPL(CScriptEffect, kET_ScriptEffect)
CAST_TO_REF_IMPL(CScriptPickup, kET_ScriptPickup)
CAST_TO_REF_IMPL(CScriptSpawnPoint, kET_ScriptSpawnPoint)
CAST_TO_REF_IMPL(CScriptStreamedMusic, kET_ScriptStreamedMusic)
CAST_TO_REF_IMPL(CScriptForgottenObject, kET_ScriptForgottenObject)

#undef CAST_TO_REF_IMPL

template <>
CActor* TCastToPtr< CActor >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 1) != 0) {
    return static_cast< CActor* >(entity);
  }
  return nullptr;
}

template <>
CActor* TCastToPtr< CActor >(CEntity& entity) {
  if ((entity.GetCastFlags() & 1) != 0) {
    return static_cast< CActor* >(&entity);
  }
  return nullptr;
}

template <>
CPhysicsActor* TCastToPtr< CPhysicsActor >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 2) != 0) {
    return static_cast< CPhysicsActor* >(entity);
  }
  return nullptr;
}

template <>
CPhysicsActor* TCastToPtr< CPhysicsActor >(CEntity& entity) {
  if ((entity.GetCastFlags() & 2) != 0) {
    return static_cast< CPhysicsActor* >(&entity);
  }
  return nullptr;
}
