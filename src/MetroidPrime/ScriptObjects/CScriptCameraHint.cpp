#include "MetroidPrime/ScriptObjects/CScriptCameraHint.hpp"

#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPathCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp"
#include "MetroidPrime/TCastTo.hpp"

CCameraOverrideInfo::CCameraOverrideInfo(
    uint flags, uint overrideFlags, CBallCamera::EBallCameraBehaviour behaviour, float minDist,
    float maxDist, float backwardsDist, const CVector3f& lookAtOffset, const CVector3f& worldOffset,
    float fov, float attitudeRange, float azimuthRange, float anglePerSecond, float elevation,
    float interpolateOnTime, float interpolateOffTime, float controlInterpDur,
    int interpolateOnType, int interpolationMode, int interpolateOffType)
: mFlags(flags)
, mOverrideFlags(overrideFlags)
, mBehaviour(behaviour)
, mMinDist(minDist)
, mMaxDist(maxDist)
, mBackwardsDist(backwardsDist)
, mLookAtOffset(lookAtOffset)
, mWorldOffset(worldOffset)
, mFov(fov)
, mAttitudeRange(attitudeRange)
, mAzimuthRange(azimuthRange)
, mAnglePerSecond(anglePerSecond)
, mElevation(elevation)
, mInterpolateOnTime(interpolateOnTime)
, mInterpolateOffTime(interpolateOffTime)
, mControlInterpDur(controlInterpDur)
, mInterpolateOnType(interpolateOnType)
, mInterpolationMode(interpolationMode)
, mInterpolateOffType(interpolateOffType) {}

// The two `SCallback` arguments are spelled `SCallback(SCallback())` on purpose. Retail's object
// materialises each argument's SCallback *and* a copy of it (two memsets, then six word copies per
// argument, at 0x1c/0x34 and 0x4c/0x64 in the retail disassembly), which is what copy-initialising
// a temporary from a temporary produces. A plain `SCCallback()` is constructed straight into the
// argument slot: 604 bytes instead of 700, and the function stops at 81.82%. Adding a user-declared
// copy constructor to SCallback instead scores 71.37% and does not reproduce the copy.
CScriptCameraHint::CScriptCameraHint(TUniqueId uid, const rstl::string& name,
                                     const CEntityInfo& info, const CTransform4f& xf, int priority,
                                     float timer, CBallCamera::EBallCameraBehaviour behaviour,
                                     uint flags, uint overrideFlags, float minDist, float maxDist,
                                     float backwardsDist, const CVector3f& lookAtOffset,
                                     const CVector3f& worldOffset, float fov, float attitudeRange,
                                     float azimuthRange, float anglePerSecond, float elevation,
                                     float interpolateOnTime, float interpolateOffTime,
                                     float controlInterpDur, int interpolateOnType,
                                     int interpolationMode, int interpolateOffType, int acrossAreas)
: CGameHint(uid, name, info, xf, priority, timer, acrossAreas, kBHT_None, 0, 0, 0.f, SCallback(),
            SCallback(), 0.f)
, mOverrideInfo(flags, overrideFlags, behaviour, minDist, maxDist, backwardsDist, lookAtOffset,
                worldOffset, fov, attitudeRange, azimuthRange, anglePerSecond, elevation,
                interpolateOnTime, interpolateOffTime, controlInterpDur, interpolateOnType,
                interpolationMode, interpolateOffType)
, mDelegatedCameraId(kInvalidUniqueId)
, mCameraTargetId(kInvalidUniqueId)
, mOrigXf(xf) {}

CScriptCameraHint::~CScriptCameraHint() {}

void CScriptCameraHint::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  const TUniqueId sender = msg.GetUnk();
  // Retail's `li r26,0` sits on the branch-taken side of the CPlayer test (`.L_800B8154`), not
  // before the call, so the initialisation is in the `else` rather than at the declaration.
  uint playerIndex;
  if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(msg.GetOriginator()))) {
    playerIndex = mgr.MaskUIdNumPlayers(msg.GetOriginator());
  } else {
    playerIndex = 0;
  }
  if (const CGameCamera* camera =
          TCastToConstPtr< CGameCamera >(mgr.GetObjectById(msg.GetOriginator()))) {
    playerIndex = camera->GetControllerNumber();
  }

  if (playerIndex < mgr.GetNumPlayers()) {
    CHintManager* hints = mgr.CameraManager(playerIndex)->HintManager();
    switch (message) {
    case kSM_Deactivate:
    case kSM_XDelete:
    case kSM_SetToZero:
      hints->ForceRemoveHint(GetUniqueId(), mgr, kInvalidUniqueId);
      break;
    default:
      break;
    }

    if (GetActive()) {
      switch (message) {
      case kSM_Increment:
        hints->AddHint(GetUniqueId(), sender, mgr);
        break;
      case kSM_Decrement:
        hints->RemoveHint(GetUniqueId(), sender, mgr);
        break;
      default:
        break;
      }
    }

    // Retail tests the message with lis/addi/cmpw plus a taken and a not-taken branch, which is
    // what a switch lowers to here; spelling the same test as `message == kSM_Follow` in an
    // if-condition gets the two-instruction subis/cmplwi idiom instead, 8 bytes short of retail.
    switch (message) {
    case kSM_Follow: {
      if (!GetActive()) {
        SetActive(true);
      }
      if (const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(sender))) {
        CVector3f direction = mOrigXf.GetTranslation() - actor->GetTranslation();
        direction.SetZ(0.f);
        if (direction.CanBeNormalized()) {
          direction.Normalize();
        } else {
          direction = actor->GetTransform().GetColumn(kDY);
        }
        CVector3f position = actor->GetTranslation();
        position.SetZ(mOrigXf.GetTranslation().GetZ());
        SetTransform(CTransform4f::LookAt(position, position + direction));

        // Also a switch, for the same lis/addi/cmpw reason as `kSM_Follow` above. The cast is
        // `TCastToPtr< CScriptSpindleCamera >`, not `TryCast(..., kET_ScriptSpindleCamera)`: retail
        // calls the TCastToPtr specialisation and its `cmplwi r3,0` has no `li r4` type argument, so
        // the TryCast spelling is one instruction longer and moves the whole following block.
        switch (mOverrideInfo.GetBehaviourType()) {
        case CBallCamera::kBCB_Unknown8: {
          // The script spindle actor is distinct from the runtime CSpindleCamera.
          if (CActor* camera = TCastToPtr< CScriptSpindleCamera >(
                  mgr.ObjectById(mDelegatedCameraId))) {
            camera->SetTransform(GetTransform());
          }
          break;
        }
        default:
          break;
        }
      }
      hints->AddHint(GetUniqueId(), sender, mgr);
      break;
    }
    default:
      break;
    }
  }

  if (message == kSM_XALD) {
    mDelegatedCameraId = FindConnectedObject(mgr, kSS_Connect, kSM_Attach);
    mCameraTargetId = FindConnectedObject(mgr, kSS_CameraTarget, kSM_Attach);
  }
  CGameHint::AcceptScriptMsg(mgr, msg);
}

void CScriptCameraHint::SetPathCameraPosition(const CVector3f& position, CStateManager& mgr) const {
  if (CScriptPathCamera* camera =
          TCastToPtr< CScriptPathCamera >(mgr.ObjectById(mDelegatedCameraId))) {
    camera->TranslateSplines(position);
  }
}
