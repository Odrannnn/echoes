#ifndef _CENTITYINFO
#define _CENTITYINFO

#include "MetroidPrime/TGameTypes.hpp"

#include "dolphin/types.h"
#include "rstl/vector.hpp"

enum EEntityType {
  kET_Entity = 0,
  kET_Actor = 1,
  kET_PhysicsActor = 2,
  kET_Ai = 3,
  kET_Patterned = 4,
  kET_GameCamera = 5,
  kET_Weapon = 6,
  kET_Effect = 7,
  kET_GameProjectile = 8,
  kET_ScriptWaypoint = 9,
  kET_ScriptGuiWidget = 11,
  kET_ScriptSequenceTimer = 12,
  kET_Bomb = 14,
  kET_BouncingBomb = 15,
  kET_BouncyGrenade = 16,
  kET_CinematicCamera = 17,
  kET_CollisionActor = 18,
  kET_EnergyProjectile = 19,
  kET_ScattershotProjectile = 21,
  kET_Explosion = 22,
  kET_FirstPersonCamera = 23,
  kET_FishCloud = 25,
  kET_GameLight = 26,
  kET_HUDBillboardEffect = 28,
  kET_IngPuddle = 29,
  kET_IngSnatchingSwarm = 30,
  kET_Player = 32,
  kET_ScriptActor = 34,
  kET_ScriptActorKeyframe = 35,
  kET_ScriptAIHint = 37,
  kET_ScriptAiJumpPoint = 38,
  kET_ScriptAIWaypoint = 39,
  kET_ScriptCameraShaker = 41,
  kET_ScriptCamera = 44,
  kET_ScriptColorModulate = 45,
  kET_ScriptCounter = 47,
  kET_ScriptCoverPoint = 48,
  kET_ScriptDamageableTrigger = 49,
  kET_DarkSamusBattleStage = 51,
  kET_ScriptDestructibleBarrier = 53,
  kET_ScriptDock = 55,
  kET_ScriptDoor = 56,
  kET_ScriptDynamicLight = 57,
  kET_ScriptEffect = 58,
  kET_ScriptGrapplePoint = 59,
  kET_ScriptGuiMenu = 60,
  kET_ScriptGuiScreen = 61,
  kET_ScriptGuiSlider = 62,
  kET_ScriptLayerController = 64,
  kET_ScriptPickup = 66,
  kET_ScriptPlayerHint = 68,
  kET_ScriptPlayerProxy = 69,
  kET_ScriptPlatform = 70,
  kET_ScriptPortalTransition = 72,
  kET_Relay = 73,
  kET_ScriptRepulsor = 74,
  kET_ScriptRiftPortal = 75,
  kET_ScriptSound = 77,
  kET_ScriptSpawnPoint = 79,
  kET_ScriptSpecialFunction = 80,
  kET_ScriptStreamedMusic = 84,
  kET_ScriptSwitch = 86,
  kET_ScriptTargetingPoint = 87,
  kET_ScriptTeamAi = 88,
  kET_ScriptTextPane = 89,
  kET_ScriptTrigger = 92,
  kET_ScriptTriggerEllipsoid = 93,
  kET_ScriptTriggerOrientated = 94,
  kET_ScriptSafeZone = 95,
  kET_ScriptWater = 97,
  kET_ScriptWorldTeleporter = 98,
  kET_SnakeWeedSwarm = 99,
  kET_SwarmBasics = 102,
  kET_FlyerSwarm = 103,
  kET_WallCrawler = 104,
  kET_BacteriaSwarm = 105,
  kET_MetareeSwarm = 106,
  kET_IngBlobSwarm = 107,
  kET_PlantScarabSwarm = 108,
  kET_BeamProjectile = 109,
  kET_PlasmaProjectile = 110,
  kET_DarkSamus = 111,
  kET_DigitalGuardian = 112,
  kET_DigitalGuardianHead = 113,
  kET_ElitePirate = 114,
  kET_Grenchler = 115,
  kET_Ing = 116,
  kET_IngBoostBallGuardian = 117,
  kET_IngSpaceJumpGuardian = 118,
  kET_IngSpiderballGuardian = 119,
  kET_Lumite = 120,
  kET_Metaree = 121,
  kET_Metroid = 122,
  kET_BabyMetroid = 123,
  kET_Parasite = 124,
  kET_PillBug = 125,
  kET_Puffer = 126,
  kET_Rezbit = 127,
  kET_Ripper = 128,
  kET_SandBoss = 129,
  kET_Sandworm = 130,
  kET_SandwormEye = 131,
  kET_SpacePirate = 132,
  kET_SpankWeed = 133,
  kET_SplitterMainChassis = 134,
  kET_SplitterCommandModule = 135,
  kET_WispTentacle = 136,
  kET_ScriptPlayerTurret = 138,
  kET_GunTurretBase = 139,
  kET_GunTurretTop = 140,
  kET_Kralee = 141,
  kET_Glowbug = 142,
  kET_SporbBase = 143,
  kET_SporbNeedle = 144,
  kET_SporbTop = 145,
  kET_SporbProjectile = 146,
  kET_MinorIng = 147,
  kET_BoostBallGuardian = 148,
  kET_Blogg = 149,
  kET_WallWalker = 150,
  kET_Shredder = 151,
  kET_AIMannedTurret = 153,
  kET_StoneToad = 154,
  kET_ScriptFrontEndDataNetwork = 155,
  kET_PowerBomb = 156,
  kET_Krocuss = 157,
  kET_OctapedeSegment = 158,
  kET_PuddleSpore = 159,
  kET_ScriptForgottenObject = 160,
};

enum EScriptObjectState {
  kSS_Active = 0x41435456,
  kSS_Inactive = 0x49435456,
  kSS_Zero = 0x5a45524f,
  kSS_DefaultState = 0x44465354,
  kSS_MaxReached = 0x4d415852,
  kSS_ScanStart = 0x4553434e,
  kSS_ScanProcessing = 0x4253434e,
  kSS_ScanDone = 0x53434e44,
  kSS_Dead = 0x44454144,
  kSS_InvalidState = 0xffffffff,
};

enum EScriptObjectMessage {
  kSM_Start = 0x53545254,
  kSM_Stop = 0x53544f50,
  kSM_Play = 0x504c4159,
  kSM_Load = 0x4c4f4144,
  kSM_Activate = 0x41435456,
  kSM_Deactivate = 0x44435456,
  kSM_ToggleActive = 0x54435456,
  kSM_SetToZero = 0x5a45524f,
  kSM_Reset = 0x52534554,

  kSM_Increment = 0x494e4352,
  kSM_Decrement = 0x44454352,

  kSM_XCRT = 0x58435254,
  kSM_XALD = 0x58414c44,
  kSM_XDelete = 0x5844454c,

  kSM_None = 0xffffffff,
};

struct SConnection {
  EScriptObjectState state;
  EScriptObjectMessage msg;
  TEditorId objId;

  SConnection(EScriptObjectState state, EScriptObjectMessage msg, TEditorId id)
  : state(state), msg(msg), objId(id) {}
};

namespace rstl {
template <>
struct is_trivially_destructible< SConnection > {
  enum { value = true };
};

template <>
inline void construct< SConnection >(void* dest, const SConnection& src) {
  *static_cast< SConnection* >(dest) = src;
}
} // namespace rstl

class CEntityInfo {
  TAreaId areaId;
  rstl::vector< SConnection > conns;
  TEditorId editorId;
  uchar active : 1;
  uchar scriptingBlocked : 1;
  uchar unk : 1;

public:
  CEntityInfo(TAreaId aid, const rstl::vector< SConnection >& conns, bool active,
              TEditorId eid = kInvalidEditorId);
  CEntityInfo(const CEntityInfo&);
  ~CEntityInfo();

  TAreaId GetAreaId() const { return areaId; }
  const rstl::vector< SConnection >& GetConnectionList() const { return conns; }
  TEditorId GetEditorId() const { return editorId; }
  uchar GetActive() const { return active; }
  uchar GetScriptingBlocked() const { return scriptingBlocked; }
  uchar GetUnk() const { return unk; }
};

class CScriptMsg {
public:
  CScriptMsg()
  : m_unk(kInvalidUniqueId)
  , m_originator(kInvalidUniqueId)
  , m_id(kInvalidUniqueId)
  , m_msg(kSM_None)
  , m_state(kSS_InvalidState)
  {}

  CScriptMsg(TUniqueId unk, TUniqueId id, TUniqueId originator, EScriptObjectMessage msg, EScriptObjectState state)
  : m_unk(unk)
  , m_originator(originator)
  , m_id(id)
  , m_msg(msg)
  , m_state(state)
  {}

  TUniqueId GetUnk() const { return m_unk; }
  TUniqueId GetOriginator() const { return m_originator; }
  TUniqueId GetId() const { return m_id; }
  EScriptObjectMessage GetMessage() const { return m_msg; }
  EScriptObjectState GetState() const { return m_state; }

  void SetMessage(EScriptObjectMessage msg) {
    m_msg = msg;
  }

public:
  TUniqueId m_unk;
  TUniqueId m_originator;
  TUniqueId m_id;
  EScriptObjectMessage m_msg;
  EScriptObjectState m_state;
};

struct SLdrEditorProperties;
const CEntityInfo& LdrToEntityInfo(const CEntityInfo&, const SLdrEditorProperties&);

#endif // _CENTITYINFO
