#ifndef _CCHARANIMTIME
#define _CCHARANIMTIME

#include "types.h"

class COutputStream;
class CInputStream;
class CCharAnimTime {
public:
  enum EType {
    kT_NonZero,
    kT_ZeroIncreasing,
    kT_ZeroSteady,
    kT_ZeroDecreasing,
    kT_Infinity,
  };
  const float GetSeconds() const { return x0_time; }

  explicit CCharAnimTime(CInputStream& in);
  explicit CCharAnimTime(float time = 0.f);
  explicit CCharAnimTime(const EType& type, const float& time) : x0_time(time), x4_type(type) {}

  bool operator>(const CCharAnimTime& other) const;
  bool operator==(const CCharAnimTime& other) const;
  bool operator!=(const CCharAnimTime& other) const;
  bool operator<(const CCharAnimTime& other) const;
  float operator/(const CCharAnimTime& other) const;
  CCharAnimTime operator*(const float& other) const;
  CCharAnimTime operator-(const CCharAnimTime& other) const;
  CCharAnimTime operator+(const CCharAnimTime& other) const;
  const CCharAnimTime& operator+=(const CCharAnimTime& other);
  const CCharAnimTime& operator-=(const CCharAnimTime& other);
  bool operator<=(const CCharAnimTime& other) const;
  bool operator>=(const CCharAnimTime& other) const;
  bool GreaterThanZero() const;
  bool EqualsZero() const;
  void PutTo(COutputStream& out) const;
  // The two constants are put in locals before the constructor call on purpose. Written as one
  // expression (`CCharAnimTime(kT_Infinity, 1.0f)`) MWCC materialises the operands of the call in
  // .data in *every* TU that includes this header, even when the function is never called: 8 bytes
  // per function, 24 here. DOL units absorb that (mwldeppc drops the unreferenced words) but a REL
  // unit with a fixed .data split cannot, and ForgottenObject's is exactly 40 bytes of vtable.
  static CCharAnimTime Infinity() {
    const EType type = kT_Infinity;
    const float time = 1.0f;
    return CCharAnimTime(type, time);
  }
  static CCharAnimTime ZeroPlus() {
    const EType type = kT_ZeroIncreasing;
    const float time = 0.f;
    return CCharAnimTime(type, time);
  }
  static CCharAnimTime ZeroMinus() {
    const EType type = kT_ZeroDecreasing;
    const float time = 0.f;
    return CCharAnimTime(type, time);
  }

  int ZeroOrdering() const {
    if (x4_type == kT_ZeroDecreasing) {
      return -1;
    }
    if (x4_type == kT_ZeroSteady) {
      return 0;
    }
    return 1;
  }

  static EType ZeroTypeFromOrdering(int ordering) {
    if (ordering == -1) {
      return kT_ZeroDecreasing;
    }
    if (ordering == 0) {
      return kT_ZeroSteady;
    }

    return kT_ZeroIncreasing;
  }

  CCharAnimTime ZeroSignScale(float other) const;

private:
  static CCharAnimTime ZeroFlat();

  float x0_time;
  EType x4_type;
};
CHECK_SIZEOF(CCharAnimTime, 0x8)

#endif // _CCHARANIMTIME
