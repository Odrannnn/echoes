#include "WorldFormat/CCollisionSurface.hpp"

#include "WorldFormat/CCollisionEdge.hpp"
#include "WorldFormat/CCollisionPrimitiveData.hpp"

CUnitVector3f CCollisionSurface::GetNormal() const {
  return CUnitVector3f(
      CVector3f::Cross(mVertices[1] - mVertices[0], mVertices[2] - mVertices[0]).AsNormalized(),
      CUnitVector3f::kN_No);
}

CPlane CCollisionSurface::GetPlane() const {
  const CVector3f surfaceNormal = GetNormal();
  const CUnitVector3f normal(surfaceNormal);
  return CPlane(CVector3f::Dot(normal, mVertices[0]), normal);
}

// Guessed name
CPlane CCollisionSurface::GetEdgePlane(int edge) const {
  const CVector3f surfaceNormal = GetNormal();
  const CUnitVector3f normal(surfaceNormal);
  const int nextVertex[] = {1, 2, 0};
  const CVector3f edgeDirection = mVertices[nextVertex[edge]] - mVertices[edge];
  const CUnitVector3f edgeNormal(CVector3f::Cross(normal, edgeDirection));
  return CPlane(CVector3f::Dot(edgeNormal, mVertices[edge]), edgeNormal);
}

// Guessed name
bool CCollisionSurface::IsDegenerate() const {
  return mVertices[0] == mVertices[1] || mVertices[1] == mVertices[2] ||
         mVertices[0] == mVertices[2];
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
