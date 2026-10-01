#include "MetroidPrime/Player/CPlayer.hpp"

#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"

#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"

#include "Collision/CMaterialList.hpp"

#include "Kyoto/Math/CVector3f.hpp"

// NonMatching scaffold. Definitions are in reverse target order for deferred inlining.

// Interpolates mControlDir/mControlDirFlat back from the pre-UpdatePlayerHints direction
// while the control-direction interpolation is running and Samus is morphed. The
// primitive is CVector3f::Lerp, so blend 0 keeps the fresh direction and blend 1 the old.
void CPlayer::fn_8022c338(float dt, CStateManager& mgr) {
  const CVector3f oldDirection = mControlDir;
  const CVector3f oldFlatDirection = mControlDirFlat;
  UpdatePlayerHints(mgr);
  if (mInterpolatingControlDir && mMorphBallState == kMS_Morphed) {
    mControlDirInterpTime = mControlDirInterpTime + dt;
    if (mControlDirInterpTime > mControlDirInterpDuration) {
      mControlDirInterpTime = mControlDirInterpDuration;
      ResetControlDirectionInterpolation();
    }
    const float blend = CMath::Limit(mControlDirInterpTime / mControlDirInterpDuration, 1.f);
    mControlDir = CVector3f::Lerp(oldDirection, mControlDir, blend);
    mControlDirFlat = CVector3f::Lerp(oldFlatDirection, mControlDir, blend);
  }
}

// Recomputes mControlDir/mControlDirFlat. Echoes' symbol database calls this
// UpdatePlayerHints; Prime 1 calls the same body CalculatePlayerControlDirection.
void CPlayer::UpdatePlayerHints(CStateManager& mgr) {
  if (x1268_30_) {
    if (mControlDirOverride.CanBeNormalized()) {
      mControlDir = mControlDirOverride.AsNormalized();
      mControlDirFlat = mControlDirOverride;
      mControlDirFlat.SetZ(0.f);
      if (mControlDirFlat.CanBeNormalized()) {
        mControlDirFlat.Normalize();
      } else {
        mControlDir = CVector3f(0.f, 1.f, 0.f);
        mControlDirFlat = CVector3f(0.f, 1.f, 0.f);
      }
    } else {
      mControlDir = CVector3f(0.f, 1.f, 0.f);
      mControlDirFlat = CVector3f(0.f, 1.f, 0.f);
    }
  } else {
    const CVector3f cameraToPlayer =
        GetTranslation() - GetCameraManager()->GetCurrentCamera(mgr, true)->GetTranslation();
    if (!cameraToPlayer.CanBeNormalized()) {
      mControlDir = CVector3f(0.f, 1.f, 0.f);
      mControlDirFlat = CVector3f(0.f, 1.f, 0.f);
    } else {
      CVector3f flatDirection = cameraToPlayer;
      flatDirection.SetZ(0.f);
      if (flatDirection.CanBeNormalized()) {
        if (flatDirection.Magnitude() > gpTweakBall->GetBallCameraControlDistance()) {
          mControlDir = cameraToPlayer.AsNormalized();
          if (flatDirection.CanBeNormalized()) {
            flatDirection.Normalize();
            switch (mMorphBallState) {
            case kMS_Morphed:
              mControlDirFlat = flatDirection;
              break;
            case kMS_Unmorphed:
            case kMS_Morphing:
            case kMS_Unmorphing:
              mControlDir = GetTransform().GetForward();
              mControlDirFlat = mControlDir;
              mControlDirFlat.SetZ(0.f);
              if (mControlDirFlat.CanBeNormalized()) {
                mControlDirFlat.Normalize();
              }
              break;
            }
          } else if (mMorphBallState != kMS_Morphed) {
            mControlDir = GetTransform().GetForward();
            mControlDirFlat = mControlDir;
            mControlDirFlat.SetZ(0.f);
            if (mControlDirFlat.CanBeNormalized()) {
              mControlDirFlat.Normalize();
            }
          }
        } else {
          if (mFlatMoveSpeed < 0.25f) {
            mControlDir = cameraToPlayer;
            mControlDirFlat = flatDirection;
          } else if (mMorphBallState != kMS_Morphed) {
            mControlDir = GetTransform().GetForward();
            mControlDirFlat = mControlDir;
            mControlDirFlat.SetZ(0.f);
            if (mControlDirFlat.CanBeNormalized()) {
              mControlDirFlat.Normalize();
            }
          }
        }
      }
    }
  }
}

void CPlayer::ResetPlayerHintState(CStateManager& mgr) {
  // Twelve separate byte read-modify-writes: each flag is its own `bool : 1` object, so every
  // store is its own lbz/rlwimi/stb, and it is the *sequence* of them that is observable. The
  // order below is retail's, not declaration order - see tools/dis.sh 0x8022BE74.
  x1268_26_ = true;
  x1268_27_ = true;
  x1268_28_ = true;
  x1268_30_ = false;
  x1269_28_ = false;
  x1269_30_ = false;
  x1268_29_ = false;
  x126a_31_ = false;
  x1269_31_ = false;
  x126a_24_ = false;
  x126a_25_ = false;
  x126b_31_ = false;
  GetMorphBall()->SetBoostEnabled(true);
  ResetControlDirectionInterpolation();
  RemoveMaterial(kMT_Immovable, mgr);
  if (mControlHintManager && x14bc_ != kInvalidUniqueId) {
    mControlHintManager->RemoveHint(x14bc_, GetUniqueId(), mgr);
  }
}

bool CPlayer::SetAreaPlayerHint(const CScriptPlayerHint& hint, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return false;
}

// Guessed name
bool CPlayer::FireBeamHeld(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_FireOrBomb, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_FireOrBomb2, input));
}

bool CPlayer::FireBeamPressed(const CFinalInput& input) const {
  return !!(mControlMapper.GetPressInput(CControlMapper::kC_FireOrBomb, input) ||
            mControlMapper.GetPressInput(CControlMapper::kC_FireOrBomb2, input));
}

unsigned char CPlayer::fn_8022b974(const CFinalInput& input) const {
  bool result = false;
  if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown15, input)) {
    result = true;
  }
  return result;
}

// Guessed name
bool CPlayer::JumpHeld(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_JumpOrBoost, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_JumpOrBoost2, input));
}

// Guessed name
bool CPlayer::JumpPressed(const CFinalInput& input) const {
  return !!(mControlMapper.GetPressInput(CControlMapper::kC_JumpOrBoost, input) ||
            mControlMapper.GetPressInput(CControlMapper::kC_JumpOrBoost2, input));
}

bool CPlayer::fn_8022b7f4(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_ChargeBeam, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_ChargeBeam2, input));
}

unsigned char CPlayer::fn_8022b7a8(const CFinalInput& input) const {
  bool result = false;
  if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown73, input)) {
    result = true;
  }
  return result;
}

// Counts left/right turns while the Rezbit recovery prompt is up. The recovery
// direction is 0 (neutral), 1 (left) or 2 (right); a turn only counts while the
// player is neutral or already turning the same way.
void CPlayer::UpdateRezbitRecoveryInput(const CFinalInput& input) {
  bool turnedLeft = mControlMapper.GetPressInput(CControlMapper::kC_TurnLeft, input);
  if (mControlMapper.GetPressInput(CControlMapper::kC_TurnRight, input)) {
    if (mRezbitRecoveryDirection == 1 || mRezbitRecoveryDirection == 0) {
      mRezbitRecoveryInputCount = mRezbitRecoveryInputCount + 1;
      mRezbitRecoveryDirection = 2;
    }
  }
  if (turnedLeft) {
    if (mRezbitRecoveryDirection == 2 || mRezbitRecoveryDirection == 0) {
      mRezbitRecoveryInputCount = mRezbitRecoveryInputCount + 1;
      mRezbitRecoveryDirection = 1;
    }
  }
}

// Called from CPlayer::Freeze; retail clears the two Rezbit recovery fields.
extern "C" void fn_8022B6D0(CPlayer* self) {
  self->ResetRezbitRecoveryState();
}

CPlayer::ERezbitState CPlayer::GetRezbitState() const { return mRezbitState; }

// Guessed name
void CPlayer::SetRezbitState(ERezbitState state) {
  if (state == kRS_None || state == kRS_Recovered) {
    mRezbitState = state;
  }
}

void CPlayer::StartRezbitState(CStateManager& mgr, const CRezbitEffectOptions& options) {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::UpdateRezbitState(float dt) {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::BeginRezbitRecovery() {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
// tools/dis.sh 0x8022B164 0x4C. The tail after `fn_8022EA5C` is straight-line, with one `li r4,0`
// reused by both the `mRezbitState` word store and the game-state byte store - hence `= kRS_None`
// and `= 0` rather than a second zero constant.
void CPlayer::ResetRezbitState(CStateManager& mgr) {
  fn_8022EA5C(mRezbitEffectToken, mgr, mPlayerIndex);
  mRezbitState = kRS_None;
  mRezbitEffectId = kInvalidUniqueId;
  reinterpret_cast< char* >(gpGameState)[0xD8] = 0;
}

// Guessed name
// tools/dis.sh 0x8022B0D8 0x8C. The whole body is inside the guard; both `beq`s branch to the
// epilogue, so it is `&&` and not a nested pair. `mRezbitEffectId` is loaded twice - once for the
// compare and once for the by-value `TUniqueId` temporary `DeleteObjectRequest` takes.
void CPlayer::StopRezbitState(CStateManager& mgr) {
  if (mRezbitState != kRS_None && mRezbitEffectId != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mRezbitEffectId);
    fn_8022EA5C(mRezbitEffectToken, mgr, mPlayerIndex);
    mRezbitState = kRS_None;
    mRezbitEffectId = kInvalidUniqueId;
    reinterpret_cast< char* >(gpGameState)[0xD8] = 0;
  }
}

TUniqueId CPlayer::fn_8022af0c(CStateManager& mgr, uint controls, TUniqueId source, float duration,
                               int breakType) {
  // TODO: Create a temporary control hint. Recover the shared break-hint enum.
  return kInvalidUniqueId;
}
