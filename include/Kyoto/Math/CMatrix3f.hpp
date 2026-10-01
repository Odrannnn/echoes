#ifndef _CMATRIX3F
#define _CMATRIX3F

#include "types.h"

#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "rstl/construct.hpp"

// CMatrix3f has no destructor of its own, so it is trivially destructible, and rstl's
// containers take the "nothing to tear down" path for it. Retail's bytes say so:
// `rstl::vector<CMatrix3f>::clear` at 0x802DBD64 is three instructions - `li r0,0`,
// `stw r0,4(r3)`, `blr` - the bare count reset, with no per-element loop.
// Must precede any use of a container's members: mwceppc 2.7 rejects an explicit
// specialisation that arrives after the primary template has been instantiated for it.

//! The out-of-line 0x20-byte head of a CMatrix3f block copy, retail 0x802DBE78. Four
//! doubleword moves, which is how mwceppc writes a 0x20-byte flat block, and it is where the
//! inline size limit puts the head of a 0x24-byte one. It is declared, not defined, here
//! because `rstl::construct_impl` below is its only caller and both units that hold a
//! `rstl::vector<CMatrix3f>` instantiate that; marking it `inline` instead would make mwcceppc
//! inline the moves and lose the call, and a second definition would be a duplicate symbol.
//! The definition lives in `Kyoto/Animation/CPoseAsTransforms_Linear.cpp` - see the comment
//! there for why not in `Kyoto/Math/CMatrix3f.cpp`.
extern "C" void fn_802DBE78(void* self, const void* src);

namespace rstl {
//! A bit-exact overlay of CMatrix3f, used only to spell the element copy the way retail's
//! bytes do. A CMatrix3f held in a container is copied as one flat 0x24-byte block: the head
//! out of line as four doubleword moves and the trailing word inline, because that is where
//! mwceppc's inline size limit puts the split. `mTail` is `m22`, the ninth float, and it
//! moves as bits - retail's `lwz`/`stw` where a memberwise assignment is `lfs`/`stfs`.
struct CMatrix3fBlock {
  double mHead[4];
  uint mTail;
};

template <>
struct is_trivially_destructible< CMatrix3f > {
  enum { value = true };
};

//! `rstl::vector<CMatrix3f>::reserve` (retail 0x802DC2C0) and `::assign` (0x802DBCE4) both
//! call 0x802DBE78 per element and copy the trailing word themselves. The plain
//! `new (dest) T(src)` in the primary `construct_impl` is a call to the out-of-line
//! `__ct__9CMatrix3fFRC9CMatrix3f` (retail 0x802C62F0, 0x2C bytes) instead, and it also brings
//! a null guard on the destination with it, which retail has no trace of.
template <>
inline void construct_impl(void* dest, const CMatrix3f& src) {
  CMatrix3fBlock* self = static_cast< CMatrix3fBlock* >(dest);
  const CMatrix3fBlock* other = reinterpret_cast< const CMatrix3fBlock* >(&src);
  fn_802DBE78(self, other);
  self->mTail = other->mTail;
}
} // namespace rstl

class CInputStream;
class CMatrix3f {
  static const CMatrix3f sIdentity;

public:
  CMatrix3f(const float _m00, const float _m01, const float _m02, const float _m10,
            const float _m11, const float _m12, const float _m20, const float _m21,
            const float _m22)
  : m00(_m00)
  , m01(_m01)
  , m02(_m02)
  , m10(_m10)
  , m11(_m11)
  , m12(_m12)
  , m20(_m20)
  , m21(_m21)
  , m22(_m22) {}
  CMatrix3f(const CVector3f& _m0, const CVector3f& _m1, const CVector3f& _m2);
  CMatrix3f(const CMatrix3f& other, float scale);
  CMatrix3f(const CMatrix3f& left, float leftScale, const CMatrix3f& right, float rightScale);
  CMatrix3f(CInputStream& in);
  CMatrix3f(const CMatrix3f&);
  //  fake but useful for CEulerAngles?
  CMatrix3f(const CTransform4f& xf); /*
   : m0(xf.GetRow(kDX))
   , m1(xf.GetRow(kDY))
   , m2(xf.GetRow(kDZ)) {}*/

  static CMatrix3f FromColumns(const CVector3f& col0, const CVector3f& col1, const CVector3f& col2);
  static CMatrix3f RotateX(const CRelAngle& angle);
  static CMatrix3f RotateY(const CRelAngle& angle);
  static CMatrix3f RotateZ(const CRelAngle& angle);
  const CMatrix3f& operator=(const CMatrix3f& other);
  const CVector3f operator*(const CVector3f&) const;
  const CMatrix3f operator*(const CMatrix3f&) const;

  static const CMatrix3f& Identity() { return sIdentity; }

  CMatrix3f Orthonormalized() const;
  float Determinant() const;
  CMatrix3f Inverse() const;
  CMatrix3f GetTranspose() const { return CMatrix3f(m00, m10, m20, m01, m11, m21, m02, m12, m22); }
  void AddScaledMatrix(const CMatrix3f& mat, float scale);

  // TODO: names/check
  inline const CVector3f& GetRow(EDimX dim) const {
    return *reinterpret_cast< const CVector3f* >(&m00);
  }
  inline const CVector3f& GetRow(EDimY dim) const {
    return *reinterpret_cast< const CVector3f* >(&m10);
  }
  inline const CVector3f& GetRow(EDimZ dim) const {
    return *reinterpret_cast< const CVector3f* >(&m20);
  }

  float Get00() const { return m00; }
  float Get01() const { return m01; }
  float Get02() const { return m02; }
  float Get10() const { return m10; }
  float Get11() const { return m11; }
  float Get12() const { return m12; }
  float Get20() const { return m20; }
  float Get21() const { return m21; }
  float Get22() const { return m22; }

  inline CVector3f GetColumn(EDimX dim) const {
    return CVector3f(GetRow(kDX)[dim], GetRow(kDY)[dim], GetRow(kDZ)[dim]);
  }

  inline CVector3f GetColumn(EDimY dim) const {
    return CVector3f(GetRow(kDX)[dim], GetRow(kDY)[dim], GetRow(kDZ)[dim]);
  }

  inline CVector3f GetColumn(EDimZ dim) const {
    return CVector3f(GetRow(kDX)[dim], GetRow(kDY)[dim], GetRow(kDZ)[dim]);
  }

  inline CVector3f GetColumn(int idx) const {
    switch (idx) {
    case 0:
      return CVector3f(m00, m10, m20);
    case 1:
      return CVector3f(m01, m11, m21);
    case 2:
      return CVector3f(m02, m12, m22);
    default:
      return CVector3f::Zero();
    }
  }

  static inline CMatrix3f Scale(float s) {
    return CMatrix3f(s, 0.f, 0.f, 0.f, s, 0.f, 0.f, 0.f, s);
  }

  static inline CMatrix3f Scale(float x, float y, float z) {
    return CMatrix3f(x, 0.f, 0.f, 0.f, y, 0.f, 0.f, 0.f, z);
  }

  static CMatrix3f FromTransform(const CTransform4f& xf);

private:
  float m00;
  float m01;
  float m02;
  float m10;
  float m11;
  float m12;
  float m20;
  float m21;
  float m22;
};
CHECK_SIZEOF(CMatrix3f, 0x24);

#endif // _CMATRIX3F
