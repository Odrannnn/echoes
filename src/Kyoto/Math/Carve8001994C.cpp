// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`, from the retail function
// `Cross__9CVector3fFRC9CVector3fRC9CVector3f` = `_ZNK9CVector3f5CrossERKS_S1_`.
//
// .text 0x8001994C..0x8001998C, 0x40 = 64 bytes:
//
//   8001994c  lfs     f5,0x8(r4)      ; lhs.z
//   80019950  lfs     f7,0x4(r5)      ; rhs.y
//   80019954  lfs     f3,0x0(r4)      ; lhs.x
//   80019958  lfs     f2,0x8(r5)      ; rhs.z
//   8001995c  fmuls   f0,f7,f5        ; rhs.y * lhs.z
//   80019960  lfs     f4,0x4(r4)      ; lhs.y
//   80019964  lfs     f6,0x0(r5)      ; rhs.x
//   80019968  fmuls   f1,f2,f3        ; rhs.z * lhs.x
//   8001996c  fmsubs  f2,f4,f2,f0     ; lhs.y*rhs.z - (rhs.y*lhs.z)   -> x
//   80019970  fmuls   f0,f6,f4        ; rhs.x * lhs.y
//   80019974  fmsubs  f1,f5,f6,f1     ; lhs.z*rhs.x - (rhs.z*lhs.x)   -> y
//   80019978  stfs    f2,0x0(r3)      ; hidden return pointer, r3
//   8001997c  fmsubs  f0,f3,f7,f0     ; lhs.x*rhs.y - (rhs.x*lhs.y)   -> z
//   80019980  stfs    f1,0x4(r3)
//   80019984  stfs    f0,0x8(r3)
//   80019988  blr
//
// Every multiply is computed *before* any of the three `fmsubs`, and each `fmsubs` consumes
// the two products its own component needs. That is a scheduler that hoisted the six `fmuls`
// out of the three differences, so the three subtractions are independent - which is the
// shape you get from three named `float` locals and one `CVector3f` return, not from three
// inline expressions in a constructor call.
//
// Its own unit: 0x80019990 is `Dot` and 0x80019918 is the previous function, so a unit
// covering either end of this one would be two discontiguous `.text` ranges.
//
// `include/Kyoto/Math/CVector3f.hpp` already declares `Cross` (line 58) and already carries
// the body as a comment (lines 133-138), so this file only has to define it; no header
// changes, and therefore no SDA or layout blast radius.
#include "Kyoto/Math/CVector3f.hpp"

CVector3f CVector3f::Cross(const CVector3f& lhs, const CVector3f& rhs) {
  const float x = (lhs.GetY() * rhs.GetZ()) - (rhs.GetY() * lhs.GetZ());
  const float y = (lhs.GetZ() * rhs.GetX()) - (rhs.GetZ() * lhs.GetX());
  const float z = (lhs.GetX() * rhs.GetY()) - (rhs.GetX() * lhs.GetY());
  return CVector3f(x, y, z);
}
