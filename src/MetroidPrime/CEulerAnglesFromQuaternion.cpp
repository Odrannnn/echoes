// `CEulerAngles::FromQuaternion` - the callee `COrbitPointMarker::Update` calls at retail
// 0x800ac25c and 0x800ac4d8, and the only new DOL symbol that function's byte-exact body
// brings into the port's link.
//
// `src/MetroidPrime/CEulerAngles.cpp` already holds these two bodies verbatim, but
// `configure.py` keeps that unit `NonMatching` and `files.cmake` does not list it - the
// same carve-out as `CGameCameraGetPerspectiveMatrix.cpp`, for the same reason: listing
// the whole unit drags in its siblings.
//
// The **whole chain** is here rather than only the first hop, which is what makes it a net
// -1 in the port's undefined count instead of a net +1: `FromQuaternion` calls the private
// static `FromMatrix`, and `FromMatrix`'s square root is MSL's `sqrt(float)` shim over
// `msl_sqrtf(float)`. Defining only `FromQuaternion` trades one undefined symbol for two
// others. `close_enough` and `atan2` are header-only and open nothing.
//
// This is a port-only file (no `configure.py` unit claims it), so the bodies are the
// retail ones with one substitution: `sqrtf` from libm where MSL's `sqrt` shim stood. The
// retail spelling needs `__frsqrte`, a MW builtin with no host equivalent, and its
// `float sqrt(float)` definition collides with the `<math.h>` overload on the host. The
// `#ifdef TARGET_PC` host-body shape is `src/Kyoto/Math/CMathSqrtF.cpp`'s, for the same
// reason. Nothing here is on the boot path's measured behaviour differently: both are the
// same square root to within float rounding.
//
// Bodies otherwise verbatim from `src/MetroidPrime/CEulerAngles.cpp` lines 52-83.
#include <math.h>

#include "MetroidPrime/CEulerAngles.hpp"

#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CQuaternion.hpp"

CEulerAngles CEulerAngles::FromMatrix(const CMatrix3f& mtx) {
  const float sq = sqrtf(mtx.Get11() * mtx.Get11() + mtx.Get01() * mtx.Get01());
  if (!close_enough(sq, 0.f)) {
    const float yaw = atan2(mtx.Get01(), mtx.Get11());
    const float pitch = atan2(mtx.Get20(), mtx.Get22());
    const float roll = atan2(-mtx.Get21(), sq);
    return CEulerAngles(-roll, -pitch, -yaw);
  }
  const float pitch = atan2(-mtx.Get02(), mtx.Get00());
  const float roll = atan2(-mtx.Get21(), sq);
  return CEulerAngles(-roll, -pitch, 0.f);
}

CEulerAngles CEulerAngles::FromQuaternion(const CQuaternion& quat) {
  float magnitudeSquared = quat.GetVector().GetX() * quat.GetVector().GetX() +
                           quat.GetVector().GetY() * quat.GetVector().GetY() +
                           quat.GetVector().GetZ() * quat.GetVector().GetZ() +
                           quat.GetScalar() * quat.GetScalar();
  float scale = magnitudeSquared > 0.f ? 2.f / magnitudeSquared : 0.f;

  float sx = scale * quat.GetVector().GetX();
  float sy = scale * quat.GetVector().GetY();
  float sz = scale * quat.GetVector().GetZ();

  float yy = sy * quat.GetVector().GetY();
  float xx = sx * quat.GetVector().GetX();
  float zz = sz * quat.GetVector().GetZ();
  float wx = sx * quat.GetScalar();
  float yz = sz * quat.GetVector().GetY();
  float wy = sy * quat.GetScalar();
  float xz = sz * quat.GetVector().GetX();
  float wz = sz * quat.GetScalar();
  float xy = sy * quat.GetVector().GetX();

  CMatrix3f mtx(1.f - (yy + zz), xy - wz, xz + wy, xy + wz, 1.f - (xx + zz), yz - wx, xz - wy,
                yz + wx, 1.f - (xx + yy));
  return FromMatrix(mtx);
}