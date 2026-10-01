#include "MetroidPrime/CActorModelParticles.hpp"

#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/Animation/CSkinRules.hpp"
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
    mTokens.push_back_unsafe(gpSimplePool->GetObj(*it));
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
  // One `||`, not two separate early returns: MWCC lays the fall-through into the `then` block, so
  // retail's `bnelr` (skip the whole body when loaded, fall into the loop when refcount is
  // non-zero) only comes out of a single condition.
  if (mLoaded || mRefCount == 0) {
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
  // Echoes tests the whole mask once before walking it; Prime 1 has the same guard.
  if (mLockDeps != 0) {
    for (int i = 0; i < 8; ++i) {
      if (mLockDeps & (1 << i)) {
        mParent->DelTypeRef(static_cast< ESystemTypes >(i));
      }
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
        // Chained, with no named `CAABox`: retail's `GetBounds` sret slot *is* the box the
        // `GetCenterPoint()` call reads, and the named `CVector3f` is what puts the second copy on
        // the stack. A named `const CAABox` costs an extra 12-instruction box copy.
        const CVector3f center =
            actor->GetModelData()->GetBounds(actor->GetTransform()).GetCenterPoint();
        gen->SetGlobalTranslation(center);
      } else {
        const CVector3f center = actor->GetOtherBounds().GetCenterPoint();
        gen->SetGlobalTranslation(center);
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
        // Chained, with no named `CAABox`: retail's `GetBounds` sret slot *is* the box the
        // `GetCenterPoint()` call reads, and the named `CVector3f` is what puts the second copy on
        // the stack. A named `const CAABox` costs an extra 12-instruction box copy.
        const CVector3f center =
            actor->GetModelData()->GetBounds(actor->GetTransform()).GetCenterPoint();
        gen->SetGlobalTranslation(center);
      } else {
        const CVector3f center = actor->GetOtherBounds().GetCenterPoint();
        gen->SetGlobalTranslation(center);
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
      // Echoes queues the ash points here rather than consuming them; retail clamps to sixteen
      // and subtracts the queued count from what is left. The clamp is *inside* the `actor` test:
      // retail's `beq` at 0x8014E6D4 skips the `SetGlobalOrientAndTrans` call **and** the whole
      // clamp block, so a null actor skips both.
      if (actor != nullptr) {
        mAshGen->SetGlobalOrientAndTrans(actor->GetTransform());
        if (mAshMaxParticles > 0) {
          mAshQueuedParticles = rstl::min_val(16, mAshMaxParticles);
          mAshMaxParticles -= mAshQueuedParticles;
        }
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
          // Echoes picks the looping sfx from the player count: the single-player pair, the
          // multiplayer pair, and one extra id that only a `CPlayer` can reach. The `srawi` mask
          // makes 0 the true arm, so each `addi` immediate is the *false* value.
          ushort sfx = static_cast< ushort >(IsMediumOrLarge(*actor) ? 7393 : 7394);
          if (mgr.IsMultiplayer()) {
            sfx = static_cast< ushort >(IsMediumOrLarge(*actor) ? 9865 : 9866);
          } else if (TCastToPtr< CPlayer >(actor) != nullptr) {
            sfx = 155;
          }
          // The cast is redundant to a reader and not redundant to the compiler: retail
          // re-narrows the merged value with `clrlwi r4,r26,16` before the call.
          mSfx = CSfxManager::AddEmitter(static_cast< ushort >(sfx), actor->GetTranslation(),
                                         actor->GetCurrentAreaId().Value(), true, true,
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
              // `CTransform4f::GetTranslation()` returns by value, so retail materialises the
              // three `m03/m13/m23` loads into a stack `CVector3f` and passes its address;
              // `CActor::GetTranslation()` returns a reference to `mPosition` and would not.
              gen->SetGlobalTranslation(actor->GetTransform().GetTranslation());
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
  // Echoes passes the skin rules' vertex count as the sample limit; `mSkinRules` is the third
  // 12-byte `TLockedToken` word, so this is `model.GetSkinRules()->GetNumPoints()`.
  reinterpret_cast< CItem* >(context)
      ->GeneratePoints(model, workspace, model.GetSkinRules()->GetNumPoints());
}

static int GetNextBestPt(int start, const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                         int count, CRandom16& random) {
  int best = start;
  // Declared before `startVec`, not after: retail loads the 0.0f into f28 at 0x8014CD6C, in the
  // prologue's constant block ahead of the `mr` block, while declaring it after the
  // `GetSkinnedPosition` call sinks the load to just before the loop.
  float maxDistance = 0.f;
  const CVector3f startVec = model.GetSkinnedPosition(workspace, start);
  for (int i = 0; i < 10; ++i) {
    const int index = random.Range(0, count - 1);
    const CVector3f point = model.GetSkinnedPosition(workspace, index);
    const CVector3f& delta = startVec - point;
    // Spelled out, not `MagSquared()`: the inline is three `fmadds` and never touches memory,
    // where retail emits three `fmuls`, two `fadds` and three stack stores.
    const float distance = delta.GetX() * delta.GetX() + delta.GetY() * delta.GetY() +
                           delta.GetZ() * delta.GetZ();
    if (distance > maxDistance) {
      best = index;
      maxDistance = distance;
    }
  }
  return best;
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
  // Prime 1's `StartBurnDeath` has no `mgr`; Echoes picks the burn-death emitter from the player
  // count. The `srawi` mask yields 0/-1, so each `addi` immediate is the *false* arm.
  rstl::list< CItem >::iterator it = FindOrCreateSystem(actor);
  ushort sfx = static_cast< ushort >(IsMediumOrLarge(actor) ? 7521 : 7522);
  if (mgr.IsMultiplayer()) {
    if (CPlayer* player = TCastToPtr< CPlayer >(&actor)) {
      sfx = static_cast< ushort >(
          player->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed ? 9602 : 9601);
    } else {
      sfx = static_cast< ushort >(IsMediumOrLarge(actor) ? 9601 : 9602);
    }
  }
  // Same re-narrowing as `UpdateOnFire`: retail emits `clrlwi r4,r29,16` before this call.
  CSfxManager::AddEmitter(static_cast< ushort >(sfx), actor.GetTranslation(),
                          actor.GetCurrentAreaId().Value(), true, false, CSfxManager::kMedPriority);
  it->mAshy.Lock();
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
