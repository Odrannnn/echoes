#include "MetroidPrime/Enemies/CAi.hpp"

#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CFluidPlaneManager.hpp"
#include "MetroidPrime/CSimpleShadow.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"

#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/TToken.hpp"

// The seventh float constant FluidFXThink needs already exists in retail's small-data pool at
// 0x8041AD50 - an earlier translation unit emitted it and the linker shared it - so retail's CAi
// references that address rather than carrying a copy. Declared extern here for the same reason:
// a local copy makes this object four bytes larger than the .sdata2 range CAi claims, which
// shifts every address above it (and every REL import table) by eight.
extern const float kCAiSplashDenom;

// Echoes' CActorLights keeps its cast-shadows flag at 0x2a0, second bit; include/ still has
// Prime 1's layout, where it is x298_25.
struct SActorLightsFlags {
  uchar x0_pad[0x2a0];
  bool x2a0_24 : 1;
  bool x2a0_25_castShadows : 1;
};
static inline void SetCastShadows(CActorLights* lights) {
  reinterpret_cast< SActorLightsFlags* >(lights)->x2a0_25_castShadows = true;
}

CAi::CAi(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, uint flags,
         const CTransform4f& xf, const CModelData& mData, const CAABox& bounds, float mass,
         const CHealthInfo& hInfo, const CDamageVulnerability& dVuln,
         const CMaterialList& matList, CAssetId stateMachine, CAssetId stateMachine2,
         const CActorParameters& actParams, float stepUp, float stepDown)
: CPhysicsActor(uid, name, info, flags | 8, xf, mData,
                matList | CMaterialList(kMT_AIBlock, kMT_CameraPassthrough),
                bounds, SMoverData(mass), actParams, StepData(stepUp, stepDown, 0))
, x2d0_healthInfo(hInfo)
, x2f0_damageVulnerability(dVuln)
, x320_stateMachine() {
  if (stateMachine != kInvalidAssetId) {
    x320_stateMachine = gpSimplePool->GetObj(SObjectTag('AFSM', stateMachine));
  } else {
    x320_stateMachine = gpSimplePool->GetObj(SObjectTag('FSM2', stateMachine2));
  }
  x320_stateMachine.data().Lock();

  CreateShadowIfNeeded();
  if (GetShadow()) {
    CreateShadow(true);
    Shadow()->SetAlwaysCalculateRadius(false);
  }
  if (GetActorLights()) {
    SetCastShadows(ActorLights());
  }
}

CAi::~CAi() {}

CHealthInfo* CAi::HealthInfo(CStateManager&) { return &x2d0_healthInfo; }

const CDamageVulnerability* CAi::GetDamageVulnerability() const {
  return &x2f0_damageVulnerability;
}

CDamageVulnerability* CAi::DamageVulnerability() { return &x2f0_damageVulnerability; }

void CAi::TakeDamage(const CVector3f&, float) {}

void CAi::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_XALD:
    SetMaterialFilter(CMaterialFilter::MakeIncludeExclude(
        GetMaterialFilter().GetIncludeList() | CMaterialList(kMT_AIBlock),
        GetMaterialFilter().GetExcludeList()));
    break;
  }
  CActor::AcceptScriptMsg(mgr, msg);
}

void CAi::FluidFXThink(EFluidState state, CScriptWater& water, CStateManager& mgr) {
  switch (state) {
  case kFS_EnteredFluid:
  case kFS_LeftFluid: {
    const float dt = mgr.FluidPlaneManager()->GetLastSplashDeltaTime(GetUniqueId());
    if (dt >= 0.2f) {
      const float energy = 0.5f * GetMass() * GetVelocityWR().MagSquared();
      if (energy > 500.f) {
        const float clamped = energy < 30000.f ? energy : 30000.f;
        const CVector3f& translation = GetTranslation();
        CVector3f pos(translation.GetX(), translation.GetY(),
                      water.GetTriggerBoundsWR().GetMaxPoint().GetZ());
        mgr.FluidPlaneManager()->CreateSplash(GetUniqueId(), mgr, water, pos,
                                              0.1f + 0.4f * (clamped - 500.f) / kCAiSplashDenom, true);
      }
    }
    break;
  }
  default:
    break;
  }
}

// Trilogy names only the second (GetStateMachine2); the two bodies are identical.
// The token is copied through a non-template stand-in for TToken<CStateMachine>: a class template
// makes the compiler emit an out-of-line copy of its destructor into this translation unit, and
// retail's has none. TToken::GetT() is just GetObj()->GetContents() with a cast, and the
// stand-in's destructor chain is still CToken's, so the instruction sequence is unchanged.
struct CAiStateMachineToken : CToken {
  CAiStateMachineToken(const CToken& token) : CToken(token) {}
  CStateMachine* GetT() { return reinterpret_cast< CStateMachine* >(GetObj()->GetContents()); }
};

CStateMachine* CAi::GetStateMachine() {
  if (x320_stateMachine.data().IsLoaded()) {
    CAiStateMachineToken tok(x320_stateMachine.data());
    return tok.GetT();
  }
  return nullptr;
}

CStateMachine* CAi::GetStateMachine2() {
  if (x320_stateMachine.data().IsLoaded()) {
    CAiStateMachineToken tok(x320_stateMachine.data());
    return tok.GetT();
  }
  return nullptr;
}

EWeaponCollisionResponseTypes CAi::GetCollisionResponseType(const CVector3f&, const CVector3f&,
                                                            const CWeaponMode&, int) const {
  return kWCR_EnemyNormal;
}
