#ifndef _CAI
#define _CAI

#include "types.h"

#include "MetroidPrime/CDamageVulnerability.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/SObjectTag.hpp"

#include "rstl/optional_object.hpp"

class CKnockBackInfo;
class CStateMachine;
class CTeamAiRole;

enum EListenNoiseType {
  kLNT_PlayerFire,
  kLNT_BombExplode,
  kLNT_ProjectileExplode,
};

// Echoes' CAi, read from the retail constructor at 0x80097024 and the vtable at 0x803B29E0.
// Prime 1's ~130 state and trigger virtuals are gone: Echoes' CAi adds eight slots after
// CPhysicsActor's, two of them pure. The whole translation unit (0x80096C94-0x800972BC, its vtable
// and its .sdata material constants) is outside config/G2ME01/splits.txt.
class CAi : public CPhysicsActor {
public:
  // Trilogy: __ct__3CAiF9TUniqueIdRC...basic_string...RC11CEntityInfoUiRC12CTransform4fRC10CModelData
  //          RC6CAABoxfRC11CHealthInfoRC20CDamageVulnerabilityRC13CMaterialListUiUi
  //          RC16CActorParametersff
  CAi(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, uint flags,
      const CTransform4f& xf, const CModelData& mData, const CAABox& bounds, float mass,
      const CHealthInfo& hInfo, const CDamageVulnerability& dVuln, const CMaterialList& matList,
      CAssetId stateMachine, CAssetId stateMachine2, const CActorParameters& actParams,
      float stepUp, float stepDown);

  // CEntity
  ~CAi() override;
  CEntity* TypesMatch(int typeId) const override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override;

  // CActor
  CHealthInfo* HealthInfo(CStateManager& mgr) override; // Trilogy: HealthInfo__3CAiFv
  const CDamageVulnerability* GetDamageVulnerability() const override;
  EWeaponCollisionResponseTypes GetCollisionResponseType(const CVector3f&, const CVector3f&,
                                                         const CWeaponMode&, int) const override;
  void FluidFXThink(EFluidState state, CScriptWater& water, CStateManager& mgr) override;

  // CAi, vtable slots 38-45
  virtual void Death(CStateManager& mgr, const CVector3f& direction,
                     EScriptObjectState state) = 0;
  virtual void KnockBack(CStateManager& mgr, const CKnockBackInfo& info) = 0;
  virtual CDamageVulnerability* DamageVulnerability();
  virtual void TakeDamage(const CVector3f& direction, float magnitude);
  // Retail does not define these in CAi's own translation unit: the retail vtable's last four
  // entries point at copies the linker kept in other units (0x800358D8, 0x80073CAC, 0x8003C59C,
  // 0x80073CB4). Declared out of line here for the same reason, and renamed in symbols.txt to
  // these names so our vtable entries land on the same addresses.
  virtual bool CanBeShot(const CStateManager&, int);
  virtual bool IsListening() const;
  virtual bool Listen(const CVector3f&, EListenNoiseType);
  virtual CVector3f GetOrigin(const CStateManager&, const CTeamAiRole&,
                              const CVector3f&) const;

  CStateMachine* GetStateMachine();
  CStateMachine* GetStateMachine2();

private:
  CHealthInfo x2d0_healthInfo;
  CDamageVulnerability x2f0_damageVulnerability;
  // Loaded as 'AFSM' when the first asset id is valid, else 'FSM2' from the second.
  rstl::optional_object< CToken > x320_stateMachine;
};
CHECK_SIZEOF(CAi, 0x330)

#endif // _CAI
