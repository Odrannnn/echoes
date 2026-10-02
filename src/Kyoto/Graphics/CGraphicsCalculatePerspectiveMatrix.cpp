// `CGraphics::CalculatePerspectiveMatrix` - the static `CGameCamera::GetPerspectiveMatrix` calls.
//
// `src/Kyoto/Graphics/DolphinCGraphics.cpp` is not in `files.cmake` (see
// `CGraphicsGetPerspectiveProjectionMatrix.cpp` for why), so this body had no definition in the
// port's link. `CGameCameraGetPerspectiveMatrix.cpp` needs it, and defining one without the
// other would only trade one undefined symbol for another. The body is verbatim from
// `DolphinCGraphics.cpp:703`; it reads no `CGraphics` state.
#include <math.h>

#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CRelAngle.hpp"

CMatrix4f CGraphics::CalculatePerspectiveMatrix(float fovy, float aspect, float znear, float zfar) {
  float t = tan(CRelAngle::FromDegrees(fovy).AsRadians() / 2.f);
  float right = aspect * 2.f * znear * t * 0.5f;
  float left = -right;
  float top = znear * 2.f * t * 0.5f;
  float bottom = -top;
  // construct in place
  return CMatrix4f(
      // clang-format off
    (2.f * znear) / (right - left),
    -(right + left) / (right - left),
    0.f,
    0.f,
    0.f,
    -(top + bottom) / (top - bottom),
    (2.f * znear) / (top - bottom),
    0.f,
    0.f,
    (zfar + znear) / (zfar - znear),
    0.f,
    -(2.f * zfar * znear) / (zfar - znear),
    0.f,
    1.f,
    0.f,
    0.f
      // clang-format on
  );
}
