#include "MetroidPrime/Player/CMorphBallShadow.hpp"

#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"

#include "MetaRender/CCubeRenderer.hpp"

CMorphBallShadow::CMorphBallShadow(int width, int height, const TToken< CTexture >& ballFade)
: mTexture(kTF_I8, width, height, 1)
, mBallFade(ballFade)
, mWidth(width)
, mHeight(height)
, mShadowVolume(CAABox::MakeMaxInvertedBox())
, mHasIds(false) {
  mBallFade.Lock();
}

CMorphBallShadow::~CMorphBallShadow() { mTexture.ScheduleDeletion(); }

void CMorphBallShadow::RenderIdBuffer(const CAABox& aabb, CStateManager& mgr, CPlayer& player) {
  mShadowVolume = aabb;
  mActors.clear();
  mAreas.clear();
  mWorldModelBits = rstl::vector< uint >();

  gpRender->SetRequestRGBA6(true);
  if (!gpRender->IsRGBA6Current()) {
    mHasIds = false;
    return;
  }

  GatherAreas(mgr);
  // TODO: Gather eligible actors, render receiver IDs, then copy the alpha texture.
  mHasIds = false;
}

void CMorphBallShadow::Render(CStateManager& mgr, float alpha, const CTexture& shadowTexture) {
  if (!mHasIds || !AreasValid(mgr)) {
    return;
  }

  // TODO: Project receiver IDs using shadowTexture, optional ball fade, and actor/world geometry.
}

void CMorphBallShadow::GatherAreas(CStateManager& mgr) {
  mAreas.clear();
  for (CGameArea::CConstChainIterator it = mgr.GetWorld()->GetChainHead(CWorld::kC_Alive);
       it != CWorld::skGlobalEnd; ++it) {
    if (it->GetOcclusionState() == CGameArea::kOS_Visible) {
      mAreas.push_back(it->GetId());
    }
  }
}

bool CMorphBallShadow::AreasValid(const CStateManager& mgr) const {
  rstl::list< TAreaId >::const_iterator area = mAreas.begin();
  for (CGameArea::CConstChainIterator it = mgr.GetWorld()->GetChainHead(CWorld::kC_Alive);
       it != CWorld::skGlobalEnd; ++it) {
    if (it->GetOcclusionState() == CGameArea::kOS_Visible) {
      if (area == mAreas.end()) {
        return false;
      }
      if (*area != it->GetId()) {
        return false;
      }
      ++area;
    }
  }
  return true;
}
