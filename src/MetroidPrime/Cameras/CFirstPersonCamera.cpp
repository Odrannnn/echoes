#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"

#include "Kyoto/Math/CloseEnough.hpp"

#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"

CFirstPersonCamera::CFirstPersonCamera(const TUniqueId& uid, const CTransform4f& xf,
                                       TUniqueId watchedId, float orbitCameraSpeed, float fov,
                                       float nearZ, float farZ, float aspect, int index,
                                       int controllerIdx)
: CGameCamera(uid, rstl::string_l("First Person Camera"),
              CEntityInfo(kInvalidAreaId, NullConnectionList, true), xf, fov, nearZ, farZ, aspect,
              watchedId, index, controllerIdx)
, mOrbitCameraSpeed(orbitCameraSpeed)
, mLockCamera(false)
, mGunFollowXf(xf)
, mPitch(0.f)
, mPitchId(kInvalidUniqueId)
, mPitchTransitionTimer(0.f)
, mPendingFluidId(kInvalidUniqueId)
, mCloseInVec(CVector3f::Zero())
, mCloseInTimer(0.f)
, mInitialFov(fov)
, mDeferBallTransitionProcessing(false)
, mFluidEffectsPending(false) {}

CFirstPersonCamera::~CFirstPersonCamera() {}

void CFirstPersonCamera::ProcessInput(const CFinalInput& input, CStateManager& mgr) {}

class CUnknown42;
extern float fn_801FB6CC(const CUnknown42*, const CTransform4f&);
void CFirstPersonCamera::UpdateElevation(CStateManager& mgr) {
  mPitch = 0.f;
  if (CameraManager(mgr).IsInCinematicCamera()) {
    return;
  }
  const CPlayer* player = TCastToConstPtr< CPlayer >(mgr.GetObjectById(GetWatchedObject()));
  if (player == nullptr) {
    return;
  }
  if (mPitchId == kInvalidUniqueId) {
    return;
  }
  const CUnknown42* vol = TCastToConstPtr< CUnknown42 >(mgr.GetObjectById(mPitchId));
  if (vol == nullptr) {
    return;
  }
  mPitch = 0.0174532924f * fn_801FB6CC(vol, player->GetTransform());
}

void CFirstPersonCamera::UpdateTransform(CStateManager& mgr, float dt) {
  // TODO: recover Echoes's free-look/orbit/grapple interpolation and camera-bob composition.
  // Use shared vector/quaternion helpers; Prime's free-look damping differs here.
}

void CFirstPersonCamera::PreThink(float dt, CStateManager& mgr) {}

void CFirstPersonCamera::Render(const CStateManager& mgr) const {}

void CFirstPersonCamera::Reset(const CTransform4f& xf, CStateManager& mgr) {
  SetTransform(xf);
  SetTranslation(Player(mgr).GetEyePosition());
  mGunFollowXf = GetTransform();
  mPitchId = kInvalidUniqueId;
  mPitchTransitionTimer = 0.f;
}

void CFirstPersonCamera::SkipCinematic() {
  mCloseInVec = CVector3f::Zero();
  mCloseInTimer = 0.f;
}

void CFirstPersonCamera::Think(float dt, CStateManager& mgr) {
  CPlayer* player = TCastToPtr< CPlayer >(mgr.ObjectById(GetWatchedObject()));
  if (player && !(player->GetHealthInfo()->GetHP() <= 0.f)) {
    if (mFluidEffectsPending) {
      UpdateFluidEffects(mgr);
      mFluidEffectsPending = false;
    }
    if (!mDeferBallTransitionProcessing) {
      if (player->GetMorphballTransitionState() == CPlayer::kMS_Morphed) {
        if (player->GetCameraState() == CPlayer::kCS_Cinematic) {
          SetTransform(player->CreateTransformFromMovementDirection());
          SetTranslation(player->GetEyePosition());
        }
        return;
      } else if (player->GetMorphballTransitionState() != CPlayer::kMS_Unmorphed) {
        if (player->GetMorphballTransitionState() != CPlayer::kMS_Unmorphing) {
          return;
        }
        const float kZero = 0.f;
        const float morph = CMath::Clamp(kZero, kZero == player->GetMorphDuration()
                                                     ? kZero
                                                     : player->GetMorphTime() /
                                                           player->GetMorphDuration(),
                                         1.f);
        if (!close_enough(morph, 1.f)) {
          return;
        }
      }
    } else {
      mDeferBallTransitionProcessing = false;
    }
    if (mPitchTransitionTimer > 0.f) {
      mPitchTransitionTimer -= dt;
    }
    const CTransform4f backupXf = GetTransform();
    UpdateElevation(mgr);
    UpdateTransform(mgr, dt);
    SetTransform(ValidateCameraTransform(GetTransform(), backupXf));
    if (mCloseInTimer > 0.f) {
      mCloseInTimer -= dt;
    }
    if (player->GetTurretState() == CPlayer::kTS_Entering) {
      CTransform4f xf = player->GetTurretTransform(mgr);
      const float blend = 1.f - CMath::Clamp(0.f, player->GetTurretTimer() / 0.5f, 1.f);
      xf.SetTranslation(
          xf.GetTranslation() + blend * (GetTranslation() - xf.GetTranslation()));
      SetTransform(xf);
    } else if (player->GetTurretState() == CPlayer::kTS_Active) {
      SetTransform(player->GetTurretTransform(mgr));
    }
    CActor::Think(dt, mgr);
  }
}

const CTransform4f& CFirstPersonCamera::GetGunFollowTransform() const { return mGunFollowXf; }

void CFirstPersonCamera::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CGameCamera::AcceptScriptMsg(mgr, msg);
  switch (msg.GetMessage()) {
  case kSM_XALD:
    mPitchId = kInvalidUniqueId;
    break;
  default:
    break;
  }
}

CVector3f CFirstPersonCamera::GetScanObjectIndicatorPosition(const CStateManager& mgr) const {
  return GetTranslation() + 5.f * GetTransform().GetForward();
}

void CFirstPersonCamera::UnkVtable84() {}

void CFirstPersonCamera::UnkVtable88(TUniqueId fluidId) {
  mFluidEffectsPending = true;
  mPendingFluidId = fluidId;
}

void CFirstPersonCamera::UpdateFluidEffects(CStateManager& mgr) {
  // TODO: create the water-sheet HUD effect and play the fluid entry/exit sound for this player.
  mPendingFluidId = kInvalidUniqueId;
}

void CFirstPersonCamera::SetScriptPitchId(TUniqueId uid) {
  mPitchId = uid;
  mPitchTransitionTimer = 1.f;
}
