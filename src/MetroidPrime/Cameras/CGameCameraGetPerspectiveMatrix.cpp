// `CGameCamera::GetPerspectiveMatrix() const`.  The body is the one already written in
// `src/MetroidPrime/Cameras/CGameCamera.cpp` (line 90), which `configure.py` holds as a
// `NonMatching` unit and `files.cmake` does not list - the same carve-out as
// `CGameCameraSetAspectRatio.cpp`, for the same reason: listing the whole unit adds its callees.
//
// `CCompoundTargetReticle::CalculateClampedScale` (byte-exact) calls this twice, which would
// otherwise raise the port's undefined count by one. The body adds no new callee:
// `CGraphics::CalculatePerspectiveMatrix` is defined in `DolphinCGraphics.cpp`, and `GetFov` is
// already in the recorded baseline.
#include "Kyoto/Graphics/CGraphics.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"

const CMatrix4f& CGameCamera::GetPerspectiveMatrix() const {
  if (mPerspDirty == true) {
    mPerspectiveMatrix = CGraphics::CalculatePerspectiveMatrix(GetFov(), mAspect, mZnear, mZfar);
    mPerspDirty = false;
  }
  return mPerspectiveMatrix;
}
