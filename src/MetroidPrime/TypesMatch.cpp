#include "MetroidPrime/CEntity.hpp"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/TToken.hpp"
#include "rstl/optional_object.hpp"
#include "rstl/single_ptr.hpp"
#include "rstl/vector.hpp"
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
// PlasmaProjectile chain). Id 23 CFirstPersonCamera comes from its (CEntity&) cast, already named
// CCameraManager::CastGameCameratoFirstPersonCamera in config/G2ME01/symbols.txt. Everything else
// was read from the Trilogy configs in config/R3ME01, R3MP01 and R32J01, or from the EEntityType
// enum. Each destructor here is named by the TypesMatch override in the vtable it stores.
//
// Classes without headers yet, declared just far enough to define their overrides. Each parent is
// the class whose override the retail function calls.
// Member types for the destructors below whose real types no source here names. Each stands in
// only for what the retail destructor does with it: an out-of-line destructor (called with -1), or
// for SPolyMember a virtual destructor in the fifth vtable slot.
struct SOutOfLineMember {
  ~SOutOfLineMember();
  uchar x0_data[4];
};

struct SInlineWrapper {
  SOutOfLineMember x0_member;
};

struct SRefHolder {
  ~SRefHolder() { Release(); }
  void Release();
  void* x0_ptr;
  int* x4_count;
};

struct SRefHolderWrapper : SRefHolder {};

class SPolyMember {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual ~SPolyMember();
};

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
class CCollisionActor : public CPhysicsActor {
public:
  ~CCollisionActor();
  CEntity* TypesMatch(int typeId) const;

private:
  uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)];
  SOutOfLineMember x2f0_member;
  rstl::single_ptr< SPolyMember > x2f4_ptr;
  rstl::single_ptr< SPolyMember > x2f8_ptr;
  rstl::single_ptr< SPolyMember > x2fc_ptr;
  uchar x300_pad[0x324 - 0x300];
  SOutOfLineMember x324_member;
  uchar x328_pad[0x368 - 0x328];
  SRefHolderWrapper x368_ref;
};
class CEnergyProjectile : public CGameProjectile {
public:
  ~CEnergyProjectile();
  CEntity* TypesMatch(int typeId) const;

private:
  uchar x_pad0[0x434 - sizeof(CGameProjectile)];
  SOutOfLineMember x434_member;
  uchar x438_pad[0x528 - 0x438];
  SInlineWrapper x528_member;
};
TYPES_MATCH_CLASS(CScattershotProjectile, CWeapon)
TYPES_MATCH_CLASS(CExplosion, CEffect)
TYPES_MATCH_CLASS(CFirstPersonCamera, CGameCamera)
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
class CScriptDamageableTrigger : public CActor {
public:
  ~CScriptDamageableTrigger();
  CEntity* TypesMatch(int typeId) const;

private:
  uchar x_pad0[0x190 - sizeof(CActor)];
  SOutOfLineMember x190_member;
};
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

// Only the members its destructor touches, at the offsets it touches them; the token and
// particle types are placeholders.
class CPlasmaProjectile : public CBeamProjectile {
public:
  ~CPlasmaProjectile();
  CEntity* TypesMatch(int typeId) const;

private:
  uchar x_pad0[0x598 - sizeof(CBeamProjectile)];
  rstl::vector< TUniqueId > x598_ids;
  uchar x5a8_pad[0x620 - 0x5a8];
  TCachedToken< CGenDescription > x620_token;
  TCachedToken< CGenDescription > x62c_token;
  TCachedToken< CGenDescription > x638_token;
  rstl::optional_object< TCachedToken< CGenDescription > > x644_token;
  rstl::optional_object< TCachedToken< CGenDescription > > x654_token;
  rstl::single_ptr< CElementGen > x664_gen;
  rstl::single_ptr< CElementGen > x668_gen;
  rstl::single_ptr< CElementGen > x66c_gen;
  rstl::single_ptr< CElementGen > x670_gen;
  uchar x674_pad[0x688 - 0x674];
  rstl::optional_object< TToken< CGenDescription > > x688_token;
  rstl::optional_object< TToken< CGenDescription > > x694_token;
};

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

// 2026-09-25: the 32 classes below are unidentified. Every naming source this tree holds was
// exhausted - the R3ME01/R3MP01/R32J01 configs (Prime 1 names, all already used here),
// G2ME01/symbols.txt (no vtable covers them), the DOL's strings, and the RELs (their DOL
// references are `0xFFFFFFFF` plus a stripped import table) - so `CUnknown<id>` is a placeholder.
// What is *not* a guess is the shape: each class's parent is the class whose `::TypesMatch` the
// retail override calls, and the id, the two cast addresses and the class's own virtuals are read
// out of the vtable that holds them. docs/research/TypesMatch_unnamed_ids.txt is that table, and
// docs/research/rename_typesmatch_ids.py regenerates this block from it - when a real name is
// found, put it in CLASS there and re-run; nothing else changes. All 94 functions below compile to
// bytes identical to retail, which is why they are worth having under a placeholder.
TYPES_MATCH_CLASS(CUnknown10, CActor)
TYPES_MATCH_CLASS(CUnknown13, CGameCamera)
TYPES_MATCH_CLASS(CUnknown20, CEnergyProjectile)
TYPES_MATCH_CLASS(CUnknown24, CGameCamera)
TYPES_MATCH_CLASS(CUnknown27, CWeapon)
TYPES_MATCH_CLASS(CUnknown31, CGameCamera)
TYPES_MATCH_CLASS(CUnknown33, CActor)
TYPES_MATCH_CLASS(CUnknown36, CEntity)
TYPES_MATCH_CLASS(CUnknown42, CActor)
TYPES_MATCH_CLASS(CUnknown43, CScriptWaypoint)
TYPES_MATCH_CLASS(CUnknown50, CScriptDamageableTrigger)
TYPES_MATCH_CLASS(CUnknown52, CPhysicsActor)
TYPES_MATCH_CLASS(CUnknown54, CEntity)
TYPES_MATCH_CLASS(CUnknown63, CActor)
TYPES_MATCH_CLASS(CUnknown65, CEntity)
TYPES_MATCH_CLASS(CUnknown67, CEntity)
TYPES_MATCH_CLASS(CUnknown71, CActor)
TYPES_MATCH_CLASS(CUnknown76, CEntity)
TYPES_MATCH_CLASS(CUnknown78, CEntity)
TYPES_MATCH_CLASS(CUnknown81, CActor)
TYPES_MATCH_CLASS(CUnknown82, CScriptWaypoint)
TYPES_MATCH_CLASS(CUnknown83, CActor)
TYPES_MATCH_CLASS(CUnknown85, CActor)
TYPES_MATCH_CLASS(CUnknown90, CEntity)
TYPES_MATCH_CLASS(CUnknown91, CEntity)
TYPES_MATCH_CLASS(CUnknown96, CActor)
TYPES_MATCH_CLASS(CUnknown100, CGameCamera)
TYPES_MATCH_CLASS(CUnknown101, CGameCamera)
TYPES_MATCH_CLASS(CUnknown137, CActor)
TYPES_MATCH_CLASS(CUnknown152, CEnergyProjectile)
TYPES_MATCH_CLASS(CUnknown40, CActor)
TYPES_MATCH_CLASS(CUnknown46, CActor)

#undef TYPES_MATCH_CLASS

CPlasmaProjectile::~CPlasmaProjectile() {}

CBeamProjectile::~CBeamProjectile() {}

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
TYPES_MATCH_IMPL(CUnknown152, CEnergyProjectile, 152)
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
TYPES_MATCH_IMPL(CUnknown137, CActor, 137)
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
TYPES_MATCH_IMPL(CUnknown101, CGameCamera, 101)
TYPES_MATCH_IMPL(CUnknown100, CGameCamera, 100)
TYPES_MATCH_IMPL(CSnakeWeedSwarm, CActor, kET_SnakeWeedSwarm)
TYPES_MATCH_IMPL(CScriptWorldTeleporter, CEntity, kET_ScriptWorldTeleporter)
TYPES_MATCH_IMPL(CScriptWater, CScriptTrigger, kET_ScriptWater)
TYPES_MATCH_IMPL(CUnknown96, CActor, 96)
TYPES_MATCH_IMPL(CScriptSafeZone, CScriptTriggerEllipsoid, kET_ScriptSafeZone)
TYPES_MATCH_IMPL(CScriptTriggerOrientated, CScriptTrigger, kET_ScriptTriggerOrientated)
TYPES_MATCH_IMPL(CScriptTriggerEllipsoid, CScriptTrigger, kET_ScriptTriggerEllipsoid)
TYPES_MATCH_IMPL(CScriptTrigger, CActor, kET_ScriptTrigger)
TYPES_MATCH_IMPL(CUnknown91, CEntity, 91)
TYPES_MATCH_IMPL(CUnknown90, CEntity, 90)
TYPES_MATCH_IMPL(CScriptTextPane, CActor, kET_ScriptTextPane)
TYPES_MATCH_IMPL(CScriptTeamAiMgr, CEntity, kET_ScriptTeamAi)
TYPES_MATCH_IMPL(CScriptTargetingPoint, CActor, kET_ScriptTargetingPoint)
TYPES_MATCH_IMPL(CScriptSwitch, CEntity, kET_ScriptSwitch)
TYPES_MATCH_IMPL(CUnknown85, CActor, 85)
TYPES_MATCH_IMPL(CScriptStreamedMusic, CEntity, kET_ScriptStreamedMusic)
TYPES_MATCH_IMPL(CUnknown83, CActor, 83)
TYPES_MATCH_IMPL(CUnknown82, CScriptWaypoint, 82)
TYPES_MATCH_IMPL(CUnknown81, CActor, 81)
TYPES_MATCH_IMPL(CScriptSpecialFunction, CActor, kET_ScriptSpecialFunction)
TYPES_MATCH_IMPL(CScriptSpawnPoint, CEntity, kET_ScriptSpawnPoint)
TYPES_MATCH_IMPL(CUnknown78, CEntity, 78)
TYPES_MATCH_IMPL(CScriptSound, CActor, kET_ScriptSound)
TYPES_MATCH_IMPL(CUnknown76, CEntity, 76)
TYPES_MATCH_IMPL(CScriptRiftPortal, CActor, kET_ScriptRiftPortal)
TYPES_MATCH_IMPL(CScriptRepulsor, CActor, kET_ScriptRepulsor)
TYPES_MATCH_IMPL(CScriptRelay, CEntity, kET_Relay)
TYPES_MATCH_IMPL(CScriptPortalTransition, CEntity, kET_ScriptPortalTransition)
TYPES_MATCH_IMPL(CUnknown71, CActor, 71)
TYPES_MATCH_IMPL(CScriptPlatform, CPhysicsActor, kET_ScriptPlatform)
TYPES_MATCH_IMPL(CScriptPlayerProxy, CActor, kET_ScriptPlayerProxy)
TYPES_MATCH_IMPL(CUnknown67, CEntity, 67)
TYPES_MATCH_IMPL(CScriptPickup, CActor, kET_ScriptPickup)
TYPES_MATCH_IMPL(CUnknown65, CEntity, 65)
TYPES_MATCH_IMPL(CScriptLayerController, CEntity, kET_ScriptLayerController)
TYPES_MATCH_IMPL(CUnknown63, CActor, 63)
TYPES_MATCH_IMPL(CScriptGuiSlider, CScriptGuiWidget, kET_ScriptGuiSlider)
TYPES_MATCH_IMPL(CScriptGuiScreen, CActor, kET_ScriptGuiScreen)
TYPES_MATCH_IMPL(CScriptGuiMenu, CScriptGuiWidget, kET_ScriptGuiMenu)
TYPES_MATCH_IMPL(CScriptGrapplePoint, CActor, kET_ScriptGrapplePoint)
TYPES_MATCH_IMPL(CScriptEffect, CActor, kET_ScriptEffect)
TYPES_MATCH_IMPL(CScriptDynamicLight, CGameLight, kET_ScriptDynamicLight)
TYPES_MATCH_IMPL(CScriptDoor, CPhysicsActor, kET_ScriptDoor)
TYPES_MATCH_IMPL(CScriptDock, CPhysicsActor, kET_ScriptDock)
TYPES_MATCH_IMPL(CUnknown54, CEntity, 54)
TYPES_MATCH_IMPL(CScriptDestructibleBarrier, CPhysicsActor, kET_ScriptDestructibleBarrier)
TYPES_MATCH_IMPL(CUnknown52, CPhysicsActor, 52)
TYPES_MATCH_IMPL(CScriptDarkSamusBattleStage, CEntity, kET_DarkSamusBattleStage)
TYPES_MATCH_IMPL(CUnknown50, CScriptDamageableTrigger, 50)
TYPES_MATCH_IMPL(CScriptDamageableTrigger, CActor, kET_ScriptDamageableTrigger)
TYPES_MATCH_IMPL(CScriptCoverPoint, CActor, kET_ScriptCoverPoint)
TYPES_MATCH_IMPL(CScriptCounter, CEntity, kET_ScriptCounter)
TYPES_MATCH_IMPL(CScriptColorModulate, CEntity, kET_ScriptColorModulate)
TYPES_MATCH_IMPL(CScriptCamera, CActor, kET_ScriptCamera)
TYPES_MATCH_IMPL(CUnknown43, CScriptWaypoint, 43)
TYPES_MATCH_IMPL(CUnknown42, CActor, 42)
TYPES_MATCH_IMPL(CScriptCameraShaker, CEntity, kET_ScriptCameraShaker)
TYPES_MATCH_IMPL(CScriptAIWaypoint, CScriptWaypoint, kET_ScriptAIWaypoint)
TYPES_MATCH_IMPL(CScriptAiJumpPoint, CActor, kET_ScriptAiJumpPoint)
TYPES_MATCH_IMPL(CScriptAIHint, CActor, kET_ScriptAIHint)
TYPES_MATCH_IMPL(CUnknown36, CEntity, 36)
TYPES_MATCH_IMPL(CScriptActorKeyframe, CEntity, kET_ScriptActorKeyframe)
TYPES_MATCH_IMPL(CScriptActor, CPhysicsActor, kET_ScriptActor)
TYPES_MATCH_IMPL(CUnknown33, CActor, 33)
TYPES_MATCH_IMPL(CPlayer, CPhysicsActor, kET_Player)
TYPES_MATCH_IMPL(CUnknown31, CGameCamera, 31)
TYPES_MATCH_IMPL(CIngSnatchingSwarm, CActor, kET_IngSnatchingSwarm)
TYPES_MATCH_IMPL(CIngPuddle, CPhysicsActor, kET_IngPuddle)
TYPES_MATCH_IMPL(CHUDBillboardEffect, CEffect, kET_HUDBillboardEffect)
TYPES_MATCH_IMPL(CUnknown27, CWeapon, 27)
TYPES_MATCH_IMPL(CGameLight, CActor, kET_GameLight)
TYPES_MATCH_IMPL(CFishCloud, CActor, kET_FishCloud)
TYPES_MATCH_IMPL(CUnknown24, CGameCamera, 24)
TYPES_MATCH_IMPL(CFirstPersonCamera, CGameCamera, kET_FirstPersonCamera)
TYPES_MATCH_IMPL(CExplosion, CEffect, kET_Explosion)
TYPES_MATCH_IMPL(CScattershotProjectile, CWeapon, kET_ScattershotProjectile)
TYPES_MATCH_IMPL(CUnknown20, CEnergyProjectile, 20)
TYPES_MATCH_IMPL(CEnergyProjectile, CGameProjectile, kET_EnergyProjectile)
TYPES_MATCH_IMPL(CCollisionActor, CPhysicsActor, kET_CollisionActor)
TYPES_MATCH_IMPL(CCinematicCamera, CGameCamera, kET_CinematicCamera)
TYPES_MATCH_IMPL(CBouncyGrenade, CPhysicsActor, kET_BouncyGrenade)
TYPES_MATCH_IMPL(CBouncingBomb, CWeapon, kET_BouncingBomb)
TYPES_MATCH_IMPL(CBomb, CWeapon, kET_Bomb)
TYPES_MATCH_IMPL(CUnknown13, CGameCamera, 13)
TYPES_MATCH_IMPL(CScriptSequenceTimer, CEntity, kET_ScriptSequenceTimer)
TYPES_MATCH_IMPL(CScriptGuiWidget, CEntity, kET_ScriptGuiWidget)
TYPES_MATCH_IMPL(CUnknown10, CActor, 10)
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
CAST_TO_IMPL(CUnknown152, 152)
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
CAST_TO_IMPL(CUnknown137, 137)
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
CAST_TO_IMPL(CUnknown101, 101)
CAST_TO_IMPL(CUnknown100, 100)
CAST_TO_IMPL(CSnakeWeedSwarm, kET_SnakeWeedSwarm)
CAST_TO_IMPL(CScriptWorldTeleporter, kET_ScriptWorldTeleporter)
CAST_TO_IMPL(CScriptWater, kET_ScriptWater)
CAST_TO_IMPL(CUnknown96, 96)
CAST_TO_IMPL(CScriptSafeZone, kET_ScriptSafeZone)
CAST_TO_IMPL(CScriptTriggerOrientated, kET_ScriptTriggerOrientated)
CAST_TO_IMPL(CScriptTriggerEllipsoid, kET_ScriptTriggerEllipsoid)
CAST_TO_IMPL(CScriptTrigger, kET_ScriptTrigger)
CAST_TO_IMPL(CUnknown91, 91)
CAST_TO_IMPL(CUnknown90, 90)
CAST_TO_IMPL(CScriptTextPane, kET_ScriptTextPane)
CAST_TO_IMPL(CScriptTeamAiMgr, kET_ScriptTeamAi)
CAST_TO_IMPL(CScriptTargetingPoint, kET_ScriptTargetingPoint)
CAST_TO_IMPL(CScriptSwitch, kET_ScriptSwitch)
CAST_TO_IMPL(CUnknown85, 85)
CAST_TO_IMPL(CScriptStreamedMusic, kET_ScriptStreamedMusic)
CAST_TO_IMPL(CUnknown83, 83)
CAST_TO_IMPL(CUnknown82, 82)
CAST_TO_IMPL(CUnknown81, 81)
CAST_TO_IMPL(CScriptSpecialFunction, kET_ScriptSpecialFunction)
CAST_TO_IMPL(CScriptSpawnPoint, kET_ScriptSpawnPoint)
CAST_TO_IMPL(CUnknown78, 78)
CAST_TO_IMPL(CScriptSound, kET_ScriptSound)
CAST_TO_IMPL(CUnknown76, 76)
CAST_TO_IMPL(CScriptRiftPortal, kET_ScriptRiftPortal)
CAST_TO_IMPL(CScriptRepulsor, kET_ScriptRepulsor)
CAST_TO_IMPL(CScriptRelay, kET_Relay)
CAST_TO_IMPL(CScriptPortalTransition, kET_ScriptPortalTransition)
CAST_TO_IMPL(CUnknown71, 71)
CAST_TO_IMPL(CScriptPlatform, kET_ScriptPlatform)
CAST_TO_IMPL(CScriptPlayerProxy, kET_ScriptPlayerProxy)
CAST_TO_IMPL_INCOMPLETE(CScriptPlayerHint, kET_ScriptPlayerHint)
CAST_TO_IMPL(CUnknown67, 67)
CAST_TO_IMPL(CScriptPickup, kET_ScriptPickup)
CAST_TO_IMPL(CUnknown65, 65)
CAST_TO_IMPL(CScriptLayerController, kET_ScriptLayerController)
CAST_TO_IMPL(CUnknown63, 63)
CAST_TO_IMPL(CScriptGuiSlider, kET_ScriptGuiSlider)
CAST_TO_IMPL(CScriptGuiScreen, kET_ScriptGuiScreen)
CAST_TO_IMPL(CScriptGuiMenu, kET_ScriptGuiMenu)
CAST_TO_IMPL(CScriptGrapplePoint, kET_ScriptGrapplePoint)
CAST_TO_IMPL(CScriptEffect, kET_ScriptEffect)
CAST_TO_IMPL(CScriptDynamicLight, kET_ScriptDynamicLight)
CAST_TO_IMPL(CScriptDoor, kET_ScriptDoor)
CAST_TO_IMPL(CScriptDock, kET_ScriptDock)
CAST_TO_IMPL(CUnknown54, 54)
CAST_TO_IMPL(CScriptDestructibleBarrier, kET_ScriptDestructibleBarrier)
CAST_TO_IMPL(CUnknown52, 52)
CAST_TO_IMPL(CScriptDarkSamusBattleStage, kET_DarkSamusBattleStage)
CAST_TO_IMPL(CUnknown50, 50)
CAST_TO_IMPL(CScriptDamageableTrigger, kET_ScriptDamageableTrigger)
CAST_TO_IMPL(CScriptCoverPoint, kET_ScriptCoverPoint)
CAST_TO_IMPL(CScriptCounter, kET_ScriptCounter)
CAST_TO_IMPL(CUnknown46, 46)
CAST_TO_IMPL(CScriptColorModulate, kET_ScriptColorModulate)
CAST_TO_IMPL(CScriptCamera, kET_ScriptCamera)
CAST_TO_IMPL(CUnknown43, 43)
CAST_TO_IMPL(CUnknown42, 42)
CAST_TO_IMPL(CScriptCameraShaker, kET_ScriptCameraShaker)
CAST_TO_IMPL(CUnknown40, 40)
CAST_TO_IMPL(CScriptAIWaypoint, kET_ScriptAIWaypoint)
CAST_TO_IMPL(CScriptAiJumpPoint, kET_ScriptAiJumpPoint)
CAST_TO_IMPL(CScriptAIHint, kET_ScriptAIHint)
CAST_TO_IMPL(CUnknown36, 36)
CAST_TO_IMPL(CScriptActorKeyframe, kET_ScriptActorKeyframe)
CAST_TO_IMPL(CScriptActor, kET_ScriptActor)
CAST_TO_IMPL(CUnknown33, 33)
CAST_TO_IMPL(CPlayer, kET_Player)
CAST_TO_IMPL(CUnknown31, 31)
CAST_TO_IMPL(CIngSnatchingSwarm, kET_IngSnatchingSwarm)
CAST_TO_IMPL(CIngPuddle, kET_IngPuddle)
CAST_TO_IMPL(CHUDBillboardEffect, kET_HUDBillboardEffect)
CAST_TO_IMPL(CUnknown27, 27)
CAST_TO_IMPL(CGameLight, kET_GameLight)
CAST_TO_IMPL(CFishCloud, kET_FishCloud)
CAST_TO_IMPL(CUnknown24, 24)
// id 23 is CFirstPersonCamera: its (CEntity&) cast is already named in symbols.txt as
// CCameraManager::CastGameCameratoFirstPersonCamera, the name CPlayerState calls it by, so that
// one keeps its name.
template <>
CFirstPersonCamera* TCastToPtr< CFirstPersonCamera >(CEntity* entity) {
  return static_cast< CFirstPersonCamera* >(TryCast(entity, kET_FirstPersonCamera));
}

const CGameCamera* CCameraManager::CastGameCameratoFirstPersonCamera(const CGameCamera* camera) {
  return static_cast< const CFirstPersonCamera* >(camera->TypesMatch(kET_FirstPersonCamera));
}

CAST_TO_IMPL(CExplosion, kET_Explosion)
CAST_TO_IMPL(CScattershotProjectile, kET_ScattershotProjectile)
CAST_TO_IMPL(CUnknown20, 20)
CAST_TO_IMPL(CEnergyProjectile, kET_EnergyProjectile)
CAST_TO_IMPL(CCollisionActor, kET_CollisionActor)
CAST_TO_IMPL(CCinematicCamera, kET_CinematicCamera)
CAST_TO_IMPL(CBouncyGrenade, kET_BouncyGrenade)
CAST_TO_IMPL(CBouncingBomb, kET_BouncingBomb)
CAST_TO_IMPL(CBomb, kET_Bomb)
CAST_TO_IMPL(CUnknown13, 13)
CAST_TO_IMPL(CScriptSequenceTimer, kET_ScriptSequenceTimer)
CAST_TO_IMPL(CScriptGuiWidget, kET_ScriptGuiWidget)
CAST_TO_IMPL(CUnknown10, 10)
CAST_TO_IMPL(CScriptWaypoint, kET_ScriptWaypoint)
CAST_TO_IMPL(CGameProjectile, kET_GameProjectile)
CAST_TO_IMPL(CEffect, kET_Effect)
CAST_TO_IMPL(CWeapon, kET_Weapon)
CAST_TO_IMPL(CGameCamera, kET_GameCamera)
CAST_TO_IMPL(CEntity, kET_Entity)

#undef CAST_TO_IMPL_INCOMPLETE
#undef CAST_TO_IMPL

// Destructors retail places after TryCast, in its order. Those of ids 76, 63 and 50 are left out
// with their classes, and so are the member helpers that follow them, whose types are unnamed.
CScriptTargetingPoint::~CScriptTargetingPoint() {}
CScriptPortalTransition::~CScriptPortalTransition() {}
CScriptGuiScreen::~CScriptGuiScreen() {}
CScriptDamageableTrigger::~CScriptDamageableTrigger() {}
CScriptCoverPoint::~CScriptCoverPoint() {}
CScriptAiJumpPoint::~CScriptAiJumpPoint() {}
CGameLight::~CGameLight() {}
CEnergyProjectile::~CEnergyProjectile() {}
CCollisionActor::~CCollisionActor() {}
