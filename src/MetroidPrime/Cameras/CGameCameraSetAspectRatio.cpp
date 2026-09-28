// `CGameCamera::SetAspectRatio(float)`.  The body is the one already written in
// `src/MetroidPrime/Cameras/CGameCamera.cpp` (line 31), which `configure.py` holds as a
// `NonMatching` unit (line 435) and `files.cmake` does not list.
//
// **Why a carve-out and not `CGameCamera.cpp`.**  That file is a whole `NonMatching` unit - the
// camera's transform and projection state, the FOV interpolation, the screen-space conversion,
// the input-driven yaw/pitch and the spline follow - and listing it would add all of that and
// its callees, a net *rise* in the port's undefined count.
//
// **Net -1 with no new callees**: the body is two stores.
//
// The dirty flag is the load-bearing half.  `GetPerspectiveMatrix()` is lazy - it rebuilds
// `CGraphics::CalculatePerspectiveMatrix(GetFov(), mAspect, mZnear, mZfar)` only when
// `mPerspDirty` is set - so storing the new aspect without setting the flag would leave the
// camera rendering with the previous projection for as long as the flag stayed clear, and
// nothing else in the class clears it.  Every consumer of the aspect ratio therefore sees the
// new value at the same time the next `GetPerspectiveMatrix()` does.
#include "MetroidPrime/Cameras/CGameCamera.hpp"

void CGameCamera::SetAspectRatio(float aspect) {
  mAspect = aspect;
  mPerspDirty = true;
}
