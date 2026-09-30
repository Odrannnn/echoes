#include "MetroidPrime/CGameCollision.hpp"

#include "Collision/CCollidableAABoxSphere.hpp"
#include "Collision/CCollidableSphere.hpp"
#include "Collision/CCollisionInfoList.hpp"
#include "Collision/CCollisionPrimitive.hpp"
#include "Collision/CInternalRayCastStructure.hpp"
#include "Collision/CMaterialFilter.hpp"
#include "Collision/CRayCastResult.hpp"
#include "Kyoto/Math/CLine.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "WorldFormat/CAreaOctTree.hpp"
#include "WorldFormat/CMetroidAreaCollider.hpp"
#include "rstl/math.hpp"

#include <float.h>

// 138, not the project's 125 (configure.py's `-pragma "inline_max_size(125)"`). This unit needs
// `CAreaOctTree::Node`'s constructor inlined into `fn_8012753C` below - retail emits that 18-
// instruction constructor body out of line and the threshold that produces it starts at 130.
// Measured: 130/135/138 give 15/52 functions at 100% with **no** function in the unit worse;
// 140 crosses a second threshold and drops `RayStaticIntersection` 87.48 -> 76.25 and
// `RayDynamicIntersection` 89.28 -> 74.66. mwceppc takes the last `#pragma inline_max_size` in a
// file as the file's value, so this cannot be scoped to the one function.
#pragma inline_max_size(138)

// The meanings of the two implicit static-geometry materials are not yet known.
static const CMaterialList skStaticGeometryMaterials(kMT_Unknown59,
                                                     static_cast< EMaterialTypes >(60));

static float CollisionImpulseFiniteVsInfinite(float, float, float);
static float CollisionImpulseFiniteVsFinite(float, float, float, float);
static bool CollideCachedAABox(const CAreaCollisionCache&, const CAABox&, const CMaterialFilter&,
                               CCollisionInfoList&, const CCollisionPrimitive&);

extern "C" CAreaOctTree::Node fn_8012753C(const CAreaOctTree& tree);

// Retail calls a 0x10-byte function at 0x800A4840 in eight places in this unit and discards its
// result each time. Its whole body is `subfic r0,r3,1 ; cntlzw r0,r0 ; srwi r3,r0,5`, i.e. it
// returns `name == 1`. Prime 1's tree has it as `bool IsUser(int name) { return name == 1; }`
// (`src/MetroidPrime/UserNames.cpp`, declared in `include/MetroidPrime/UserNames.hpp`) and calls
// it as a bare `IsUser(0);` at the same points: after `BuildAreaCollisionCache`, and after the
// `actor.SetVelocityWR` in `CollideWithDynamicBodyNoRot`. Echoes' copy is the last function of
// `MetroidPrime/ScriptObjects/CScriptPlatform.cpp`'s retail range and no unit in this tree emits
// it, so it is declared here under retail's own symbol name and dtk fills the body from retail.
extern "C" bool fn_800A4840(int name);

// Retail's `CMaterialFilter::WithImplicitMaterials` is one out-of-line 0x24C-byte function at
// 0x802896F0 (an unclaimed gap after `Collision/CCollidableSphere.cpp`) that all eight of this
// unit's `filter.WithImplicitMaterials(skStaticGeometryMaterials)` sites call. It takes the
// filter in r4, the material list in r5 and writes the result through the hidden return pointer
// in r3, and its body is this tree's inline header version verbatim - the switch on `type` at
// +0x10, the include/exclude word pairs at +0x0/+0x4/+0x8/+0xC, and the two `0xFFFFFFFF`
// fallbacks. No object in this tree defines it, so the header's inline spelling inlines ~20
// instructions at each site instead of emitting the call. Declared here under retail's own
// symbol name; dtk fills the body from retail.
extern "C" CMaterialFilter fn_802896F0(const CMaterialFilter& filter,
                                       const CMaterialList& materials);

void CGameCollision::InitCollision(CStateManager*) {
  // TODO: OBB-tree-group collider registration, mode-dependent duplicate buffers, and debug models.
}

void CGameCollision::UninitializeCollision() {
  // TODO: release debug-model tokens, unregister their cache views, and free duplicate buffers.
  CCollisionPrimitive::Uninitialize();
}

bool CGameCollision::NullCollisionCollider(const CInternalCollisionStructure&,
                                           CCollisionInfoList&) {
  return false;
}

bool CGameCollision::NullMovingCollider(const CInternalCollisionStructure&, const CVector3f&,
                                        double&, CCollisionInfo&) {
  return false;
}

bool CGameCollision::NullBooleanCollider(const CInternalCollisionStructure&) { return false; }

CRayCastResult
CGameCollision::RayWorldIntersection(const CStateManager& mgr, TUniqueId& idOut,
                                     const CVector3f& position, const CVector3f& direction,
                                     float length, const CMaterialFilter& filter,
                                     const rstl::reserved_vector< TUniqueId, 1024 >& nearList) {
  const CRayCastResult staticResult =
      RayStaticIntersection(mgr, position, direction, length, filter);
  const CRayCastResult dynamicResult =
      RayDynamicIntersection(mgr, idOut, position, direction, length, filter, nearList);

  if (dynamicResult.IsValid()) {
    if (!staticResult.IsValid()) {
      return dynamicResult;
    }
    if (staticResult.GetTime() >= dynamicResult.GetTime()) {
      return dynamicResult;
    }
  }

  idOut = kInvalidUniqueId;
  return staticResult;
}

CRayCastResult
CGameCollision::RayDynamicIntersection(const CStateManager& mgr, TUniqueId& idOut,
                                       const CVector3f& position, const CVector3f& direction,
                                       float length, const CMaterialFilter& filter,
                                       const rstl::reserved_vector< TUniqueId, 1024 >& nearList) {
  float closest = length > 0.f ? length : 100000.f;
  CRayCastResult result;
  for (const TUniqueId* id = nearList.begin(); id != nearList.end(); ++id) {
    if (const CPhysicsActor* actor = TCastToConstPtr< CPhysicsActor >(mgr.GetObjectById(*id))) {
      const CTransform4f& xf = actor->GetPrimitiveTransform();
      const CCollisionPrimitive* prim = actor->GetCollisionPrimitive();
      const CInternalRayCastStructure ray(position, direction, closest, xf, filter);
      const CRayCastResult candidate = prim->CastRayInternal(ray);
      if (candidate.IsValid() && candidate.GetTime() < closest) {
        result = candidate;
        closest = candidate.GetTime();
        idOut = actor->GetUniqueId();
        if (closest <= FLT_EPSILON) {
          break;
        }
      }
    }
  }
  return result;
}

bool CGameCollision::RayDynamicLineOfSightTest(
    const CStateManager& mgr, const CVector3f& position, const CVector3f& direction, float length,
    const CMaterialFilter& filter, const rstl::reserved_vector< TUniqueId, 1024 >& nearList,
    const CActor* ignoreActor) {
  const float maxDistance = length > 0.f ? length : 100000.f;
  for (const TUniqueId* id = nearList.begin(); id != nearList.end(); ++id) {
    if (const CPhysicsActor* actor = TCastToConstPtr< CPhysicsActor >(mgr.GetObjectById(*id))) {
      if (ignoreActor == nullptr || actor->GetUniqueId() != ignoreActor->GetUniqueId()) {
        const CTransform4f& xf = actor->GetPrimitiveTransform();
        const CCollisionPrimitive* prim = actor->GetCollisionPrimitive();
        const CInternalRayCastStructure ray(position, direction, maxDistance, xf, filter);
        const CRayCastResult candidate = prim->CastRayInternal(ray);
        if (candidate.IsValid()) {
          return false;
        }
      }
    }
  }
  return true;
}

bool CGameCollision::RayStaticLineOfSightTest(const CStateManager& mgr, const CVector3f& position,
                                              const CVector3f& direction, float length,
                                              const CMaterialFilter& filter) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  const CLine line(position, CUnitVector3f(direction.GetX(), direction.GetY(), direction.GetZ()));
  const float maxDistance = length > 0.f ? length : 100000.f;
  for (CGameArea::CConstChainIterator area = mgr.GetWorld()->GetChainHead(CWorld::kC_Alive);
       area != CWorld::skGlobalEnd; ++area) {
    const CAreaOctTree& tree = *area->GetPostConstructed()->mCollision;
    if (!fn_8012753C(tree).LineTest(line, staticFilter, maxDistance)) {
      return false;
    }
  }
  return true;
}

bool CGameCollision::RayStaticLineOfSightTest(const CGameArea& area, const CVector3f& position,
                                              const CVector3f& direction, float length,
                                              const CMaterialFilter& filter) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  const CLine line(position, CUnitVector3f(direction.GetX(), direction.GetY(), direction.GetZ()));
  const CAreaOctTree& tree = *area.GetPostConstructed()->mCollision;
  if (fn_8012753C(tree).LineTest(line, staticFilter, length > 0.f ? length : 100000.f)) {
    return true;
  }
  return false;
}

CRayCastResult CGameCollision::RayStaticIntersection(const CStateManager& mgr,
                                                     const CVector3f& position,
                                                     const CVector3f& direction, float length,
                                                     const CMaterialFilter& filter) {
  CRayCastResult result;
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return result;
  }
  const CLine line(position, CUnitVector3f(direction.GetX(), direction.GetY(), direction.GetZ()));
  float closest = length > 0.f ? length : 100000.f;
  for (CGameArea::CConstChainIterator area = mgr.GetWorld()->GetChainHead(CWorld::kC_Alive);
       area != CWorld::skGlobalEnd; ++area) {
    CAreaOctTree::SRayResult candidate;
    const CAreaOctTree& tree = *area->GetPostConstructed()->mCollision;
    fn_8012753C(tree).LineTestEx(line, staticFilter, candidate, length);
    if (candidate.mSurface && (length == 0.f || length >= candidate.mT) &&
        candidate.mT < closest) {
      result = CRayCastResult(candidate.mT, position + candidate.mT * direction, candidate.mPlane,
                              CMaterialList(candidate.mSurface->GetSurfaceFlags()));
      closest = candidate.mT;
    }
  }
  return result;
}

// Retail `fn_8012753C` (0x8012753C, 0x48 bytes) is the out-of-line copy of
// `CAreaOctTree::GetRootNode()` that retail calls from all four octree entry points in this unit
// (`BuildAreaCollisionCache`, `RayStaticIntersection` and both `RayStaticLineOfSightTest`
// overloads), where this build inlines the header's `GetRootNode()` and reaches
// `__ct__Q212CAreaOctTree4Node...` through a weak COMDAT copy instead. Its body is exactly
// `Node(mTreeBuf, mAabb, *this, mTreeType)`: the six CAABox floats from 0x34(r4)-0x48(r4), then
// `mTreeBuf` from 0x54(r4) into mPtr at 0x18(r3), `r4` itself into mOwner at 0x1c(r3), and
// `mTreeType` from 0x4c(r4) into 0x20(r3) - member order, no frame.

extern "C" CAreaOctTree::Node fn_8012753C(const CAreaOctTree& tree) {
  return CAreaOctTree::Node(tree.GetTreeMemory(), tree.GetBoundingBox(), tree, tree.GetTreeType());
}

void CGameCollision::BuildAreaCollisionCache(const CStateManager& mgr, CAreaCollisionCache& cache) {
  cache.ClearCache();
  for (CGameArea::CConstChainIterator area = mgr.GetWorld()->GetChainHead(CWorld::kC_Alive);
       area != CWorld::skGlobalEnd; ++area) {
    const CAreaOctTree& tree = *area->GetPostConstructed()->mCollision;
    CMetroidAreaCollider::COctreeLeafCache leaves(tree, area->GetId());
    CMetroidAreaCollider::BuildOctreeLeafCache(fn_8012753C(tree), cache.GetCacheBounds(), leaves);
    cache.AddOctreeLeafCache(leaves);
  }
  fn_800A4840(0);
}

bool CGameCollision::DetectCollisionBoolean(
    const CStateManager& mgr, const CCollisionPrimitive& primitive, const CTransform4f& transform,
    const CMaterialFilter& filter, const rstl::reserved_vector< TUniqueId, 1024 >& nearList) {
  if (!filter.GetExcludeList().HasMaterial(kMT_NoStaticCollision) &&
      DetectStaticCollisionBoolean(mgr, primitive, transform, filter)) {
    return true;
  }

  if (DetectDynamicCollisionBoolean(primitive, transform, nearList, mgr)) {
    return true;
  }

  return false;
}

bool CGameCollision::DetectCollisionBoolean_Cached(
    const CStateManager& mgr, CAreaCollisionCache& cache, const CCollisionPrimitive& primitive,
    const CTransform4f& transform, const CMaterialFilter& filter,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList) {
  if (!filter.GetExcludeList().HasMaterial(kMT_NoStaticCollision) &&
      DetectStaticCollisionBoolean_Cached(mgr, cache, primitive, transform, filter)) {
    return true;
  }

  if (DetectDynamicCollisionBoolean(primitive, transform, nearList, mgr)) {
    return true;
  }

  return false;
}

bool CGameCollision::DetectCollision_Cached(
    const CStateManager& mgr, CAreaCollisionCache& cache, const CCollisionPrimitive& primitive,
    const CTransform4f& transform, const CMaterialFilter& filter,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, TUniqueId& idOut,
    CCollisionInfoList& collisions) {
  idOut = kInvalidUniqueId;
  bool hit = false;
  if (!filter.GetExcludeList().HasMaterial(kMT_NoStaticCollision)) {
    if (DetectStaticCollision_Cached(mgr, cache, primitive, transform, filter, collisions)) {
      hit = true;
    }
  }
  TUniqueId dynamicId = kInvalidUniqueId;
  if (DetectDynamicCollision(primitive, transform, nearList, dynamicId, collisions, mgr)) {
    hit = true;
    idOut = dynamicId;
  }
  return hit;
}

bool CGameCollision::DetectCollision_Cached_Moving(
    const CStateManager& mgr, CAreaCollisionCache& cache, const CCollisionPrimitive& primitive,
    const CTransform4f& transform, const CMaterialFilter& filter,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CVector3f& direction,
    TUniqueId& idOut, CCollisionInfo& collision, double& distance) {
  idOut = kInvalidUniqueId;
  bool hit = false;
  if (!filter.GetExcludeList().HasMaterial(kMT_NoStaticCollision)) {
    if (DetectStaticCollision_Cached_Moving(mgr, cache, primitive, transform, filter, direction,
                                            collision, distance)) {
      hit = true;
    }
  }
  if (DetectDynamicCollisionMoving(primitive, transform, nearList, direction, idOut, collision,
                                   distance, mgr)) {
    hit = true;
  }
  return hit;
}

bool CGameCollision::DetectDynamicCollisionMoving(const CCollisionPrimitive& primitive,
                                                  const CTransform4f& transform,
                                                  const CPhysicsActor& actor,
                                                  const CVector3f& direction,
                                                  CCollisionInfo& collision, double& distance) {
  const CMaterialFilter& filter = CMaterialFilter::GetPassEverything();
  return CCollisionPrimitive::CollideMoving(
      CInternalCollisionStructure::CPrimDesc(primitive, filter, transform),
      CInternalCollisionStructure::CPrimDesc(*actor.GetCollisionPrimitive(), filter,
                                             actor.GetPrimitiveTransform()),
      direction, distance, collision);
}

bool CGameCollision::DetectDynamicCollisionMoving(
    const CCollisionPrimitive& primitive, const CTransform4f& transform,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CVector3f& direction,
    TUniqueId& idOut, CCollisionInfo& collision, double& distance, const CStateManager& mgr) {
  bool hit = false;
  for (const TUniqueId* id = nearList.begin(); id != nearList.end(); ++id) {
    const CPhysicsActor* actor = TCastToConstPtr< CPhysicsActor >(mgr.GetObjectById(*id));
    double candidateDistance = distance;
    CCollisionInfo candidate;
    if (actor != nullptr) {
      if (DetectDynamicCollisionMoving(primitive, transform, *actor, direction, candidate,
                                       candidateDistance) &&
          candidateDistance < distance) {
        hit = true;
        collision = candidate;
        distance = candidateDistance;
        idOut = actor->GetUniqueId();
      }
    }
  }
  return hit;
}

bool CGameCollision::DetectDynamicCollisionBoolean(const CCollisionPrimitive& primitive,
                                                   const CTransform4f& transform,
                                                   const CPhysicsActor& actor) {
  const CMaterialFilter& filter = CMaterialFilter::GetPassEverything();
  return CCollisionPrimitive::CollideBoolean(
      CInternalCollisionStructure::CPrimDesc(primitive, filter, transform),
      CInternalCollisionStructure::CPrimDesc(*actor.GetCollisionPrimitive(), filter,
                                             actor.GetPrimitiveTransform()));
}

bool CGameCollision::DetectDynamicCollisionBoolean(
    const CCollisionPrimitive& primitive, const CTransform4f& transform,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CStateManager& mgr) {
  for (const TUniqueId* id = nearList.begin(); id != nearList.end(); ++id) {
    if (const CPhysicsActor* actor = TCastToConstPtr< CPhysicsActor >(mgr.GetObjectById(*id))) {
      if (DetectDynamicCollisionBoolean(primitive, transform, *actor)) {
        return true;
      }
    }
  }
  return false;
}

bool CGameCollision::DetectDynamicCollision(const CCollisionPrimitive& primitive,
                                            const CTransform4f& transform,
                                            const CPhysicsActor& actor,
                                            CCollisionInfoList& collisions) {
  const CMaterialFilter& filter = CMaterialFilter::GetPassEverything();
  return CCollisionPrimitive::Collide(
      CInternalCollisionStructure::CPrimDesc(primitive, filter, transform),
      CInternalCollisionStructure::CPrimDesc(*actor.GetCollisionPrimitive(), filter,
                                             actor.GetPrimitiveTransform()),
      collisions);
}

bool CGameCollision::DetectDynamicCollision(
    const CCollisionPrimitive& primitive, const CTransform4f& transform,
    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, TUniqueId& idOut,
    CCollisionInfoList& collisions, const CStateManager& mgr) {
  for (const TUniqueId* id = nearList.begin(); id != nearList.end(); ++id) {
    if (const CPhysicsActor* actor = TCastToConstPtr< CPhysicsActor >(mgr.GetObjectById(*id))) {
      if (DetectDynamicCollision(primitive, transform, *actor, collisions)) {
        idOut = actor->GetUniqueId();
        return true;
      }
    }
  }
  idOut = kInvalidUniqueId;
  return false;
}

// Guessed name for the legacy cache's AABox contact-collection helper.
static bool CollideCachedAABox(const CAreaCollisionCache& cache, const CAABox& bounds,
                               const CMaterialFilter& filter, CCollisionInfoList& collisions,
                               const CCollisionPrimitive& primitive) {
  bool hit = false;
  const CMaterialList& material = primitive.GetMaterial();
  for (int i = 0; i < int(cache.GetNumCaches()); ++i) {
    if (CMetroidAreaCollider::AABoxCollisionCheck_Cached(cache.GetOctreeLeafCache(i), bounds,
                                                         filter, material, collisions) == true) {
      hit = true;
    }
  }
  return hit;
}

bool CGameCollision::DetectStaticCollision_Cached(
    const CStateManager& mgr, CAreaCollisionCache& cache, const CCollisionPrimitive& primitive,
    const CTransform4f& transform, const CMaterialFilter& filter, CCollisionInfoList& collisions) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  if (primitive.GetPrimType() == 'OBTG') {
    return false;
  }
  const CAABox bounds = primitive.CalculateAABox(transform);
  if (!bounds.Inside(cache.GetCacheBounds())) {
    CAABox expanded(bounds.GetMinPoint() - CVector3f(0.2f, 0.2f, 0.2f),
                    bounds.GetMaxPoint() + CVector3f(0.2f, 0.2f, 0.2f));
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMinPoint());
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMaxPoint());
    cache.SetCacheBounds(expanded);
    BuildAreaCollisionCache(mgr, cache);
    fn_800A4840(0);
  }
  if (cache.HasCacheOverflowed()) {
    return DetectStaticCollision(mgr, primitive, transform, staticFilter, collisions);
  }
  bool hit = false;
  if (primitive.GetPrimType() == 'AABX') {
    hit = CollideCachedAABox(cache, bounds, staticFilter, collisions, primitive);
  } else if (primitive.GetPrimType() == 'SPHR') {
    const CSphere sphere = static_cast< const CCollidableSphere& >(primitive).Transform(transform);
    const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
    for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) {
      if (CMetroidAreaCollider::SphereCollisionCheck_Cached(*leaf, bounds, sphere,
                                                            primitive.GetMaterial(), staticFilter,
                                                            collisions)) {
        hit = true;
      }
    }
  }
  if (primitive.GetPrimType() == 'ABSH') {
    const CCollidableAABoxSphere& compound =
        static_cast< const CCollidableAABoxSphere& >(primitive);
    if (DetectStaticCollision_Cached(mgr, cache, compound.GetCollidableAABox(), transform,
                                     staticFilter, collisions)) {
      hit = true;
    }
    if (DetectStaticCollision_Cached(mgr, cache, compound.GetCollidableSphere(), transform,
                                     staticFilter, collisions)) {
      hit = true;
    }
  }
  return hit;
}

bool CGameCollision::DetectStaticCollision(const CStateManager& mgr,
                                           const CCollisionPrimitive& primitive,
                                           const CTransform4f& transform,
                                           const CMaterialFilter& filter,
                                           CCollisionInfoList& collisions) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  if (primitive.GetPrimType() == 'OBTG') {
    return false;
  }
  bool hit = false;
  const CWorld* world = mgr.GetWorld();
  if (primitive.GetPrimType() == 'AABX') {
    const CAABox bounds = primitive.CalculateAABox(transform);
    for (CGameArea::CConstChainIterator area = world->GetChainHead(CWorld::kC_Alive);
         area != CWorld::skGlobalEnd; ++area) {
      if (CMetroidAreaCollider::AABoxCollisionCheck(*area->GetPostConstructed()->mCollision, bounds,
                                                    staticFilter, primitive.GetMaterial(),
                                                    collisions)) {
        hit = true;
      }
    }
  } else if (primitive.GetPrimType() == 'SPHR') {
    const CAABox bounds = primitive.CalculateAABox(transform);
    const CSphere sphere = static_cast< const CCollidableSphere& >(primitive).Transform(transform);
    for (CGameArea::CConstChainIterator area = world->GetChainHead(CWorld::kC_Alive);
         area != CWorld::skGlobalEnd; ++area) {
      if (CMetroidAreaCollider::SphereCollisionCheck(*area->GetPostConstructed()->mCollision,
                                                     bounds, sphere, primitive.GetMaterial(),
                                                     staticFilter, collisions)) {
        hit = true;
      }
    }
  }
  if (primitive.GetPrimType() == 'ABSH') {
    const CCollidableAABoxSphere& compound =
        static_cast< const CCollidableAABoxSphere& >(primitive);
    if (DetectStaticCollision(mgr, compound.GetCollidableAABox(), transform, staticFilter,
                              collisions)) {
      hit = true;
    }
    if (DetectStaticCollision(mgr, compound.GetCollidableSphere(), transform, staticFilter,
                              collisions)) {
      hit = true;
    }
  }
  return hit;
}

bool CGameCollision::DetectStaticCollisionBoolean_Cached(const CStateManager& mgr,
                                                         CAreaCollisionCache& cache,
                                                         const CCollisionPrimitive& primitive,
                                                         const CTransform4f& transform,
                                                         const CMaterialFilter& filter) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  if (primitive.GetPrimType() == 'OBTG') {
    return false;
  }
  const CAABox bounds = primitive.CalculateAABox(transform);
  if (!bounds.Inside(cache.GetCacheBounds())) {
    CAABox expanded(bounds.GetMinPoint() - CVector3f(0.2f, 0.2f, 0.2f),
                    bounds.GetMaxPoint() + CVector3f(0.2f, 0.2f, 0.2f));
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMinPoint());
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMaxPoint());
    cache.SetCacheBounds(expanded);
    BuildAreaCollisionCache(mgr, cache);
    fn_800A4840(0);
  }
  if (cache.HasCacheOverflowed()) {
    return DetectStaticCollisionBoolean(mgr, primitive, transform, staticFilter);
  }
  if (primitive.GetPrimType() == 'AABX') {
    const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
    for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) {
      if (CMetroidAreaCollider::AABoxCollisionCheckBoolean_Cached(*leaf, bounds, staticFilter)) {
        return true;
      }
    }
  } else if (primitive.GetPrimType() == 'SPHR') {
    const CSphere sphere = static_cast< const CCollidableSphere& >(primitive).Transform(transform);
    const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
    for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) {
      if (CMetroidAreaCollider::SphereCollisionCheckBoolean_Cached(*leaf, bounds, sphere,
                                                                  staticFilter)) {
        return true;
      }
    }
  }
  if (primitive.GetPrimType() == 'ABSH') {
    const CCollidableAABoxSphere& compound =
        static_cast< const CCollidableAABoxSphere& >(primitive);
    if (DetectStaticCollisionBoolean_Cached(mgr, cache, compound.GetCollidableAABox(), transform,
                                            staticFilter)) {
      return true;
    }
    if (DetectStaticCollisionBoolean_Cached(mgr, cache, compound.GetCollidableSphere(), transform,
                                            staticFilter)) {
      return true;
    }
  }
  return false;
}

bool CGameCollision::DetectStaticCollisionBoolean(const CStateManager& mgr,
                                                  const CCollisionPrimitive& primitive,
                                                  const CTransform4f& transform,
                                                  const CMaterialFilter& filter) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  if (primitive.GetPrimType() == 'OBTG') {
    return false;
  }
  const CWorld* world = mgr.GetWorld();
  if (primitive.GetPrimType() == 'AABX') {
    const CAABox bounds = primitive.CalculateAABox(transform);
    for (CGameArea::CConstChainIterator area = world->GetChainHead(CWorld::kC_Alive);
         area != CWorld::skGlobalEnd; ++area) {
      if (CMetroidAreaCollider::AABoxCollisionCheckBoolean(*area->GetPostConstructed()->mCollision,
                                                           bounds, staticFilter)) {
        return true;
      }
    }
  } else if (primitive.GetPrimType() == 'SPHR') {
    const CAABox bounds = primitive.CalculateAABox(transform);
    const CSphere sphere = static_cast< const CCollidableSphere& >(primitive).Transform(transform);
    for (CGameArea::CConstChainIterator area = world->GetChainHead(CWorld::kC_Alive);
         area != CWorld::skGlobalEnd; ++area) {
      if (CMetroidAreaCollider::SphereCollisionCheckBoolean(*area->GetPostConstructed()->mCollision,
                                                            bounds, sphere, staticFilter)) {
        return true;
      }
    }
  }
  if (primitive.GetPrimType() == 'ABSH') {
    const CCollidableAABoxSphere& compound =
        static_cast< const CCollidableAABoxSphere& >(primitive);
    if (DetectStaticCollisionBoolean(mgr, compound.GetCollidableAABox(), transform, staticFilter)) {
      return true;
    }
    if (DetectStaticCollisionBoolean(mgr, compound.GetCollidableSphere(), transform, staticFilter)) {
      return true;
    }
  }
  return false;
}

bool CGameCollision::DetectStaticCollision_Cached_Moving(
    const CStateManager& mgr, CAreaCollisionCache& cache, const CCollisionPrimitive& primitive,
    const CTransform4f& transform, const CMaterialFilter& filter, const CVector3f& direction,
    CCollisionInfo& collision, double& distance) {
  const CMaterialFilter staticFilter = fn_802896F0(filter, skStaticGeometryMaterials);
  if (staticFilter.GetType() == CMaterialFilter::kFT_Never) {
    return false;
  }
  if (primitive.GetPrimType() == 'OBTG') {
    return false;
  }
  const CAABox bounds = primitive.CalculateAABox(transform);
  const CVector3f displacement = float(distance) * direction;
  CAABox sweptBounds(bounds);
  sweptBounds.AccumulateBounds(bounds.GetMinPoint() + displacement);
  sweptBounds.AccumulateBounds(bounds.GetMaxPoint() + displacement);
  if (!sweptBounds.Inside(cache.GetCacheBounds())) {
    CAABox expanded(sweptBounds.GetMinPoint() - CVector3f(0.2f, 0.2f, 0.2f),
                    sweptBounds.GetMaxPoint() + CVector3f(0.2f, 0.2f, 0.2f));
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMinPoint());
    expanded.AccumulateBounds(cache.GetCacheBounds().GetMaxPoint());
    cache.SetCacheBounds(expanded);
    BuildAreaCollisionCache(mgr, cache);
    fn_800A4840(0);
  }

  if (primitive.GetPrimType() == 'AABX') {
    const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
    for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) {
      CCollisionInfo candidate;
      double candidateDistance = distance;
      if (CMetroidAreaCollider::MovingAABoxCollisionCheck_Cached(
              *leaf, bounds, staticFilter, CMaterialList(kMT_Unknown59), direction, float(distance),
              candidate, candidateDistance) &&
          candidateDistance < distance) {
        collision = candidate;
        distance = float(candidateDistance);
      }
    }
  } else if (primitive.GetPrimType() == 'SPHR') {
    const CSphere& sphere = static_cast< const CCollidableSphere& >(primitive).GetSphere();
    const CMetroidAreaCollider::COctreeLeafCache* leaf = &cache.GetOctreeLeafCache(0);
    for (uint i = 0; i < cache.GetNumCaches(); ++i, ++leaf) {
      CCollisionInfo candidate;
      double candidateDistance = distance;
      if (CMetroidAreaCollider::MovingSphereCollisionCheck_Cached(
              *leaf, bounds, CSphere(transform * sphere.GetCenter(), sphere.GetRadius()),
              staticFilter, CMaterialList(kMT_Unknown59), direction, float(distance), candidate,
              candidateDistance) &&
          candidateDistance < distance) {
        collision = candidate;
        distance = float(candidateDistance);
      }
    }
  }
  return collision.IsValid();
}

void CGameCollision::MakeCollisionCallbacks(CStateManager& mgr, CPhysicsActor& actor,
                                            const TUniqueId& id,
                                            const CCollisionInfoList& collisions) {
  actor.CollidedWith(id, collisions, mgr);
  if (id != kInvalidUniqueId) {
    if (CPhysicsActor* other = TCastToPtr< CPhysicsActor >(mgr.ObjectById(id))) {
      CCollisionInfoList swapped(collisions);
      for (int i = 0; i < swapped.GetCount(); ++i) {
        swapped[i].Swap();
      }
      // The original passes the unswapped list despite constructing the swapped copy.
      other->CollidedWith(actor.GetUniqueId(), collisions, mgr);
    }
  }
}

void CGameCollision::SendScriptMessages(CStateManager& mgr, CActor& actor, CActor* other,
                                        const CCollisionInfoList& collisions) {
  CMaterialList materials;
  bool hasFloor = false;
  bool hasPlatform = false;
  for (int i = 0; i < collisions.GetCount(); ++i) {
    materials.Add(collisions[i].GetMaterialLeft());
  }
  for (int i = 0; i < collisions.GetCount(); ++i) {
    const CCollisionInfo& collision = collisions[i];
    if (IsFloor(collision.GetMaterialLeft(), collision.GetNormalLeft())) {
      hasFloor = true;
      if (collision.GetMaterialLeft().HasMaterial(kMT_Platform)) {
        hasPlatform = true;
      }
    }
  }
  SendMaterialMessage(mgr, materials, actor);
  if (hasFloor) {
    mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, kInvalidUniqueId, actor.GetUniqueId(),
                                    kSM_OnFloor, kSS_InvalidState));
    if (hasPlatform) {
      if (CScriptPlatform* platform = TCastToPtr< CScriptPlatform >(other)) {
        mgr.DeliverScriptMsg(
            CScriptMsg(actor.GetUniqueId(), kInvalidUniqueId, platform->GetUniqueId(),
                       static_cast< EScriptObjectMessage >('XONP'), kSS_InvalidState));
      }
    } else {
      mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, kInvalidUniqueId, actor.GetUniqueId(),
                                      static_cast< EScriptObjectMessage >('XLSG'),
                                      kSS_InvalidState));
    }
  } else if (other != nullptr) {
    if (CScriptPlatform* platform = TCastToPtr< CScriptPlatform >(&actor)) {
      for (int i = 0; i < collisions.GetCount(); ++i) {
        const CCollisionInfo& collision = collisions[i];
        if (IsFloor(collision.GetMaterialRight(), collision.GetNormalRight()) &&
            collision.GetMaterialRight().HasMaterial(kMT_Platform)) {
          hasPlatform = true;
          break;
        }
      }
      if (hasPlatform) {
        mgr.DeliverScriptMsg(
            CScriptMsg(other->GetUniqueId(), kInvalidUniqueId, platform->GetUniqueId(),
                       static_cast< EScriptObjectMessage >('XONP'), kSS_InvalidState));
      }
    }
  }
}

void CGameCollision::SendMaterialMessage(CStateManager& mgr, const CMaterialList&, CActor& actor) {
  // Echoes always sends the normal-surface message here.
  mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, kInvalidUniqueId, actor.GetUniqueId(),
                                  static_cast< EScriptObjectMessage >('XOND'), kSS_InvalidState));
}

void CGameCollision::ShowCollisionResults(CCollisionInfoList&, const CColor&) {}

float CGameCollision::GetCoefficientOfRestitution(const CCollisionInfo&) { return 0.f; }

static float CollisionImpulseFiniteVsFinite(float mass, float otherMass, float velocity,
                                            float restitution) {
  return -(1.f + restitution) * velocity / (1.f / mass + 1.f / otherMass);
}

static float CollisionImpulseFiniteVsInfinite(float mass, float velocity, float restitution) {
  return mass * (-(1.f + restitution) * velocity);
}

bool CGameCollision::IsFloor(const CMaterialList& material, const CVector3f& normal) {
  if (material.HasMaterial(kMT_Floor)) {
    return true;
  }
  return normal.GetZ() > 0.85f;
}

bool CGameCollision::CanBlock(const CMaterialList& material, const CVector3f& normal) {
  if (material.HasMaterial(kMT_Character) && !material.HasMaterial(kMT_SolidCharacter)) {
    return false;
  }
  if (material.HasMaterial(kMT_NoPlayerCollision)) {
    return false;
  }
  if (material.HasMaterial(kMT_Floor)) {
    return true;
  }
  return normal.GetZ() > 0.85f;
}

float CGameCollision::GetMinExtentForCollisionPrimitive(const CCollisionPrimitive& primitive) {
  if (primitive.GetPrimType() == 'SPHR') {
    return 2.f * static_cast< const CCollidableSphere& >(primitive).GetSphere().GetRadius();
  }
  if (primitive.GetPrimType() == 'AABX') {
    const CAABox& bounds = static_cast< const CCollidableAABox& >(primitive).GetBox();
    const CVector3f extents = bounds.GetMaxPoint() - bounds.GetMinPoint();
    return rstl::min_val(rstl::min_val(extents[0], extents[1]), extents[2]);
  }
  if (primitive.GetPrimType() == 'ABSH') {
    const CCollidableAABoxSphere& compound =
        static_cast< const CCollidableAABoxSphere& >(primitive);
    return rstl::min_val(GetMinExtentForCollisionPrimitive(compound.GetCollidableAABox()),
                         GetMinExtentForCollisionPrimitive(compound.GetCollidableSphere()));
  }
  return 1.f;
}

// A field-for-field mirror of `CCollisionInfo`, so that retail's implicit copy constructor can be
// written out without touching the class. Retail's own 0x80125288 body fixes the offsets: 0x00..0x2F
// are the four `CVector3f` extents, 0x30 and 0x38 are the two `CMaterialList`s (two words each),
// 0x40..0x57 are the two normals, 0x58 is the `TUniqueId` and 0x5A holds both bit-fields in one
// byte. The trailing pad makes the mirror 0x60, so `CHECK_SIZEOF` ties it to the object.
struct SCCollisionInfoFields {
  float mExtents[12];
  unsigned mMaterials[4];
  float mNormals[6];
  ushort mObjectId;
  uchar mFlags;
  uchar mPad[0x60 - 0x5B];
};
CHECK_SIZEOF(SCCollisionInfoFields, 0x60)

// Retail 0x80125288, 0xC4 = 49 insns - `CCollisionInfo`'s implicit copy constructor, emitted
// memberwise and unnamed in retail's symbol table, so claimable only under an `extern "C"` name.
// The fields are `float`/`unsigned` rather than the members' own class types on purpose: mwceppc
// copies a class member by word (`lwz`/`stw`, which is what `*self = other` produces, 0xB4 bytes)
// and a scalar member in its own width, and retail's 49 instructions are `lfs`/`stfs` for the
// eighteen floats, `lwz`/`stw` for the four material words and `lhz`/`sth`, `lbz`/`stb` for the
// tail - the shape a member-by-member *construction* has, not an assignment's.
// Each `CMaterialList`'s two words are written high half first because that is the order retail's
// copy loads them in (0x34 before 0x30, 0x3C before 0x38); in order it measures 99.84%, the same
// 49 instructions with four `lwz`/`stw` pairs the other way round.
extern "C" void fn_80125288(CCollisionInfo* self, const CCollisionInfo& other) {
  SCCollisionInfoFields* dst = reinterpret_cast< SCCollisionInfoFields* >(self);
  const SCCollisionInfoFields* src = reinterpret_cast< const SCCollisionInfoFields* >(&other);
  for (int i = 0; i < 12; ++i) {
    dst->mExtents[i] = src->mExtents[i];
  }
  for (int g = 0; g < 2; ++g) {
    dst->mMaterials[g * 2 + 1] = src->mMaterials[g * 2 + 1];
    dst->mMaterials[g * 2] = src->mMaterials[g * 2];
  }
  for (int i = 0; i < 6; ++i) {
    dst->mNormals[i] = src->mNormals[i];
  }
  dst->mObjectId = src->mObjectId;
  dst->mFlags = src->mFlags;
}

void CGameCollision::ResolveCollisions(CPhysicsActor& actor, CPhysicsActor* other,
                                       const CCollisionInfoList& collisions) {
  for (int i = 0; i < collisions.GetCount(); ++i) {
    const CCollisionInfo collision = collisions[i];
    const float restitution =
        GetCoefficientOfRestitution(collision) + actor.GetCoefficientOfRestitutionModifier();
    if (other != nullptr) {
      CollideWithDynamicBodyNoRot(actor, *other, collision, restitution, false);
    } else {
      CollideWithStaticBodyNoRot(actor, collision.GetMaterialLeft(), collision.GetMaterialRight(),
                                 CUnitVector3f(collision.GetNormalLeft()),
                                 restitution, false);
    }
  }
}

void CGameCollision::CollideWithDynamicBodyNoRot(CPhysicsActor& actor, CPhysicsActor& other,
                                                 const CCollisionInfo& collision, float restitution,
                                                 bool flattenNormal) {
  CVector3f normal = collision.GetNormalLeft();
  if (flattenNormal) {
    normal.SetZ(0.f);
  }
  const float mass = actor.GetMass();
  const float otherMass = other.GetMass();
  const float normalVelocity = CVector3f::Dot(GetActorRelativeVelocities(&actor, &other), normal);
  const float maxSpeed =
      rstl::max_val(actor.GetMaximumCollisionVelocity(), actor.GetVelocityWR().Magnitude());
  const float otherMaxSpeed =
      rstl::max_val(other.GetMaximumCollisionVelocity(), other.GetVelocityWR().Magnitude());
  const bool immovable = actor.GetMaterialList().HasMaterial(kMT_Immovable) || mass == 0.f;
  const bool otherImmovable =
      other.GetMaterialList().HasMaterial(kMT_Immovable) || otherMass == 0.f;

  if (normalVelocity < -0.0001f) {
    if (immovable && otherImmovable) {
      actor.SetVelocityWR(CVector3f::Zero());
      other.SetVelocityWR(CVector3f::Zero());
    } else if (immovable) {
      const float impulse =
          CollisionImpulseFiniteVsInfinite(otherMass, normalVelocity, restitution);
      other.ApplyImpulseWR(-impulse * normal, CAxisAngle::Identity());
    } else if (otherImmovable) {
      const float impulse = CollisionImpulseFiniteVsInfinite(mass, normalVelocity, restitution);
      actor.ApplyImpulseWR(impulse * normal, CAxisAngle::Identity());
    } else {
      const float impulse =
          CollisionImpulseFiniteVsFinite(mass, otherMass, normalVelocity, restitution);
      actor.ApplyImpulseWR(impulse * normal, CAxisAngle::Identity());
      other.ApplyImpulseWR(-impulse * normal, CAxisAngle::Identity());
    }
    actor.UseCollisionImpulses();
    other.UseCollisionImpulses();
  } else if (normalVelocity < 0.1f) {
    if (!immovable) {
      fn_800A4840(0);
      actor.ApplyImpulseWR((0.05f * mass) * normal, CAxisAngle::Identity());
      actor.UseCollisionImpulses();
    }
    if (!otherImmovable) {
      fn_800A4840(0);
      other.ApplyImpulseWR((-0.05f * otherMass) * normal, CAxisAngle::Identity());
      other.UseCollisionImpulses();
    }
  }
  const float speed = actor.GetVelocityWR().Magnitude();
  if (speed > maxSpeed) {
    actor.SetVelocityWR(maxSpeed * (actor.GetVelocityWR() / speed));
    fn_800A4840(0);
  }
  const float otherSpeed = other.GetVelocityWR().Magnitude();
  if (otherSpeed > otherMaxSpeed) {
    other.SetVelocityWR(otherMaxSpeed * (other.GetVelocityWR() / otherSpeed));
    fn_800A4840(0);
  }
}

void CGameCollision::CollideWithStaticBodyNoRot(CPhysicsActor& actor, const CMaterialList& material,
                                                const CMaterialList& otherMaterial,
                                                const CUnitVector3f& normal, float restitution,
                                                bool flattenNormal) {
  CVector3f collisionNormal(normal);
  if (flattenNormal && material.HasMaterial(kMT_Player) && !otherMaterial.HasMaterial(kMT_Floor)) {
    collisionNormal.SetZ(0.f);
  }
  if (!collisionNormal.CanBeNormalized()) {
    return;
  }
  collisionNormal.Normalize();
  const CVector3f velocity = actor.GetVelocityWR();
  const float normalVelocity = CVector3f::Dot(velocity, collisionNormal);
  if (normalVelocity < -0.0001f) {
    const float impulse =
        CollisionImpulseFiniteVsInfinite(actor.GetMass(), normalVelocity, restitution);
    actor.ApplyImpulseWR(impulse * collisionNormal, CAxisAngle::Identity());
    actor.UseCollisionImpulses();
  } else {
    const float speed = velocity.Magnitude();
    const float cosAngle = speed > 0.001f ? normalVelocity / speed : 0.f;
    if (normalVelocity < 0.001f || cosAngle < 0.0008f) {
      actor.ApplyImpulseWR((0.05f * actor.GetMass()) * collisionNormal, CAxisAngle::Identity());
      actor.UseCollisionImpulses();
    }
  }
}

void CGameCollision::Move(CStateManager&, CPhysicsActor&, float,
                          const rstl::reserved_vector< TUniqueId, 1024 >*) {
  // TODO: recover the movement/filter dispatch and player failsafe sampling condition.
}

void CGameCollision::MovePlayer(CStateManager&, CPhysicsActor&, float,
                                const rstl::reserved_vector< TUniqueId, 1024 >*) {
  // TODO: recover the ball collision filter and packed-cache movement dispatcher.
}

// Retail 0x801247D4, 0x24 = 9 insns - `CPhysicsActor::GetLastNonCollidingState()`, unnamed in
// retail's symbol table, so claimable only under an `extern "C"` name. Its caller,
// `CGameCollision::CollisionFailsafe`, is still a TODO below, so nothing calls it here yet; the
// body is the whole function and is byte-exact, and the member it copies is at 0x264 - the last
// `CMotionState` before `rstl::optional_object<CVector3f>` and the `mNumTicksStuck` counter
// retail's caller increments at 0x2BC.
extern "C" CMotionState fn_801247D4(const CPhysicsActor& actor) {
  return actor.GetLastNonCollidingState();
}

void CGameCollision::CollisionFailsafe(const CStateManager&, CAreaCollisionCache&,
                                       CPhysicsActor& actor, const CCollisionPrimitive&,
                                       const rstl::reserved_vector< TUniqueId, 1024 >&, float, uint,
                                       float) {
  actor.MoveCollisionPrimitive(CVector3f::Zero());
  // TODO: restore the last safe state, collision-normal impulse, and displacement search.
}

rstl::optional_object< CVector3f >
CGameCollision::FindNonIntersectingVector(const CStateManager&, CPhysicsActor&,
                                          const CCollisionPrimitive&,
                                          const rstl::optional_object< CVector3f >&) {
  // TODO: recover the 26-direction search and its world-ray/area-boundary checks.
  return rstl::optional_object_null();
}

CVector3f CGameCollision::GetActorRelativeVelocities(const CPhysicsActor* actor,
                                                     const CPhysicsActor* other) {
  float x = actor->GetVelocityWR().GetX();
  float y = actor->GetVelocityWR().GetY();
  float z = actor->GetVelocityWR().GetZ();

  if (other != nullptr) {
    const CScriptPlatform* platform = TCastToConstPtr< CScriptPlatform >(other);
    bool rider = false;
    if (platform != nullptr) {
      rider = platform->IsRider(actor->GetUniqueId());
    }
    if (!rider) {
      x -= other->GetVelocityWR().GetX();
      y -= other->GetVelocityWR().GetY();
      z -= other->GetVelocityWR().GetZ();
    }
  }

  return CVector3f(x, y, z);
}

void CGameCollision::PushActorAwayFromWalls(CStateManager& mgr, CPhysicsActor& actor, float dt,
                                            float height, float distance, float acceleration,
                                            int iterations, float radius) {
  const CVector3f actorPosition = actor.GetTranslation();
  const CVector3f center = actorPosition + CVector3f(0.f, 0.f, height);
  const float cacheRadius = 1.2f * radius;
  const CVector3f extent(distance + cacheRadius, distance + cacheRadius, cacheRadius);
  CAreaCollisionCache cache(CAABox(center - extent, center + extent));
  BuildAreaCollisionCache(mgr, cache);
  const CSphere sphere(center, radius);
  const CMaterialFilter filter = CMaterialFilter::MakeExclude(CMaterialList(kMT_Floor));
  if (DetectStaticCollisionBoolean_Cached(mgr, cache,
                                          CCollidableSphere(sphere, CMaterialList(kMT_Unknown59)),
                                          CTransform4f::Identity(), filter)) {
    return;
  }

  CVector3f correction = CVector3f::Zero();
  const float angleStep = M_2PIF / float(iterations);
  for (int i = 0; i < iterations; ++i) {
    const float angle = angleStep * float(i);
    const CVector3f direction(CMath::SlowSineR(angle), CMath::SlowCosineR(angle), 0.f);
    double collisionDistance = distance;
    CCollisionInfo collision;
    if (cache.HasCacheOverflowed()) {
      cache.ClearCache();
      CAABox bounds(center, center);
      bounds.AccumulateBounds(actorPosition + distance * direction);
      const CVector3f radiusVector(radius, radius, radius);
      // Both corners use the minimum point in the original.
      cache.SetCacheBounds(
          CAABox(bounds.GetMinPoint() - radiusVector, bounds.GetMinPoint() + radiusVector));
      BuildAreaCollisionCache(mgr, cache);
    }
    if (DetectStaticCollision_Cached_Moving(
            mgr, cache, CCollidableSphere(sphere, CMaterialList(kMT_Unknown59)),
            CTransform4f::Identity(), filter, direction, collision, collisionDistance)) {
      const float fraction = float(distance - collisionDistance) / distance / float(iterations);
      correction -= fraction * direction;
    }
  }
  actor.SetVelocityWR(actor.GetVelocityWR() + dt * (acceleration * correction));
}

