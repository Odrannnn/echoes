#include "MetroidPrime/CPhysicsActor.hpp"

#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CloseEnough.hpp"

#include "rstl/math.hpp"

// Retail 0x800EA17C, 0x3C = 60 bytes. **The name is a placeholder**, so this is `extern "C"` with
// `self` as a first parameter - the mechanism `src/MetroidPrime/Carve80049244.cpp` documents for
// exactly this case, and the one `main.cpp` and `CAnimData.cpp` use. It is retail's
// `CPhysicsActor::SetCollisionPrimitive`, which `include/MetroidPrime/CPhysicsActor.hpp` declares
// and no TU defined. `CScriptDebris.o` (0x800D303C) and `CScriptWater.o` (0x800D9ED4) call it.
//
// The body copies 32 bytes from `&prim + 8` to `self + 0x238`. Three measurements fix that:
// `GetCollisionPrimitive` is `addi r3,r3,0x230` (asm:491), so `mCollisionPrimitive` is at 0x230;
// `CHECK_SIZEOF(CCollidableAABox, 0x28)` with a vtable at +0 gives `CCollisionPrimitive`'s
// `x4_` at +4 and `mMaterial` at +8, so `+0x238` is `mCollisionPrimitive.mMaterial`. The copy is
// therefore `mMaterial` (8) + `mAabb` (0x18) = 0x20 bytes, and **it skips the vtable at +0 and
// `x4_` at +4**. `x4_` is declared in `include/Collision/CCollisionPrimitive.hpp` and read and
// written nowhere in the tree, and `mCollisionPrimitive = prim` was measured to copy it (0.00% ->
// 86.67%, one extra `lwz`/`stw` pair), so the 32-byte payload is written through the same-layout
// overlay `Carve80049244.cpp` uses - no header edit, no layout change, and `CHECK_SIZEOF` covers
// the offsets.
struct SCollisionPrimitivePayload {
  CMaterialList mMaterial;
  CAABox mAabb;
};

extern "C" void SetCollisionPrimitive__13CPhysicsActorFRC16CCollidableAABox(CPhysicsActor* self, const CCollidableAABox& prim);

// Retail 0x800EBD24, 0x1C = 28 bytes, listed in this unit's `.ctors` (asm:2477-2480): the
// **static constructor** that zeroes a 12-byte object at 0x80410974. **The name is a placeholder**,
// so it is `extern "C"` - the mechanism `src/MetroidPrime/Carve80049244.cpp` documents.
//
// The object is read from retail as a reference argument, not through this unit:
// `SMoverData::__ct__(..., lbl_80410974)` (0x800707F4) passes its address as the last
// `const CVector3f&`, and `CScriptActor`/`CScriptDoor`/`CScriptDock`/`CScriptPlatform`/
// `CScriptDebris`/`CCollisionActor` do the same.
//
// **`main/auto_08_80410974_bss` owns the symbol, and this unit must not define it under MWCC.**
// That dtk auto unit is linked into the matching build and already defines `lbl_80410974`
// (12 bytes) and `lbl_80410980` (0x18) at 0x80410974/0x80410980 - `nm
// build/G2ME01/obj/auto_08_80410974_bss.o` reports both as `C` and `main.elf` reports both as
// `B` at those addresses. So 0x80410974..0x80410998 is a **claimed** range, not a gap:
// `config/G2ME01/splits.txt:425` ends `CDecalManager.cpp`'s `.bss` there, and this unit's own
// split (`:433-437`) claims no `.bss` at all. A definition here would be a second common
// definition of a symbol the matching link already owns; GNU ld merges commons silently and
// `.bss` is NOBITS, so the shift never reaches the DOL and no gate would report it -
// `files.cmake:929-934` is exactly that hazard, 28 duplicates here.
//
// Hence the `#ifdef __MWERKS__` split this repo already uses for a loader variable a unit's
// split does not claim `.bss` for (`files.cmake:943-945`,
// `src/MetroidPrime/ScriptObjects/CScriptRsfAudio.cpp:22-26`): declare under MWCC, define on
// the host only, where no dtk split object exists and the flat port link would otherwise grow
// a 251st undefined symbol. objdiff resolves the `@ha`/`@l` relocations against retail's
// `3C 60 80 41` / `D4 03 09 74` from the declaration alone - that is what pairs the function.
//
// Retail's three stores are two `stfs` of 0.0f and one **`stw` of a word zero**, so the third
// component is not a `float` field: with a `float` third member the compiler emits `stfs f0, 8(r3)`
// and the function measures 77.14%. The struct below is what retail's bytes say, and nothing reads
// the object through it.
struct SLbl80410974 {
  float x;
  float y;
  int z;
};

extern "C" {
#ifdef __MWERKS__
// The matching build's owner is `main/auto_08_80410974_bss`; a definition here would be a second
// common definition of a symbol it already owns. Declared, not defined - see above.
extern SLbl80410974 lbl_80410974;
#else
// Host-only, because no dtk split object exists in the port link. Uninitialised, so it lands in
// `.bss` and the port's copy is already the zero retail's constructor writes.
SLbl80410974 lbl_80410974;
#endif

void fn_800EBD24() {
  lbl_80410974.x = 0.f;
  lbl_80410974.y = 0.f;
  lbl_80410974.z = 0;
}
}

const float CPhysicsActor::kGravityAccel = 9.81f * 2.5f;

CPhysicsActor::CPhysicsActor(TUniqueId uid, const rstl::string& name,
                             const CEntityInfo& info, uint inGrave, const CTransform4f& xf,
                             const CModelData& mData, const CMaterialList& matList,
                             const CAABox& aabb, const SMoverData& moverData,
                             const CActorParameters& actParams, const StepData& stepData)
: CActor(uid, name, info, inGrave | 2, xf, mData, matList, actParams, kInvalidUniqueId)
, mMass(moverData.mMass)
, mMassRecip(moverData.mMass > 0.f ? 1.f / moverData.mMass : 1.f)
, mInertiaTensor(0.f)
, mInertiaTensorRecip(0.f)
, mMovable(true)
, mAngularEnabled(false)
, mStandardCollider(false)
, mConstantForce(CVector3f(0.f, 0.f, 0.f))
, mAngularMomentum(CAxisAngle::Identity())
, x114_(CMatrix3f::Identity())
, mVelocity(CVector3f(0.f, 0.f, 0.f))
, mAngularVelocity(CAxisAngle::Identity())
, mMomentum(moverData.mMomentum)
, mForce(CVector3f(0.f, 0.f, 0.f))
, mImpulse(CVector3f(0.f, 0.f, 0.f))
, mTorque(CAxisAngle::Identity())
, mAngularImpulse(CAxisAngle::Identity())
, mMoveImpulse(CVector3f(0.f, 0.f, 0.f))
, mMoveAngularImpulse(CAxisAngle::Identity())
, mBaseBoundingBox(aabb)
, mCollisionPrimitive(aabb, matList)
, mPrimitiveOffset(xf.GetTranslation())
, mLastNonCollidingState(xf.GetTranslation(),
                             CNUQuaternion::BuildFromMatrix3f(xf.BuildMatrix3f()),
                             CVector3f::Zero(), CAxisAngle::Identity())
, mMaximumCollisionVelocity(1000000.0)
, mStepUpHeight(stepData.stepUp)
, mStepDownHeight(stepData.stepDown)
, mRestitutionCoefModifier(0.f)
, mCollisionAccuracyModifier(1.f)
, mNumTicksStuck(0)
, mNumTicksPartialUpdate(0) {
  SetMass(moverData.mMass);
  MoveCollisionPrimitive(CVector3f::Zero());
  SetVelocityOR(moverData.mVelocity);
  SetAngularVelocityOR(moverData.mAngularVelocity);
  ComputeDerivedQuantities();
}

// Retail 0x800EB944, 0x64 = 100 bytes. **The name is a placeholder**, so `extern "C"` with `self`
// first. `__dt__` (0x800EB8C8) calls it once, at asm:2199, with `r3 = this + 0x2c4` and
// `r4 = -1`; it is the out-of-line destructor chain for `x254_`, the `CPhysicsActorUnkB` member at
// `include/MetroidPrime/CPhysicsActor.hpp:245`.
//
// NOT DEFINED HERE, and that is a measured decision, not an omission. The body is 25
// instructions and is characterisable, and written as below it matches **every byte** (100.00%,
// verified instruction by instruction). It is left out because its one call,
// `bl fn_800CD460` (asm:2223), is **undefined in the whole tree**: `nm` over all 905 objects of
// `build/G2ME01/src` finds no definition, and it is not among the 250 symbols
// `tools/link_check.sh` already tolerates, so defining `fn_800EB944` makes that count 251 and
// `goal_check.sh` fails the gate on "link_check: STRICT FAIL - regression gate: 251 undefined
// against a baseline of 250 (GREW)". `fn_800CD460` lives in `CMorphBall.o` (0x800CD460, 0x58
// bytes) and is called from three units (`CPhysicsActor.s:2219`, `CGroundMovement.s:2357`,
// `auto_03_801F7AD0_text.s:129`) with no definition anywhere; it in turn calls `fn_800CD4B8`,
// which calls `fn_8033D2F4`, **also undefined**. So closing this needs that chain, not this unit.
// The three measured spellings, for whoever does it:
//
//   1. `r4` is a **deleting-destructor flag**, not a pointer: `__dt__` passes `-1` here and `0` for
//     the other two teardown calls, and the body tests it with `extsh. r0,r31` / `ble` (asm:2222-3),
//     a sign-extended halfword branched on **sign**, which is why the parameter is `short` and the
//     test is `deleting > 0` rather than `if (deleting)`. A `bool` parameter measures 93.40% and
//     emits `clrlwi.` + `beq` instead of the `extsh.`.
//   2. `lbz r0,0(r30)` / `cmplwi r0,0` / `beq` (asm:2216-2220) is `if (obj->a)`, the `int a` of
//     `CPhysicsActorUnkB` (`include/MetroidPrime/CPhysicsActor.hpp:77`) tested for non-zero.
//   3. The return type is `void*`, not `void`, and there is a **single exit**: retail's epilogue is
//     one `mr r3,r30` immediately before the reloads (asm:2226), so it returns `self`. A `void`
//     return drops that instruction (measured 24 instructions); an early `return self` on the
//     null path adds one back (measured 26). One exit with a `void*` return is 25.
//
// Three things in the 25 instructions, all measured from the disassembly:
//   - `r4` is a **deleting-destructor flag**, not a pointer: `__dt__` passes `-1` here and `0` for
//     the other two teardown calls, and the body tests it with `extsh. r0,r31` / `ble` (asm:2222-3),
//     a sign-extended halfword branched on **sign**, which is why the parameter is `short` and the
//     test is `deleting > 0` rather than `if (deleting)`. A `bool` parameter measures 93.40% and
//     emits `clrlwi.` + `beq` instead of the `extsh.`.
//   - `lbz r0,0(r30)` / `cmplwi r0,0` / `beq` (asm:2216-2220) is `if (obj->a)`, the `int a` of
//     `CPhysicsActorUnkB` (`include/MetroidPrime/CPhysicsActor.hpp:77`) tested for non-zero.

CPhysicsActor::~CPhysicsActor() {}

void CPhysicsActor::ApplyImpulseWR(const CVector3f& impulse, const CAxisAngle& angularImpulse) {
  mImpulse = mImpulse + impulse;
  mAngularImpulse = mAngularImpulse + angularImpulse;
}

void CPhysicsActor::ApplyTorqueWR(const CVector3f& torque) {
  mTorque = mTorque + CAxisAngle(torque);
}

void CPhysicsActor::ApplyForceWR(const CVector3f& force, const CAxisAngle& torque) {
  mForce = mForce + force;
  mTorque = mTorque + torque;
}

void CPhysicsActor::ApplyImpulseOR(const CVector3f& impulse, const CAxisAngle& angle) {
  mImpulse = mImpulse + GetTransform().Rotate(impulse);
  CAxisAngle rotatedAngle(GetTransform().Rotate(angle.GetVector()));
  mAngularImpulse = mAngularImpulse + rotatedAngle;
}

void CPhysicsActor::ApplyForceOR(const CVector3f& force, const CAxisAngle& torque) {
  mForce = mForce + GetTransform().Rotate(force);
  CAxisAngle rotatedTorque(GetTransform().Rotate(torque.GetVector()));
  mTorque = mTorque + rotatedTorque;
}

void CPhysicsActor::ComputeDerivedQuantities() {
  mVelocity = mConstantForce * mMassRecip;
  x114_ = GetTransform().BuildMatrix3f();
  mAngularVelocity = CAxisAngle(mAngularMomentum.GetVector() * mInertiaTensorRecip);
}

CPhysicsState CPhysicsActor::GetPhysicsState() const {
  return CPhysicsState(GetTranslation(), GetRotation(), GetConstantForceWR(),
                       GetAngularMomentumWR(), GetMomentumWR(), GetForceWR(), GetImpulseWR(),
                       GetTorqueWR(), GetAngularImpulseWR());
}

void CPhysicsActor::SetPhysicsState(const CPhysicsState& state) {
  SetTranslation(state.GetTranslation());
  SetTransform(state.GetOrientation().BuildTransform4f(GetTranslation()));
  SetConstantForceWR(state.GetConstantForceWR());
  SetAngularMomentumWR(state.GetAngularMomentumWR());
  SetMomentumWR(state.GetMomentumWR());
  SetForceWR(state.GetForceWR());
  SetImpulseWR(state.GetImpulseWR());
  SetTorqueWR(state.GetTorque());
  SetAngularImpulseWR(state.GetAngularImpulseWR());
  ComputeDerivedQuantities();
}

CVector3f CPhysicsActor::CalculateNewVelocityWR_UsingImpulses() const {
  return mVelocity + mMassRecip * (mImpulse + mMoveImpulse);
}

CMotionState CPhysicsActor::PredictMotion(float dt) const {
  CMotionState msl = PredictLinearMotion(dt);
  CMotionState msa = PredictAngularMotion(dt);
  return CMotionState(msl.GetTranslation(), msa.GetOrientation(), msl.GetVelocity(),
                      msa.GetAngularMomentum());
}

CMotionState CPhysicsActor::PredictAngularMotion(float dt) const {
  CVector3f v1 = (mAngularImpulse.GetVector() + mMoveAngularImpulse.GetVector()) *
                 mInertiaTensorRecip;
  CVector3f v2 = mAngularVelocity.GetVector() + v1;

  CNUQuaternion q3 = (0.5f * CNUQuaternion(0.f, v2)) *
                     CNUQuaternion::BuildFromQuaternion(CQuaternion::FromMatrix(GetTransform()));
  CAxisAngle torque = mTorque;

  return CMotionState(CVector3f::Zero(), q3 * dt, CVector3f::Zero(),
                      (torque * dt) + mAngularImpulse);
}

CMotionState CPhysicsActor::PredictLinearMotion(float dt) const {
  CVector3f velocity = CVector3f(CalculateNewVelocityWR_UsingImpulses());
  CVector3f sum = mForce + mMomentum;

  return CMotionState(dt * velocity, CNUQuaternion(0.f, CVector3f::Zero()), dt * sum + mImpulse,
                      CAxisAngle::Identity());
}

CMotionState CPhysicsActor::PredictMotion_Internal(float dt) const {
  if (!mAngularEnabled) {
    CMotionState msl = PredictLinearMotion(dt);
    CMotionState msa = PredictAngularMotion(dt);
    return CMotionState(msl.GetTranslation(), msa.GetOrientation(), msl.GetVelocity(),
                        msa.GetAngularMomentum());

  } else {
    return PredictLinearMotion(dt);
  }
}

void CPhysicsActor::SetMotionState(const CMotionState& state) {
  SetTransform(
      CQuaternion::FromNUQuaternion(state.GetOrientation()).BuildTransform4f(GetTranslation()));
  SetTranslation(state.GetTranslation());

  mConstantForce = state.GetVelocity();
  mAngularMomentum = state.GetAngularMomentum();
  ComputeDerivedQuantities();
}

CMotionState CPhysicsActor::GetMotionState() const {
  return CMotionState(GetTranslation(), CNUQuaternion::BuildFromQuaternion(GetRotation()),
                      GetConstantForceWR(), GetAngularMomentumWR());
}

void CPhysicsActor::AddMotionState(const CMotionState& state) {
  CNUQuaternion q(CNUQuaternion::BuildFromQuaternion(CQuaternion::FromMatrix(GetTransform())));
  q += state.GetOrientation();
  SetTransform(CQuaternion::FromNUQuaternion(q).BuildTransform4f(GetTranslation()));
  SetTranslation(GetTranslation() + state.GetTranslation());

  mConstantForce += state.GetVelocity();
  mAngularMomentum += state.GetAngularMomentum();

  ComputeDerivedQuantities();
}

bool CPhysicsActor::WillMove(const CStateManager& mgr) {
  if (close_enough(mVelocity, CVector3f::Zero()) &&
      close_enough(mImpulse, CVector3f::Zero()) &&
      close_enough(mTorque.GetVector(), CVector3f::Zero()) &&
      close_enough(mMoveImpulse, CVector3f::Zero()) &&
      close_enough(mAngularVelocity.GetVector(), CVector3f::Zero()) &&
      close_enough(mAngularImpulse.GetVector(), CVector3f::Zero()) &&
      close_enough(mMoveAngularImpulse.GetVector(), CVector3f::Zero()) &&
      close_enough(GetTotalForceWR(), CVector3f::Zero())) {
    return false;
  }

  return true;
}

void CPhysicsActor::Stop() {
  ClearForcesAndTorques();
  mConstantForce = CVector3f::Zero();
  mAngularMomentum = CAxisAngle::Identity();
  ComputeDerivedQuantities();
}

void CPhysicsActor::ClearForcesAndTorques() {
  mForce = mImpulse = mMoveImpulse = CVector3f::Zero();
  mTorque = mAngularImpulse = mMoveAngularImpulse = CAxisAngle::Identity();
}


void CPhysicsActor::ClearImpulses() {
  mImpulse = mMoveImpulse = CVector3f::Zero();
  mAngularImpulse = mMoveAngularImpulse = CAxisAngle::Identity();
}

// Retail 0x800EA984, 0x5C = 92 bytes. **The name is a placeholder**, so `extern "C"` with `self`
// first, the mechanism `Carve80049244.cpp` documents. It sits immediately before
// `ClearImpulses` (0x800EA9E0) in retail and is that function's **angular half split out into its
// own out-of-line body**: it calls `CAxisAngle::Identity()`, stores the result at `+0x208`, then
// reloads it and stores it at `+0x1f0` - i.e. `mAngularImpulse = mMoveAngularImpulse =
// CAxisAngle::Identity()`. `ClearImpulses` inlines the same sequence (asm:1127-1148, byte-identical
// modulo the vector half), and `CMorphBall.o` calls this one at 0x800CB150.
//
// The store offsets are the header's own: `mAngularImpulse` and `mMoveAngularImpulse` are the
// adjacent `CAxisAngle` members at `include/MetroidPrime/CPhysicsActor.hpp:229,231`, and
// `ClearImpulses` at 100% proves those two land at 0x208 and 0x1f0. They are private and the
// header has no accessors for the move one, so the two words go through the same-layout overlay
// `Carve80049244.cpp` uses - no header edit and no layout change.
extern "C" void fn_800EA984(CPhysicsActor* self);

extern "C" void fn_800EA984(CPhysicsActor* self) {
  CAxisAngle* const angularImpulse = reinterpret_cast< CAxisAngle* >(
      reinterpret_cast< char* >(self) + 0x208);
  *angularImpulse = CAxisAngle::Identity();
  *reinterpret_cast< CAxisAngle* >(reinterpret_cast< char* >(self) + 0x1f0) = *angularImpulse;
}

void CPhysicsActor::UseCollisionImpulses() {
  mConstantForce += mImpulse;
  mAngularMomentum += mAngularImpulse;
  mImpulse = CVector3f::Zero();
  mAngularImpulse = CAxisAngle::Identity();
  ComputeDerivedQuantities();
}

void CPhysicsActor::MoveToWR(const CVector3f& trans, float d) {
  mConstantForce = (trans - GetTranslation()) * GetMass() * (1.f / d);
  ComputeDerivedQuantities();
}

void CPhysicsActor::MoveInOneFrameWR(const CVector3f& trans, float d) {
  mMoveImpulse += (trans - GetTranslation()) * GetMass() * (1.f / d);
}

CVector3f CPhysicsActor::GetMoveToORImpulseWR(const CVector3f& trans, float d) const {
  CVector3f impulse = GetTransform().Rotate(trans);
  return (1.f / d) * (GetMass() * impulse);
}

CVector3f CPhysicsActor::GetRotateToORAngularMomentumWR(const CQuaternion& q, float d) const {
  if (q.GetScalar() > 0.99999976f) {
    return CVector3f::Zero();
  } else {
    const CQuaternion rotated(q.GetScalar(), GetTransform().Rotate(q.GetVector()));

    const double ac = acos(rotated.GetScalar());
    return rotated.GetVector().AsNormalized() * ((static_cast< float >(ac) * 2.0f) * (1.0f / d)) *
           mInertiaTensor;
  }
}

void CPhysicsActor::MoveToOR(const CVector3f& trans, float d) {
  mConstantForce = GetMoveToORImpulseWR(trans, d);
  ComputeDerivedQuantities();
}

void CPhysicsActor::RotateToOR(const CQuaternion& q, float d) {
  const CVector3f& vec = GetRotateToORAngularMomentumWR(q, d);
  mAngularMomentum = CAxisAngle(vec);
  ComputeDerivedQuantities();
}

void CPhysicsActor::MoveInOneFrameOR(const CVector3f& trans, float d) {
  mMoveImpulse += GetMoveToORImpulseWR(trans, d);
}

void CPhysicsActor::RotateInOneFrameOR(const CQuaternion& q, float d) {
  const CVector3f& vec = GetRotateToORAngularMomentumWR(q, d);
  mMoveAngularImpulse += CAxisAngle(vec);
}

void CPhysicsActor::SetVelocityOR(const CVector3f& vel) {
  SetVelocityWR(GetTransform().Rotate(vel));
}

CVector3f CPhysicsActor::GetTotalForceWR() const { return mForce + mMomentum; }

void CPhysicsActor::SetVelocityWR(const CVector3f& vel) {
  mVelocity = vel;
  mConstantForce = mMass * mVelocity;
}

void CPhysicsActor::SetAngularVelocityWR(const CAxisAngle& angVel) {
  mAngularVelocity = angVel;
  mAngularMomentum = CAxisAngle(mAngularVelocity.GetVector() * mInertiaTensor);
}

CAxisAngle CPhysicsActor::GetAngularVelocityOR() const {
  return CAxisAngle(GetTransform().TransposeRotate(mAngularVelocity.GetVector()));
}

void CPhysicsActor::SetAngularVelocityOR(const CAxisAngle& angVel) {
  mAngularVelocity = CAxisAngle(GetTransform().Rotate(angVel.GetVector()));
  mAngularMomentum = CAxisAngle(mAngularVelocity.GetVector() * mInertiaTensor);
}

void CPhysicsActor::SetMass(float mass) {
  mMass = mass;
  mMassRecip = (mMass > 0.0f) ? (1.0f / mMass) : 1.0f;
  SetInertiaTensorScalar(0.16666667f * mMass);
}

void CPhysicsActor::SetInertiaTensorScalar(float tensor) {
  mInertiaTensor = (tensor > 0.0f) ? tensor : 1.0f;
  mInertiaTensorRecip = 1.0f / mInertiaTensor;
}

const CCollisionPrimitive* CPhysicsActor::GetCollisionPrimitive() const {
  return &mCollisionPrimitive;
}

extern "C" void SetCollisionPrimitive__13CPhysicsActorFRC16CCollidableAABox(CPhysicsActor* self, const CCollidableAABox& prim) {
  *reinterpret_cast< SCollisionPrimitivePayload* >(reinterpret_cast< char* >(self) + 0x238) =
      *reinterpret_cast< const SCollisionPrimitivePayload* >(
          reinterpret_cast< const char* >( &prim ) + 8);
}
void CPhysicsActor::MoveCollisionPrimitive(const CVector3f& offset) {
  mPrimitiveOffset = offset;
}





CTransform4f CPhysicsActor::GetPrimitiveTransform() const {
  return CTransform4f::Translate(GetTransform().GetTranslation() + mPrimitiveOffset);
}

void CPhysicsActor::CollidedWith(const TUniqueId& id, const CCollisionInfoList& list,
                                 CStateManager& mgr) {}

const CAABox& CPhysicsActor::GetBaseBoundingBox() const { return mBaseBoundingBox; }

CAABox CPhysicsActor::GetBoundingBox() const {
  CVector3f off = mPrimitiveOffset + GetTranslation();
  return CAABox(mBaseBoundingBox.GetMinPoint() + off, mBaseBoundingBox.GetMaxPoint() + off);
}

CAABox CPhysicsActor::GetMotionVolume(float dt) const {
  CAABox aabox = GetCollisionPrimitive()->CalculateAABox(GetPrimitiveTransform());
  CVector3f velocity = CalculateNewVelocityWR_UsingImpulses();

  const CVector3f dv = (dt * velocity);
  aabox.AccumulateBounds(aabox.GetMaxPoint() + dv);
  aabox.AccumulateBounds(aabox.GetMinPoint() + dv);

  // The three expansion components are the **same** literal in retail: `lbl_8041B76C` = 0.3f
  // (asm:0x800E9F5C/0x800E9FBC, one `lfs f3` feeding all three `fadds`/`fsubs`), not Prime 1's
  // `CVector3f(0.5f, 0.5f, up + 1.f)` / `down + 1.5f`. That single-literal shape is also what makes
  // the register allocation match; with two distinct literals MWCC picks f2/f0 where retail picks
  // f0/f3 and the function scores 97.19%.
  float up = rstl::max_val(GetStepUpHeight(), 0.f);
  aabox.AccumulateBounds(aabox.GetMaxPoint() + CVector3f(0.3f, 0.3f, up + 0.3f));

  float down = rstl::max_val(GetStepDownHeight(), 0.f);
  aabox.AccumulateBounds(aabox.GetMinPoint() - CVector3f(0.3f, 0.3f, down + 0.3f));
  return aabox;
}

void CPhysicsActor::SetBoundingBox(const CAABox& box) {
  mBaseBoundingBox = box;
  MoveCollisionPrimitive(CVector3f::Zero());
}

float CPhysicsActor::GetWeight() const { return CPhysicsActor::GravityConstant() * GetMass(); }

CVector3f CPhysicsActor::GetPrimitiveOffset() const { return mPrimitiveOffset; }

float CPhysicsActor::GetStepDownHeight() const { return mStepDownHeight; }

void CPhysicsActor::SetStepUpHeight(float h) {
  mStepUpHeight = h;
}

float CPhysicsActor::GetStepUpHeight() const { return mStepUpHeight; }

CVector3f CPhysicsActor::GetOrbitPosition(const CStateManager&) const {
  return GetBoundingBox().GetCenterPoint();
}

CVector3f CPhysicsActor::GetAimPosition(const CStateManager&, float dt) const {
  if (dt > 0.0f) {
    CMotionState s = PredictMotion(dt);
    return GetBoundingBox().GetCenterPoint() + s.GetTranslation();
  } else {
    return GetBoundingBox().GetCenterPoint();
  }
}

void CPhysicsActor::Render(const CStateManager& mgr) const { CActor::Render(mgr); }

void CPhysicsActor::SetCoefficientOfRestitutionModifier(float modifier) {
  mRestitutionCoefModifier = modifier;
}

float CPhysicsActor::GetCoefficientOfRestitutionModifier() const {
  return mRestitutionCoefModifier;
}

void CPhysicsActor::SetCollisionAccuracyModifier(float modifier) {
  mCollisionAccuracyModifier = modifier;
}

float CPhysicsActor::GetCollisionAccuracyModifier() const { return mCollisionAccuracyModifier; }

void CPhysicsActor::SetMaxVelocityAfterCollision(float velocity) {
  mMaximumCollisionVelocity = velocity;
}

float CPhysicsActor::GetMaximumCollisionVelocity() const { return mMaximumCollisionVelocity; }

bool CPhysicsActor::IsOnStaticGround() const {
  return 0;
}
