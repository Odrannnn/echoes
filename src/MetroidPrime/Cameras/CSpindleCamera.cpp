#include "MetroidPrime/Cameras/CSpindleCamera.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptCameraHint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp"
#include "MetroidPrime/TCastTo.hpp"

CSpindleCameraInterpolant::CSpindleCameraInterpolant(ESpindleInput input, const CMayaSpline& spline)
: mInput(input), mSpline(spline) {}

float CSpindleCameraInterpolant::InterpolateValue(float input) { return mSpline.EvaluateAt(input); }

CSpindleCameraParameters::CSpindleCameraParameters(
    uint flags, const CSpindleCameraInterpolant& angularSpeed,
    const CSpindleCameraInterpolant& linearSpeed, const CSpindleCameraInterpolant& motionRadius,
    const CSpindleCameraInterpolant& radialOffset,
    const CSpindleCameraInterpolant& desiredAngularOffset,
    const CSpindleCameraInterpolant& minAngularOffset,
    const CSpindleCameraInterpolant& maxAngularOffset,
    const CSpindleCameraInterpolant& lookAtAngularOffset,
    const CSpindleCameraInterpolant& lookAtZOffset, const CSpindleCameraInterpolant& zOffset,
    const CSpindleCameraInterpolant& angularConstraint,
    const CSpindleCameraInterpolant& angularDampening,
    const CSpindleCameraInterpolant& desiredAngularSpeed,
    const CSpindleCameraInterpolant& deactivateRadius,
    const CSpindleCameraInterpolant& constraintFlipAngle, const CSpindleCameraInterpolant& fov)
: mFlags(flags)
, mAngularSpeed(angularSpeed)
, mLinearSpeed(linearSpeed)
, mMotionRadius(motionRadius)
, mRadialOffset(radialOffset)
, mDesiredAngularOffset(desiredAngularOffset)
, mMinAngularOffset(minAngularOffset)
, mMaxAngularOffset(maxAngularOffset)
, mLookAtAngularOffset(lookAtAngularOffset)
, mLookAtZOffset(lookAtZOffset)
, mZOffset(zOffset)
, mAngularConstraint(angularConstraint)
, mAngularDampening(angularDampening)
, mDesiredAngularSpeed(desiredAngularSpeed)
, mDeactivateRadius(deactivateRadius)
, mConstraintFlipAngle(constraintFlipAngle)
, mFov(fov) {}

CSpindleCameraParameters::~CSpindleCameraParameters() {}

CSpindleCamera::CSpindleCamera(TUniqueId uid, const CTransform4f& xf, bool active, int index,
                               int controllerIdx)
: CGameCamera(uid, rstl::string_l("Spindle Camera"),
              CEntityInfo(kInvalidAreaId, NullConnectionList, active), xf,
              CCameraManager::GetDefaultThirdPersonVerticalFOV(),
              CCameraManager::GetDefaultFirstPersonNearClipDistance(),
              CCameraManager::GetDefaultFirstPersonFarClipDistance(),
              CCameraManager::GetDefaultAspectRatio(), kInvalidUniqueId, index, controllerIdx)
, mSpindleCameraId(kInvalidUniqueId)
, mInVars()
, mMaxAzimuthInterpTimer(0.f)
, mLookDir(xf.GetForward())
, mTargetSplineDistance(0.f)
, mPlayerSplineDistance(0.f)
, mLookPosition(CVector3f::Zero())
, mOutsideClampedAzimuth(false)
, mInResetThink(false)
, mFixedPositionInitialized(false) {}

CSpindleCamera::~CSpindleCamera() {}

void CSpindleCamera::Reset(const CTransform4f& xf, CStateManager& mgr) {
  // Retail resolves and casts the active hint before it tests the flag, and tests the two guards
  // separately. See docs/goal-notes/progress-unit-cspindlecamera.md: this spelling reproduces
  // retail's block layout exactly, but mwcc materialises the flag with `extrwi.` where retail
  // tests the bit in place with `rlwinm.`.
  CScriptCameraHint* hint =
      TCastToPtr< CScriptCameraHint >(fn_801B9480(CameraManager(mgr).HintManager(), mgr));
  if (GetActive()) {
    if (hint) {
      mInResetThink = true;
      CameraManager(mgr).BallCamera()->UpdateLookAtPosition(0.01f, mgr, false);
      Think(0.01f, mgr);
      mInResetThink = false;
      mFixedPositionInitialized = false;
    }
  }
}

float CSpindleCamera::CalculateTargetSplineDistance(CStateManager& mgr) const {
  const CScriptSpindleCamera* camera =
      TCastToConstPtr< CScriptSpindleCamera >(mgr.GetObjectById(mSpindleCameraId));
  if (!camera) {
    return 0.f;
  }

  CMotionSpline& targetSpline = camera->GetTargetSpline();
  if (targetSpline.GetControlPointCount() == 0u) {
    return 0.f;
  }

  if (camera->GetPlayerSpline().GetControlPointCount() == 0u) {
    // The ball position is passed straight from the temporary `GetBallPosition` returns into;
    // naming it makes mwcc copy it into a second stack slot first.
    return targetSpline.FindClosestLengthOnSpline(mTargetSplineDistance,
                                                  Player(mgr).GetBallPosition());
  }

  if (close_enough(camera->GetPlayerSpline().GetLength(), 0.f, 3.f)) {
    return 0.f;
  }
  const float progress =
      CMath::Clamp(0.f, mPlayerSplineDistance / camera->GetPlayerSpline().GetLength(), 1.f);
  return camera->GetTargetControlSpline().EvaluateAt(progress) * targetSpline.GetLength();
}

float CSpindleCamera::GetInVar(const CSpindleCameraInterpolant& interpolant) const {
  return mInVars[interpolant.GetInput()];
}

float CSpindleCamera::GetInterpolant(CSpindleCameraInterpolant& interpolant) const {
  return interpolant.InterpolateValue(GetInVar(interpolant));
}

void CSpindleCamera::Think(float dt, CStateManager& mgr) {
  // TODO: Populate the eight spline inputs, apply radial/angular constraints and update the camera.
}

void CSpindleCamera::ProcessInput(const CFinalInput& input, CStateManager& mgr) {}

void CSpindleCamera::Render(const CStateManager& mgr) const {}

CVector3f CSpindleCamera::GetScanObjectIndicatorPosition(const CStateManager& mgr) const {
  // TODO: Recover the ball-camera look target, spline projection and angular/vertical offsets.
  return mLookPosition;
}
