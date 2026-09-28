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
  const float GetSeconds() const { return mTime; }

  explicit CCharAnimTime(CInputStream& in);
  explicit CCharAnimTime(float time = 0.f);
  explicit CCharAnimTime(const EType& type, const float& time) : mTime(time), mType(type) {}

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
  static CCharAnimTime Infinity();
#ifdef CCHARANIMTIME_LOCAL_CONSTANTS
  // Opt-in form with the constants in locals. Passed straight to the const-reference constructor,
  // MWCC materialises them in .sdata in every TU that includes this header, even when nothing calls
  // these: 36 bytes across the five. mwldeppc keeps the unreferenced words, so a unit whose .sdata
  // split is smaller cannot link them (CAi's is 0x10 bytes). CCharAnimTime.cpp itself needs the
  // direct form, so this stays opt-in.
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
#else
  static CCharAnimTime ZeroPlus() { return CCharAnimTime(kT_ZeroIncreasing, 0.f); }
  static CCharAnimTime ZeroMinus() { return CCharAnimTime(kT_ZeroDecreasing, 0.f); }
#endif

  int ZeroOrdering() const {
    if (mType == kT_ZeroDecreasing) {
      return -1;
    }
    if (mType == kT_ZeroSteady) {
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

  static CCharAnimTime ZeroFlat();

private:
  float mTime;
  EType mType;
};
CHECK_SIZEOF(CCharAnimTime, 0x8)

inline CCharAnimTime CCharAnimTime::ZeroSignScale(float other) const {
  if (other > 0.f) {
    return *this;
  } else if (other < 0.f) {
#ifdef CCHARANIMTIME_LOCAL_CONSTANTS
    const float time = 0.f;
    return CCharAnimTime(ZeroTypeFromOrdering(-ZeroOrdering()), time);
#else
    return CCharAnimTime(ZeroTypeFromOrdering(-ZeroOrdering()), 0.f);
#endif
  }
  return ZeroFlat();
}

#ifdef CCHARANIMTIME_LOCAL_CONSTANTS
inline CCharAnimTime CCharAnimTime::ZeroFlat() {
  const EType type = kT_ZeroSteady;
  const float time = 0.f;
  return CCharAnimTime(type, time);
}

inline CCharAnimTime CCharAnimTime::Infinity() {
  const EType type = kT_Infinity;
  const float time = 1.f;
  return CCharAnimTime(type, time);
}
#else
inline CCharAnimTime CCharAnimTime::ZeroFlat() { return CCharAnimTime(kT_ZeroSteady, 0.f); }

inline CCharAnimTime CCharAnimTime::Infinity() { return CCharAnimTime(kT_Infinity, 1.f); }
#endif

#endif // _CCHARANIMTIME
