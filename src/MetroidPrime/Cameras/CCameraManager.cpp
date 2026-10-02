#include "MetroidPrime/CCameraManager.hpp"

#include "Collision/CRayCastResult.hpp"
#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "MetroidPrime/CCameraShakeManager.hpp"
#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CUnknown85.hpp"
#include "MetroidPrime/CFluidPlaneCPU.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/Cameras/CCinematicCamera.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Cameras/CFixedCamera.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Cameras/CInterpolationCamera.hpp"
#include "MetroidPrime/Cameras/CPathCamera.hpp"
#include "MetroidPrime/Cameras/CSpindleCamera.hpp"
#include "MetroidPrime/Cameras/CSurfaceCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptObjects/CScriptCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPathCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptTrigger.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"

// Retail 0x801AAC08, defined near the foot of this file: this vector's out-of-line element
// constructor, called by both `vector<CRayCastResult>` helpers below.
extern "C" void fn_801AAC08(CRayCastResult* self, const CRayCastResult& other);

// Retail 0x801AD8DC, 104 bytes: the outlined `rstl::uninitialized_copy` this unit instantiates for
// `vector<CRayCastResult>`. `first` arrives as a `const CRayCastResult* const&` - retail reads it
// once, `lwz r31,0x0(r3)`, and keeps it in a callee-saved register - while `last` arrives **as a
// pointer to a pointer**, `mr r29,r4` then `lwz r0,0x0(r29)`: retail reloads the end pointer on
// every iteration, so it is not a by-value parameter. `fn_801AD824` sets up exactly that pair (it
// stores the old vector's begin at 20(r1) and its end at 12(r1) and passes `&begin`, `&end`,
// `newItems`), which is what fixes the two spellings below.
//
// Returns the advanced destination, not the source: retail loads `mr r3,r30` in the epilogue, and
// `uninitialized_copy` returns the end of what it wrote.
extern "C" CRayCastResult* fn_801AD8DC(const CRayCastResult* const& first,
                                        const CRayCastResult** last, CRayCastResult* out) {
  const CRayCastResult* cur = first;
  CRayCastResult* dest = out;
  while (cur != *last) {
    fn_801AAC08(dest, *cur);
    ++cur;
    ++dest;
  }
  return dest;
}

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
// 0x848)` - `mObjectLists[kOL_ScriptActors]`.
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

// Retail 0x801ABB38, 320 bytes. Every callee is already referenced from this unit
// (`TCastToPtr<11CGameCamera>` by `AddCamera` and `UpdateCameraHistory`, `GetObjectById` by both),
// so writing the body opens no port gap - runs 2 and 4 refused it on the ground that
// `TCastToPtr` lives in `TypesMatch.cpp`, which is true but not the test; run 6 measured that the
// object already resolves it.
//
// The shape is one test with two tails and a shared epilogue block, which is `if/else` followed by
// the common tail - not three returns. `CPlayer+0x38C` is `mMorphBallState` and the compare is
// *signed* (`cmpwi` against 3 then 0), which MWCC only emits for a plain enum compare; 0 and 3 are
// `kMS_Unmorphed` and `kMS_Unmorphing`, i.e. the state is one where the player is not in morph ball
// form, so the first-person camera is the right one.
void CCameraManager::SetPlayerCamera(CStateManager& mgr, TUniqueId uid) {
  if (!mInterpCamera->GetActive()) {
    return;
  }

  // Both failing tests branch to the *same* target, which is laid out *after* the success block, so
  // the success test is the fall-through (`beq` past it) and this is the `&&` form, not the `||` one.
  const CGameCamera* cam = TCastToConstPtr< CGameCamera >(mgr.GetObjectById(uid));
  if (cam != nullptr && cam->GetActive()) {
    SetCurrentCameraId(uid);
  } else {
    switch (mgr.GetPlayer(mPlayerIndex)->GetMorphballTransitionState()) {
    case CPlayer::kMS_Unmorphed:
    case CPlayer::kMS_Unmorphing:
      SetCurrentCameraId(mFpCamera->GetUniqueId());
      break;
    default:
      SetCurrentCameraId(mBallCamera->GetUniqueId());
      break;
    }
  }

  UpdateCameraTriggers(GetCurrentCameraId(false), mgr);
  mInterpCamera->SetActive(false);
  // 93.24%, not 100%: retail emits three dead `mr r5,r31` (the value of `mgr`, kept live in an
  // argument register for the trailing `UpdateCameraTriggers`) where this emits one, and because
  // of that it allocates **seven** 4-byte outgoing-argument slots (8..32) against this function's
  // eight (8..36). The extra slot is the by-ref copy of the `TUniqueId` for `UpdateCameraTriggers`:
  // retail passes `r1+8` - the `GetCurrentCameraId` sret buffer - straight through
  // (`addi r4,r1,8` with no reload), and MWCC 2.7 always materialises a fresh one. The nested call,
  // a named `const TUniqueId id`, `GetCurrentCameraId(0)` and a `const TUniqueId&` cast all measure
  // the same; see docs/goal-notes/progress-prime1-ccameramanager.md.
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

// Retail 0x801AB8DC, 256 bytes: SetSurfaceCamera's shape with the id at +0x200 and the const
// GetObjectById feeding `TCastToPtr<17CScriptPathCamera>` (0x8009952C). Same repeated `GetActive()`
// guard as SetSurfaceCamera.
void CCameraManager::SetPathCamera(TUniqueId uid, CStateManager& mgr) {
  if (mPathCamera != nullptr &&
      !(mPathCamera->GetActive() &&
        (!mPathCamera->GetActive() || mPathCamera->GetScriptCameraId() == uid))) {
    if (TCastToConstPtr< CScriptPathCamera >(mgr.GetObjectById(uid))) {
      mPathCamera->SetActive(true);
      mPathCamera->SetScriptCameraId(uid);
      mPathCamera->Reset(GetCurrentCameraTransform(mgr, false), mgr);
      UpdateCameraTriggers(mPathCamera->GetUniqueIdRef(), mgr);
    }
  }
}

void CCameraManager::ClearPathCamera() {
  mPathCamera->SetActive(false);
  mPathCamera->SetScriptCameraId(kInvalidUniqueId);
}

void CCameraManager::SetSpindleCamera(TUniqueId uid, CStateManager& mgr) {
  // Retail 0x801AB794. The guard repeats `GetActive()` on purpose: MWCC then emits one branch per
  // operand off a single CSE'd load+mask, which is retail's second, unreachable `beq` (see
  // SetSurfaceCamera for the measurement).
  if (!(mSpindleCamera->GetActive() &&
        (!mSpindleCamera->GetActive() || mSpindleCamera->GetSpindleCameraId() == uid))) {
    if (TCastToPtr< CScriptSpindleCamera >(mgr.ObjectById(uid))) {
      mSpindleCamera->SetActive(true);
      mSpindleCamera->SetSpindleCameraId(uid);
      mSpindleCamera->Reset(GetCurrentCameraTransform(mgr, false), mgr);
      UpdateCameraTriggers(mSpindleCamera->GetUniqueIdRef(), mgr);
    }
  }
}

void CCameraManager::ClearSpindleCamera() {
  mSpindleCamera->SetActive(false);
  mSpindleCamera->SetSpindleCameraId(kInvalidUniqueId);
}

// Retail 0x801AB674, 208 bytes. Takes the transform as a parameter, so there is no
// GetCurrentCameraTransform call and the frame is 32 bytes. The id write is the out-of-line setter
// at 0x80228910 (see CFixedCamera.hpp).
void CCameraManager::SetFixedCamera(TUniqueId uid, const CTransform4f& xf, CStateManager& mgr) {
  if (!(mFixedCamera->GetActive() &&
        (!mFixedCamera->GetActive() || mFixedCamera->GetScriptCameraId() == uid))) {
    mFixedCamera->SetActive(true);
    mFixedCamera->SetScriptCameraId(uid);
    mFixedCamera->Reset(xf, mgr);
    UpdateCameraTriggers(mFixedCamera->GetUniqueIdRef(), mgr);
  }
}

void CCameraManager::ClearFixedCamera() {
  // Measured 2026-09-30 at retail 0x801AB640: the whole body is the virtual
  // CGameCamera::SetActive(vtable+0x1C) on the fixed camera at +0x38.
  mFixedCamera->SetActive(false);
}

// Retail 0x801AB53C, 260 bytes. Unlike SetSpindleCamera and SetPathCamera this one *does* null-test
// the surface camera, and the test on the id is `==` rather than `!=`, i.e. it bails out when the
// camera is already active on this id. The script-actor cast target is
// `TCastToPtr<10CUnknown85>__FP7CEntity` (0x80098E9C) - retail entity type 85, the placeholder
// TypesMatch.cpp already calls `CUnknown85`. `Reset` goes through vtable+0x80 and takes the
// transform *by const reference* at 20(r1), which is why it is the same address the
// `GetCurrentCameraTransform` call returns into.
void CCameraManager::SetSurfaceCamera(TUniqueId uid, CStateManager& mgr) {
  if (mSurfaceCamera != nullptr &&
      !(mSurfaceCamera->GetActive() &&
        (!mSurfaceCamera->GetActive() || mSurfaceCamera->GetScriptCameraId() == uid))) {
    if (TCastToConstPtr< CUnknown85 >(mgr.GetObjectById(uid))) {
      mSurfaceCamera->SetActive(true);
      mSurfaceCamera->SetScriptCameraId(uid);
      mSurfaceCamera->Reset(GetCurrentCameraTransform(mgr, false), mgr);
      UpdateCameraTriggers(mSurfaceCamera->GetUniqueIdRef(), mgr);
    }
  }
  // 96.82%, not 100%: one instruction. Retail emits a **second, unreachable `beq` to the epilogue**
  // on the same `rlwinm.` condition, immediately after the one that enters the body
  // (`beq 0x801AB58C` then `beq 0x801AB624`); MWCC emits one branch where retail emits two. This is
  // the same dead branch run 2 measured on `SetSpindleCamera` (96.68%) and `SetFixedCamera`. Tried
  // here, all 60 instructions and all identical codegen apart from the missing branch: the
  // `cam != nullptr && (!GetActive() || id != uid)` guard (shipped), two separate early returns
  // (`if (cam == nullptr) return;` then `if (GetActive() && id == uid) return;`), and the id test
  // hoisted ahead of the active test - which reorders the two `lhz`/`cmplw` and is worse. Do not
  // retry these.
}

// Retail 0x801AB4E8, 84 bytes: the virtual CGameCamera::SetActive(false) on the camera at +0x34,
// then the **out-of-line** script-id setter at 0x801E95A8 - unlike ClearPathCamera and
// ClearSpindleCamera, whose 0x200 stores are inline. The 0xFFFFFF9C constant is `kInvalidUniqueId`
// read from `.sdata` at r13-27740, and the `li r4,0` is hoisted above the frame, which is why the
// false is not materialised next to the call. The setter is declared and not defined (see
// CSurfaceCamera.hpp); the port-side definition is in PortGlobals.cpp.
void CCameraManager::ClearSurfaceCamera() {
  mSurfaceCamera->SetActive(false);
  mSurfaceCamera->SetScriptCameraId(kInvalidUniqueId);
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

// Retail 0x801AAC08, 32 bytes: a 0x10 frame, `mflr`/`stw lr`, one `bl` with `this` in r3 and the
// source reference still in r4, then the epilogue. Both `fn_801AABD0` and `fn_801AD8DC` call it to
// construct one `CRayCastResult` in place, so it is this vector's out-of-line element constructor.
//
// The element type is `CRayCastResult`, not `CTransform4f`: all four `fn_801AABD0` call sites in
// this unit (below) hand it a `RayWorldIntersection` return or a `MakeInvalid__14CRayCastResultFv`
// result, and `fn_80034D88` - retail's callee here - is `rstl::construct_impl<CRayCastResult>`,
// whose body is the null test on `dest` followed by `bl __ct__14CRayCastResultFRC14CRayCastResult`
// (tools/dis.sh 0x80034D88 0x40). The two classes happen to be the same 0x30 bytes wide, which is
// why the shape below is identical either way.
//
// The call target is that `construct_impl`, which mwcceppc keeps out of line (`include/rstl/
// construct.hpp` declares it before defining it for that reason), so the frame, the call and the
// epilogue are reproduced exactly - the emitted `bl` carries `fn_80034D88`, retail's own name for it.
//
// The host has no such out-of-line copy and needs none: GCC inlines `construct_impl<CRayCastResult>`
// here too, so `build-port-link`'s `CCameraManager.cpp.o` references no `construct_impl` symbol with
// or without the guard (measured both ways). The branch is spelled out anyway so the host states the
// placement new rather than depending on that inlining. Same operation either way.
extern "C" void fn_801AAC08(CRayCastResult* self, const CRayCastResult& other) {
#ifdef TARGET_PC
  new (self) CRayCastResult(other);
#else
  rstl::construct_impl< CRayCastResult >(self, other);
#endif
}

// Retail 0x801AABD0, 56 bytes: `mCount++` into 4(r3), `mItems` read from 12(r3), the slot address
// scaled by 0x30 (sizeof CRayCastResult) and constructed through `fn_801AAC08`. `self` is in r3 and
// the source reference is still in r4 - which is why the two temporaries start at r5 and r6 rather
// than r4 and r5.
//
// The placement shape is measured from all four of its call sites in this unit (0x801AA960,
// 0x801AA9C4, 0x801AA9DC, 0x801AA9F0), which are each an `addi r3,<slot>` / `addi r4,<source>` /
// `bl` triple: it constructs one element into an empty container rather than copying a whole one.
// Those four sites are what type the container: two push a `CStateManager::RayWorldIntersection`
// return straight in and two push a `MakeInvalid__14CRayCastResultFv` result, so it is
// `rstl::vector<CRayCastResult>`.
//
// Declared between `fn_801AAC28` and `CheckSplineCollision` because mwcceppc emits definitions in
// reverse source order and 0x801AABD0 sits between those two retail addresses
// (`tools/check_decl_order.py`; placing it at the foot of the file permutes the unit).
extern "C" void fn_801AABD0(rstl::vector< CRayCastResult >* self, const CRayCastResult& other) {
  fn_801AAC08(&self->mItems[self->mCount++], other);
}

bool CCameraManager::CheckSplineCollision(const CMotionSpline& spline, int mode,
                                          const CMaterialFilter& filter, CStateManager& mgr,
                                          CMaterialList& hitMaterial, float step,
                                          float thickness) const {
  // TODO: sample the motion spline and perform the selected raycast/obstruction/thickness test.
  return false;
}
