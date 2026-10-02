#include "MetroidPrime/Player/CPlayerGunBase.hpp"

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Input/CFinalInput.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CGameLight.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorldShadow.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"

// Retail's two unnamed readers in `auto_03_80215424_text.o`, called as
// `CPlayer::GetTweakPlayerControls()`'s return value by `UpdateGunHolster` (0x801DDAAC) at
// 0x801DDB14/0x801DDCD4 and 0x801DDB78/0x801DDBE4. Their bodies are in
// `src/MetroidPrime/PortCTweakPlayerControls.cpp`, next to `fn_80215854`/`fn_80215860`.
extern "C" bool fn_8021580C(const CTweakPlayerControls* self);
extern "C" bool fn_80215818(const CTweakPlayerControls* self);
extern "C" uint fn_801DDF18(uint, uint);
extern "C" uint fn_801DDF0C(uint, uint);
extern "C" bool fn_8022A5B4(const CHintManager*, int, CStateManager&);

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

// **The `const` overload, `GetObjectById`, not `ObjectById`** - same reason and same evidence as
// `GetPlayerFromAll` below: objdiff compares the instruction stream but not the symbol name a
// `R_PPC_REL24` names, so calling the non-const twin still scored 100%. The target object says
// `GetObjectById__13CStateManagerCF9TUniqueId`. The two accessors differ only in constness, so
// this is invisible in the bytes and only the relocation disagrees, and the const one returns a
// `const CEntity*` that has to be cast away before `TCastToPtr` will take it.
CPlayer* CPlayerGunBase::GetPlayer(CStateManager& mgr) const {
  return TCastToPtr< CPlayer >(const_cast< CEntity* >(mgr.GetObjectById(mPlayerUniqueId)));
}

// **This calls the plain `ObjectById`, not `GetObjectByIdFromListAll`** - which the name suggests
// and which the source did until this unit was linked. objdiff does not compare the *symbol name*
// of a `R_PPC_REL24` relocation, so the wrong callee still scored 100%: the instruction stream is
// identical, one `bl` either way. The two objects disagree, and the target object is the one to
// believe:
//
//   build/G2ME01/obj/…/CPlayerGunBase.o   bl -> ObjectById__13CStateManagerF9TUniqueId
//   build/G2ME01/src/…/CPlayerGunBase.o   bl -> GetObjectByIdFromListAll__13CStateManagerF9TUniqueId
//
// `tools/flip_test.sh` is what caught it, as an `undefined: 'CStateManager::GetObjectByIdFromListAll
// (TUniqueId)'` from the linker: the unit is `NonMatching`, so retail's object was satisfying that
// reference all along and the error only appears when our object replaces it. **Comparing
// relocation symbol names between the two objects is a check every `progress` item in this unit
// should have run** - see `docs/goal-notes/progress-unit-cplayergunbase.md`.
CPlayer* CPlayerGunBase::GetPlayerFromAll(CStateManager& mgr) const {
  return TCastToPtr< CPlayer >(mgr.ObjectById(mPlayerUniqueId));
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

// Retail 0x801DE29C, 404 bytes, matched instruction for instruction
// (`.tmp/opencode/sbs.py`: `differing instrs: 0 retail len 101 ours len 101`).
//
// Four things here are measured rather than guessed, and each is a *shape* MWCC produces only one
// way:
//
// - **The masks are cleared when a control hint is ACTIVE, not when there is none.** Retail's
//   `clrlwi. r0,r3,24 / beq <body>` at 0x801DE334 branches *over* the clearing block, so a true
//   `fn_8022A5B4` clears. The check asks "is a control hint active", and an active hint blocks
//   weapon input exactly as a big strike or a freeze does. Writing `!fn_8022A5B4(...)` compiles,
//   inverts the program and mirrors the branch: 76.39%.
// - **`morphed` is computed once, into a named value.** `r26 = (player->mMorphBallState == 1)` is
//   retail's `subfic / cntlzw / srwi`, after which `r28 = mInBigStrike && !r26` (bit 4 of the byte
//   at 942) and `r27 = player->GetFrozenState() && !r26` are each initialised to 0 and set to 1
//   by a branch. Inlining the morph test into both of them moves registers.
// - **Each of the last three terms introduces its condition as a *named* variable before the mask
//   it feeds.** Retail's three phi pairs are `li r6,0 / beq / li r6,4`, `li r5,0 / beq / li r5,2`
//   and `li r3,0 / beq / li r3,8`: each in a **volatile** register, and each `li rX,0` landing
//   *after* the `bl` that produced its condition. That is the whole reason for the shape. A mask
//   written as `mask = 0u; if (cond) { mask = Nu; } mInputFlags |= mask;` has a zero-init that
//   is hoistable above the call, so MWCC makes `mask` live across the `bl` and has to give it a
//   callee-saved register (r26) - which is the last 10 instructions of the difference. Pulling
//   the call into a named `const bool` first makes the zero-init depend on it, so the `li rX,0`
//   stays put and the mask becomes volatile-local. Measured with `tools/try_batch.py`: one
//   reused variable **10** differing instructions, one variable per term declared at the top
//   **28**, the named-condition form **7**.
// - **The third condition is a `const uint` compared `!= 0u`, not a `bool`.** That is the last 7
//   instructions. With a `bool`, MWCC lowers `mask = c8 ? 8u : 0u` to `clrlwi / cmplwi r0,1 /
//   neg / or / srwi.` in **r4** instead of retail's `clrlwi. r0,r3,24 / li r3,0 / beq / li r3,8`,
//   because a boolean compare against a constant is exactly what its if-conversion pass is built
//   for. Widening the condition to `uint` removes the boolean-ness, so the phi survives: 7 -> **0**.
//   The other two terms still want a `bool` - `const uint` on term 2 costs 10 - so the three
//   spellings differ deliberately rather than by accident.
//
// `mInputFlags` is a plain `= c ? 1 : 0` for the first term, which MWCC if-converts into
// `neg / or / srwi r0,r0,31`, and is read-modify-written from the member between the other three
// - which is why retail reloads 908(r29) before every `or`.
void CPlayerGunBase::ProcessInput(const CFinalInput& input, CStateManager& mgr) {
  CPlayer* player = GetPlayer(mgr);
  const bool morphed = player->GetMorphballTransitionState() == CPlayer::kMS_Morphed;
  const bool inBigStrike = mInBigStrike && !morphed;
  const bool frozen = player->GetFrozenState() && !morphed;
  // **The `const` `GetControlHintManager`**, matching retail's
  // `GetControlHintManager__7CPlayerCFv`; `player` is not a `const CPlayer*`, so overload
  // resolution picks the non-const twin on its own and the call has to be made through a const
  // pointer to pick the right one. The two differ only in constness, so this is invisible in the
  // bytes and only the relocation disagrees - see the note on `GetPlayer`.
  const CPlayer* constPlayer = player;
  if (inBigStrike || frozen || fn_8022A5B4(constPlayer->GetControlHintManager(), 1, mgr)) {
    mPressedInputFlags = 0;
    mReleasedInputFlags = 0;
    mLastInputFlags = 0;
    mInputFlags = 0;
    return;
  }
  mInputFlags = player->FireBeamHeld(input) ? 1 : 0;
  const bool chargeHeld = player->fn_8022b7f4(input);
  uint mask = 0u;
  if (chargeHeld) {
    mask = 4u;
  }
  mInputFlags |= mask;
  const bool missileOrBomb = player->GetControlMapper().GetDigitalInput(
      CControlMapper::kC_MissileOrPowerBomb, input);
  mask = 0u;
  if (missileOrBomb) {
    mask = 2u;
  }
  mInputFlags |= mask;
  // `const uint`, not `const bool` - see the note above the function.
  const uint secondaryHeld = player->fn_8022b974(input);
  mask = 0u;
  if (secondaryHeld != 0u) {
    mask = 8u;
  }
  mInputFlags |= mask;
  mReleasedInputFlags = fn_801DDF18(mLastInputFlags, mInputFlags);
  mPressedInputFlags = fn_801DDF0C(mLastInputFlags, mInputFlags);
  mLastInputFlags = mInputFlags;
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

// Retail 0x801DD78C, 800 bytes, matched instruction for instruction.
//
// Three things here are measured, not guessed:
//
// - **The arm order in the source is 1, 0, 2, 3**, because mwcceppc emits the arms in source order
//   and retail's `.text` has `kGHS_Drawing` at +0x7C, `kGHS_Holstered` at +0x148 and
//   `kGHS_Holstering` at +0x1D8, with `kGHS_Drawn` falling straight to the tail. All 24
//   permutations measured with tools/try_batch.py; 1,0,2,3 is the only one that puts the arms
//   where retail does. (`UpdateGunHolster` below is the opposite: 2,1,0,3.)
// - **`CMath::Limit` is the `-20600(r2)` idiom, not a hand-written clamp.** Retail's
//   `fabs/frsp/fcmpo/ble` + `lfs -1.0 / fsel f1,f31,f0,f1 / fmuls f31,f0,f1` is exactly
//   `CMath::Limit(v, h)` = `AbsF(v) > h ? h * Sign(v) : v`: `include/Kyoto/Math/CMath.hpp:44-52`
//   spells `Sign` as `FastFSel(v, 1.f, -1.f)` and `FastFSel` is the `fsel out, v, h, l` the header
//   already carries. With h = 1.f that is `fsel f1,f31,f0,f1` then `fmuls f31,f0,f1`.
// - **`CRelAngle`'s constructor is private**, so the angle has to go through
//   `CRelAngle::FromRadians`; `CRelAngle(angle)` does not compile. Retail materialises the 4-byte
//   result and passes its address (`addi r5,r1,16`), which is what `FromRadians` gives.
//
// The `rightVec` is read once before the switch because retail does: `lfs` of `rotation`'s
// m00/m10/m20 into a stack `CVector3f`, then the out-of-line
// `__ct__13CUnitVector3fFRC9CVector3f` (`CTransform4f::GetRight` is `CVector3f(m00, m10, m20)`).
void CPlayerGunBase::UpdateTransform(CStateManager& mgr, const CVector3f& position,
                                     const CTransform4f& rotation, CTransform4f& result) {
  const CUnitVector3f rightVec(rotation.GetRight());
  switch (mGunHolsterState) {
  case kGHS_Drawing: {
    // 0.45f is .sdata2 0x8041D340 (the draw time `DrawGun` stores), 0.01f is 0x8041D34C.
    const float t = CMath::Limit(mGunHolsterRemTime / 0.45f, 1.f);
    if (t > 0.01f) {
      const float angle = -t * gpTweakPlayerGun->GetFixedVerticalAim();
      const CQuaternion quat = CQuaternion::AxisAngle(rightVec, CRelAngle::FromRadians(angle));
      result = quat.BuildTransform4f() * rotation.GetRotation();
      result.SetTranslation(position);
    }
    break;
  }
  case kGHS_Holstered: {
    // Retail passes the tweak's angle straight through here: `bl GetFixedVerticalAim` then
    // `fneg f0,f1`, with no multiply, and the holstered state has no 0.01f cut-off.
    const float angle = -gpTweakPlayerGun->GetFixedVerticalAim();
    const CQuaternion quat = CQuaternion::AxisAngle(rightVec, CRelAngle::FromRadians(angle));
    result = quat.BuildTransform4f() * rotation.GetRotation();
    result.SetTranslation(position);
    break;
  }
  case kGHS_Drawn:
    break;
  case kGHS_Holstering: {
    float t = 1.f - CMath::Limit(mGunHolsterRemTime / gpTweakPlayerGun->GetGunHolsterTime(), 1.f);
    // A morph in progress halves the holster: the second `Limit` divides by 0.1f (.sdata2
    // 0x8041D350), the same constant `HolsterGun` uses for its morph case.
    if (GetPlayer(mgr)->GetMorphballTransitionState() == CPlayer::kMS_Morphing) {
      t = 1.f - CMath::Limit(mGunHolsterRemTime / 0.1f, 1.f);
    }
    if (t > 0.01f) {
      const float angle = -t * gpTweakPlayerGun->GetFixedVerticalAim();
      const CQuaternion quat = CQuaternion::AxisAngle(rightVec, CRelAngle::FromRadians(angle));
      result = quat.BuildTransform4f() * rotation.GetRotation();
      result.SetTranslation(position);
    }
    break;
  }
  default:
    break;
  }
  // `mTransform` is at this+0x24; every state, including `default`, lands here.
  mTransform = result;
}

// Retail 0x801DDAAC, 784 bytes, matched instruction for instruction.
//
// **Arm order in the source is 2, 1, 0, 3** (all 24 permutations measured with
// tools/try_batch.py): mwcceppc emits the arms in source order here, and retail's `.text` runs
// `kGHS_Drawn` from +0x60, `kGHS_Drawing` +0x178, `kGHS_Holstered` +0x1AC, `kGHS_Holstering` +0x2A4.
//
// **The two control ids are 18 and 28, and this header's names for them are right.** They come
// straight out of the `li r4,18` / `li r4,28` the four `CControlMapper` calls pass, and retail's
// `CControlMapper::GetDescriptionForCommand` (0x80009E74) is a jump table at 0x803B1434 indexed by
// the command, whose entries land on a string blob at 0x803A5880 - so 18 is "Missile/PowerBomb"
// and 28 is "Toggle Holster", i.e. `kC_MissileOrPowerBomb` and `kC_ToggleHolster`. Guessing
// `kC_ChargeBeam`/`kC_PlasmaBeam` by play instinct compiles, and is wrong by 2 and 1.
//
// Three more measured points:
// - The `<= 0.f` test after the countdown is `cror eq,lt,eq` + `bne` (i.e. "if not above zero,
//   holster"), while the same test spelled as the *then* clause of a two-armed `if` wants the
//   `<= 0.f` form to be written `> 0.f {countdown} else {finish}`. Both spellings are 2-instruction
//   swaps and only one of each reproduces retail.
// - The `kGHS_Drawn` arm's `else` half is spelled `else if (!(fire || missile)) { ... } else { ... }`
//   rather than `else if (fire || missile) { ... } else if (f0C) { ... }`. The two are the same
//   program; only the first puts the `GetGunNotFiringTime` tail merge after the countdown block,
//   which is where retail has it.
// - `kGHS_Holstered` calls `GetPlayer(mgr)` a second time for the morph test (retail does not
//   CSE it), so that read is written out rather than reusing the `player` above.
void CPlayerGunBase::UpdateGunHolster(const CFinalInput& input, CStateManager& mgr) {
  const float dt = input.DeltaTime();
  CPlayer* player = GetPlayer(mgr);
  switch (mGunHolsterState) {

  case kGHS_Drawn: {
    bool holster = false;
    if (fn_80215818(player->GetTweakPlayerControls())) {
      if (player->GetControlMapper().GetPressInput(CControlMapper::kC_ToggleHolster, input)) {
        holster = true;
      }
      if (!player->FireBeamHeld(input) &&
          !player->GetControlMapper().GetDigitalInput(CControlMapper::kC_MissileOrPowerBomb,
                                                      input) &&
          fn_8021580C(player->GetTweakPlayerControls())) {
        mGunHolsterRemTime -= dt;
        if (mGunHolsterRemTime <= 0.f) {
          holster = true;
        }
      }
    } else if (!(player->FireBeamHeld(input) ||
                player->GetControlMapper().GetDigitalInput(CControlMapper::kC_MissileOrPowerBomb,
                                                           input))) {
      if (fn_8021580C(player->GetTweakPlayerControls())) {
        mGunHolsterRemTime -= dt;
      }
    } else {
      mGunHolsterRemTime = gpTweakPlayerGun->GetGunNotFiringTime();
    }
    if (holster) {
      HolsterGun(mgr);
    }
    break;
  }

  case kGHS_Drawing:
    if (mGunHolsterRemTime > 0.f) {
      mGunHolsterRemTime -= dt;
    } else {
      mGunHolsterState = kGHS_Drawn;
      mGunHolsterRemTime = gpTweakPlayerGun->GetGunNotFiringTime();
    }
    break;

  case kGHS_Holstered: {
    if (GetPlayer(mgr)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
      mPressedInputFlags = 0;
      mReleasedInputFlags = 0;
      mLastInputFlags = 0;
      mInputFlags = 0;
    }
    bool draw = false;
    if (player->FireBeamHeld(input) ||
        player->GetControlMapper().GetDigitalInput(CControlMapper::kC_MissileOrPowerBomb, input) ||
        player->GetGrappleState() == CPlayer::kGS_None) {
      draw = true;
    } else if (fn_80215818(player->GetTweakPlayerControls()) &&
               player->GetControlMapper().GetPressInput(CControlMapper::kC_ToggleHolster, input)) {
      draw = true;
    }
    if (player->GetPlayerState()->GetCurrentVisor() == CPlayerState::kPV_Scan ||
        player->GetPlayerState()->GetTransitioningVisor() == CPlayerState::kPV_Scan ||
        player->GetMorphballTransitionState() != CPlayer::kMS_Unmorphed ||
        mGunDrawBlockCount != 0) {
      draw = false;
    }
    if (draw) {
      DrawGun(mgr);
    }
    break;
  }

  // No gun transform work here: retail's holstering arm only runs the countdown and flips back to
  // `kGHS_Holstered` at zero, so this stays in step with the arm above by also clearing the input
  // masks when Samus is unmorphed.
  case kGHS_Holstering:
    if (GetPlayer(mgr)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
      mPressedInputFlags = 0;
      mReleasedInputFlags = 0;
      mLastInputFlags = 0;
      mInputFlags = 0;
    }
    if (mGunHolsterRemTime > 0.f) {
      mGunHolsterRemTime -= dt;
    } else {
      mGunHolsterState = kGHS_Holstered;
    }
    break;

  default:
    break;
  }
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

// Retail's `.rodata` blob at 0x803AAC38, seven bytes, `3f 3f 28 3f 3f 29 00 00` - the same
// "??" placeholder `rs_new` passes as its `operator new` file argument (see
// `include/Kyoto/Alloc/CMemory.hpp`), but living in `auto_06_803AAC38_rodata.o` rather than in
// this unit's own `.rodata`. It is the gun light's *name* string's base: `CreateGunLight` asks
// for `+7`, which is the second NUL, i.e. the empty name. `CFirstPersonCamera.cpp` uses the same
// `rstl::string_l(<label> + N)` spelling for the same reason. The host copy of the bytes is in
// `src/MetroidPrime/PortPoolStandIns.cpp`.
extern "C" const char lbl_803AAC38[];

void CPlayerGunBase::CreateGunLight(CStateManager& mgr) {
  if (mLightId != kInvalidUniqueId) {
    return;
  }
  mLightId = mgr.AllocateUniqueId();
  const uint lightSource = mLightId.Value();
  // Retail allocates 440 bytes (`li r3,440` into `operator new(unsigned long, char const*,
  // char const*)`), and the `mr. r28,r3 / beq` pair after it is MWCC's own null check, so this
  // is a plain `rs_new` and not a hand-written one. The argument list, from the call at
  // 0x801DE064:
  //   r4  &24(r1)  = mLightId          (TUniqueId, `sth`)
  //   r5  &36(r1)  = kInvalidAreaId    (TAreaId is four bytes, so `lwz`/`stw` - the light is
  //                                    in no area)
  //   r6  0        = active = false
  //   r7  &40(r1)  = the name, `lbl_803AAC38 + 7`
  //   r8  this+36  = mTransform        (CEntity is 0x24, so mTransform is the first member)
  //   r9  &28(r1)  = mPlayerUniqueId   (parent)
  //   r10 &56(r1)  = CLight::BuildDirectional(CVector3f::sForwardVector, CColor::Black())
  //                 (`CVector3f::Forward()` is the public accessor for that same static, and
  //                 mwcceppc does not enforce the `protected` that gcc does)
  //   8(r1)        = lightSource       (`clrlwi r29,r0,22`, and `TUniqueId::Value()` is
  //                                    exactly `value & 0x3FF` - the header's own accessor.
  //                                    It has to be a named local: written inline as the eighth
  //                                    argument, MWCC emits the `lhz` before the allocation and
  //                                    the `clrlwi` inside the null-check arm - 12 differing
  //                                    instructions; hoisted, 6, and the six are all one pair
  //                                    swapped, see below)
  //   12(r1)       = 0                 (priority)
  //   f1           = 0.f               (lifetime, .sdata2 0x8041D354)
  //   16(r1)       = nullptr           (the `const CEntityInfo*`; this tree's constructor
  //                                    defaults it, and retail stores a literal 0)
  // The `rs_new` and the `AddObject` are **one full expression**, not a named local followed by
  // a second statement: retail calls `AddObject` (0x801DE074) *before* the `extsb. r0,r27 / beq`
  // pair that destroys the `rstl::string` temporary, so the temporary's scope has to reach past
  // the call, which only happens inside the expression that made it. With the pointer in a local
  // the destructor runs first and the two calls swap: 91.30%.
  //
  // The `extsb. r0,r27 / beq` pair itself is MWCC's own guard on that temporary - r27 is set once
  // the temporary exists - so it needs no spelling here.
  mgr.AddObject(rs_new CGameLight(mLightId, kInvalidAreaId, false,
                                  rstl::string_l(lbl_803AAC38 + 7), mTransform, mPlayerUniqueId,
                                  CLight::BuildDirectional(CVector3f::Forward(),
                                                           CColor::Black()),
                                  lightSource, 0, 0.f, nullptr));
}

void CPlayerGunBase::DeleteGunLight(CStateManager& mgr) {
  if (mLightId != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mLightId);
    mLightId = kInvalidUniqueId;
  }
}
