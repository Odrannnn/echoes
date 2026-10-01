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

    bool inFrontOf = planes[i].GetHeight(vec.front()) >= 0.f;
    for (int j = 0; j < vec.size(); ++j) {
      const CVector3f& b = vec[j == vec.size() - 1 ? 0 : j + 1];
      if (inFrontOf) {
        otherVec.push_back(vec[j]);
      }
      bool nextInFrontOf = planes[i].GetHeight(b) >= 0.f;
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

bool CMetroidAreaCollider::AABoxCollisionCheck_Internal(const CAreaOctTree::Node&,
                                                        CAABoxAreaCache&) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

bool CMetroidAreaCollider::AABoxCollisionCheck_Cached(const COctreeLeafCache& leafCache,
                                                      const CAABox& aabb,
                                                      const CMaterialFilter& filter,
                                                      const CMaterialList& matList,
                                                      CCollisionInfoList& list) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
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

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean_Internal(const CAreaOctTree::Node&,
                                                               const CBooleanAABoxAreaCache&) {
  // TODO: reconstruct this collision query from the Echoes target. It needs
  // CCollisionPrimitiveData::GetTriangle(), which is declared in the header but
  // has no definition yet; see docs/goal-notes/progress-prime1-cmetroidareacollider.md.
  return false;
}

bool CMetroidAreaCollider::AABoxCollisionCheckBoolean_Cached(const COctreeLeafCache& leafCache,
                                                             const CAABox& aabb,
                                                             const CMaterialFilter& filter) {
  // TODO: reconstruct this collision query from the Echoes target.
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

bool CMetroidAreaCollider::SphereCollisionCheck_Internal(const CAreaOctTree::Node&,
                                                         CSphereAreaCache&) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheck_Cached(const COctreeLeafCache& leafCache,
                                                       const CAABox& aabb, const CSphere& sphere,
                                                       const CMaterialList& matList,
                                                       const CMaterialFilter& filter,
                                                       CCollisionInfoList& list) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
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

bool CMetroidAreaCollider::SphereCollisionCheckBoolean_Internal(const CAreaOctTree::Node&,
                                                                const CBooleanSphereAreaCache&) {
  // TODO: reconstruct this collision query from the Echoes target.
  return false;
}

bool CMetroidAreaCollider::SphereCollisionCheckBoolean_Cached(const COctreeLeafCache& leafCache,
                                                              const CAABox& aabb,
                                                              const CSphere& sphere,
                                                              const CMaterialFilter& filter) {
  // TODO: reconstruct this collision query from the Echoes target.
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
    const CMaterialList& matList, CVector3f dir, float d, CCollisionInfo& infoOut, double& dOut) {
  dOut = d;
  ResetInternalCounters();
  // TODO: test swept box faces, vertices and edges against cached triangles.
  return false;
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
    CVector3f dir, double& d, CVector3f& normal, CVector3f& point) {
  bool ret = false;

  for (int i = 0; i < edges.size(); ++i) {
    const SBoxEdge& edge = edges[i];
    CVector3d ev0d = ev0;
    CVector3d ev1d = ev1;
    if ((CVector3d::Dot(edge.mCoDir, ev0d) >= edge.mDirCoDirDot) ==
        (CVector3d::Dot(edge.mCoDir, ev1d) >= edge.mDirCoDirDot))
      continue;

    CVector3d delta = ev0d - ev1d;
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

    CVector3d clipped = ev0d + (-(CVector3d::Dot(ev0d, edge.mCoDir) - edge.mDirCoDirDot) /
                                CVector3d::Dot(delta, edge.mCoDir)) *
                                   delta;
    int maxCompIdx;
    if (CMath::AbsD(edge.mCoDir.GetX()) > CMath::AbsD(edge.mCoDir.GetY()))
      maxCompIdx = 0;
    else
      maxCompIdx = 1;
    if (CMath::AbsD(edge.mCoDir[maxCompIdx]) < CMath::AbsD(edge.mCoDir.GetZ()))
      maxCompIdx = 2;

    int ci0, ci1;
    if (maxCompIdx == 0) {
      ci0 = 1;
      ci1 = 2;
    } else if (maxCompIdx == 1) {
      ci0 = 0;
      ci1 = 2;
    } else {
      ci0 = 0;
      ci1 = 1;
    }

    const float& dir0 = dir[ci0];
    const float& dir1 = dir[ci1];
    const double& edgeDelta0 = edge.mDelta[ci0];
    const double& edgeDelta1 = edge.mDelta[ci1];
    const double denominator = edgeDelta0 * dir1 - edgeDelta1 * dir0;
    double eMag = (edge.mDelta[ci0] * (clipped[ci1] - edge.mStart[ci1]) -
                   edge.mDelta[ci1] * (clipped[ci0] - edge.mStart[ci0])) /
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

CMetroidAreaCollider::COctreeLeafCache::COctreeLeafCache(const CAreaOctTree& octTree,
                                                         TAreaId areaId)
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

CMetroidAreaCollider::SBoxEdge::SBoxEdge()
: mStart(0., 0., 0.), mEnd(0., 0., 0.), mDelta(0., 0., 0.), mCoDir(0., 0., 0.), mDirCoDirDot(0.) {}

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
