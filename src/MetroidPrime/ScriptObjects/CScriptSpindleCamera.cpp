#include "MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CScriptCameraSpline.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrSplineType.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWaypoint.hpp"
#include "MetroidPrime/TCastTo.hpp"

// The two retail data references this constructor makes. Spelled as `1.f` and `kMT_NoStepLogic`,
// MW pools a private copy of each constant into this object - `@622` (`.sdata2`, 4 bytes, 1.0f) and
// `@392` (`.sdata`, 4 bytes, zero) - instead of relocating to the DOL's globals. Both copies are
// live, so the link grows `.sdata2` and `.sdata` by eight bytes each and every address after them
// moves: the object still scores 100% fuzzy while `build.sha1` stops matching retail. A `Matching`
// unit may not own data (`config/G2ME01/splits.txt` claims no `.sdata`/`.sdata2` here), so both are
// declared with C linkage and read where retail reads them; the definitions come from the DOL's own
// `auto_09_80418448_sdata.o` and `auto_11_8041D340_sdata2.o`, as `CWorldStateCtor.cpp` and
// `GlowbugAccessors.cpp` already do.
//
// `lbl_8041D3D0` is `.sdata2:0x8041D3D0`, 4 bytes, `3f800000` = 1.0f - retail's two `lfs f1` of it
// are this constructor's two `CMotionSpline` duration arguments, so passing it in place of the
// literal `1.f` changes no behaviour.
//
// `lbl_804186F0` is `.sdata:0x804186F0`, eight zero bytes, and it is the `lwz r5` of retail's
// `CMaterialList` window (`li r0,0 / lwz r5,<zero> / stw r0,60(r1) / li r3,0 / li r4,1 /
// stw r0,56(r1) / bl __shl2i`). Passing its value as the material keeps the material
// `kMT_NoStepLogic` - the word is zero - and the `CMaterialList` value 1, but the word is read
// through a symbol instead of the constant pool, so the relocation becomes an external reference
// and the pool copy disappears.
extern "C" const float lbl_8041D3D0;
extern "C" const EMaterialTypes lbl_804186F0;

CScriptSpindleCamera::CScriptSpindleCamera(
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
    SLdrSplineType playerType, bool playerLoops)
// Upstream's `CActorParameters` (ninth sync) has no user destructor, so the argument is
// upstream's `CActorParameters::None()`; the comma trick the older header needed to sequence the
// kInvalidUniqueId copy is gone. `lbl_804186F0` stays: `kMT_NoStepLogic` makes MW emit its own
// 8-byte `.sdata` constant, which retail's object does not have.
: CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(lbl_804186F0),
         CActorParameters::None(), kInvalidUniqueId)
, mParameters(flags, angularSpeed, linearSpeed, motionRadius, radialOffset, desiredAngularOffset,
              minAngularOffset, maxAngularOffset, lookAtAngularOffset, lookAtZOffset, zOffset,
              angularConstraint, angularDampening, desiredAngularSpeed, deactivateRadius,
              constraintFlipAngle, fov)
, mTargetSpline(targetLoops, lbl_8041D3D0,
                static_cast< CMotionSpline::ESplineType >(targetType.type))
, mTargetControlSpline(targetControlSpline)
, mPlayerSpline(playerLoops, lbl_8041D3D0,
                static_cast< CMotionSpline::ESplineType >(playerType.type))
, mOrigXf(xf) {}

CScriptSpindleCamera::~CScriptSpindleCamera() {}

void CScriptSpindleCamera::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  CActor::AcceptScriptMsg(mgr, msg);
  if (GetActive()) {
    // Retail compares the message with lis/addi/cmpw plus a taken and a not-taken branch, which
    // is what a switch lowers to here; spelling the same test as `message == kSM_XALD` in an
    // if-condition gets the two-instruction addis/cmplwi idiom instead, 8 bytes short of retail.
    switch (message) {
    case kSM_XALD: {
      rstl::vector< CVector3f > targetPoints;
      rstl::vector< CQuaternion > targetOrientations;
      const TUniqueId target = FindConnectedObject(mgr, kSS_CameraTarget, kSM_Attach);
      if (TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(target))) {
        ScriptCameraSpline::CollectWaypoints(*this, kSS_CameraTarget, kSM_Attach, targetPoints,
                                             targetOrientations, mgr);
        mTargetSpline.Initialise(targetPoints);
      }

      rstl::vector< CVector3f > playerPoints;
      rstl::vector< CQuaternion > playerOrientations;
      const TUniqueId player = FindConnectedObject(mgr, kSS_CameraPlayer, kSM_Attach);
      if (TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(player))) {
        ScriptCameraSpline::CollectWaypoints(*this, kSS_CameraPlayer, kSM_Attach, playerPoints,
                                             playerOrientations, mgr);
        mPlayerSpline.Initialise(playerPoints);
      }
      break;
    }
    default:
      break;
    }
  }
}
