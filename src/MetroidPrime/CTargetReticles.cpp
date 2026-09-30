#include "MetroidPrime/CTargetReticles.hpp"

#include "Kyoto/Basics/CCast.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakTargeting.hpp"
#include "rstl/math.hpp"
#include <math.h>
#include <stdio.h>

static bool IsDamageOrbit(CPlayer::EPlayerOrbitRequest request) {
  // TODO: recover the Echoes orbit-request enumerators used by damage/lock-break transitions.
  return false;
}

static float offshoot_func(float amplitude, float angularScale, float time) {
  return amplitude * CMath::FastSinR((time - 0.5f) * angularScale) + 0.5f;
}

static float calculate_premultiplied_overshoot_offset(float overshoot) {
  return 2.f * (M_PIF - static_cast< float >(asin(1.f / overshoot)));
}

CCompoundTargetReticle::SOuterItemInfo::SOuterItemInfo(const char* modelName)
: mModel(gpSimplePool->GetObj(modelName))
, mOffshootBaseAngle(0.f)
, mRotationAngle(0.f)
, mBaseAngle(0.f)
, mOffshootAngleDelta(0.f) {}

CCompoundTargetReticle::CCompoundTargetReticle(const CStateManager& mgr, int playerIndex)
: mPlayerIndex(playerIndex)
, mLeadingOrientation(CQuaternion::NoRotation())
, mLaggingOrientation(CQuaternion::NoRotation())
, mPreviousState(kRS_Unspecified)
, mNextState(kRS_Unspecified)
, mNoDrawTicks(0)
, mOvershootOffsetHalf(0.f)
, mPremultipliedOvershootOffset(0.f)
, mCrosshairs(gpSimplePool->GetObj("CMDL_Crosshairs"))
, mSeeker(gpSimplePool->GetObj("CMDL_Seeker"))
, mTargetFlower(gpSimplePool->GetObj("CMDL_TargetFlower"))
, mMissileBracket(gpSimplePool->GetObj("CMDL_MissileBracket"))
, mInnerBeamIcon(gpSimplePool->GetObj("CMDL_InnerBeamIcon"))
, mLockConfirm(gpSimplePool->GetObj("CMDL_LockConfirm"))
, mLockFire(gpSimplePool->GetObj("CMDL_LockFire"))
, mLockDagger(gpSimplePool->GetObj("CMDL_LockDagger0"))
, mGrapple(gpSimplePool->GetObj("CMDL_Grapple"))
, mChargeTickFirst(gpSimplePool->GetObj("CMDL_ChargeTickFirst"))
, mScanTargetCenter(gpSimplePool->GetObj("CMDL_ScanTargetCenter"))
, mScanTargetLeft(gpSimplePool->GetObj("CMDL_ScanTargetLeft"))
, mScanTargetRight(gpSimplePool->GetObj("CMDL_ScanTargetRight"))
, mChargeGauge("CMDL_ChargeGauge")
, mQuarterCurve(gpSimplePool->GetObj("TXTR_QuaterCurve"))
, mSeekerMissileLockConfirm(gpSimplePool->GetObj("CMDL_SeekerMissileLockConfirm"))
, mSeekerMissileCrosshair(gpSimplePool->GetObj("CMDL_SeekerMissileCrosshair"))
, mRadarPaintFirst(gpSimplePool->GetObj("TXTR_RadarPaint"))
, mRadarPaintSecond(gpSimplePool->GetObj("TXTR_RadarPaint"))
, mTargetId(kInvalidUniqueId)
, mNextTargetId(kInvalidUniqueId)
, mTargetPosition(CVector3f::Zero())
, mLaggingTargetPosition(CVector3f::Zero())
, mCurrentGroupInterpolated(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupA(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupB(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupDuration(0.f)
, mCurrentGroupTimer(0.f)
, mNextGroupInterpolated(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupA(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupB(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupDuration(0.f)
, mNextGroupTimer(0.f)
, mGrapplePointA(kInvalidUniqueId)
, mGrapplePointB(kInvalidUniqueId)
, mGrapplePointFactorA(0.f)
, mGrapplePointFactorB(0.f)
, mVulnerabilityTarget(kInvalidUniqueId)
, mTargetVulnerability(CDamageVulnerability::ImmuneVulnerabilty())
, mCrosshairsScale(0.f)
, mSeekerAngle(0.f)
, mCrosshairsDrawScale(0.f)
, x274(0.f)
, x278(0.f)
, mMissileActive(false)
, mMissileBracketTimer(0.f)
, mMissileBracketScaleTimer(0.f)
, mBeam(CPlayerState::kBI_Power)
, mChargeGaugeOvershootTimer(0.f)
, mLockOnTimer(0.f)
, x294(0.f)
, mLockFireTimer(0.f)
, mFullChargeFadeTimer(0.f)
, mScanBracketFactor(0.f)
, mScanTargetFactor(0.f)
, mBeamShot(false)
, mMissileShot(false)
, mFullyCharged(false) {
  mOuterBeamIconSquares.reserve(9);
  for (int i = 0; i < 9; ++i) {
    char name[64];
    sprintf(name, "CMDL_BeamSquare%d", i);
    mOuterBeamIconSquares.push_back(SOuterItemInfo(name));
  }
  mCrosshairs.Lock();
  mQuarterCurve.Lock();
  mSeeker.Lock();
  mGrapple.Lock();
  mSeekerMissileLockConfirm.Lock();
  mSeekerMissileCrosshair.Lock();
  mRadarPaintFirst.Lock();
  mRadarPaintSecond.Lock();
  // TODO: initial camera orientations, targeting-tweak overshoot and orbit-zone positions.
}

bool CCompoundTargetReticle::CheckLoadComplete() { return true; }

EReticleState CCompoundTargetReticle::GetDesiredReticleState(const CStateManager& mgr) const {
  switch (mgr.GetPlayerState(mPlayerIndex)->GetCurrentVisor()) {
  case CPlayerState::kPV_Scan:
    return kRS_Scan;
  case CPlayerState::kPV_Echo:
    return kRS_Echo;
  case CPlayerState::kPV_Combat:
    return kRS_Combat;
  case CPlayerState::kPV_Dark:
    return kRS_Dark;
  default:
    return kRS_Combat;
  }
}

void CCompoundTargetReticle::Update(float dt, const CStateManager& mgr) {
  // TODO: orientation lag, visor state transitions, token locks and group updates.
}

void CCompoundTargetReticle::UpdateCurrLockOnGroup(float dt, const CStateManager& mgr) {
  // TODO: lock-on transitions, vulnerability, beam/missile and charge state.
}

void CCompoundTargetReticle::UpdateNextLockOnGroup(float dt, const CStateManager& mgr) {
  // TODO: next-target transition and interpolation.
}

void CCompoundTargetReticle::UpdateOrbitZoneGroup(float dt, const CStateManager& mgr) {
  if (mTargetId == kInvalidUniqueId && mNextTargetId != kInvalidUniqueId) {
    x294 = rstl::min_val(2.f * dt + x294, 1.f);
  } else {
    x294 = rstl::max_val(x294 - 2.f * dt, 0.f);
  }

  const CPlayer* player = mgr.GetPlayer(mPlayerIndex);
  if (player->IsCrosshairsOpen() &&
      player->GetPlayerState()->GetCurrentVisor() != CPlayerState::kPV_Scan) {
    // Retail's crosshairs fade scale lives at +0x270, which this header currently calls
    // mCrosshairsDrawScale; +0x268 (mCrosshairsScale) is only ever zeroed by the ctor.
    mCrosshairsDrawScale =
        rstl::min_val(mCrosshairsDrawScale + dt / gpTweakTargeting->GetCrosshairsFadeInOutTime(), 1.f);
  } else {
    mCrosshairsDrawScale =
        rstl::max_val(mCrosshairsDrawScale - dt / gpTweakTargeting->GetCrosshairsFadeInOutTime(), 0.f);
  }
}

void CCompoundTargetReticle::Draw(const CStateManager& mgr, bool hideLockOn) const {
  if (mgr.GetPlayer(mPlayerIndex)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed &&
      !mgr.GetCameraManager(mPlayerIndex)->IsInCinematicCamera()) {
    CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
    CGraphics::SetViewPointMatrix(camXf);
    CMatrix3f rot = camXf.BuildMatrix3f();

    CGraphics::SetCullMode(kCM_None);

    if (!hideLockOn) {
      DrawCurrLockOnGroup(rot, mgr);
      DrawSeeker(rot, mgr);
      DrawCrosshairs(rot);
      DrawScanTargetGroup(rot, mgr);
      DrawNextLockOnGroup(rot, mgr);
      DrawOrbitZoneGroup(rot, mgr);
    }

    DrawGrappleGroup(rot, mgr, hideLockOn);

    CGraphics::SetCullMode(kCM_Front);
  }

  if (mNoDrawTicks > 0) {
    --mNoDrawTicks;
  }
}

void CCompoundTargetReticle::DrawGrappleGroup(const CMatrix3f& rotation, const CStateManager& mgr,
                                              bool hideLockOn) const {
  // TODO: interpolate and draw grapple candidates.
}

void CCompoundTargetReticle::DrawGrapplePoint(const CScriptGrapplePoint& point, float factor,
                                              const CStateManager& mgr, const CMatrix3f& rotation,
                                              bool zEqual) const {
  // TODO: grapple model position, scale, color and depth mode.
}

void CCompoundTargetReticle::DrawCurrLockOnGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: current-target, charge/missile, seeker-missile and radar-paint rendering.
}

void CCompoundTargetReticle::DrawSeeker(const CMatrix3f& rotation, const CStateManager& mgr) const {
  // TODO: scan/next-target seeker rendering.
}

void CCompoundTargetReticle::DrawCrosshairs(const CMatrix3f& rotation) const {
  // TODO: crosshair scale, alpha and model rendering.
}

void CCompoundTargetReticle::DrawScanTargetGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: scan target center and left/right completion brackets.
}

void CCompoundTargetReticle::DrawNextLockOnGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: next-target lock-on rendering.
}

void CCompoundTargetReticle::DrawOrbitZoneGroup(const CMatrix3f& rotation,
                                                const CStateManager& mgr) const {
  // TODO: orbit-zone group rendering.
}

void CCompoundTargetReticle::UpdateTargetParameters(CTargetReticleRenderState& state,
                                                    const CStateManager& mgr) {
  if (const CActor* actor = TCastToConstPtr< CActor >(
          mgr.GetObjectListById(kOL_All).GetObjectById(state.GetTargetId()))) {
    state.SetRadiusWorld(CalculateRadiusWorld(*actor, mgr));
    CVector3f pos = CalculatePositionWorld(*actor, mgr);
    state.SetTargetPositionWorld(pos);
  } else if (state.GetIsOrbitZoneIdlePosition()) {
    state.SetRadiusWorld(1.f);
    state.SetTargetPositionWorld((mPreviousState == kRS_Echo || mPreviousState == kRS_Dark)
                                     ? mLaggingTargetPosition
                                     : mTargetPosition);
  }
}

float CCompoundTargetReticle::CalculateRadiusWorld(const CActor& actor,
                                                   const CStateManager& mgr) const {
  // TODO: derive reticle radius from the actor's bounds.
  return 1.f;
}

CVector3f CCompoundTargetReticle::CalculatePositionWorld(const CActor& actor,
                                                         const CStateManager& mgr) const {
  return mPreviousState == kRS_Scan ? actor.GetOrbitPosition(mgr)
                                    : actor.GetAimPosition(mgr, 0.f);
}

CVector3f CCompoundTargetReticle::CalculateOrbitZoneReticlePosition(const CStateManager& mgr,
                                                                    bool lag) const {
  const CGameCamera* cam = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true);
  float halfExtY = CCast::LtoF(
      mgr.GetPlayer(mPlayerIndex)->GetTweakPlayer()->GetOrbitZoneHeight(CPlayer::kZI_Targeting));
  float dist = 224.f / halfExtY;
  dist /= static_cast< float >(tan(cam->GetFov() * 0.5f * (1.f / 360.f) * (2.f * M_PIF)));

  CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
  CVector3f fwd = camXf.GetForward();

  if (lag) {
    fwd = mLaggingOrientation.Transform(fwd);
  }

  return camXf.GetTranslation() + dist * fwd;
}

bool CCompoundTargetReticle::IsGrappleTarget(TUniqueId id, const CStateManager& mgr) {
  return TCastToConstPtr< CScriptGrapplePoint >(mgr.GetObjectById(id)) != nullptr;
}

float CCompoundTargetReticle::CalculateClampedScale(CVector3f position, float scale, float clampMin,
                                                    float clampMax, const CStateManager& mgr,
                                                    int playerIndex) {
  // TODO: player viewport scaling and perspective-size clamp.
  return scale;
}

CTargetReticleRenderState::CTargetReticleRenderState(TUniqueId target, float radius,
                                                     CVector3f position, float factor,
                                                     float minimumViewportScale,
                                                     bool orbitZoneIdlePosition)
: mTarget(target)
, mRadius(radius)
, mPosition(position)
, mFactor(factor)
, mMinimumViewportScale(minimumViewportScale)
, mOrbitZoneIdlePosition(orbitZoneIdlePosition) {}

void CTargetReticleRenderState::InterpolateWithClamp(const CTargetReticleRenderState& a,
                                                     CTargetReticleRenderState& out,
                                                     const CTargetReticleRenderState& b, float t) {
  float t2 = CMath::Clamp(0.f, t, 1.f);
  float omt = 1.f - t2;
  out.mRadius = omt * a.mRadius + t2 * b.mRadius;
  out.mFactor = omt * a.mFactor + t2 * b.mFactor;
  out.mMinimumViewportScale = omt * a.mMinimumViewportScale + t2 * b.mMinimumViewportScale;
  out.mPosition = CVector3f::Lerp(a.mPosition, b.mPosition, t2);
  if (t2 == 1.f) {
    out.SetTargetId(b.GetTargetId());
  } else if (t2 == 0.f) {
    out.SetTargetId(a.GetTargetId());
  } else {
    out.SetTargetId(kInvalidUniqueId);
  }
}

CTargetingManager::CTargetingManager(const CStateManager& mgr, int playerIndex)
: mPlayerIndex(playerIndex), mTargetReticle(mgr, playerIndex), mOrbitPointMarker(playerIndex) {}

bool CTargetingManager::CheckLoadComplete() {
  return mTargetReticle.CheckLoadComplete() && mOrbitPointMarker.CheckLoadComplete();
}

void CTargetingManager::Update(float dt, const CStateManager& mgr) {
  mTargetReticle.Update(dt, mgr);
  mOrbitPointMarker.Update(dt, mgr);
}

void CTargetingManager::Draw(const CStateManager& mgr, bool hideLockOn) const {
  // TODO: establish ambient lighting, view and perspective before drawing.
  mOrbitPointMarker.Draw(mgr);
  mTargetReticle.Draw(mgr, hideLockOn);
}

void CCompoundTargetReticle::Touch() const {
  if (mCrosshairs.GetObject()) {
    mCrosshairs.GetObject()->Touch(0);
  }
  if (mSeeker.GetObject()) {
    mSeeker.GetObject()->Touch(0);
  }
  if (mGrapple.GetObject()) {
    mGrapple.GetObject()->Touch(0);
  }
  if (mSeekerMissileLockConfirm.GetObject()) {
    mSeekerMissileLockConfirm.GetObject()->Touch(0);
  }
  if (mSeekerMissileCrosshair.GetObject()) {
    mSeekerMissileCrosshair.GetObject()->Touch(0);
  }
  if (mTargetFlower.GetObject()) {
    mTargetFlower.GetObject()->Touch(0);
  }
  if (mMissileBracket.GetObject()) {
    mMissileBracket.GetObject()->Touch(0);
  }
  if (mInnerBeamIcon.GetObject()) {
    mInnerBeamIcon.GetObject()->Touch(0);
  }
  if (mLockFire.GetObject()) {
    mLockFire.GetObject()->Touch(0);
  }
  if (mLockDagger.GetObject()) {
    mLockDagger.GetObject()->Touch(0);
  }
  if (mGrapple.GetObject()) {
    mGrapple.GetObject()->Touch(0);
  }
  if (mChargeTickFirst.GetObject()) {
    mChargeTickFirst.GetObject()->Touch(0);
  }
  if (mChargeGauge.mModel.GetObject()) {
    mChargeGauge.mModel.GetObject()->Touch(0);
  }
  if (mScanTargetCenter.GetObject()) {
    mScanTargetCenter.GetObject()->Touch(0);
  }
  if (mScanTargetLeft.GetObject()) {
    mScanTargetLeft.GetObject()->Touch(0);
  }
  if (mScanTargetRight.GetObject()) {
    mScanTargetRight.GetObject()->Touch(0);
  }
  for (rstl::vector< CCompoundTargetReticle::SOuterItemInfo >::const_iterator it =
           mOuterBeamIconSquares.begin();
       it != mOuterBeamIconSquares.end(); ++it) {
    if (it->mModel.GetObject()) {
      it->mModel.GetObject()->Touch(0);
    }
  }
}

void CTargetingManager::Touch() const { mTargetReticle.Touch(); }

COrbitPointMarker::COrbitPointMarker(int playerIndex)
: mPlayerIndex(playerIndex)
, mZOffset(gpTweakTargeting->GetOrbitPointZOffset())
, mCameraRelativeZ(true)
, mLagAzimuth(0.f)
, mAzimuth(0.f)
, mLagTargetPosition(CVector3f::Zero())
, mLastFreeOrbit(false)
, mInterpolationTimer(0.f)
, mCurrentTime(0.f)
, mOrbitPointModel(gpSimplePool->GetObj("CMDL_OrbitPoint")) {
  mOrbitPointModel.Lock();
}

bool COrbitPointMarker::CheckLoadComplete() { return mOrbitPointModel.IsLoaded(); }

void COrbitPointMarker::Update(float dt, const CStateManager& mgr) {
  // TODO: free-orbit state, target lag, azimuth and interpolation.
}

void COrbitPointMarker::Draw(const CStateManager& mgr) const {
  // TODO: orbit-marker camera-relative projection and rendering.
}

void COrbitPointMarker::ResetInterpolationTimer(float time) { mInterpolationTimer = time; }
