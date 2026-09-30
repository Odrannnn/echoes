#include "MetroidPrime/BodyState/CBodyStateInfo.hpp"

#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/BodyState/CBSLocomotion.hpp"
#include "MetroidPrime/BodyState/CBSTurn.hpp"
#include "MetroidPrime/BodyState/CBodyController.hpp"

/**
 * `.text 0x800F1234..0x800F128C` - the eleven constant-returning thunks that close this unit.
 *
 * Every one of them is exactly two instructions, `li r3, <0|1>` and `blr`, and none of them
 * is called by a `bl` anywhere in the DOL. They are the `CBodyState` vtable's constant
 * implementations, emitted as weak out-of-line copies: `CBodyStateInfo`'s own `.data`
 * (`0x803B3CC0..0x803B3D60`, 160 bytes) holds the vtables of the body-state hierarchy, and
 * reading its words as a table puts `fn_800F124C/54/5C/64/6C`, `fn_800F1234`, `fn_800766CC`,
 * `fn_800F123C` and `fn_800F1244` in one of them and `fn_800F124C/54/5C/64/6C`,
 * `ApplyHeadTracking__10CBodyStateCFv`, `fn_800766CC`, `fn_800F1274` and `fn_800F127C` in
 * another. **Superseded 2026-09-29:** the slots are now determined - by reading
 * every body-state vtable's words against the relocations of `CABSIdle`'s weak copies - and ten
 * of the eleven carry their `CBodyState`/`CAdditiveBodyState` mangled names in `symbols.txt`
 * (`scope:weak`, so a flipped unit's own weak copy deduplicates against them). The bodies keep
 * the `extern "C"` spelling so the mangled name is the symbol; `fn_800F1284` is still unplaced.
 *
 * The seven that answer 0 and the four that answer 1 are the pattern `CBodyState.hpp` already
 * spells inline for the base class (`IsDead`/`IsDying`/`IsMoving`/`IsInAir`/`CanShoot`/
 * `UnkVtable2C` answer false, `ApplyGravity`/`ApplyHeadTracking`/`ApplyAnimationDeltas` answer
 * true) and `CAdditiveBodyState.hpp` for the four that answer 1. They are written here as
 * plain functions rather than as member definitions because the vtable slot each belongs to is
 * not determined: giving them to a class would add slots to every vtable in the hierarchy, and
 * a header that changes `CBodyState`'s virtual count moves bytes in units other than this one.
 */
extern "C" int fn_800F1284() { return 0; }
extern "C" int UnkVtable2C__10CBodyStateCFv() { return 0; }
extern "C" int CanShoot__10CBodyStateCFv() { return 0; }
extern "C" int ApplyGravity__10CBodyStateCFv() { return 1; }
extern "C" int IsMoving__10CBodyStateCFv() { return 0; }
extern "C" int IsDying__10CBodyStateCFv() { return 0; }
extern "C" int IsDead__10CBodyStateCFv() { return 0; }
extern "C" int IsInAir__10CBodyStateCFRC15CBodyController() { return 0; }
extern "C" int UnkVtable2C__18CAdditiveBodyStateCFv() { return 1; }
extern "C" int CanShoot__18CAdditiveBodyStateCFv() { return 1; }
extern "C" int ApplyHeadTracking__18CAdditiveBodyStateCFv() { return 1; }

CBodyStateInfo::CBodyStateInfo(CActor& actor, EBodyType type)
: mStates(29, static_cast< CBodyState* >(nullptr))
, mState(pas::kAS_Invalid)
, mAdditiveState(pas::kAS_AdditiveIdle)
, mLieOnGround(actor)
, mBodyController(nullptr)
, mMaxPitch(0.f)
, mChangeLocoAtEndOfAnimOnly(false) {
  SetupBodyStates(actor, type);
}

// Retail inlines every embedded state's destructor here rather than calling an out-of-line
// copy, so the seventeen simple states (CBSFall..CBSCover) plus CABSLoopReaction must declare
// their destructor inline in the header - a declared-but-undefined `~X() override;` makes
// mwcceppc emit an external call instead, which is 50% of this function.
CBodyStateInfo::~CBodyStateInfo() {}

void CBodyStateInfo::SetState(pas::EAnimationState state) { mState = state; }

const CBodyState* CBodyStateInfo::GetCurrentState() const { return mStates[mState]; }

CBodyState* CBodyStateInfo::GetCurrentState() { return mStates[mState]; }

bool CBodyStateInfo::ApplyHeadTracking() const {
  if (mState != pas::kAS_Invalid) {
    return GetCurrentState()->ApplyHeadTracking();
  }
  return false;
}

void CBodyStateInfo::SetAdditiveState(pas::EAnimationState state) { mAdditiveState = state; }

CAdditiveBodyState* CBodyStateInfo::GetCurrentAdditiveState() {
  return static_cast< CAdditiveBodyState* >(mStates[mAdditiveState]);
}

float CBodyStateInfo::GetMaxSpeed() const {
  float speed = GetLocomotionSpeed(pas::kLA_Run);
  if (close_enough(speed, 0.f)) {
    for (int i = 0; i <= pas::kLA_StrafeDown; ++i) {
      const float candidate = GetLocomotionSpeed(pas::ELocomotionAnim(i));
      if (candidate > speed) {
        speed = candidate;
      }
    }
  }
  return speed;
}

float CBodyStateInfo::GetLocomotionSpeed(pas::ELocomotionAnim anim) const {
  const CBSLocomotion* locomotion =
      static_cast< const CBSLocomotion* >(mStates[pas::kAS_Locomotion]);
  if (locomotion && mBodyController) {
    return locomotion->GetLocomotionSpeed(mBodyController->GetLocomotionType(), anim);
  }
  return 0.f;
}

void CBodyStateInfo::SetupBodyStates(CActor& actor, EBodyType type) {
  SetupLocomotionStates(actor, type);
  mStates[pas::kAS_Locomotion] = mLocomotion.get();
  mStates[pas::kAS_Turn] = mTurn.get();
  mStates[pas::kAS_Fall] = &mFall;
  mStates[pas::kAS_Getup] = &mGetup;
  mStates[pas::kAS_LieOnGround] = &mLieOnGround;
  mStates[pas::kAS_Step] = &mStep;
  mStates[pas::kAS_Death] = &mDie;
  mStates[pas::kAS_KnockBack] = &mKnockBack;
  mStates[pas::kAS_MeleeAttack] = &mAttack;
  mStates[pas::kAS_ProjectileAttack] = &mProjectileAttack;
  mStates[pas::kAS_LoopAttack] = &mLoopAttack;
  mStates[pas::kAS_LoopReaction] = &mLoopReaction;
  mStates[pas::kAS_GroundHit] = &mGroundHit;
  mStates[pas::kAS_Generate] = &mGenerate;
  mStates[pas::kAS_Jump] = &mJump;
  mStates[pas::kAS_Hurled] = &mHurled;
  mStates[pas::kAS_Slide] = &mSlide;
  mStates[pas::kAS_Taunt] = &mTaunt;
  mStates[pas::kAS_Scripted] = &mScripted;
  mStates[pas::kAS_Cover] = &mCover;
  mStates[pas::kAS_WallHang] = &mWallHang;
  mStates[pas::kAS_AdditiveIdle] = &mAdditiveIdle;
  mStates[pas::kAS_AdditiveAim] = &mAdditiveAim;
  mStates[pas::kAS_AdditiveFlinch] = &mAdditiveFlinch;
  mStates[pas::kAS_AdditiveReaction] = &mAdditiveReaction;
  mStates[pas::kAS_AdditiveLoopReaction] = &mAdditiveLoopReaction;
}

void CBodyStateInfo::SetupLocomotionStates(CActor& actor, EBodyType type) {
  switch (type) {
  case kBT_BiPedal:
    mLocomotion = rs_new CBSBiPedLocomotion(actor);
    mTurn = rs_new CBSTurn();
    break;
  case kBT_Flyer:
    mLocomotion = rs_new CBSFlyerLocomotion(actor, false);
    mTurn = rs_new CBSFlyerTurn();
    break;
  case kBT_Pitchable:
    mLocomotion = rs_new CBSFlyerLocomotion(actor, true);
    mTurn = rs_new CBSPitchableFlyerTurn();
    break;
  // Retail lays these three case bodies out in the order WallWalker, NewFlyer,
  // RestrictedFlyer, not in `EBodyType` order - so they are spelled that way here.
  case kBT_WallWalker:
    mLocomotion = rs_new CBSWallWalkerLocomotion(actor);
    mTurn = rs_new CBSFlyerTurn();
    break;
  case kBT_NewFlyer:
    mLocomotion = rs_new CBSAiMovedFlyerLocomotion(actor);
    mTurn = rs_new CBSTurn();
    break;
  case kBT_RestrictedFlyer:
    mLocomotion = rs_new CBSFloaterLocomotion(actor);
    mTurn = rs_new CBSTurn();
    break;
  case kBT_Blended:
    mLocomotion = rs_new CBSBlendedLocomotion(actor, 300.f);
    mTurn = rs_new CBSTurn();
    break;
  default:
    mLocomotion = rs_new CBSRestrictedLocomotion(actor);
    mTurn = rs_new CBSTurn();
    break;
  }
}
