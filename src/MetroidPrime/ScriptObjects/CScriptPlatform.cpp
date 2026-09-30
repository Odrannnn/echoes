#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/ScriptObjects/CScriptTrigger.hpp"

#include "rstl/algorithm.hpp"

// Retail keeps these two `vector<SRiders>` helpers as this unit's own out-of-line symbols at
// 0x800A47A8 and 0x800A46F0. Their bodies are the rstl ones - `uninitialized_copy` over
// `vector<SRiders>::iterator`, and `vector<SRiders>::reserve` - which the templates already
// reproduce instruction for instruction; what a template instantiation cannot do is carry
// retail's names, so both are written out here. Retail passes the range's end pointer to
// 0x800A47A8 in a fourth argument (r6 at 0x800A4740); no instruction of that body reads it, so
// it is left out instead of re-computed (passing it costs four instructions).
extern "C" SRiders* fn_800A47A8(rstl::vector< SRiders >::iterator first,
                                 rstl::vector< SRiders >::iterator last, SRiders* out) {
  SRiders* tmp = out;
  rstl::vector< SRiders >::iterator cur = first;
  for (; cur != last; ++cur, ++tmp) {
    rstl::construct(tmp, *cur);
  }
  return tmp;
}

extern "C" void fn_800A46F0(rstl::vector< SRiders >& slaves, int count) {
  if (count <= slaves.mCapacity) {
    return;
  }
  SRiders* items;
  slaves.mAllocator.allocate(items, count);
  fn_800A47A8(slaves.begin(), slaves.end(), items);
  rstl::destroy(slaves.mItems, slaves.mItems + slaves.mCount);
  rstl::rmemory_allocator::deallocate(slaves.mItems);
  slaves.mItems = items;
  slaves.mCapacity = count;
}

extern "C" void fn_800A14DC(rstl::vector< SRiders >& slaves, const SRiders& slave);

CScriptPlatform::CScriptPlatform(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CTransform4f& xf,
    const CModelData& model, const CActorParameters& params, const CAABox& bounds,
    const rstl::optional_object< TLockedToken< COBBTreeGroup > >& dcln,
    const CHealthInfo& health, const CDamageVulnerability& vulnerability,
    const CMaterialList& materials, bool detectCollision, uint maxRainSplashes, uint rainGenRate,
    const SPlatformMotionSpline& motionSpline, uint motionFlags, const CVector3f& conveyorVelocity,
    const CMayaSpline& rollSpline, const CMayaSpline& yawSpline, const CMayaSpline& pitchSpline,
    float initialTime, float xrayAlpha)
: CPhysicsActor(uid, name, info, 0, xf, model, materials, bounds, SMoverData(15000.f), params,
                StepData(0.f, 0.f, 0))
, mMoveDelay(0.f)
, mCollisionRecoverDelay(0.f)
, mFadeInTime(params.GetFadeInTime())
, mFadeOutTime(params.GetFadeOutTime())
, mConveyorVelocity(conveyorVelocity)
, mDragDelta(CVector3f::Zero())
, mRotationDelta(CQuaternion::NoRotation())
, mPreviousRotation(xf.GetRotation())
, mCurrentRotation(xf.GetRotation())
, mInitialHealth(health)
, mHealth(health)
, mDamageVulnerability(vulnerability)
, mTreeGroupContainer(dcln)
, mMaxRainSplashes(maxRainSplashes)
, mRainGenRate(rainGenRate)
, mBoundsTrigger(kInvalidUniqueId)
, mMotionSpline(rs_new SPlatformMotionSpline(motionSpline))
, mSplineController(nullptr)
, mMotionTime(0.f)
, mMotionFlags(motionFlags)
, mInitialTime(initialTime)
, mMotionDuration(motionSpline.mDuration)
, mWaypointTracker(nullptr)
, mRollSpline(rollSpline.GetKnotCount() ? rs_new CMayaSpline(rollSpline) : nullptr)
, mYawSpline(yawSpline.GetKnotCount() ? rs_new CMayaSpline(yawSpline) : nullptr)
, mPitchSpline(pitchSpline.GetKnotCount() ? rs_new CMayaSpline(pitchSpline) : nullptr)
, x450_(kInvalidUniqueId)
, x452_(kInvalidUniqueId)
, mLookAtTarget(kInvalidUniqueId)
, mXrayAlpha(xrayAlpha)
, mInitialTransform(xf)
, mDead(false)
, mControlledAnimation(false)
, mDetectCollision(detectCollision)
, mSquishedRider(false)
, mMotionActive(false)
, mPassedMotionEnd(false)
, mPassedMotionStart(false)
, mMotionForward(true)
, mPreviousMotionForward(true)
, x48d_25_(false)
, mMotionTransformed(false) {
  SetMovable(false);
  // TODO: original StepData initialization, material filter, animation setup and DCLN allocation.
}

CScriptPlatform::~CScriptPlatform() {
  // TODO: delete the spline controller and waypoint tracker once their interfaces are recovered.
}

rstl::optional_object< CAABox > CScriptPlatform::GetTouchBounds() const {
  if (GetActive()) {
    if (!mTreeGroup.null()) {
      return mTreeGroup->CalculateAABox(GetTransform());
    } else {
      return GetBoundingBox();
    }
  }
  return rstl::optional_object< CAABox >();
}

void CScriptPlatform::StopMotion() {
  mMotionActive = false;
  Stop();
  mPreviousRotation = GetTransform().GetRotation();
  mPreviousRotation.Orthonormalize();
  mCurrentRotation = mPreviousRotation;
  mDragDelta = CVector3f::Zero();
  mRotationDelta = CQuaternion::NoRotation();
}

void CScriptPlatform::fn_800a3d18() { StopMotion(); }

void CScriptPlatform::AdvanceMotionTime(float dt) {
  // TODO: forward/reverse, loop/clamp and endpoint events.
}

void CScriptPlatform::AddRider(rstl::vector< SRiders >& riders, TUniqueId id,
                               const CPhysicsActor* ridee, CStateManager& mgr,
                               const rstl::optional_object< float >& decayTimer) {
  rstl::vector< SRiders >::iterator it =
      rstl::find(riders.begin(), riders.end(),
                 SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >()));
  if (it == riders.end()) {
    SRiders rider(id, CTransform4f::Identity(), rstl::optional_object< float >(decayTimer));
    if (ridee != nullptr) {
      if (CPhysicsActor* actor = TCastToPtr< CPhysicsActor >(mgr.ObjectById(id))) {
        CVector3f relative = ridee->GetTransform().TransposeRotate(
            actor->GetTranslation() - ridee->GetTranslation());
        rider.mTransform = CTransform4f::Translate(relative);
        // Retail repeats this guard after computing the rider transform.
        if (ridee != nullptr) {
          mgr.DeliverScriptMsg(CScriptMsg(
              ridee->GetUniqueId(), kInvalidUniqueId, actor->GetUniqueId(),
              static_cast< EScriptObjectMessage >(0x584f4e50), kSS_InvalidState));
        }
      }
    } else {
      mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, kInvalidUniqueId, id,
                                      static_cast< EScriptObjectMessage >(0x584f4e50),
                                      kSS_InvalidState));
    }
    fn_800A46F0(riders, riders.mCount + 1);
    fn_800A14DC(riders, rider);
  } else {
    it->mDecayTimer = decayTimer;
  }
}

CScriptPlatform::TNearList
CScriptPlatform::BuildNearListFromRiders(CStateManager& mgr,
                                         const rstl::vector< SRiders >& riders) {
  TNearList result;
  rstl::vector< SRiders >::const_iterator end = riders.end();
  for (rstl::vector< SRiders >::const_iterator it = riders.begin(); it != end; ++it) {
    if (CActor* actor = TCastToPtr< CActor >(mgr.GetObjectByIdFromListAll(it->mUid))) {
      result.push_back(actor->GetUniqueId());
    }
  }
  return result;
}

void CScriptPlatform::DecayRiders(rstl::vector< SRiders >& riders, float dt, CStateManager& mgr) {
  // TODO: decrement optional timers, erase expired riders and send XONP.
}

void CScriptPlatform::MoveRiders(CStateManager& mgr, bool active, rstl::vector< SRiders >& riders,
                                 rstl::vector< SRiders >& collidedRiders, const TNearList& nearList,
                                 const CTransform4f& oldXf, const CTransform4f& newXf,
                                 const CVector3f& dragDelta, const CQuaternion& rotDelta) {
  // TODO: collision-tested rider displacement and rotation.
}

void CScriptPlatform::PreThink(float dt, CStateManager& mgr) {
  // TODO: platform motion, collision filtering and rider movement.
}

void CScriptPlatform::BuildSlaveList(CStateManager& mgr) {
  fn_800A46F0(mStaticSlaves, GetConnectionList().size());
  for (rstl::vector< SConnection >::const_iterator conn = GetConnectionList().begin();
       conn != GetConnectionList().end(); ++conn) {
    if (conn->state == kSS_Play && conn->msg == kSM_Activate) {
      if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(mgr.GetIdForScript(conn->objId)))) {
        actor->AddMaterial(kMT_PlatformSlave, mgr);
        CTransform4f transform = actor->GetTransform();
        transform.SetTranslation(actor->GetTranslation() - GetTranslation());
        fn_800A14DC(mStaticSlaves,
                    SRiders(actor->GetUniqueId(), transform, rstl::optional_object< float >()));
      }
    } else if (conn->state == kSS_InheritBounds && conn->msg == kSM_Activate) {
      CStateManager::TIdListResult ids = mgr.GetIdListForScript(conn->objId);
      for (CStateManager::TIdList::const_iterator it = ids.first; it != ids.second; ++it) {
        if (TCastToConstPtr< CScriptTrigger >(mgr.GetObjectById(it->second))) {
          mBoundsTrigger = it->second;
        }
      }
    }
  }
}

void CScriptPlatform::DragSlave(CStateManager& mgr, TMovedList& moved, const SRiders& slave) {
  // TODO: apply the motion flags and recursively move platform slaves.
}

void CScriptPlatform::DragSlaves(CStateManager& mgr, TMovedList& moved) {
  // TODO: propagate motion to static/dynamic slaves and special actor types.
}

void CScriptPlatform::Think(float dt, CStateManager& mgr) {
  // TODO: animation, fade/death and rider decay.
}

bool CScriptPlatform::IsInMovedList(TUniqueId id, const TMovedList& moved) {
  // Retail stores and compares the low 10 bits of the id (DragSlave masks the same way).
  const ushort index = id.Value() & 0x3ff;
  for (const ushort* it = moved.begin(); it != moved.end(); ++it) {
    if (index == *it) {
      return true;
    }
  }
  return false;
}

CHealthInfo* CScriptPlatform::HealthInfo() { return &mHealth; }

const CDamageVulnerability* CScriptPlatform::GetDamageVulnerability() const {
  return &mDamageVulnerability;
}

void CScriptPlatform::SetMotionTime(float time, CStateManager& mgr) {
  mMotionTime = time;
  // TODO: reset motion flags, evaluate the controller and move slaves.
}

void CScriptPlatform::TeleportToWaypoint(TUniqueId id, CStateManager& mgr) {
  // TODO: obtain the connected waypoint time, then update motion.
}

void CScriptPlatform::TranslateMotion(const CVector3f& delta) {
  if (!mSplineController.null()) {
    mSplineController->PositionSpline().Translate(delta);
  }
  SetTranslation(GetTranslation() + delta);
  mMotionTransformed = true;
}

void CScriptPlatform::RotateMotion(const CQuaternion& rotation, const CVector3f& pivot) {
  if (!mSplineController.null()) {
    mSplineController->PositionSpline().Rotate(rotation, pivot);
  }
  SetTranslation(rotation.Transform(GetTranslation() - pivot) + pivot);
  mMotionTransformed = true;
}

void CScriptPlatform::fn_800a1df8() {
  x48d_25_ = true;
  if (mMotionFlags & 8) {
    mMotionActive = true;
  } else {
    StopMotion();
  }
  mDead = false;
  mHealth = mInitialHealth;
}

void CScriptPlatform::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: motion setup/control, riders, bounds trigger and health reset.
  CPhysicsActor::AcceptScriptMsg(mgr, msg);
}

const CCollisionPrimitive* CScriptPlatform::GetCollisionPrimitive() const {
  if (mTreeGroup.null()) {
    return CPhysicsActor::GetCollisionPrimitive();
  }
  return mTreeGroup.get();
}

CTransform4f CScriptPlatform::GetPrimitiveTransform() const {
  CTransform4f xf = GetTransform();
  xf.AddTranslation(GetPrimitiveOffset());
  return xf;
}

void CScriptPlatform::SplashThink(const CAABox& bounds, const CFluidPlane& fluid, float dt,
                                  CStateManager& mgr) const {}

void CScriptPlatform::AddRider(TUniqueId id, CStateManager& mgr,
                               const rstl::optional_object< float >& decayTimer) {
  // Retail copies the timer onto the stack before forwarding it.
  AddRider(mRiders, id, this, mgr, rstl::optional_object< float >(decayTimer));
}

// Retail's out-of-line `rstl::vector< SRiders >::push_back_unsafe`: AddSlave, BuildSlaveList and
// AddRider(vector) each call this one copy (0x800A14DC), so it is this unit's own symbol.
extern "C" void fn_800A14DC(rstl::vector< SRiders >& slaves, const SRiders& slave) {
  rstl::construct(&slaves.mItems[slaves.mCount++], slave);
}

void CScriptPlatform::AddSlave(TUniqueId id, CStateManager& mgr,
                               const rstl::optional_object< float >& decayTimer) {
  rstl::vector< SRiders >::iterator slave =
      rstl::find(mDynamicSlaves.begin(), mDynamicSlaves.end(),
                 SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >()));
  if (slave == mDynamicSlaves.end()) {
    if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(id))) {
      actor->AddMaterial(kMT_PlatformSlave, mgr);
      CTransform4f xf = GetTransform().GetQuickInverse() * actor->GetTransform();
      fn_800A46F0(mDynamicSlaves, mDynamicSlaves.mCount + 1);
      fn_800A14DC(mDynamicSlaves, SRiders(id, xf, rstl::optional_object< float >(decayTimer)));
    }
  } else {
    slave->mDecayTimer = decayTimer;
  }
}

void CScriptPlatform::UpdateSlaveTransforms(CStateManager& mgr) {
  // TODO: recompute dynamic slave transforms recursively.
}

bool CScriptPlatform::IsRider(TUniqueId id) const {
  return rstl::find(mRiders.begin(), mRiders.end(),
                    SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >())) !=
         mRiders.end();
}

// The two ends of retail's out-of-line erase chain. Retail's DOL has no symbol for either, so
// config/G2ME01/symbols.txt names them after their own addresses; our rstl::destroy /
// rstl::destroy_impl instantiations are byte-identical, but they carry mangled names and objdiff
// pairs functions by name, so they can never score. `extern "C"`, which suppresses mangling, is
// how the rest of the tree names an unnamed retail function (see CAnimData.cpp). These are
// rstl::destroy and rstl::destroy_impl for pointer_iterator<SRiders, ...>, which is one word, so
// SRiders* const* passes its arguments identically. Declared descending by retail offset like
// everything else here: mwcceppc emits definitions in reverse source order.
extern "C" void fn_800A1180(SRiders* const* first, SRiders* const* last) {
  for (SRiders* cur = *first; cur != *last; ++cur) {
    cur->~SRiders();
  }
}

extern "C" void fn_800A1148(SRiders* const* first, SRiders* const* last) {
  SRiders* localFirst;
  SRiders* localLast;
  localLast = *last;
  localFirst = *first;
  fn_800A1180(&localFirst, &localLast);
}

// The two ends of the same chain that reach those: rstl::vector<SRiders>::erase(first, last) and
// the one-argument erase(first) that forwards to it, which retail also leaves unnamed. Same
// extern "C" trick for the same reason - the templates in include/rstl/vector.hpp already emit
// these bytes, they just cannot carry retail's names. The vector arrives as a reference in r4,
// where a member function's `this` would be, and the return value is the one-word iterator the
// sret pointer in r3 names, exactly as the member functions pass it.
extern "C" rstl::vector< SRiders >::iterator
fn_800A1050(rstl::vector< SRiders >& riders, rstl::vector< SRiders >::iterator first,
            rstl::vector< SRiders >::iterator last) {
  rstl::destroy(first, last);

  const rstl::vector< SRiders >::iterator::difference_type tmp = first - riders.begin();

  int newCount = tmp;

  for (rstl::vector< SRiders >::iterator it = last,
       moved = rstl::vector< SRiders >::iterator(riders.mItems + tmp);
       it != riders.end(); ++moved, ++newCount, ++it) {
    rstl::construct(&*moved, *it);
    rstl::destroy(&*it);
  }
  riders.mCount = newCount;

  return first;
}

extern "C" rstl::vector< SRiders >::iterator
fn_800A1004(rstl::vector< SRiders >& riders, rstl::vector< SRiders >::iterator it) {
  return fn_800A1050(riders, it, it + 1);
}

bool CScriptPlatform::RemoveRider(TUniqueId id) {
  rstl::vector< SRiders >::iterator it =
      rstl::find(mRiders.begin(), mRiders.end(),
                 SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >()));
  if (it == mRiders.end()) {
    return false;
  }
  fn_800A1004(mRiders, it);
  return true;
}

SRiders::SRiders(TUniqueId uid, const CTransform4f& xf,
                 const rstl::optional_object< float >& decayTimer)
: mUid(uid), mDecayTimer(decayTimer), mTransform(xf) {}

bool CScriptPlatform::IsSlave(TUniqueId id) const {
  return rstl::find(mStaticSlaves.begin(), mStaticSlaves.end(),
                    SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >())) !=
             mStaticSlaves.end() ||
         rstl::find(mDynamicSlaves.begin(), mDynamicSlaves.end(),
                    SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >())) !=
             mDynamicSlaves.end();
}

CQuaternion CScriptPlatform::Move(float dt, CStateManager& mgr) {
  // TODO: spline position/orientation, collision recovery and look-at target.
  return CQuaternion::NoRotation();
}

void CScriptPlatform::PreRender(CStateManager& mgr) {
  CActor::PreRender(mgr);
  if (mgr.GetObjectById(mBoundsTrigger) == nullptr) {
    mBoundsTrigger = kInvalidUniqueId;
  }
}

CVector3f CScriptPlatform::GetOrbitPosition(const CStateManager& mgr) const {
  return GetAimPosition(mgr, 0.f);
}

CVector3f CScriptPlatform::GetAimPosition(const CStateManager& mgr, float dt) const {
  if (GetTouchBounds()) {
    return GetTouchBounds()->GetCenterPoint();
  }
  return CPhysicsActor::GetAimPosition(mgr, dt);
}

CAABox CScriptPlatform::GetSortingBounds(const CStateManager& mgr) const {
  if (mBoundsTrigger != kInvalidUniqueId) {
    const CScriptTrigger* trigger =
        static_cast< const CScriptTrigger* >(mgr.GetObjectById(mBoundsTrigger));
    if (trigger != nullptr) {
      return trigger->GetTriggerBoundsWR();
    }
  }
  return CActor::GetSortingBounds(mgr);
}

void CScriptPlatform::Render(const CStateManager& mgr) const { CPhysicsActor::Render(mgr); }

void CScriptPlatform::SetTransformExplicitly(const CTransform4f& xf) { mCurrentRotation = xf; }

CQuaternion CScriptPlatform::CalculateRotationDelta() {
  CTransform4f delta = mCurrentRotation * mPreviousRotation.GetQuickInverse();
  mPreviousRotation = mCurrentRotation;
  return CQuaternion::FromMatrix(delta);
}

void CScriptPlatform::fn_800a0200(float time, CStateManager& mgr) {
  // TODO: restore initial orientation and clamp the spline time.
}
