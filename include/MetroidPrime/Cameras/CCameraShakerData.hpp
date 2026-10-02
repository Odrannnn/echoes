#ifndef _CCAMERASHAKERDATA
#define _CCAMERASHAKERDATA

#include "Kyoto/Math/CMayaSpline.hpp"
#include "Kyoto/Math/CVector3f.hpp"

class CCameraShakerData {
public:
  // Guessed flag name
  enum EFlags { kF_ExplicitDuration = 4 };

  CCameraShakerData(float attenuationDistance, float duration, uint flags,
                    const CVector3f& position, const CMayaSpline& horizontalMotion,
                    const CMayaSpline& verticalMotion, const CMayaSpline& forwardMotion,
                    int audioEffect);

  // Declared, not defined in-class, because retail's CEnergyProjectile.o carries it as a real
  // function (0x1D4, 172 bytes) called from SetCameraShakerData rather than inlined into it.
  // Left implicit, mwcceppc folds it into that one caller, and both it and the caller stop
  // matching. Retail defines it in no other object, and nothing outside CEnergyProjectile.cpp
  // assigns a CCameraShakerData, so the definition goes there.
  CCameraShakerData& operator=(const CCameraShakerData& other);

  CCameraShakerData NewTranslation(const CVector3f& position) const;
  CVector3f GetPoint(float time);
  float GetMaxAmplitude();

  // Guessed names: threshold crossings of the three motion splines.
  float FindFirstIntersection(float amplitude);
  float FindLastIntersection(float amplitude);
  void UpdateThresholdTimes();

private:
  uint mFlags;
  float mDuration;
  float mAttenuationDistance;
  CVector3f mPosition;
  CMayaSpline mHorizontalMotion;
  CMayaSpline mForwardMotion;
  CMayaSpline mVerticalMotion;
  int mAudioEffect;
  float mMaxAmplitude;
  float mLastThresholdTime;
  float mFirstThresholdTime;
};
CHECK_SIZEOF(CCameraShakerData, 0xf4)

#endif // _CCAMERASHAKERDATA
