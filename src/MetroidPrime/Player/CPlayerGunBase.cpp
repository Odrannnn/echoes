#include "MetroidPrime/Player/CPlayerGunBase.hpp"

#include "Kyoto/Input/CFinalInput.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorldShadow.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"

CPlayerGunBase::CPlayerGunBase(const rstl::string& name, TUniqueId playerId, const CVector3f& scale,
                               int maxSplashes)
: CEntity(kInvalidUniqueId, NullEntityInfo, name, 0)
, mTransform(CTransform4f::Identity())
, mAssistAimXf(CTransform4f::Identity())
, mScale(scale)
, mRainSplashGenerator(rs_new CRainSplashGenerator(scale, maxSplashes, 2, 0.f, 0.125f))
, mLights(8, CVector3f::Zero(), 4, 4, 0.1f, false, false, false, false)
, mPlayerUniqueId(playerId)
, mLightId(kInvalidUniqueId)
, mWorldShadow(rs_new CWorldShadow(32, 32, true))
, mCooldown(0.f)
, mSecondaryCooldown(0.f)
, mGunHolsterRemTime(0.f)
, mInputFlags(0)
, mLastInputFlags(0)
, mReleasedInputFlags(0)
, mPressedInputFlags(0)
, mFiredWeaponFlags(0)
, mGunDrawBlockCount(0)
, mChargeState(CPlayerState::kCS_Normal)
, mGunHolsterState(kGHS_Drawn)
, mSoundVolume(0x4a)
, mUnderwater(false)
, x3ae_25_(false)
, mInBigStrike(false)
, mMissileMode(false)
, mInPhazonPool(false) {}

CPlayerGunBase::~CPlayerGunBase() {}

CPlayer* CPlayerGunBase::GetPlayer(CStateManager& mgr) const {
  return TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerUniqueId));
}

CPlayer* CPlayerGunBase::GetPlayerFromAll(CStateManager& mgr) const {
  return TCastToPtr< CPlayer >(mgr.GetObjectByIdFromListAll(mPlayerUniqueId));
}

CWorldShadow* CPlayerGunBase::GetWorldShadow() { return mWorldShadow.get(); }
const CWorldShadow* CPlayerGunBase::GetWorldShadow() const { return mWorldShadow.get(); }
void CPlayerGunBase::AddGunDrawBlock() { ++mGunDrawBlockCount; }
void CPlayerGunBase::RemoveGunDrawBlock() {
  if (mGunDrawBlockCount != 0) {
    --mGunDrawBlockCount;
  }
}

void CPlayerGunBase::Reset(CStateManager& mgr) {
  const bool wasInBigStrike = mInBigStrike;
  mInBigStrike = true;
  ProcessInput(CFinalInput(), mgr);
  mInBigStrike = wasInBigStrike;
  mGunDrawBlockCount = 0;
}

void CPlayerGunBase::Update(float dt, CStateManager& mgr) {
  // Retail reads `CActor::mFluidIds` at 0x110 on the first-person camera: `lwz r4,4888(r3)` is
  // `CPlayer::mCameraManager` (0x1318), `lwz r4,24(r4)` is `CCameraManager::mFpCamera` (0x18 -
  // pinned by `IsInCinematicCamera`, which reads `mCinematicCameraId` at 0x16) and
  // `lwz r5,272(r4)` is `mFluidIds`'s first word, which this tree's `rstl::reserved_vector` puts
  // `mCount` in. The `neg`/`or` pair is the `!= 0` bool conversion, so the read is a count test:
  // `CActor::IsInFluid()`. `const_cast` is invisible in the object; `GetCameraManager()` is a
  // const accessor and `FirstPersonCamera()` is not.
  mUnderwater = const_cast< CCameraManager* >(GetPlayer(mgr)->GetCameraManager())
                    ->FirstPersonCamera()
                    ->IsInFluid();
  mFiredWeaponFlags = 0;
  if (mCooldown > 0.f) {
    mCooldown -= dt;
  }
  if (mSecondaryCooldown > 0.f) {
    mSecondaryCooldown -= dt;
  }
  // Retail tests the pointer explicitly (`lwz r3,144(r30)` / `cmplwi r3,0` / `beq`); this tree's
  // `rstl::single_ptr::operator->` does not, so the test has to be written out.
  if (mRainSplashGenerator.get() != nullptr) {
    mRainSplashGenerator->Update(dt, mgr);
  }
}

void CPlayerGunBase::ProcessInput(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Populate fire/charge/missile flags using the player's input mapping and strike gates.
}

void CPlayerGunBase::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // Retail reads `msg.m_msg` into r28 *before* the `GetPlayer` call and keeps it in a
  // callee-saved register across it (`lwz r28,8(r5)` then `bl GetPlayer`, with r28 spilled at
  // 16(r1)), so the message is pulled out into a named value first and the player second.
  // mwcceppc emits the arms in source order, and retail's are at 0x801DE160 (XCRT), 0x801DE17C
  // (XDelete), 0x801DE18C (XEPZ), 0x801DE1A0 (XXPZ).
  const EScriptObjectMessage message = msg.GetMessage();
  CPlayer* player = GetPlayer(mgr);
  switch (message) {
  case kSM_XCRT:
    mSoundVolume = player->GetSoundPan(CPlayer::kMSP_3);
    CreateGunLight(mgr);
    break;
  case kSM_XDelete:
    DeleteGunLight(mgr);
    break;
  case kSM_XEPZ:
  case kSM_XIPZ:
    mInPhazonPool = true;
    break;
  case kSM_XXPZ:
    mInPhazonPool = false;
    break;
  case kSM_XENF:
  case kSM_XEXF:
  case kSM_XINF:
  default:
    break;
  }
  CEntity::AcceptScriptMsg(mgr, msg);
}

void CPlayerGunBase::UpdateTransform(CStateManager& mgr, const CVector3f& position,
                                     const CTransform4f& rotation, CTransform4f& result) {
  // TODO: Apply the holster-state quaternion rotation and update mTransform.
}

void CPlayerGunBase::UpdateGunHolster(const CFinalInput& input, CStateManager& mgr) {
  // TODO: Resolve scan/cinematic/input transitions and advance drawing/holstering timers.
}

void CPlayerGunBase::DrawGun(CStateManager& mgr) {
  if (mGunHolsterState == kGHS_Holstered) {
    // `switch`, not `if`, because this is the only spelling that reproduces retail's branch pair
    // here: mwcceppc lowers a one-armed `switch` to `beq <arm>; b <end>; <arm>`, while every
    // `if` spelling tried collapses to a single `bne <end>`. Measured with tools/try_batch.py.
    const bool inCooldown = GetPlayer(mgr)->InGrappleJumpCooldown();
    switch (inCooldown) {
    case false:
      mGunHolsterState = kGHS_Drawing;
      mGunHolsterRemTime = 0.45f;
      break;
    }
  }
}

void CPlayerGunBase::HolsterGun(CStateManager& mgr) {
  // One `if` with `||`, not two `if`s and not a `switch`: retail's `cmpwi 0 / beq <end>` then
  // `cmpwi 3 / bne <body> / b <end>` (with the state word loaded once) is MWCC's lowering of the
  // disjunction, and it is the only spelling measured that reproduces it - 0 differing
  // instructions here against 2 for two `if`s and 6 for a `switch` (tools/try_batch.py).
  if (mGunHolsterState == kGHS_Holstered || mGunHolsterState == kGHS_Holstering) {
    return;
  }
  CPlayer* player = GetPlayerFromAll(mgr);
  // A morph in progress holsters fast: retail keeps the tweak's time in f1 and overwrites it with
  // .sdata2 0x8041D350 = 0.1f when the player's morph state (0x38C) is 2, `kMS_Morphing`.
  float holsterTime = gpTweakPlayerGun->GetGunHolsterTime();
  if (player->GetMorphballTransitionState() == CPlayer::kMS_Morphing) {
    holsterTime = 0.1f;
  }
  // Reversing a partial draw: retail recomputes the remaining time out of the 0.45s draw time in
  // .sdata2 0x8041D340, with the operand order (`1 - rem/draw`, then `time *`) taken from the
  // `fdivs / fsubs / fmuls` order.
  if (mGunHolsterState == kGHS_Drawing) {
    mGunHolsterRemTime = holsterTime * (1.f - mGunHolsterRemTime / 0.45f);
  } else {
    mGunHolsterRemTime = holsterTime;
  }
  mGunHolsterState = kGHS_Holstering;
  player->SetAimTarget(kInvalidUniqueId);
}

// Retail's two 12-byte leaves, `./tools/dis.sh 0x801DDF0C 0x18`:
//
//   fn_801DDF0C:  xor r0,r3,r4 ; and r3,r4,r0 ; blr      ->  b & ~a
//   fn_801DDF18:  xor r0,r3,r4 ; and r3,r3,r0 ; blr      ->  a & ~a  (with r3 as `a`)
//
// `ProcessInput` (retail 0x801DE29C) is their only caller, at 0x801DE40C and 0x801DE3FC, passing
// the pair `(mLastInputFlags, mInputFlags)`; the results are stored to `mReleasedInputFlags` (916)
// and `mPressedInputFlags` (920). So they are the released/pressed edge masks.
//
// Two details are measured, not guessed:
//
// - **`a & ~b` does not produce this code.** MWCC 2.7 lowers `& ~` to a single `andc`, 8 bytes;
//   retail's leaves are 12. Spelling the complement as `(a ^ b) & a` is what keeps the `xor`, and
//   it is byte-identical for both helpers (`.tmp/opencode/battery.py`, 12 spellings measured).
// - **`extern "C"` is required, not decoration.** dtk's target object
//   `build/G2ME01/obj/MetroidPrime/Player/CPlayerGunBase.o` carries them as *global* symbols named
//   exactly `fn_801DDF0C` / `fn_801DDF18` (retail has no name for them, so dtk synthesises one from
//   the address), and objdiff pairs a target function with the built function of the same symbol
//   name. A `static` C++ definition would be mangled to `fn_801DDF0C__FUiUi` and would never be
//   paired, so it would sit at 0% no matter how exact the bytes are.
//
// The declarations sit between the two `GetWorldShadow` overloads and `Holster` because mwcceppc
// emits definitions in reverse source order.
extern "C" uint fn_801DDF18(uint lastInputFlags, uint inputFlags) {
  return lastInputFlags & (lastInputFlags ^ inputFlags);
}
extern "C" uint fn_801DDF0C(uint lastInputFlags, uint inputFlags) {
  return inputFlags & (lastInputFlags ^ inputFlags);
}

void CPlayerGunBase::Holster(CStateManager& mgr) {
  mGunHolsterState = kGHS_Holstered;
  mGunHolsterRemTime = 0.f;
  GetPlayerFromAll(mgr)->SetAimTarget(kInvalidUniqueId);
}

void CPlayerGunBase::CreateGunLight(CStateManager& mgr) {
  // TODO: Allocate/register the gun's CGameLight and store its unique ID.
}

void CPlayerGunBase::DeleteGunLight(CStateManager& mgr) {
  if (mLightId != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mLightId);
    mLightId = kInvalidUniqueId;
  }
}
