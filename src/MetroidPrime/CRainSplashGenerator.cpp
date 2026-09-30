#include "MetroidPrime/CRainSplashGenerator.hpp"

#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"

#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Animation/CSkinRules.hpp"
#include "Kyoto/Basics/CCast.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

#include "dolphin/gx/GXTev.h"
#include "dolphin/gx/GXVert.h"

const float CRainSplashGenerator::SSplashLine::skInitialSpeed = 4.f;
const float CRainSplashGenerator::SSplashLine::skInitialHeight = 0.015625f;
const uchar CRainSplashGenerator::SSplashLine::skInitialWidth = 3;

static const GXVtxDescList vtxDescv[] = {
  { GX_VA_POS, GX_DIRECT },
  { GX_VA_CLR0, GX_DIRECT },
  { GX_VA_NULL, GX_NONE },
};

// The three model-sampling functions below are the only ones in this unit that reach outside
// it, and the only ones the port build cannot have. `files.cmake` does not list
// `src/Kyoto/Animation/DolphinCSkinnedModel.cpp` (it pulls in `CModel`, `CCubeModel`,
// `CCubeMaterial` and `CFrameDelayedKiller`, none of them ported), so on the host
// `CSkinnedModel::GetSkinnedPosition` and `GetSkinnedNormal` have no definition at all: calling
// them from here adds two symbols to the port's link gap and removes none, which is the
// regression `tools/link_check.sh --strict` refuses. The same arrangement as
// `src/MetroidPrime/CMiscTableInit.cpp`: retail's body under `#ifndef TARGET_PC`, and a
// `TARGET_PC` branch that records the gap instead of hiding it. The guards are separate
// because the file's declaration order is reverse retail offset (mwcceppc emits definitions in
// reverse source order, so the order here is what puts retail's `.text` order back) and the
// constructor and `AddPoint` sit between the three sampling functions.
#ifndef TARGET_PC

int CRainSplashGenerator::GetNextBestPt(int pt, const CSkinnedModel& model,
                                        const SSkinningWorkspace& workspace, int count,
                                        CRandom16& rand, float minZ) {
  int nextPt = pt;
  float maxDist = 0.f;
  const CVector3f refVert = model.GetSkinnedPosition(workspace, pt);
  for (int i = 0; i < 3; ++i) {
    const int idx = rand.Range(0, count - 1);
    const CVector3f vert = model.GetSkinnedPosition(workspace, idx);

    const CVector3f& delta = refVert - vert;
    const float distSq = delta.MagSquared();
    const CVector3f norm = model.GetSkinnedNormal(workspace, idx);
    const float normDot = CVector3f::Dot(norm, CVector3f::Up());

    const bool goodNorm = normDot >= 0.f && normDot <= 1.f;

    bool goodZ;
    if (minZ > 0.f) {
      goodZ = vert.GetZ() > minZ;
    } else {
      goodZ = true;
    }

    if (distSq > maxDist && goodNorm && goodZ) {
      nextPt = idx;
      maxDist = distSq;
    }
  }
  return nextPt;
}

#else // TARGET_PC

// Real behaviour gap, recorded rather than papered over: the port has no skinned model to
// sample, so it chooses no vertex. Nothing here reads or fakes a value the port has - the
// positions the GameCube build samples are simply not there to be read. Delete this branch when
// `DolphinCSkinnedModel.cpp` is listed in `files.cmake`, or the link sees two bodies.
int CRainSplashGenerator::GetNextBestPt(int point, const CSkinnedModel&, const SSkinningWorkspace&,
                                        int, CRandom16&, float) {
  return point;
}

#endif // TARGET_PC

CRainSplashGenerator::CRainSplashGenerator(const CVector3f& scale, int maxSplashes, int genRate,
                                           float minZ, float alpha)
: mScale(scale)
, mGenerateTimer(0.f)
, mDt(0.f)
, mMinZ(minZ)
, mAlpha(alpha > 1.f ? 255.f : alpha * 255.f)
, mCurrentPoint(0)
, mQueueTail(0)
, mQueueHead(0)
, mQueueSize(0)
, mGenerationRate(genRate > maxSplashes ? maxSplashes : genRate)
, x48_24_(false)
, mRaining(true)
, mForceRaining(false) {
  mRainSplashes.reserve(maxSplashes);
  for (int i = 0; i < maxSplashes; ++i) {
    mRainSplashes.push_back(SRainSplash());
  }
}

void CRainSplashGenerator::AddPoint(const CVector3f& pos) {
  if (mQueueTail >= mRainSplashes.size()) {
    mQueueTail = 0;
  }
  mRainSplashes[mQueueTail].SetPoint(pos);
  mQueueSize += 1;
  mQueueTail += 1;
}

#ifndef TARGET_PC

void CRainSplashGenerator::GeneratePoints(const CSkinnedModel& model,
                                          const SSkinningWorkspace& workspace) {
  const int count = model.GetSkinRules()->GetNumPoints();
  if (!mRaining) {
    return;
  }

  if (mGenerateTimer > mGenerateInterval) {
    int pt = mCurrentPoint;
    for (int i = 0; i < mGenerationRate; ++i) {
      if (mQueueSize >= mRainSplashes.size()) {
        break;
      }
      const int nextPt = GetNextBestPt(pt, model, workspace, count, mRandom, mMinZ);
      AddPoint(CVector3f::ByElementMultiply(mScale, model.GetSkinnedPosition(workspace, nextPt)));
      pt = nextPt;
    }
    mCurrentPoint = pt;
    mGenerateTimer = 0.f;
  }
}

CVector3f CRainSplashGenerator::GeneratePoint(const CSkinnedModel& model,
                                              const SSkinningWorkspace& workspace) {
  const int count = model.GetSkinRules()->GetNumPoints();
  mCurrentPoint = GetNextBestPt(mCurrentPoint, model, workspace, count, mRandom, mMinZ);
  return CVector3f::ByElementMultiply(mScale, model.GetSkinnedPosition(workspace, mCurrentPoint));
}

#else // TARGET_PC

// The other half of the gap recorded above: the port generates no rain-splash points and
// returns no sampled point, because the skinned model it would sample is not built. Delete this
// branch when `DolphinCSkinnedModel.cpp` is listed in `files.cmake`.
void CRainSplashGenerator::GeneratePoints(const CSkinnedModel&, const SSkinningWorkspace&) {}

CVector3f CRainSplashGenerator::GeneratePoint(const CSkinnedModel&, const SSkinningWorkspace&) {
  return CVector3f::Zero();
}

#endif // TARGET_PC

void CRainSplashGenerator::UpdateRainSplashRange(CStateManager& mgr, const int start, const int end,
                                                 float dt) {
  for (int i = start; i < end; ++i) {
    SRainSplash& set = mRainSplashes[i];
    set.Update(dt, mgr);
    if (!set.IsActive()) {
      mQueueSize -= 1;
      mQueueHead += 1;
      if (mQueueHead >= mRainSplashes.size()) {
        mQueueHead = 0;
      }
    }
  }
}

void CRainSplashGenerator::UpdateRainSplashes(CStateManager& mgr, float magnitude, float dt) {
  mGenerateTimer += dt;
  mGenerateInterval = 1.f / (70.f * magnitude);
  if (mQueueSize > 0) {
    if (mQueueTail <= mQueueHead) {
      UpdateRainSplashRange(mgr, mQueueHead, static_cast< int >(mRainSplashes.size()), dt);
      UpdateRainSplashRange(mgr, 0, mQueueTail, dt);
    } else {
      UpdateRainSplashRange(mgr, mQueueHead, mQueueTail, dt);
    }
  }
}

void CRainSplashGenerator::Update(float dt, CStateManager& mgr) {
  mDt = dt;
  bool raining = mForceRaining;
  float magnitude = 10.f;
  if (!raining) {
    const CEnvFxManager& envFx = *mgr.GetEnvFxManager();
    const int neededFx = mgr.GetWorld()->GetNeededEnvFx();
    if (neededFx != kEFX_None && envFx.IsSplashActive()) {
      if (envFx.GetRainMagnitude()) {
        switch (neededFx) {
        case kEFX_Rain:
          raining = true;
          magnitude = envFx.GetRainMagnitude();
          break;
        default:
          break;
        }
      }
    }
  }

  if (raining) {
    UpdateRainSplashes(mgr, magnitude, dt);
    mRaining = true;
  } else {
    mRaining = false;
  }
}

void CRainSplashGenerator::Draw(const CTransform4f& xf) const {
  if (!mRaining) {
    return;
  }

  DoDraw(xf);
}

void CRainSplashGenerator::DoDraw(const CTransform4f& xf) const {
  if (mDt <= 0.0f) {
    return;
  }

  CGX::SetVtxDescv(vtxDescv);
  CGX::SetNumChans(1);
  CGX::SetNumTevStages(1);
  CGX::SetChanCtrl(CGX::Channel0, false, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                   GX_AF_NONE);
  CGX::SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
  CGX::SetNumTexGens(0);
  CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  CGX::SetZMode(true, GX_LEQUAL, false);
  CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvPassthru);
  CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
  CGraphics::SetModelMatrix(xf);
  if (mQueueSize > 0) {
    if (mQueueTail <= mQueueHead) {
      for (int i = mQueueHead; i < mRainSplashes.size(); ++i) {
        const SRainSplash& splash = mRainSplashes[i];
        splash.Draw(mAlpha, mDt, splash.mPosition);
      }
      for (int i = 0; i < mQueueTail; ++i) {
        const SRainSplash& splash = mRainSplashes[i];
        splash.Draw(mAlpha, mDt, splash.mPosition);
      }
    } else {
      for (int i = mQueueHead; i < mQueueTail; ++i) {
        const SRainSplash& splash = mRainSplashes[i];
        splash.Draw(mAlpha, mDt, splash.mPosition);
      }
    }
  }
  CGX::SetLineWidth(6, GX_TO_ZERO);
}

void CRainSplashGenerator::SSplashLine::SetActive() { mActive = true; }

void CRainSplashGenerator::SSplashLine::Update(float dt, CStateManager& mgr) {
  if (!mActive) {
    return;
  }
  if (mTime <= 0.8f) {
    mLineWidth = CCast::ToUint8(5.f * (1.f - mTime) + 3.f * mTime);
    mTime += dt * mSpeed;
  } else if (mLength != 0) {
    mLength -= 1;
  } else {
    mActive = false;
    mTime = 0.f;
    mSpeed = mgr.Random()->Range(4.0f, 8.0f);
    mParabolaHeight = mgr.Random()->Range(0.015625f, 0.03125f);
    mEndX = mgr.Random()->Range(-0.125f, 0.125f);
    mEndY = mgr.Random()->Range(-0.125f, 0.125f);
    mLength = static_cast< uchar >(mgr.Random()->Range(1, 2));
  }
}

void CRainSplashGenerator::SSplashLine::Draw(float alpha, float dt, const CVector3f& pos) const {
  if (mTime > 0.f) {
    const float delta = dt * mSpeed;
    const float trail = delta * mSpeed;
    float vt = mTime - trail;
    if (vt < 0.0f) {
      vt = 0.0f;
    }
    int vertCount = static_cast< int >((mTime - vt) / delta + 1.f);

    CGX::SetLineWidth(static_cast< uchar >(mLineWidth * 6), GX_TO_ZERO);
    CGX::Begin(GX_LINESTRIP, GX_VTXFMT0, vertCount);

    for (int i = 0; i < vertCount; ++i) {
      const float height = -4.f * vt * (vt - 1.f) * mParabolaHeight;
      GXPosition3f32(vt * mEndX + pos.GetX(), vt * mEndY + pos.GetY(), height + pos.GetZ());
      GXColor1u32(static_cast< uint >(vt * alpha) | 0xffffff00);
      vt += delta;
    }
    CGX::End();
  }
}

CRainSplashGenerator::SRainSplash::SRainSplash()
: mLines(4, SSplashLine()), mPosition(CVector3f::Zero()), x70_(0.f) {}

void CRainSplashGenerator::SRainSplash::Update(float dt, CStateManager& mgr) {
  for (SSplashLine* it = mLines.begin(); it != mLines.end(); ++it) {
    it->Update(dt, mgr);
  }
}

void CRainSplashGenerator::SRainSplash::Draw(float alpha, float dt,
                                             const CVector3f& position) const {
  for (const SSplashLine* it = mLines.begin(); it != mLines.end(); ++it) {
    it->Draw(alpha, dt, position);
  }
}

const bool CRainSplashGenerator::SRainSplash::IsActive() const {
  bool ret = false;
  for (const SSplashLine* it = mLines.begin(); it != mLines.end(); ++it) {
    ret |= it->mActive;
  }
  return ret;
}

void CRainSplashGenerator::SRainSplash::SetPoint(const CVector3f& position) {
  for (SSplashLine* it = mLines.begin(); it != mLines.end(); ++it) {
    it->SetActive();
  }
  mPosition = position;
}
