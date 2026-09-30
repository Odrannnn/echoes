#include "MetroidPrime/Weapons/CBeamProjectile.hpp"

#include "Collision/CMaterialFilter.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"

void fn_80049ED8(CActor*, CStateManager&);
extern "C" CDamageInfo fn_800B5FF0(const CDamageInfo&, float); // Retail time-scaled damage copy.
extern "C" EMaterialTypes lbl_80418360; // kMT_NoPlatformCollision (20) in retail.
extern "C" const float lbl_8041A7C0;          // 0.1f touch-bounds allowance.
extern "C" const float lbl_8041C024;          // Retail's 0.0f constant.

static inline void ApplyBeamWorldDamage(CBeamProjectile& beam, CStateManager& mgr,
                                        const CVector3f& point, const CDamageInfo& damage,
                                        const CMaterialFilter& filter) {
  const TUniqueId owner = beam.GetOwnerId();
  mgr.ApplyDamageToWorld(owner, beam, point, damage, filter);
}

CBeamProjectile::CBeamProjectile(const TToken< CWeaponDescription >& description,
                                 const rstl::string& name, EWeaponType type, const CTransform4f& xf,
                                 float maxLength, float beamRadius, float travelSpeed,
                                 EMaterialTypes material, const CDamageInfo& damage, TUniqueId uid,
                                 TAreaId areaId, TUniqueId owner, uint attribs, bool growingBeam)
: CGameProjectile(false, description, name, type, xf, material, damage, uid, areaId, owner,
                  kInvalidUniqueId, attribs, false, CVector3f(1.f, 1.f, 1.f), CImpactVisorEffect())
, mMaxLength(maxLength)
, mInvMaxLength(1.f / mMaxLength)
, mBeamRadius(beamRadius)
, mDamageType(kDT_None)
, x428_(kInvalidUniqueId)
, mCollisionActorId(kInvalidUniqueId)
, mGrowingBeamLength(growingBeam ? 0.f : mMaxLength)
, mBeamLength(mMaxLength)
, mTravelSpeed(travelSpeed)
, mCollisionNormal(CVector3f::Up())
, mCollisionPoint(CVector3f::Zero())
, mXf(CTransform4f::Identity())
, mLocalBounds(CAABox::Identity())
, mWorldBounds(CAABox::Identity())
, x4b0_(CVector3f::Zero())
, mPointCache(CVector3f::Zero())
, mGrowingBeam(growingBeam)
, mEnableTouchDamage(false) {}

rstl::optional_object< CAABox > CBeamProjectile::GetTouchBounds() const {
  if (!GetActive() || !mEnableTouchDamage) {
    return rstl::optional_object_null();
  }
  const CVector3f pos = GetTranslation();
  return CAABox(pos.GetX() - lbl_8041A7C0, pos.GetY() - lbl_8041A7C0,
                pos.GetZ() - lbl_8041A7C0, pos.GetX() + lbl_8041A7C0,
                pos.GetY() + lbl_8041A7C0, pos.GetZ() + lbl_8041A7C0);
}

void CBeamProjectile::PreRenderAllViewports(CStateManager& mgr) {
  // Bound by reference, not copied into a value: retail reads both inlined bounds copies straight
  // out of the GetTransformedAABox return slot, and a value local makes the compiler stage the box
  // through the stack as floats first (39.70%).
  const CAABox& bounds = mLocalBounds.GetTransformedAABox(mXf);
  SetOtherBounds(bounds);
  SetRenderBounds(bounds);
  fn_80049ED8(this, mgr);
}

void CBeamProjectile::ResetBeam(CStateManager&, bool) {
  if (mGrowingBeam) {
    mGrowingBeamLength = 0.f;
  }
}

void CBeamProjectile::SetCollisionResultData(EDamageType dType, CRayCastResult& res, TUniqueId id) {
  mDamageType = dType;
  mBeamLength = res.GetTime();
  mCollisionPoint = res.GetPoint();
  mCollisionNormal = res.GetPlane().GetNormal();
  mCollisionActorId = dType == kDT_Actor ? id : kInvalidUniqueId;
  SetTranslation(res.GetPoint());
}

void CBeamProjectile::UpdateFx(const CTransform4f& xf, float dt, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }
  SetTransform(xf.GetRotation());
  if (mGrowingBeam) {
    mGrowingBeamLength += mTravelSpeed * dt;
    if (mGrowingBeamLength > mMaxLength) {
      mGrowingBeamLength = mMaxLength;
    }
  }
  mBeamLength = mGrowingBeamLength;
  mDamageType = kDT_None;
  const CVector3f origin = xf.GetTranslation();
  const CVector3f beamEnd =
      xf.GetTranslation() + mGrowingBeamLength * xf.GetColumn(kDY).AsNormalized();
  mPreviousPos = origin;
  SetTranslation(beamEnd);

  mLocalBounds = CAABox(-mBeamRadius, lbl_8041C024, -mBeamRadius, mBeamRadius, mBeamLength,
                        mBeamRadius);
  mWorldBounds = CAABox(CVector3f(-mBeamRadius, lbl_8041C024, -mBeamRadius),
                        CVector3f(mBeamRadius, mGrowingBeamLength, mBeamRadius))
                     .GetTransformedAABox(xf);

  TUniqueId collideId = kInvalidUniqueId;
  rstl::reserved_vector< TUniqueId, 1024 > nearList;
  // Retail's exclude filter keeps the all-material include mask as well.
  mgr.BuildNearList(nearList, mWorldBounds,
                    CMaterialFilter(CMaterialList(0x00000000ffffffff),
                                    CMaterialList(lbl_80418360), CMaterialFilter::kFT_Exclude),
                    this);

  CRayCastResult res = RayCollisionCheckWithWorld(collideId, origin, beamEnd, mGrowingBeamLength,
                                                   nearList, mgr, kSGT_CollisionGeometry);
  if (TCastToPtr< CActor >(mgr.ObjectById(collideId))) {
    SetCollisionResultData(kDT_Actor, res, collideId);
    if (mEnableTouchDamage) {
      ApplyDamageToActors(mgr, fn_800B5FF0(mCurDamageInfo, dt));
    }
  } else if (res.IsValid()) {
    SetCollisionResultData(kDT_World, res, kInvalidUniqueId);
    if (mEnableTouchDamage) {
      ApplyBeamWorldDamage(*this, mgr, res.GetPoint(), fn_800B5FF0(mCurDamageInfo, dt), GetFilter());
    }
  } else {
    mCollisionPoint = xf * CVector3f(mBeamRadius, mBeamLength, mBeamRadius);
    SetTranslation(mCollisionPoint);
  }

  mXf = xf;
}

void CBeamProjectile::SetMaxLength(float length) {
  mMaxLength = length;
  mInvMaxLength = 1.f / mMaxLength;
  if (!mGrowingBeam) {
    mGrowingBeamLength = mMaxLength;
  }
}
