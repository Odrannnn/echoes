#ifndef _CBEAMPROJECTILE
#define _CBEAMPROJECTILE

#include "MetroidPrime/Weapons/CGameProjectile.hpp"

class CBeamProjectile : public CGameProjectile {
public:
  enum EDamageType { kDT_None, kDT_Actor, kDT_World };

  CBeamProjectile(const TToken< CWeaponDescription >& description, const rstl::string& name,
                  EWeaponType type, const CTransform4f& xf, float maxLength, float beamRadius,
                  float travelSpeed, EMaterialTypes material, const CDamageInfo& damage,
                  TUniqueId uid, TAreaId areaId, TUniqueId owner, uint attribs, bool growingBeam);

  // CEntity
  // Declared here and defined in MetroidPrime/TypesMatch.cpp, because retail's symbol for it
  // (`__dt__15CBeamProjectileFv`, 0x800974C0) sits inside *that* unit's range and `~CPlasmaProjectile`
  // (0x800972BC) calls it there out of line. Defined in the class body, mwcceppc inlines the call and
  // `__dt__17CPlasmaProjectileFv` comes out 5 instructions long (95.66%); see
  // docs/goal-notes/match-typesmatch.md. Do not move the definition into CBeamProjectile.cpp: the
  // DOL link would then see a second definition of the symbol retail's TypesMatch object already has.
  ~CBeamProjectile() override;
  CEntity* TypesMatch(int typeId) const override;

  // CActor
  void PreRenderAllViewports(CStateManager& mgr) override;
  rstl::optional_object< CAABox > GetTouchBounds() const override;

  // CBeamProjectile
  virtual void UpdateFx(const CTransform4f& xf, float dt, CStateManager& mgr);
  virtual void ResetBeam(CStateManager& mgr, bool fullReset);
  virtual void Fire(const CTransform4f& xf, CStateManager& mgr, bool flag) = 0;

  void SetMaxLength(float length);
  // `const` on the by-value parameter is retail's spelling (Prime 1 has `const bool b`) and it is
  // load-bearing: mwcceppc allocates the caller's temporary differently without it, which is the
  // whole of the `li r3`/`li r4` difference in `CPlasmaProjectile::UpdateFx`.
  void CauseDamage(const bool damage) { mEnableTouchDamage = damage; }
  EDamageType GetDamageType() const { return mDamageType; }
  const CVector3f& GetCurrentPos() const { return mCollisionPoint; }
  const CVector3f& GetSurfaceNormal() const { return mCollisionNormal; }
  const CTransform4f& GetBeamTransform() const { return mXf; }
  float GetCurrentLength() const { return mBeamLength; }
  float GetMaxLength() const { return mMaxLength; }
  float GetInvMaxLength() const { return mInvMaxLength; }
  float GetMaxRadius() const { return mBeamRadius; }
  TUniqueId GetCollisionActorId() const { return mCollisionActorId; }
  const rstl::reserved_vector< CVector3f, 8 >& GetPointCache() const { return mPointCache; }
  rstl::reserved_vector< CVector3f, 8 >& PointCache() { return mPointCache; }

private:
  void SetCollisionResultData(EDamageType type, CRayCastResult& result, TUniqueId id);

  float mMaxLength;
  float mInvMaxLength;
  float mBeamRadius;
  EDamageType mDamageType;
  TUniqueId x428_;
  TUniqueId mCollisionActorId;
  float mGrowingBeamLength; // Guessed name: length before collision clipping.
  float mBeamLength;
  float mTravelSpeed;
  CVector3f mCollisionNormal;
  CVector3f mCollisionPoint;
  CTransform4f mXf;
  CAABox mLocalBounds; // Guessed name
  CAABox mWorldBounds; // Guessed name
  rstl::reserved_vector< CVector3f, 10 > x4b0_;
  rstl::reserved_vector< CVector3f, 8 > mPointCache;
  bool mGrowingBeam : 1;
  bool mEnableTouchDamage : 1;
};
CHECK_SIZEOF(CBeamProjectile, 0x598)

#endif // _CBEAMPROJECTILE
