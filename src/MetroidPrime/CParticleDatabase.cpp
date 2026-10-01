#include "MetroidPrime/CParticleDatabase.hpp"

#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleGen.hpp"
#include "MetroidPrime/CParticleGenInfo.hpp"

#include "rstl/rc_ptr.hpp"

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

void CParticleDatabase::CacheParticleDesc(const CCharacterInfo::CParticleResData& data) {
  // TODO: correct CParticleResData's five resource lists before traversing them.
}

void CParticleDatabase::CacheParticleDesc(const SObjectTag& tag) {
  // TODO: cache the five supported resource description types.
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

