// Retail's CAi object calls the SMoverData constructor, the CHealthInfo and CDamageVulnerability
// copies and that destructor out of line, and its vtable's last four slots point into other
// units, so none of them is emitted here. This TU opts in to the out-of-line declarations; every
// other TU keeps the inline forms, which is what the units that do emit them need. Its .sdata
// split is 0x10 bytes, so it also opts out of CCharAnimTime's pooled header constants.
#define MP_RETAIL_OUT_OF_LINE_COPIES
#define CCHARANIMTIME_LOCAL_CONSTANTS
#include "MetroidPrime/Enemies/CAi.hpp"

#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CFluidPlaneManager.hpp"
#include "MetroidPrime/CSimpleShadow.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"

#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"

// The seventh float constant FluidFXThink needs already exists in retail's small-data pool at
// 0x8041AD50 - an earlier translation unit emitted it and the linker shared it - so retail's CAi
// references that address rather than carrying a copy. Declared extern here for the same reason:
// a local copy makes this object four bytes larger than the .sdata2 range CAi claims, which
// shifts every address above it (and every REL import table) by eight.
extern const float kCAiSplashDenom;

CAi::CAi(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, uint castFlags,
         const CTransform4f& xf, const CModelData& modelData, const CAABox& bounds, float mass,
         const CHealthInfo& health, const CDamageVulnerability& vulnerability,
         const CMaterialList& materials, CAssetId stateMachine, CAssetId stateMachine2,
         const CActorParameters& params, float stepUp, float stepDown)
: CPhysicsActor(uid, name, info, castFlags | 8, xf, modelData,
                materials | CMaterialList(kMT_AIBlock, kMT_CameraPassthrough), bounds,
                SMoverData(mass), params, StepData(stepUp, stepDown, 0))
, mHealthInfo(health)
, mDamageVulnerability(vulnerability)
, mStateMachine() {
  if (stateMachine != kInvalidAssetId) {
    mStateMachine = gpSimplePool->GetObj(SObjectTag('AFSM', stateMachine));
  } else {
    mStateMachine = gpSimplePool->GetObj(SObjectTag('FSM2', stateMachine2));
  }
  mStateMachine.data().Lock();

  AllocateShadow();
  if (HasShadow()) {
    SetDrawShadow(true);
    Shadow()->SetAlwaysCalculateRadius(false);
  }
  if (HasActorLights()) {
    ActorLights()->SetCastShadows(true);
  }
}

CAi::~CAi() {}

CHealthInfo* CAi::HealthInfo() { return &mHealthInfo; }

const CDamageVulnerability* CAi::GetDamageVulnerability() const { return &mDamageVulnerability; }

CDamageVulnerability* CAi::DamageVulnerability() { return &mDamageVulnerability; }

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
                                              0.1f + 0.4f * (clamped - 500.f) / kCAiSplashDenom,
                                              true);
      }
    }
    break;
  }
  default:
    break;
  }
}

// The token is copied through a non-template stand-in for TToken: a class template makes the
// compiler emit an out-of-line copy of its destructor into this translation unit, and retail's
// has none. TToken::GetT() is just GetObj()->GetContents() with a cast, and the stand-in's
// destructor chain is still CToken's, so the instruction sequence is unchanged.
struct CAiStateMachineToken : CToken {
  CAiStateMachineToken(const CToken& token) : CToken(token) {}
  void* GetT() { return GetObj()->GetContents(); }
};

CStateMachine* CAi::GetStateMachine() {
  if (mStateMachine.data().IsLoaded()) {
    CAiStateMachineToken tok(mStateMachine.data());
    return static_cast< CStateMachine* >(tok.GetT());
  }
  return nullptr;
}

CStateMachine2* CAi::GetStateMachine2() {
  if (mStateMachine.data().IsLoaded()) {
    CAiStateMachineToken tok(mStateMachine.data());
    return static_cast< CStateMachine2* >(tok.GetT());
  }
  return nullptr;
}

EWeaponCollisionResponseTypes CAi::GetCollisionResponseType(const CVector3f&, const CVector3f&,
                                                            const CWeaponMode&, int) const {
  return kWCR_EnemyNormal;
}
