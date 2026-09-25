#include "Kyoto/Math/CTransform4f.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

CTransform4f CTransform4f::sIdentity(CVector3f(1.0f, 0.0f, 0.0f), CVector3f(0.0f, 1.0f, 0.0f),
                                     CVector3f(0.0f, 0.0f, 1.0f), CVector3f(0.0f, 0.0f, 0.0f));

CTransform4f CTransform4f::LookAt(const CVector3f& pos, const CVector3f& lookPos,
                                  const CVector3f& up) {
  CVector3f look = lookPos - pos;
  const float magnitude = look.Magnitude();
  if (magnitude <= FLT_EPSILON) {
    look = CVector3f(0.0f, 1.0f, 0.0f);
  } else {
    look *= 1.0f / magnitude;
  }

  CVector3f adjustedUp = up - look * CMath::Limit(CVector3f::Dot(up, look), 1.0f);
  float upMagnitude = adjustedUp.Magnitude();
  if (upMagnitude <= FLT_EPSILON) {
    adjustedUp = CVector3f(0.0f, 0.0f, 1.0f) - look * look.GetZ();
    upMagnitude = adjustedUp.Magnitude();
    if (upMagnitude <= FLT_EPSILON) {
      adjustedUp = CVector3f(0.0f, 1.0f, 0.0f) - look * look.GetY();
      upMagnitude = adjustedUp.Magnitude();
    }
  }

  adjustedUp *= 1.0f / upMagnitude;
  const CVector3f right = CVector3f::Cross(look, adjustedUp);
  return CTransform4f(CVector3f(right.GetX(), look.GetX(), adjustedUp.GetX()),
                      CVector3f(right.GetY(), look.GetY(), adjustedUp.GetY()),
                      CVector3f(right.GetZ(), look.GetZ(), adjustedUp.GetZ()), pos);
}

CTransform4f CTransform4f::MakeRotationsBasedOnY(const CUnitVector3f& yRot) {
  uint i;
  if (yRot.GetY() < yRot.GetX()) {
    if (yRot.GetZ() < yRot.GetY()) {
      i = 2;
    } else {
      i = 1;
    }
  } else if (yRot.GetZ() < yRot.GetX()) {
    i = 2;
  } else {
    i = 1;
  }

  CVector3f v(0.0f, 0.0f, 0.0f);
  v[i] = 1.0f;

  CUnitVector3f xRot(CVector3f::Cross(yRot, v));
  return CTransform4f::FromColumns(xRot, yRot, CVector3f::Cross(xRot, yRot), CVector3f::Zero());
}

CTransform4f::CTransform4f(float m0x, float m0y, float m0z, float m1x, float m1y, float m1z,
                           float m2x, float m2y, float m2z, float posX, float posY, float posZ)
: m0(m0x, m0y, m0z), posX(posX), m1(m1x, m1y, m1z), posY(posY), m2(m2x, m2y, m2z), posZ(posZ) {}

CTransform4f CTransform4f::RotateX(const CRelAngle& angle) {
  float fVar1;
  double dVar2;
  double dVar3;

  float s = sine(angle);
  float c = cosine(angle);
  return CTransform4f(CVector3f(1.0f, 0.0f, 0.0f), CVector3f(0.0f, c, -s), CVector3f(0.0f, s, c),
                      CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::RotateY(const CRelAngle& angle) {
  float fVar1;
  double dVar2;
  double dVar3;

  float s = sine(angle);
  float c = cosine(angle);
  return CTransform4f(CVector3f(c, 0.0f, s), CVector3f(0.0f, 1.0f, 0.0f), CVector3f(-s, 0.0f, c),
                      CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::RotateZ(const CRelAngle& angle) {
  float fVar1;
  double dVar2;
  double dVar3;

  float s = sine(angle);
  float c = cosine(angle);
  return CTransform4f(CVector3f(c, -s, 0.0f), CVector3f(s, c, 0.0f), CVector3f(0.0f, 0.0f, 1.0f),
                      CVector3f(0.0f, 0.0f, 0.0f));
}

void CTransform4f::RotateLocalZ(const CRelAngle& angle) {
  float s = sine(angle);
  float c = cosine(angle);

  float oldM0x = m0.GetX();
  float oldM1x = m1.GetX();
  float oldM2x = m2.GetX();
  float oldM0y = m0.GetY();
  float oldM1y = m1.GetY();
  float oldM2y = m2.GetY();

  oldM0x *= s;
  oldM1x *= s;
  oldM2x *= s;
  oldM0y *= s;
  oldM1y *= s;
  oldM2y *= s;

  m0[kDX] *= c;
  m1[kDX] *= c;
  m2[kDX] *= c;
  m0[kDY] *= c;
  m1[kDY] *= c;
  m2[kDY] *= c;

  m0[kDX] += oldM0y;
  m1[kDX] += oldM1y;
  m2[kDX] += oldM2y;
  m0[kDY] -= oldM0x;
  m1[kDY] -= oldM1x;
  m2[kDY] -= oldM2x;
}

void CTransform4f::RotateLocalY(const CRelAngle& angle) {
  float s = sine(angle);
  float c = cosine(angle);

  float oldM0x = m0.GetX();
  float oldM1x = m1.GetX();
  float oldM2x = m2.GetX();
  float oldM0z = m0.GetZ();
  float oldM1z = m1.GetZ();
  float oldM2z = m2.GetZ();

  oldM0x *= s;
  oldM1x *= s;
  oldM2x *= s;
  oldM0z *= s;
  oldM1z *= s;
  oldM2z *= s;

  m0[kDX] *= c;
  m1[kDX] *= c;
  m2[kDX] *= c;
  m0[kDZ] *= c;
  m1[kDZ] *= c;
  m2[kDZ] *= c;

  m0[kDX] -= oldM0z;
  m1[kDX] -= oldM1z;
  m2[kDX] -= oldM2z;
  m0[kDZ] += oldM0x;
  m1[kDZ] += oldM1x;
  m2[kDZ] += oldM2x;
}

void CTransform4f::RotateLocalX(const CRelAngle& angle) {
  float s = sine(angle);
  float c = cosine(angle);

  float oldM0y = m0.GetY();
  float oldM1y = m1.GetY();
  float oldM2y = m2.GetY();
  float oldM0z = m0.GetZ();
  float oldM1z = m1.GetZ();
  float oldM2z = m2.GetZ();

  oldM0y *= s;
  oldM1y *= s;
  oldM2y *= s;
  oldM0z *= s;
  oldM1z *= s;
  oldM2z *= s;

  m0[kDY] *= c;
  m1[kDY] *= c;
  m2[kDY] *= c;
  m0[kDZ] *= c;
  m1[kDZ] *= c;
  m2[kDZ] *= c;

  m0[kDY] += oldM0z;
  m1[kDY] += oldM1z;
  m2[kDY] += oldM2z;
  m0[kDZ] -= oldM0y;
  m1[kDZ] -= oldM1y;
  m2[kDZ] -= oldM2y;
}

CTransform4f::CTransform4f(CInputStream& in) {
  m0.SetX(in.ReadFloat());
  m0.SetY(in.ReadFloat());
  m0.SetZ(in.ReadFloat());
  posX = in.ReadFloat();
  m1.SetX(in.ReadFloat());
  m1.SetY(in.ReadFloat());
  m1.SetZ(in.ReadFloat());
  posY = in.ReadFloat();
  m2.SetX(in.ReadFloat());
  m2.SetY(in.ReadFloat());
  m2.SetZ(in.ReadFloat());
  posZ = in.ReadFloat();
}

CTransform4f::CTransform4f(const CMatrix3f& rotation, const CVector3f& translation) {
  m0 = rotation.GetRow(kDX);
  posX = translation.GetX();
  m1 = rotation.GetRow(kDY);
  posY = translation.GetY();
  m2 = rotation.GetRow(kDZ);
  posZ = translation.GetZ();
}

CTransform4f CTransform4f::Scale(float s) {
  return CTransform4f(CVector3f(s, 0.0f, 0.0f), CVector3f(0.0f, s, 0.0f), CVector3f(0.0f, 0.0f, s),
                      CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::Scale(float x, float y, float z) {
  return CTransform4f(CVector3f(x, 0.0f, 0.0f), CVector3f(0.0f, y, 0.0f), CVector3f(0.0f, 0.0f, z),
                      CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::Scale(const CVector3f& v) {
  return CTransform4f(CVector3f(v.GetX(), 0.0f, 0.0f), CVector3f(0.0f, v.GetY(), 0.0f),
                      CVector3f(0.0f, 0.0f, v.GetZ()), CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::Translate(float x, float y, float z) {
  return CTransform4f(CVector3f(1.0f, 0.0f, 0.0f), CVector3f(0.0f, 1.0f, 0.0f),
                      CVector3f(0.0f, 0.0f, 1.0f), CVector3f(x, y, z));
}

CTransform4f CTransform4f::Translate(const CVector3f& v) {
  return CTransform4f(CVector3f(1.0f, 0.0f, 0.0f), CVector3f(0.0f, 1.0f, 0.0f),
                      CVector3f(0.0f, 0.0f, 1.0f), CVector3f(v.GetX(), v.GetY(), v.GetZ()));
}

CMatrix3f CTransform4f::BuildMatrix3f() const {
  return CMatrix3f(Get00(), Get01(), Get02(), Get10(), Get11(), Get12(), Get20(), Get21(), Get22());
}

#ifdef __MWERKS__
CTransform4f CTransform4f::operator*(const CTransform4f& vec) const {
  register CTransform4f* ret;
  register const CTransform4f* thiz = this;
  register const CTransform4f* x = &vec;
  asm volatile {
    psq_l f0, CTransform4f.m0(thiz), 0, 0
    psq_l f7, CTransform4f.m0.mZ(x), 0, 0
    ps_merge11 f1, f0, f0
    psq_l f4, CTransform4f.m0(x), 0, 0
    ps_merge00 f0, f0, f0
    lfs f3, 0.f
    psq_l f2, CTransform4f.m0.mZ(thiz), 0, 0
    psq_l f8, CTransform4f.m1.mZ(x), 0, 0
    ps_mul f11, f0, f7
    psq_l f5, CTransform4f.m1(x), 0, 0
    ps_mul f10, f0, f4
    psq_l f9, CTransform4f.m2.mZ(x), 0, 0
    ps_merge01 f3, f3, f2
    psq_l f6, CTransform4f.m2(x), 0, 0
    ps_merge00 f2, f2, f2
    psq_l f0, CTransform4f.m1(thiz), 0, 0
    ps_madd f11, f1, f8, f11
    ps_madd f10, f1, f5, f10
    ps_merge11 f1, f0, f0
    ps_madd f11, f2, f9, f11
    ps_madd f10, f2, f6, f10
    psq_l f2, CTransform4f.m1.mZ(thiz), 0, 0
    ps_merge00 f0, f0, f0
    ps_add f11, f3, f11
    psq_st f10, CTransform4f.m0(ret), 0, 0
    ps_merge01 f3, f3, f2
    ps_mul f10, f0, f4
    psq_st f11, CTransform4f.m0.mZ(ret), 0, 0
    ps_mul f11, f0, f7
    ps_merge00 f2, f2, f2
    psq_l f0, CTransform4f.m2(thiz), 0, 0
    ps_madd f10, f1, f5, f10
    ps_madd f11, f1, f8, f11
    ps_merge11 f1, f0, f0
    ps_madd f10, f2, f6, f10
    ps_madd f11, f2, f9, f11
    psq_l f2, CTransform4f.m2.mZ(thiz), 0, 0
    ps_merge00 f0, f0, f0
    psq_st f10, CTransform4f.m1(ret), 0, 0
    ps_add f11, f3, f11
    ps_mul f10, f0, f4
    ps_merge01 f3, f3, f2
    psq_st f11, CTransform4f.m1.mZ(ret), 0, 0
    ps_mul f11, f0, f7
    ps_merge00 f2, f2, f2
    ps_madd f10, f1, f5, f10
    ps_madd f11, f1, f8, f11
    ps_madd f10, f2, f6, f10
    ps_madd f11, f2, f9, f11
    psq_st f10, CTransform4f.m2(ret), 0, 0
    ps_add f11, f3, f11
    psq_st f11, CTransform4f.m2.mZ(ret), 0, 0
  }
  // The result was stored through the hidden return pointer.
}
#else
CTransform4f CTransform4f::operator*(const CTransform4f& vec) const {
  return CTransform4f(
      m0.GetX() * vec.m0.GetX() + m0.GetY() * vec.m1.GetX() + m0.GetZ() * vec.m2.GetX(),
      m0.GetX() * vec.m0.GetY() + m0.GetY() * vec.m1.GetY() + m0.GetZ() * vec.m2.GetY(),
      m0.GetX() * vec.m0.GetZ() + m0.GetY() * vec.m1.GetZ() + m0.GetZ() * vec.m2.GetZ(),
      m1.GetX() * vec.m0.GetX() + m1.GetY() * vec.m1.GetX() + m1.GetZ() * vec.m2.GetX(),
      m1.GetX() * vec.m0.GetY() + m1.GetY() * vec.m1.GetY() + m1.GetZ() * vec.m2.GetY(),
      m1.GetX() * vec.m0.GetZ() + m1.GetY() * vec.m1.GetZ() + m1.GetZ() * vec.m2.GetZ(),
      m2.GetX() * vec.m0.GetX() + m2.GetY() * vec.m1.GetX() + m2.GetZ() * vec.m2.GetX(),
      m2.GetX() * vec.m0.GetY() + m2.GetY() * vec.m1.GetY() + m2.GetZ() * vec.m2.GetY(),
      m2.GetX() * vec.m0.GetZ() + m2.GetY() * vec.m1.GetZ() + m2.GetZ() * vec.m2.GetZ(),
      m0.GetX() * vec.posX + m0.GetY() * vec.posY + m0.GetZ() * vec.posZ + posX,
      m1.GetX() * vec.posX + m1.GetY() * vec.posY + m1.GetZ() * vec.posZ + posY,
      m2.GetX() * vec.posX + m2.GetY() * vec.posY + m2.GetZ() * vec.posZ + posZ);
}
#endif

CTransform4f CTransform4f::MultiplyIgnoreTranslation(const CTransform4f& other) {
  return CTransform4f(
      CVector3f(m0.GetX() * other.m0.GetX() + m0.GetY() * other.m1.GetX() + m0.GetZ() * other.m2.GetX(),
                m0.GetX() * other.m0.GetY() + m0.GetY() * other.m1.GetY() + m0.GetZ() * other.m2.GetY(),
                m0.GetX() * other.m0.GetZ() + m0.GetY() * other.m1.GetZ() + m0.GetZ() * other.m2.GetZ()),
      CVector3f(m1.GetX() * other.m0.GetX() + m1.GetY() * other.m1.GetX() + m1.GetZ() * other.m2.GetX(),
                m1.GetX() * other.m0.GetY() + m1.GetY() * other.m1.GetY() + m1.GetZ() * other.m2.GetY(),
                m1.GetX() * other.m0.GetZ() + m1.GetY() * other.m1.GetZ() + m1.GetZ() * other.m2.GetZ()),
      CVector3f(m2.GetX() * other.m0.GetX() + m2.GetY() * other.m1.GetX() + m2.GetZ() * other.m2.GetX(),
                m2.GetX() * other.m0.GetY() + m2.GetY() * other.m1.GetY() + m2.GetZ() * other.m2.GetY(),
                m2.GetX() * other.m0.GetZ() + m2.GetY() * other.m1.GetZ() + m2.GetZ() * other.m2.GetZ()),
      CVector3f(other.posX + posX, other.posY + posY, other.posZ + posZ));
}

void CTransform4f::ScaleBy(float s) {
  m0 *= s;
  m1 *= s;
  m2 *= s;
}

CTransform4f CTransform4f::FromColumns(const CVector3f& v1, const CVector3f& v2,
                                       const CVector3f& v3, const CVector3f& pos) {
  return CTransform4f(CVector3f(v1.GetX(), v2.GetX(), v3.GetX()),
                      CVector3f(v1.GetY(), v2.GetY(), v3.GetY()),
                      CVector3f(v1.GetZ(), v2.GetZ(), v3.GetZ()), pos);
}

CTransform4f CTransform4f::FromRows(const CVector3f& row0, const CVector3f& row1,
                                    const CVector3f& row2, const CVector3f& pos) {
  return CTransform4f(row0, row1, row2, pos);
}

void CTransform4f::SetRotation(const CTransform4f& rotation) {
  m0 = rotation.m0;
  m1 = rotation.m1;
  m2 = rotation.m2;
}

CTransform4f CTransform4f::GetRotation() const {
  return CTransform4f(m0, m1, m2, CVector3f(0.0f, 0.0f, 0.0f));
}

CTransform4f CTransform4f::GetQuickInverse() const {
  const float negX = -posX;
  return CTransform4f(CVector3f(m0.GetX(), m1.GetX(), m2.GetX()),
                      CVector3f(m0.GetY(), m1.GetY(), m2.GetY()),
                      CVector3f(m0.GetZ(), m1.GetZ(), m2.GetZ()),
                      CVector3f(-(m2.GetX() * posZ - (m0.GetX() * negX - m1.GetX() * posY)),
                                -(m2.GetY() * posZ - (m0.GetY() * negX - m1.GetY() * posY)),
                                -(m2.GetZ() * posZ - (m0.GetZ() * negX - m1.GetZ() * posY))));
}

void CTransform4f::Orthonormalize() {
  const CVector3f right = GetColumn(kDX).AsNormalized();
  const CVector3f& forwardColumn = GetColumn(kDY);
  CVector3f upCross = CVector3f::Cross(right, forwardColumn);
  const CVector3f up = upCross.AsNormalized();
  CVector3f forward = CVector3f::Cross(up, right);
  m0.SetX(right.GetX());
  m1.SetX(right.GetY());
  m2.SetX(right.GetZ());
  m0.SetY(forward.GetX());
  m1.SetY(forward.GetY());
  m2.SetY(forward.GetZ());
  m0.SetZ(up.GetX());
  m1.SetZ(up.GetY());
  m2.SetZ(up.GetZ());
}

#ifdef __MWERKS__
CTransform4f& CTransform4f::operator=(const CTransform4f& other) {
  register CTransform4f* thiz = this;
  register const CTransform4f* src = &other;
  asm {
    lfd f0, CTransform4f.m0(src)
    lfd f1, CTransform4f.m0.mZ(src)
    lfd f2, CTransform4f.m1(src)
    stfd f0, CTransform4f.m0(thiz)
    stfd f1, CTransform4f.m0.mZ(thiz)
    stfd f2, CTransform4f.m1(thiz)
    lfd f0, CTransform4f.m1.mZ(src)
    lfd f1, CTransform4f.m2(src)
    lfd f2, CTransform4f.m2.mZ(src)
    stfd f0, CTransform4f.m1.mZ(thiz)
    stfd f1, CTransform4f.m2(thiz)
    stfd f2, CTransform4f.m2.mZ(thiz)
  }
  return *this;
}

CTransform4f::CTransform4f(const CTransform4f& other) {
  register CTransform4f* thiz = this;
  register const CTransform4f* src = &other;
  asm {
    lfd f0, CTransform4f.m0(src)
    lfd f1, CTransform4f.m0.mZ(src)
    lfd f2, CTransform4f.m1(src)
    stfd f0, CTransform4f.m0(thiz)
    stfd f1, CTransform4f.m0.mZ(thiz)
    stfd f2, CTransform4f.m1(thiz)
    lfd f0, CTransform4f.m1.mZ(src)
    lfd f1, CTransform4f.m2(src)
    lfd f2, CTransform4f.m2.mZ(src)
    stfd f0, CTransform4f.m1.mZ(thiz)
    stfd f1, CTransform4f.m2(thiz)
    stfd f2, CTransform4f.m2.mZ(thiz)
  }
}
#else
CTransform4f& CTransform4f::operator=(const CTransform4f& other) {
  m0 = other.m0;
  posX = other.posX;
  m1 = other.m1;
  posY = other.posY;
  m2 = other.m2;
  posZ = other.posZ;
  return *this;
}

CTransform4f::CTransform4f(const CTransform4f& other)
: m0(other.m0), posX(other.posX), m1(other.m1), posY(other.posY), m2(other.m2), posZ(other.posZ) {}
#endif

#ifdef __MWERKS__
CVector3f CTransform4f::operator*(const CVector3f& vec) const {
  // The return slot is written directly; ret is the hidden result pointer.
  register const CVector3f* ret;
  register const CTransform4f* thiz = this;
  register const CVector3f* v = &vec;
  asm {
    psq_l f2, CVector3f.mX(v), 0, 0
    psq_l f3, CVector3f.mZ(v), 1, 0
    psq_l f4, CTransform4f.m0(thiz), 0, 0
    psq_l f5, CTransform4f.m0.mZ(thiz), 0, 0
    ps_mul f0, f4, f2
    psq_l f4, CTransform4f.m1(thiz), 0, 0
    ps_madd f0, f5, f3, f0
    ps_mul f1, f4, f2
    psq_l f5, CTransform4f.m1.mZ(thiz), 0, 0
    psq_l f4, CTransform4f.m2(thiz), 0, 0
    ps_mul f2, f4, f2
    ps_madd f1, f5, f3, f1
    psq_l f5, CTransform4f.m2.mZ(thiz), 0, 0
    ps_madd f2, f5, f3, f2
    lfs f6, 1.f
    ps_madds1 f0, f6, f0, f0
    ps_madds1 f1, f6, f1, f1
    ps_madds1 f2, f6, f2, f2
    stfs f0, CVector3f.mX(ret)
    stfs f1, CVector3f.mY(ret)
    stfs f2, CVector3f.mZ(ret)
  }
  return *ret;
}
#else
CVector3f CTransform4f::operator*(const CVector3f& vec) const {
  return CVector3f(m0.GetX() * vec.GetX() + m0.GetY() * vec.GetY() + m0.GetZ() * vec.GetZ() + posX,
                   m1.GetX() * vec.GetX() + m1.GetY() * vec.GetY() + m1.GetZ() * vec.GetZ() + posY,
                   m2.GetX() * vec.GetX() + m2.GetY() * vec.GetY() + m2.GetZ() * vec.GetZ() + posZ);
}
#endif

#ifdef __MWERKS__
CVector3f CTransform4f::Rotate(const CVector3f& in) const {
  register const CVector3f* ret;
  register const CTransform4f* thiz = this;
  register const CVector3f* v = &in;
  asm {
    lfs f7, 0.f
    lfs f6, 1.f
    psq_l f5, CTransform4f.m0.mZ(thiz), 1, 0
    psq_l f2, CVector3f.mX(v), 0, 0
    psq_l f4, CTransform4f.m0(thiz), 0, 0
    ps_merge00 f5, f5, f7
    psq_l f3, CVector3f.mZ(v), 1, 0
    ps_mul f0, f4, f2
    psq_l f4, CTransform4f.m1(thiz), 0, 0
    ps_mul f1, f4, f2
    psq_l f4, CTransform4f.m2(thiz), 0, 0
    ps_madd f0, f5, f3, f0
    psq_l f5, CTransform4f.m1.mZ(thiz), 1, 0
    ps_mul f2, f4, f2
    ps_merge00 f5, f5, f7
    ps_madds1 f0, f6, f0, f0
    ps_madd f1, f5, f3, f1
    psq_l f5, CTransform4f.m2.mZ(thiz), 1, 0
    ps_merge00 f5, f5, f7
    ps_madds1 f1, f6, f1, f1
    ps_madd f2, f5, f3, f2
    ps_madds1 f2, f6, f2, f2
    stfs f0, CVector3f.mX(ret)
    stfs f1, CVector3f.mY(ret)
    stfs f2, CVector3f.mZ(ret)
  }
  return *ret;
}
#else
CVector3f CTransform4f::Rotate(const CVector3f& in) const {
  return CVector3f(m0.GetX() * in.GetX() + m0.GetY() * in.GetY() + m0.GetZ() * in.GetZ(),
                   m1.GetX() * in.GetX() + m1.GetY() * in.GetY() + m1.GetZ() * in.GetZ(),
                   m2.GetX() * in.GetX() + m2.GetY() * in.GetY() + m2.GetZ() * in.GetZ());
}
#endif

#ifdef __MWERKS__
CVector3f CTransform4f::TransposeRotate(const CVector3f& in) const {
  register const CVector3f* ret;
  register const CTransform4f* thiz = this;
  register const CVector3f* v = &in;
  asm volatile {
    lfs f0, CVector3f.mX(v)
    lfs f1, CVector3f.mY(v)
    ps_merge00 f3, f0, f0
    psq_l f4, CTransform4f.m0(thiz), 0, 0
    lfs f2, CVector3f.mZ(v)
    ps_merge00 f1, f1, f1
    lfs f0, CTransform4f.m1.mZ(thiz)
    ps_mul f5, f4, f3
    psq_l f6, CTransform4f.m1(thiz), 0, 0
    ps_merge00 f4, f2, f2
    psq_l f7, CTransform4f.m2(thiz), 0, 0
    fmuls f0, f0, f1
    ps_madd f5, f6, f1, f5
    lfs f1, CTransform4f.m0.mZ(thiz)
    lfs f2, CTransform4f.m2.mZ(thiz)
    ps_madd f5, f7, f4, f5
    fmadds f0, f1, f3, f0
    ps_merge11 f1, f5, f5
    stfs f5, CVector3f.mX(ret)
    fmadds f0, f2, f4, f0
    stfs f1, CVector3f.mY(ret)
    stfs f0, CVector3f.mZ(ret)
  }
  return *ret;
}
#else
CVector3f CTransform4f::TransposeRotate(const CVector3f& in) const {
  return CVector3f(m0.GetX() * in.GetX() + m1.GetX() * in.GetY() + m2.GetX() * in.GetZ(),
                   m0.GetY() * in.GetX() + m1.GetY() * in.GetY() + m2.GetY() * in.GetZ(),
                   m0.GetZ() * in.GetX() + m1.GetZ() * in.GetY() + m2.GetZ() * in.GetZ());
}
#endif

CTransform4f CTransform4f::GetInverse() const {
  const float a00 = m0.GetX();
  const float a01 = m0.GetY();
  const float a02 = m0.GetZ();
  const float a10 = m1.GetX();
  const float a11 = m1.GetY();
  const float a12 = m1.GetZ();
  const float a20 = m2.GetX();
  const float a21 = m2.GetY();
  const float a22 = m2.GetZ();

  const float c00 = a11 * a22 - a12 * a21;
  const float c01 = a02 * a21 - a01 * a22;
  const float c02 = a01 * a12 - a02 * a11;
  const float c10 = a12 * a20 - a10 * a22;
  const float c11 = a00 * a22 - a02 * a20;
  const float c12 = a02 * a10 - a00 * a12;
  const float c20 = a10 * a21 - a11 * a20;
  const float c21 = a01 * a20 - a00 * a21;
  const float c22 = a00 * a11 - a01 * a10;
  const float invDet = 1.0f / (a02 * c20 + a00 * c00 + a01 * c10);

  const float inversePosX =
      invDet * (a21 * (-a02 * posY + posX * a12) + a01 * (-a12 * posZ + posY * a22) +
                a11 * (a02 * posZ - posX * a22));
  const float inversePosY =
      invDet * (a20 * (a02 * posY - posX * a12) + a00 * (a12 * posZ - posY * a22) +
                a10 * (-a02 * posZ + posX * a22));
  const float inversePosZ =
      invDet * (a20 * (-a01 * posY + posX * a11) + a00 * (-a11 * posZ + posY * a21) +
                a10 * (a01 * posZ - posX * a21));

  return CTransform4f(c00 * invDet, c01 * invDet, c02 * invDet, c10 * invDet, c11 * invDet,
                      c12 * invDet, c20 * invDet, c21 * invDet, c22 * invDet, inversePosX,
                      inversePosY, inversePosZ);
}

CTransform4f::CTransform4f(float m0y, float m0z, float m1x, float m1z, float m2x, float m2y)
: m0(1.f, m0y, m0z), posX(0.f), m1(m1x, 1.f, m1z), posY(0.f), m2(m2x, m2y, 1.f), posZ(0.f) {}
