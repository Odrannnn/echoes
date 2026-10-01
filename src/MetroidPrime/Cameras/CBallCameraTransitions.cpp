#include "MetroidPrime/Cameras/CBallCamera.hpp"

#include "Collision/CMaterialFilter.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"

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
  // TODO: finish the morphed-player transition and recover collision-safe placement.
  return false;
}
