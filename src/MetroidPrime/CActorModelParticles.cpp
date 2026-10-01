#include "MetroidPrime/CActorModelParticles.hpp"

#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CDependencyGroup.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleElectric.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include "rstl/string.hpp"

static const char* const skParticleNames[] = {
    "Effect_OnFire",   "Effect_IceBreak", "Effect_Ash",       "Effect_FirePop",
    "Effect_Electric", "Effect_IcePop",   "Effect_Blackhole", "Effect_Imploder",
};

static bool IsMediumOrLarge(const CActor& actor) {
  if (const CPatterned* patterned = TCastToConstPtr< CPatterned >(&actor)) {
    // Retail reads `mCreatureSize` (0x358) directly and compares it against zero, not against a
    // named `kCS_Small`: the field is loaded with `lwz` and normalised with `neg/or/srwi`, so the
    // test is a plain `!= 0`.
    return patterned->GetCreatureSize() != 0;
  }
  return TCastToConstPtr< CPlayer >(&actor) != nullptr;
}

CActorModelParticles::CSystem::CSystem(const char* name) : mRefCount(0), mLoaded(false) {
  TLockedToken< CDependencyGroup > group = gpSimplePool->GetObj(name);
  const rstl::vector< SObjectTag >& tags = group->GetObjectTagVector();
  mTokens.reserve(tags.size());
  for (rstl::vector< SObjectTag >::const_iterator it = tags.begin(); it != tags.end(); ++it) {
    mTokens.push_back(gpSimplePool->GetObj(*it));
  }
}

void CActorModelParticles::CSystem::AddRef() {
  ++mRefCount;
  if (mRefCount == 1) {
    Lock();
  }
}

void CActorModelParticles::CSystem::DelRef() {
  --mRefCount;
  if (mRefCount <= 0) {
    Unlock();
  }
}

void CActorModelParticles::CSystem::Lock() {
  bool loading = false;
  for (rstl::vector< CToken >::iterator it = mTokens.begin(); it != mTokens.end(); ++it) {
    if (!it->HasLock()) {
      it->Lock();
      loading = true;
    } else if (!it->IsLoaded()) {
      loading = true;
    }
  }
  if (!loading) {
    mLoaded = true;
  }
}

void CActorModelParticles::CSystem::Unlock() {
  for (rstl::vector< CToken >::iterator it = mTokens.begin(); it != mTokens.end(); ++it) {
    it->Unlock();
  }
  mLoaded = false;
}

void CActorModelParticles::CSystem::Update() {
  if (mLoaded) {
    return;
  }
  if (mRefCount == 0) {
    return;
  }
  bool loading = false;
  for (rstl::vector< CToken >::const_iterator it = mTokens.begin(); it != mTokens.end(); ++it) {
    if (!it->IsLoaded()) {
      loading = true;
      break;
    }
  }
  if (!loading) {
    mLoaded = true;
  }
}

CActorModelParticles::CItem::CItem(const CEntity& ent, CActorModelParticles& parent)
: mId(ent.GetUniqueId())
, mAreaId(ent.GetCurrentAreaId())
, mOnFireGens(rstl::pair< rstl::auto_ptr< CElementGen >, uint >(rstl::auto_ptr< CElementGen >(), 0))
, mOnFireDelayTimer(0.f)
, mOnFire(false)
, mAshPointIterator(0)
, mAshMaxParticles(-1)
, mAshQueuedParticles(0)
, mAshSeed(99)
, mIcePointIterator(-1)
, mIceSeed(99)
, mElectricPointIterator(0)
, mElectricSeed(99)
, mElectricColor(CColor::White())
, mImplosionPointIterator(0)
, mImplosionMaxParticles(-1)
, mImplosionQueuedParticles(0)
, mImplosionSeed(99)
, mImplosionPoint(CVector3f::Zero())
, mImplosionClipPlane(0.f, CUnitVector3f(CVector3f::Up()))
, mAshy(parent.mAshy)
, mParticleOffsetScale(1.f, 1.f, 1.f)
, mIceXf(CTransform4f::Identity())
, mParent(&parent)
, mRemTime(10.f)
, mLockDeps(0) {}

CActorModelParticles::CItem::~CItem() {
  if (mSfx) {
    CSfxManager::RemoveEmitter(mSfx);
  }
  for (int i = 0; i < 8; ++i) {
    if (mLockDeps & (1 << i)) {
      mParent->DelTypeRef(static_cast< ESystemTypes >(i));
    }
  }
}

bool CActorModelParticles::CItem::Update(float dt, CStateManager& mgr) {
  bool active = false;
  CActor* actor = static_cast< CActor* >(mgr.ObjectById(mId));
  if (actor != nullptr && actor->HasModelData()) {
    mParticleOffsetScale = actor->GetModelScale();
    mIceXf = actor->GetTransform();
    mAreaId = actor->GetCurrentAreaId();
  } else {
    mId = kInvalidUniqueId;
    mAshMaxParticles = 0;
    // Echoes clears the queued ash batch and resets the implosion iterator alongside the ice one.
    mAshQueuedParticles = 0;
    mIcePointIterator = -1;
    if (!mImplosionGen.null()) {
      mImplosionGen->SetParticleEmission(false);
      mImplosionMaxParticles = 0;
      mImplosionQueuedParticles = 0;
    }
    if (!mElectricGen.null()) {
      mElectricGen->SetParticleEmission(false);
    }
    if (mSfx) {
      CSfxManager::RemoveEmitter(mSfx);
      mSfx.Clear();
    }
    mRemTime -= dt;
    if (mRemTime <= 0.f) {
      return false;
    }
  }
  if (UpdateOnFire(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateAshGen(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateIce(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateFirePop(dt, actor)) {
    active = true;
  }
  if (UpdateElectric(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateImplosion(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateRainSplash(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateBurn(dt, actor, mgr)) {
    active = true;
  }
  if (UpdateIcePop(dt, actor)) {
    active = true;
  }
  return active;
}

bool CActorModelParticles::CItem::UpdateRainSplash(float dt, const CActor* actor,
                                                   CStateManager& mgr) {
  if (!mRainSplashGen.null()) {
    if (!mRainSplashGen->IsRaining()) {
      mRainSplashGen = rstl::auto_ptr< CRainSplashGenerator >();
    } else {
      mRainSplashGen->Update(dt, mgr);
      return true;
    }
  }
  return false;
}

bool CActorModelParticles::CItem::UpdateElectric(float dt, const CActor* actor,
                                                 CStateManager& mgr) {
  if (!mElectricGen.null()) {
    if (mElectricGen->IsSystemDeletable()) {
      mElectricGen = rstl::auto_ptr< CParticleElectric >();
    } else {
      if (actor != nullptr && actor->GetActive()) {
        mElectricGen->SetGlobalOrientation(actor->GetTransform().GetRotation());
        mElectricGen->SetGlobalTranslation(actor->GetTranslation());
      }
      if (actor == nullptr || actor->GetActive()) {
        mElectricGen->SetModulationColor(mElectricColor);
        mElectricGen->Update(dt);
        return true;
      }
    }
  } else if (mLockDeps & (1 << kST_Electric)) {
    if (mParent->mLoadedDeps & (1 << kST_Electric)) {
      CParticleElectric* gen = mParent->MakeElectricGen();
      gen->SetModulationColor(mElectricColor);
      mElectricGen = gen;
      mElectricPointIterator = 0;
      mElectricSeed = mgr.Random()->Next();
    }
    return true;
  }
  DontUseType(kST_Electric);
  return false;
}

bool CActorModelParticles::CItem::UpdateIce(float dt, const CActor* actor, CStateManager& mgr) {
  if (mIcePointIterator != -1) {
    return true;
  }
  if (!mIceGens.empty()) {
    bool active = false;
    for (rstl::reserved_vector< rstl::auto_ptr< CElementGen >, 4 >::iterator it = mIceGens.begin();
         it != mIceGens.end(); ++it) {
      CElementGen* gen = it->get();
      if (!gen->IsSystemDeletable()) {
        active = true;
      }
      gen->Update(dt);
    }
    if (!active) {
      mIceGens.clear();
    } else {
      return true;
    }
  } else if ((mLockDeps & (1 << kST_Ice)) && actor != nullptr) {
    if (mParent->mLoadedDeps & (1 << kST_Ice)) {
      mIcePointIterator = 0;
      mIceSeed = mgr.Random()->Next();
    }
    return true;
  }
  DontUseType(kST_Ice);
  return false;
}

bool CActorModelParticles::CItem::UpdateFirePop(float dt, const CActor* actor) {
  if (!mFirePopGen.null()) {
    if (mFirePopGen->IsSystemDeletable()) {
      mFirePopGen = rstl::auto_ptr< CElementGen >();
    } else {
      mFirePopGen->Update(dt);
      return true;
    }
  } else if ((mLockDeps & (1 << kST_FirePop)) && actor != nullptr) {
    if (mParent->mLoadedDeps & (1 << kST_FirePop)) {
      CElementGen* gen = mParent->MakeFirePopGen();
      gen->SetGlobalOrientation(actor->GetTransform());
      if (actor->HasModelData()) {
        const CAABox bounds = actor->GetModelData()->GetBounds(actor->GetTransform());
        gen->SetGlobalTranslation(bounds.GetCenterPoint());
      } else {
        gen->SetGlobalTranslation(actor->GetOtherBounds().GetCenterPoint());
      }
      mFirePopGen = gen;
    }
    return true;
  }
  DontUseType(kST_FirePop);
  return false;
}

bool CActorModelParticles::CItem::UpdateIcePop(float dt, const CActor* actor) {
  if (!mIcePopGen.null()) {
    if (mIcePopGen->IsSystemDeletable()) {
      mIcePopGen = rstl::auto_ptr< CElementGen >();
    } else {
      mIcePopGen->Update(dt);
      return true;
    }
  } else if ((mLockDeps & (1 << kST_IcePop)) && actor != nullptr) {
    if (mParent->mLoadedDeps & (1 << kST_IcePop)) {
      CElementGen* gen = mParent->MakeIcePopGen();
      gen->SetGlobalOrientation(actor->GetTransform());
      if (actor->HasModelData()) {
        const CAABox bounds = actor->GetModelData()->GetBounds(actor->GetTransform());
        gen->SetGlobalTranslation(bounds.GetCenterPoint());
      } else {
        gen->SetGlobalTranslation(actor->GetOtherBounds().GetCenterPoint());
      }
      mIcePopGen = gen;
    }
    return true;
  }
  DontUseType(kST_IcePop);
  return false;
}

bool CActorModelParticles::CItem::UpdateAshGen(float dt, const CActor* actor, CStateManager& mgr) {
  if (!mAshGen.null()) {
    if (mAshMaxParticles == 0 && mAshGen->IsSystemDeletable()) {
      mAshGen = rstl::auto_ptr< CElementGen >();
    } else {
      if (actor != nullptr) {
        mAshGen->SetGlobalOrientAndTrans(actor->GetTransform());
      }
      // Echoes queues the ash points here rather than consuming them; retail clamps to sixteen
      // and subtracts the queued count from what is left.
      if (mAshMaxParticles > 0) {
        mAshQueuedParticles = rstl::min_val(16, mAshMaxParticles);
        mAshMaxParticles -= mAshQueuedParticles;
      }
      mAshGen->Update(dt);
      return true;
    }
  } else if ((mLockDeps & (1 << kST_Ash)) && actor != nullptr) {
    if (mParent->mLoadedDeps & (1 << kST_Ash)) {
      CElementGen* gen = mParent->MakeAshGen();
      mAshGen = gen;
      mAshPointIterator = 0;
      gen->SetGlobalOrientAndTrans(actor->GetTransform());
      float scale = IsMediumOrLarge(*actor) ? 1.f : 0.3f;
      mAshMaxParticles = static_cast< uint >(scale * gen->GetMaxParticles());
      mAshSeed = mgr.Random()->Next();
      // Echoes queues the first batch in the same call that creates the generator.
      if (mAshMaxParticles > 0) {
        mAshQueuedParticles = rstl::min_val(16, mAshMaxParticles);
        mAshMaxParticles -= mAshQueuedParticles;
      }
    }
    return true;
  }
  DontUseType(kST_Ash);
  return false;
}

bool CActorModelParticles::CItem::UpdateImplosion(float dt, const CActor* actor,
                                                  CStateManager& mgr) {
  // TODO: select black-hole/imploder resources, transform the clip plane and queue five points.
  return false;
}

bool CActorModelParticles::CItem::UpdateBurn(float dt, const CActor* actor, CStateManager& mgr) {
  if (actor == nullptr) {
    mAshy.Unlock();
  }
  return mAshy.HasLock();
}

bool CActorModelParticles::CItem::UpdateOnFire(float dt, CActor* actor, CStateManager& mgr) {
  bool sfxActive = false;
  bool effectActive = false;
  mOnFireDelayTimer -= dt;
  if (mOnFireDelayTimer < 0.f) {
    mOnFireDelayTimer = 0.f;
  }
  if (mLockDeps & (1 << kST_OnFire)) {
    if (mParent->mLoadedDeps & (1 << kST_OnFire)) {
      if (mOnFire && actor != nullptr) {
        bool create = true;
        if (!mAshGen.null() || mAshy.HasLock()) {
          create = false;
        } else if (!IsMediumOrLarge(*actor)) {
          int count = 0;
          for (int i = 0; i < 8; ++i) {
            if (!mOnFireGens[i].first.null()) {
              ++count;
            }
          }
          if (count >= 4) {
            create = false;
          }
        }
        if (create) {
          for (int i = 0; i < 8; ++i) {
            rstl::pair< rstl::auto_ptr< CElementGen >, uint >& pair = mOnFireGens[i];
            if (pair.first.null()) {
              pair.second = mgr.Random()->Next();
              pair.first = mParent->MakeOnFireGen();
              mOnFireDelayTimer = 0.3f;
              break;
            }
          }
        }
        if (!mSfx) {
          // Echoes picks a different looping sfx per player count; the ids are the two Prime 1
          // ones plus the multiplayer variants retail's `AddEmitter` call takes (0x1D62 / 0x25C2).
          const int sfx = IsMediumOrLarge(*actor) ? 7522 : 7523;
          mSfx = CSfxManager::AddEmitter(static_cast< ushort >(sfx), actor->GetTranslation(),
                                         actor->GetCurrentAreaId().Value(), 1, false, true,
                                         CSfxManager::kMedPriority);
        }
        mOnFire = false;
      }
      for (int i = 0; i < 8; ++i) {
        if (!mOnFireGens[i].first.null()) {
          CElementGen* const gen = mOnFireGens[i].first.get();
          if (gen->IsSystemDeletable()) {
            mOnFireGens[i].first = rstl::auto_ptr< CElementGen >();
          } else {
            if (actor != nullptr) {
              gen->SetGlobalOrientAndTrans(actor->GetTransform());
            }
            gen->Update(dt);
            effectActive = true;
            sfxActive = true;
          }
        }
      }
    } else {
      effectActive = true;
    }
  }
  if (mSfx) {
    if (sfxActive) {
      CSfxManager::UpdateEmitter(mSfx, mIceXf.GetTranslation(), CVector3f::Zero(), 0x7f);
    } else {
      CSfxManager::RemoveEmitter(mSfx);
      mSfx.Clear();
    }
  }
  if (!effectActive) {
    DontUseType(kST_OnFire);
  }
  return effectActive;
}

void CActorModelParticles::CItem::UseType(ESystemTypes type) {
  const uchar mask = 1 << type;
  if (!(mLockDeps & mask)) {
    mParent->AddTypeRef(type);
    mLockDeps |= mask;
  }
}

void CActorModelParticles::CItem::DontUseType(ESystemTypes type) {
  const uchar mask = 1 << type;
  if (mLockDeps & mask) {
    mParent->DelTypeRef(type);
    mLockDeps &= ~mask;
  }
}

CActorModelParticles::CActorModelParticles()
: mOnFire(gpSimplePool->GetObj(skParticleNames[kST_OnFire]))
, mAsh(gpSimplePool->GetObj(skParticleNames[kST_Ash]))
, mIceBreak(gpSimplePool->GetObj(skParticleNames[kST_Ice]))
, mFirePop(gpSimplePool->GetObj(skParticleNames[kST_FirePop]))
, mIcePop(gpSimplePool->GetObj(skParticleNames[kST_IcePop]))
, mBlackHole(gpSimplePool->GetObj(skParticleNames[kST_BlackHole]))
, mImploder(gpSimplePool->GetObj(skParticleNames[kST_Imploder]))
, mElectric(gpSimplePool->GetObj(skParticleNames[kST_Electric]))
, mAshy(gpSimplePool->GetObj("TXTR_Ashy")) {
  InitializeSystemTypes();
}

void CActorModelParticles::Update(float dt, CStateManager& mgr) {
  UpdateSystemTypes();
  rstl::list< CItem >::iterator it = mItems.begin();
  while (it != mItems.end()) {
    if (!it->Update(dt, mgr)) {
      if (CActor* actor = static_cast< CActor* >(mgr.ObjectById(it->mId))) {
        actor->SetPointGeneratorParticles(false);
      }
      it = mItems.erase(it);
    } else {
      ++it;
    }
  }
}

CElementGen* CActorModelParticles::MakeAshGen() { return rs_new CElementGen(mAsh); }

CElementGen* CActorModelParticles::MakeFirePopGen() { return rs_new CElementGen(mFirePop); }

CElementGen* CActorModelParticles::MakeIcePopGen() { return rs_new CElementGen(mIcePop); }

CElementGen* CActorModelParticles::MakeBlackHoleGen() { return rs_new CElementGen(mBlackHole); }

CElementGen* CActorModelParticles::MakeImploderGen() { return rs_new CElementGen(mImploder); }

CParticleElectric* CActorModelParticles::MakeElectricGen() {
  return rs_new CParticleElectric(mElectric);
}

CElementGen* CActorModelParticles::MakeOnFireGen() { return rs_new CElementGen(mOnFire); }

void CActorModelParticles::StartAsh(CActor& actor) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  it->UseType(kST_Ash);
}

void CActorModelParticles::StartImplosion(CActor& actor, const CVector3f& point, bool blackHole) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  it->mImplosionPoint = point;
  it->UseType(blackHole ? kST_BlackHole : kST_Imploder);
}

void CActorModelParticles::StopImplosion(CActor& actor) {
  if (actor.GetPointGeneratorParticles()) {
    rstl::list< CItem >::iterator it = FindSystem(actor.GetUniqueId());
    if (it != mItems.end()) {
      if (!it->mImplosionGen.null()) {
        it->mImplosionGen->SetParticleEmission(false);
      }
      it->mImplosionMaxParticles = 0;
      it->mImplosionQueuedParticles = 0;
    }
  }
}

void CActorModelParticles::DoFirePop(CActor& actor) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  it->UseType(kST_FirePop);
}

void CActorModelParticles::StartElectric(CActor& actor) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  if (it->mElectricGen.get() == nullptr) {
    it->UseType(kST_Electric);
  } else {
    CParticleElectric* gen = it->mElectricGen.get();
    if (!gen->GetParticleEmission()) {
      gen->SetParticleEmission(true);
    }
  }
}

void CActorModelParticles::StopElectric(CActor& actor) {
  if (actor.GetPointGeneratorParticles()) {
    rstl::list< CItem >::iterator it = FindSystem(actor.GetUniqueId());
    if (it != mItems.end() && !it->mElectricGen.null()) {
      it->mElectricGen->SetParticleEmission(false);
    }
  }
}

void CActorModelParticles::LightDudeOnFire(CActor& actor) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  it->UseType(kST_OnFire);
  if (it->mOnFireDelayTimer <= 0.f) {
    it->mOnFire = true;
  }
}

void CActorModelParticles::StopFire(CActor& actor) {
  if (actor.GetPointGeneratorParticles()) {
    rstl::list< CItem >::iterator it = FindSystem(actor.GetUniqueId());
    if (it != mItems.end()) {
      for (int i = 0; i < 8; ++i) {
        CElementGen* gen = it->mOnFireGens[i].first.get();
        if (gen != nullptr) {
          gen->SetParticleEmission(false);
        }
      }
    }
  }
}

void CActorModelParticles::StartRainSplashes(CActor& actor, CStateManager& mgr, int maxSplashes,
                                             int genRate, float minZ) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  if (it->mRainSplashGen.null() && actor.HasModelData()) {
    it->mRainSplashGen = rs_new CRainSplashGenerator(actor.GetModelScale(), maxSplashes, genRate,
                                                      minZ, 0.1875f);
  }
}

void CActorModelParticles::StopRainSplashes(CActor& actor) {
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  if (!it->mRainSplashGen.null()) {
    it->mRainSplashGen = rstl::auto_ptr< CRainSplashGenerator >();
  }
}

void CActorModelParticles::PointGenerator(const CSkinnedModel& model,
                                          const SSkinningWorkspace& workspace, void* context) {
  // TODO: forward the model's vertex count to the item's GeneratePoints method.
}

static int GetNextBestPt(int start, const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                         int count, CRandom16& random) {
  // TODO: sample ten skinned vertices and select the point farthest from the starting vertex.
  return start;
}

void CActorModelParticles::CItem::GeneratePoints(const CSkinnedModel& model,
                                                 const SSkinningWorkspace& workspace, int count) {
  // TODO: sample skinned positions/normals for fire, ash, implosion, ice, electric and rain
  // effects.
}

void CActorModelParticles::SetupHook(TUniqueId uid) const {
  rstl::list< CItem >::const_iterator it = FindSystem(uid);
  if (it != mItems.end()) {
    CSkinnedModel::SetPointGeneratorFunc(const_cast< CItem* >(&*it), PointGenerator);
  }
}

rstl::list< CActorModelParticles::CItem >::iterator
CActorModelParticles::FindOrCreateSystem(CActor& actor) {
  const TUniqueId uid = actor.GetUniqueId();
  if (actor.GetPointGeneratorParticles()) {
    for (rstl::list< CItem >::iterator it = mItems.begin(); it != mItems.end(); ++it) {
      if (it->mId == uid) {
        return it;
      }
    }
  }
  actor.SetPointGeneratorParticles(true);
  return mItems.insert(mItems.begin(), CItem(actor, *this));
}

rstl::list< CActorModelParticles::CItem >::const_iterator
CActorModelParticles::FindSystem(TUniqueId uid) const {
  for (rstl::list< CItem >::const_iterator it = mItems.begin(); it != mItems.end(); ++it) {
    if (it->mId == uid) {
      return it;
    }
  }
  return mItems.end();
}

rstl::list< CActorModelParticles::CItem >::iterator
CActorModelParticles::FindSystem(TUniqueId uid) {
  for (rstl::list< CItem >::iterator it = mItems.begin(); it != mItems.end(); ++it) {
    if (it->mId == uid) {
      return it;
    }
  }
  return mItems.end();
}

void CActorModelParticles::AddStragglersToRenderer(const CStateManager& mgr) const {
  // TODO: submit particles from visible areas without Prime's thermal-visor branches.
}

void CActorModelParticles::Render(const CStateManager& mgr, const CActor& actor) const {
  // TODO: render the visible actor's generators and restore the model matrix.
}

CElementGen* CActorModelParticles::MakeIceGen() { return rs_new CElementGen(mIceBreak); }

void CActorModelParticles::InitializeSystemTypes() {
  for (int i = 0; i < 8; ++i) {
    const rstl::string name = rstl::string_l(skParticleNames[i]) + rstl::string_l("_DGRP");
    mDgrps.push_back(CSystem(name.data()));
  }
}

void CActorModelParticles::AddTypeRef(ESystemTypes type) {
  const uchar mask = 1 << type;
  mDgrps[type].AddRef();
  if (!(mLoadedDeps & mask)) {
    mLoadingDeps |= mask;
  }
}

void CActorModelParticles::DelTypeRef(ESystemTypes type) {
  CSystem& system = mDgrps[type];
  system.DelRef();
  if (system.mRefCount == 0) {
    const uchar mask = ~(1 << type);
    mLoadingDeps &= mask;
    mLoadedDeps &= mask;
    mJustLoadedDeps &= mask;
  }
}

void CActorModelParticles::UpdateSystemTypes() {
  if (mLoadingDeps == 0) {
    return;
  }
  mJustLoadedDeps = 0;
  for (int i = 0; i < 8; ++i) {
    const uchar mask = 1 << i;
    if (mLoadingDeps & mask) {
      CSystem& system = mDgrps[i];
      system.Update();
      if (system.mLoaded) {
        mJustLoadedDeps |= mask;
        mLoadingDeps &= ~mask;
      }
    }
  }
  mLoadedDeps |= mJustLoadedDeps;
}

void CActorModelParticles::StartBurnDeath(CActor& actor, CStateManager& mgr) {
  // TODO: choose the single/multiplayer burn sound and lock the actor item's ash texture.
}

void CActorModelParticles::StopBurnDeath(CActor& actor) {
  rstl::list< CItem >::iterator it = FindSystem(actor.GetUniqueId());
  if (it != mItems.end()) {
    it->mAshy.Unlock();
  }
}

CTexture* CActorModelParticles::GetAshyTexture(const CActor& actor) const {
  rstl::list< CItem >::const_iterator it = FindSystem(actor.GetUniqueId());
  if (it != mItems.end() && const_cast< CToken& >(it->mAshy).HasLock()
      && it->mAshy.IsLoaded()) {
    return *TToken< CTexture >(it->mAshy);
  }
  return nullptr;
}
