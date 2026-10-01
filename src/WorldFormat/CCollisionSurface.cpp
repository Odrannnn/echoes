#include "WorldFormat/CCollisionSurface.hpp"

#include "WorldFormat/CCollisionEdge.hpp"
#include "WorldFormat/CCollisionPrimitiveData.hpp"

CVector3f CCollisionSurface::GetNormal() const {
  CVector3f baDiff = mVertices()[1] - mVertices()[0];
  CVector3f caDiff = mVertices()[2] - mVertices()[0];
  return CVector3f::Cross(baDiff, caDiff).AsNormalized();
}

CPlane CCollisionSurface::GetPlane() const {
  CUnitVector3f normal(GetNormal());
  return CPlane(CVector3f::Dot(normal, mVertices()[0]), normal);
}

// Guessed name
CPlane CCollisionSurface::GetEdgePlane(int edge) const {
  CUnitVector3f normal(GetNormal());
  const int nextVertex[] = {1, 2, 0};
  const CVector3f& v = mVertices()[edge];
  const CVector3f edgeDirection = mVertices()[nextVertex[edge]] - v;
  const CUnitVector3f edgeNormal(CVector3f::Cross(normal, edgeDirection));
  return CPlane(CVector3f::Dot(edgeNormal, v), edgeNormal);
}

// Guessed name
bool CCollisionSurface::IsDegenerate() const {
  return mVert0 == mVert1 || mVert1 == mVert2 || mVert0 == mVert2;
}

// `CCollisionPrimitiveData::GetTriangle` is out of line in retail (0x80257A14, an unclaimed gap
// between CAreaRenderOctTree and CProjectileWeapon). It lives with CCollisionSurface because that
// is the type it returns and the port compiles this file while it excludes CAreaOctTree.cpp (see
// tools/check_files_cmake.py), so a definition there would open three link-gap symbols for the
// line tests that call it. The surface indices are three shorts per triangle; only the first two
// name edges, and the third vertex is the one end of the second edge the first edge does not
// already use. Bit 24 of the surface's 64-bit material word selects the reversed winding.
CCollisionSurface CCollisionPrimitiveData::GetTriangle(ushort index) const {
  const int start = index * 3;
  const CCollisionEdge& edge0 = mEdges[mSurfaceIndices[start]];
  const CCollisionEdge& edge1 = mEdges[mSurfaceIndices[start + 1]];
  const u64 material = mMaterials[mSurfaceMaterials[index]];
  if (material & 0x1000000) {
    const ushort third =
        edge1.GetVertIndex1() != edge0.GetVertIndex1() &&
                edge1.GetVertIndex1() != edge0.GetVertIndex2()
            ? edge1.GetVertIndex1()
            : edge1.GetVertIndex2();
    return CCollisionSurface(mVertices[edge0.GetVertIndex2()], mVertices[edge0.GetVertIndex1()],
                             mVertices[third], material);
  }
  const ushort third = edge1.GetVertIndex1() != edge0.GetVertIndex1() &&
                               edge1.GetVertIndex1() != edge0.GetVertIndex2()
                           ? edge1.GetVertIndex1()
                           : edge1.GetVertIndex2();
  return CCollisionSurface(mVertices[edge0.GetVertIndex1()], mVertices[edge0.GetVertIndex2()],
                           mVertices[third], material);
}

#ifdef TARGET_PC
// retail 0x800E88A8 is `CCollisionSurface(v0, v1, v2, flags)` out of line - a strong `T` in
// `MetroidPrime/CDecalManager.o`, not a COMDAT copy - so `src/WorldFormat/CCollisionPrimitiveData.cpp`
// has to call it by that name. The header's constructor is inline and mangles to a C++ name nothing
// references under the retail one, so the host build needs the definition again. The guard direction
// is `#ifdef TARGET_PC`, not `#ifndef`: the port defines TARGET_PC (tools/probe_sources.sh:49) and
// the DOL build does not, so `#ifndef` compiles this out of the build that needs it.
extern "C" void fn_800E88A8(CCollisionSurface* out, const CVector3f* v0, const CVector3f* v1,
                            const CVector3f* v2, u64 flags) {
  *out = CCollisionSurface(*v0, *v1, *v2, flags);
}
#endif
