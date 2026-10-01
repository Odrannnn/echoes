#ifndef _CCOLLISIONSURFACE
#define _CCOLLISIONSURFACE

#include "Kyoto/Math/CPlane.hpp"
#include "Kyoto/Math/CVector3f.hpp"

class CCollisionSurface {
public:
  CCollisionSurface() {}
  CCollisionSurface(const CVector3f& a, const CVector3f& b, const CVector3f& c, u64 flags)
  : mFlags(flags) {
    mVert0 = a;
    mVert1 = b;
    mVert2 = c;
  }

  CVector3f GetNormal() const;
  CPlane GetPlane() const;
  CPlane GetEdgePlane(int edge) const; // Guessed name
  bool IsDegenerate() const;           // Guessed name

  u64 GetSurfaceFlags() const { return mFlags; }
  const CVector3f& GetVert(int index) const { return mVertices()[index]; }

private:
  // Retail declares the three vertices as three separate `CVector3f` members, not a
  // `CVector3f[3]`: the copy of the surface in `optional_object<CCollisionSurface>::operator=`
  // (0x80246258) moves them three 12-byte groups (`lwz/stw` at 0x0/0x4/0x8, then 0xc/0x10/0x14,
  // then 0x18/0x1c/0x20), where an array member makes mwcceppc flatten the whole 0x30 into 8-byte
  // word pairs and pair 0x8 with 0xc. `mVertices()` gives the same address arithmetic the array
  // subscript did, so every call site is unchanged.
  const CVector3f* mVertices() const { return &mVert0; }

  CVector3f mVert0;
  CVector3f mVert1;
  CVector3f mVert2;
  u64 mFlags;
};
CHECK_SIZEOF(CCollisionSurface, 0x30)

#endif // _CCOLLISIONSURFACE
