#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"

#include "Kyoto/Math/CloseEnough.hpp"

#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"

/**
 * Retail's own merged `.rodata` string pool, `lbl_803AA5B0` (0x28 bytes,
 * `config/G2ME01/symbols.txt`). It reads `??(??)\0WaterSheets\0First Person Camera\0`,
 * so the ctor's name literal starts 19 bytes into the label and retail materialises
 * `lis r9,hi(lbl_803AA5B0); addi r4,r9,19`. A local literal makes our pool start at
 * offset 0 and the compiler emits `mr r4,r0` instead, which is the last 4 bytes of the
 * ctor. Same bytes, same string - the reference is how this repo spells pool labels
 * (see `CConsoleOutputWindowCtor.cpp`, `CMainShutdownSubsystems.cpp`).
 */
extern "C" const char lbl_803AA5B0[];

CFirstPersonCamera::CFirstPersonCamera(const TUniqueId& uid, const CTransform4f& xf,
                                       TUniqueId watchedId, float orbitCameraSpeed, float fov,
                                       float nearZ, float farZ, float aspect, int index,
                                       int controllerIdx)
: CGameCamera(uid, rstl::string_l(lbl_803AA5B0 + 19),
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
  // Retail 0x801AEAAC..0x801AEAE4 is the *guard* shape, not a wrapped body: two branches to
  // the epilogue and then `bne <body> / b <epilogue>` for the HP test. Writing the early
  // return as `player == nullptr || hp <= 0.f` is what reproduces those four branches
  // (97.79 % -> 98.36 %); a wrapping `if (player && !(hp <= 0.f))` collapses them to two.
  if (player == nullptr || player->GetHealthInfo()->GetHP() <= 0.f) {
    return;
  }
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
      // Retail 0x801AEB78..0x801AEBB0 holds `0.f` in f2 across the divide-by-zero guard *and*
      // uses it as the clamp's lower bound, and the `duration == 0` arm jumps straight past
      // the clamp (0x801AEB88 `b 0x801AEBB4`) because it is already the clamp's minimum.
      // Clamping inside the non-zero arm is what lets MWCC keep one register for it; the
      // clamp around the whole ternary reloads `0.f` into f2 and misses 2 instructions.
      const float kZero = 0.f;
      const float morph =
          kZero == player->GetMorphDuration()
              ? kZero
              : CMath::Clamp(kZero, player->GetMorphTime() / player->GetMorphDuration(),
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
    xf.SetTranslation(xf.GetTranslation() + blend * (GetTranslation() - xf.GetTranslation()));
    SetTransform(xf);
  } else if (player->GetTurretState() == CPlayer::kTS_Active) {
    SetTransform(player->GetTurretTransform(mgr));
  }
  CActor::Think(dt, mgr);
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

void CFirstPersonCamera::UnkVtable84(TUniqueId, CStateManager&) {}

void CFirstPersonCamera::UnkVtable88(TUniqueId fluidId, CStateManager&) {
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
