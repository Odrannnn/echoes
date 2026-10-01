extern "C" const char lbl_803A64B0[];
extern "C" const float lbl_8041A7C4;
#define CMEMORY_NEW_FILE (lbl_803A64B0 + 20)
#include "MetroidPrime/Weapons/CGameProjectile.hpp"

#include "MetroidPrime/CGameLight.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/ScriptObjects/CScriptDock.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/math.hpp"

extern "C" const EMaterialTypes lbl_80417E54;

// Retail emits this as an unnamed global at 0x800361A4, so the project names it after the
// address; the body is byte-for-byte the same helper.
extern "C" CTransform4f fn_800361A4(const CTransform4f& xf) {
  CTransform4f result(xf);
  result.SetTranslation(CVector3f::Zero());
  return result;
}

CGameProjectile::CGameProjectile(bool active, const TToken< CWeaponDescription >& description,
                                 const rstl::string& name, EWeaponType weaponType,
                                 const CTransform4f& xf, EMaterialTypes excludeMaterial,
                                 const CDamageInfo& damageInfo, TUniqueId uid, TAreaId areaId,
                                 TUniqueId owner, TUniqueId homingTarget, uint attribs,
                                 bool underwater, const CVector3f& scale,
                                 const CImpactVisorEffect& visorEffect)
: CWeapon(uid, areaId, active, owner, weaponType, name, xf,
          CMaterialFilter::MakeIncludeExclude(
              CMaterialList(kMT_Unknown59, kMT_NonSolidDamageable),
              CMaterialList(kMT_Projectile, kMT_NoPlatformCollision, excludeMaterial)),
          CMaterialList(kMT_Projectile), damageInfo, attribs | GetBeamAttribType(weaponType),
           CModelData::CModelDataNull())
, mInitialTransform(xf)
, mVisorEffect(visorEffect)
, mProjectile(description, xf.GetTranslation(), fn_800361A4(xf), scale,
              (attribs & kPA_ParticleOPTS) ? 1 : 0)
, mPreviousPos(xf.GetTranslation())
, mProjExtent(HasAttrib(kPA_BigProjectile) ? 0.25f : 0.1f)
, mHomingDt(0.03f)
, mTargetHomingTime(0.0)
, mCurHomingTime(mHomingDt)
, mHomingTargetId(homingTarget)
, mLastResolvedObj(kInvalidUniqueId)
, mHitProjectileOwner(kInvalidUniqueId)
, mPendingDamagee(kInvalidUniqueId)
, mProjectileLight(kInvalidUniqueId)
, mWpscId(description.GetTag().GetId())
, mTouchedDock(kInvalidUniqueId)
, x404_(0)
, mMinHomingDist(0.f)
, mHomingTurnRateScale(1.f)
, mActive(true)
, mStartedUnderwater(underwater)
, mWaterUpdate(underwater)
, mInWater(underwater)
, x410_4_(false)
, mAppliedDamage(false)
, mAppliedDamageToPlayer(false)
, mMovingTowardTarget(false) {}

void CGameProjectile::StopProjectile(CStateManager& mgr) {
  DeleteProjectileLight(mgr);
  mgr.AddWeaponId(GetOwnerId(), GetType());
  mActive = false;
  MaterialList() = CMaterialList();
  mgr.UpdateActorInSortedLists(this);
}

void CGameProjectile::Render(const CStateManager& mgr) const {
  mProjectile.Render();
  CWeapon::Render(mgr);
}

CAABox CGameProjectile::GetProjectileBounds() const {
  const CVector3f translation = GetTranslation();
  return CAABox(rstl::min_val(mPreviousPos.GetX(), translation.GetX()) - mProjExtent,
                rstl::min_val(mPreviousPos.GetY(), translation.GetY()) - mProjExtent,
                rstl::min_val(mPreviousPos.GetZ(), translation.GetZ()) - mProjExtent,
                rstl::max_val(mPreviousPos.GetX(), translation.GetX()) + mProjExtent,
                rstl::max_val(mPreviousPos.GetY(), translation.GetY()) + mProjExtent,
                rstl::max_val(mPreviousPos.GetZ(), translation.GetZ()) + mProjExtent);
}

void CGameProjectile::Touch(CActor& actor, CStateManager& mgr) {
  CActor::Touch(actor, mgr);
  if (CScriptDock* dock = TCastToPtr< CScriptDock >(actor)) {
    if (dock->GetCurrentAreaId() == GetCurrentAreaId()) {
      mTouchedDock = actor.GetUniqueId();
    }
  }
}

rstl::optional_object< CAABox > CGameProjectile::GetTouchBounds() const {
  if (!mActive) {
    return rstl::optional_object_null();
  }
  return GetProjectileBounds();
}

CProjectileTouchResult CGameProjectile::CanCollideWithTrigger(CActor& actor, CStateManager& mgr) {
  const bool isWater = TCastToPtr< CScriptWater >(actor) != nullptr;
  if (isWater) {
    const bool enteredWater = (isWater && !IsInFluid()) &&
                              !mProjectile.GetWeaponDescription()->mEWTR;
    const bool leftWater = (!isWater && IsInFluid()) &&
                           !mProjectile.GetWeaponDescription()->mLWTR;
    const bool collide = enteredWater || leftWater;
    return CProjectileTouchResult(collide ? actor.GetUniqueId() : kInvalidUniqueId,
                                  rstl::optional_object_null());
  }
  return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
}

CProjectileTouchResult CGameProjectile::CanCollideWithGameObject(CActor& actor,
                                                                 CStateManager& mgr) {
  CGameProjectile* proj = TCastToPtr< CGameProjectile >(actor);
  if (!proj) {
    if (!actor.GetMaterialList().HasMaterial(kMT_Solid) && !actor.HealthInfo()) {
      return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
    } else if (actor.GetUniqueId() == GetOwnerId()) {
      return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
    } else if (actor.GetUniqueId() == mLastResolvedObj) {
      return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
    } else if (actor.GetMaterialList().SharesMaterials(GetFilter().GetExcludeList())) {
      return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
    } else if (CPatterned* ai = TCastToPtr< CPatterned >(actor)) {
      if (!ai->CanBeShot(mgr, GetAttribField())) {
        return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
      }
    }
  } else if (HasAttrib(kPA_PartialCharge) || proj->HasAttrib(kPA_PartialCharge)) {
    return CProjectileTouchResult(actor.GetUniqueId(), rstl::optional_object_null());
  } else if (!HasAttrib(kPA_PartialCharge) && !proj->HasAttrib(kPA_PartialCharge)) {
    return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
  }
  return CProjectileTouchResult(actor.GetUniqueId(), rstl::optional_object_null());
}

CProjectileTouchResult CGameProjectile::CanCollideWithComplexCollision(CActor& actor,
                                                                       CStateManager& mgr) {
  // TODO: cast against the actor's primitive, including embedded sphere handling.
  return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
}

// Guessed name
CProjectileTouchResult CGameProjectile::CanCollideWithDoor(CActor& actor, CStateManager& mgr) {
  // TODO: cast the movement segment against the door's oriented box.
  return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
}

CProjectileTouchResult CGameProjectile::CanCollideWith(CActor& actor, CStateManager& mgr) {
  // TODO: vulnerability test and dispatch to the trigger, primitive, door or ordinary actor path.
  return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());
}

CRayCastResult
CGameProjectile::RayCollisionCheckWithWorld(TUniqueId& idOut, const CVector3f& start,
                                            const CVector3f& end, float magnitude,
                                            rstl::reserved_vector< TUniqueId, 1024 >& nearList,
                                            CStateManager& mgr, EStaticGeometryTest staticTest) {
  idOut = kInvalidUniqueId;
  mPendingDamagee = kInvalidUniqueId;
  // TODO: static geometry selection and nearest actor hit, including overlapping bounds.
  return CRayCastResult();
}

void CGameProjectile::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_XCRT:
    x404_ = mgr.GetRenderFrameIndex();
    break;
  case kSM_XDelete:
    DeleteProjectileLight(mgr);
    break;
  case kSM_XENF:
    if (mInWater != true) {
      mInWater = true;
      mWaterUpdate = true;
    }
    break;
  case kSM_XINF:
    if (!mWaterUpdate) {
      mWaterUpdate = true;
    }
    break;
  case kSM_XEXF:
    if (mWaterUpdate) {
      mWaterUpdate = false;
      mInWater = false;
    }
    break;
  default:
    break;
  }
}

void CGameProjectile::FluidFXThink(EFluidState state, CScriptWater& water, CStateManager& mgr) {
  if (mProjectile.GetWeaponDescription()->mSWTR) {
    CWeapon::FluidFXThink(state, water, mgr);
  }
}

void CGameProjectile::ApplyDamageToOneActor(CStateManager& mgr, const CDamageInfo& damageInfo,
                                            TUniqueId id, const CVector3f& direction) {
  // TODO: direct/radius damage, hit flags and the player unfreeze attribute.
}

void CGameProjectile::ApplyDamageToActors(CStateManager& mgr, const CDamageInfo& damageInfo) {
  const CVector3f forward = GetTransform().GetForward();
  if (mPendingDamagee != kInvalidUniqueId) {
    ApplyDamageToOneActor(mgr, damageInfo, mPendingDamagee, forward);
    mPendingDamagee = kInvalidUniqueId;
  }
}

CRayCastResult CGameProjectile::DoCollisionCheck(TUniqueId& idOut, CStateManager& mgr) {
  CRayCastResult result = CRayCastResult::MakeInvalid();
  if (mActive) {
    const CVector3f delta = GetTranslation() - mPreviousPos;
    rstl::reserved_vector< TUniqueId, 1024 > nearList;
    mgr.BuildNearList(nearList, GetProjectileBounds(),
                      CMaterialFilter(CMaterialList(0x00000000FFFFFFFF),
                                      CMaterialList(lbl_80417E54), CMaterialFilter::kFT_Exclude),
                      this);
    const EStaticGeometryTest staticTest =
        mgr.IsMultiplayer() ? kSGT_CollisionGeometry : kSGT_RenderGeometry;
    result = RayCollisionCheckWithWorld(idOut, mPreviousPos, GetTranslation(), delta.Magnitude(),
                                        nearList, mgr, staticTest);
  }
  return result;
}

void CGameProjectile::UpdateProjectileMovement(float dt, CStateManager& mgr) {
  float useDt = dt;
  if (mWaterUpdate) {
    useDt = 37.5f * (dt * dt);
  }
  mPreviousPos = GetTranslation();
  mProjectile.Update(useDt);
  SetTransform(mProjectile.GetTransform());
  SetTranslation(mProjectile.GetTranslation());
  UpdateHoming(dt, mgr);
  // TODO: cross touched docks and remove projectiles left in occluded areas.
}

void CGameProjectile::UpdateHoming(float dt, CStateManager& mgr) {
  if (mActive && mHomingTargetId != kInvalidUniqueId && mHomingDt > 0.f) {
    mTargetHomingTime += dt;
    while (mTargetHomingTime >= mCurHomingTime) {
      Chase(mHomingDt, mgr);
      mCurHomingTime += mHomingDt;
    }
  }
}

void CGameProjectile::Chase(float dt, CStateManager& mgr) {
  // TODO: target eligibility, aim-point selection and constrained homing rotation.
}

void CGameProjectile::CreateProjectileLight(const rstl::string& name, const CLight& light,
                                            CStateManager& mgr) {
  if (mgr.GetNumPlayers() >= 3u) {
    return;
  }
  DeleteProjectileLight(mgr);
  mProjectileLight = mgr.AllocateUniqueId();
  const CAssetId sourceId = mWpscId;
  mgr.AddObject(rs_new CGameLight(mProjectileLight, GetAreaIdForPersistence(), GetActive(), name,
                                  GetTransform(), GetUniqueId(), light, sourceId, 0, lbl_8041A7C4));
}

void CGameProjectile::DeleteProjectileLight(CStateManager& mgr) {
  if (mProjectileLight != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mProjectileLight);
    mProjectileLight = kInvalidUniqueId;
  }
}

CWeapon::EProjectileAttrib CGameProjectile::GetBeamAttribType(EWeaponType type) {
  switch (type) {
  case kWT_Phazon:
    return kPA_Phazon;
  case kWT_Dark:
    return kPA_Dark;
  case kWT_Light:
    return kPA_Light;
  case kWT_Annihilator:
    return kPA_Annihilator;
  default:
    return kPA_None;
  }
}

void CGameProjectile::ResolveCollisionWithActor(const CRayCastResult& result, CActor& actor,
                                                CStateManager& mgr) {
  // TODO: apply the per-player blur, low-pass, forced-visor and billboard impact effects.
}

CGameProjectile::~CGameProjectile() {}
