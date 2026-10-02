#include "WorldFormat/CMetroidAreaCollider.hpp"
#include "WorldFormat/CCollisionCache.hpp"

#include "Collision/CMRay.hpp"
#include "Collision/CollisionUtil.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CVector3d.hpp"

#include <float.h>
#include <string.h>

static uint gCalledClip = 0;
static uint gRejectedByClip = 0;
static uint gTrianglesProcessed = 0;
static uint gDupTrianglesProcessed = 0;
uchar CMetroidAreaCollider::sDupPrimitiveCheckCount = 0xff;
uchar* CMetroidAreaCollider::spDupVertexList;
uchar* CMetroidAreaCollider::spDupEdgeList;
uchar* CMetroidAreaCollider::spDupTriangleList;
ushort CMetroidAreaCollider::sDupVertexCount;
ushort CMetroidAreaCollider::sDupEdgeCount;
ushort CMetroidAreaCollider::sDupTriangleCount;

static void FlagVertexIndicesForFace(uint face, bool* vertFlags);
static void FlagEdgeIndicesForFace(uint face, bool* edgeFlags);
static float PlaneIntersectionFraction(const CVector3f& start, const CVector3f& end,
                                       const CPlane& plane); // Guessed name

// The twelve functions below are the object-level twins of code this file already emits as
// `rstl`'s out-of-line template instantiations, plus the two deleting destructors mwceppc emits
// for `CMovingAABoxComponents`' members. Each is written out by hand and named as retail's
// symbol table names it, for the reason `fn_80143CD4` is written out in
// `src/MetroidPrime/Player/CGameState.cpp` and `fn_800F4FB4` in
// `src/MetroidPrime/BodyState/CBSLocomotion.cpp`: mwceppc emits an out-of-line copy of a
// template under its *mangled* name, and dtk gives retail's own copy of the same code the name
// of the address it sits at, so objdiff has nothing to pair the two together and scores retail's
// function 0.00% however right our code is. No C++ declaration can rename a template
// instantiation, so the only way to give objdiff a symbol to match is to write the function out.
//
// The bodies are not transcribed disassembly: each is the body `include/rstl/reserved_vector.hpp`
// and `include/rstl/construct.hpp` already spell, and each was checked against `tools/dis.sh`
// before it was written. Nothing here is a stub - each function is retail's whole body.
//
// A body calls its neighbour *by its retail name* rather than inlining it, because a `bl` is a
// `bl` in both objects and objdiff compares the instruction rather than the symbol it relocates
// to. That is not a guess: `CAreaCollisionCache::AddOctreeLeafCache` scores 100.00% today with
// retail's `bl fn_80248D74` against our `bl push_back__Q24rstl61...`, and `AddLeaf` scores 100.00%
// with retail's `bl fn_80248FEC` against our `bl push_back__Q24rstl41...`.
typedef CMetroidAreaCollider::SBoxEdge SBoxEdge;

// retail `.rodata:0x803AD854` = {1,0,0} and `0x803AD860` = {2,2,1}, read by
// `MovingAABoxCollisionCheck_Edge` as `slwi r0,r0,2` / two `lwzx` off two `lis`-formed bases
// (`0x80249280`-`0x8024929c`), indexed by `edge.mDominantAxis`. Together they are the two
// non-dominant components for every axis. `0x803AD84C`, which an earlier pass named here, is
// not a table this function reads: it holds neither of these, and the `{2,0,1}` value that
// address was read as would make `ci0 == ci1` for two of the three axes.
static const int sBoxEdgeCompIdxA[3] = {1, 0, 0};
static const int sBoxEdgeCompIdxB[3] = {2, 2, 1};
typedef CMetroidAreaCollider::COctreeLeafCache COctreeLeafCache;
typedef rstl::reserved_vector< COctreeLeafCache, 3 > CLeafCacheVec;
typedef rstl::reserved_vector< CAreaOctTree::Node, 64 > CNodeVec;

extern "C" void fn_80248D74(CLeafCacheVec* self, const COctreeLeafCache& in);
extern "C" void fn_80248DBC(void* dest, const COctreeLeafCache& src);
extern "C" void fn_80248DDC(void* dest, const COctreeLeafCache& src);
extern "C" CNodeVec* fn_80248E60(CNodeVec* self, const CNodeVec* other);
extern "C" void fn_80248F2C(void* dest, const CAreaOctTree::Node& src);
extern "C" void fn_80248F0C(void* dest, const CAreaOctTree::Node& src);
extern "C" CAreaOctTree::Node* fn_80248EA4(const CAreaOctTree::Node* src, int n,
                                           CAreaOctTree::Node* dest);

// Guessed name
static float PlaneIntersectionFraction(const CVector3f& start, const CVector3f& end,
                                       const CPlane& plane) {
  return -(CVector3f::Dot(start, plane.GetNormal()) - plane.GetConstant()) /
         CVector3f::Dot(end - start, plane.GetNormal());
}

bool CMetroidAreaCollider::ConvexPolyCollision(const CPlane* planes, const CVector3f* verts,
                                               CAABox& aabb) {
  typedef rstl::reserved_vector< CVector3f, 20 > ClipVec;
  ClipVec vecs[2];
  ++gCalledClip;
  ++gRejectedByClip;
  int vecIdx = 0;
  int otherVecIdx = 1;

  for (int i = 0; i < 3; ++i) {
    vecs[0].push_back(verts[i]);
  }

  for (int i = 0; i < 6; ++i) {
    ClipVec& vec = vecs[vecIdx];
    ClipVec& otherVec = vecs[otherVecIdx];
    otherVec.clear();

    // Measured: retail keeps `&planes[i]` in one register for the whole iteration (`addi r29,r29,16`
    // at 0x8024D58 is the only advance) and reads the three normal components at `0(r29)`,
    // `4(r29)`, `8(r29)` and the constant at `12(r29)`. Written as `planes[i].GetHeight(...)` at
    // each of the two call sites, mwceppc keeps the *element* pointer in `r28` and materialises a
    // second one for the constant, `addi r29,r28,12` (measured: 96.66%, 198 instructions against
    // retail's 197, every GPR one lower because that extra live value shifts the allocation).
    // Binding the reference once is what collapses the two into one.
    const CPlane& plane = planes[i];
    bool inFrontOf = plane.GetHeight(vec.front()) >= 0.f;
    for (int j = 0; j < vec.size(); ++j) {
      const CVector3f& b = vec[j == vec.size() - 1 ? 0 : j + 1];
      if (inFrontOf) {
        otherVec.push_back(vec[j]);
      }
      bool nextInFrontOf = plane.GetHeight(b) >= 0.f;
      if (nextInFrontOf ^ inFrontOf) {
        float f = PlaneIntersectionFraction(vec[j], b, planes[i]);
        otherVec.push_back((1.f - f) * (vec[j] - b) + b);
      }
      inFrontOf = nextInFrontOf;
    }

    if (otherVec.empty()) {
      return false;
    }

    otherVecIdx ^= 1;
    vecIdx ^= 1;
  }

  ClipVec& accumVec = vecs[otherVecIdx ^ 1];
  for (ClipVec::const_iterator it = accumVec.begin(); it != accumVec.end(); ++it) {
    aabb.AccumulateBounds(*it);
  }

  --gRejectedByClip;
  return true;
}

// Guessed name
void CMetroidAreaCollider::SetDuplicatePrimitiveBuffers(uchar* vertices, ushort vertexCount,
                                                        uchar* edges, ushort edgeCount,
                                                        uchar* triangles, ushort triangleCount) {
  spDupVertexList = vertices;
  sDupVertexCount = vertexCount;
  spDupEdgeList = edges;
  sDupEdgeCount = edgeCount;
  spDupTriangleList = triangles;
  sDupTriangleCount = triangleCount;
  sDupPrimitiveCheckCount = 0xff;
}

void CMetroidAreaCollider::ResetInternalCounters() {
  gCalledClip = 0;
  gRejectedByClip = 0;
  gTrianglesProcessed = 0;
  gDupTrianglesProcessed = 0;
  if (sDupPrimitiveCheckCount == 0xff) {
    memset(spDupVertexList, 0, sDupVertexCount);
    memset(spDupEdgeList, 0, sDupEdgeCount);
    memset(spDupTriangleList, 0, sDupTriangleCount);
    ++sDupPrimitiveCheckCount;
  }
  ++sDupPrimitiveCheckCount;
}

bool CMetroidAreaCollider::AABoxCollisionCheck_Internal(const CAreaOctTree::Node& node,
                                                        CAABoxAreaCache& cache) {
  bool ret = false;

  switch (node.GetTreeType()) {
  case CAreaOctTree::Node::kTT_Invalid:
    return false;
  case CAreaOctTree::Node::kTT_Branch: {
    for (int i = 0; i < 8; ++i) {
      CAreaOctTree::Node ch = node.GetChild(i);
      CAABox box = ch.GetBoundingBox();
      if (box.DoBoundsOverlap(cache.mAabb))
        if (AABoxCollisionCheck_Internal(ch, cache))
          ret = true;
    }
    break;
  }
  case CAreaOctTree::Node::kTT_Leaf: {
    CAreaOctTree::TriListReference list = node.GetTriangleArray();
    int size = list.GetSize();
    const CAreaOctTree& owner = node.GetOwner();
    const CMaterialFilter& filter = cache.mFilter;
    const CPlane* planes = cache.mPlanes;
    for (int j = 0; j < size; ++j) {
      ++gTrianglesProcessed;
      ushort triIdx = list.GetAt(j);
      if (sDupPrimitiveCheckCount == DupTriangleListValue(triIdx)) {
        ++gDupTrianglesProcessed;
      } else {
        DupTriangleListValue(triIdx) = sDupPrimitiveCheckCount;
        CCollisionSurface surf = owner.GetTriangle(triIdx);
        CMaterialList material(surf.GetSurfaceFlags());
        if (filter.Passes(material)) {
          if (CollisionUtil::TriBoxOverlap(cache.mCenter, cache.mHalfExtent, surf.GetVert(0),
                                           surf.GetVert(1), surf.GetVert(2)) == true) {
            CAABox aabb = CAABox::MakeMaxInvertedBox();
            if (ConvexPolyCollision(planes, &surf.GetVert(0), aabb)) {
              CPlane plane = surf.GetPlane();
              cache.mCollisionList.Add(CCollisionInfo(aabb, cache.mMaterial, material,
                                                     plane.GetNormal(), -plane.GetNormal(),
                                                     static_cast< ushort >(-1)));
              ret = true;
            }
          }
        }
      }
    }
    break;
  }
  default:
    break;
  }

  return ret;
}

bool CMetroidAreaCollider::AABoxCollisionCheck_Cached(const COctreeLeafCache& leafCache,
                                                      const CAABox& aabb,
                                                      const CMaterialFilter& filter,
                                                      const CMaterialList& matList,
                                                      CCollisionInfoList& list) {
  // Measured: retail materialises the three axis vectors into one 36-byte static at
  // `.bss:0x803DE7F8`, each guarded by its own byte flag (`.sbss:0x804196FC`, `+1`, `+2`), so
  // they are function-local `static`s and not plain const locals - the guard is the `lbz`/
  // `extsb.`/`bne` that skips each three-`stfs` block. Prime 1 writes them as plain locals.
  static const CUnitVector3f right(1.f, 0.f, 0.f);
  static const CUnitVector3f forward(0.f, 1.f, 0.f);
  static const CUnitVector3f up(0.f, 0.f, 1.f);
  const CVector3f& min = aabb.GetMinPoint();
  const CVector3f& max = aabb.GetMaxPoint();
  const CPlane planes[6] = {
      CPlane(min, right),   CPlane(max, -right),
      CPlane(min, forward), CPlane(max, -forward),
      CPlane(min, up),      CPlane(max, -up),
  };

  ResetInternalCounters();
  CVector3f center = aabb.GetCenterPoint();
  // Measured: retail materialises this one at the call site rather than calling
  // `CAABox::GetHalfExtent` - the `fsubs`/`fmuls` pair with `lfs f5,-18016(r2)` (0.5f) at
  // 0x8024CAFC is `GetHalfExtent`'s own body inlined, exactly as in the two cache constructors.
  CVector3f halfExtent = (aabb.GetMaxPoint() - aabb.GetMinPoint()) * 0.5f;
  bool ret = false;

  for (int i = 0; i < leafCache.GetNumLeaves(); ++i) {
    const CAreaOctTree::Node& node = leafCache.GetLeaf(i);
    if (aabb.DoBoundsOverlap(node.GetBoundingBox())) {
      CAreaOctTree::TriListReference listRef = node.GetTriangleArray();
      const CAreaOctTree& owner = node.GetOwner();
      int size = listRef.GetSize();
      for (int j = 0; j < size; ++j) {
        ++gTrianglesProcessed;
        ushort triIdx = listRef.GetAt(j);
        if (sDupPrimitiveCheckCount == DupTriangleListValue(triIdx)) {
          ++gDupTrianglesProcessed;
        } else {
          DupTriangleListValue(triIdx) = sDupPrimitiveCheckCount;
          CCollisionSurface surf = owner.GetTriangle(triIdx);
          CMaterialList material(surf.GetSurfaceFlags());
          if (filter.Passes(material)) {
            if (CollisionUtil::TriBoxOverlap(center, halfExtent, surf.GetVert(0), surf.GetVert(1),
                                             surf.GetVert(2)) == true) {
              CAABox aabb2 = CAABox::MakeMaxInvertedBox();
              if (ConvexPolyCollision(planes, &surf.GetVert(0), aabb2)) {
                CPlane plane = surf.GetPlane();
                list.Add(CCollisionInfo(aabb2, matList, material, plane.GetNormal(),
                                        -plane.GetNormal(), static_cast< ushort >(-1)));
                ret = true;
              }
            }
          }
        }
      }
    }
  }

  return ret;
}

bool CMetroidAreaCollider::AABoxCollisionCheck_Cached(const CCollisionCache& cache,
                                                      const CAABox& aabb,
                                                      const CMaterialFilter& filter,
                                                      const CMaterialList& matList,
                                                      CCollisionInfoList& list) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

CAABoxAreaCache::CAABoxAreaCache(const CAABox& aabb, const CPlane* pl,
                                 const CMaterialFilter& filter, const CMaterialList& material,
                                 CCollisionInfoList& collisionList)
: mAabb(aabb)
, mPlanes(pl)
, mFilter(filter)
, mMaterial(material)
, mCollisionList(collisionList)
, mCenter(aabb.GetCenterPoint())
, mHalfExtent((aabb.GetMaxPoint() - aabb.GetMinPoint()) * 0.5f) {}

bool CMetroidAreaCollider::AABoxCollisionCheck(const CAreaOctTree& octTree, const CAABox& aabb,
                                               const CMaterialFilter& filter,
                                               const CMaterialList& matList,
                                               CCollisionInfoList& list) {
  // Measured: retail reads all six `CVector3f` components straight out of the `CAABox&` argument
  // register (`lfs f1,0(r4)` .. `lfs f11,20(r4)` at 0x8024C870), so `min` and `max` are *references*
  // into the box and never materialise. Written by value they are copied to the frame first, which
  // is the 4-byte extra at `88(r1)` and one more live float (measured 65.15%, 348 B).
  const CVector3f& min = aabb.GetMinPoint();
  const CVector3f& max = aabb.GetMaxPoint();
  const CUnitVector3f xAxis(1.f, 0.f, 0.f);
  const CUnitVector3f yAxis(0.f, 1.f, 0.f);
  const CUnitVector3f zAxis(0.f, 0.f, 1.f);
  CPlane planes[6] = {
      CPlane(min, xAxis),  CPlane(max, -xAxis), CPlane(min, yAxis),
      CPlane(max, -yAxis), CPlane(min, zAxis),  CPlane(max, -zAxis),
  };
  CAABoxAreaCache cache(aabb, planes, filter, matList, list);
  ResetInternalCounters();
  return AABoxCollisionCheck_Internal(octTree.GetRootNode(), cache);
}

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean_Internal(const CAreaOctTree::Node& node,
                                                               const CBooleanAABoxAreaCache& cache) {
  for (int i = 0; i < 8; ++i) {
    CAreaOctTree::Node::ETreeType type = node.GetChildType(i);
    if (type != CAreaOctTree::Node::kTT_Invalid) {
      CAreaOctTree::Node ch = node.GetChild(i);
      if (cache.mAabb.DoBoundsOverlap(ch.GetBoundingBox())) {
        if (type == CAreaOctTree::Node::kTT_Leaf) {
          CAreaOctTree::TriListReference list = ch.GetTriangleArray();
          const CAreaOctTree& owner = ch.GetOwner();
          int size = list.GetSize();
          for (int j = 0; j < size; ++j) {
            ++gTrianglesProcessed;
            CCollisionSurface surf = owner.GetTriangle(list.GetAt(j));
            if (cache.mFilter.Passes(CMaterialList(surf.GetSurfaceFlags()))) {
              if (CollisionUtil::TriBoxOverlap(cache.mCenter, cache.mHalfExtent, surf.GetVert(0),
                                               surf.GetVert(1), surf.GetVert(2)) == true)
                return true;
            }
          }
        } else {
          if (AABoxCollisionCheckBoolean_Internal(ch, cache) == true)
            return true;
        }
      }
    }
  }
  return false;
}

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean_Cached(const COctreeLeafCache& leafCache,
                                                             const CAABox& aabb,
                                                             const CMaterialFilter& filter) {
  CVector3f center = aabb.GetCenterPoint();
  CVector3f halfExtent = (aabb.GetMaxPoint() - aabb.GetMinPoint()) * 0.5f;

  for (int i = 0; i < leafCache.GetNumLeaves(); ++i) {
    const CAreaOctTree::Node& node = leafCache.GetLeaf(i);
    if (aabb.DoBoundsOverlap(node.GetBoundingBox())) {
      CAreaOctTree::TriListReference list = node.GetTriangleArray();
      const CAreaOctTree& owner = node.GetOwner();
      int size = list.GetSize();
      for (int j = 0; j < size; ++j) {
        ++gTrianglesProcessed;
        CCollisionSurface surf = owner.GetTriangle(list.GetAt(j));
        if (filter.Passes(CMaterialList(surf.GetSurfaceFlags()))) {
          if (CollisionUtil::TriBoxOverlap(center, halfExtent, surf.GetVert(0), surf.GetVert(1),
                                           surf.GetVert(2)) == true)
            return true;
        }
      }
    }
  }

  return false;
}

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean_Cached(const CCollisionCache& cache,
                                                             const CAABox& aabb,
                                                             const CMaterialFilter& filter) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

CBooleanAABoxAreaCache::CBooleanAABoxAreaCache(const CAABox& aabb, const CMaterialFilter& filter)
: mAabb(aabb), mFilter(filter), mCenter(aabb.GetCenterPoint()), mHalfExtent((aabb.GetMaxPoint() - aabb.GetMinPoint()) * 0.5f) {}

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean(const CAreaOctTree& octTree,
                                                      const CAABox& aabb,
                                                      const CMaterialFilter& filter) {
  CBooleanAABoxAreaCache cache(aabb, filter);
  return AABoxCollisionCheckBoolean_Internal(octTree.GetRootNode(), cache);
}

bool CMetroidAreaCollider::SphereCollisionCheck_Internal(const CAreaOctTree::Node& node,
                                                         CSphereAreaCache& cache) {
  bool ret = false;
  CVector3f point = CVector3f::Zero();
  CVector3f normal = CVector3f::Zero();

  for (int i = 0; i < 8; ++i) {
    CAreaOctTree::Node::ETreeType chTp = node.GetChildType(i);
    if (chTp != CAreaOctTree::Node::kTT_Invalid) {
      CAreaOctTree::Node ch = node.GetChild(i);
      if (cache.mAabb.DoBoundsOverlap(ch.GetBoundingBox())) {
        if (chTp == CAreaOctTree::Node::kTT_Leaf) {
          CAreaOctTree::TriListReference list = ch.GetTriangleArray();
          const CAreaOctTree& owner = ch.GetOwner();
          int size = list.GetSize();
          for (int j = 0; j < size; ++j) {
            ++gTrianglesProcessed;
            ushort triIdx = list.GetAt(j);
            if (sDupPrimitiveCheckCount == DupTriangleListValue(triIdx)) {
              ++gDupTrianglesProcessed;
            } else {
              DupTriangleListValue(triIdx) = sDupPrimitiveCheckCount;
              CCollisionSurface surf = owner.GetTriangle(triIdx);
              CMaterialList material(surf.GetSurfaceFlags());
              if (cache.mFilter.Passes(material)) {
                if (CollisionUtil::TriSphereIntersection(cache.mSphere, surf.GetVert(0),
                                                         surf.GetVert(1), surf.GetVert(2), point,
                                                         normal)) {
                  cache.mCollisionList.Add(
                      CCollisionInfo(point, cache.mMaterial, material, normal, static_cast< ushort >(-1)));
                  ret = true;
                }
              }
            }
          }
        } else {
          if (SphereCollisionCheck_Internal(ch, cache) == true)
            ret = true;
        }
      }
    }
  }

  return ret;
}

bool CMetroidAreaCollider::SphereCollisionCheck_Cached(const COctreeLeafCache& leafCache,
                                                       const CAABox& aabb, const CSphere& sphere,
                                                       const CMaterialList& matList,
                                                       const CMaterialFilter& filter,
                                                       CCollisionInfoList& list) {
  ResetInternalCounters();

  bool ret = false;
  CVector3f point = CVector3f::Zero();
  CVector3f normal = CVector3f::Zero();

  for (int i = 0; i < leafCache.GetNumLeaves(); ++i) {
    const CAreaOctTree::Node& node = leafCache.GetLeaf(i);
    if (aabb.DoBoundsOverlap(node.GetBoundingBox())) {
      CAreaOctTree::TriListReference listRef = node.GetTriangleArray();
      const CAreaOctTree& owner = node.GetOwner();
      int size = listRef.GetSize();
      for (int j = 0; j < size; ++j) {
        ++gTrianglesProcessed;
        ushort triIdx = listRef.GetAt(j);
        if (sDupPrimitiveCheckCount == DupTriangleListValue(triIdx)) {
          ++gDupTrianglesProcessed;
        } else {
          DupTriangleListValue(triIdx) = sDupPrimitiveCheckCount;
          CCollisionSurface surf = owner.GetTriangle(triIdx);
          CMaterialList material(surf.GetSurfaceFlags());
          if (filter.Passes(material)) {
            if (CollisionUtil::TriSphereIntersection(sphere, surf.GetVert(0), surf.GetVert(1),
                                                     surf.GetVert(2), point, normal)) {
              list.Add(CCollisionInfo(point, matList, material, normal, static_cast< ushort >(-1)));
              ret = true;
            }
          }
        }
      }
    }
  }

  return ret;
}

bool CMetroidAreaCollider::SphereCollisionCheck_Cached(const CCollisionCache& cache,
                                                       const CAABox& aabb, const CSphere& sphere,
                                                       const CMaterialList& matList,
                                                       const CMaterialFilter& filter,
                                                       CCollisionInfoList& list) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheck(const CAreaOctTree& octTree, const CAABox& aabb,
                                                const CSphere& sphere, const CMaterialList& matList,
                                                const CMaterialFilter& filter,
                                                CCollisionInfoList& list) {
  CSphereAreaCache cache(aabb, sphere, filter, matList, list);
  ResetInternalCounters();
  return SphereCollisionCheck_Internal(octTree.GetRootNode(), cache);
}

bool CMetroidAreaCollider::SphereCollisionCheckBoolean_Internal(
    const CAreaOctTree::Node& node, const CBooleanSphereAreaCache& cache) {
  for (int i = 0; i < 8; ++i) {
    CAreaOctTree::Node::ETreeType type = node.GetChildType(i);
    if (type != CAreaOctTree::Node::kTT_Invalid) {
      CAreaOctTree::Node ch = node.GetChild(i);
      if (cache.mAabb.DoBoundsOverlap(ch.GetBoundingBox())) {
        if (type == CAreaOctTree::Node::kTT_Leaf) {
          CAreaOctTree::TriListReference list = ch.GetTriangleArray();
          const CAreaOctTree& owner = ch.GetOwner();
          int size = list.GetSize();
          for (int j = 0; j < size; ++j) {
            ++gTrianglesProcessed;
            CCollisionSurface surf = owner.GetTriangle(list.GetAt(j));
            if (cache.mFilter.Passes(CMaterialList(surf.GetSurfaceFlags()))) {
              if (CollisionUtil::TriSphereOverlap(cache.mSphere, surf.GetVert(0),
                                                  surf.GetVert(1), surf.GetVert(2)) == true)
                return true;
            }
          }
        } else {
          if (SphereCollisionCheckBoolean_Internal(ch, cache) == true)
            return true;
        }
      }
    }
  }
  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheckBoolean_Cached(const COctreeLeafCache& leafCache,
                                                              const CAABox& aabb,
                                                              const CSphere& sphere,
                                                              const CMaterialFilter& filter) {
  for (int i = 0; i < leafCache.GetNumLeaves(); ++i) {
    const CAreaOctTree::Node& node = leafCache.GetLeaf(i);
    if (aabb.DoBoundsOverlap(node.GetBoundingBox())) {
      CAreaOctTree::TriListReference list = node.GetTriangleArray();
      const CAreaOctTree& owner = node.GetOwner();
      int size = list.GetSize();
      for (int j = 0; j < size; ++j) {
        ++gTrianglesProcessed;
        CCollisionSurface surf = owner.GetTriangle(list.GetAt(j));
        if (filter.Passes(CMaterialList(surf.GetSurfaceFlags()))) {
          if (CollisionUtil::TriSphereOverlap(sphere, surf.GetVert(0), surf.GetVert(1),
                                              surf.GetVert(2)) == true)
            return true;
        }
      }
    }
  }

  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheckBoolean_Cached(const CCollisionCache& cache,
                                                              const CAABox& aabb,
                                                              const CSphere& sphere,
                                                              const CMaterialFilter& filter) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheckBoolean(const CAreaOctTree& octTree,
                                                       const CAABox& aabb, const CSphere& sphere,
                                                       const CMaterialFilter& filter) {
  CBooleanSphereAreaCache cache(aabb, sphere, filter);
  return SphereCollisionCheckBoolean_Internal(octTree.GetRootNode(), cache);
}

bool CMetroidAreaCollider::MovingSphereCollisionCheck_Cached(
    const COctreeLeafCache& leafCache, const CAABox& aabb, const CSphere& sphere,
    const CMaterialFilter& filter, const CMaterialList& matList, CVector3f dir, float d,
    CCollisionInfo& infoOut, double& dOut) {
  dOut = d;
  ResetInternalCounters();
  // TODO: test the swept sphere against triangle faces, edges and vertices.
  return false;
}

bool CMetroidAreaCollider::MovingAABoxCollisionCheck_Cached(
    const CCollisionCache& cache, const CAABox& aabb, const CMaterialFilter& filter,
    const CMaterialList& matList, CVector3f dir, float d, CCollisionInfo& infoOut, double& dOut) {
  dOut = d;
  ResetInternalCounters();
  // TODO: test swept box faces, vertices and edges against cached triangles.
  return false;
}

bool CMetroidAreaCollider::MovingAABoxCollisionCheck_Cached(
    const COctreeLeafCache& leafCache, const CAABox& aabb, const CMaterialFilter& filter,
    const CMaterialList& matList, CVector3f dir, float mag, CCollisionInfo& infoOut,
    double& dOut) {
  dOut = mag;
  ResetInternalCounters();

  CVector3f moveVec = mag * dir;
  CMovingAABoxComponents components(aabb, dir);

  CAABox movedAABB = components.mAabb;
  movedAABB.AccumulateBounds(aabb.GetMinPoint() + moveVec);
  movedAABB.AccumulateBounds(aabb.GetMaxPoint() + moveVec);

  CVector3f center = movedAABB.GetCenterPoint();
  CVector3f extent = (movedAABB.GetMaxPoint() - movedAABB.GetMinPoint()) * 0.5f;
  bool ret = false;

  CVector3f normal(CVector3f::Zero());
  CVector3f point(CVector3f::Zero());

  for (int i = 0; i < leafCache.GetNumLeaves(); ++i) {
    const CAreaOctTree::Node& node = leafCache.GetLeaf(i);
    if (movedAABB.DoBoundsOverlap(node.GetBoundingBox())) {
      CAreaOctTree::TriListReference list = node.GetTriangleArray();
      const CAreaOctTree& owner = node.GetOwner();
      int listSize = list.GetSize();
      for (int j = 0; j < listSize; ++j) {
        ushort triIdx = list.GetAt(j);
        if (sDupPrimitiveCheckCount != spDupTriangleList[triIdx]) {
          spDupTriangleList[triIdx] = sDupPrimitiveCheckCount;
          ++gTrianglesProcessed;
          u64 matValue = owner.GetTriangleMaterial(triIdx);
          CMaterialList triMat(matValue);
          if (filter.Passes(triMat)) {
            ushort vertIndices[3];
            owner.GetTriangleVertexIndices(triIdx, vertIndices);
            CCollisionSurface surf(owner.GetVert(vertIndices[0]), owner.GetVert(vertIndices[1]),
                                   owner.GetVert(vertIndices[2]), matValue);

            if (CollisionUtil::TriBoxOverlap(center, extent, surf.GetVert(0), surf.GetVert(1),
                                             surf.GetVert(2)) == true) {
              bool triRet = false;
              double d = dOut;
              if (MovingAABoxCollisionCheck_BoxVertexTri(surf, aabb, components.mVertIdxs, dir,
                                                         d, normal, point) &&
                  d < dOut) {
                triRet = true;
                infoOut = CCollisionInfo(point, matList, triMat, normal, static_cast< ushort >(-1));
                ret = true;
                dOut = d;
              }

              for (int k = 0; k < 3; ++k) {
                int vertIdx = vertIndices[k];
                u64 vertMatValue = owner.GetVertMaterial(vertIdx);
                if (!(vertMatValue & (1ull << kMT_NoEdgeCollision))) {
                  if (sDupPrimitiveCheckCount != spDupVertexList[vertIdx]) {
                    spDupVertexList[vertIdx] = sDupPrimitiveCheckCount;
                    const CVector3f& vtx = owner.GetVert(vertIdx);
                    if (movedAABB.PointInside(vtx)) {
                      d = dOut;
                      if (MovingAABoxCollisionCheck_TriVertexBox(vtx, aabb, dir, d, normal,
                                                                 point) &&
                          d < dOut) {
                        CMaterialList vertMat(vertMatValue);
                        triRet = true;
                        infoOut = CCollisionInfo(point, matList, vertMat, normal,
                                                 static_cast< ushort >(-1));
                        ret = true;
                        dOut = d;
                      }
                    }
                  }
                }
              }

              const ushort* edgeIndices = owner.GetTriangleEdgeIndices(triIdx);
              for (int k = 0; k < 3; ++k) {
                int edgeIdx = edgeIndices[k];
                if (sDupPrimitiveCheckCount != spDupEdgeList[edgeIdx]) {
                  spDupEdgeList[edgeIdx] = sDupPrimitiveCheckCount;
                  u64 edgeMat = owner.GetEdgeMaterial(edgeIdx);
                  if (!(edgeMat & (1ull << kMT_NoEdgeCollision))) {
                    d = dOut;
                    const CCollisionEdge& edge = owner.GetEdge(edgeIdx);
                    if (MovingAABoxCollisionCheck_Edge(owner.GetVert(edge.GetVertIndex1()),
                                                       owner.GetVert(edge.GetVertIndex2()),
                                                       components.mEdges, dir, d, normal,
                                                       point) &&
                        d < dOut) {
                      triRet = true;
                      infoOut = CCollisionInfo(point, matList, CMaterialList(edgeMat), normal,
                                           static_cast< ushort >(-1));
                      ret = true;
                      dOut = d;
                    }
                  }
                }
              }

              if (triRet) {
                moveVec = static_cast< float >(dOut) * dir;
                movedAABB = components.mAabb;
                movedAABB.AccumulateBounds(aabb.GetMinPoint() + moveVec);
                movedAABB.AccumulateBounds(aabb.GetMaxPoint() + moveVec);
                center = movedAABB.GetCenterPoint();
                extent = (movedAABB.GetMaxPoint() - movedAABB.GetMinPoint()) * 0.5f;
              }
            } else {
              const ushort* edgeIndices = owner.GetTriangleEdgeIndices(triIdx);
              spDupEdgeList[edgeIndices[0]] = sDupPrimitiveCheckCount;
              spDupEdgeList[edgeIndices[1]] = sDupPrimitiveCheckCount;
              spDupEdgeList[edgeIndices[2]] = sDupPrimitiveCheckCount;
              spDupVertexList[vertIndices[0]] = sDupPrimitiveCheckCount;
              spDupVertexList[vertIndices[1]] = sDupPrimitiveCheckCount;
              spDupVertexList[vertIndices[2]] = sDupPrimitiveCheckCount;
            }
          }
        }
      }
    }
  }

  return ret;
}

bool CMetroidAreaCollider::MovingAABoxCollisionCheck_TriVertexBox(const CVector3f& vert,
                                                                  const CAABox& aabb, CVector3f dir,
                                                                  double& dOut, CVector3f& normalOut,
                                                                  CVector3f& pointOut) {
  bool ret = false;
  float rayLen = static_cast< float >(dOut);
  CMRay ray(vert, -dir, rayLen);
  CVector3f norm(CVector3f::Zero());
  double d;
  if (CollisionUtil::RayAABoxIntersection_Double(ray, aabb, norm, d) == 2) {
    double nd = d * dOut;
    if (nd < dOut) {
      ret = true;
      normalOut = -norm;
      dOut = nd;
      pointOut = vert;
    }
  }
  return ret;
}

bool CMetroidAreaCollider::MovingAABoxCollisionCheck_BoxVertexTri(
    const CCollisionSurface& surf, const CAABox& aabb,
    const rstl::reserved_vector< uint, 8 >& vertIndices, CVector3f dir, double& d,
    CVector3f& normalOut, CVector3f& pointOut) {
  bool ret = false;
  for (int i = 0; i < vertIndices.size(); ++i) {
    CVector3f point = aabb.GetPoint(vertIndices[i]);
    if (CollisionUtil::RayTriangleIntersection_Double(point, dir, &surf.GetVert(0), d)) {
      pointOut = point + dir * static_cast< float >(d);
      normalOut = surf.GetNormal();
      ret = true;
    }
  }
  return ret;
}

bool CMetroidAreaCollider::MovingAABoxCollisionCheck_Edge(
    const CVector3f& ev0, const CVector3f& ev1, const rstl::reserved_vector< SBoxEdge, 12 >& edges,
    const CVector3f& dir, double& d, CVector3f& normal, CVector3f& point) {
  bool ret = false;

  // Measured: retail builds `ev0d` at `464(r1)`, `ev1d` at `440(r1)` and `delta = ev0d - ev1d` at
  // `296(r1)` *before* the loop header at `0x802490d4`, all three with out-of-line
  // `__ct__9CVector3dFRC9CVector3f` / `__mi__FRC9CVector3dRC9CVector3d` calls. The previous source
  // rebuilt them on every iteration.
  CVector3d ev0d = ev0;
  CVector3d ev1d = ev1;
  CVector3d delta = ev0d - ev1d;

  for (int i = 0; i < edges.size(); ++i) {
    const SBoxEdge& edge = edges[i];
    // Measured: retail evaluates the `ev1d` dot first (`0x802490e0`, with `r22 = &ev0d` saved
    // across it) and the `ev0d` dot second (`0x80249100`, `mr r4,r22`), keeping the first
    // comparison's boolean in `r23` for the `cmplw` at `0x8024911c`. The previous source took them
    // the other way round.
    if ((CVector3d::Dot(edge.mCoDir, ev1d) >= edge.mDirCoDirDot) ==
        (CVector3d::Dot(edge.mCoDir, ev0d) >= edge.mDirCoDirDot))
      continue;

    CVector3d cross0 = CVector3d::Cross(edge.mDelta, delta);
    if (cross0.MagSquared() < FLT_EPSILON)
      continue;

    CVector3d cross0Norm = cross0.AsNormalized();
    if (CVector3d::Dot(cross0Norm, dir) >= 0.0) {
      ev1d = ev0;
      ev0d = ev1;
      delta = ev0d - ev1d;
      cross0 = CVector3d::Cross(edge.mDelta, delta);
      cross0Norm = cross0.AsNormalized();
    }

    // Measured: retail takes the two `Dot`s in the other order - `Dot(delta, mCoDir)` at
    // `0x80249234` into `f31`, then `Dot(ev, mCoDir)` at `0x8024924c` - and scales `delta` by the
    // quotient first (`__ml__FdRC9CVector3d` into `104(r1)`) before adding `ev` (`__pl__` into
    // `128(r1)`), so the divisor has to be spelled first here.
    const double deltaCoDirDot = CVector3d::Dot(delta, edge.mCoDir);
    CVector3d clipped =
        ev0d + (-(CVector3d::Dot(ev0d, edge.mCoDir) - edge.mDirCoDirDot) / deltaCoDirDot) * delta;
    // Measured: retail indexes two 3-byte tables at `.rodata:0x803AD854` = {1,0,0} and
    // `0x803AD860` = {2,2,1} by `edge.mDominantAxis` (+0x68) - one `slwi r0,r0,2` then two
    // `lwzx` off `r3`/`r4` - instead of choosing the two non-dominant components with an
    // if/else chain as Prime 1 does. This repo's `SBoxEdge` has the `mDominantAxis` field for
    // exactly this; the previous source computed it from `mCoDir` at run time, which retail
    // never does.
    // Measured further, and it did NOT work: retail *appears* to keep one table per operand -
    // `r8` (the `0x803AD860` table) is scaled by 8 onto the `mDelta` base and `r7` (the
    // `0x803AD854` table) by 4 into `dir`, at 0x802492A4-0x802492BC and again at
    // 0x802492D0-0x802492F0 - so giving `dir` and `mDelta` their own `sBoxEdgeCompIdxA/B`
    // reads is the obvious next spelling. Measured: **77.50%, worse than the 78.31% the shared
    // `ci0` gives.** mwceppc folds the four `lwzx` back onto the two live values either way, so
    // the split costs an instruction and buys nothing. The shared index below is the better
    // spelling and is what stays.
    const int ci0 = sBoxEdgeCompIdxA[edge.mDominantAxis];
    const int ci1 = sBoxEdgeCompIdxB[edge.mDominantAxis];

    const float& dir0 = dir[ci0];
    const float& dir1 = dir[ci1];
    const double& edgeDelta0 = edge.mDelta[ci0];
    const double& edgeDelta1 = edge.mDelta[ci1];
    const double denominator = edgeDelta0 * dir1 - edgeDelta1 * dir0;
    double eMag = (edgeDelta0 * (clipped[ci1] - edge.mStart[ci1]) -
                   edgeDelta1 * (clipped[ci0] - edge.mStart[ci0])) /
                  denominator;

    if (!(eMag < 0.0) && !(eMag >= d)) {
      CVector3d clippedMag = clipped - eMag * CVector3d(dir);
      double dotCheck =
          (edge.mStart.GetX() - clippedMag.GetX()) * (edge.mEnd.GetX() - clippedMag.GetX()) +
          (edge.mStart.GetY() - clippedMag.GetY()) * (edge.mEnd.GetY() - clippedMag.GetY()) +
          (edge.mStart.GetZ() - clippedMag.GetZ()) * (edge.mEnd.GetZ() - clippedMag.GetZ());
      if (dotCheck < 0.0 && eMag < d) {
        normal = cross0Norm.AsCVector3f();
        d = eMag;
        point = clipped.AsCVector3f();
        ret = true;
      }
    }
  }

  return ret;
}

// retail `.text:0x80249034`, 0x20 = 32 bytes. The second parameter is a scalar, not a `TAreaId`:
// retail stores the argument register straight through (`stw r5,0(r3)`) and its caller loads r5
// out of the area object (`lwz r5,4(r31)` in `CGameCollision::BuildAreaCollisionCache`) rather
// than materialising the caller-side temporary a by-value class would need. Written with a
// `TAreaId` parameter the body starts `lwz r0,0(r5)` and scores 77.25%; written `int` it is
// byte-exact. The reasons and the ABI note are at the declaration in the header.
CMetroidAreaCollider::COctreeLeafCache::COctreeLeafCache(const CAreaOctTree& octTree, int areaId)
: mAreaId(areaId), mOctTree(octTree), mOverflow(false) {}

// `fn_80248FEC` - retail `.text:0x80248FEC`, 0x48 = 72 bytes, unnamed in `symbols.txt`. It is
// `rstl::reserved_vector< CAreaOctTree::Node, 64 >::push_back`, the copy `AddLeaf` below makes on
// its `mNodeCache` when there is room.
//
//     80248ffc  mr   r31,r3                          ; `this` moves off r3 for the call
//     80249000  lwz  r0,0(r3)                        ; mCount
//     80249004  mulli r0,r0,36                       ; sizeof(CAreaOctTree::Node) == 0x24
//     80249008  add  r3,r31,r0
//     8024900c  addi r3,r3,4                         ; + mData
//     80249010  bl   fn_80248F0C                     ; construct(data() + mCount, in)
//     80249014  lwz  r3,0(r31) / addi r0,r3,1 / stw r0,0(r31)   ; ++mCount
//
// Two details are measured. The count is reloaded out of `self` before the increment, which is
// what makes the increment read memory rather than reuse the `lwz` at 0x80249000; and the
// element size is the *node* size (0x24, this unit's `NESTED_CHECK_SIZEOF(CAreaOctTree, Node,
// 0x24)`), reached as one `mulli` because `rstl::reserved_vector` holds its elements inline -
// see the public members in `include/rstl/reserved_vector.hpp`.
extern "C" void fn_80248FEC(CNodeVec* self, const CAreaOctTree::Node& in) {
  fn_80248F0C(self->data() + self->mCount, in);
  ++self->mCount;
}

void CMetroidAreaCollider::COctreeLeafCache::AddLeaf(const CAreaOctTree::Node& node) {
  if (mNodeCache.size() == mNodeCache.capacity()) {
    mOverflow = true;
    return;
  }
  mNodeCache.push_back(node);
}

CAreaCollisionCache::CAreaCollisionCache(const CAABox& aabb)
: mAabb(aabb), mLeafOverflow(false), mCacheOverflow(false) {}

// `fn_80248F2C` - retail `.text:0x80248F2C`, 0x28 = 40 bytes, unnamed.
// `rstl::construct_impl< CAreaOctTree::Node >` from `include/rstl/construct.hpp`: the placement
// new's own null test on the destination (0x80248F34 `cmplwi r3,0` / 0x80248F3C `beq`) and then
// one `bl` (0x80248F40) to `fn_802461A0` - `CAreaOctTree::Node`'s copy constructor, which retail
// keeps in another unit (0x802461A0, 0x4C bytes, nine `lfs`/`stfs` pairs) and this object holds
// as its own weak COMDAT copy of the same 76 bytes. The node-sized twin of `fn_80248DDC` below.
extern "C" void fn_80248F2C(void* dest, const CAreaOctTree::Node& src) {
  new (dest) CAreaOctTree::Node(src);
}

// `fn_80248F0C` - retail `.text:0x80248F0C`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< CAreaOctTree::Node >`: a frame and one unconditional `bl` (0x80248F18) to
// `fn_80248F2C`, nothing else. It is written as a call rather than as `rstl::construct(...)`
// because `construct` and `construct_impl` are both in-class-inline and mwceppc folds them -
// folding `construct_impl` here would fold in its null test too, which is retail's *previous*
// function and not this one.
extern "C" void fn_80248F0C(void* dest, const CAreaOctTree::Node& src) { fn_80248F2C(dest, src); }

// `fn_80248EA4` - retail `.text:0x80248EA4`, 0x68 = 104 bytes, unnamed. It is
// `rstl::uninitialized_copy_n< const CAreaOctTree::Node*, CAreaOctTree::Node* >`, called once,
// from `fn_80248E60` below.
//
//     80248eb4  mr   r31,r3 / mr r30,r5 / mr r29,r4    ; src, dest, n
//     80248ec8  b    80248ee4                        ; the count test comes first
//     80248ecc  mr   r3,r30 / mr r4,r31                ; construct(&*cur, *it): a reference, so &*it
//     80248ed4  bl   fn_80248F0C
//     80248ed8  addi r29,r29,-1                       ; --remaining
//     80248edc  addi r31,r31,36                       ; ++it
//     80248ee0  addi r30,r30,36                       ; ++cur
//     80248ee4  cmpwi r29,0 / bne 80248ecc
//     80248ef0  mr   r3,r30                           ; returns the end cursor
//
// The loop is entered at its bottom test and the cursors advance *after* the construct, so a
// zero count copies nothing and still returns `dest`. All three cursors live in non-volatile
// registers, which is what the 0x20 frame is for.
extern "C" CAreaOctTree::Node* fn_80248EA4(const CAreaOctTree::Node* src, int n,
                                           CAreaOctTree::Node* dest) {
  const CAreaOctTree::Node* it = src;
  CAreaOctTree::Node* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    fn_80248F0C(cur, *it);
  }
  return cur;
}

// `fn_80248E60` - retail `.text:0x80248E60`, 0x44 = 68 bytes, unnamed. It is
// `rstl::reserved_vector< CAreaOctTree::Node, 64 >`'s copy constructor, and it has the same
// "store the count, then read it back" shape as `fn_80143CD4` in
// `src/MetroidPrime/Player/CGameState.cpp`, for the same reason:
//
//     80248e6c  lwz  r0,0(r4)                        ; other->mCount
//     80248e78  addi r3,r4,4                         ; other->data()
//     80248e7c  stw  r0,0(r31)                       ; self->mCount
//     80248e80  addi r5,r31,4                        ; self->data()
//     80248e84  lwz  r4,0(r31)                       ; the reload: the bound is self->mCount
//     80248e88  bl   fn_80248EA4
//     80248e90  mr   r3,r31                          ; returns self
//
// The reload at 0x80248E84 is why the bound below is `self->mCount` and not `other->mCount`: with
// the bound taken from `other` there is nothing to reload and the two cursors land in different
// registers. Retail's copy constructor of the *leaf cache* that owns this vector is
// `fn_80248E04` at 0x80248E04, which this object also holds - under its mangled name, so it
// cannot be paired; see the note in `docs/goal-notes/progress-prime1-cmetroidareacollider.md`.
extern "C" CNodeVec* fn_80248E60(CNodeVec* self, const CNodeVec* other) {
  self->mCount = other->mCount;
  fn_80248EA4(other->data(), self->mCount, self->data());
  return self;
}

// `fn_80248DDC` - retail `.text:0x80248DDC`, 0x28 = 40 bytes, unnamed. It is
// `rstl::construct_impl< COctreeLeafCache >`: the same placement new as `fn_80248F2C` above, with
// its null test on the destination at 0x80248DE4 / 0x80248DEC, calling the 92-byte copy
// constructor `fn_80248E04`.
extern "C" void fn_80248DDC(void* dest, const COctreeLeafCache& src) {
  new (dest) COctreeLeafCache(src);
}

// `fn_80248DBC` - retail `.text:0x80248DBC`, 0x20 = 32 bytes, unnamed. It is
// `rstl::construct< COctreeLeafCache >`: a frame and one unconditional `bl` (0x80248DC8) to
// `fn_80248DDC`, nothing else.
extern "C" void fn_80248DBC(void* dest, const COctreeLeafCache& src) { fn_80248DDC(dest, src); }

// `fn_80248D74` - retail `.text:0x80248D74`, 0x48 = 72 bytes, unnamed. It is
// `rstl::reserved_vector< COctreeLeafCache, 3 >::push_back`, the copy `AddOctreeLeafCache` below
// makes at 0x80248D40 once it has checked `mLeafCaches.size() < mLeafCaches.capacity()`.
//
//     80248d84  mr   r31,r3
//     80248d88  lwz  r0,0(r3)                        ; mCount
//     80248d8c  mulli r0,r0,2320                     ; sizeof(COctreeLeafCache) == 0x910
//     80248d90  add  r3,r31,r0
//     80248d94  addi r3,r3,4                         ; + mData
//     80248d98  bl   fn_80248DBC                     ; construct(data() + mCount, in)
//     80248d9c  lwz  r3,0(r31) / addi r0,r3,1 / stw r0,0(r31)
//
// `0x910` is this unit's own `NESTED_CHECK_SIZEOF(CMetroidAreaCollider, COctreeLeafCache, 0x910)`
// and it is what makes the indexing a single `mulli`: the destination is `data() + mCount`, not a
// pointer load, because `rstl::reserved_vector` holds its elements inline. The count is reloaded
// before the increment, as in `fn_80248FEC` above.
extern "C" void fn_80248D74(CLeafCacheVec* self, const COctreeLeafCache& in) {
  fn_80248DBC(self->data() + self->mCount, in);
  ++self->mCount;
}

void CAreaCollisionCache::AddOctreeLeafCache(
    const CMetroidAreaCollider::COctreeLeafCache& leafCache) {
  if (!leafCache.GetNumLeaves())
    return;
  if (leafCache.HasCacheOverflowed())
    mLeafOverflow = true;
  if (mLeafCaches.size() < mLeafCaches.capacity())
    mLeafCaches.push_back(leafCache);
  else {
    mLeafOverflow = true;
    mCacheOverflow = true;
  }
}

void CAreaCollisionCache::SetCacheBounds(const CAABox& aabb) { mAabb = aabb; }

// `fn_80248C94` - retail `.text:0x80248C94`, 0x34 = 52 bytes, unnamed. It is
// `rstl::reserved_vector< COctreeLeafCache, 3 >::clear()`, which `ClearCache` below calls before
// it sets the two flag bits.
//
//     80248ca4  mr   r31,r3
//     80248ca8  bl   8000fcc0                       ; destroy_elements()
//     80248cac  li   r0,0
//     80248cb0  stw  r0,0(r31)                       ; mCount = 0
//
// The count is stored *after* the destroy call and from a fresh `li r0,0`, not from a value the
// destroy loop left behind, and the function returns nothing - there is no `mr r3,r31` before
// the `blr`, which is why `clear()` is a statement here and not an expression.
//
// The `mCount = 0` is written out rather than left to `clear()`'s own body: written as
// `self->clear()` alone, mwceppc folds the whole of `clear()` in and drops the store, which
// measured 32 bytes and 0.00%.
extern "C" void fn_80248C94(CLeafCacheVec* self) {
  self->clear();
  self->mCount = 0;
}

void CAreaCollisionCache::ClearCache() {
  mLeafCaches.clear();
  mLeafOverflow = false;
  mCacheOverflow = false;
}

void CMetroidAreaCollider::BuildOctreeLeafCache(const CAreaOctTree::Node& node, const CAABox& aabb,
                                                COctreeLeafCache& leafCache) {
  for (int i = 0; i < 8; ++i) {
    CAreaOctTree::Node::ETreeType type = node.GetChildType(i);
    if (type == CAreaOctTree::Node::kTT_Invalid)
      continue;
    CAreaOctTree::Node child = node.GetChild(i);
    if (!aabb.DoBoundsOverlap(child.GetBoundingBox()))
      continue;
    if (type == CAreaOctTree::Node::kTT_Leaf)
      leafCache.AddLeaf(child);
    else
      BuildOctreeLeafCache(child, aabb, leafCache);
  }
}

void CMetroidAreaCollider::BuildCollisionCache(const CAreaOctTree::Node& node,
                                               CCollisionCache& cache) {
  ResetInternalCounters();
  // TODO: open a packed-cache geometry group and cache nodes inside its bounds.
}

void CMetroidAreaCollider::CacheNodes(const CAreaOctTree::Node& node, CCollisionCacheWriter& writer,
                                      const CVector3f& center, const CVector3f& halfExtent) {
  // TODO: traverse overlapping nodes and append triangles through the shared writer.
}

void CCollisionCacheWriter::ReserveTriangles(int count) {
  // TODO: reserve packed slots and rebase the leaf-count, triangle-count and bounds pointers.
}

CCachedCollisionSurface::CCachedCollisionSurface(const CCollisionSurface& surface,
                                                 ushort triangleIndex)
: mSurface(surface), mTriangleIndex(triangleIndex) {}

void CCollisionCacheWriter::AddTriangle(const CCollisionSurface& surface, ushort triangleIndex) {
  // TODO: append a triangle payload in a fixed-size packed slot and expand leaf bounds.
}

void CMetroidAreaCollider::CacheAllNodes(const CAreaOctTree::Node& node,
                                         CCollisionCacheWriter& writer) {
  // TODO: append leaf triangles to the packed cache, suppressing duplicates.
}

static void FlagEdgeIndicesForFace(uint face, bool* edgeFlags) {
  switch (face) {
  case 0:
    edgeFlags[10] = true;
    edgeFlags[11] = true;
    edgeFlags[2] = true;
    edgeFlags[4] = true;
    return;
  case 1:
    edgeFlags[8] = true;
    edgeFlags[9] = true;
    edgeFlags[0] = true;
    edgeFlags[6] = true;
    return;
  case 2:
    edgeFlags[4] = true;
    edgeFlags[5] = true;
    edgeFlags[6] = true;
    edgeFlags[7] = true;
    return;
  case 3:
    edgeFlags[0] = true;
    edgeFlags[1] = true;
    edgeFlags[2] = true;
    edgeFlags[3] = true;
    return;
  case 4:
    edgeFlags[7] = true;
    edgeFlags[8] = true;
    edgeFlags[3] = true;
    edgeFlags[11] = true;
    return;
  case 5:
    edgeFlags[1] = true;
    edgeFlags[5] = true;
    edgeFlags[9] = true;
    edgeFlags[10] = true;
    return;
  default:
    break;
  }
}

static void FlagVertexIndicesForFace(uint face, bool* vertFlags) {
  switch (face) {
  case 0:
    vertFlags[1] = true;
    vertFlags[3] = true;
    vertFlags[5] = true;
    vertFlags[7] = true;
    return;
  case 1:
    vertFlags[0] = true;
    vertFlags[2] = true;
    vertFlags[4] = true;
    vertFlags[6] = true;
    return;
  case 2:
    vertFlags[2] = true;
    vertFlags[3] = true;
    vertFlags[6] = true;
    vertFlags[7] = true;
    return;
  case 3:
    vertFlags[0] = true;
    vertFlags[1] = true;
    vertFlags[4] = true;
    vertFlags[5] = true;
    return;
  case 4:
    vertFlags[4] = true;
    vertFlags[5] = true;
    vertFlags[6] = true;
    vertFlags[7] = true;
    return;
  case 5:
    vertFlags[0] = true;
    vertFlags[1] = true;
    vertFlags[2] = true;
    vertFlags[3] = true;
    return;
  default:
    break;
  }
}

// retail `.text:0x8024844C`, 0x70 = 112 bytes. `lis r4,-32703` / `lfdu f1,29704(r4)` is
// 0x80417408 = `CVector3d::sZeroVector`, and each of the four `CVector3d` members is three
// `lfd`/`stfd` pairs out of it, so the members are copy-initialised from that static rather than
// from a `CVector3d(0., 0., 0.)` temporary - the latter is an out-of-line ctor, one `bl` per
// member, and measured 0x80 bytes at 0.00%. `mDirCoDirDot` is the only literal.
CMetroidAreaCollider::SBoxEdge::SBoxEdge()
: mStart(CVector3d::Zero()), mEnd(CVector3d::Zero()), mDelta(CVector3d::Zero()),
  mCoDir(CVector3d::Zero()), mDirCoDirDot(0.) {}

// `fn_80248410` and `fn_802483D4` - retail `.text:0x80248410` and `0x802483D4`, 0x3C = 60 bytes
// each, unnamed, and instruction for instruction identical. They are the *deleting* destructors
// of the two `rstl::reserved_vector` members `CMovingAABoxComponents` owns. Which is which is the
// declaration order in `include/WorldFormat/CMetroidAreaCollider.hpp`: `mEdges` (a
// `reserved_vector< SBoxEdge, 12 >`, 0x544 bytes at +0) before `mVertIdxs` (a
// `reserved_vector< uint, 8 >`, 0x24 bytes at +0x544), and mwceppc emits them in that order.
//
//     802483d4  frame, r31 saved
//     802483e4  mr.  r31,r3                          ; `this`, and the return value
//     802483e8  beq                                 ; if (self == nullptr) return self
//     802483ec  extsh. r0,r4                        ; the delete flag, as a *signed halfword*
//     802483f0  ble                                 ; if (flag <= 0) return self
//     802483f4  bl   802ce388 <CMemory::Free>
//     802483fc  mr   r3,r31
//     8024834c  blr
//
// Three points are measured. The flag is sign-extended from 16 bits by the *function* and the
// sign-extended value is what is tested, so the source compares a `short` and not an `int` -
// the same reading as `fn_80004A4C` in `src/MetroidPrime/Player/CGameStateBlockDtor.cpp`. The
// flag stays in `r4` throughout, because the only call is `CMemory::Free` and it takes its
// argument in `r3`, so nothing has to spill it and only `r31` is saved. And the function
// returns `this` instead of falling off the end, which is the deleting-destructor convention and
// costs nothing.
extern "C" void* fn_80248410(void* self, int flag) {
  if (self != nullptr) {
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void* fn_802483D4(void* self, int flag) {
  if (self != nullptr) {
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_80248360` - retail `.text:0x80248360`, 0x74 = 116 bytes, unnamed, and the first function
// after `CMovingAABoxComponents`' constructor. It is the block copy mwceppc generates for
// `CMetroidAreaCollider::SBoxEdge`'s copy assignment: thirteen `lfd`/`stfd` pairs over the four
// `CVector3d` members and `mDirCoDirDot` (0x00..0x68) and one `lwz`/`stw` for `mDominantAxis`
// (+0x68), then `blr`.
//
// Thirteen pairs and a word is 0x6C bytes of *data*, which is this unit's
// `NESTED_CHECK_SIZEOF(CMetroidAreaCollider, SBoxEdge, 0x70)` less the four bytes of tail padding:
// `CVector3d` is three doubles, so the object is eight-byte aligned and `mDominantAxis` at 0x68
// is followed by padding to 0x70.
//
// Which class's copy assignment this is, is read off the size: 0x6C bytes of data is
// `SBoxEdge` and nothing else in this unit. Retail emits it out of line, so something in retail's
// translation unit assigned an `SBoxEdge`; this file never does - the edge list is meant to be
// built by `CMovingAABoxComponents` - so the function is written out here for the name, exactly as
// `fn_800F4FB4` is written out in `src/MetroidPrime/BodyState/CBSLocomotion.cpp` for the same
// reason.
extern "C" void fn_80248360(SBoxEdge* self, const SBoxEdge* other) { *self = *other; }

CMetroidAreaCollider::CMovingAABoxComponents::CMovingAABoxComponents(const CAABox& aabb,
                                                                     const CVector3f& dir)
: mAabb(aabb) {
  bool edgeFlags[12] = {};
  bool vertFlags[8] = {};
  for (int i = 0; i < 3; ++i) {
    if (dir[i] != 0.f) {
      uint face = i * 2 + (dir[i] < 0.f);
      FlagEdgeIndicesForFace(face, edgeFlags);
      FlagVertexIndicesForFace(face, vertFlags);
    }
  }
  for (uint i = 0; i < 8; ++i) {
    if (vertFlags[i])
      mVertIdxs.push_back(i);
  }
  // TODO: precompute selected edges and their dominant axes; collapse the
  // working bounds to the leading face for motion along a single axis.
}
