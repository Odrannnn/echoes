#include "MetroidPrime/CEulerAngles.hpp"

#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CQuaternion.hpp"

CEulerAngles CEulerAngles::sIdentity(0.f, 0.f, 0.f);

CEulerAngles CEulerAngles::FromTransform(const CTransform4f& xf) {
  return FromMatrix(CMatrix3f::FromTransform(xf));
}

extern "C" float msl_sqrtf__Ff(float x);

extern "C" float sqrt__Ff(float x) { return msl_sqrtf__Ff(x); }

CEulerAngles CEulerAngles::FromQuaternion(const CQuaternion& quat) {
  const CVector3f& vector = quat.GetVector();
  float magnitudeSquared = vector.GetX() * vector.GetX() + vector.GetY() * vector.GetY() +
                           vector.GetZ() * vector.GetZ() +
                           quat.GetScalar() * quat.GetScalar();
  float scale = magnitudeSquared > 0.f ? 2.f / magnitudeSquared : 0.f;

  float sx = scale * vector.GetX();
  float sy = scale * vector.GetY();
  float sz = scale * vector.GetZ();

  float zz = sz * vector.GetZ();
  float xx = sx * vector.GetX();
  float wz = sz * quat.GetScalar();
  float yy = sy * vector.GetY();
  float xy = sy * vector.GetX();
  float wy = sy * quat.GetScalar();
  float xz = sz * vector.GetX();
  float yz = sz * vector.GetY();
  float wx = sx * quat.GetScalar();

  CMatrix3f mtx(1.f - (yy + zz), xy - wz, xz + wy, xy + wz, 1.f - (xx + zz), yz - wx, xz - wy,
                yz + wx, 1.f - (xx + yy));
  return FromMatrix(mtx);
}
