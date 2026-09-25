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

// The casts run in retail order: the cast-flag tests from CAi (flag 8, set by its constructor)
// down to CActor (flag 1), then one TryCast pair per type id from 160 down to 5, then CEntity.
// Ids whose class no source here names are left as comments.
template <>
CAi* TCastToPtr< CAi >(CEntity& entity) {
  if ((entity.GetCastFlags() & 8) != 0) {
    return static_cast< CAi* >(&entity);
  }
  return nullptr;
}

template <>
CAi* TCastToPtr< CAi >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 8) != 0) {
    return static_cast< CAi* >(entity);
  }
  return nullptr;
}

template <>
CPatterned* TCastToPtr< CPatterned >(CEntity& entity) {
  if ((entity.GetCastFlags() & 4) != 0) {
    return static_cast< CPatterned* >(&entity);
  }
  return nullptr;
}

template <>
CPatterned* TCastToPtr< CPatterned >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 4) != 0) {
    return static_cast< CPatterned* >(entity);
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

template <>
CPhysicsActor* TCastToPtr< CPhysicsActor >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 2) != 0) {
    return static_cast< CPhysicsActor* >(entity);
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
CActor* TCastToPtr< CActor >(CEntity* entity) {
  if (entity != nullptr && (entity->GetCastFlags() & 1) != 0) {
    return static_cast< CActor* >(entity);
  }
  return nullptr;
}

#define CAST_TO_IMPL(cls, id)                                \
  template <>                                                \
  cls* TCastToPtr< cls >(CEntity* entity) {                  \
    return static_cast< cls* >(TryCast(entity, id));         \
  }                                                          \
  template <>                                                \
  cls* TCastToPtr< cls >(CEntity& entity) {                  \
    return static_cast< cls* >(entity.TypesMatch(id));       \
  }

// Named by the Trilogy's TCastToPtr<17CScriptPlayerHint>; its parent (id 33) is not named
// anywhere, so the class stays incomplete and the casts cannot be static.
class CScriptPlayerHint;

#define CAST_TO_IMPL_INCOMPLETE(cls, id)                     \
  template <>                                                \
  cls* TCastToPtr< cls >(CEntity* entity) {                  \
    return reinterpret_cast< cls* >(TryCast(entity, id));    \
  }                                                          \
  template <>                                                \
  cls* TCastToPtr< cls >(CEntity& entity) {                  \
    return reinterpret_cast< cls* >(entity.TypesMatch(id));  \
  }

CAST_TO_IMPL(CScriptForgottenObject, kET_ScriptForgottenObject)
CAST_TO_IMPL(CPuddleSpore, kET_PuddleSpore)
CAST_TO_IMPL(COctapedeSegment, kET_OctapedeSegment)
CAST_TO_IMPL(CKrocuss, kET_Krocuss)
CAST_TO_IMPL(CPowerBomb, kET_PowerBomb)
CAST_TO_IMPL(CScriptFrontEndDataNetwork, kET_ScriptFrontEndDataNetwork)
CAST_TO_IMPL(CStoneToad, kET_StoneToad)
CAST_TO_IMPL(CAIMannedTurret, kET_AIMannedTurret)
// id 152: class not named by any source here (fn_800978A0, fn_800978C4)
CAST_TO_IMPL(CShredder, kET_Shredder)
CAST_TO_IMPL(CWallWalker, kET_WallWalker)
CAST_TO_IMPL(CBlogg, kET_Blogg)
CAST_TO_IMPL(CBoostBallGuardian, kET_BoostBallGuardian)
CAST_TO_IMPL(CMinorIng, kET_MinorIng)
CAST_TO_IMPL(CSporbProjectile, kET_SporbProjectile)
CAST_TO_IMPL(CSporbTop, kET_SporbTop)
CAST_TO_IMPL(CSporbNeedle, kET_SporbNeedle)
CAST_TO_IMPL(CSporbBase, kET_SporbBase)
CAST_TO_IMPL(CGlowbug, kET_Glowbug)
CAST_TO_IMPL(CKralee, kET_Kralee)
CAST_TO_IMPL(CGunTurretTop, kET_GunTurretTop)
CAST_TO_IMPL(CGunTurretBase, kET_GunTurretBase)
CAST_TO_IMPL(CScriptPlayerTurret, kET_ScriptPlayerTurret)
// id 137: class not named by any source here (fn_80097D8C, fn_80097DB0)
CAST_TO_IMPL(CWispTentacle, kET_WispTentacle)
CAST_TO_IMPL(CSplitterCommandModule, kET_SplitterCommandModule)
CAST_TO_IMPL(CSplitterMainChassis, kET_SplitterMainChassis)
CAST_TO_IMPL(CSpankWeed, kET_SpankWeed)
CAST_TO_IMPL(CSpacePirate, kET_SpacePirate)
CAST_TO_IMPL(CSandwormEye, kET_SandwormEye)
CAST_TO_IMPL(CSandworm, kET_Sandworm)
CAST_TO_IMPL(CSandBoss, kET_SandBoss)
CAST_TO_IMPL(CRipper, kET_Ripper)
CAST_TO_IMPL(CRezbit, kET_Rezbit)
CAST_TO_IMPL(CPuffer, kET_Puffer)
CAST_TO_IMPL(CPillBug, kET_PillBug)
CAST_TO_IMPL(CParasite, kET_Parasite)
CAST_TO_IMPL(CBabyMetroid, kET_BabyMetroid)
CAST_TO_IMPL(CMetroid, kET_Metroid)
CAST_TO_IMPL(CMetaree, kET_Metaree)
CAST_TO_IMPL(CLumite, kET_Lumite)
CAST_TO_IMPL(CIngSpiderballGuardian, kET_IngSpiderballGuardian)
CAST_TO_IMPL(CIngSpaceJumpGuardian, kET_IngSpaceJumpGuardian)
CAST_TO_IMPL(CIngBoostBallGuardian, kET_IngBoostBallGuardian)
CAST_TO_IMPL(CIng, kET_Ing)
CAST_TO_IMPL(CGrenchler, kET_Grenchler)
CAST_TO_IMPL(CElitePirate, kET_ElitePirate)
CAST_TO_IMPL(CDigitalGuardianHead, kET_DigitalGuardianHead)
CAST_TO_IMPL(CDigitalGuardian, kET_DigitalGuardian)
CAST_TO_IMPL(CDarkSamus, kET_DarkSamus)
CAST_TO_IMPL(CPlasmaProjectile, kET_PlasmaProjectile)
CAST_TO_IMPL(CBeamProjectile, kET_BeamProjectile)
CAST_TO_IMPL(CPlantScarabSwarm, kET_PlantScarabSwarm)
CAST_TO_IMPL(CIngBlobSwarm, kET_IngBlobSwarm)
CAST_TO_IMPL(CMetareeSwarm, kET_MetareeSwarm)
CAST_TO_IMPL(CBacteriaSwarm, kET_BacteriaSwarm)
CAST_TO_IMPL(CWallCrawler, kET_WallCrawler)
CAST_TO_IMPL(CFlyerSwarm, kET_FlyerSwarm)
CAST_TO_IMPL(CSwarmBasics, kET_SwarmBasics)
// id 101: class not named by any source here (fn_8009895C, fn_80098980)
// id 100: class not named by any source here (fn_800989B0, fn_800989D4)
CAST_TO_IMPL(CSnakeWeedSwarm, kET_SnakeWeedSwarm)
CAST_TO_IMPL(CScriptWorldTeleporter, kET_ScriptWorldTeleporter)
CAST_TO_IMPL(CScriptWater, kET_ScriptWater)
// id 96: class not named by any source here (fn_80098B00, fn_80098B24)
CAST_TO_IMPL(CScriptSafeZone, kET_ScriptSafeZone)
CAST_TO_IMPL(CScriptTriggerOrientated, kET_ScriptTriggerOrientated)
CAST_TO_IMPL(CScriptTriggerEllipsoid, kET_ScriptTriggerEllipsoid)
CAST_TO_IMPL(CScriptTrigger, kET_ScriptTrigger)
// id 91: class not named by any source here (fn_80098CA4, fn_80098CC8)
// id 90: class not named by any source here (fn_80098CF8, fn_80098D1C)
CAST_TO_IMPL(CScriptTextPane, kET_ScriptTextPane)
CAST_TO_IMPL(CScriptTeamAiMgr, kET_ScriptTeamAi)
CAST_TO_IMPL(CScriptTargetingPoint, kET_ScriptTargetingPoint)
CAST_TO_IMPL(CScriptSwitch, kET_ScriptSwitch)
// id 85: class not named by any source here (fn_80098E9C, fn_80098EC0)
CAST_TO_IMPL(CScriptStreamedMusic, kET_ScriptStreamedMusic)
// id 83: class not named by any source here (fn_80098F44, fn_80098F68)
// id 82: class not named by any source here (fn_80098F98, fn_80098FBC)
// id 81: class not named by any source here (fn_80098FEC, fn_80099010)
CAST_TO_IMPL(CScriptSpecialFunction, kET_ScriptSpecialFunction)
CAST_TO_IMPL(CScriptSpawnPoint, kET_ScriptSpawnPoint)
// id 78: class not named by any source here (fn_800990E8, fn_8009910C)
CAST_TO_IMPL(CScriptSound, kET_ScriptSound)
// id 76: class not named by any source here (fn_80099190, fn_800991B4)
CAST_TO_IMPL(CScriptRiftPortal, kET_ScriptRiftPortal)
CAST_TO_IMPL(CScriptRepulsor, kET_ScriptRepulsor)
CAST_TO_IMPL(CScriptRelay, kET_Relay)
CAST_TO_IMPL(CScriptPortalTransition, kET_ScriptPortalTransition)
// id 71: class not named by any source here (fn_80099334, fn_80099358)
CAST_TO_IMPL(CScriptPlatform, kET_ScriptPlatform)
CAST_TO_IMPL(CScriptPlayerProxy, kET_ScriptPlayerProxy)
CAST_TO_IMPL_INCOMPLETE(CScriptPlayerHint, kET_ScriptPlayerHint)
// id 67: class not named by any source here (fn_80099484, fn_800994A8)
CAST_TO_IMPL(CScriptPickup, kET_ScriptPickup)
// id 65: class not named by any source here (fn_8009952C, fn_80099550)
CAST_TO_IMPL(CScriptLayerController, kET_ScriptLayerController)
// id 63: class not named by any source here (fn_800995D4, fn_800995F8)
CAST_TO_IMPL(CScriptGuiSlider, kET_ScriptGuiSlider)
CAST_TO_IMPL(CScriptGuiScreen, kET_ScriptGuiScreen)
CAST_TO_IMPL(CScriptGuiMenu, kET_ScriptGuiMenu)
CAST_TO_IMPL(CScriptGrapplePoint, kET_ScriptGrapplePoint)
CAST_TO_IMPL(CScriptEffect, kET_ScriptEffect)
CAST_TO_IMPL(CScriptDynamicLight, kET_ScriptDynamicLight)
CAST_TO_IMPL(CScriptDoor, kET_ScriptDoor)
CAST_TO_IMPL(CScriptDock, kET_ScriptDock)
// id 54: class not named by any source here (fn_800998C8, fn_800998EC)
CAST_TO_IMPL(CScriptDestructibleBarrier, kET_ScriptDestructibleBarrier)
// id 52: class not named by any source here (fn_80099970, fn_80099994)
CAST_TO_IMPL(CScriptDarkSamusBattleStage, kET_DarkSamusBattleStage)
// id 50: class not named by any source here (fn_80099A18, fn_80099A3C)
CAST_TO_IMPL(CScriptDamageableTrigger, kET_ScriptDamageableTrigger)
CAST_TO_IMPL(CScriptCoverPoint, kET_ScriptCoverPoint)
CAST_TO_IMPL(CScriptCounter, kET_ScriptCounter)
// id 46: class not named by any source here (fn_80099B68, fn_80099B8C)
CAST_TO_IMPL(CScriptColorModulate, kET_ScriptColorModulate)
CAST_TO_IMPL(CScriptCamera, kET_ScriptCamera)
// id 43: class not named by any source here (fn_80099C64, fn_80099C88)
// id 42: class not named by any source here (fn_80099CB8, fn_80099CDC)
CAST_TO_IMPL(CScriptCameraShaker, kET_ScriptCameraShaker)
// id 40: class not named by any source here (fn_80099D60, fn_80099D84)
CAST_TO_IMPL(CScriptAIWaypoint, kET_ScriptAIWaypoint)
CAST_TO_IMPL(CScriptAiJumpPoint, kET_ScriptAiJumpPoint)
CAST_TO_IMPL(CScriptAIHint, kET_ScriptAIHint)
// id 36: class not named by any source here (fn_80099EB0, fn_80099ED4)
CAST_TO_IMPL(CScriptActorKeyframe, kET_ScriptActorKeyframe)
CAST_TO_IMPL(CScriptActor, kET_ScriptActor)
// id 33: class not named by any source here (fn_80099FAC, fn_80099FD0)
CAST_TO_IMPL(CPlayer, kET_Player)
// id 31: class not named by any source here (fn_8009A054, fn_8009A078)
CAST_TO_IMPL(CIngSnatchingSwarm, kET_IngSnatchingSwarm)
CAST_TO_IMPL(CIngPuddle, kET_IngPuddle)
CAST_TO_IMPL(CHUDBillboardEffect, kET_HUDBillboardEffect)
// id 27: class not named by any source here (fn_8009A1A4, fn_8009A1C8)
CAST_TO_IMPL(CGameLight, kET_GameLight)
CAST_TO_IMPL(CFishCloud, kET_FishCloud)
// id 24: class not named by any source here (fn_8009A2A0, fn_8009A2C4)
// id 23: class not named by any source here (fn_8009A2F4, CastGameCameratoFirstPersonCamera__14CCameraManagerFPC11CGameCamera)
CAST_TO_IMPL(CExplosion, kET_Explosion)
CAST_TO_IMPL(CScattershotProjectile, kET_ScattershotProjectile)
// id 20: class not named by any source here (fn_8009A3F0, fn_8009A414)
CAST_TO_IMPL(CEnergyProjectile, kET_EnergyProjectile)
CAST_TO_IMPL(CCollisionActor, kET_CollisionActor)
CAST_TO_IMPL(CCinematicCamera, kET_CinematicCamera)
CAST_TO_IMPL(CBouncyGrenade, kET_BouncyGrenade)
CAST_TO_IMPL(CBouncingBomb, kET_BouncingBomb)
CAST_TO_IMPL(CBomb, kET_Bomb)
// id 13: class not named by any source here (fn_8009A63C, fn_8009A660)
CAST_TO_IMPL(CScriptSequenceTimer, kET_ScriptSequenceTimer)
CAST_TO_IMPL(CScriptGuiWidget, kET_ScriptGuiWidget)
// id 10: class not named by any source here (fn_8009A738, fn_8009A75C)
CAST_TO_IMPL(CScriptWaypoint, kET_ScriptWaypoint)
CAST_TO_IMPL(CGameProjectile, kET_GameProjectile)
CAST_TO_IMPL(CEffect, kET_Effect)
CAST_TO_IMPL(CWeapon, kET_Weapon)
CAST_TO_IMPL(CGameCamera, kET_GameCamera)
CAST_TO_IMPL(CEntity, kET_Entity)

#undef CAST_TO_IMPL_INCOMPLETE
#undef CAST_TO_IMPL
