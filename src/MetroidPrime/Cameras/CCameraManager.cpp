#include "MetroidPrime/CCameraManager.hpp"

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
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
#include "MetroidPrime/ScriptObjects/CScriptTrigger.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"

// Retail's `SCameraHistory` constructor, which it emits out of line as a weak COMDAT at
// 0x801AD79C rather than inlining into `CCameraManager`'s constructor. 136 bytes: `mCount = 80`,
// then 80 copy-constructor calls filling the inline buffer, then `mBegin = data()` and
// `mEnd = mBegin + 1`. Named as retail names it for the reason `fn_801AB298` is.
extern "C" void fn_801AD79C(CCameraManager::SCameraHistory* self, const CTransform4f& initial) {
  self->mTransforms.mCount = 80;
  rstl::uninitialized_fill_n(self->mTransforms.data(), 80, initial);
  self->mBegin = self->mTransforms.data();
  self->mEnd = self->mBegin + 1;
}

// Retail's `SCameraHistory::Last`, out of line at 0x801AAE20. It is a free `extern "C"` function
// taking the history as its `this` rather than a member, for the reason `fn_801AB298` is: a member
// is emitted under its mangled name, which objdiff pairs with nothing, and retail's 264 bytes then
// sit at 0.00% forever. Defined between `StartScreenFlash` and `GetLastCameraTransform` because
// 0x801AAE20 sits between 0x801AAF28 and 0x801AAD3C (`tools/check_decl_order.py`).
extern "C" rstl::optional_object< CTransform4f >
fn_801AAE20(const CCameraManager::SCameraHistory* self);

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
, mScreenFlashTimer(0.f)
, mInWater(false)
, xfa4_25_(false)
, mWasFogEnabled(false)
, mFogEnabled(false) {
  // Retail 0x801AD734 calls the history's out-of-line fill (`fn_801AD79C`) from the constructor
  // body, with the identity transform read out of `.rodata`. `mCameraHistory` is left out of the
  // list above so no default-construction stores precede the call; `fn_801AD79C` writes every
  // field retail's copy constructor does (`mCount`, the buffer, `mBegin`, `mEnd`).
  fn_801AD79C(&mCameraHistory, CTransform4f::Identity());
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

// Retail 0x801AC638 / 0x801AC588 / 0x801AC4C4. All three walk one `CObjectList` off the state
// manager with an index (`lha` on `mFirstId` at +0x2008, then `GetNextObjectIndex`'s
// `mObjects[idx].mNext` at +8+8*idx, both signed halfwords), pull the actor out of the slot and
// cast it to `CScriptTrigger`, and skip anything null or not active (`CEntity::GetActive`, byte
// +0x20). `UpdateCameraTriggers` first requires the id to name a `CGameCamera`.
//
// The list is read once into a register before the loop, as `*(CObjectList**)(CStateManager +
// 0x848)` - `m_objectLists[kOL_ScriptActors]`.
void CCameraManager::TransferCameraTriggers(CGameCamera& from, CGameCamera& to,
                                            CStateManager& mgr) {
  CObjectList& list = mgr.ObjectListById(kOL_ScriptActors);
  for (int idx = list.GetFirstObjectIndex(); idx != -1; idx = list.GetNextObjectIndex(idx)) {
    if (CScriptTrigger* trigger = TCastToPtr< CScriptTrigger >(list[idx])) {
      if (trigger->GetActive()) {
        trigger->ReplaceInhabitant(from.GetUniqueId(), to.GetUniqueId(), mgr);
      }
    }
  }
}

void CCameraManager::UpdateCameraTriggerOccupancy(CGameCamera& camera, CStateManager& mgr) {
  CObjectList& list = mgr.ObjectListById(kOL_ScriptActors);
  for (int idx = list.GetFirstObjectIndex(); idx != -1; idx = list.GetNextObjectIndex(idx)) {
    if (CScriptTrigger* trigger = TCastToPtr< CScriptTrigger >(list[idx])) {
      if (trigger->GetActive()) {
        trigger->RemoveInhabitantIfOutside(camera.GetUniqueId(), mgr);
      }
    }
  }
}

void CCameraManager::UpdateCameraTriggers(TUniqueId uid, CStateManager& mgr) {
  if (TCastToPtr< CGameCamera >(mgr.ObjectById(uid))) {
    CObjectList& list = mgr.ObjectListById(kOL_ScriptActors);
    for (int idx = list.GetFirstObjectIndex(); idx != -1; idx = list.GetNextObjectIndex(idx)) {
      if (CScriptTrigger* trigger = TCastToPtr< CScriptTrigger >(list[idx])) {
        if (trigger->GetActive()) {
          trigger->UpdateCameraInhabitant(uid, mgr);
        }
      }
    }
  }
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
  // Retail 0x801ABD68 ends `lwz r0,532(r3); rlwinm r3,r0,31,31,31`. SH=31 with mask 31,31 is MWCC's
  // extract for a 1-bit field at bit 0, so the flag word is read as a bit 0, not bit 31: `& 1u`
  // gives `clrlwi r3,r0,31` and 96.667%, one instruction short. Getting the `rlwinm` needs a real
  // bitfield member on `CCinematicCamera`; see docs/goal-notes/progress-prime1-ccameramanager.md
  // for the two spellings measured (65.50% and 93.33%) and why neither can hold.
  return (mCinematicCamera->GetFlags() & 1u) != 0;
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
  // Measured 2026-09-30 at retail 0x801ABB38, and deliberately not written. The skeleton is:
  //   if (!mInterpCamera->GetActive()) return;                        // byte +0x20, bit 7
  //   if (CGameCamera* cam = TCastToPtr<CGameCamera>(mgr.GetObjectById(uid))) {  // 0x8009A8DC
  //     if (cam->GetActive()) { SetCurrentCameraId(uid); goto notify; }
  //   }
  //   { int s = mgr.GetPlayer(mPlayerIndex)-><+0x38C>;               // signed compare vs 3, then 0
  //     SetCurrentCameraId((s == 0 || s == 3) ? mFpCamera : mBallCamera)->GetUniqueId(); }
  // notify:
  //   UpdateCameraTriggers(GetCurrentCameraId(false), mgr);
  //   mInterpCamera->SetActive(false);                               // vtable+0x1C
  // It is not written because `TCastToPtr<11CGameCamera>__FP7CEntity` (0x8009A8DC) lives in
  // TypesMatch.cpp, which is in the DOL build but deliberately NOT in the port build
  // (files.cmake), so writing the body opens a port gap for zero matched functions - the same
  // trade run 2 rejected for SetSpindleCamera. this+0x18 is mFpCamera, this+0x1C mBallCamera,
  // and the camera's unique id is at +0x8. See
  // docs/goal-notes/progress-prime1-ccameramanager.md.
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

// Retail 0x801AB42C. The three constant-pool reads at +0x00/+0x04/+0x08 of 0x804174BC are
// `CVector3f::sUpVector` (0,0,1) and the three transform reads at fpCam+0x28/+0x38/+0x48 are
// `m01`/`m11`/`m21`, i.e. `GetTransform().GetForward()`. Echoes calls the **double** `cos` with
// `M_PIF / 6.f` promoted to double (the .sdata2 word pair at 0x8041CD48 is
// (float)(pi/6) = 0.5235987901687622, not pi/6 itself), so this is `cos(...)` and not Prime 1's
// `cosf(...)`.
float CCameraManager::GetCameraBobMagnitude() const {
  const float dot = CMath::AbsF(CMath::Limit(
      CVector3f::Dot(mFpCamera->GetTransform().GetForward(), CVector3f::Up()), 1.f));
  const float pitch = CMath::Limit(dot / static_cast< float >(cos(M_PIF / 6.f)), 1.f);
  return 1.f - pitch;
}

void CCameraManager::AddCamera(TUniqueId uid, CStateManager& mgr) {
  if (!TCastToConstPtr< CGameCamera >(mgr.GetObjectById(uid))) {
    return;
  }

  // Retail 0x801AB34C walks `mCameras` with two `rstl::vector<TUniqueId>::iterator` locals held
  // in registers (its own `it` is spilled at 24(r1)), and grows the vector with an explicit
  // `size() == capacity()` test that reserves `size() + 1` before a `push_back_unsafe`. The
  // doubling `reserve` inside `rstl::vector::push_back` is a different function (46.96% before).
  rstl::vector< TUniqueId >::iterator it = mCameras.begin();
  rstl::vector< TUniqueId >::iterator const end = mCameras.end();
  while (it != end && *it != uid) {
    ++it;
  }
  if (it != end) {
    return;
  }
  if (mCameras.size() == mCameras.capacity()) {
    mCameras.reserve(mCameras.size() + 1);
  }
  mCameras.push_back_unsafe(uid);
}

// Retail's `SCameraHistory::Push` at 0x801AB298, named as retail names it. It is a free
// `extern "C"` function taking the history as its `this` rather than a member, because a member
// is emitted under its mangled name and objdiff pairs by name: as a member this same body scored
// 0.00% while retail's 180 bytes sat unmatched under the same address. The body itself had to be
// written this way too - `bool full = false; if (mBegin == mEnd) full = true; *mEnd = xf;
// ++mEnd;` rather than `const bool full = mBegin == mEnd; *mEnd++ = xf;`, because only the first
// shape gives MWCC a branch on the compare (`cmplw r0,r3; bne`) instead of materialising the
// bool as `subf`/`cntlzw`/`srwi` and branching later.
extern "C" void fn_801AB298(CCameraManager::SCameraHistory* self, const CTransform4f& xf) {
  bool full = false;
  if (self->mBegin == self->mEnd) {
    full = true;
  }
  *self->mEnd = xf;
  ++self->mEnd;
  if (self->mEnd == self->mTransforms.end()) {
    self->mEnd = self->mTransforms.begin();
  }
  if (full) {
    ++self->mBegin;
    if (self->mBegin == self->mTransforms.end()) {
      self->mBegin = self->mTransforms.begin();
    }
  }
}

void CCameraManager::UpdateCameraHistory(CStateManager& mgr) {
  // Retail 0x801AB11C does not go through `GetCurrentCamera`: it calls `GetCurrentCameraId(false)`
  // (with a literal `false`, so `li r5,0`), `CStateManager::GetObjectById`, then
  // `TCastToPtr<11CGameCamera>` (0x8009A8DC), and copy-constructs from the camera's transform at
  // +0x24. That symbol is already referenced from this unit by `AddCamera`, so writing it out
  // opens no port gap. Calling `GetCurrentCamera` instead emits a call to a function retail does
  // not call here, which is most of the 79.34% this scored before.
  const CTransform4f xf = TCastToConstPtr< CGameCamera >(
                              mgr.GetObjectById(GetCurrentCameraId(false)))
                              ->GetTransform();
  if (mCameraHistory.Size() != 0) {
    const CTransform4f last = *fn_801AAE20(&mCameraHistory);
    const CVector3f delta = xf.GetTranslation() - last.GetTranslation();
    if (delta.IsMagnitudeSafe() && delta.Magnitude() > 0.5f) {
      fn_801AB298(&mCameraHistory, xf);
    }
  } else {
    fn_801AB298(&mCameraHistory, xf);
  }
}

void CCameraManager::Reset(TUniqueId uid, CStateManager& mgr) {
  // TODO: reset camera selection, hints, shakes, fog, audio, and transform history together.
}

void CCameraManager::StartScreenFlash() { mScreenFlashTimer = 0.95f; }

// Retail 0x801AAE20, 264 bytes. Retail returns the `optional_object` through the hidden pointer in
// r3 with `this` in r4, which a free function returning a 52-byte class reproduces exactly.
//
// Two details are measured, not guessed. Two separate `return`s rather than a `?:` plus
// `*--last`, because retail has two straight-line copy-constructor calls, one per path; and the
// wrap-around case indexes as `mTransforms[mTransforms.size() - 1]` rather than `end() - 1`.
// The copy-constructor calls carry no null test because `rstl::construct_impl` for `CTransform4f`
// is a call to a bodyless function (`include/Kyoto/Math/CTransform4f.hpp`); the older comment
// here claiming this "cannot reach 264 from our headers" predated that specialisation and no
// longer holds.
extern "C" rstl::optional_object< CTransform4f >
fn_801AAE20(const CCameraManager::SCameraHistory* self) {
  if (self->Size() == 0) {
    return rstl::optional_object< CTransform4f >();
  }
  if (self->mEnd == self->mTransforms.begin()) {
    return rstl::optional_object< CTransform4f >(self->mTransforms[self->mTransforms.size() - 1]);
  }
  return rstl::optional_object< CTransform4f >(*(self->mEnd - 1));
}

const CTransform4f& CCameraManager::GetLastCameraTransform() const {
  // Retail 0x801AAD3C. Two separate `fn_801AAE20` calls, each into its own 52-byte stack slot, and
  // both the inlined `Size()` test and the first call's valid-flag test branch to the same
  // `sIdentity` return - so the condition is `Size() != 0 && Last()` and the value is a *second*
  // `Last()`. This deliberately returns a reference into a temporary that dies at the closing
  // brace: that is the undefined behaviour retail has (it is why the old code here, which returned
  // a pointer into the ring buffer instead, was only 64.1579%), and nothing in this tree calls the
  // function, so no port behaviour depends on it.
  if (mCameraHistory.Size() != 0 && fn_801AAE20(&mCameraHistory)) {
    return fn_801AAE20(&mCameraHistory).data();
  }
  return CTransform4f::Identity();
}

// Retail 0x801AACAC, 144 bytes: four calls and no branches. `CActor::mPosition` is x54, so the
// first argument is `addi r4,r29,84` off `from`; `GetFluidList()` returns the vector by reference
// and its result goes straight back in r4 for `SetFluidList`.
void CCameraManager::TransferCameraState(CGameCamera& from, CGameCamera& to, CStateManager& mgr) {
  to.SetTranslation(from.GetTranslation());
  to.SetFluidList(from.GetFluidList());
  TransferCameraTriggers(from, to, mgr);
  UpdateCameraTriggerOccupancy(to, mgr);
}

// Retail's `rstl::vector<CTransform4f>::~vector` COMDAT, emitted at 0x801AAC28 because this unit
// instantiates it. Named as retail names it so objdiff pairs it; see `fn_801AB298`.
extern "C" rstl::vector< CTransform4f >* fn_801AAC28(rstl::vector< CTransform4f >* self, int flag) {
  if (self != nullptr) {
    rstl::destroy(self->begin(), self->end());
    self->mAllocator.deallocate(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

bool CCameraManager::CheckSplineCollision(const CMotionSpline& spline, int mode,
                                          const CMaterialFilter& filter, CStateManager& mgr,
                                          CMaterialList& hitMaterial, float step,
                                          float thickness) const {
  // TODO: sample the motion spline and perform the selected raycast/obstruction/thickness test.
  return false;
}
