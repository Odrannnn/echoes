#ifndef _SMODELRENDERDATA
#define _SMODELRENDERDATA

#include "types.h"

class CModel;
class CSkinnedModel;
struct SSkinningWorkspace;
class CPoseAsTransforms_Linear;

// Guessed name. Shared render input for a static or skinned model.
struct SModelRenderData {
  explicit SModelRenderData(const CModel& model)
  : mModel(&model), mSkinnedModel(nullptr), mWorkspace(nullptr), mPose(nullptr) {}

  // The skinned arm. Retail builds this on the stack as four consecutive words
  // `{ nullptr, &skinned, nullptr, &mAnimData->mPose() }` - see `CModelData::DisintegrateDraw`
  // at 0x800E631C, whose fourth word is `mAnimData + 0x2B0`, `CAnimData::mPose` by
  // `CHECK_SIZEOF(CAnimData, 0x5b8)`.
  SModelRenderData(const CSkinnedModel& skinnedModel, const CPoseAsTransforms_Linear* pose)
  : mModel(nullptr), mSkinnedModel(&skinnedModel), mWorkspace(nullptr), mPose(pose) {}

  const CModel* mModel;
  const CSkinnedModel* mSkinnedModel;
  const SSkinningWorkspace* mWorkspace;
  const CPoseAsTransforms_Linear* mPose;
};
CHECK_SIZEOF(SModelRenderData, 0x10)

#endif // _SMODELRENDERDATA
