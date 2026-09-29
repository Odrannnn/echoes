// `CGrappleArm::Render` - retail `Render__11CGrappleArmCFRC13CStateManagerRC9CVector3fRC11CModelFlagsPC12CActorLights`.
//
// `CPlayerGun::DrawArm` (src/MetroidPrime/Player/CPlayerGun.cpp) calls it, and `files.cmake`
// does not list `src/MetroidPrime/Player/CGrappleArm.cpp`, so the port's link had no definition
// and the symbol went undefined when that body was decompiled.
//
// **Why a carve-out and not `CGrappleArm.cpp`.** Same reason as
// `CGrappleArmTouchModel.cpp` and `CGrappleArmReturnToDefault.cpp`: that file is a whole
// `NonMatching` unit and listing it is a net *rise* in the port's undefined count.
//
// The body is the one already written in `CGrappleArm.cpp:180`, verbatim - the arm model drawn
// through `Translate(pos) * mTransform * mAuxTransform` with the shader set masked to the local
// player, the rain-splash point generator installed around it, and the grapple gear drawn
// through its own locator. The arm's own point generator comes with it, from the `NonMatching`
// unit unchanged: `Render` installs it as the skinned model's point generator and the linker
// needs it in the same translation unit.
#include "MetroidPrime/Player/CGrappleArm.hpp"

#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CStateManager.hpp"

void CGrappleArm::PointGenerator(const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                                 void* context) {
  if (context) {
    static_cast< CRainSplashGenerator* >(context)->GeneratePoints(model, workspace);
  }
}

void CGrappleArm::Render(const CStateManager& mgr, const CVector3f& pos, const CModelFlags& flags,
                         const CActorLights* lights) const {
  if (mStateFlags == 0) {
    return;
  }
  const CTransform4f xf = CTransform4f::Translate(pos) * mTransform * mAuxTransform;
  const CModelFlags armFlags = flags.UseShaderSet(mgr.MaskUIdNumPlayers(mPlayerId));
  if (mRainSplashGenerator.get() && mRainSplashGenerator->IsRaining()) {
    CSkinnedModel::SetPointGeneratorFunc(mRainSplashGenerator.get(), PointGenerator);
  }
  mArmModel->Render(mgr, xf, lights, armFlags);
  if (mRainSplashGenerator.get() && mRainSplashGenerator->IsRaining()) {
    CSkinnedModel::ClearPointGeneratorFunc();
    mRainSplashGenerator->Draw(xf);
  }
  if (!mGrappleGearModel.IsNull()) {
    mGrappleGearModel.Render(mgr, xf * mGrappleLocatorXf, lights, flags);
  }
}
