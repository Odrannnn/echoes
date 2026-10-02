#include "MetroidPrime/CTargetReticles.hpp"

#include "Kyoto/Basics/CCast.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CEulerAngles.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakTargeting.hpp"
#include "rstl/math.hpp"
#include <math.h>
#include <stdio.h>

// Declared here because `fn_800B2FD8` (the highest-addressed function in this unit, so
// the first definition in the file under the descending-declaration rule) calls it and
// it is defined further down.
extern "C" void fn_800B2D64(CCompoundTargetReticle::SOuterItemInfo* out,
                            const CCompoundTargetReticle::SOuterItemInfo& in);

// `fn_800B2FD8` (0x800B2FD8, 0x68): the element-by-element copy loop `fn_800B2F14`
// calls when it reallocates `mOuterBeamIconSquares`. Both range endpoints arrive as
// pointers-to-pointers, and `*last` is re-read from memory on every iteration
// (`lwz r0,0(r29)` sits inside the loop test). **The two differ in const and it
// matters:** `first` must be `T* const*` (const pointee) and `last` `T**`
// (non-const), and only that pair reloads `*last` in the loop and gets r31/r30/r29
// for it/`dst`/`last` the way retail does. Measured 7 spellings: both `T* const*`
// hoists the end pointer and scores 91.73%, both `T**` scores 92.31% with the loop
// test right but the prologue load misordered, `T* const&` and a named `out` 91.92%
// (the latter two also flip which register holds what), and a `do`-`while` 86.15%.
// The loop is rotated: the test runs before the first iteration, and the body ends
// with `++it; ++dst` so both step by 0x1C before the next `cmplw`.
extern "C" CCompoundTargetReticle::SOuterItemInfo*
fn_800B2FD8(CCompoundTargetReticle::SOuterItemInfo* const* first,
            CCompoundTargetReticle::SOuterItemInfo** last,
            CCompoundTargetReticle::SOuterItemInfo* dst) {
  CCompoundTargetReticle::SOuterItemInfo* it = *first;
  while (it != *last) {
    fn_800B2D64(dst, *it);
    ++it;
    ++dst;
  }
  return dst;
}

// `fn_800B2F14` (0x800B2F14, 0xC4): `rstl::vector<SOuterItemInfo>::reserve`, which the
// constructor calls at 0x800B2C70 to size `mOuterBeamIconSquares` to 9. It is spelled
// out here under retail's own name rather than left to the `reserve` template because
// `symbols.txt` has no name for the instantiation, so objdiff had **no partner** for
// it and scored it `None` even though the template's bytes were already
// instruction-for-instruction identical to retail's (verified with an opcode-by-opcode
// diff: every difference was a branch displacement, i.e. a relocation). Calling the
// `rstl` templates from inside is fine for the same reason - objdiff compares opcodes,
// not relocation targets. It has to be declared **after** `fn_800B2FD8`, because 0x800B2FD8
// is the higher of the two addresses and this unit emits in reverse source order.
extern "C" void fn_800B2F14(rstl::vector< CCompoundTargetReticle::SOuterItemInfo >& vec, int newSize) {
  if (newSize <= vec.mCapacity) {
    return;
  }
  CCompoundTargetReticle::SOuterItemInfo* newData;
  rstl::rmemory_allocator::allocate(newData, newSize);
  rstl::uninitialized_copy(vec.begin(), vec.end(), newData);
  rstl::destroy(vec.mItems, vec.mItems + vec.mCount);
  rstl::rmemory_allocator::deallocate(vec.mItems);
  vec.mItems = newData;
  vec.mCapacity = newSize;
}

// The five file-local helpers at the tail of this unit. `symbols.txt` has no name for
// any of them, so they are declared `extern "C"` under retail's own `fn_` name, which
// is what gives objdiff a partner for each one; the three float/int helpers below were
// previously unnamed `static` placeholders, so they emitted no symbol at all.
//
// `fn_800B2EE8` (0x800B2EE8, 0x2C) takes the orbit request as an `int` and is the
// "is this a damage/lock-break orbit" predicate the callers at 0x800AEAEC and 0x800B1144
// branch on with `clrlwi r0,r3,24`. Retail's body is a decision tree over the request
// values {5} u {8,9,10,11} - `cmpwi 8 / bge / cmpwi 5 / beq / b / cmpwi 12 / bge` - and
// both results are separate `li / blr` blocks, so it is one `switch`, not a chain.
extern "C" bool fn_800B2EE8(int request) {
  switch (request) {
  case 5:
  case 8:
  case 9:
  case 10:
  case 11:
    return true;
  default:
    return false;
  }
}

// `fn_800B2EA0` (0x800B2EA0, 0x48): the outer-beam icon's offshoot angle. Both
// literals are the same `lfs f0,-29432(r2)`, and `_SDA2_BASE_ - 29432` is
// 0x8041B0C8 = 0x5F000000 = 0.5f, so the shift and the addend are both 0.5f.
// `fmadds f1,f31,f1,f0` is `amplitude * sin(...) + 0.5f`, so the return is a
// product plus the phase shift, not a bare product.
extern "C" float fn_800B2EA0(float amplitude, float angularScale, float time) {
  return amplitude * CMath::FastSinR((time - 0.5f) * angularScale) + 0.5f;
}

// `fn_800B2E64` (0x800B2E64, 0x3C): the charge gauge's premultiplied overshoot
// offset. The divide's numerator is `_SDA2_BASE_ - 29468` = 0x8041B0A4 = 0x3F800000
// = 1.0f, the subtracted term is `_SDA2_BASE_ - 29360` = 0x8041B110 = pi, and the
// multiplier is `_SDA2_BASE_ - 29320` = 0x8041B138 = 0x40000000 = 2.0f.
// `frsp f2,f1` sits immediately after the `asin` call, so the `double` is narrowed
// before the subtract, and the narrowed value has to sit in a **named local**:
// written inline, MW allocates `pi` to f1 and `2.0f` to f0 and emits
// `fsubs f1,f1,f2 / fmuls f1,f0,f1` against retail's
// `fsubs f0,f0,f2 / fmuls f1,f1,f0` (98.00%). Measured: hoisting only the
// division into `inv` does not move it (98.00%); hoisting only the narrowed
// `asin` result does (100.00%), and hoisting both is the same code as hoisting
// the result alone.
extern "C" float fn_800B2E64(float overshoot) {
  const float a = static_cast< float >(asin(1.f / overshoot));
  return 2.f * (M_PIF - a);
}

CCompoundTargetReticle::SOuterItemInfo::SOuterItemInfo(const char* modelName)
: mModel(gpSimplePool->GetObj(modelName))
, mOffshootBaseAngle(0.f)
, mRotationAngle(0.f)
, mBaseAngle(0.f)
, mOffshootAngleDelta(0.f) {}

// `fn_800B2D84` (0x800B2D84, 0x64): the guarded member-wise copy `fn_800B2D64` and
// `fn_800B2D2C` both funnel into. The guard is `mr. r30,r3` followed by `beq` with no
// `cmp` at all, so it tests **r3 against zero** - the condition is `out != nullptr`, not
// a self-assignment test, and MW folds the null test into the `mr` because it needs r30
// anyway. The token is copied through a **call** to `CToken::CToken(const CToken&)` at
// 0x803015B4 rather than inline, and the `+8` word next to it is
// `TCachedToken<CModel>::mItem` - so this is a whole-object copy of `SOuterItemInfo`,
// not of its `TCachedToken` member alone.
extern "C" void fn_800B2D84(CCompoundTargetReticle::SOuterItemInfo* out,
                            const CCompoundTargetReticle::SOuterItemInfo& in) {
  if (out != nullptr) {
    out->mModel = in.mModel;
    out->mOffshootBaseAngle = in.mOffshootBaseAngle;
    out->mRotationAngle = in.mRotationAngle;
    out->mBaseAngle = in.mBaseAngle;
    out->mOffshootAngleDelta = in.mOffshootAngleDelta;
  }
}

// `fn_800B2D64` (0x800B2D64, 0x20) is a bare forwarder to `fn_800B2D84` - retail emits
// the out-of-line copy itself as a call rather than inlining it, so the frame, the
// `mflr`/`stw`/`lwz`/`mtlr` pair and the single `bl` are all of it.
extern "C" void fn_800B2D64(CCompoundTargetReticle::SOuterItemInfo* out,
                            const CCompoundTargetReticle::SOuterItemInfo& in) {
  fn_800B2D84(out, in);
}

// `fn_800B2D2C` (0x800B2D2C, 0x38): the `mOuterBeamIconSquares` append, called from the
// constructor's nine-iteration loop at 0x800B2CAC. Retail multiplies the **pre**-increment
// count by 0x1C, so the slot indexed is the one the `addi`/`stw` pair has just claimed -
// which is what a post-increment subscript says. Measured: spelling the count as a named
// local first (or naming the slot pointer) reuses r0 for the `+1` and reorders the
// `mulli`/`add` around the store, and neither spelling matches.
extern "C" void fn_800B2D2C(rstl::vector< CCompoundTargetReticle::SOuterItemInfo >& vec,
                            const CCompoundTargetReticle::SOuterItemInfo& in) {
  fn_800B2D64(&vec.mItems[vec.mCount++], in);
}

CCompoundTargetReticle::CCompoundTargetReticle(const CStateManager& mgr, int playerIndex)
: mPlayerIndex(playerIndex)
, mLeadingOrientation(CQuaternion::NoRotation())
, mLaggingOrientation(CQuaternion::NoRotation())
, mPreviousState(kRS_Unspecified)
, mNextState(kRS_Unspecified)
, mNoDrawTicks(0)
, mOvershootOffsetHalf(0.f)
, mPremultipliedOvershootOffset(0.f)
, mCrosshairs(gpSimplePool->GetObj("CMDL_Crosshairs"))
, mSeeker(gpSimplePool->GetObj("CMDL_Seeker"))
, mTargetFlower(gpSimplePool->GetObj("CMDL_TargetFlower"))
, mMissileBracket(gpSimplePool->GetObj("CMDL_MissileBracket"))
, mInnerBeamIcon(gpSimplePool->GetObj("CMDL_InnerBeamIcon"))
, mLockConfirm(gpSimplePool->GetObj("CMDL_LockConfirm"))
, mLockFire(gpSimplePool->GetObj("CMDL_LockFire"))
, mLockDagger(gpSimplePool->GetObj("CMDL_LockDagger0"))
, mGrapple(gpSimplePool->GetObj("CMDL_Grapple"))
, mChargeTickFirst(gpSimplePool->GetObj("CMDL_ChargeTickFirst"))
, mScanTargetCenter(gpSimplePool->GetObj("CMDL_ScanTargetCenter"))
, mScanTargetLeft(gpSimplePool->GetObj("CMDL_ScanTargetLeft"))
, mScanTargetRight(gpSimplePool->GetObj("CMDL_ScanTargetRight"))
, mChargeGauge("CMDL_ChargeGauge")
, mQuarterCurve(gpSimplePool->GetObj("TXTR_QuaterCurve"))
, mSeekerMissileLockConfirm(gpSimplePool->GetObj("CMDL_SeekerMissileLockConfirm"))
, mSeekerMissileCrosshair(gpSimplePool->GetObj("CMDL_SeekerMissileCrosshair"))
, mRadarPaintFirst(gpSimplePool->GetObj("TXTR_RadarPaint"))
, mRadarPaintSecond(gpSimplePool->GetObj("TXTR_RadarPaint"))
, mTargetId(kInvalidUniqueId)
, mNextTargetId(kInvalidUniqueId)
, mTargetPosition(CVector3f::Zero())
, mLaggingTargetPosition(CVector3f::Zero())
, mCurrentGroupInterpolated(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupA(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupB(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mCurrentGroupDuration(0.f)
, mCurrentGroupTimer(0.f)
, mNextGroupInterpolated(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupA(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupB(kInvalidUniqueId, 1.f, CVector3f::Zero(), 0.f, 1.f, true)
, mNextGroupDuration(0.f)
, mNextGroupTimer(0.f)
, mGrapplePointA(kInvalidUniqueId)
, mGrapplePointB(kInvalidUniqueId)
, mGrapplePointFactorA(0.f)
, mGrapplePointFactorB(0.f)
, mVulnerabilityTarget(kInvalidUniqueId)
, mTargetVulnerability(CDamageVulnerability::ImmuneVulnerability())
, mCrosshairsScale(0.f)
, mSeekerAngle(0.f)
, mCrosshairsDrawScale(0.f)
, x274(0.f)
, x278(0.f)
, mMissileActive(false)
, mMissileBracketTimer(0.f)
, mMissileBracketScaleTimer(0.f)
, mBeam(CPlayerState::kBI_Power)
, mChargeGaugeOvershootTimer(0.f)
, mLockOnTimer(0.f)
, x294(0.f)
, mLockFireTimer(0.f)
, mFullChargeFadeTimer(0.f)
, mScanBracketFactor(0.f)
, mScanTargetFactor(0.f)
, mBeamShot(false)
, mMissileShot(false)
, mFullyCharged(false) {
  fn_800B2F14(mOuterBeamIconSquares, 9);
  for (int i = 0; i < 9; ++i) {
    char name[64];
    sprintf(name, "CMDL_BeamSquare%d", i);
    mOuterBeamIconSquares.push_back(SOuterItemInfo(name));
  }
  mCrosshairs.Lock();
  mQuarterCurve.Lock();
  mSeeker.Lock();
  mGrapple.Lock();
  mSeekerMissileLockConfirm.Lock();
  mSeekerMissileCrosshair.Lock();
  mRadarPaintFirst.Lock();
  mRadarPaintSecond.Lock();
  // TODO: initial camera orientations, targeting-tweak overshoot and orbit-zone positions.
}

bool CCompoundTargetReticle::CheckLoadComplete() { return true; }

EReticleState CCompoundTargetReticle::GetDesiredReticleState(const CStateManager& mgr) const {
  switch (mgr.GetPlayerState(mPlayerIndex)->GetCurrentVisor()) {
  case CPlayerState::kPV_Scan:
    return kRS_Scan;
  case CPlayerState::kPV_Echo:
    return kRS_Echo;
  case CPlayerState::kPV_Combat:
    return kRS_Combat;
  case CPlayerState::kPV_Dark:
    return kRS_Dark;
  default:
    return kRS_Combat;
  }
}

void CCompoundTargetReticle::Update(float dt, const CStateManager& mgr) {
  // TODO: orientation lag, visor state transitions, token locks and group updates.
}

void CCompoundTargetReticle::UpdateCurrLockOnGroup(float dt, const CStateManager& mgr) {
  // TODO: lock-on transitions, vulnerability, beam/missile and charge state.
}

void CCompoundTargetReticle::UpdateNextLockOnGroup(float dt, const CStateManager& mgr) {
  const CPlayer* player = mgr.GetPlayer(mPlayerIndex);
  TUniqueId nextTargetId = player->GetOrbitNextTargetId();
  if (mPreviousState == kRS_Scan) {
    nextTargetId = kInvalidUniqueId;
  }

  if (nextTargetId != mNextTargetId) {
    if (kInvalidUniqueId == nextTargetId) {
      mNextGroupA = mNextGroupInterpolated;
      mNextGroupA.SetIsOrbitZoneIdlePosition(false);
      const bool lag = mPreviousState == kRS_Echo || mPreviousState == kRS_Dark;
      mNextGroupB = CTargetReticleRenderState(
          kInvalidUniqueId, 1.f, lag ? mLaggingTargetPosition : mTargetPosition, 0.f, 1.f, true);
      mNextGroupDuration = gpTweakTargeting->GetNextLockOnExitDuration();
      mNextGroupTimer = mNextGroupDuration;
      mNextTargetId = kInvalidUniqueId;
    } else {
      mNextGroupA = mNextGroupInterpolated;
      mNextGroupA.SetIsOrbitZoneIdlePosition(false);
      const float scale =
          IsGrappleTarget(nextTargetId, mgr) ? gpTweakTargeting->GetGrappleMinClampScale() : 1.f;
      mNextGroupB =
          CTargetReticleRenderState(nextTargetId, 1.f, CVector3f::Zero(), 1.f, scale, true);
      mNextGroupDuration = (kInvalidUniqueId == mNextTargetId)
                              ? gpTweakTargeting->GetNextLockOnEnterDuration()
                              : gpTweakTargeting->GetNextLockOnSwitchDuration();
      mNextGroupTimer = mNextGroupDuration;
      mNextTargetId = nextTargetId;
    }
  }

  if (mNextGroupTimer > 0.f) {
    UpdateTargetParameters(mNextGroupA, mgr);
    UpdateTargetParameters(mNextGroupB, mgr);
    mNextGroupTimer = rstl::max_val(0.f, mNextGroupTimer - dt);
    CTargetReticleRenderState::InterpolateWithClamp(
        mNextGroupA, mNextGroupInterpolated, mNextGroupB, 1.f - mNextGroupTimer / mNextGroupDuration);
  } else {
    UpdateTargetParameters(mNextGroupInterpolated, mgr);
  }
}

void CCompoundTargetReticle::UpdateOrbitZoneGroup(float dt, const CStateManager& mgr) {
  if (mTargetId == kInvalidUniqueId && mNextTargetId != kInvalidUniqueId) {
    x294 = rstl::min_val(2.f * dt + x294, 1.f);
  } else {
    x294 = rstl::max_val(x294 - 2.f * dt, 0.f);
  }

  const CPlayer* player = mgr.GetPlayer(mPlayerIndex);
  if (player->IsCrosshairsOpen() &&
      player->GetPlayerState()->GetCurrentVisor() != CPlayerState::kPV_Scan) {
    // Retail's crosshairs fade scale lives at +0x270, which this header currently calls
    // mCrosshairsDrawScale; +0x268 (mCrosshairsScale) is only ever zeroed by the ctor.
    mCrosshairsDrawScale =
        rstl::min_val(mCrosshairsDrawScale + dt / gpTweakTargeting->GetCrosshairsFadeInOutTime(), 1.f);
  } else {
    mCrosshairsDrawScale =
        rstl::max_val(mCrosshairsDrawScale - dt / gpTweakTargeting->GetCrosshairsFadeInOutTime(), 0.f);
  }
}

void CCompoundTargetReticle::Draw(const CStateManager& mgr, bool hideLockOn) const {
  if (mgr.GetPlayer(mPlayerIndex)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed &&
      !mgr.GetCameraManager(mPlayerIndex)->IsInCinematicCamera()) {
    CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
    CGraphics::SetViewPointMatrix(camXf);
    CMatrix3f rot = camXf.BuildMatrix3f();

    CGraphics::SetCullMode(kCM_None);

    if (!hideLockOn) {
      DrawCurrLockOnGroup(rot, mgr);
      DrawSeeker(rot, mgr);
      DrawCrosshairs(rot, mgr);
      DrawScanTargetGroup(rot, mgr);
      DrawNextLockOnGroup(rot, mgr);
      DrawOrbitZoneGroup(rot, mgr);
    }

    DrawGrappleGroup(rot, mgr, hideLockOn);

    CGraphics::SetCullMode(kCM_Front);
  }

  if (mNoDrawTicks > 0) {
    --mNoDrawTicks;
  }
}

void CCompoundTargetReticle::DrawGrappleGroup(const CMatrix3f& rotation, const CStateManager& mgr,
                                              bool hideLockOn) const {
  // TODO: interpolate and draw grapple candidates.
}

void CCompoundTargetReticle::DrawGrapplePoint(const CScriptGrapplePoint& point, float factor,
                                              const CStateManager& mgr, const CMatrix3f& rotation,
                                              bool zEqual) const {
  // TODO: grapple model position, scale, color and depth mode.
}

void CCompoundTargetReticle::DrawCurrLockOnGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: current-target, charge/missile, seeker-missile and radar-paint rendering.
}

void CCompoundTargetReticle::DrawSeeker(const CMatrix3f& rotation, const CStateManager& mgr) const {
  // TODO: scan/next-target seeker rendering.
}

// `mgr` is a dead parameter - retail passes it (0x800b0aa8 `mr r5,r30` ahead of the
// `bl` at 0x800b0ab0, same shape as the five sibling draw calls around it) and the
// body never reads the incoming r5 anywhere in 0x800AE33C..0x800AE4A8, so the
// declaration carries it and `config/G2ME01/symbols.txt` is renamed to the two-
// argument spelling to match.
void CCompoundTargetReticle::DrawCrosshairs(const CMatrix3f& rotation,
                                           const CStateManager& mgr) const {
  if (mNoDrawTicks <= 0 && mCrosshairsDrawScale > 0.f) {
    const_cast< TCachedToken< CModel >& >(mCrosshairs).IsLoaded();
    if (CModel* model = mCrosshairs.GetObject()) {
      const CColor& color = gpTweakTargeting->GetCrosshairsColor();
      gpRender->SetModelMatrix(CTransform4f(rotation, mTargetPosition) *
                               CTransform4f::Scale(mCrosshairsDrawScale));
      model->Draw(CModelFlags::Additive(color.WithAlphaModulatedBy(mCrosshairsDrawScale))
                      .DepthCompareUpdate(false, false));
    }
  }
}

void CCompoundTargetReticle::DrawScanTargetGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: scan target center and left/right completion brackets.
}

void CCompoundTargetReticle::DrawNextLockOnGroup(const CMatrix3f& rotation,
                                                 const CStateManager& mgr) const {
  // TODO: next-target lock-on rendering.
}

void CCompoundTargetReticle::DrawOrbitZoneGroup(const CMatrix3f& rotation,
                                                const CStateManager& mgr) const {
  // TODO: orbit-zone group rendering.
}

void CCompoundTargetReticle::UpdateTargetParameters(CTargetReticleRenderState& state,
                                                    const CStateManager& mgr) {
  const CObjectList& objects = mgr.GetObjectListById(kOL_All);
  if (const CActor* actor = TCastToConstPtr< CActor >(objects.GetObjectById(state.GetTargetId()))) {
    state.SetRadiusWorld(CalculateRadiusWorld(*actor, mgr));
    CVector3f pos = CalculatePositionWorld(*actor, mgr);
    state.SetTargetPositionWorld(pos);
  } else if (state.GetIsOrbitZoneIdlePosition()) {
    state.SetRadiusWorld(1.f);
    state.SetTargetPositionWorld((mPreviousState == kRS_Echo || mPreviousState == kRS_Dark)
                                     ? mLaggingTargetPosition
                                     : mTargetPosition);
  }
}

float CCompoundTargetReticle::CalculateRadiusWorld(const CActor& actor,
                                                   const CStateManager& mgr) const {
  // TODO: derive reticle radius from the actor's bounds. The body below is reached and
  // scores 96.11% (see docs/goal-notes/progress-unit-ctargetreticles.md), but it needs a
  // `TCastToPtr<CSandwormEye>` call for the Sandworm-eye special case, and the port has no
  // `TCastToPtr<12CSandwormEye>__FR7CEntity` to resolve it to, so landing it would raise the
  // port's undefined count and fail tools/link_check.sh --strict.
  return 1.f;
}

CVector3f CCompoundTargetReticle::CalculatePositionWorld(const CActor& actor,
                                                         const CStateManager& mgr) const {
  return mPreviousState == kRS_Scan ? actor.GetOrbitPosition(mgr)
                                    : actor.GetAimPosition(mgr, 0.f);
}

// `CalculateOrbitZoneReticlePosition` (0x800ACCFC, 0x17C). The whole shape of the function is
// the *order* MW evaluates in: retail calls `GetCurrentCamera`, then loads
// `mgr.GetPlayer(mPlayerIndex)` into r31, then calls `GetFov`, and only then
// `GetTweakPlayer`/`GetOrbitZoneHeight`. So the player pointer has to be a named local
// evaluated *between* the camera and the fov term - inlined, or declared after the fov term,
// MW moves the `GetFov` call and the score drops to 82% (measured). That one change took the
// function 81.00% -> 99.68%; the four instructions still out are a float-register swap, f2
// and f3 exchanged between the `0.5f` numerator of the `fdivs` and the `int`->`double`
// temporary of the `(float)CCast::LtoF(...)` below, and 11 spellings did not move them
// (see docs/goal-notes/progress-unit-ctargetreticles.md).
CVector3f CCompoundTargetReticle::CalculateOrbitZoneReticlePosition(const CStateManager& mgr,
                                                                    bool lag) const {
  const CGameCamera* cam = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true);
  const CPlayer* player = mgr.GetPlayer(mPlayerIndex);
  const float fovHalf = cam->GetFov() * 0.5f;
  const float halfExtY =
      CCast::LtoF(player->GetTweakPlayer()->GetOrbitZoneHeight(CPlayer::kZI_Targeting));
  float dist = 224.f / halfExtY;
  dist /= static_cast< float >(tan(fovHalf * (1.f / 360.f) * (2.f * M_PIF)));

  CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
  CVector3f fwd = camXf.GetForward();

  if (lag) {
    fwd = mLaggingOrientation.Transform(fwd);
  }

  return camXf.GetTranslation() + dist * fwd;
}

bool CCompoundTargetReticle::IsGrappleTarget(TUniqueId id, const CStateManager& mgr) {
  return TCastToConstPtr< CScriptGrapplePoint >(mgr.GetObjectById(id)) != nullptr;
}

float CCompoundTargetReticle::CalculateClampedScale(CVector3f position, float scale, float clampMin,
                                                    float clampMax, const CStateManager& mgr,
                                                    int playerIndex) {
  static const float kViewportScales[3] = {1.f, 0.8f, 0.6f};
  const float viewportScale = kViewportScales[mgr.fn_80036B6C()];
  const float scaledMin = viewportScale * clampMin;
  const float scaledMax = viewportScale * clampMax;
  const CCameraManager* camMgr = mgr.GetCameraManager(playerIndex);
  const CGameCamera& cam = *camMgr->GetCurrentCamera(mgr, true);
  CTransform4f camXf = camMgr->GetCurrentCameraTransform(mgr, true);
  CVector3f viewSpace = cam.GetTransform().TransposeMultiply(position);
  float projX1 = cam.GetPerspectiveMatrix().MultiplyOneOverW(viewSpace).GetX();
  float pixelScale =
      cam.GetPerspectiveMatrix().MultiplyOneOverW(viewSpace + CVector3f(scale, 0.f, 0.f)).GetX() -
      projX1;
  pixelScale *= 640.f * viewportScale;
  return scale * (CMath::Clamp(scaledMin, pixelScale, scaledMax) / pixelScale);
}

CTargetReticleRenderState::CTargetReticleRenderState(TUniqueId target, float radius,
                                                     CVector3f position, float factor,
                                                     float minimumViewportScale,
                                                     bool orbitZoneIdlePosition)
: mTarget(target)
, mRadius(radius)
, mPosition(position)
, mFactor(factor)
, mMinimumViewportScale(minimumViewportScale)
, mOrbitZoneIdlePosition(orbitZoneIdlePosition) {}

void CTargetReticleRenderState::InterpolateWithClamp(const CTargetReticleRenderState& a,
                                                     CTargetReticleRenderState& out,
                                                     const CTargetReticleRenderState& b, float t) {
  float t2 = CMath::Clamp(0.f, t, 1.f);
  float omt = 1.f - t2;
  out.mRadius = omt * a.mRadius + t2 * b.mRadius;
  out.mFactor = omt * a.mFactor + t2 * b.mFactor;
  out.mMinimumViewportScale = omt * a.mMinimumViewportScale + t2 * b.mMinimumViewportScale;
  out.mPosition = CVector3f::Lerp(a.mPosition, b.mPosition, t2);
  if (t2 == 1.f) {
    out.SetTargetId(b.GetTargetId());
  } else if (t2 == 0.f) {
    out.SetTargetId(a.GetTargetId());
  } else {
    out.SetTargetId(kInvalidUniqueId);
  }
}

CTargetingManager::CTargetingManager(const CStateManager& mgr, int playerIndex)
: mPlayerIndex(playerIndex), mTargetReticle(mgr, playerIndex), mOrbitPointMarker(playerIndex) {}

bool CTargetingManager::CheckLoadComplete() {
  return mTargetReticle.CheckLoadComplete() && mOrbitPointMarker.CheckLoadComplete();
}

void CTargetingManager::Update(float dt, const CStateManager& mgr) {
  mTargetReticle.Update(dt, mgr);
  mOrbitPointMarker.Update(dt, mgr);
}

// `CTargetingManager::Draw` (0x800AC744, 0x150). The two viewport locals are load-bearing in
// this order: retail loads `CGraphics::GetViewport().mHeight` (+0x0C) before `mWidth` (+0x08)
// and keeps them in f31/f30, which is the argument order `SetPerspective` then copies into
// f3/f2. It is the same pair, in the same order, as `COrbitPointMarker::Draw` (which is at
// 100%), so the two functions agree by construction rather than by coincidence. The
// `int`->`float` conversions of the two viewport fields are MW's own `xoris/lis/lfd/fsubs`
// sequence, not a reinterpret - `CCast::LtoF` is `static_cast<float>` here.
void CTargetingManager::Draw(const CStateManager& mgr, bool hideLockOn) const {
  CGraphics::SetAmbientColor(CColor::White());
  CGraphics::DisableAllLights();
  mOrbitPointMarker.Draw(mgr);
  const CGameCamera* curCam = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true);
  const CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
  CGraphics::SetViewPointMatrix(camXf);
  const float vpHeight = static_cast< float >(CGraphics::GetViewport().mHeight);
  const float vpWidth = static_cast< float >(CGraphics::GetViewport().mWidth);
  gpRender->SetPerspective(curCam->GetFov(), vpWidth, vpHeight, curCam->GetNearClipDistance(),
                           curCam->GetFarClipDistance());
  mTargetReticle.Draw(mgr, hideLockOn);
}

void CCompoundTargetReticle::Touch() const {
  if (mCrosshairs.GetObject()) {
    mCrosshairs.GetObject()->Touch(0);
  }
  if (mSeeker.GetObject()) {
    mSeeker.GetObject()->Touch(0);
  }
  if (mGrapple.GetObject()) {
    mGrapple.GetObject()->Touch(0);
  }
  if (mSeekerMissileLockConfirm.GetObject()) {
    mSeekerMissileLockConfirm.GetObject()->Touch(0);
  }
  if (mSeekerMissileCrosshair.GetObject()) {
    mSeekerMissileCrosshair.GetObject()->Touch(0);
  }
  if (mTargetFlower.GetObject()) {
    mTargetFlower.GetObject()->Touch(0);
  }
  if (mMissileBracket.GetObject()) {
    mMissileBracket.GetObject()->Touch(0);
  }
  if (mInnerBeamIcon.GetObject()) {
    mInnerBeamIcon.GetObject()->Touch(0);
  }
  if (mLockFire.GetObject()) {
    mLockFire.GetObject()->Touch(0);
  }
  if (mLockDagger.GetObject()) {
    mLockDagger.GetObject()->Touch(0);
  }
  if (mGrapple.GetObject()) {
    mGrapple.GetObject()->Touch(0);
  }
  if (mChargeTickFirst.GetObject()) {
    mChargeTickFirst.GetObject()->Touch(0);
  }
  if (mChargeGauge.mModel.GetObject()) {
    mChargeGauge.mModel.GetObject()->Touch(0);
  }
  if (mScanTargetCenter.GetObject()) {
    mScanTargetCenter.GetObject()->Touch(0);
  }
  if (mScanTargetLeft.GetObject()) {
    mScanTargetLeft.GetObject()->Touch(0);
  }
  if (mScanTargetRight.GetObject()) {
    mScanTargetRight.GetObject()->Touch(0);
  }
  for (rstl::vector< CCompoundTargetReticle::SOuterItemInfo >::const_iterator it =
           mOuterBeamIconSquares.begin();
       it != mOuterBeamIconSquares.end(); ++it) {
    if (it->mModel.GetObject()) {
      it->mModel.GetObject()->Touch(0);
    }
  }
}

void CTargetingManager::Touch() const { mTargetReticle.Touch(); }

COrbitPointMarker::COrbitPointMarker(int playerIndex)
: mPlayerIndex(playerIndex)
, mZOffset(gpTweakTargeting->GetOrbitPointZOffset())
, mCameraRelativeZ(true)
, mLagAzimuth(0.f)
, mAzimuth(0.f)
, mLagTargetPosition(CVector3f::Zero())
, mLastFreeOrbit(false)
, mInterpolationTimer(0.f)
, mCurrentTime(0.f)
, mOrbitPointModel(gpSimplePool->GetObj("CMDL_OrbitPoint")) {
  mOrbitPointModel.Lock();
}

bool COrbitPointMarker::CheckLoadComplete() { return mOrbitPointModel.IsLoaded(); }

void COrbitPointMarker::Update(float dt, const CStateManager& mgr) {
  mCurrentTime += dt;
  const CPlayer* player = mgr.GetPlayer(mPlayerIndex);
  CPlayer::EPlayerOrbitState orbitState = player->GetOrbitState();
  const CGameCamera& curCam = *mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true);

  const bool freeOrbit =
      (orbitState == CPlayer::kOS_OrbitPoint || orbitState == CPlayer::kOS_OrbitCarcass);

  if (mLastFreeOrbit != freeOrbit) {
    if (orbitState == CPlayer::kOS_OrbitPoint || orbitState == CPlayer::kOS_OrbitCarcass) {
      ResetInterpolationTimer(gpTweakTargeting->GetOrbitPointInterpolateInTime());
      mLagTargetPosition = !mCameraRelativeZ
                               ? player->GetHUDOrbitTargetPosition() + CVector3f(0.f, 0.f, mZOffset)
                               : CVector3f(player->GetHUDOrbitTargetPosition().GetX(),
                                           player->GetHUDOrbitTargetPosition().GetY(),
                                           mZOffset + curCam.GetTranslation().GetZ());
      mLagAzimuth = CMath::Deg2Rad(45.f) +
                    CEulerAngles::FromQuaternion(CQuaternion::FromMatrix(curCam.GetTransform()))
                        .GetZ();
    } else if (orbitState == CPlayer::kOS_NoOrbit) {
      ResetInterpolationTimer(gpTweakTargeting->GetOrbitPointInterpolateOutTime());
    } else {
      ResetInterpolationTimer(0.01f);
    }
    mLastFreeOrbit = !mLastFreeOrbit;
  }

  if (mInterpolationTimer > 0.f) {
    mInterpolationTimer = rstl::max_val(0.f, mInterpolationTimer - dt);
  }

  if (!mCameraRelativeZ) {
    const CVector3f orbitPos = player->GetHUDOrbitTargetPosition();
    const float targetZ = mZOffset + orbitPos.GetZ();
    const float delta = targetZ - mLagTargetPosition.GetZ();
    if (delta < 0.1f) {
      mLagTargetPosition = orbitPos + CVector3f(0.f, 0.f, mZOffset);
    } else if (delta < 0.f) {
      mLagTargetPosition = CVector3f(orbitPos.GetX(), orbitPos.GetY(),
                                    mLagTargetPosition.GetZ() - 0.1f);
    } else {
      mLagTargetPosition = CVector3f(orbitPos.GetX(), orbitPos.GetY(),
                                    mLagTargetPosition.GetZ() + 0.1f);
    }
  } else {
    mLagTargetPosition = CVector3f(player->GetHUDOrbitTargetPosition().GetX(),
                                   player->GetHUDOrbitTargetPosition().GetY(),
                                   mZOffset + player->GetHUDOrbitTargetPosition().GetZ());
  }

  if (mLastFreeOrbit) {
    const CEulerAngles euler =
        CEulerAngles::FromQuaternion(CQuaternion::FromMatrix(curCam.GetTransform()));
    const float newAzimuth = CMath::Deg2Rad(45.f) + euler.GetZ();
    const float aziDelta = newAzimuth - mAzimuth;
    if (player->GetInFreeLook()) {
      mLagAzimuth += aziDelta;
    }
    mAzimuth = newAzimuth;
  }
}

void COrbitPointMarker::Draw(const CStateManager& mgr) const {
  if ((mLastFreeOrbit || mInterpolationTimer > 0.f) && gpTweakTargeting->GetDrawOrbitPoint()) {
    const_cast< TCachedToken< CModel >& >(mOrbitPointModel).IsLoaded();
    if (mOrbitPointModel.GetObject() != nullptr) {
      const CGameCamera& curCam =
          *mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true);
      CTransform4f camXf = mgr.GetCameraManager(mPlayerIndex)->GetCurrentCameraTransform(mgr, true);
      CGraphics::SetViewPointMatrix(camXf);
      const float vpHeight = static_cast< float >(CGraphics::GetViewport().mHeight);
      const float vpWidth = static_cast< float >(CGraphics::GetViewport().mWidth);
      gpRender->SetPerspective(curCam.GetFov(), vpWidth, vpHeight, curCam.GetNearClipDistance(),
                               curCam.GetFarClipDistance());

      // Both arms hoist their quotient into a named local, and the `else` arm's
      // `scale = t` is load-bearing, not noise: retail's else arm is
      // `fdivs f0,f0,f1 / fmr f29,f0` (0x800abfc0) while its `if` arm is
      // `fdivs f1,f2,f1 / fsubs f29,f0,f1` (0x800abfa8) - the quotient is computed into a
      // scratch register and copied into `scale` only in that arm. Written inline the two
      // arms both write `scale` directly and MW puts `scale` in f30 instead of f29, which
      // also costs a 16-byte frame (336 against retail's 320) because the second int-to-double
      // conversion then needs its own temporary pair. Hoisting only the `if` arm gets the
      // register and the frame right and scores 99.41%; hoisting both is 100%.
      float scale;
      if (mLastFreeOrbit) {
        const float t = mInterpolationTimer / gpTweakTargeting->GetOrbitPointInterpolateInTime();
        scale = 1.f - t;
      } else {
        const float t = mInterpolationTimer / gpTweakTargeting->GetOrbitPointInterpolateOutTime();
        scale = t;
      }

      const CColor& color = gpTweakTargeting->GetOrbitPointModelColor();
      CTransform4f modelXf = CTransform4f::RotateZ(CRelAngle::FromRadians(mLagAzimuth));
      modelXf.ScaleBy(scale);
      modelXf.AddTranslation(mLagTargetPosition);
      gpRender->SetModelMatrix(modelXf);

      CModel* model = mOrbitPointModel.GetObject();
      model->Draw(
          CModelFlags::Additive(color.WithAlphaModulatedBy(scale)).DepthCompareUpdate(false, false));
    }
  }
}

void COrbitPointMarker::ResetInterpolationTimer(float time) { mInterpolationTimer = time; }
