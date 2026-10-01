#include "MetroidPrime/CParticleDatabase.hpp"

#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleGen.hpp"
#include "MetroidPrime/CParticleGenInfo.hpp"

#include "rstl/rc_ptr.hpp"
#include "rstl/vector.hpp"

#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/SObjectTag.hpp"

/**
 * A same-layout view of the one derived member `SetParticleExternalParam` needs.
 *
 * `GetParticleEffect` hands this unit a `CParticleGenInfo*`, but retail reads the generation
 * system's two words at **+0x78**: `fn_800A7A7C` copies `0x78`/`0x7C`, and
 * `CParticleGenInfoGeneric::~CParticleGenInfoGeneric` (0x800A6038) releases `this+0x78` through
 * `fn_800A6444` and reads its first word as a vtable pointer
 * (`CParticleGenInfoGeneric::IsSystemDeletable`, 0x800A5A34, calls through it). That is
 * `CParticleGenInfoGeneric::mSystem`, a private `rstl::ncrc_ptr<CParticleGen>` which the class's
 * own `GetParticleSystem()` returns by value, so there is no reference to take here. `rstl::rc_ptr`
 * keeps `mPtr` at +0 and `mRefCount` at +4, which is what `rstl::CRcPtrData` is a view of; the
 * view is eight bytes and changes no layout. See `include/rstl/rc_ptr.hpp`.
 */
struct SGenericParticleGenInfo {
  char mBase[0x78];
  rstl::CRcPtrData mSystem;
};

/** 0x800A6444, retail's lowest function in the unit, so defined at the end of this file. */
extern "C" void fn_800A6444(rstl::CRcPtrData* self);
/** 0x800A7A7C, immediately below `SetParticleExternalParam` in retail, so defined after it. */
extern "C" void fn_800A7A7C(rstl::CRcPtrData* dest, CParticleGenInfo* src);

CParticleDatabase::CParticleDatabase() : mUpdatesEnabled(true), mAnySystemsDrawnWithModel(false) {}

CParticleDatabase::~CParticleDatabase() {}

/**
 * The ten list-walkers the two `CacheParticleDesc` overloads dispatch to.
 *
 * Retail's `CacheParticleDesc(const CParticleResData&)` (0x800A947C, 112 B) is five tail calls -
 * nothing else - and `CacheParticleDesc(const SObjectTag&)` (0x800A93A8, 212 B) is a binary search
 * on the tag's four-character code followed by one of five more. The ten callees are
 * 0x800A9C50/0x800A9DE8/0x800A9F80/0x800AA118/0x800AA2B0 (408 B each, the list form, one per
 * description type) and 0x800AA448/0x800AA5C8/0x800AA748/0x800AA8C8/0x800AAA48 (384 B each, the
 * single-id form). Ten near-identical bodies exist because each is a separate copy of one
 * template, and the linker kept them local, so the map gives none of them a mangled name.
 *
 * The bodies are this: for every id in the list, if the description map has no entry for it, make
 * one out of `gpSimplePool->GetObj(SObjectTag(<type>, id))`. `TDesc` is only ever used as a
 * pointer type - `TLockedToken<T>` holds a `CToken` and a `T*` - so the two guessed description
 * classes (`CParticleDescriptionSPSC`, `CParticleDescriptionSRSC`) stay forward declarations and
 * no layout is invented for them.
 */
template < typename TDesc, uint Type >
static void CacheParticleDescList(
    const rstl::vector< CAssetId >& ids,
    rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > >& descs) {
  // `rstl::map::value_type` is this pair; naming it directly keeps the dependent type out of the
  // expression, which MWCC and the host compiler spell differently.
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescValue;
  for (rstl::vector< CAssetId >::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    if (descs.find(*it) == descs.end()) {
      const CToken token = gpSimplePool->GetObj(SObjectTag(Type, *it));
      descs.insert(DescValue(*it, rstl::rc_ptr< TLockedToken< TDesc > >(
                                  rs_new TLockedToken< TDesc >(token))));
    }
  }
}

/** The single-id form, `CacheParticleDesc(const SObjectTag&)`'s five callees. */
template < typename TDesc, uint Type >
static void CacheParticleDescOne(
    CAssetId id, rstl::map< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > >& descs) {
  typedef rstl::pair< CAssetId, rstl::rc_ptr< TLockedToken< TDesc > > > DescValue;
  if (descs.find(id) == descs.end()) {
    const CToken token = gpSimplePool->GetObj(SObjectTag(Type, id));
    descs.insert(DescValue(id, rstl::rc_ptr< TLockedToken< TDesc > >(
                               rs_new TLockedToken< TDesc >(token))));
  }
}

void CParticleDatabase::CacheParticleDesc(const CCharacterInfo::CParticleResData& data) {
  CacheParticleDescList< CGenDescription, 'PART' >(data.GetPartIds(), mParticleDescs);
  CacheParticleDescList< CSwooshDescription, 'SWHC' >(data.GetSwhcIds(), mSwooshDescs);
  CacheParticleDescList< CElectricDescription, 'ELSC' >(data.GetElscAIds(), mElectricDescs);
  CacheParticleDescList< CParticleDescriptionSPSC, 'SPSC' >(data.GetSpscIds(), mSpscDescs);
  CacheParticleDescList< CParticleDescriptionSRSC, 'SRSC' >(data.GetSrscIds(), mSrscDescs);
}

void CParticleDatabase::CacheParticleDesc(const SObjectTag& tag) {
  // Retail reads both halves of the tag before the dispatch: `lwz r5,4(r4)` (the id) and
  // `mr r4,r3` (this) sit between the first `cmpw` and the first `beq`, so the five cases only
  // ever `mr r3,r5` and `addi r4,r4,<map offset>`.
  const CAssetId id = tag.GetId();
  switch (tag.GetType()) {
  case 'PART':
    CacheParticleDescOne< CGenDescription, 'PART' >(id, mParticleDescs);
    break;
  case 'SWHC':
    CacheParticleDescOne< CSwooshDescription, 'SWHC' >(id, mSwooshDescs);
    break;
  case 'ELSC':
    CacheParticleDescOne< CElectricDescription, 'ELSC' >(id, mElectricDescs);
    break;
  case 'SPSC':
    CacheParticleDescOne< CParticleDescriptionSPSC, 'SPSC' >(id, mSpscDescs);
    break;
  case 'SRSC':
    CacheParticleDescOne< CParticleDescriptionSRSC, 'SRSC' >(id, mSrscDescs);
    break;
  }
}

void CParticleDatabase::InsertParticleGen(bool oneShot, int flags, uint name,
                                          const rstl::auto_ptr< CParticleGenInfo >& gen) {
  DrawMap* map;
  if (oneShot) {
    switch (flags & 0x60) {
    case 0x20:
      map = &mFirstDraw;
      break;
    case 0x40:
      map = &mLastDraw;
      break;
    default:
      map = &mRendererDraw;
      break;
    }
  } else {
    switch (flags & 0x60) {
    case 0x20:
      map = &mFirstDrawLoop;
      break;
    case 0x40:
      map = &mLastDrawLoop;
      break;
    default:
      map = &mRendererDrawLoop;
      break;
    }
  }
  map->insert(DrawMap::value_type(name, gen));
  if (flags & 0x60)
    mAnySystemsDrawnWithModel = true;
}

void CParticleDatabase::AddParticleEffect(uint name, int flags, const CParticleData& data,
                                          const CVector3f& scale, CStateManager* mgr,
                                          TAreaId areaId, bool oneShot, uint lightId) {
  // TODO: cached PART/SWHC/ELSC/SPSC/SRSC construction and effect initialization.
}

void CParticleDatabase::AddParticleEffect(uint name, int flags, const CPositionalParticleData& data,
                                          const CVector3f& scale, CStateManager* mgr,
                                          TAreaId areaId, uint lightId) {
  // TODO: instantiate a positional PART effect from the cached description.
}

CParticleGenInfo* CParticleDatabase::GetParticleEffect(uint name) {
  {
    DrawMap::iterator it = mRendererDrawLoop.find(name);
    if (it != mRendererDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mFirstDrawLoop.find(name);
    if (it != mFirstDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mLastDrawLoop.find(name);
    if (it != mLastDrawLoop.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mRendererDraw.find(name);
    if (it != mRendererDraw.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mFirstDraw.find(name);
    if (it != mFirstDraw.end())
      return it->second.get();
  }
  {
    DrawMap::iterator it = mLastDraw.find(name);
    if (it != mLastDraw.end())
      return it->second.get();
  }
  return nullptr;
}

void CParticleDatabase::SetParticleEffectState(CParticleGenInfo* effect, bool active,
                                               CStateManager* mgr) {
  if (effect == nullptr)
    return;
  effect->SetParticleEmission(active, mgr);
  effect->SetIsActive(active);
  if (!active && (effect->GetFlags() & 1))
    effect->DestroyParticles();
  effect->SetIsGrabInitialData(true);
}

void CParticleDatabase::SetParticleEffectState(uint name, bool active, CStateManager* mgr) {
  SetParticleEffectState(GetParticleEffect(name), active, mgr);
}

void CParticleDatabase::SetParticleExternalParam(uint name, int index, float value) {
  CParticleGenInfo* effect = GetParticleEffect(name);
  if (effect == nullptr) {
    return;
  }
  // Retail's 0x800A7AA0: the effect's `mSystem` pair is copied out by an out-of-line call, its first
  // word is kept, the pair is released, and only then is the parameter set. `fn_800A7A7C` is the
  // out-of-line `rstl::rc_ptr` copy constructor (`rstl::CRcPtrData::CopyInto` is the same nine
  // instructions at 0x80049010, but a distinct symbol here) and `fn_800A6444` is its release.
  rstl::CRcPtrData system;
  fn_800A7A7C(&system, effect);
  CElementGen* gen = static_cast< CElementGen* >(system.x0_ptr);
  fn_800A6444(&system);
  gen->SetExternalParam(index, value);
}

/**
 * 0x800A7A7C, 36 bytes - the out-of-line `rstl::rc_ptr` copy constructor this unit calls.
 *
 * `rstl::CRcPtrData::CopyInto` (0x80049010, `src/rstl/rc_ptr_copy.cpp`) is retail's shared copy,
 * one for every `T`; this is the second, distinct symbol with the same nine instructions, reached
 * only from `SetParticleExternalParam` above. Same body, same ABI (r3 = destination, r4 = source),
 * for the reason in `docs/research/rc_ptr.md`: the words are not inside a template here.
 *
 * The `+0x78` is the source's own member offset, not the caller's: retail's caller passes the
 * effect pointer unchanged, so the two words are read as `0x78(src)`/`0x7C(src)` here.
 */
extern "C" void fn_800A7A7C(rstl::CRcPtrData* dest, CParticleGenInfo* src) {
  const SGenericParticleGenInfo& info = *reinterpret_cast< const SGenericParticleGenInfo* >(src);
  dest->x0_ptr = info.mSystem.x0_ptr;
  dest->x4_refCount = info.mSystem.x4_refCount;
  ++(*dest->x4_refCount);
}

void CParticleDatabase::Update(float dt, CAnimData& animData, const CCharLayoutInfo& layout,
                               const CTransform4f& xf, const CVector3f& scale, CStateManager* mgr) {
  if (!mUpdatesEnabled)
    return;
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mRendererDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mFirstDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mLastDrawLoop, true);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mRendererDraw, false);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mFirstDraw, false);
  UpdateParticleGenDB(dt, animData, layout, xf, scale, mgr, mLastDraw, false);
  mAnySystemsDrawnWithModel =
      mFirstDrawLoop.size() || mLastDrawLoop.size() || mFirstDraw.size() || mLastDraw.size();
}

void CParticleDatabase::UpdateParticleGenDB(float dt, CAnimData& animData,
                                            const CCharLayoutInfo& layout, const CTransform4f& xf,
                                            const CVector3f& scale, CStateManager* mgr,
                                            DrawMap& map, bool deleteIfDone) {
  // TODO: segment transforms, parenting, lifetime and deletion.
}

void CParticleDatabase::AddToRendererClipped(const CFrustumPlanes& frustum) const {
  AddToRendererClippedParticleGenMap(mRendererDraw, frustum);
  AddToRendererClippedParticleGenMap(mRendererDrawLoop, frustum);
}

void CParticleDatabase::RenderSystemsNormallyAddedToRenderer() const {
  RenderParticleGenMap(mRendererDraw);
  RenderParticleGenMap(mRendererDrawLoop);
}

void CParticleDatabase::AddToRendererClippedMasked(const CFrustumPlanes& frustum, uint mask,
                                                   uint target) const {
  AddToRendererClippedParticleGenMapMasked(mRendererDraw, frustum, mask, target);
  AddToRendererClippedParticleGenMapMasked(mRendererDrawLoop, frustum, mask, target);
}

void CParticleDatabase::AddToRendererClippedParticleGenMap(const DrawMap& map,
                                                           const CFrustumPlanes& frustum) const {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    CParticleGenInfo* const gen = it->second.get();
    if (frustum.BoxInFrustumPlanes(gen->GetBounds()) == true)
      gen->AddToRenderer();
  }
}

void CParticleDatabase::AddToRendererClippedParticleGenMapMasked(const DrawMap& map,
                                                                 const CFrustumPlanes& frustum,
                                                                 uint mask, uint target) const {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    CParticleGenInfo* const gen = it->second.get();
    if ((gen->GetFlags() & mask) == target &&
        frustum.BoxInFrustumPlanes(gen->GetBounds()) == true)
      gen->AddToRenderer();
  }
}

void CParticleDatabase::RenderSystemsToBeDrawnFirst() const {
  RenderParticleGenMap(mFirstDraw);
  RenderParticleGenMap(mFirstDrawLoop);
}

void CParticleDatabase::RenderSystemsToBeDrawnFirstPOICheck(uint mask, uint target) const {
  RenderParticleGenMapMasked(mFirstDraw, mask, target);
  RenderParticleGenMapMasked(mFirstDrawLoop, mask, target);
}

void CParticleDatabase::RenderSystemsToBeDrawnLast() const {
  RenderParticleGenMap(mLastDraw);
  RenderParticleGenMap(mLastDrawLoop);
}

void CParticleDatabase::RenderSystemsToBeDrawnLastPOICheck(uint mask, uint target) const {
  RenderParticleGenMapMasked(mLastDraw, mask, target);
  RenderParticleGenMapMasked(mLastDrawLoop, mask, target);
}

void CParticleDatabase::RenderParticleGenMap(const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    it->second->Render();
  }
}

void CParticleDatabase::RenderParticleGenMapMasked(const DrawMap& map, uint mask, uint target) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    if ((it->second->GetFlags() & mask) == target)
      it->second->Render();
  }
}

void CParticleDatabase::DeleteAllLights(CStateManager* mgr) {
  DeleteAllLightsForParticleDB(mgr, mRendererDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mFirstDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mLastDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mRendererDraw);
  DeleteAllLightsForParticleDB(mgr, mFirstDraw);
  DeleteAllLightsForParticleDB(mgr, mLastDraw);
}

void CParticleDatabase::DeleteAllLightsForParticleDB(CStateManager* mgr, const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    it->second->DeleteLight(mgr);
  }
}

void CParticleDatabase::SuspendAllActiveEffects(CStateManager* mgr) {
  SuspendAllActiveEffectsForParticleDB(mgr, mRendererDrawLoop);
  SuspendAllActiveEffectsForParticleDB(mgr, mFirstDrawLoop);
  SuspendAllActiveEffectsForParticleDB(mgr, mLastDrawLoop);
}

void CParticleDatabase::SuspendAllActiveEffectsForParticleDB(CStateManager* mgr,
                                                             const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    SetParticleEffectState(it->second.get(), false, mgr);
  }
}

void CParticleDatabase::SetModulationColorAllActiveEffects(const CColor& color) {
  SetModulationColorAllActiveEffectsForParticleDB(color, mRendererDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mFirstDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mLastDrawLoop);
  SetModulationColorAllActiveEffectsForParticleDB(color, mRendererDraw);
  SetModulationColorAllActiveEffectsForParticleDB(color, mFirstDraw);
  SetModulationColorAllActiveEffectsForParticleDB(color, mLastDraw);
}

void CParticleDatabase::SetModulationColorAllActiveEffectsForParticleDB(const CColor& color,
                                                                        const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    if (it->second.get())
      it->second->SetModulationColor(color);
  }
}

void CParticleDatabase::DestroyAllActiveParticles() {
  DestroyParticlesForParticleDB(mRendererDrawLoop);
  DestroyParticlesForParticleDB(mFirstDrawLoop);
  DestroyParticlesForParticleDB(mLastDrawLoop);
  DestroyParticlesForParticleDB(mRendererDraw);
  DestroyParticlesForParticleDB(mFirstDraw);
  DestroyParticlesForParticleDB(mLastDraw);
}

void CParticleDatabase::DestroyParticlesForParticleDB(const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); map.end() != it; ++it) {
    it->second->DestroyParticles();
  }
}

void CParticleDatabase::ClearAllNonPersistentEffects(CStateManager* mgr) {
  DeleteAllLightsForParticleDB(mgr, mRendererDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mFirstDrawLoop);
  DeleteAllLightsForParticleDB(mgr, mLastDrawLoop);
  mRendererDrawLoop.clear();
  mFirstDrawLoop.clear();
  mLastDrawLoop.clear();
}

void CParticleDatabase::AccumulateBounds(rstl::optional_object< CAABox >& bounds,
                                         const DrawMap& map) {
  for (DrawMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    rstl::optional_object< CAABox > partBounds = it->second->GetBounds();
    if (!partBounds)
      continue;
    if (!bounds) {
      bounds = partBounds;
    } else {
      bounds->AccumulateBounds(partBounds->GetMinPoint());
      bounds->AccumulateBounds(partBounds->GetMaxPoint());
    }
  }
}

rstl::optional_object< CAABox > CParticleDatabase::GetTotalBounds() const {
  rstl::optional_object< CAABox > bounds;
  AccumulateBounds(bounds, mFirstDrawLoop);
  AccumulateBounds(bounds, mLastDrawLoop);
  AccumulateBounds(bounds, mFirstDraw);
  AccumulateBounds(bounds, mLastDraw);
  return bounds;
}

/**
 * 0x800A6444, 100 bytes - `rstl::rc_ptr<CParticleGen>::ReleaseData()`.
 *
 * `if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }`, which is retail's own body in
 * `include/rstl/rc_ptr.hpp`. The 0x64 rather than the 0x50 of the 0x50-byte instantiations is
 * `CParticleGen`'s **virtual** destructor, so `delete` goes through the vtable
 * (`lwz r12,0(r3) / li r4,1 / lwz r12,8(r12) / mtctr r12 / bctrl`) behind a null check; a class
 * with a plain destructor gets the direct `bl ~D0` instead and is ten bytes shorter. The map gives
 * this one no mangled name, so the object has to define retail's own placeholder for objdiff to
 * pair it - the same reason `main.cpp` spells its three releases `fn_80009224`, `fn_80009008` and
 * `fn_800095E4`. It is reached from `CParticleGenInfoGeneric::~CParticleGenInfoGeneric` and from
 * every `rc_ptr` temporary in this unit's `AddParticleEffect` pair, one symbol for all of them.
 */
extern "C" void fn_800A6444(rstl::CRcPtrData* self) {
  if (--*self->x4_refCount <= 0) {
    delete static_cast< CParticleGen* >(self->x0_ptr);
    delete self->x4_refCount;
  }
}

