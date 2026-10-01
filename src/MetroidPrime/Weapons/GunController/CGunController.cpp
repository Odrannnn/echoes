#include "MetroidPrime/Weapons/GunController/CGunController.hpp"

#include "Kyoto/Animation/CPASAnimParmData.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CAnimPlaybackParms.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"

CGunController::CGunController(CModelData& modelData)
: mModelData(modelData)
, mGunState(kGS_Inactive)
, mCurAnimId(-1)
, mAnimDone(true)
, mEnteredComboFire(false) {}

void CGunController::EnterFreeLook(CStateManager& mgr, int gunId, int setId) {
  if (mGunState != kGS_ComboFire && !mEnteredComboFire) {
    mCurAnimId = mFreeLook.SetAnim(*mModelData.AnimationData(), gunId, setId, 0, mgr, 0.f);
  } else {
    mFreeLook.SetLoopState(mComboFire.GetLoopState());
  }

  mGunState = kGS_FreeLook;
}

void CGunController::EnterComboFire(CStateManager& mgr, int gunId) {
  if (mGunState != kGS_FreeLook) {
    mCurAnimId = mComboFire.SetAnim(*mModelData.AnimationData(), gunId, 0, mgr, 0.f);
  } else {
    mComboFire.SetLoopState(mFreeLook.GetLoopState());
  }

  mGunState = kGS_ComboFire;
  mEnteredComboFire = true;
}

void CGunController::EnterFidget(CStateManager& mgr, int type, int gunId, int animSet) {
  mCurAnimId = mFidget.SetAnim(*mModelData.AnimationData(), type, gunId, animSet, mgr);
  mGunState = kGS_Fidget;
}

void CGunController::EnterStruck(CStateManager& mgr, float angle, bool bigStrike,
                                 bool notInFreeLook) {
  switch (mGunState) {
  case kGS_FreeLook:
    mFreeLook.SetIdle(true);
    break;
  case kGS_Inactive:
  case kGS_Fidget:
    break;
  default:
    return;
  }

  const CPASAnimParmData parms = CPASAnimParmData(
      pas::kAS_LieOnGround, CPASAnimParm::FromInt32(mFreeLook.GetGunId()),
      CPASAnimParm::FromReal32(angle), CPASAnimParm::FromBool(bigStrike),
      CPASAnimParm::FromBool(notInFreeLook));
  CAnimData& data = *mModelData.AnimationData();
  const rstl::pair< float, int > anim =
      data.GetPASDatabase().FindBestAnimation(parms, *mgr.Random(), -1);
  data.EnableLooping(false);
  data.SetAnimation(CAnimPlaybackParms(anim.second, -1, 1.f, true), false);
  mCurAnimId = anim.second;
  mGunState = bigStrike ? kGS_BigStrike : kGS_Strike;
}

// `CPASAnimParmData`'s copy constructor. Retail emits it out of line at 0x801DC820, 232 bytes,
// unnamed in `config/G2ME01/symbols.txt` because Metaforce never sees an implicitly-declared
// special member, and this unit's split (0x801DC1B8..0x801DCC68) claims it. `EnterStruck` is its
// only caller, so it is defined here rather than in a `src/Kyoto/Animation/` unit that does not
// exist; `include/Kyoto/Animation/CPASAnimParmData.hpp` is where it is declared, for the same
// reason `fn_800D042C` is declared `extern "C"` in `include/Collision/CCollisionInfo.hpp`.
//
// **It is declared here, between `EnterStruck` and `LoadFidgetAnimAsync`, because retail's order
// is `LoadFidgetAnimAsync` 0x801DC7F0, `fn_801DC820` 0x801DC820, `EnterStruck` 0x801DC908** and
// mwceppc emits definitions in reverse source order. `tools/check_decl_order.py --unit` says so
// too; the 232 bytes are byte-identical either way, so only the flip can see this.
//
// The body is `rstl::reserved_vector`'s copy constructor
// (`include/rstl/reserved_vector.hpp`), not a block move: retail counts the parms out of the
// **destination's** count (the `lwz r5,4(r3)` reload at 0x801DC838) and copies eight at a time.
// That shape is only reached because `CPASAnimParm` is registered trivially constructible in
// `include/Kyoto/Animation/CPASAnimParm.hpp`; through the placement-`new` form `construct`
// expands to "call `operator new`, test it against null, then construct", the surviving null test
// leaves a 1x loop behind a `cmplwi`/`beq`, and the function is 0x50 bytes and scores 0.00%.
CPASAnimParmData::CPASAnimParmData(const CPASAnimParmData& other)
  : mStateId(other.mStateId)
  , mParms(other.mParms) {}

void CGunController::LoadFidgetAnimAsync(CStateManager& mgr, int type, int gunId, int animSet) {
  mFidget.LoadAnimAsync(*mModelData.AnimationData(), type, gunId, animSet, mgr);
}

int CGunController::Update(float dt, CStateManager& mgr) {
  CAnimData& data = *mModelData.AnimationData();
  mAnimDone = false;
  switch (mGunState) {
  case kGS_FreeLook: {
    mAnimDone = mFreeLook.Update(data, dt, mgr);
    if (!mAnimDone || !mEnteredComboFire) {
      break;
    }
    EnterComboFire(mgr, mFreeLook.GetGunId());
    mAnimDone = false;
    break;
  }
  case kGS_ComboFire:
    mAnimDone = mComboFire.Update(data, dt, mgr);
    break;
  case kGS_Fidget:
    mAnimDone = mFidget.Update(data, dt, mgr);
    break;
  case kGS_Strike: {
    if (data.IsAnimTimeRemaining(0.001f, rstl::string_l("Whole Body"))) {
      break;
    }
    mCurAnimId = mFreeLook.SetAnim(*mModelData.AnimationData(), mFreeLook.GetGunId(),
                                   mFreeLook.GetSetId(), 0, mgr, 0.f);
    mGunState = kGS_FreeLook;
    break;
  }
  case kGS_BigStrike:
  case kGS_Unknown8:
    mAnimDone = !data.IsAnimTimeRemaining(0.001f, rstl::string_l("Whole Body"));
    break;
  case kGS_Inactive:
  case kGS_Default:
  case kGS_Idle:
    break;
  }
  if (mAnimDone) {
    mGunState = kGS_Inactive;
    mEnteredComboFire = false;
    return true;
  }
  return false;
}

void CGunController::EnterIdle(CStateManager& mgr) {
  CPASAnimParm parm = CPASAnimParm::NoParameter();
  switch (mGunState) {
  case kGS_FreeLook:
    parm = CPASAnimParm::FromEnum(1);
    mFreeLook.SetIdle(true);
    break;
  case kGS_ComboFire:
    parm = CPASAnimParm::FromEnum(1);
    mComboFire.SetIdle(true);
    break;
  default:
    return;
  }

  CAnimData& data = *mModelData.AnimationData();
  const rstl::pair< float, int > anim = data.GetPASDatabase().FindBestAnimation(
      CPASAnimParmData(pas::kAS_Locomotion, parm), *mgr.Random(), -1);
  data.EnableLooping(false);
  data.SetAnimation(CAnimPlaybackParms(anim.second, -1, 1.f, true), false);
  mCurAnimId = anim.second;
  mGunState = kGS_Idle;
  mEnteredComboFire = false;
}

void CGunController::ReturnToDefault(CStateManager& mgr, float delay, bool setState) {
  CAnimData& data = *mModelData.AnimationData();
  switch (mGunState) {
  case kGS_Strike:
    mGunState = kGS_FreeLook;
  case kGS_Idle:
    mFreeLook.SetIdle(false);
  case kGS_FreeLook:
    if (!setState) {
      mCurAnimId =
          mFreeLook.SetAnim(data, mFreeLook.GetGunId(), mFreeLook.GetSetId(), 2, mgr, delay);
      mEnteredComboFire = false;
    }
    break;
  case kGS_ComboFire:
    mCurAnimId = mComboFire.SetAnim(data, mComboFire.GetGunId(), 2, mgr, delay);
    break;
  case kGS_Fidget:
    ReturnToBasePosition(mgr);
    break;
  case kGS_BigStrike:
    mFreeLook.SetIdle(false);
    break;
  default:
    break;
  }
  if (setState) {
    mGunState = kGS_Default;
  }
}

void CGunController::Reset() {
  mAnimDone = true;
  mEnteredComboFire = false;
  mGunState = kGS_Inactive;
}

void CGunController::ReturnToBasePosition(CStateManager& mgr) {
  CAnimData& data = *mModelData.AnimationData();
  const rstl::pair< float, int > anim = data.GetPASDatabase().FindBestAnimation(
      CPASAnimParmData(pas::kAS_KnockBack), *mgr.Random(), -1);
  data.EnableLooping(false);
  data.SetAnimation(CAnimPlaybackParms(anim.second, -1, 1.f, true), false);
  mCurAnimId = anim.second;
  mEnteredComboFire = false;
}
