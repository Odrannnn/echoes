#include "MetroidPrime/Cameras/CGameCamera.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CCameraSpring.hpp"
#include "MetroidPrime/Cameras/CBallCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"

extern "C" void fn_801B19F8(SFovInterpolation* self, float delay, float remaining, float duration,
                            float current, float target, TUniqueId cameraId) {
  self->mDelay = delay;
  self->mRemaining = remaining;
  self->mDuration = duration;
  self->mCurrent = current;
  self->mTarget = target;
  self->mCameraId = cameraId;
}

// `fn_801B19D8` is the SFovInterpolation setter. Retail gives the body no name, so it has to be
// a free function with C linkage: a C++ member mangles to a name objdiff cannot pair with
// `fn_801B19D8`. `ResetFovInterpolation` and both `InterpolateFOV`s call exactly this one
// (`R_PPC_REL24 fn_801B19D8` in the retail object); only CGameCamera's own constructor uses the
// other copy, `fn_801B19F8`.
extern "C" void fn_801B19D8(SFovInterpolation* self, float delay, float remaining, float duration,
                            float current, float target, TUniqueId cameraId) {
  self->mDelay = delay;
  self->mRemaining = remaining;
  self->mDuration = duration;
  self->mCurrent = current;
  self->mTarget = target;
  self->mCameraId = cameraId;
}

// Retail's copy constructor is `__ct__9CMatrix4fFRC9CMatrix4f` at 0x801B1954, i.e. in *this*
// object's range rather than in `Kyoto/Math/CMatrix4f`'s, so it is defined here. Written out as
// sixteen member initialisers rather than `*this = other` because the compiler-generated
// assignment is the same 0x84 bytes of load/store pairs and a call to it would not be.
CMatrix4f::CMatrix4f(const CMatrix4f& other)
: m00(other.m00)
, m01(other.m01)
, m02(other.m02)
, m03(other.m03)
, m10(other.m10)
, m11(other.m11)
, m12(other.m12)
, m13(other.m13)
, m20(other.m20)
, m21(other.m21)
, m22(other.m22)
, m23(other.m23)
, m30(other.m30)
, m31(other.m31)
, m32(other.m32)
, m33(other.m33) {}

CGameCamera::CGameCamera(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                         const CTransform4f& xf, float fov, float nearZ, float farZ, float aspect,
                         TUniqueId watchedId, int index, int controllerIdx)
: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(kMT_NoStepLogic), CActorParameters(),
         kInvalidUniqueId)
, mWatchedObject(watchedId)
, mPerspectiveMatrix(CMatrix4f::Identity())
, mOrigXf(xf)
, mZnear(nearZ)
, mZfar(farZ)
, mAspect(aspect)
, mInputIndex(index)
, mControllerIdx(controllerIdx)
, mFovInterpolation(0.f, 0.f, 0.f, fov, fov, kInvalidUniqueId)
, mPerspDirty(true) {
  SetDrawEnabled(false);
}

CGameCamera::~CGameCamera() {}

void CGameCamera::SetAspectRatio(float aspect) {
  mAspect = aspect;
  mPerspDirty = true;
}

const CMatrix4f& CGameCamera::GetPerspectiveMatrix() const {
  if (mPerspDirty == true) {
    mPerspectiveMatrix = CGraphics::CalculatePerspectiveMatrix(GetFov(), mAspect, mZnear, mZfar);
    mPerspDirty = false;
  }
  return mPerspectiveMatrix;
}

CVector3f CGameCamera::ConvertToScreenSpace(const CVector3f& position) const {
  const CVector3f local = GetTransform().TransposeMultiply(position);
  if (local.IsNonZero()) {
    return GetPerspectiveMatrix().MultiplyOneOverW(local);
  }
  return CVector3f(-1.f, -1.f, 1.f);
}

float CMatrix4f::Determinant() const {
  const float a = m20 * m31 - m21 * m30;
  const float b = m20 * m32 - m22 * m30;
  const float c = m20 * m33 - m23 * m30;
  const float d = m21 * m32 - m22 * m31;
  const float e = m21 * m33 - m23 * m31;
  const float f = m22 * m33 - m23 * m32;

  // Group 1's three terms are `m11*f + m13*d - m12*e`, not the textbook
  // `m11*f - m12*e + m13*d`. Retail evaluates it as two `fmadds`/`fmsubs` in that order and the
  // alternative spelling is worth 22 points (55.42% -> 77.47%). Groups 2-4 were swept over the
  // same 18 reorderings each and this is already their best.
  return m00 * (m11 * f + m13 * d - m12 * e) - m01 * (m10 * f - m12 * c + m13 * b) +
         m02 * (m10 * e - m11 * c + m13 * a) - m03 * (m10 * d - m11 * b + m12 * a);
}

CMatrix4f CMatrix4f::GetInverse() const {
  // Two-by-two minors for the adjugate matrix.
  const float a = m20 * m31 - m21 * m30;
  const float b = m20 * m32 - m22 * m30;
  const float c = m20 * m33 - m23 * m30;
  const float d = m21 * m32 - m22 * m31;
  const float e = m21 * m33 - m23 * m31;
  const float f = m22 * m33 - m23 * m32;
  const float g = m10 * m31 - m11 * m30;
  const float h = m10 * m32 - m12 * m30;
  const float i = m10 * m33 - m13 * m30;
  const float j = m11 * m32 - m12 * m31;
  const float k = m11 * m33 - m13 * m31;
  const float l = m12 * m33 - m13 * m32;
  const float m = m10 * m21 - m11 * m20;
  const float n = m10 * m22 - m12 * m20;
  const float o = m10 * m23 - m13 * m20;
  const float p = m11 * m22 - m12 * m21;
  const float q = m11 * m23 - m13 * m21;
  const float r = m12 * m23 - m13 * m22;
  const float invDet = 1.f / Determinant();

  return CMatrix4f(invDet * (m11 * f - m12 * e + m13 * d), -invDet * (m01 * f - m02 * e + m03 * d),
                   invDet * (m01 * l - m02 * k + m03 * j), -invDet * (m01 * r - m02 * q + m03 * p),
                   -invDet * (m10 * f - m12 * c + m13 * b), invDet * (m00 * f - m02 * c + m03 * b),
                   -invDet * (m00 * l - m02 * i + m03 * h), invDet * (m00 * r - m02 * o + m03 * n),
                   invDet * (m10 * e - m11 * c + m13 * a), -invDet * (m00 * e - m01 * c + m03 * a),
                   invDet * (m00 * k - m01 * i + m03 * g), -invDet * (m00 * q - m01 * o + m03 * m),
                   -invDet * (m10 * d - m11 * b + m12 * a), invDet * (m00 * d - m01 * b + m02 * a),
                   -invDet * (m00 * j - m01 * h + m02 * g), invDet * (m00 * p - m01 * n + m02 * m));
}

CVector3f CGameCamera::ConvertToWorldSpace(const CVector3f& position) const {
  const CVector3f v = GetPerspectiveMatrix().GetInverse().MultiplyOneOverW(position);
  const CVector3f r = GetTransform() * v;
  return r;
}

float CCameraSpring::ApplyDistanceSpring(float target, float current, float dt) {
  float result = current + mTardis * (mDx * dt);
  const float acceleration = mK * (target - current) - mK2Sqrt * mDx;
  mDx += mTardis * (acceleration * dt);

  if (result < target) {
    result = target;
  }
  if (result - target > mMax) {
    result = target + mMax;
  }
  return result;
}

void CCameraSpring::Reset() {
  mK2Sqrt = 2.f * CMath::SqrtF(mK);
  mDx = 0.f;
}

void CGameCamera::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CActor::AcceptScriptMsg(mgr, msg);
}

void CGameCamera::SetActive(const bool active) {
  CActor::SetActive(active);
  SetDrawEnabled(false);
}

CTransform4f CGameCamera::ValidateCameraTransform(const CTransform4f& newXf,
                                                  const CTransform4f& oldXf) {
  // Retail's own body, recovered from the object; every callee here is a plain `bl`, no dispatch.
  // The return slot arrives in r3 (MWCC returns a 48-byte aggregate through a hidden pointer), so
  // r4 is `this` - unused - r5 is `newXf` and r6 is `oldXf`. The .sdata2 constants are
  // lbl_8041CDEC = 1.f, lbl_8041CDF4 = 1e-5f, lbl_8041CDF8 = -1.f, lbl_8041CDFC = 0.999f,
  // lbl_8041CE00 = 0.01f and lbl_8041CE04 = 2.f. The `fsel`/`fmuls` pair at 0x7CC is
  // `CMath::Limit`'s `h * CMath::Sign(v)` on the branch where `AbsF(v) > h` was already proven.
  //
  // Blocks 1 and 2 read `newXf` (r30) rather than the local copy, block 3 and 4 read the local,
  // and the return re-loads the translation from `newXf`; all three are visible in the object.
  //
  // NOT yet byte-exact: retail's frame is 0x120 bytes with nine 12-byte vector slots, ours is
  // 0x100 with seven, so every stack offset differs. Retail materialises two `CVector3f`
  // temporaries our compiler forwards away (`up` at 0x38 and `right` at 0x20, both written and
  // never read). The control flow, every callee and the two re-reads of `newXf` at the tail are
  // recovered; the allocation is not.
  CTransform4f xf = newXf;
  // `bge, bge, blt`: one condition of three `!(x < eps)` terms, not three `||` of `>=`.
  if (!(CMath::AbsF(newXf.GetRight().Magnitude() - 1.f) < 0.00001f &&
        CMath::AbsF(newXf.GetForward().Magnitude() - 1.f) < 0.00001f &&
        CMath::AbsF(newXf.GetUp().Magnitude() - 1.f) < 0.00001f)) {
    xf.Orthonormalize();
  }
  if (CMath::AbsF(CMath::Limit(CVector3f::Dot(newXf.GetForward(), CVector3f::Up()), 1.f)) >
      0.999f) {
    xf = oldXf;
  }
  const CVector3f up = xf.GetUp();
  // `stfs` of the forward column's z into the slot that `SetZ(0.f)` then overwrites: the flat
  // vector is a copy of `GetForward()` with its z replaced, not a fresh three-argument build.
  CVector3f flat = xf.GetForward();
  flat.SetZ(0.f);
  if (up.GetZ() < 0.01f) {
    if (!flat.CanBeNormalized()) {
      xf = oldXf;
    } else {
      xf = CTransform4f::LookAt(CUnitVector3f(CVector3f::Zero()), flat, CVector3f::Up());
    }
  }
  const CVector3f right = xf.GetRight();
  const CVector3f up2 = xf.GetUp();
  // `blt, bge` again, so both halves are spelled as negations of `<`. The body of the `if` is
  // spelled on block 3's `flat`, not on `up2`: retail passes r3 = r1+104 for `IsMagnitudeSafe` and
  // r5 = r1+104 for `LookAt`, and r1+104 is the slot `flat` was built in, while the (m02, m12, m22)
  // triple it stores at r1+20/24/28 is never read again.
  if (!(CMath::AbsF(right.GetZ()) < 2.f) && CMath::AbsF(up2.GetZ()) < 2.f) {
    if (!flat.IsMagnitudeSafe()) {
      xf = oldXf;
    } else {
      xf = CTransform4f::LookAt(CUnitVector3f(CVector3f::Zero()), flat, CVector3f::Up());
    }
  }
  // Retail's three `stfs` at 0x801b0e44..0x801b0e60 write `newXf`'s m03/m13/m23 into the local's
  // translation slots (r1+0xe0/0xf0/0x100) straight before the returning copy-construct reads it,
  // so the translation survives the `LookAt` branches - which `LookAt` zeroes - and comes back
  // from `newXf` whichever way out of this function goes. The members are private, so the three
  // assignments are spelled `SetTranslation(newXf.GetTranslation())`.
  xf.SetTranslation(newXf.GetTranslation());
  return xf;
}

CPlayer& CGameCamera::Player(CStateManager& mgr) const { return *mgr.GetPlayer(mControllerIdx); }

const CPlayer& CGameCamera::GetPlayer(const CStateManager& mgr) const {
  return *mgr.GetPlayer(mControllerIdx);
}

CCameraManager& CGameCamera::CameraManager(CStateManager& mgr) const {
  return *mgr.CameraManager(mControllerIdx);
}

CCameraManager& CGameCamera::GetCameraManager(const CStateManager& mgr) const {
  return *const_cast< CCameraManager* >(mgr.GetCameraManager(mControllerIdx));
}

void CGameCamera::SetTargetFov(float fov) {
  mFovInterpolation.mTarget = fov;
  mPerspDirty = true;
}

float CGameCamera::GetTargetFov() const { return mFovInterpolation.mTarget; }

void CGameCamera::SetFov(float fov) {
  mFovInterpolation.mCurrent = fov;
  mPerspDirty = true;
}

float CGameCamera::GetFov() const { return mFovInterpolation.mCurrent; }

void CGameCamera::SetFovAndTarget(float fov) {
  mFovInterpolation.mCurrent = fov;
  mFovInterpolation.mTarget = fov;
  mPerspDirty = true;
}

void CGameCamera::ResetFovInterpolation(float fov) {
  fn_801B19D8(&mFovInterpolation, 0.f, 0.f, 0.f, fov, fov, kInvalidUniqueId);
  mPerspDirty = true;
}

void CGameCamera::InterpolateFOV(float fov, float duration, float delay) {
  if (duration <= 0.f) {
    ResetFovInterpolation(fov);
  } else {
    fn_801B19D8(&mFovInterpolation, delay, duration, duration, GetFov(), fov,
                kInvalidUniqueId);
  }
}

void CGameCamera::InterpolateFOV(float startFov, float duration, float delay, TUniqueId cameraId,
                                 CStateManager& mgr) {
  CGameCamera* camera = TCastToPtr< CGameCamera >(mgr.ObjectById(cameraId));
  if (camera != nullptr) {
    const float target = camera->GetFov();
    if (duration <= 0.f) {
      ResetFovInterpolation(target);
    } else {
      fn_801B19D8(&mFovInterpolation, delay, duration, duration, startFov, target, cameraId);
    }
  }
}

void CGameCamera::UpdatePerspective(float dt, CStateManager& mgr) {
  if (mFovInterpolation.mDelay > 0.f) {
    mFovInterpolation.mDelay -= dt;
  } else if (mFovInterpolation.mRemaining > 0.f) {
    CGameCamera* camera = TCastToPtr< CGameCamera >(mgr.ObjectById(mFovInterpolation.mCameraId));
    if (camera != nullptr && camera->GetUniqueId() != GetUniqueId()) {
      SetTargetFov(camera->GetFov());
    }

    mFovInterpolation.mRemaining -= dt;
    if (mFovInterpolation.mRemaining <= 0.f) {
      SetFov(GetTargetFov());
    } else {
      // Three named locals, in this order, are what retail's register allocation needs: the
      // `target` and `delta` pair is what it keeps in f31/f30 across the Clamp, and writing the
      // difference inline leaves a third value live and grows the frame from 64 to 80 bytes.
      const float target = GetTargetFov();
      const float delta = GetFov() - target;
      const float t =
          CMath::Clamp(0.f, mFovInterpolation.mRemaining / mFovInterpolation.mDuration, 1.f);
      SetFov(delta * t + GetTargetFov());
    }
  } else if (!(CMath::AbsF(GetFov() - GetTargetFov()) < 0.00001f)) {
    SetFov(GetTargetFov());
  }
}

CVector3f CGameCamera::GetScanObjectIndicatorPosition(const CStateManager& mgr) const {
  // `mWatchedObject` is at 0x158, the first byte past `CActor` (`CHECK_SIZEOF(CActor, 0x158)`), and
  // retail reads it with `lhz` - it is a `TUniqueId`, not an index. Both `GetObjectById` and
  // `TCastToPtr` are reached through the *const* manager, while `CameraManager` takes a mutable
  // one, so both casts away below are the source's own.
  //
  // The three exits call vtable offset 0x5C, which is `GetScanObjectIndicatorPosition` in both
  // `CActor`'s and `CGameCamera`'s tables (`objdump -r -j .data` of either retail object). Two of
  // them dispatch on the camera manager's field at 0x1C; that offset is `mBallCamera`, once
  // `rstl::vector`'s `rmemory_allocator` member is counted, and `CBallCamera` derives from
  // `CGameCamera` so the slot exists there too. Naming the field `x20_` instead emits `lwz r4,32(r4)`
  // and lands at 99.97%.
  if (TCastToPtr< CPlayer >(
          const_cast< CEntity* >(mgr.GetObjectById(mWatchedObject))) != nullptr) {
    return CameraManager(const_cast< CStateManager& >(mgr))
        .BallCamera()
        ->GetScanObjectIndicatorPosition(mgr);
  }
  const CActor* actor =
      TCastToPtr< CActor >(const_cast< CEntity* >(mgr.GetObjectById(mWatchedObject)));
  if (actor == nullptr) {
    return CameraManager(const_cast< CStateManager& >(mgr))
        .BallCamera()
        ->GetScanObjectIndicatorPosition(mgr);
  }
  return actor->GetScanObjectIndicatorPosition(mgr);
}

rstl::optional_object< CAABox > CGameCamera::GetTouchBounds() const {
  return CAABox(GetTranslation(), GetTranslation());
}

void CGameCamera::UnkVtable84(TUniqueId, CStateManager&) {}

void CGameCamera::UnkVtable88(TUniqueId, CStateManager&) {}

void CGameCamera::ClearFluidList(CStateManager& mgr) {
  // Retail copies `GetFluidList()`'s vector onto the stack first (`srwi./mtctr` with an
  // eight-halfword unrolled body is `reserved_vector`'s copy constructor inlined), then walks the
  // *copy* with a running pointer in r30 and a separate index in r29 against the count in r31.
  // The index must be `int`: with `uint` MWCC keeps the induction variable split and emits
  // `lhzx r0,base,byteoffset` plus two counters (89.40%), where `int` gives the pointer walk
  // retail has (`lhz r0,0(r30)`; `addi r30,r30,2`; `addi r29,r29,1`; `cmpw r29,r31`) at 100%.
  // The `const` local is what forces the copy; iterating `GetFluidList()` directly emits none.
  const rstl::reserved_vector< TUniqueId, 4 > fluids = GetFluidList();
  for (int i = 0; i < fluids.size(); ++i) {
    CScriptWater* water = TCastToPtr< CScriptWater >(mgr.ObjectById(fluids[i]));
    if (water != nullptr) {
      // Two `sth` of `this+8` (`CEntity::mUniqueId`) into two stack slots: the by-value `TUniqueId`
      // return temporary and the by-value argument, each 0x10-aligned.
      water->RemoveInhabitant(GetUniqueId(), mgr);
    }
  }
  CActor::ClearFluidList(mgr);
}
