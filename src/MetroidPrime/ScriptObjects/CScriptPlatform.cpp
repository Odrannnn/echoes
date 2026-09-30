#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/ScriptObjects/CScriptTrigger.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

#include "rstl/algorithm.hpp"

#include "Kyoto/Math/CGameSplineDesc.hpp"

// `x == 1`, out of line: the 4-instruction leaf at 0x800A4840 that `CGameCollision` and
// `CGroundMovement` call after masking an id down to its low 9 bits (`clrlwi. r0,rX,24` at
// 0x80124FE4 and 0x80125030). Retail's symbol table names nothing here, so dtk calls it after
// its address; `subfic/cntlzw/srwi` is CodeWarrior's spelling of `arg == 1` and nothing else.
extern "C" bool fn_800A4840(int id) { return id == 1; }

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
// `CGameSplineDesc::operator=`. The `SLdrSpline` member at offset 0 is **copy-constructed**, not
// assigned - the call at 0x800A45B8 is `__ct__11CMayaSplineFRC11CMayaSpline`, and its `this` is
// the object itself, so this is a placement-new over a live member - and then the three trailing
// members are copied one field at a time: `mType` (int, 0x44), `mDuration` (float, 0x48) and
// `mClosedLoop` (bool, 0x4c). The mirror struct is the same trick `fn_800D042C` in
// `src/MetroidPrime/Player/CMorphBall.cpp` uses, and it is needed because those three members are
// private. Retail's symbol table gives this function no name, so dtk calls it after its address.
extern "C" CGameSplineDesc* fn_800A469C(CGameSplineDesc* self, const CGameSplineDesc& other) {
  struct SMirror {
    SLdrSpline mSpline;
    CMotionSpline::ESplineType mType;
    float mDuration;
    bool mClosedLoop;
  };
  SMirror* dst = reinterpret_cast< SMirror* >(self);
  const SMirror* src = reinterpret_cast< const SMirror* >(&other);
  rstl::construct(&dst->mSpline, src->mSpline);
  dst->mType = src->mType;
  dst->mDuration = src->mDuration;
  dst->mClosedLoop = src->mClosedLoop;
  return self;
}

// `rstl::single_ptr<CMayaSpline>::operator=(T* const)`, emitted out of line by CodeWarrior and
// called from the constructor once per roll/yaw/pitch spline (0x800A43A8, 0x800A43E8, 0x800A4428).
// Retail's symbol table gives it no name, so dtk calls it after its address. The body is
// `include/rstl/single_ptr.hpp`'s: the old pointee's deleting destructor takes the flag in r4
// (`li r4,1`), and only r30 is saved because the store of the new pointer needs the incoming r4
// across that call.
extern "C" rstl::single_ptr< CMayaSpline >* fn_800A4654(rstl::single_ptr< CMayaSpline >* self,
                                                        CMayaSpline* ptr) {
  delete self->mPtr;
  self->mPtr = ptr;
  return self;
}

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

// The two `rstl::single_ptr` deleting destructors this unit needs. `__dt__` calls the first three
// times (0x800A3F04, 0x800A3F10, 0x800A3F1C - one per mRollSpline / mYawSpline / mPitchSpline) and
// the second once (0x800A3F78, mMotionSpline); retail's symbol table names neither, so dtk calls
// them after their addresses. Our object emits both bodies already, as the weak
// `__dt__Q24rstl25single_ptr<11CMayaSpline>Fv` and `__dt__Q24rstl35single_ptr<21SPlatform...>Fv`
// instantiations, and they are instruction-for-instruction what is below; a template instantiation
// cannot carry retail's name, so the bodies are written out. The three details that decide the
// register allocation are the same three `src/MetroidPrime/Player/CGameStateBlockDtor.cpp` records
// for this shape: the flag is a **`short`**, the return type is a **pointer**, and `this` is tested
// once (`mr. r30,r3 ; beq`) with the epilogue's `mr r3,r30` being the return.
//
// The pointee is destroyed with `delete`, which is what puts the deleting flag `1` in r4
// (`li r4,1` at 0x800A3E18); a spelled-out `p->~T()` call leaves r4 at -1 instead and costs the
// function its last percent. The free of the block is guarded by *this* function's own flag, not
// that one - which is why the `Free` after the `extsh.` is on `self` and not on `self->mPtr`.
extern "C" rstl::single_ptr< CMayaSpline >* fn_800A4090(rstl::single_ptr< CMayaSpline >* self,
                                                        int flag) {
  if (self != nullptr) {
    delete self->mPtr;
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// The same for mMotionSpline. The pointee is typed `CGameSplineDesc` and not
// `SPlatformMotionSpline` because that is the name in retail's own call at 0x800A3E60
// (`__dt__15CGameSplineDescFv`); the two are both 0x50 bytes and the member is only ever handed
// back to the constructor, so the pointee's type is not load-bearing here.
extern "C" rstl::single_ptr< CGameSplineDesc >*
fn_800A4038(rstl::single_ptr< CGameSplineDesc >* self, int flag) {
  if (self != nullptr) {
    delete self->mPtr;
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}



extern "C" void fn_800A1148(SRiders* const* first, SRiders* const* last);

// `rstl::destroy`'s two-iterator forward. Retail's `fn_800A31A0` inlines the `destroy` call and
// calls the out-of-line `destroy_impl` (`fn_800A1148`) with the two by-value parameters *by
// address*, which is what leaves four stores in the frame rather than two. `static` so the
// compiler inlines it and the object gains no symbol retail's does not have.
static void fn_800A31A0_destroy(SRiders* const* first, SRiders* const* last) {
  fn_800A1148(first, last);
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
    (*it).mDecayTimer = decayTimer;
  }
}

CScriptPlatform::TNearList
CScriptPlatform::BuildNearListFromRiders(CStateManager& mgr,
                                         const rstl::vector< SRiders >& riders) {
  // Retail keeps only the loop cursor in a register here: writing `it != riders.end()` in the
  // condition instead of hoisting `end` into a local is what drops the hoisted pointer's live
  // range, and the register assignment (r27=sret, r28=mgr, r29=result, r30=cursor, r31=end)
  // only matches retail that way.
  TNearList result;
  for (rstl::vector< SRiders >::const_iterator it = riders.begin(); it != riders.end(); ++it) {
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

// `rstl::vector<SRiders>::~vector(int)`, out of line: `PreThink` (0x800A2C14, 0x800A2C84,
// 0x800A2CC8, 0x800A3170) and `__dt__` (0x800A3F84, 0x800A3F90, 0x800A3F9C - one per
// mDynamicSlaves / mStaticSlaves / mRiders) all call this one copy, so it is this unit's own
// symbol and retail's name for it is only its address. Our object already emits the identical body
// as the weak `__dt__Q24rstl43vector<7SRiders,...>Fv` instantiation, which cannot carry retail's
// name, so the body is written out here. Three details are measurements, and each is the same one
// `src/MetroidPrime/Player/CGameStateBlockDtor.cpp` records for the same shape: the flag parameter
// is a **`short`** (an `int` gives `cmpwi r31,0` where retail has `extsh. r0,r31`), the return
// type is a **pointer** (a `void` leaf loses the trailing `mr r3,r30`), and the element count is
// read from `mCount` *before* `mItems` so the multiply lands on the count register.
extern "C" rstl::vector< SRiders >* fn_800A31A0(rstl::vector< SRiders >* self, int flag) {
  if (self != nullptr) {
    // Both ends are named and passed **by value** through `destroy` and then by address to
    // `fn_800A1148`, which is the shape that produces retail's four frame stores (r1+8 and r1+16
    // hold the by-value copies, r1+12 and r1+20 the addresses) and its r3 = r1+20 / r4 = r1+12.
    // Handing `fn_800A1148` two `SRiders*` locals directly stores two, not four. The *count* is
    // also read before the *items* pointer: retail's 0x2fc0/0x2fc8 load 0x4(r30) then 0xc(r30) and
    // set up r3/r4 before the `mulli`, and the end is `items + count` on an `SRiders*`
    // (`mulli r0,r0,0x3c`), not `&items + count` on an `SRiders**` (`slwi r0,r0,2`).
    // `last` is declared first: retail's `last` pair sits at r1+8/r1+12 with its address in r4,
    // and `first`'s pair above it at r1+16/r1+20 with its address in r3.
    SRiders* lastItems = self->mItems + self->mCount;
    SRiders* firstItems = self->mItems;
    fn_800A31A0_destroy(&firstItems, &lastItems);
    CMemory::Free(self->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
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

// `rstl::single_ptr<CGameSplineDesc>::operator=(T* const)` - the same function as `fn_800A4654`
// below, for the other pointee. `AcceptScriptMsg` calls this one copy at 0x800A196C with
// `addi r3,r31,1064` (mMotionSpline) and `li r4,0`, i.e. it releases the spline, and the pointee's
// destructor name in that call (`__dt__15CGameSplineDescFv`) is what fixes the type.
extern "C" rstl::single_ptr< CGameSplineDesc >*
fn_800A1CA0(rstl::single_ptr< CGameSplineDesc >* self, CGameSplineDesc* ptr) {
  delete self->mPtr;
  self->mPtr = ptr;
  return self;
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
    (*slave).mDecayTimer = decayTimer;
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
