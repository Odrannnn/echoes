#include "MetroidPrime/CCollisionActorManager.hpp"

#include "Collision/CMaterialList.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CCollisionActor.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include "Kyoto/Basics/CCast.hpp"

#include <float.h>
#include <math.h>

CJointCollisionDescription
CJointCollisionDescription::SphereCollision(CSegId pivotId, const CVector3f& pivotPoint,
                                            float radius, const rstl::string& name, float mass) {
  return CJointCollisionDescription(kCT_Sphere, pivotId, CSegId::Invalid(), CVector3f::Zero(),
                                    CMatrix3f::Identity(), pivotPoint, radius, 0.f, kOT_Pivot, name,
                                    mass);
}

CJointCollisionDescription CJointCollisionDescription::SphereSubdivideCollision(
    CSegId pivotId, CSegId nextId, float radius, float maxSeparation,
    EOrientationType orientationType, const rstl::string& name, float mass) {
  return CJointCollisionDescription(kCT_SphereSubdivide, pivotId, nextId, CVector3f::Zero(),
                                    CMatrix3f::Identity(), CVector3f::Zero(), radius, maxSeparation,
                                    orientationType, name, mass);
}

CJointCollisionDescription CJointCollisionDescription::AABoxCollision(CSegId pivotId,
                                                                      const CVector3f& bounds,
                                                                      const rstl::string& name,
                                                                      float mass) {
  return CJointCollisionDescription(kCT_AABox, pivotId, CSegId::Invalid(), bounds,
                                    CMatrix3f::Identity(), CVector3f::Zero(), 0.f, 0.f, kOT_Pivot,
                                    name, mass);
}

CJointCollisionDescription CJointCollisionDescription::OBBAutoSizeCollision(
    CSegId pivotId, CSegId nextId, const CVector3f& bounds, EOrientationType orientationType,
    const rstl::string& name, float mass) {
  return CJointCollisionDescription(kCT_OBBAutoSize, pivotId, nextId, bounds, CMatrix3f::Identity(),
                                    CVector3f::Zero(), 0.f, 0.f, orientationType, name, mass);
}

CJointCollisionDescription CJointCollisionDescription::OBBCollision(CSegId pivotId,
                                                                    const CVector3f& bounds,
                                                                    const CVector3f& pivotPoint,
                                                                    const rstl::string& name,
                                                                    float mass) {
  return CJointCollisionDescription(kCT_OBB, pivotId, CSegId::Invalid(), bounds,
                                    CMatrix3f::Identity(), pivotPoint, 0.f, 0.f, kOT_Pivot, name,
                                    mass);
}

CJointCollisionDescription::CJointCollisionDescription(
    ECollisionType type, CSegId pivotId, CSegId nextId, const CVector3f& bounds,
    const CMatrix3f& orientation, const CVector3f& pivotPoint, float radius, float maxSeparation,
    EOrientationType orientationType, const rstl::string& name, float mass)
: mType(type)
, mOrientationType(orientationType)
, mPivotId(pivotId)
, mNextId(nextId)
, mBounds(bounds)
, mPivotPoint(pivotPoint)
, mRadius(radius)
, mMaxSeparation(maxSeparation)
, mName(name)
, mActorId(kInvalidUniqueId)
, mMass(mass)
, mOrientation(orientation) {}

CJointCollisionDescription CJointCollisionDescription::OBBFromMayaPlugInCollision(
    CSegId pivotId, const CVector3f& bounds, const CMatrix3f& orientation,
    const CVector3f& pivotPoint, const rstl::string& name, float mass) {
  return CJointCollisionDescription(kCT_OBBFromMayaPlugIn, pivotId, CSegId::Invalid(), bounds,
                                    orientation, pivotPoint, 0.f, 0.f, kOT_Pivot, name, mass);
}

void CJointCollisionDescription::ScaleAllBounds(const CVector3f& scale) {
  mBounds = CVector3f::ByElementMultiply(scale, mBounds);
  mRadius *= scale.GetX();
  mMaxSeparation *= scale.GetX();
  mPivotPoint = CVector3f::ByElementMultiply(scale, mPivotPoint);
}

CCollisionActorManager::CCollisionActorManager(
    CStateManager& mgr, TUniqueId owner, TAreaId areaId,
    const rstl::vector< CJointCollisionDescription >& descriptions, bool active)
: mOwnerId(owner), mActive(active), mDestroyed(false), mPhysicsActive(true) {
  const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(mOwnerId));
  if (actor == nullptr)
    return;

  const CAnimData* animData = actor->GetAnimationData();
  const CTransform4f& worldXf = actor->GetTransform();
  const CVector3f& scale = actor->GetModelData()->GetScale();
  const CTransform4f scaleXf = CTransform4f::Scale(scale);
  mJointDescriptions.reserve(descriptions.size());
  for (rstl::vector< CJointCollisionDescription >::const_iterator it = descriptions.begin();
       it != descriptions.end(); ++it) {
    CJointCollisionDescription desc = *it;
    desc.ScaleAllBounds(scale);
    const CTransform4f pivotXf =
        GetWRLocatorTransform(*animData, desc.GetPivotId(), worldXf, scaleXf);

    if (desc.GetNextId() == CSegId::Invalid()) {
      const TUniqueId id = mgr.AllocateUniqueId();
      CCollisionActor* colActor;
      if (desc.GetType() == CJointCollisionDescription::kCT_Sphere) {
        colActor =
            rs_new CCollisionActor(id, areaId, mOwnerId, active, desc.GetRadius(), desc.GetMass());
      } else if (desc.GetType() == CJointCollisionDescription::kCT_OBB) {
        colActor = rs_new CCollisionActor(id, areaId, mOwnerId, desc.GetBounds(),
                                          desc.GetPivotPoint(), active, desc.GetMass());
      } else if (desc.GetType() == CJointCollisionDescription::kCT_OBBFromMayaPlugIn) {
        colActor = rs_new CCollisionActor(id, areaId, mOwnerId, 0.5f * desc.GetBounds(),
                                          CVector3f::Zero(), active, desc.GetMass());
      } else {
        colActor =
            rs_new CCollisionActor(id, areaId, mOwnerId, desc.GetBounds(), active, desc.GetMass());
      }

      colActor->SetTransform(pivotXf);
      if (desc.GetType() == CJointCollisionDescription::kCT_Sphere) {
        colActor->SetTranslation(pivotXf.GetTranslation() + pivotXf.Rotate(desc.GetPivotPoint()));
      } else if (desc.GetType() == CJointCollisionDescription::kCT_OBBFromMayaPlugIn) {
        CTransform4f locatorXf = animData->GetLocatorTransform(desc.GetPivotId(), nullptr);
        locatorXf.SetTranslation(CVector3f::ByElementMultiply(scale, locatorXf.GetTranslation()));
        colActor->SetTransform(worldXf * locatorXf *
                               CTransform4f(desc.GetOrientation(), desc.GetPivotPoint()));
      } else {
        colActor->SetTranslation(pivotXf.GetTranslation());
      }
      mgr.AddObject(colActor);
      mJointDescriptions.push_back_unsafe(*it);
      (mJointDescriptions.end() - 1)->SetCollisionActorId(id);
      continue;
    }

    const CTransform4f nextXf =
        GetWRLocatorTransform(*animData, desc.GetNextId(), worldXf, scaleXf);
    const float distance = (nextXf.GetTranslation() - pivotXf.GetTranslation()).Magnitude();
    if (desc.GetType() == CJointCollisionDescription::kCT_OBBAutoSize) {
      if (distance <= FLT_EPSILON)
        continue;

      const CVector3f bounds(desc.GetBounds().GetX(), distance + desc.GetBounds().GetY(),
                             desc.GetBounds().GetZ());
      const CVector3f center(0.f, 0.5f * distance, 0.f);
      const TUniqueId id = mgr.AllocateUniqueId();
      CCollisionActor* colActor =
          rs_new CCollisionActor(id, areaId, mOwnerId, bounds, center, active, desc.GetMass());
      if (desc.GetOrientationType() == CJointCollisionDescription::kOT_Pivot) {
        colActor->SetTransform(pivotXf);
      } else {
        CVector3f up = pivotXf.GetColumn(kDZ);
        const CVector3f direction =
            (nextXf.GetTranslation() - pivotXf.GetTranslation()).AsNormalized();
        if (fabsf(1.f - fabsf(CVector3f::Dot(direction, up))) < 100.f * FLT_EPSILON)
          up = pivotXf.GetColumn(kDY);
        colActor->SetTransform(CTransform4f::LookAt(pivotXf.GetTranslation(),
                                                    pivotXf.GetTranslation() + direction, up));
      }
      mgr.AddObject(colActor);
      mJointDescriptions.push_back_unsafe(*it);
      (mJointDescriptions.end() - 1)->SetCollisionActorId(id);
      continue;
    }

    const TUniqueId id = mgr.AllocateUniqueId();
    CCollisionActor* colActor =
        rs_new CCollisionActor(id, areaId, mOwnerId, active, desc.GetRadius(), desc.GetMass());
    colActor->SetTransform(pivotXf);
    mgr.AddObject(colActor);
    mJointDescriptions.push_back_unsafe(CJointCollisionDescription::SphereCollision(
        desc.GetPivotId(), CVector3f::Zero(), desc.GetRadius(), desc.GetName(), 0.001f));
    (mJointDescriptions.end() - 1)->SetCollisionActorId(id);

    const uint numSeparations = CCast::ToUint32(distance / desc.GetMaxSeparation());
    if (numSeparations == 0)
      continue;
    mJointDescriptions.reserve(mJointDescriptions.capacity() + numSeparations);
    const float pitch = distance / float(numSeparations + 1);
    for (uint i = 0; i < numSeparations; ++i) {
      const float separation = pitch * float(i + 1);
      mJointDescriptions.push_back_unsafe(CJointCollisionDescription::SphereSubdivideCollision(
          desc.GetPivotId(), desc.GetNextId(), desc.GetRadius(), separation,
          CJointCollisionDescription::kOT_BetweenJoints, desc.GetName(), 0.001f));
      const TUniqueId newId = mgr.AllocateUniqueId();
      CCollisionActor* newActor =
          rs_new CCollisionActor(newId, areaId, mOwnerId, active, desc.GetRadius(), desc.GetMass());
      CVector3f direction = pivotXf.GetColumn(kDY);
      if (desc.GetOrientationType() == CJointCollisionDescription::kOT_BetweenJoints) {
        CVector3f up = pivotXf.GetColumn(kDZ);
        const CVector3f between =
            (nextXf.GetTranslation() - pivotXf.GetTranslation()).AsNormalized();
        if (fabsf(1.f - fabsf(CVector3f::Dot(between, up))) < 100.f * FLT_EPSILON)
          up = direction;
        direction = CTransform4f::LookAt(CVector3f::Zero(), between, up).GetColumn(kDY);
      }
      newActor->SetTransform(
          CTransform4f::Translate(pivotXf.GetTranslation() + separation * direction));
      mgr.AddObject(newActor);
      (mJointDescriptions.end() - 1)->SetCollisionActorId(newId);
    }
  }
}

CCollisionActorManager::~CCollisionActorManager() {}

void CCollisionActorManager::Update(float dt, CStateManager& mgr, EUpdateOptions options) {
  if (!mPhysicsActive)
    SetPhysicsActive(mgr, true);
  if (!mActive)
    return;

  const CActor* owner = TCastToConstPtr< CActor >(mgr.GetObjectById(mOwnerId));
  if (owner == nullptr)
    return;

  const CAnimData* animData = owner->GetAnimationData();
  const CTransform4f worldXf = owner->GetTransform();
  const CTransform4f scaleXf = CTransform4f::Scale(owner->GetModelData()->GetScale());
  for (int i = 0; i < mJointDescriptions.size(); ++i) {
    const CJointCollisionDescription& desc = mJointDescriptions[i];
    CCollisionActor* actor =
        TCastToPtr< CCollisionActor >(mgr.ObjectById(desc.GetCollisionActorId()));
    if (actor == nullptr)
      continue;

    const CTransform4f pivotXf =
        GetWRLocatorTransform(*animData, desc.GetPivotId(), worldXf, scaleXf);
    CVector3f origin = pivotXf.GetTranslation();
    if (desc.GetType() == CJointCollisionDescription::kCT_OBB ||
        desc.GetType() == CJointCollisionDescription::kCT_OBBAutoSize) {
      if (desc.GetOrientationType() == CJointCollisionDescription::kOT_Pivot) {
        actor->SetRotation(CQuaternion::FromMatrix(pivotXf));
      } else {
        const CTransform4f nextXf =
            GetWRLocatorTransform(*animData, desc.GetNextId(), worldXf, scaleXf);
        actor->SetRotation(CQuaternion::FromMatrix(CTransform4f::LookAt(
            pivotXf.GetTranslation(), nextXf.GetTranslation(), pivotXf.GetColumn(kDZ))));
      }
    } else if (desc.GetType() == CJointCollisionDescription::kCT_SphereSubdivide) {
      if (desc.GetOrientationType() == CJointCollisionDescription::kOT_Pivot) {
        origin += desc.GetMaxSeparation() * pivotXf.GetColumn(kDY);
      } else {
        const CTransform4f nextXf =
            GetWRLocatorTransform(*animData, desc.GetNextId(), worldXf, scaleXf);
        const float maxSep = desc.GetMaxSeparation();
        origin += maxSep * CTransform4f::LookAt(origin, nextXf.GetTranslation(),
                                                pivotXf.GetColumn(kDZ))
                                   .GetColumn(kDY);
      }
    }

    if (options == kUO_ObjectSpace) {
      const CVector3f movement = actor->GetTransform().TransposeMultiply(origin);
      actor->MoveToOR(movement, dt);
    } else if (desc.GetType() == CJointCollisionDescription::kCT_Sphere)
      actor->SetTranslation(pivotXf.GetTranslation() +
                            pivotXf.BuildMatrix3f() * desc.GetPivotPoint());
    else if (desc.GetType() == CJointCollisionDescription::kCT_OBBFromMayaPlugIn) {
      CTransform4f locatorXf = animData->GetLocatorTransform(desc.GetPivotId(), nullptr);
      locatorXf.SetTranslation(CVector3f::ByElementMultiply(owner->GetModelData()->GetScale(),
                                                            locatorXf.GetTranslation()));
      const CTransform4f orientXf = CTransform4f(desc.GetOrientation(), desc.GetPivotPoint());
      const CTransform4f xf = worldXf * locatorXf * orientXf;
      actor->SetTransform(xf);
    } else
      actor->SetTranslation(pivotXf.GetTranslation());
  }
}

void CCollisionActorManager::Destroy(CStateManager& mgr) const {
  for (int i = 0; i < mJointDescriptions.size(); ++i)
    mgr.DeleteObjectRequest(mJointDescriptions[i].GetCollisionActorId());
  mDestroyed = true;
}

void CCollisionActorManager::SetActive(CStateManager& mgr, bool active) {
  mActive = active;
  for (int i = 0; i < mJointDescriptions.size(); ++i) {
    CEntity* entity = mgr.ObjectById(mJointDescriptions[i].GetCollisionActorId());
    if (entity != nullptr && entity->GetActive() != active) {
      entity->SetActive(active);
      if (active)
        Update(0.f, mgr, kUO_WorldSpace);
    }
  }
}

void CCollisionActorManager::AddMaterialList(CStateManager& mgr, const CMaterialList& materials) {
  for (int i = 0; i < mJointDescriptions.size(); ++i) {
    CActor* actor =
        TCastToPtr< CActor >(mgr.ObjectById(mJointDescriptions[i].GetCollisionActorId()));
    if (actor != nullptr)
      actor->AddMaterial(materials);
  }
}

void CCollisionActorManager::RemoveMaterialList(CStateManager& mgr,
                                                const CMaterialList& materials) {
  for (int i = 0; i < mJointDescriptions.size(); ++i) {
    CActor* actor =
        TCastToPtr< CActor >(mgr.ObjectById(mJointDescriptions[i].GetCollisionActorId()));
    if (actor != nullptr)
      actor->MaterialList().Remove(materials);
  }
}

uint CCollisionActorManager::GetNumCollisionActors() const { return mJointDescriptions.size(); }

const CJointCollisionDescription&
CCollisionActorManager::GetCollisionDescFromIndex(uint index) const {
  return mJointDescriptions[index];
}

// retail 0x80135A88 keeps two induction variables here, not one: `r6` walks the vector in
// **bytes** and `r8` counts elements, and the element is fetched as `mItems + r6` with an
// `lhzx` (0x80135AA8 `addi r3,r6,60` / `lhzx r3,r5,r3`) rather than through a pointer that
// walks. It also reads `mItems` (`lwz r5,12(r3)`, 0x80135A90) **once, before the loop**.
// Written as `mJointDescriptions[i]` mwcceppc re-reads `12(r3)` inside the loop instead
// (75.79%), and a plain `data()` local makes it strength-reduce to a single pointer
// induction variable and drop two instructions (83.95%). Naming the byte offset explicitly
// gets both: the hoisted base and the `lhzx` pair. The `(int)` casts matter - without them
// the same source is 46.05%, because the uncast `sizeof` changes the division's type and
// with it the register the offset lands in. 96.05%, still short of retail's register
// numbering (`r6` count / `r8` index / `r5` base / `r6` offset against ours `r6` / `r7` /
// `r5` / `r3`); see docs/goal-notes/progress-unit-ccollisionactormanager.md.
int CCollisionActorManager::GetCollisionDescIndexFromUniqueId(TUniqueId id) const {
  const CJointCollisionDescription* items = mJointDescriptions.data();
  for (int i = 0, offset = 0; i < mJointDescriptions.size();
       offset += (int)sizeof(CJointCollisionDescription), ++i) {
    if (items[offset / (int)sizeof(CJointCollisionDescription)].GetCollisionActorId() == id)
      return i;
  }
  return -1;
}

CTransform4f CCollisionActorManager::GetWRLocatorTransform(const CAnimData& animData, CSegId id,
                                                           const CTransform4f& worldXf,
                                                           const CTransform4f& scaleXf) {
  CTransform4f locatorXf = animData.GetLocatorTransform(id, nullptr);
  const CVector3f origin = worldXf * (scaleXf * locatorXf.GetTranslation());
  locatorXf = worldXf.MultiplyIgnoreTranslation(locatorXf);
  locatorXf.SetTranslation(origin);
  return locatorXf;
}

void CCollisionActorManager::SetPhysicsActive(CStateManager& mgr, bool active) {
  if (active == mPhysicsActive)
    return;
  mPhysicsActive = active;
  for (int i = 0; i < mJointDescriptions.size(); ++i) {
    CCollisionActor* actor =
        TCastToPtr< CCollisionActor >(mgr.ObjectById(mJointDescriptions[i].GetCollisionActorId()));
    if (actor != nullptr) {
      actor->SetMovable(mPhysicsActive);
      actor->SetUseInSortedLists(mPhysicsActive);
    }
  }
}

// retail's own 8-byte `.sbss` object for this translation unit (`lbl_804191A8`, i.e. DOL
// 0x804191A8, the unit's `.sbss` at `build/report.json` -> sections). Nothing in the DOL
// reads or writes the second word.
extern "C" TAreaId lbl_804191A8[2];

// retail 0x801358B4, `fn_801358B4`: three instructions, the first definition in retail's
// `.text` for this unit and therefore the **last** one in retail's source file. It is the
// only reference to `lbl_804191A8` in retail's object, and nothing in the image calls it,
// so what it is for is not recoverable from the bytes; what it does is not in doubt -
// `lwz r0,-27736(r13) ; stw r0,-27608(r13) ; blr`, a copy of `kInvalidAreaId`
// (`0x80419128`) into this unit's own zeroed `.sbss` word. Reproducing it needs the
// C-linkage name retail's symbol table gives it, hence `extern "C"` (the same shape as
// `PortCTweakPlayerControls.cpp`'s `fn_80215860`).
extern "C" void fn_801358B4() { lbl_804191A8[0] = kInvalidAreaId; }
