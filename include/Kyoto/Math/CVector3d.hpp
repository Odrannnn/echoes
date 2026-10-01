#ifndef _CVECTOR3D
#define _CVECTOR3D

#include "Kyoto/Math/CVector3f.hpp"

class CMotionState;

class CVector3d {
public:
  CVector3d(double x, double y, double z);
  CVector3d(const CVector3f& other);
  double Magnitude() const;
  double MagSquared() const;

  CVector3d AsNormalized() const;
  CVector3f AsCVector3f() const;

  double GetX() const { return mX; }
  double GetY() const { return mY; }
  double GetZ() const { return mZ; }
  double& operator[](int index) { return (&mX)[index]; }
  const double& operator[](int index) const { return (&mX)[index]; }

  static double Dot(const CVector3d& a, const CVector3d& b);
  static CVector3d Cross(const CVector3d& a, const CVector3d& b);

  // retail `CMetroidAreaCollider::SBoxEdge`'s default constructor (0x8024844C) initialises its four
  // `CVector3d` members by copying twelve doubles out of `sZeroVector`: `lis r4,-32703` /
  // `lfdu f1,29704(r4)` is 0x80417408, which is where this file's `sZeroVector` already sits.
  // Spelled as `CVector3d(0., 0., 0.)` the three-argument ctor is out of line and each member costs
  // a `bl` (measured 0x80 bytes, 0.00%), so the zero has to be spelled as this static.
  static const CVector3d& Zero() { return sZeroVector; }

private:
  double mX;
  double mY;
  double mZ;
  
  static CVector3d sZeroVector;
  static CVector3d sUpVector;
  static CVector3d sDownVector;
  static CVector3d sLeftVector;
  static CVector3d sRightVector;
  static CVector3d sForwardVector;
  static CVector3d sBackVector;
};

CVector3d operator+(const CVector3d& lhs, const CVector3d& rhs);
CVector3d operator-(const CVector3d& lhs, const CVector3d& rhs);
CVector3d operator-(const CVector3d& v);
CVector3d operator*(double lhs, const CVector3d& rhs);

#endif // _CVECTOR3D
