// Retail's `msl_sqrtf` inlines `isnan`, so the pre-2.4.7 MSL classification numbers (FP_NAN 1,
// FP_INFINITE 2, FP_ZERO 3, FP_NORMAL 4, FP_SUBNORMAL 5) reach the instructions. That is the
// branch `libc/math.h` gates behind MSL_OLD_FP_CLASSIFY, and it has to be set before the first
// include, which is the one that pulls `math.h` in.
#define MSL_OLD_FP_CLASSIFY

#include "MetroidPrime/CEulerAngles.hpp"

#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CQuaternion.hpp"

CEulerAngles CEulerAngles::sIdentity(0.f, 0.f, 0.f);

CEulerAngles CEulerAngles::FromTransform(const CTransform4f& xf) {
  return FromMatrix(CMatrix3f::FromTransform(xf));
}

// mwcceppc mangles `msl_sqrtf(float)` and `sqrt(float)` to exactly the retail symbols
// `msl_sqrtf__Ff` and `sqrt__Ff`, so neither needs an `extern "C"`.
//
// Retail 0x8001D678, 228 bytes. `x > 0` is three Newton steps in double precision from an
// `frsqrte` seed. `x <= 0` compares against the **double** literal `0.0` - that is the `lfd` of
// the last pool word, and it is also why the source says `0.0` and not `0.0f` here - and then
// runs the inlined `isnan`, whose `li r0,1` / `li r0,2` / `li r0,3` / `li r0,5` are retail's own
// FP_NAN / FP_INFINITE / FP_ZERO / FP_SUBNORMAL numbers (see MSL_OLD_FP_CLASSIFY above). Both
// failure paths materialise the same NaN from `__float_nan`. `Kyoto/Math/CMathSqrtF.cpp` reaches
// these same bytes as `fn_8001D678`.
float msl_sqrtf(float x) {
  if (x > 0.0f) {
    const double half = .5;
    const double three = 3.0;
    double guess = __frsqrte(static_cast< double >(x));
    guess = half * guess * (three - guess * guess * x);
    guess = half * guess * (three - guess * guess * x);
    guess = half * guess * (three - guess * guess * x);
    return static_cast< float >(x * guess);
  }
  if (x < 0.0) {
    return NAN;
  }
  if (isnan(x)) {
    return NAN;
  }
  return x;
}

float sqrt(float x) { return msl_sqrtf(x); }

// Retail 0x8001D52C, 300 bytes. `sq` is the length of the m01/m11 pair, and each branch negates
// outside the `atan2` call, which is why the `frsp` on each result precedes the `fneg` pair.
// `sqrt` here is the `float sqrt(float)` above, not the `sqrt(double)` in `libc/math.h`: the
// latter is inlined and would put four Newton steps in the middle of this function.
CEulerAngles CEulerAngles::FromMatrix(const CMatrix3f& mtx) {
  const float sq = sqrt(mtx.Get11() * mtx.Get11() + mtx.Get01() * mtx.Get01());
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

// Retail 0x8001D430, 252 bytes, 99.52% - measured, and left as the best spelling rather than a
// guess. It is the same 67 instructions as retail with one register swap left in it: retail puts
// `yy` in f11 and `xx` in f3, this puts `xx` in f11 and `yy` in f3, which also swaps the operands
// of the three `fadds`. The product declaration order below is retail's own instruction order
// (the `sx sy sz` group, then `yy xx zz wx yz wy xz wz xy`); 70-odd other spellings of that order,
// of the `vector` local, of `const`, of the `scale` ternary and of naming the nine matrix entries
// separately were measured and none reached it. What is left is register allocation, not logic.
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
