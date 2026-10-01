#include "MetroidPrime/Player/CPlayer.hpp"

#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"

#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"

#include "MetroidPrime/HUD/CHUDMemoParms.hpp"
#include "MetroidPrime/HUD/CSamusHud.hpp"

#include "Kyoto/Text/CStringTable.hpp"

#include "rstl/string.hpp"

#include <float.h>

// libc/float.h's `FLT_MAX` is `(*(float*)__float_max)`, which makes mwcceppc materialise the
// address in r3/r4 and load through it (`lis r3 / addi r4,r3 / lfs f1,0(r4)`) instead of reading
// the constant in place. Retail does the in-place read, and retail's unit references no
// `__float_max` at all, so the constant is spelled as a literal. Same finding, same workaround,
// as `src/MetroidPrime/PathFinding/CPathFindArea.cpp:17`.
#undef FLT_MAX
#define FLT_MAX 3.402823466e+38f

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
// tools/dis.sh 0x8022B228 0xE4. Two pieces, both read straight off the disassembly:
//
// 1. While `mRezbitState` is `kRS_Infected` and `mRezbitRecoveryTimer` (0x1170) is still
//    positive, it counts the timer down by `dt`; the *first* time it reaches zero or below it
//    shows the memo and stops (the `cror eq,lt,eq` is `<= 0.f`, written as a negation so the
//    branch skips the block). Retail keeps `kRS_Infected`'s value in a `lwz`/`cmpwi`, not a
//    bit test, so the enum is an `int` - which is what the header declares.
// 2. Unconditionally afterwards, if `mStaticTimer` (0x1148) is below 0.5f it calls
//    `SetHudDisable(0.5f, 0.5f, 0.5f)` - the same 0.5f into all three arguments (`fmr f2,f1` /
//    `fmr f3,f1`), i.e. the *constant*, not the field just compared.
//
// The memo's display time is 0x7f7fffff, which retail keeps as a named `.sdata2` object
// (`lbl_8041DB14`, = FLT_MAX) rather than a literal; `src/MetroidPrime/PathFinding/CPathFindArea.cpp`
// records the same finding and the same workaround, so it is spelled as the literal here too.
void CPlayer::UpdateRezbitState(float dt) {
  if (mRezbitRecoveryTimer > 0.f && mRezbitState == kRS_Infected) {
    mRezbitRecoveryTimer -= dt;
    if (mRezbitRecoveryTimer <= 0.f) {
      // Own statement, as in BeginRezbitRecovery: retail calls `GetPlayerIndex` before it
      // builds the text, and leaves it in the parms argument mwcceppc builds the text first.
      const int playerIndex = GetPlayerIndex();
      CSamusHud::DisplayHudMemo(
          rstl::wstring_l(gpStringTable->GetString("RezbitSuitSoftwareVirus")),
          CHUDMemoParms(FLT_MAX, true, false, false, 1 << playerIndex, false));
    }
  }
  if (mStaticTimer < 0.5f) {
    SetHudDisable(0.5f, 0.5f, 0.5f);
  }
}

// tools/dis.sh 0x8022B1B0 0x78. Retail stores 2 (kRS_Recovering) into mRezbitState first,
// then shows a hint memo with an *empty* text, the float at -18608(r2) (= 0.0f, measured in
// .sdata2), clearMemoWindow=false / fadeOutOnly=true / hintMemo=false, the player mask
// `1 << GetPlayerIndex()` and fadeInText=true. So there is no string-table lookup and no
// string content in this unit at all - only the `L""` literal, which mwcceppc pools in `.sdata`.
void CPlayer::BeginRezbitRecovery() {
  mRezbitState = kRS_Recovering;
  // `GetPlayerIndex` is its own statement. Retail calls it first and parks the result in r31
  // across the `wstring_l` call; left inside the parms argument, mwcceppc hoists the `L""` pool
  // load to the top of the frame and builds the text first instead. The parms object still lands
  // at r1+8 and the text at r1+0x14, i.e. retail's slots, with no copy of either.
  const int playerIndex = GetPlayerIndex();
  CSamusHud::DisplayHudMemo(rstl::wstring_l(L""),
                            CHUDMemoParms(0.f, false, true, false, 1 << playerIndex, true));
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

// tools/dis.sh 0x8022AF0C 0x1CC. Four things are read straight off the disassembly:
//
// 1. `mControlHintManager` (this+0x14B8 = 5304) is tested first - `lwz r0,5304(r4)` /
//    `cmplwi r0,0` / `beq +0x1a8` - and when it is null the function stores `kInvalidUniqueId`
//    into its return slot (`sth r0,0(r26)`, r26 = the incoming r3) and returns. It is the same
//    guard `ResetPlayerHintState` tests at 0x8022BF88, and r3 is a pointer here because
//    `TUniqueId` is a class type, which this toolchain returns through a hidden pointer.
// 2. Otherwise it builds an `rstl::string` from "Player Hint disabled controls" -
//    `__ct__basic_string<c>(const char*, -1, allocator)` at 0x8022AF68, `li r5,-1` being the
//    `size` default. That literal is at 0x803AD348 with **no** displacement, and that is worth
//    recording: it is the *first* entry of this unit's string pool, which is the only reason
//    `UpdateRezbitState`'s `gpStringTable->GetString("RezbitSuitSoftwareVirus")` is
//    `lbl_803AD348 + 30` and needs the extra `addi r4,r4,30`. Remove this literal and that
//    function loses one instruction and drops below 100%.
// 3. It then calls `fn_8022A640` with the hint manager, the state manager, that string, a zero,
//    the control mask, a zeroed id slot, the source id, the duration and two zero floats, and
//    destroys the string afterwards (`internal_dereference` at 0x8022B060). The value the caller
//    reads back is whatever `fn_8022A640` wrote through the pointer in r3, which is the very
//    slot this function was given - hence `return fn_8022A640(...)` with the string's destructor
//    running after the call.
// 4. `breakType` is only forwarded to `fn_8022A640` through the five stack words, and those are
//    not recovered; the call below therefore stops at the register parameters and this function
//    does not match. Its 460 bytes are still mostly unaccounted for.
TUniqueId CPlayer::fn_8022af0c(CStateManager& mgr, uint controls, TUniqueId source, float duration,
                               int breakType) {
  if (mControlHintManager == nullptr) {
    return kInvalidUniqueId;
  }
  const rstl::string label("Player Hint disabled controls");
  int id = 0;
  return fn_8022A640(mControlHintManager, mgr, label, 0, controls, id, source, duration, 0.f, 0.f);
}
