#ifndef _CPLANE
#define _CPLANE

#include "types.h"

#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Math/CVector3f.hpp"

class CInputStream;

class CPlane {
public:
  CPlane(CInputStream& in);
  CPlane(const CVector3f& vec, const CUnitVector3f& normal)
  : mNormal(normal), mConstant(CVector3f::Dot(vec, normal)) {}
  CPlane(float constant, const CUnitVector3f& normal) : mNormal(normal), mConstant(constant) {}
  CPlane(const CVector3f&, const CVector3f&, const CVector3f&);
  // TODO

  const CUnitVector3f& GetNormal() const { return mNormal; }
  float GetConstant() const { return mConstant; }
  // The normal must be the FIRST Dot operand here. GetHeight is an inline, so it has no symbol
  // of its own: retail's operand order shows up only in the callers that inline it, and they are
  // not uniform - `CPlane::GetClosestPoint` (0x802F7728), `Buckets::Insert` (0x8027249C) and
  // `CFrustumPlanes::SphereInFrustumPlanes` (0x803022F8) all multiply normal-component-first and
  // are byte-exact, while `CFluidPlaneCPU::ClipPolygonToPlane` (0x8013399C) multiplies
  // point-component-first from this same header. So the order is per-call-site scheduling, not a
  // source-order fact: spelling it Dot(pos, GetNormal()) here drops GetClosestPoint from 100% to
  // 95.71% and fails the `main.dol` sha1 gate. ClipLineSegment in CPlane.cpp is genuinely
  // point-first in retail; do not copy its order in here.
  float GetHeight(const CVector3f& pos) const {
    return CVector3f::Dot(GetNormal(), pos) - GetConstant();
  }
  bool IsFacing(const CVector3f& vec) const {
    return CVector3f::Dot(mNormal, vec) >= GetConstant();
  }
  CVector3f GetClosestPoint(const CVector3f& point) const;
  float ClipLineSegment(const CVector3f& start, const CVector3f& end) const;
  
private:
  CUnitVector3f mNormal;
  float mConstant;
};
CHECK_SIZEOF(CPlane, 0x10)

namespace rstl {
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CPlane)
} // namespace rstl

#endif // _CPLANE
