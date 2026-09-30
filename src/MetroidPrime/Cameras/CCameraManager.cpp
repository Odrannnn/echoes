#include "MetroidPrime/CCameraManager.hpp"

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "MetroidPrime/CCameraShakeManager.hpp"
#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CFluidPlaneCPU.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/Cameras/CCinematicCamera.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Cameras/CFixedCamera.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Cameras/CInterpolationCamera.hpp"
#include "MetroidPrime/Cameras/CPathCamera.hpp"
#include "MetroidPrime/Cameras/CSpindleCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptObjects/CScriptCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"

// NonMatching scaffold: camera creation and the separate hint/shake subsystems remain TODO.
CCameraManager::CCameraManager(TUniqueId curCamera, int playerIndex)
: mPlayerIndex(playerIndex)
, mCurCameraId(curCamera)
, mCinematicCameraId(kInvalidUniqueId)
, mFpCamera(nullptr)
, mBallCamera(nullptr)
, x20_(0)
, mInterpCamera(nullptr)
, mPathCamera(nullptr)
, mSpindleCamera(nullptr)
, mCinematicCamera(nullptr)
, mFixedCamera(nullptr)
, mFogDensityFactor(1.f)
, mFogDensitySpeed(0.f)
, mFogDensityFactorTarget(1.f)
, mFluidFogTime(0.f)
, mCameraHintManager(nullptr)
, mCameraShakeManager(nullptr)
, mFirstPersonFov(55.f)
, mCameraHistory(CTransform4f::Identity())
, mScreenFlashTimer(0.f)
, mInWater(false)
, xfa4_25_(false)
, mWasFogEnabled(false)
, mFogEnabled(false) {
  // TODO: construct the owned hint and shake managers once their layouts are recovered.
  // mSurfaceCamera is assigned by CreateCameras, not initialized by the original constructor.
}

float CCameraManager::GetFirstPersonFOV() const { return mFirstPersonFov; }

void CCameraManager::SetFirstPersonFOV(float fov) { mFirstPersonFov = fov; }

float CCameraManager::GetDefaultThirdPersonVerticalFOV() { return 60.f; }

float CCameraManager::GetDefaultFirstPersonNearClipDistance() { return 0.2f; }

float CCameraManager::GetDefaultFirstPersonFarClipDistance() { return 750.f; }

float CCameraManager::GetDefaultAspectRatio() { return 1.42f; }

void CCameraManager::SetAspectRatio(float aspect, CStateManager& mgr) {
  for (int i = 0; i < mCameras.size(); ++i) {
    CGameCamera* camera = static_cast< CGameCamera* >(mgr.ObjectById(mCameras[i]));
    camera->SetAspectRatio(aspect);
  }
}

void CCameraManager::CreateCameras(CStateManager& mgr) {
  // TODO: create/register the eight runtime cameras and this player's audio listener.
}

void CCameraManager::UpdateCameras(float dt, CStateManager& mgr) {
  for (int i = 0; i < mCameras.size(); ++i) {
    if (CGameCamera* camera = static_cast< CGameCamera* >(mgr.ObjectById(mCameras[i]))) {
      camera->Think(dt, mgr);
      camera->UpdatePerspective(dt, mgr);
    }
  }
}

void CCameraManager::ResetCameras(CStateManager& mgr) {
  CTransform4f xf(mgr.GetPlayer(mPlayerIndex)->CreateTransformFromMovementDirection());
  xf.SetTranslation(mgr.GetPlayer(mPlayerIndex)->GetEyePosition());

  for (int i = 0; i < mCameras.size(); ++i) {
    if (CGameCamera* camera = static_cast< CGameCamera* >(mgr.ObjectById(mCameras[i]))) {
      camera->Reset(xf, mgr);
    }
  }
}

void CCameraManager::UpdateFogState() {
  mWasFogEnabled = mFogEnabled;
  mFogEnabled = !mFog.IsFogDisabled();
}

TUniqueId CCameraManager::GetCurrentCameraId(bool selector) const {
  if (IsInCinematicCamera()) {
    if (mCinematicCamera) {
      return mCinematicCamera->GetUniqueId();
    }
    return kInvalidUniqueId;
  }
  return mCurCameraId;
}

CGameCamera* CCameraManager::CurrentCamera(CStateManager& mgr, bool selector) {
  return static_cast< CGameCamera* >(mgr.ObjectById(GetCurrentCameraId(selector)));
}

const CGameCamera* CCameraManager::GetCurrentCamera(const CStateManager& mgr, bool selector) const {
  return static_cast< const CGameCamera* >(mgr.GetObjectById(GetCurrentCameraId(selector)));
}

void CCameraManager::SetCurrentCameraId(TUniqueId uid) { mCurCameraId = uid; }

void CCameraManager::UpdateAudioListener(CStateManager& mgr) {
  const CTransform4f xf(GetCurrentCameraTransform(mgr, true));
  CSfxManager::UpdateListener(xf.GetTranslation(), CVector3f::Zero(), xf.GetColumn(kDY),
                              xf.GetColumn(kDZ), CAudioSys::kMaxVolume, mPlayerIndex);
}

void CCameraManager::UpdateFilters(float dt, CStateManager& mgr) {
  // TODO: fluid fog, underwater sound transitions, and the screen-flash filter.
}

float CCameraManager::GetWaterFarDistance(CStateManager& mgr, const CScriptWater* water) {
  float density = 1.f - water->GetFluidPlane()->GetAlpha();
  if (mgr.GetPlayerState(mPlayerIndex)->HasPowerUp(CPlayerState::kIT_GravityBoost)) {
    density = water->GetGravityWaterFogDistanceRange() * density +
              water->GetGravityWaterFogDistanceBase();
  } else {
    density = water->GetWaterFogDistanceRange() * density + water->GetWaterFogDistanceBase();
  }
  return density * mFogDensityFactor;
}

void CCameraManager::SetWaterFogScale(float target, float speed) {
  mFogDensityFactorTarget = target;
  if (mFogDensityFactorTarget < mFogDensityFactor) {
    mFogDensitySpeed = -speed;
  } else {
    mFogDensitySpeed = speed;
  }
}

void CCameraManager::TransferCameraTriggers(CGameCamera& from, CGameCamera& to,
                                            CStateManager& mgr) {
  // TODO: transfer camera occupancy in the active trigger list.
}

void CCameraManager::UpdateCameraTriggerOccupancy(CGameCamera& camera, CStateManager& mgr) {
  // TODO: update trigger occupancy for the supplied runtime camera.
}

void CCameraManager::UpdateCameraTriggers(TUniqueId uid, CStateManager& mgr) {
  // TODO: notify active triggers of the selected camera ID.
}

void CCameraManager::Update(float dt, CStateManager& mgr) {
  mCameraHintManager->Update(dt);
  UpdateCameras(dt, mgr);
  UpdateAudioListener(mgr);
  mCameraShakeManager->Update(dt, mgr);
  UpdateFilters(dt, mgr);
  UpdateCameraHistory(mgr);
}

void CCameraManager::ProcessInput(const CFinalInput& input, CStateManager& mgr) {
  for (int i = 0; i < mCameras.size(); ++i) {
    if (CGameCamera* camera = static_cast< CGameCamera* >(mgr.ObjectById(mCameras[i]))) {
      if (camera->GetInputIndex() == static_cast< int >(input.ControllerNumber())) {
        camera->ProcessInput(input, mgr);
      }
    }
  }
}

void CCameraManager::SetCinematicCameraId(CStateManager& mgr, TUniqueId uid) {
  if (mCinematicCameraId != kInvalidUniqueId && mCinematicCameraId != uid) {
    if (CScriptCamera* camera =
            TCastToPtr< CScriptCamera >(mgr.GetObjectByIdFromListAll(mCinematicCameraId))) {
      camera->MarkViewed(mgr);
    }
  }
  mCinematicCameraId = uid;
}

void CCameraManager::AddCinemaCamera(TUniqueId uid, CStateManager& mgr) {
  // TODO: copy script cinematic settings into the runtime camera and activate it.
}

void CCameraManager::EnterCinematic(CStateManager& mgr) {
  // TODO: unfreeze the player, remove owned projectiles/effects, and clear camera shakes.
}

void CCameraManager::StopCinematics(CStateManager& mgr) {
  // Measured 2026-09-30, deliberately not written here. Retail 0x801ABEDC is
  //   mCinematicCamera->SetActive(false); SetCinematicCameraId(mgr, kInvalidUniqueId);
  //   mgr.GetPlayer(mPlayerIndex)->fn_8001660c(mgr); mFpCamera->SkipCinematic();
  //   CMain::SetGameFrameDrawn(<gpGameState->GetGameMode() vtable+0x34> == 2);
  // The first four lines measure 71.56%, but mFpCamera->SkipCinematic() asks the port for
  // CFirstPersonCamera::SkipCinematic, and CFirstPersonCamera.cpp is deliberately not in the port
  // build (tools/check_files_cmake.py: listing it opens 10 symbols and closes 0). The last line's
  // CGameMode virtual is unlabelled and its `== 2` does not fold to a clean predicate either.
  // See docs/goal-notes/progress-prime1-ccameramanager.md.
}

void CCameraManager::SetCinematicPaused(bool paused) {
  if (mCinematicCamera) {
    mCinematicCamera->SetPaused(paused);
  }
}

CTransform4f CCameraManager::GetCurrentCameraTransform(const CStateManager& mgr,
                                                       bool selector) const {
  return GetCurrentCamera(mgr, selector)->GetTransform() *
         CTransform4f::Translate(mCameraShakeManager->GetShakeOffset(mgr));
}

CVector3f CCameraManager::GetGlobalCameraTranslation(const CStateManager& mgr,
                                                     bool selector) const {
  return GetCurrentCamera(mgr, selector)->GetTransform().Rotate(
      mCameraShakeManager->GetShakeOffset(mgr));
}

bool CCameraManager::IsInCinematicCamera() const { return mCinematicCameraId != kInvalidUniqueId; }

bool CCameraManager::fn_801ABD68() const {
  if (!IsInCinematicCamera()) {
    return false;
  }
  return (mCinematicCamera->GetFlags() & 0x80000000u) != 0;
}

bool CCameraManager::IsInBallCamera() const { return mCurCameraId == mBallCamera->GetUniqueId(); }

bool CCameraManager::IsInFPCamera() const {
  return mCurCameraId == mFpCamera->GetUniqueId();
}

bool CCameraManager::IsInterpolationCameraActive() const {
  return mInterpCamera->GetActive();
}

bool CCameraManager::ShouldBypassInterpolationCamera() const { return false; }

bool CCameraManager::IsBallCameraTransitioning(const CStateManager& mgr) const {
  // TODO: combine ball-camera transition state with player morph/camera state.
  return false;
}

void CCameraManager::SetPlayerCamera(CStateManager& mgr, TUniqueId uid) {
  // TODO: select the active requested camera or the morph-state fallback, then end interpolation.
}

void CCameraManager::SetupInterpolation(const CTransform4f& xf, TUniqueId from, TUniqueId to,
                                        bool interpolateRotation,
                                        CInterpolationCamera::EPositionMode positionMode,
                                        CInterpolationCamera::ERotationMode rotationMode,
                                        CStateManager& mgr, bool flag,
                                        float duration, float fov) {
  if (!IsInFPCamera()) {
    mInterpCamera->SetInterpolation(xf, from, to, interpolateRotation, positionMode, rotationMode,
                                    mgr, flag, duration, fov);
    SetCurrentCameraId(mInterpCamera->GetUniqueId());
  }
}

void CCameraManager::CinematicCut(CStateManager& mgr) {
  // Measured 2026-09-30 at retail 0x801AB9DC, and written out; 94.111%. Echoes drops Prime 1's
  // trailing SetCurrentCameraId(mBallCamera->GetUniqueId()) and interpolates for 2s with the
  // 0x3A9C4000 .sdata2 constant (1250 * 2^-20 = 0.0011920929) as the delay:
  //   if (IsInCinematicCamera()) { mBallCamera->TeleportCamera(mCinematicCamera->GetTransform(), mgr);
  //     mBallCamera->InterpolateFOV(mCinematicCamera->GetFov(), 2.f, 0.0011920929f,
  //                                 mBallCamera->GetUniqueId(), mgr); StopCinematics(mgr); }
  // The last three instructions differ only in register choice and dead-store order (see
  // docs/goal-notes/progress-prime1-ccameramanager.md). It is NOT written here because the three
  // callees it needs - TeleportCamera(CTransform4f const&, CStateManager&), GetFov() const and
  // InterpolateFOV(float,float,float,TUniqueId,CStateManager&) - are retail symbols with no port
  // definition, so writing the body takes the port from 250 to 253 undefined and fails
  // tools/gate.sh. 94% buys no matched function, so the gap is left documented rather than paid for.
}

void CCameraManager::SetPathCamera(TUniqueId uid, CStateManager& mgr) {
  // TODO: validate the path-camera script actor, activate/reset its runtime camera, and notify
  // triggers.
  // Measured 2026-09-30 at retail 0x801AB8DC: identical to SetSpindleCamera (0x801AB794) apart
  // from the two callees - the non-const ObjectById becomes the const GetObjectById (0x80041998)
  // and the cast target is TCastToPtr<17CScriptPathCamera>(CEntity*) at 0x8009952C, not
  // <20CScriptSpindleCamera> at 0x80098F44. Both callees live in TypesMatch.cpp, which is in the
  // DOL build but deliberately out of the port build (files.cmake), so writing the body needs a
  // PC-side definition for the cast as well.
}

void CCameraManager::ClearPathCamera() {
  mPathCamera->SetActive(false);
  mPathCamera->SetScriptCameraId(kInvalidUniqueId);
}

void CCameraManager::SetSpindleCamera(TUniqueId uid, CStateManager& mgr) {
  // TODO: select/reset the runtime spindle camera from the script actor and notify triggers.
  // Measured 2026-09-30 at retail 0x801AB794: the body is
  //   if (!mSpindleCamera->GetActive() || mSpindleCamera->GetSpindleCameraId() != uid)
  //     if (TCastToPtr<CScriptSpindleCamera>(mgr.ObjectById(uid))) { SetActive(true);
  //       SetSpindleCameraId(uid); Reset(GetCurrentCameraTransform(mgr,false), mgr);
  //       UpdateCameraTriggers(mSpindleCamera->GetUniqueId(), mgr); }
  // That spelling measures 96.68%: retail emits a second, unreachable `beq` to the epilogue on the
  // same condition. It also needs TCastToPtr<20CScriptSpindleCamera>__FP7CEntity (0x80098F44), which
  // is not in the port's symbol set, so it was reverted rather than carried with a stand-in.
}

void CCameraManager::ClearSpindleCamera() {
  mSpindleCamera->SetActive(false);
  mSpindleCamera->SetSpindleCameraId(kInvalidUniqueId);
}

void CCameraManager::SetFixedCamera(TUniqueId uid, const CTransform4f& xf, CStateManager& mgr) {
  // TODO: activate/reset the fixed camera with this target ID and transform, then notify triggers.
  // Measured 2026-09-30 at retail 0x801AB674: if (!mFixedCamera->GetActive() ||
  // mFixedCamera->mScriptCameraId(at +0x20C) != uid) { SetActive(true); the out-of-line
  // SetScriptCameraId at 0x80228910; Reset(xf, mgr) via vtable+0x80; UpdateCameraTriggers. Unlike
  // the other two it takes the transform as a parameter, so there is no GetCurrentCameraTransform
  // call and its frame is 32 bytes, not 96.
}

void CCameraManager::ClearFixedCamera() {
  // Measured 2026-09-30 at retail 0x801AB640: the whole body is the virtual
  // CGameCamera::SetActive(vtable+0x1C) on the fixed camera at +0x38.
  mFixedCamera->SetActive(false);
}

void CCameraManager::SetSurfaceCamera(TUniqueId uid, CStateManager& mgr) {
  // TODO: validate the surface-camera script actor and activate/reset its runtime camera.
}

void CCameraManager::ClearSurfaceCamera() {
  // TODO: deactivate the surface camera and clear its script actor ID.
  // Measured 2026-09-30 at retail 0x801AB4E8: SetActive(false) on +0x34, then the *out-of-line*
  // SetScriptCameraId at 0x801E95A8 - unlike ClearPathCamera/ClearSpindleCamera, which store
  // 0x200 inline. There is no CSurfaceCamera unit in splits.txt, so the class needs a declaration
  // like CFixedCamera's, and that callee is in an unclaimed range, so it is a new port symbol.
}

float CCameraManager::GetCameraBobMagnitude() const {
  // TODO: attenuate bob with the first-person camera's pitch using shared vector/math helpers.
  return 0.f;
}

void CCameraManager::AddCamera(TUniqueId uid, CStateManager& mgr) {
  if (!TCastToConstPtr< CGameCamera >(mgr.GetObjectById(uid))) {
    return;
  }
  for (int i = 0; i < mCameras.size(); ++i) {
    if (mCameras[i] == uid) {
      return;
    }
  }
  mCameras.push_back(uid);
}

void CCameraManager::SCameraHistory::Push(const CTransform4f& xf) {
  const bool full = mBegin == mEnd;
  *mEnd++ = xf;
  if (mEnd == mTransforms.end()) {
    mEnd = mTransforms.begin();
  }
  if (full && ++mBegin == mTransforms.end()) {
    mBegin = mTransforms.begin();
  }
}

void CCameraManager::UpdateCameraHistory(CStateManager& mgr) {
  const CTransform4f xf = GetCurrentCamera(mgr, false)->GetTransform();
  if (mCameraHistory.Size() == 0) {
    mCameraHistory.Push(xf);
    return;
  }

  const CTransform4f last = *mCameraHistory.Last();
  const CVector3f delta = xf.GetTranslation() - last.GetTranslation();
  if (delta.IsMagnitudeSafe() && delta.Magnitude() > 0.5f) {
    mCameraHistory.Push(xf);
  }
}

void CCameraManager::Reset(TUniqueId uid, CStateManager& mgr) {
  // TODO: reset camera selection, hints, shakes, fog, audio, and transform history together.
}

void CCameraManager::StartScreenFlash() { mScreenFlashTimer = 0.95f; }

rstl::optional_object< CTransform4f > CCameraManager::SCameraHistory::Last() const {
  if (Size() == 0) {
    return rstl::optional_object< CTransform4f >();
  }
  const CTransform4f* last = mEnd == mTransforms.begin() ? mTransforms.end() : mEnd;
  return rstl::optional_object< CTransform4f >(*--last);
}

const CTransform4f& CCameraManager::GetLastCameraTransform() const {
  // Return stable history storage; the target appears to return a destroyed optional's payload.
  if (mCameraHistory.Size() == 0) {
    return CTransform4f::Identity();
  }
  const CTransform4f* last = mCameraHistory.mEnd == mCameraHistory.mTransforms.begin()
                                 ? mCameraHistory.mTransforms.end()
                                 : mCameraHistory.mEnd;
  return *--last;
}

void CCameraManager::TransferCameraState(CGameCamera& from, CGameCamera& to, CStateManager& mgr) {
  // TODO: transfer translation, fluid membership and trigger occupancy, then notify triggers.
}

bool CCameraManager::CheckSplineCollision(const CMotionSpline& spline, int mode,
                                          const CMaterialFilter& filter, CStateManager& mgr,
                                          CMaterialList& hitMaterial, float step,
                                          float thickness) const {
  // TODO: sample the motion spline and perform the selected raycast/obstruction/thickness test.
  return false;
}
