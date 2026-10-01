#ifndef _CCAMERAOVERRIDEINFO
#define _CCAMERAOVERRIDEINFO

#include "MetroidPrime/Cameras/CBallCamera.hpp"

class CCameraOverrideInfo {
public:
  CCameraOverrideInfo(uint flags, uint overrideFlags, CBallCamera::EBallCameraBehaviour behaviour,
                      float minDist, float maxDist, float backwardsDist,
                      const CVector3f& lookAtOffset, const CVector3f& worldOffset, float fov,
                      float attitudeRange, float azimuthRange, float anglePerSecond,
                      float elevation, float interpolateOnTime, float interpolateOffTime,
                      float controlInterpDur, int interpolateOnType, int interpolationMode,
                      int interpolateOffType);
  // Defined inline, not out of line: retail's `CScriptCameraHint::~CScriptCameraHint` stores
  // `__vt__19CCameraOverrideInfo` into the member directly (`addi r3,r30,0x1a8` / `stw r0,0x1a8(r30)`)
  // instead of calling `__dt__19CCameraOverrideInfoFv`, which is what an inlined empty virtual
  // destructor emits. Out of line it becomes a `bl` and the destructor stops at 79.13%.
  virtual ~CCameraOverrideInfo() {}

  CBallCamera::EBallCameraBehaviour GetBehaviourType() const { return mBehaviour; }

private:
  uint mFlags;
  uint mOverrideFlags;
  CBallCamera::EBallCameraBehaviour mBehaviour;
  float mMinDist;
  float mMaxDist;
  float mBackwardsDist;
  CVector3f mLookAtOffset;
  CVector3f mWorldOffset;
  float mFov;
  float mAttitudeRange;
  float mAzimuthRange;
  float mAnglePerSecond;
  float mElevation;
  float mInterpolateOnTime;
  float mInterpolateOffTime;
  float mControlInterpDur;
  int mInterpolateOnType;  // Guessed name; original enum declaration unresolved.
  int mInterpolationMode;  // Guessed name; second interpolation control, meaning unresolved.
  int mInterpolateOffType; // Guessed name; original enum declaration unresolved.
};
CHECK_SIZEOF(CCameraOverrideInfo, 0x60)

#endif // _CCAMERAOVERRIDEINFO
