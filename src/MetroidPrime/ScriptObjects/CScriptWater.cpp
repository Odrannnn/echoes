#include "MetroidPrime/ScriptObjects/CScriptWater.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CFluidPlaneCPU.hpp"
#include "MetroidPrime/TCastTo.hpp"

const float CScriptWater::kSplashScales[6] = {1.f, 3.f, 0.71f, 1.19f, 0.71f, 1.f};

CScriptWater::CScriptWater(
    CStateManager& mgr, TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
    const CVector3f& position, const CAABox& bounds, const CDamageInfo& damage,
    const CVector3f& forceField, uint triggerFlags, float alphaInTime, float alphaOutTime,
    float morphInTime, float morphOutTime, int fluidType, CAssetId lightMap, CAssetId colorMap,
    const CColor& baseColor, CAssetId colorWarpMap, CAssetId glossMap, CAssetId envMap,
    float envMapSize, CAssetId texture, CAssetId foamMap, CAssetId alphaMap, float alpha,
    float glossFlat, float unknown1, float unknown2, float unknown3, const CFluidUVMotion& uvMotion,
    const CColor& splashColor, const CColor& insideFogColor, CAssetId splashParticle1,
    CAssetId splashParticle2, CAssetId splashParticle3, CAssetId visorRunoffParticle,
    CAssetId unmorphVisorRunoffParticle, TSfxId visorRunoffSfx, TSfxId unmorphVisorRunoffSfx,
    TSfxId splashSfx1, TSfxId splashSfx2, TSfxId splashSfx3, const CColor& fogColor, float fogBias,
    float fogMagnitude, float fogSpeed, float viscosity, bool displaySurface, float unknownScale,
    const CVector2f& uvScale, const CVector2f& uvOffset, const CVector2f& surfaceScale,
    bool useDynamicLights, float unknown4, float unknown5, float unknown6, float unknown7,
    bool occlusion, bool filterSoundEffects, int unknown8)
: CScriptTrigger(uid, name, info, position, bounds, damage, forceField,
                 triggerFlags | kTFL_BlockEnvironmentalEffects, false, false)
, mFluidPlane(nullptr)
, mPositionMorphed(position)
, mExtentMorphed(bounds.GetWidth(), bounds.GetHeight(), bounds.GetDepth())
, mMorphInTime(morphInTime)
, mPositionOrig(position)
, mExtentOrig(mExtentMorphed)
, mDamageOrig(damage.GetDamage())
, mDamageMorphed(damage.GetDamage())
, mMorphOutTime(morphOutTime)
, mMorphFactor(0.f)
, mSurfaceBounds(CAABox::MakeMaxInvertedBox())
, mFogBias(fogBias)
, mFogMagnitude(fogMagnitude)
, mOrigFogBias(fogBias)
, mOrigFogMagnitude(fogMagnitude)
, mFogSpeed(fogSpeed)
, mFogColor(fogColor)
, mSplashParticle1Id(splashParticle1)
, mSplashParticle2Id(splashParticle2)
, mSplashParticle3Id(splashParticle3)
, mVisorRunoffParticleId(visorRunoffParticle)
, mUnmorphVisorRunoffParticleId(unmorphVisorRunoffParticle)
, mVisorRunoffSfx(visorRunoffSfx)
, mUnmorphVisorRunoffSfx(unmorphVisorRunoffSfx)
, mSplashColor(splashColor)
, mInsideFogColor(insideFogColor)
, mAlphaInTime(alphaInTime)
, mAlphaOutTime(alphaOutTime)
, mAlphaInRecip(alphaInTime ? 1.f / alphaInTime : 0.f)
, mAlphaOutRecip(alphaOutTime ? 1.f / alphaOutTime : 0.f)
, mAlpha(alpha)
, mGridDimX(
      static_cast< int >(CMath::FloorF((3.f + GetTriggerBoundsWR().GetWidth() - 0.01f) / 3.f)))
, mGridDimY(
      static_cast< int >(CMath::FloorF((3.f + GetTriggerBoundsWR().GetHeight() - 0.01f) / 3.f)))
, mGridCellCount((mGridDimX + 1) * (mGridDimY + 1))
, mPatchDimX(0)
, mPatchDimY(0)
, mTileIntersects(nullptr)
, mVertIntersects(nullptr)
, mPatchIntersects(nullptr)
, mComputedGridCellCount(0)
, x310_(unknown4)
, x314_(unknown5)
, x318_(unknown6)
, x31c_(unknown7)
, mSurfaceScale(surfaceScale * 3.f)
, x328_(unknown8)
, mMorphIn(false)
, mMorphing(false)
, mAllowRender(displaySurface)
, mRecomputeClipping(true)
, mAlphaIn(false)
, mAlphaOut(false)
, x32c_6_(occlusion)
, x32c_7_(filterSoundEffects) {
  mFluidPlane = rs_new CFluidPlaneCPU(
      GetFluidUVExtent(bounds), colorMap, baseColor, colorWarpMap, glossMap, lightMap, envMap,
      texture, useDynamicLights, fluidType, uvMotion, uvScale, uvOffset, unknownScale, alpha,
      glossFlat, unknown1, unknown2, unknown3, envMapSize, viscosity);

  for (int i = 0; i < 3; ++i) {
    mSplashEffects.push_back(rstl::optional_object< TLockedToken< CGenDescription > >());
  }
  mSplashSounds.push_back(splashSfx1);
  mSplashSounds.push_back(splashSfx2);
  mSplashSounds.push_back(splashSfx3);

  // TODO: acquire the splash/runoff particle resources and configure actor lighting.
  CalculateRenderBounds();
  if (!GetActive()) {
    mAlpha = 0.f;
    mFogBias = 0.f;
    mFogMagnitude = 0.f;
  }
  SetupGrid(true);
}

CScriptWater::~CScriptWater() {}

void CScriptWater::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: recover morph targets, alpha transitions and the connected collision actor.
  CScriptTrigger::AcceptScriptMsg(mgr, msg);
}

const CScriptWater* CScriptWater::GetNextConnectedWater(const CStateManager& mgr) const {
  // Retail spells the emptiness test `first != second` here, where CEntity's connection loops
  // spell it `!(first == second)`; the two spellings give opposite branch polarity.
  rstl::vector< SConnection >::const_iterator conn = GetConnectionList().begin();
  for (; conn != GetConnectionList().end(); ++conn) {
    if (conn->state == kSS_Play && conn->msg == kSM_Activate) {
      const CStateManager::TIdListResult search = mgr.GetIdListForScript(conn->objId);
      if (search.first != search.second) {
        // Retail tests the cast result for null and keeps looping when it is null; returning it
        // outright drops that `cmplwi`/`beq` pair.
        if (const CScriptWater* water =
                TCastToConstPtr< CScriptWater >(mgr.GetObjectById(search.first->second))) {
          return water;
        }
      }
    }
  }
  return nullptr;
}

void CScriptWater::Touch(CActor& actor, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }
  // Retail reads the actor's fluid count into a bool before calling the base, and re-tests it
  // after the push_back: the actor must have been in no fluid at all for the surface test.
  const bool wasInFluid = !!actor.GetFluidCount();
  CScriptTrigger::Touch(actor, mgr);
  if (actor.GetMaterialList().HasMaterial(kMT_Trigger)) {
    return;
  }
  for (rstl::list< rstl::pair< TUniqueId, bool > >::iterator it = mWaterInhabitants.begin();
       it != mWaterInhabitants.end(); ++it) {
    if ((*it).first == actor.GetUniqueId()) {
      (*it).second = true;
      return;
    }
  }
  const rstl::optional_object< CAABox >& touchBounds = actor.GetTouchBounds();
  if (!touchBounds) {
    return;
  }
  mWaterInhabitants.push_back(rstl::pair< TUniqueId, bool >(actor.GetUniqueId(), false));
  if (wasInFluid) {
    return;
  }
  const CAABox surfaceBounds = GetTriggerBoundsWR();
  const float surfaceZ = surfaceBounds.GetMaxPoint().GetZ();
  if (touchBounds->GetMinPoint().GetZ() <= surfaceZ &&
      touchBounds->GetMaxPoint().GetZ() >= surfaceZ) {
    actor.FluidFXThink(kFS_EnteredFluid, *this, mgr);
  }
}

void CScriptWater::UpdateSplashInhabitants(CStateManager& mgr) {
  for (rstl::list< rstl::pair< TUniqueId, bool > >::node* it = mWaterInhabitants.begin().get_node();
       it != mWaterInhabitants.end().get_node();) {
    // Retail reads each node's successor into a register before the body and uses that after
    // the erase, rather than reloading it in the loop increment.
    rstl::list< rstl::pair< TUniqueId, bool > >::node* next = it->mNext;
    CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(it->get_value()->first));
    bool crossedSurface = false;
    if (actor) {
      const rstl::optional_object< CAABox > touchBounds = actor->GetTouchBounds();
      if (touchBounds) {
        const CAABox surfaceBounds = GetTriggerBoundsWR();
        const float surfaceZ = surfaceBounds.GetMaxPoint().GetZ();
        if (touchBounds->GetMinPoint().GetZ() <= surfaceZ &&
            touchBounds->GetMaxPoint().GetZ() >= surfaceZ) {
          crossedSurface = true;
        }
      }
    }
    if (actor && it->get_value()->second) {
      // Already in the water: a crossing this frame means it went under.
      if (crossedSurface) {
        actor->FluidFXThink(kFS_InFluid, *this, mgr);
      }
      it->get_value()->second = false;
      continue;
    }
    // Not in the water: drop the entry, and if it was the only fluid it left, tell it.
    mWaterInhabitants.do_erase(it);
    it = next;
    if (actor && actor->GetFluidCount() == 0 && crossedSurface) {
      actor->FluidFXThink(kFS_LeftFluid, *this, mgr);
    }
  }
}

void CScriptWater::ClearSplashInhabitants() {
  // Retail walks the nodes itself, reading each node's successor before erasing it and keeping
  // that successor, rather than using rstl::list::clear()'s out-of-line range erase.
  rstl::list< rstl::pair< TUniqueId, bool > >::node* it = mWaterInhabitants.begin().get_node();
  while (it != mWaterInhabitants.end().get_node()) {
    rstl::list< rstl::pair< TUniqueId, bool > >::node* next = it->mNext;
    mWaterInhabitants.do_erase(it);
    it = next;
  }
}

void CScriptWater::Think(float dt, CStateManager& mgr) {
  if (GetActive()) {
    CScriptTrigger::Think(dt, mgr);
    UpdateSplashInhabitants(mgr);
    // TODO: advance alpha/fog fading and morph position, bounds and damage.
    SetupGridClipping(mgr, 4);
  }
}

void CScriptWater::CalculateRenderBounds() {
  const CVector3f& min = mBounds.GetMinPoint();
  const CVector3f& max = mBounds.GetMaxPoint();
  mSurfaceBounds = CAABox(CVector3f(min.GetX(), min.GetY(), max.GetZ() - 1.f) + GetTranslation(),
                          CVector3f(max.GetX(), max.GetY(), max.GetZ() + 1.f) + GetTranslation());
}

void CScriptWater::PreRenderAllViewports(CStateManager& mgr) {
  // TODO: update fog-volume render bounds and per-viewport occlusion planes.
}

CAABox CScriptWater::GetSortingBounds(const CStateManager&) const {
  // Retail lifts mSurfaceBounds into a named reference first: the ctor then takes the min point
  // by address and the adjusted max point by address, with no second reload of the box.
  const CAABox& bounds = mSurfaceBounds;
  CVector3f maxPoint = bounds.GetMaxPoint();
  const float fogZ = maxPoint.GetZ() - 1.f;
  if (fogZ > maxPoint.GetZ()) {
    maxPoint[kDZ] = fogZ;
  }
  return CAABox(bounds.GetMinPoint(), maxPoint);
}

void CScriptWater::AddToRenderer(const CStateManager& mgr) const {
  if (GetPreRenderClipped()) {
    return;
  }
  // Retail routes fluid type 2 (the enum's own value, not a named constant here) straight to a
  // virtual Render() call and submits nothing itself. Which fluid that is is unresolved.
  if (mFluidPlane->GetFluidType() == 2) {
    Render(mgr);
    return;
  }
  const float transZ = GetTranslation().GetZ();
  const float boundsMaxZ = mBounds.GetMaxPoint().GetZ();
  // The kN_Yes ctor's own Normalize() call is the out-of-line one retail emits here, into the
  // temp it is constructing - so the temp must be the unit vector literal itself.
  mgr.AddDrawableActorPlane(
      *this,
      CPlane(boundsMaxZ + transZ,
             CUnitVector3f(CVector3f(0.f, 0.f, 1.f), CUnitVector3f::kN_Yes)),
      GetSortingBounds(mgr));
}

void CScriptWater::PreRender(CStateManager& mgr) {
  // TODO: establish visibility, update lights and prepare the fluid plane's UV extent.
}

void CScriptWater::Render(const CStateManager& mgr) const {
  // TODO: render the scaled fluid surface and the visor-dependent fog volume.
}

CVector2f CScriptWater::GetFluidUVExtent(const CAABox& bounds) const {
  return CVector2f(bounds.GetWidth() / mSurfaceScale.GetX(),
                   bounds.GetHeight() / mSurfaceScale.GetY());
}

int CScriptWater::GetSplashIndex(float scale) const {
  // Retail folds the 3.f multiply into the float argument, truncates with fctiwz, and tests
  // `>= 3` (cmpwi r3,3 / blt) rather than the `> 2` our old spelling produced (cmpwi r3,2 / ble).
  scale *= 3.f;
  int idx = static_cast< int >(scale);
  if (idx >= 3) {
    idx -= 1;
  }
  return idx;
}

const rstl::optional_object< TLockedToken< CGenDescription > >&
CScriptWater::GetSplashEffect(float scale) const {
  return mSplashEffects[GetSplashIndex(scale)];
}

TSfxId CScriptWater::GetSplashSound(float scale) const {
  return mSplashSounds[GetSplashIndex(scale)];
}

float CScriptWater::GetSplashEffectScale(float scale) const {
  if (close_enough(scale, 1.f)) {
    return kSplashScales[5];
  }
  const int index = GetSplashIndex(scale);
  scale *= 3.f;
  // Retail calls the C library `floor` (the double one) here and rounds with `frsp`; there is no
  // CMath::FloorF definition anywhere in the tree, and using it emitted the wrong call.
  scale = scale - static_cast< float >(floor(scale));
  return (1.f - scale) * kSplashScales[index * 2] + scale * kSplashScales[index * 2 + 1];
}

EWeaponCollisionResponseTypes CScriptWater::GetCollisionResponseType(const CVector3f&,
                                                                     const CVector3f&,
                                                                     const CWeaponMode&,
                                                                     int) const {
  return kWCR_Water;
}

void CScriptWater::SetMorphing(const bool m) {
  // Naming the parameter `m` and comparing `m != mMorphing` (not the reverse) is what keeps
  // CodeWarrior in r4 instead of copying the bool to r6; see notes for the scores.
  if (m != mMorphing) {
    mMorphing = m;
    SetupGrid(!m);
  }
}

void CScriptWater::SetupGridClipping(CStateManager& mgr, int computeVerts) {
  // TODO: incrementally ray-test vertices and derive tile/patch coverage flags.
}

void CScriptWater::SetupGrid(bool recomputeClipping) {
  // TODO: resize and initialize the grid buffers when the surface dimensions change.
}

bool CScriptWater::CanRippleAtPoint(const CVector3f& point) const {
  // Retail re-reads the trigger bounds once per axis: two GetTriggerBoundsWR() calls, each
  // returning into its own stack slot, with no CSE between them.
  if (mTileIntersects.null()) {
    return true;
  }
  const int x = static_cast< int >((point.GetX() - GetTriggerBoundsWR().GetMinPoint().GetX()) / 3.f);
  if (x < 0 || x >= mGridDimX) {
    return false;
  }
  const int y = static_cast< int >((point.GetY() - GetTriggerBoundsWR().GetMinPoint().GetY()) / 3.f);
  if (y < 0 || y >= mGridDimY) {
    return false;
  }
  return mTileIntersects.get()[x + y * mGridDimX] != 0;
}

void CScriptWater::InhabitantAdded(CActor& actor, CStateManager& mgr) {
  // TODO: update the actor's fluid count and send entry messages/camera callbacks.
}

void CScriptWater::InhabitantExited(CActor& actor, CStateManager& mgr) {
  // TODO: update the actor's fluid count and send exit messages/camera callbacks.
}

void CScriptWater::InhabitantIdle(CActor& actor, CStateManager& mgr) {
  CScriptTrigger::InhabitantIdle(actor, mgr);
  mgr.SendScriptMsg(&actor, GetUniqueId(), kSM_XINF, kInvalidUniqueId);
}
