#include "WorldFormat/CCollidableOBBTree.hpp"

#include "Collision/CInternalRayCastStructure.hpp"
#include "Collision/CRayCastResult.hpp"

static CPlane TransformPlane(const CPlane& plane, const CTransform4f& xf);

uint CCollidableOBBTree::sTableIndex = -1;

CCollidableOBBTree::CCollidableOBBTree(COBBTree* tree, const CMaterialList& material)
: CCollisionPrimitive(material), mTree(tree), mTries(0), mMisses(0), mHits(0) {}

CCollidableOBBTree::~CCollidableOBBTree() {}

CAABox CCollidableOBBTree::CalculateAABox(const CTransform4f& xf) const {
  COBBox obb = COBBox::FromAABox(GetOBBTree().CalculateLocalAABox(), xf);
  return obb.CalculateAABox(CTransform4f::Identity());
}

CAABox CCollidableOBBTree::CalculateLocalAABox() const {
  return GetOBBTree().CalculateLocalAABox();
}

FourCC CCollidableOBBTree::GetPrimType() const { return 'OBBT'; }

bool CCollidableOBBTree::AABoxCollision(const COBBTree::CNode& node, const CTransform4f& xf,
                                        const CAABox& box, const COBBox& obb,
                                        const CMaterialList& material,
                                        const CMaterialFilter& filter, const CPlane* planes,
                                        CCollisionInfoList& infoList) const {
  bool ret = false;

  mTries += 1;
  if (obb.OBBIntersectsBox(node.GetOBB())) {
    node.SetHit(true);
    if (node.IsLeaf()) {
      if (AABoxCollideWithLeaf(*node.GetLeafData(), xf, box, material, filter, planes, infoList)) {
        ret = true;
      }
    } else {
      if (node.GetLeftNode() && AABoxCollision(*node.GetLeftNode(), xf, box, obb, material, filter,
                                                planes, infoList)) {
        ret = true;
      }
      if (node.GetRightNode() && AABoxCollision(*node.GetRightNode(), xf, box, obb, material,
                                                 filter, planes, infoList)) {
        ret = true;
      }
    }
  } else {
    mMisses += 1;
  }

  return ret;
}

bool CCollidableOBBTree::AABoxCollideWithLeaf(const COBBTree::CLeafData& leaf,
                                              const CTransform4f& xf, const CAABox& box,
                                              const CMaterialList& material,
                                              const CMaterialFilter& filter, const CPlane* planes,
                                              CCollisionInfoList& infoList) const {
  // TODO: Filter transformed triangles, clip against the planes, and append contacts.
  return false;
}

bool CCollidableOBBTree::SphereCollision(const COBBTree::CNode& node, const CTransform4f& xf,
                                         const CSphere& sphere, const COBBox& obb,
                                         const CMaterialList& material,
                                         const CMaterialFilter& filter,
                                         CCollisionInfoList& infoList) const {
  bool ret = false;
  mTries += 1;
  if (obb.OBBIntersectsBox(node.GetOBB())) {
    node.SetHit(true);
    if (node.IsLeaf()) {
      if (SphereCollideWithLeaf(*node.GetLeafData(), xf, sphere, material, filter, infoList)) {
        ret = true;
      }
    } else {
      if (node.GetLeftNode() &&
          SphereCollision(*node.GetLeftNode(), xf, sphere, obb, material, filter, infoList)) {
        ret = true;
      }
      if (node.GetRightNode() &&
          SphereCollision(*node.GetRightNode(), xf, sphere, obb, material, filter, infoList)) {
        ret = true;
      }
    }
  } else {
    mMisses += 1;
  }
  return ret;
}

bool CCollidableOBBTree::SphereCollideWithLeaf(const COBBTree::CLeafData& leaf,
                                               const CTransform4f& xf, const CSphere& sphere,
                                               const CMaterialList& material,
                                               const CMaterialFilter& filter,
                                               CCollisionInfoList& infoList) const {
  // TODO: Test filtered triangles and append sphere contacts with combined materials.
  return false;
}

bool CCollidableOBBTree::AABoxCollisionBoolean(const COBBTree::CNode& node, const CTransform4f& xf,
                                               const CAABox& box, const COBBox& obb,
                                               const CMaterialFilter& filter) const {
  // TODO: Traverse intersecting nodes and stop at the first filtered triangle overlap.
  return false;
}

bool CCollidableOBBTree::SphereCollisionBoolean(const COBBTree::CNode& node, const CTransform4f& xf,
                                                const CSphere& sphere, const COBBox& obb,
                                                const CMaterialFilter& filter) const {
  // TODO: Traverse intersecting nodes and stop at the first filtered triangle overlap.
  return false;
}

bool CCollidableOBBTree::AABoxCollisionMoving(
    const COBBTree::CNode& node, const CTransform4f& xf, const CAABox& box, const COBBox& obb,
    const CMaterialList& material, const CMaterialFilter& filter,
    const CMetroidAreaCollider::CMovingAABoxComponents& components, const CVector3f& direction,
    double& time, CCollisionInfo& info) const {
  bool ret = false;
  mTries += 1;
  if (obb.OBBIntersectsBox(node.GetOBB())) {
    node.SetHit(true);
    if (node.IsLeaf()) {
      if (AABoxCollideWithLeafMoving(*node.GetLeafData(), xf, box, material, filter, components,
                                     direction, time, info)) {
        ret = true;
      }
    } else {
      if (node.GetLeftNode() && AABoxCollisionMoving(*node.GetLeftNode(), xf, box, obb, material,
                                                      filter, components, direction, time, info)) {
        ret = true;
      }
      if (node.GetRightNode() && AABoxCollisionMoving(*node.GetRightNode(), xf, box, obb, material,
                                                       filter, components, direction, time, info)) {
        ret = true;
      }
    }
  } else {
    mMisses += 1;
  }
  return ret;
}

bool CCollidableOBBTree::AABoxCollideWithLeafMoving(
    const COBBTree::CLeafData& leaf, const CTransform4f& xf, const CAABox& box,
    const CMaterialList& material, const CMaterialFilter& filter,
    const CMetroidAreaCollider::CMovingAABoxComponents& components, const CVector3f& direction,
    double& time, CCollisionInfo& info) const {
  // TODO: Sweep the box against triangles, vertices, and edges using the shared duplicate cache.
  return false;
}

bool CCollidableOBBTree::CacheTree(CCollisionCacheWriter& writer, const COBBTree::CNode& node,
                                   const CTransform4f& xf, const CVector3f& center,
                                   const CVector3f& halfExtent, const COBBox& obb) const {
  // TODO: Append overlapping leaf triangles to the packed cache; the target returns false.
  return false;
}

void CCollidableOBBTree::CacheSphere(CCollisionCache& cache, const CTransform4f& xf, short ownerId,
                                     u64 material) {
  // TODO: Choose a prebuilt sphere by scale and cache triangles inside the query bounds.
}

void CCollidableOBBTree::CacheAABox(CCollisionCache& cache, const CTransform4f& xf, short ownerId,
                                    u64 material) {
  // TODO: Cache overlapping triangles from the transformed prebuilt unit cube.
}

bool CCollidableOBBTree::SphereCollisionMoving(const COBBTree::CNode& node, const CTransform4f& xf,
                                               const CSphere& sphere, const COBBox& obb,
                                               const CMaterialList& material,
                                               const CMaterialFilter& filter,
                                               const CVector3f& direction, double& time,
                                               CCollisionInfo& info) const {
  bool ret = false;
  mTries += 1;
  if (obb.OBBIntersectsBox(node.GetOBB())) {
    node.SetHit(true);
    if (node.IsLeaf()) {
      if (SphereCollideWithLeafMoving(*node.GetLeafData(), xf, sphere, material, filter, direction,
                                      time, info)) {
        ret = true;
      }
    } else {
      if (node.GetLeftNode() && SphereCollisionMoving(*node.GetLeftNode(), xf, sphere, obb, material,
                                                       filter, direction, time, info)) {
        ret = true;
      }
      if (node.GetRightNode() && SphereCollisionMoving(*node.GetRightNode(), xf, sphere, obb,
                                                        material, filter, direction, time, info)) {
        ret = true;
      }
    }
  } else {
    mMisses += 1;
  }
  return ret;
}

bool CCollidableOBBTree::SphereCollideWithLeafMoving(const COBBTree::CLeafData& leaf,
                                                     const CTransform4f& xf, const CSphere& sphere,
                                                     const CMaterialList& material,
                                                     const CMaterialFilter& filter,
                                                     const CVector3f& direction, double& time,
                                                     CCollisionInfo& info) const {
  // TODO: Sweep the sphere against triangle faces, edges, and vertices, retaining the first hit.
  return false;
}

CRayCastResult CCollidableOBBTree::CastRayInternal(const CInternalRayCastStructure& rayCast) const {
  return LineIntersectsTree(rayCast.GetRay(), rayCast.GetFilter(), rayCast.GetMaxTime(),
                            rayCast.GetTransform());
}

static CPlane TransformPlane(const CPlane& pl, const CTransform4f& xf) {
  CVector3f transformed = xf * (pl.GetNormal() * pl.GetConstant());
  CVector3f normal = xf.Rotate(pl.GetNormal());
  return CPlane(CVector3f::Dot(transformed, normal),
                CUnitVector3f(normal.GetX(), normal.GetY(), normal.GetZ()));
}

CRayCastInfo::CRayCastInfo(const CMRay& ray, const CMaterialFilter& filter, float magnitude)
: mRay(ray)
, mFilter(filter)
, mMagnitude(magnitude)
, mPlane(CVector3f::Zero(), CUnitVector3f(CVector3f(0.f, 0.f, 1.f), CUnitVector3f::kN_Yes))
, mMaterial() {}

CRayCastResult CCollidableOBBTree::LineIntersectsTree(const CMRay& ray,
                                                      const CMaterialFilter& filter, float maxTime,
                                                      const CTransform4f& xf) const {
  CMRay localRay = ray.GetInvUnscaledTransformRay(xf);
  CRayCastInfo info(localRay, filter, maxTime);
  if (LineIntersectsOBBTree(GetOBBTree().GetRoot(), info)) {
    const CPlane plane = TransformPlane(info.GetPlane(), xf);
    return CRayCastResult(info.GetMagnitude(),
                          ray.GetStart() + info.GetMagnitude() * ray.GetDirection(), plane,
                          info.GetMaterial());
  }
  return CRayCastResult::MakeInvalid();
}

bool CCollidableOBBTree::LineIntersectsOBBTree(const COBBTree::CNode* node,
                                               CRayCastInfo& info) const {
  if (!node) {
    return false;
  }

  float t;
  bool ret = false;

  mTries += 1;
  if (node->GetOBB().LineIntersectsBox(info.GetRay(), t) && t < info.GetMagnitude()) {
    if (node->IsLeaf() == true) {
      if (LineIntersectsLeaf(*node->GetLeafData(), info) == true) {
        ret = true;
      }
    } else {
      if (LineIntersectsOBBTree(node->GetLeftNode(), node->GetRightNode(), info) == true) {
        ret = true;
      }
    }
    node->SetHit(true);
  } else {
    mMisses += 1;
  }

  return ret;
}

bool CCollidableOBBTree::LineIntersectsOBBTree(const COBBTree::CNode* n0, const COBBTree::CNode* n1,
                                               CRayCastInfo& info) const {
  bool ret = false;
  float t0 = 0.f;
  float t1 = 0.f;

  mTries += 2;

  const bool intersects0 = n0 && n0->GetOBB().LineIntersectsBox(info.GetRay(), t0) == true &&
                           t0 < info.GetMagnitude();
  const bool intersects1 = n1 && n1->GetOBB().LineIntersectsBox(info.GetRay(), t1) == true &&
                           t1 < info.GetMagnitude();

  if (intersects0 && intersects1) {
    if (t0 < t1) {
      if ((n0->IsLeaf() == true
               ? LineIntersectsLeaf(*n0->GetLeafData(), info)
               : LineIntersectsOBBTree(n0->GetLeftNode(), n0->GetRightNode(), info)) == true) {
        if (info.GetMagnitude() < t1) {
          return true;
        }
        ret = true;
      }
      if (n1->IsLeaf()) {
        if (LineIntersectsLeaf(*n1->GetLeafData(), info)) {
          ret = true;
        }
      } else {
        if (LineIntersectsOBBTree(n1->GetLeftNode(), n1->GetRightNode(), info) == true) {
          ret = true;
        }
      }
    } else {
      if ((n1->IsLeaf() == true
               ? LineIntersectsLeaf(*n1->GetLeafData(), info)
               : LineIntersectsOBBTree(n1->GetLeftNode(), n1->GetRightNode(), info)) == true) {
        if (info.GetMagnitude() < t0) {
          return true;
        }
        ret = true;
      }
      if (n0->IsLeaf()) {
        if (LineIntersectsLeaf(*n0->GetLeafData(), info)) {
          ret = true;
        }
      } else {
        if (LineIntersectsOBBTree(n0->GetLeftNode(), n0->GetRightNode(), info) == true) {
          ret = true;
        }
      }
    }
  } else {
    if (intersects0) {
      if (n0->IsLeaf() == true) {
        if (LineIntersectsLeaf(*n0->GetLeafData(), info)) {
          return true;
        }
      } else {
        if (LineIntersectsOBBTree(n0->GetLeftNode(), n0->GetRightNode(), info) == true) {
          return true;
        }
      }
    }
    if (intersects1) {
      if (n1->IsLeaf() == true) {
        if (LineIntersectsLeaf(*n1->GetLeafData(), info)) {
          return true;
        }
      } else {
        if (LineIntersectsOBBTree(n1->GetLeftNode(), n1->GetRightNode(), info) == true) {
          return true;
        }
      }
    }
  }

  return ret;
}

bool CCollidableOBBTree::LineIntersectsLeaf(const COBBTree::CLeafData& leaf,
                                            CRayCastInfo& info) const {
  // TODO: Intersect filtered triangles, retaining the nearest plane and triangle material.
  return false;
}

uint CCollidableOBBTree::GetTableIndex() const { return sTableIndex; }
