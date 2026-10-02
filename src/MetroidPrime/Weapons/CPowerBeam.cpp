// Retail's two `operator new` sites in this object (`Update`'s and `EnableSecondaryFx`'s
// `new CElementGen`) name the `??(??)` placement literal at `.rodata` 0x803AAB80, one of the three
// in the unclaimed `.rodata` gap at 0x803AAB70 - see `CMEMORY_NEW_FILE` in
// `Kyoto/Alloc/CMemory.hpp`. Left unset, the compiler builds a per-TU `@stringBase0` copy of the
// literal instead, and an unclaimed `.rodata` is appended by mwldeppc *after* every claimed
// `.rodata` contribution, which pushes `@stringBase0` itself 8 bytes later and rewrites a
// `lis`/`addi` pair in every other unit that references it. Must precede every include.
extern "C" const char lbl_803AAB80[];
#define CMEMORY_NEW_FILE lbl_803AAB80
#include "MetroidPrime/Weapons/CPowerBeam.hpp"

#include "Kyoto/Audio/CSfxHandle.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CStateManager.hpp"

// **`const` on this declaration is load-bearing, not tidiness.** `mainHead.cpp` defines the
// sentinel as `extern const ushort kInternalInvalidSfxId__11CSfxManager = 0xFFFF`, but reading it here through a
// *non*-const `extern "C" ushort` makes mwcceppc rank the `lhz r0,0(0) R_PPC_EMB_SDA21` load as an
// ordinary mutable global, and it schedules the load at its point of use - immediately before
// `cmplw`, after the whole `stmw r22,32` / `mr r22,r3`..`mr r29,r10` prologue block. Retail's
// `Fire` loads it in the middle of the prologue instead, between the `fmr f31,f3` and `stfd f30,80`
// pairs, with `cmplw` right after the `fmr f30,f2`. Declaring the global `const` gives the
// scheduler the same rank it gives retail's own read, and the whole 244-byte function then matches
// instruction for instruction. It was the last thing standing between this unit and 100%.
extern "C" const ushort kInternalInvalidSfxId__11CSfxManager;
// Retail's `InitializeResources` reads the pool object name from `.sdata2`, not from a `lis`/`addi`
// pair against this unit's own string pool: `lwz r5, 0x8041D394(r2)` / `lwz r5, 0x8041D398(r2)` hold
// 0x803AADF2 / 0x803AADFC, which are the `.rodata` strings "ShotSmoke" and "Power2nd_1". Those two
// pointers are the retail globals this port already defines in `src/MetroidPrime/mainHead.cpp`
// (which is where the *values* live); declaring them here is the same trick `kInternalInvalidSfxId__11CSfxManager` above
// uses. It also keeps "ShotSmoke"/"Power2nd_1" out of this unit's `@stringBase0`, which is what
// lets `rs_new`'s own `??(??)` literal sit at offset 0 - the shape retail's `EnableSecondaryFx` has.
extern "C" const char* const lbl_8041D394;
extern "C" const char* const lbl_8041D398;
extern "C" const ushort lbl_8041D248[2][2];
// `.sdata2` 0x8041D250 / 0x8041D254 are 2.0f and 0.0f - the ctor and `ReInitVariables` zero
// `mSmokeTimer` with the second, `UpdateGunFx` arms it with the first and tests it against the
// second, and retail `lfs`es all three out of `.sdata2`. Written as `2.f` / `0.f` they match, but
// mwcceppc then gives this unit a **private `.sdata2` holding a copy of each**, and an unclaimed
// `.sdata2` is appended past the end of the section by mwldeppc - which moves `.bss2` by 8 bytes
// and breaks the DOL hash on 43 bytes' worth of `.bss2` pointers. Naming the retail globals keeps
// the unit's `.sdata2` empty. Same reason as `lbl_8041D394` above, and the same trick as
// `kInternalInvalidSfxId__11CSfxManager`; the values live in `src/MetroidPrime/mainHead.cpp` for the port.
extern "C" const float lbl_8041D250;
extern "C" const float lbl_8041D254;

CPowerBeam::CPowerBeam(TUniqueId playerId, const CVector3f& scale, int unk)
: CGunWeapon(kWT_Power, playerId, scale, unk)
, mShotSmoke()
, mPower2nd1()
, mSmokeTimer(lbl_8041D254)
, mSmokeState(kSS_Inactive)
, x244_24(false)
, mLoaded(false) {}

CPowerBeam::~CPowerBeam() {}

void CPowerBeam::ReInitVariables() {
  mShotSmokeGen = nullptr;
  mPower2ndGen = nullptr;
  mSmokeTimer = lbl_8041D254;
  mSmokeState = kSS_Inactive;
  x244_24 = false;
  mLoaded = false;
  mEnabledSecondaryEffect = kSFT_None;
}

void CPowerBeam::PreRenderGunFx(const CStateManager& mgr, const CTransform4f& xf) {
  CTransform4f backupView = CGraphics::GetViewMatrix();

  CGraphics::SetViewPointMatrix(xf.GetInverse() * backupView);
  gpRender->SetModelMatrix(CTransform4f::Identity());
  if (!mShotSmokeGen.null() && mSmokeState != kSS_Inactive)
    mShotSmokeGen->Render();

  CGraphics::SetViewPointMatrix(backupView);
}

void CPowerBeam::PostRenderGunFx(const CStateManager& mgr, const CTransform4f& xf) {
  if (mEnabledSecondaryEffect != kSFT_None && !mPower2ndGen.null())
    mPower2ndGen->Render();
  CGunWeapon::PostRenderGunFx(mgr, xf);
}

void CPowerBeam::UpdateGunFx(bool shotSmoke, float dt, const CStateManager& mgr,
                             const CTransform4f& xf) {
  switch (mSmokeState) {
  case kSS_Inactive:
    if (shotSmoke) {
      if (!mShotSmokeGen.null())
        mShotSmokeGen->SetParticleEmission(true);
      mSmokeTimer = lbl_8041D250;
      mSmokeState = kSS_Active;
    }
    break;
  case kSS_Active:
    if (mSmokeTimer > lbl_8041D254) {
      mSmokeTimer -= dt;
    } else {
      if (!mShotSmokeGen.null())
        mShotSmokeGen->SetParticleEmission(false);
      mSmokeState = kSS_Done;
    }
    // [[fallthrough]];
  case kSS_Done:
    if (!mShotSmokeGen.null()) {
      CTransform4f locator =
          mSolidModelData->GetScaledLocatorTransform(rstl::string_l(CGunWeapon::skMuzzleLocator));
      mShotSmokeGen->SetGlobalTranslation(locator.GetTranslation());
      mShotSmokeGen->Update(dt);
      if (mSmokeState == kSS_Done && mShotSmokeGen->GetSystemCount() == 0)
        mSmokeState = kSS_Inactive;
    } else {
      mSmokeState = kSS_Inactive;
    }
    break;
  }

  if (mEnabledSecondaryEffect != kSFT_None && !mPower2ndGen.null()) {
    mPower2ndGen->SetGlobalOrientAndTrans(xf);
    mPower2ndGen->Update(dt);
  }

  CGunWeapon::UpdateGunFx(shotSmoke, dt, mgr, xf);
}

void CPowerBeam::Update(float dt, CStateManager& mgr) {
  CGunWeapon::Update(dt, mgr);
  if (IsLoaded())
    return;

  if (CGunWeapon::IsLoaded() && !mLoaded) {
    mLoaded = mShotSmoke->IsLoaded() && mPower2nd1->IsLoaded();
    if (mLoaded) {
      // x234_shotSmokeGen = rs_new CElementGen(x21c_shotSmoke);
      mShotSmokeGen = rs_new CElementGen(*mShotSmoke);
      mShotSmokeGen->SetParticleEmission(false);
    }
  }
}

void CPowerBeam::Fire(const TCachedToken< CWeaponDescription >& projectile, bool underwater,
                      float dt, CPlayerState::EChargeStage chargeState, const CTransform4f& xf,
                      CStateManager& mgr, TUniqueId homingTarget, uint projectileAttributes,
                      ushort soundId, TUniqueId* projectileId, CSfxHandle* soundHandle,
                      float chargeFactor1, float chargeFactor2) {

  // `kInternalInvalidSfxId__11CSfxManager` (0xFFFF) is the "caller supplied no id" sentinel; on that path retail takes the
  // per-charge-stage id from `.sdata2 0x8041D248`, a `[multiplayer][chargeStage]` table, so the row is
  // `IsMultiplayer()` and the column `chargeState` (`clrlwi`/`neg`/`or`/`rlwinm r4,r4,3,29,29` is the
  // row select turned into a 0-or-8 byte offset, `slwi r0,r25,1` the column, one `lhzx` the load).
  // A local rather than a write back to the parameter, because retail merges the two arms into one
  // register: `lhzx r5,r3,r0` on the taken path and `mr r5,r11` on the other, both feeding
  // `stw r5,8(r1)`. Storing the parameter back emits `stw r11,8(r1)` instead.
  ushort sound;
  if (soundId == kInternalInvalidSfxId__11CSfxManager) {
    sound = lbl_8041D248[mgr.IsMultiplayer() ? 1 : 0][static_cast< size_t >(chargeState)];
  } else {
    sound = soundId;
  }

  CGunWeapon::Fire(projectile, underwater, dt, chargeState, xf, mgr, homingTarget,
                   projectileAttributes, sound, projectileId, soundHandle, chargeFactor1,
                   chargeFactor2);
}

void CPowerBeam::Load(CStateManager& mgr, bool subtypeBasePose) {
  CGunWeapon::Load(mgr, subtypeBasePose);
  mShotSmoke->Lock();
  mPower2nd1->Lock();
}

void CPowerBeam::Unload(CStateManager& mgr) {
  CGunWeapon::Unload(mgr);
  if (!mgr.IsMultiplayer()) {
    mPower2nd1->Unlock();
    mShotSmoke->Unlock();
  }
  ReInitVariables();
}

void CPowerBeam::ReleaseResources(CStateManager& mgr) {
  CGunWeapon::ReleaseResources(mgr);
  if (!mgr.IsMultiplayer()) {
    mPower2nd1->Unlock();
    mShotSmoke->Unlock();
  }
  mShotSmokeGen = nullptr;
  mPower2ndGen = nullptr;
  mSmokeState = kSS_Inactive;
  x244_24 = false;
  mEnabledSecondaryEffect = kSFT_None;
}

bool CPowerBeam::IsLoaded() const { return CGunWeapon::IsLoaded() && mLoaded; }

void CPowerBeam::EnableSecondaryFx(ESecondaryFxType type) {
  switch (type) {
  case kSFT_None:
  case kSFT_ToCombo:
  case kSFT_CancelCharge:
    if (mEnabledSecondaryEffect != kSFT_None && !mPower2ndGen.null())
      mPower2ndGen->SetParticleEmission(false);
    mEnabledSecondaryEffect = kSFT_None;
    break;
  case kSFT_Charge:
    mPower2ndGen = rs_new CElementGen(*mPower2nd1);
    mPower2ndGen->SetGlobalScale(mScale);
    mEnabledSecondaryEffect = type;
    break;
  default:
    break;
  }
}

void CPowerBeam::InitializeResources(CStateManager& mgr) {
  // The guard tests **bit 0** of the flag byte at this+0x270: retail emits `rlwinm. r0,r0,31,31,31`,
  // where `mSubtypeBasePose` (bit 3) gave `rlwinm. r0,r0,28,31,31`. mwcceppc gives the first `bool : 1`
  // of a run bit 6 and fills downwards, so the seventh flag declared here, `mResourcesAllocated`, is
  // bit 0 - which is also what the guard means: retail's `CGunWeapon::InitializeResources` opens
  // with the same bit-0 test and closes with `rlwimi r0,r3,1,30,30`, which sets bit 0.
  if (!mResourcesAllocated) {
    CGunWeapon::InitializeResources(mgr);
    mShotSmoke = gpSimplePool->GetObj(lbl_8041D394);
    mPower2nd1 = gpSimplePool->GetObj(lbl_8041D398);
  }
}
