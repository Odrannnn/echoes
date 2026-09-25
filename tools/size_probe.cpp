// Temporary layout probe. Sizes are written into .data so they can be read back with
// `objdump -s`; the host compiler cannot be used for this because rstl::string and friends
// are 64-bit there and 32-bit under mwcceppc.
#include "Collision/CCollidableAABox.hpp"
#include "Collision/CCollisionPrimitive.hpp"
#include "Kyoto/Animation/CCharacterInfo.hpp"
#include "Kyoto/CDependencyGroupToken.hpp"
#include "Kyoto/CObjectReference.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CGenDescription.hpp"
#include "Kyoto/Particles/CParticleElectric.hpp"
#include "Kyoto/Text/CRasterFont.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CAdditiveAnimPlayback.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CGuiSys/CGuiSys.hpp"
#include "MetroidPrime/CHierarchyPoseBuilder.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CParticleDatabase.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/CRuleSet.hpp"
#include "MetroidPrime/CWorldState.hpp"
#include "MetroidPrime/Enemies/CAi.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/ScriptObjects/CScriptAreaProperties.hpp"
#include "MetroidPrime/ScriptObjects/CScriptHUDMemo.hpp"
#include "MetroidPrime/Weapons/CGunWeapon.hpp"
#include "MetroidPrime/Weapons/CPowerBeam.hpp"
#include "MetroidPrime/Weapons/GunController/CGunMotion.hpp"
#include "MetroidPrime/CArchitectureMessage.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "Kyoto/CVParamTransfer.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/string.hpp"

#define S(T) (sizeof(T))
unsigned int g_sizes[] = {
    S(CCollidableAABox),
    S(CCollisionPrimitive),
    S(CCharacterInfo),
    S(CDependencyGroupToken),
    S(CObjectReference),
    S(CPakFile),
    S(CResFactory),
    S(CSimplePool),
    S(CElementGen),
    S(CGenDescription),
    S(CParticleElectric),
    S(CRasterFont),
    S(CActor),
    S(CLightParameters),
    S(CScannableParameters),
    S(CVisorParameters),
    S(CActorParameters),
    S(CAdditiveAnimPlayback),
    S(CAnimRes),
    S(CGameArchitectureSupport),
    S(CGuiSys),
    S(unk_TSegIdMap),
    S(CLayoutDescription),
    S(CHierarchyPoseBuilder),
    S(CIOWinManager),
    S(CAdvancementDeltas),
    S(CModelData),
    S(CParticleDatabase),
    S(CMotionState),
    S(CPhysicsActor),
    S(CRuleValue),
    S(CRuleCondition),
    S(CRuleAction),
    S(CRuleSetRule),
    S(CRuleSet),
    S(CWorldState),
    S(CAi),
    S(CPatterned),
    S(CGameOptions),
    S(CGameState),
    S(CPlayer),
    S(CPlayerGun),
    S(CScriptAreaProperties),
    S(CScriptHUDMemo),
    S(CGunWeapon),
    S(CPowerBeam),
    S(CGunMotion),
    // Not CHECK_SIZEOF'd but load-bearing for the rc_ptr work.
    S(CArchitectureMessage),
    S(CVParamTransfer),
    S(rstl::rc_ptr< CIOWin >),
    S(rstl::ncrc_ptr< CIOWin >),
    S(CIOWinManager::IOWinPQNode),
    S(rstl::rc_ptr< IArchitectureMessageParm >),
    S(rstl::string),
};
unsigned int g_n = sizeof(g_sizes) / sizeof(g_sizes[0]);
