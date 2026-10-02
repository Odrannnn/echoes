#ifndef _CSCRIPTSPINDLECAMERA
#define _CSCRIPTSPINDLECAMERA

#include "Kyoto/Math/CMotionSpline.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/Cameras/CSpindleCamera.hpp"

struct SLdrSplineType;

class CScriptSpindleCamera : public CActor {
public:
  CScriptSpindleCamera(
      TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CTransform4f& xf,
      uint flags, const CSpindleCameraInterpolant& angularSpeed,
      const CSpindleCameraInterpolant& linearSpeed, const CSpindleCameraInterpolant& motionRadius,
      const CSpindleCameraInterpolant& radialOffset,
      const CSpindleCameraInterpolant& desiredAngularOffset,
      const CSpindleCameraInterpolant& minAngularOffset,
      const CSpindleCameraInterpolant& maxAngularOffset,
      const CSpindleCameraInterpolant& lookAtAngularOffset,
      const CSpindleCameraInterpolant& lookAtZOffset, const CSpindleCameraInterpolant& zOffset,
      const CSpindleCameraInterpolant& angularConstraint,
      const CSpindleCameraInterpolant& angularDampening,
      const CSpindleCameraInterpolant& desiredAngularSpeed,
      const CSpindleCameraInterpolant& deactivateRadius,
      const CSpindleCameraInterpolant& constraintFlipAngle, const CSpindleCameraInterpolant& fov,
      SLdrSplineType targetType, const CMayaSpline& targetControlSpline, bool targetLoops,
      SLdrSplineType playerType, bool playerLoops);

  // CEntity
  ~CScriptSpindleCamera() override;
  CEntity* TypesMatch(int typeId) const override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override;

  // Guessed names: retail reads the three splines directly out of the actor - `CSpindleCamera`
  // computes on them through pointers into this object - so nothing in retail names them.
  CMotionSpline& GetTargetSpline() const { return mTargetSpline; }
  CMayaSpline& GetTargetControlSpline() const { return mTargetControlSpline; }
  CMotionSpline& GetPlayerSpline() const { return mPlayerSpline; }

private:
  CSpindleCameraParameters mParameters;
  // Evaluation updates spline caches, not the scripted settings.
  mutable CMotionSpline mTargetSpline;
  mutable CMayaSpline mTargetControlSpline;
  mutable CMotionSpline mPlayerSpline;
  CTransform4f mOrigXf;
};
CHECK_SIZEOF(CScriptSpindleCamera, 0x6e0)

#endif // _CSCRIPTSPINDLECAMERA
