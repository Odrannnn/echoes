#include "MetroidPrime/Cameras/CPathCamera.hpp"

#include "Collision/CMaterialFilter.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptDoor.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPathCamera.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/math.hpp"

static const CMaterialList kLineOfSightIncludeList(kMT_Unknown59);
static const CMaterialList kLineOfSightExcludeList(kMT_NoPlatformCollision);
static const CMaterialFilter kLineOfSightFilter =
    CMaterialFilter::MakeIncludeExclude(kLineOfSightIncludeList, kLineOfSightExcludeList);

CPathCamera::CPathCamera(TUniqueId uid, const CTransform4f& xf, bool active, int index,
                         int controllerIdx)
: CGameCamera(uid, rstl::string_l("Path Camera"),
              CEntityInfo(kInvalidAreaId, NullConnectionList, active), xf,
              CCameraManager::GetDefaultThirdPersonVerticalFOV(),
              CCameraManager::GetDefaultFirstPersonNearClipDistance(),
              CCameraManager::GetDefaultFirstPersonFarClipDistance(),
              CCameraManager::GetDefaultAspectRatio(), kInvalidUniqueId, index, controllerIdx)
, mScriptCameraId(kInvalidUniqueId)
, mPositionDistance(0.f)
, mLookAtDistance(0.f)
, mPlayerDistance(0.f)
, mSpeed(0.f) {}

CPathCamera::~CPathCamera() {}

const CScriptPathCamera* CPathCamera::GetScriptCamera(const CStateManager& mgr) const {
  return TCastToConstPtr< CScriptPathCamera >(mgr.GetObjectById(mScriptCameraId));
}

void CPathCamera::Reset(const CTransform4f& xf, CStateManager& mgr) {
  // TODO: Select the initial side using the current camera and collision-clamped path;
  // the separate player-spline mode maps normalized progress through both control splines.
  if (const CScriptPathCamera* camera = GetScriptCamera(mgr)) {
    mSpeed = camera->GetSpeed();
  }
}

float CPathCamera::CalculateLookAtDistance(const CStateManager& mgr) const {
  const CScriptPathCamera* camera = GetScriptCamera(mgr);
  if (!camera) {
    return 0.f;
  }

  CScriptCameraSpline& spline = camera->GetSpline();
  if (close_enough(spline.GetLength(), 0.f) ||
      spline.GetLookAtSpline().GetControlPointCount() == 0u) {
    return 0.f;
  }

  const float progress = CMath::Clamp(0.f, mPlayerDistance / spline.GetLength(), 1.f);
  return spline.LookAtTimeSpline().EvaluateAt(progress) * spline.GetLookAtSpline().GetLength();
}

float CPathCamera::CalculatePositionDistance(float dt, const CStateManager& mgr) const {
  const CScriptPathCamera* camera = GetScriptCamera(mgr);
  if (!camera) {
    return 0.f;
  }

  CScriptCameraSpline& spline = camera->GetSpline();
  if (close_enough(spline.GetLength(), 0.f)) {
    return 0.f;
  }

  float extent = camera->GetDistance();
  if (camera->GetFlags() & 4) {
    // Retail materialises the zero into its register at the top of this block and overwrites it
    // only if the magnitude is safe, so `distance` is a local initialised to 0.f rather than the
    // arm of a conditional.
    float distance = 0.f;
    const CVector3f pathPosition = spline.GetPositionByLength(mPlayerDistance, GetTransform(), mgr);
    CVector3f toPlayer = Player(const_cast< CStateManager& >(mgr)).GetBallPosition() - pathPosition;
    toPlayer.SetZ(0.f);
    if (toPlayer.IsMagnitudeSafe()) {
      distance = toPlayer.Magnitude();
    }
    const float control = camera->GetPerpendicularDistanceControlSpline().EvaluateAt(distance);
    extent *= 1.f - CMath::Clamp(0.f, control, 1.f);
  }

  float newDistance;
  if (spline.GetPositionSpline().IsClosedLoop()) {
    const float positive = spline.ValidateLength(mPlayerDistance + extent);
    const float negative = spline.ValidateLength(mPlayerDistance - extent);
    const float distance = CMath::AbsF(mPositionDistance - mPlayerDistance);
    const float remaining = spline.GetLength() - distance;
    if (mPositionDistance > mPlayerDistance) {
      newDistance = distance <= remaining ? positive : negative;
    } else {
      newDistance = distance <= remaining ? negative : positive;
    }
  } else {
    // Retail branches rather than selecting the operand, and only calls ValidateLength on the
    // path it takes.
    if (mPositionDistance > mPlayerDistance) {
      newDistance = spline.ValidateLength(mPlayerDistance + extent);
    } else {
      newDistance = spline.ValidateLength(mPlayerDistance - extent);
    }
  }

  // Retail inverts this test: the damping path is the fallthrough and the early return is the
  // branch target, which is why the flag test sits at the end of the closed-loop block above.
  if (!(camera->GetFlags() & 1)) {
    float step;
    if (spline.GetPositionSpline().IsClosedLoop()) {
      const float distance = CMath::AbsF(newDistance - mPositionDistance);
      float nearest = distance;
      if (distance > spline.GetLength() - distance) {
        nearest = spline.GetLength() - distance;
      }
      step = CMath::Limit(nearest / camera->GetDampenDistance(), 1.f) * (mSpeed * dt);
      // Retail negates by multiplying by -1.f, and evaluates the wrapped-distance remainder once
      // for both arms of the sign test rather than once per arm.
      const float wrapped = CMath::AbsF(mPositionDistance - newDistance);
      const float remaining = spline.GetLength() - wrapped;
      if (mPositionDistance > newDistance) {
        if (!(wrapped > remaining)) {
          step = step * -1.f;
        }
      } else if (wrapped > remaining) {
        step = step * -1.f;
      }
    } else {
      const float limited = CMath::Limit(
          (newDistance - mPositionDistance) / camera->GetDampenDistance(), 1.f);
      step = limited * (mSpeed * dt);
    }
    return spline.ValidateLength(mPositionDistance + step);
  }
  return newDistance;
}

CVector3f CPathCamera::MoveAlongSpline(float dt, const CStateManager& mgr) {
  CVector3f ret = GetTranslation();
  const CVector3f playerPosition =
      Player(const_cast< CStateManager& >(mgr)).GetBallPosition();
  const CScriptPathCamera* camera = GetScriptCamera(mgr);
  if (!camera) {
    return ret;
  }

  CScriptCameraSpline& spline = camera->GetSpline();
  const CMotionSpline& playerSpline = camera->GetPlayerSpline();
  if (camera->GetSpeedControlSpline().GetKnotCount() != 0) {
    float progress = 0.f;
    if (playerSpline.GetControlPointCount() != 0u) {
      progress = CMath::Clamp(0.f, mPlayerDistance / playerSpline.GetLength(), 1.f);
    } else {
      if (spline.GetPositionSpline().GetControlPointCount() != 0u) {
        progress =
            CMath::Clamp(0.f, mPositionDistance / spline.GetPositionSpline().GetLength(), 1.f);
      }
      if (spline.GetLookAtSpline().GetControlPointCount() != 0u) {
        progress = CMath::Clamp(0.f, mLookAtDistance / camera->GetSpline().GetLookAtSpline().GetLength(),
                                1.f);
      }
    }
    mSpeed = camera->GetSpeedControlSpline().EvaluateAt(progress) * camera->GetSpeed();
  }

  if (playerSpline.GetControlPointCount() != 0u &&
      camera->GetSpeedControlSpline().GetKnotCount() == 0) {
    mPlayerDistance = playerSpline.FindClosestLengthOnSpline(mPlayerDistance, playerPosition);
    mPlayerDistance = playerSpline.ValidateLength(mPlayerDistance);
    const float progress = CMath::Clamp(0.f, mPlayerDistance / playerSpline.GetLength(), 1.f);
    if (spline.GetPositionSpline().GetControlPointCount() != 0u) {
      mPositionDistance =
          spline.PositionTimeSpline().EvaluateAt(progress) * spline.GetPositionSpline().GetLength();
    }
    if (spline.GetLookAtSpline().GetControlPointCount() != 0u) {
      mLookAtDistance =
          spline.LookAtTimeSpline().EvaluateAt(progress) * spline.GetLookAtSpline().GetLength();
    }
  } else {
    mPlayerDistance = spline.FindClosestLengthOnSpline(mPlayerDistance, playerPosition);
    mPositionDistance = CalculatePositionDistance(dt, mgr);
    mLookAtDistance = CalculateLookAtDistance(mgr);
  }
  ret = spline.GetPositionByLength(mPositionDistance, GetTransform(), mgr);
  return ret;
}

CTransform4f CPathCamera::AvoidDoorCollisions(const CTransform4f& xf, const CStateManager& mgr) {
  CTransform4f ret = xf;
  if (const CScriptDoor* door = TCastToConstPtr< CScriptDoor >(mgr.GetObjectById(
          GetCameraManager(mgr).GetBallCamera()->GetTooCloseActorId()))) {
    if (!door->IsOpen() &&
        GetCameraManager(mgr).GetBallCamera()->CheckDoorProximity(xf.GetTranslation(), mgr)) {
      const CScriptPathCamera* camera = GetScriptCamera(mgr);
      if (!camera) {
        return xf;
      }
      float distance = mPlayerDistance + camera->GetDistance();
      if (mPositionDistance > mPlayerDistance) {
        distance = mPlayerDistance - camera->GetDistance();
      }
      mPositionDistance = distance;
      ret.SetTranslation(camera->GetSpline().GetPositionByLength(distance, ret, mgr));
    }
  }
  return ret;
}

void CPathCamera::Think(float dt, CStateManager& mgr) {
  // TODO: Apply movement, look target, door avoidance and orientation damping; update the
  // linked time keyframe only for the selected camera, then validate the transform and Think.
}

void CPathCamera::ProcessInput(const CFinalInput& input, CStateManager& mgr) {}

void CPathCamera::Render(const CStateManager& mgr) const {}

void CPathCamera::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CGameCamera::AcceptScriptMsg(mgr, msg);
}

CVector3f CPathCamera::GetScanObjectIndicatorPosition(const CStateManager& mgr) const {
  // TODO: Apply the path's look-at/perpendicular modes and camera-hint height overrides.
  return GetCameraManager(mgr).GetBallCamera()->GetScanObjectIndicatorPosition(mgr);
}

void CPathCamera::UpdateOrientation(float dt, const CTransform4f& xf, const CStateManager& mgr) {
  // TODO: Resolve the active camera hint, clamp angular speed near vertical directions,
  // and rotate towards xf with the shared quaternion look-at helper.
}

void CPathCamera::UpdateFov(const CStateManager& mgr) {
  const CScriptPathCamera* camera = GetScriptCamera(mgr);
  CScriptCameraSpline& spline = camera->GetSpline();
  if (camera->GetPlayerSpline().GetControlPointCount() != 0u) {
    const CMotionSpline& playerSpline = camera->GetPlayerSpline();
    const float time = playerSpline.GetDuration() * (mPlayerDistance / playerSpline.GetLength());
    SetTargetFov(spline.GetFovByTime(time));
  } else {
    SetTargetFov(spline.GetFovByLength(mPlayerDistance));
  }
}
