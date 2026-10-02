#include "MetroidPrime/Cameras/CBallCamera.hpp"

#include "Collision/CMaterialFilter.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"

namespace {
// The failsafe collision filter this translation unit passes to `CheckSplineCollision`.
// Retail holds it at 0x803DB4A0 and builds it in `fn_801AA3EC`; the `.bss` for it is not
// claimed by any unit in `config/G2ME01/splits.txt` (see the notes for this item).
const CMaterialFilter skFailsafeFilter = CMaterialFilter::MakeIncludeExclude(
    CMaterialList(kMT_Unknown59),
    CMaterialList(kMT_NoPlatformCollision, kMT_Player, kMT_Character, kMT_CameraPassthrough));
} // namespace

bool CBallCamera::CheckFailsafeFromMorphBallState(CStateManager& mgr) {
  const float step = mFromBallTransition->mSpline.GetLength() / 24.f;
  CMaterialList intersectMaterial;
  return GetCameraManager(mgr).CheckSplineCollision(mFromBallTransition->mSpline, 0,
                                                    skFailsafeFilter, mgr, intersectMaterial,
                                                    step, 0.f);
}

bool CBallCamera::TransitionFromMorphBallState(CStateManager& mgr) {
  // TODO: build the from-ball spline from player and first-person camera transforms.
  return false;
}

bool CBallCamera::UpdateTransitionFromBallCamera(CStateManager& mgr) {
  // TODO: interpolate the from-ball transform and recover blocked transition placement.
  return false;
}

bool CBallCamera::CheckFailsafeToMorphBallState(CStateManager& mgr) {
  const float step = mToBallTransition->mSpline.GetLength() / 24.f;
  CMaterialList intersectMaterial;
  return GetCameraManager(mgr).CheckSplineCollision(mToBallTransition->mSpline, 0,
                                                    skFailsafeFilter, mgr, intersectMaterial,
                                                    step, 0.f);
}

bool CBallCamera::TransitionToMorphBallState(CStateManager& mgr) {
  // TODO: build the to-ball spline with collision-constrained control points.
  return false;
}

bool CBallCamera::UpdateTransitionToBallCamera(float dt, CStateManager& mgr) {
  // TODO: advance the morph transition using the owned spline and player movement.
  return false;
}

bool CBallCamera::UpdateTransitionToBallCamera(CStateManager& mgr) {
  mLookAtBall = false;
  const CPlayer& player = Player(mgr);
  // Retail builds `dir` from the player's forward first and only then overwrites it with
  // `mLookPos - GetTranslation()`. The first value is dead but its three stores are in retail's
  // bytes, so the declaration and the reassignment both have to stay.
  CVector3f dir = player.GetTransform().GetForward();
  dir = mLookPos - GetTranslation();
  const CVector3f pos = GetTranslation();
  if (dir.IsMagnitudeSafe()) {
    dir.Normalize();
    CVector3f up = GetTransform().GetForward();
    up.SetZ(0.f);
    up.Normalize();
    // Retail compares the *absolute* value here and passes that same absolute value to `acos`,
    // not the signed one; the two are different expressions, so both are named off one const.
    const float absProjection = CMath::AbsF(CMath::Limit(CVector3f::Dot(up, dir), 1.f));
    if (absProjection < 0.99999f) {
      const float progress =
          0.f == player.GetMorphDuration()
              ? 0.f
              : CMath::Clamp(0.f, player.GetMorphTime() / player.GetMorphDuration(), 1.f);
      const float fraction = 1.5f * progress;
      const float angle = CMath::Limit(fraction, 1.f);
      const CRelAngle step = CRelAngle::FromRadians(angle * acosf(absProjection));
      const CQuaternion rotation = CQuaternion::LookAt(CUnitVector3f(up), CUnitVector3f(dir), step);
      SetTransform(rotation.BuildTransform4f() *
                   CTransform4f::LookAt(pos, pos + up, CVector3f::Up()));
    } else {
      SetTransform(CTransform4f::LookAt(pos, pos + dir, CVector3f::Up()));
    }
  }
  SetTranslation(pos);
  TeleportCamera(pos, mgr);
  return false;
}
