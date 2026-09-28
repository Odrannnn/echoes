#include "MetroidPrime/CEnvFxManager.hpp"

#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CResLoader.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/auto_ptr.hpp"

// The target stores the largest finite single-precision value directly.
static const float skMaximumBlockingHeight = 3.402823466e+38F;

/**
 * `.text 0x80168498`, four bytes, a bare `blr`. It was
 * `src/MetroidPrime/Player/Carve80168498.c` on master; upstream's `config/G2ME01/splits.txt` gives
 * the range to this unit, so the body moves here and that file keeps only its note.
 *
 * It is the last function in the unit and nothing in the DOL calls it with a `bl`, so it is the
 * out-of-line copy of an empty inline rather than anything in `CEnvFxManager` - the same shape as
 * `fn_80025E08`/`fn_80025E0C` in `MetroidPrime/CAnimData.cpp`. `config/G2ME01/symbols.txt` carries
 * the `fn_<addr>` placeholder, so the spelling is retail's own, and it stays `extern "C"`: a C++
 * one would mangle and objdiff would pair nothing.
 */
extern "C" void fn_80168498() {}

CEnvFxManagerGrid::CEnvFxManagerGrid(const CVector2i& position, const CVector2i& extent,
                                     const rstl::vector< CVectorFixed8_8 >& initialParticles,
                                     int reserve)
: mBlockDirty(true)
, mPosition(position)
, mExtent(extent)
, mBlock(false, skMaximumBlockingHeight)
, mParticles(initialParticles) {
  mParticles.reserve(reserve);
}

CEnvFxManager::CEnvFxManager()
: mParticleBounds(CVector3f(-63.5f, -63.5f, -63.5f), CVector3f(63.5f, 63.5f, 63.5f))
, mFocusCellPosition(CVector3f::Zero())
, mEnableSplash(false)
, mFirstSnowForce(0.f)
, mLastBlockedGridIdx(-1)
, mFxDensity(0.f)
, mTargetFxDensity(0.f)
, mMaxDensityDeltaSpeed(0.f)
, mRainSoundFade(1.f)
, mSnowflakeTextureMipBlanked(false)
, mTxtrEnvGradient(TLockedToken< CTexture >(gpSimplePool->GetObj("TXTR_EnvGradient")))
, mEnvRainSplash(TLockedToken< CGenDescription >(gpSimplePool->GetObj("PART_EnvRainSplash")))
, mRainSoundActive(false)
, mRainSoundsStopped(false)
, mTxtrSnowFlake(TLockedToken< CTexture >(gpSimplePool->GetObj("TXTR_SnowFlake")))
, mUnderwaterFlake(TLockedToken< CTexture >(gpSimplePool->GetObj("TXTR_UnderwaterFlake")))
, mDarkWorldParticleTexture(gpSimplePool->GetObj("TXTR_DarkworldParticleTexture"))
, mPreviousFxType(kEFX_None) {
  CRandom16 random(0);
  for (int i = 0; i < 4; ++i) {
    mEnvRainSplashIds.push_back(kInvalidUniqueId);
  }

  for (int row = 0; row < 8; ++row) {
    for (int column = 0; column < 8; ++column) {
      mGrids.push_back(CEnvFxManagerGrid(CVector2i(column * 0x800, row * 0x800),
                                         CVector2i(0x800, 0x800), rstl::vector< CVectorFixed8_8 >(),
                                         0xab));
    }
  }

  for (int i = 15; i >= 0; --i) {
    mSnowZDeltas.push_back(CVector3f(0.f, 0.f, random.Range(-2.f, -4.f)));
  }
}

void CEnvFxManagerGrid::RenderRainParticles(const CTransform4f& camXf) {
  // TODO: Draw fixed-point rain lines with camera-dependent length.
}

void CEnvFxManagerGrid::RenderSnowParticles(const CTransform4f& camXf) {
  // TODO: Draw camera-facing snow quads.
}

void CEnvFxManagerGrid::RenderDriftingParticles(const CTransform4f& camXf) {
  // TODO: Draw the billboards for environment effect 5.
}

void CEnvFxManagerGrid::RenderParticleTrails(EEnvFxType type) {
  // TODO: Interpolate the eight-point histories and fade their line strips.
}

void CEnvFxManagerGrid::RenderUnderwaterParticles(const CTransform4f& camXf) {
  // TODO: Draw camera-facing underwater quads.
}

bool CEnvFxManagerGrid::SetupRender(const CTransform4f& xf, const CTransform4f& invXf,
                                    const CTransform4f& camXf, float density, EEnvFxType type) {
  if (mParticles.empty() || !mBlock.first) {
    return false;
  }

  // TODO: Set the grid model transform and the blocking-height texture matrix.
  return false;
}

void CEnvFxManagerGrid::Render(const CTransform4f& xf, const CTransform4f& invXf,
                               const CTransform4f& camXf, float density, EEnvFxType type) {
  if (!SetupRender(xf, invXf, camXf, density, type)) {
    return;
  }

  switch (type) {
  case kEFX_Snow:
    RenderSnowParticles(camXf);
    break;
  case kEFX_Rain:
    RenderRainParticles(camXf);
    break;
  case kEFX_UnderwaterFlake:
    RenderUnderwaterParticles(camXf);
    break;
  case kEFX_Unknown5:
    RenderDriftingParticles(camXf);
    break;
  case kEFX_Unknown6:
  case kEFX_Unknown7:
    RenderParticleTrails(type);
    break;
  default:
    break;
  }
}

void CEnvFxManagerGrid::RenderDarkWorldParticles(const CTransform4f& xf, const CTransform4f& invXf,
                                                 const CTransform4f& camXf, float density,
                                                 const CVectorFixed8_8* offsets,
                                                 const CVectorFixed8_8* upDeltas,
                                                 const CVectorFixed8_8* rightDeltas) {
  // TODO: Draw lifetime-faded quads using the sixteen precomputed corner offsets.
}

CVector3f CEnvFxManager::GetParticleBoundsToWorldScale() const {
  return (mParticleBounds.GetMaxPoint() - mParticleBounds.GetMinPoint()) / 127.f;
}

void CEnvFxManager::MoveWrapCells(EEnvFxType type, int moveX, int moveY) {
  // TODO: Snapshot blocking heights, wrap the grid positions and dirty exposed cells.
}

void CEnvFxManager::AsyncLoadResources(CStateManager& mgr) {
  // TODO: Create and register one persistent visor-rain billboard per player.
}

/**
 * `CEnvFxManager::Initialize` - retail `.text 0x80166880`, `size:0xEC` = 236 bytes, 32-byte frame,
 * r28-r31 saved. It was `src/MetroidPrime/CEnvFxManagerInitialize.cpp` on master; upstream's
 * `config/G2ME01/splits.txt` gives the range to this unit, so the body moves here.
 *
 * **This is the fifth and last statement of `CGameGlobalObjects::PostInitialize`**
 * (`src/MetroidPrime/main.cpp`), and it is last because the paks are first: the resource it asks
 * for by name is answered out of a pak, so it cannot run before step one.
 *
 * ```
 * 80166888:  lis/addi r4,lbl_803A96FC            <- "DUMB_SnowForces", offset 0 of the pool
 * 801668A4:  lwz r3,gpResourceFactory ; vtable +0x1C ; bctrl
 *                                                <- slot 7 = IFactory::GetResourceIdByName
 * 801668B8:  lwz r6,gpResourceFactory ; mr r4,r3 ; li r5,0 ; addi r3,r6,4 ; bl fn_802FC63C
 *                                                <- GetResLoader().LoadNewResourceSync(*tag, 0)
 * 801668CC:  neg/or/srwi -> stb 8(r1) ; stw r3,12(r1)
 *                                                <- rstl::auto_ptr< CInputStream > at r1+8
 * 801668EC:  256 x (2 x ReadFloat -> stfs, +4), +8 <- lbl_803DABE0, .bss, size:0x800
 * 80166920:  lbz 8(r1) ; lwz 12(r1) ; vtable +0x8 with r4=1
 *                                                <- the auto_ptr's destructor, `delete` via
 *                                                   ~CInputStream's deleting slot
 * ```
 *
 * Both retail objects are referenced, not defined: `lbl_803A96FC` is a 0x94-byte `.rodata` string
 * pool that 0x80166980 (the next function, not this unit's) also reads, and `lbl_803DABE0` is a
 * `.bss` array no unit claims. A literal here would put a `.rodata` section in this object that
 * retail's range does not have.
 *
 * **The host keeps the stub it had, deliberately.** `src/MetroidPrime/PortPoolStandIns.cpp`
 * registers a `DUMB_SnowForces` stand-in row and points `CResFactory::GetResourceIdByName` at the
 * stand-in table, and records there that the 512 `ReadFloat()` calls "do not happen,
 * `lbl_803DABE0` stays zeroed, and **every snow particle gets a zero force vector** - no wind, no
 * gust, straight-down fall ... a pak with the real bytes behind `DUMB_SnowForces` is the only thing
 * that makes this true, and that is the loader's missing half, not a registry row." Running the
 * real body on the host would read a stand-in stream that has no snow-force bytes, so the guard
 * below keeps that decision where it was made and only gives the matching build the retail body.
 */
#ifndef TARGET_PC
extern "C" const char lbl_803A96FC[];
extern "C" float lbl_803DABE0[256][2];
// `include/Kyoto/Streams/CInputStream.hpp` declares this under `#ifdef TARGET_PC` only, because
// the host spells it in its own byte order; the matching build has no such declaration.
extern "C" void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);

void CEnvFxManager::Initialize() {
  const SObjectTag* tag = gpResourceFactory->GetResourceIdByName(lbl_803A96FC);
  rstl::auto_ptr< CInputStream > stream(static_cast< CInputStream* >(
      fn_802FC63C(&gpResourceFactory->GetResLoader(), *tag, nullptr)));
  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 2; ++j) {
      lbl_803DABE0[i][j] = stream->ReadFloat();
    }
  }
}
#else
void CEnvFxManager::Initialize() {
  // TODO: Read the 256 pairs of floats from DUMB_SnowForces.  See the note above and the
  // "DUMB_SnowForces" section of src/MetroidPrime/PortPoolStandIns.cpp.
}
#endif // TARGET_PC

void CEnvFxManager::Cleanup() {
  mEnvRainSplashIds.clear();
  mRainSoundActive = false;
  mLeftRainSound.Clear();
  mRightRainSound.Clear();
}

void CEnvFxManager::ClearParticles() {
  for (int i = mGrids.size() - 1; i >= 0; --i) {
    CEnvFxManagerGrid& grid = mGrids[i];
    grid.mParticles = rstl::vector< CVectorFixed8_8 >();
    grid.mParticleLifetimes = rstl::vector< float >();
    grid.mTrailFrames = rstl::vector< int >();
  }
}

void CEnvFxManager::Update(float dt, CStateManager& mgr) {
  // TODO: Follow effect transitions, density fades, camera movement and particle updates.
}

void CEnvFxManager::CreateNewParticles(EEnvFxType type, const CTransform4f& invXf) {
  // TODO: Resize and seed per-grid particles, lifetimes and trail histories for this effect.
}

void CEnvFxManager::CalculateSnowForces(const CVectorFixed8_8& zVec,
                                        rstl::reserved_vector< CVectorFixed8_8, 256 >& snowForces,
                                        EEnvFxType type, const CVector3f& inverseScale, float dt) {
  // TODO: Build the force cycle, with separate dark-world and effect-5 motion.
}

void CEnvFxManager::UpdateBlockedGrids(CStateManager& mgr, EEnvFxType type,
                                       const CTransform4f& camXf, const CTransform4f& xf,
                                       const CTransform4f& invXf) {
  // TODO: Resolve ceilings, blocker triggers and water surfaces, then update splash visibility.
}

void CEnvFxManager::UpdateSnowParticles(rstl::reserved_vector< CVectorFixed8_8, 256 >& snowForces) {
  for (int i = mGrids.size() - 1; i >= 0; --i) {
    CEnvFxManagerGrid& grid = mGrids[i];
    uint force = static_cast< uint >(mFirstSnowForce);
    if (!grid.mBlock.first) {
      continue;
    }

    for (int j = grid.mParticles.size() - 1; j >= 0; --j) {
      CVectorFixed8_8& particle = grid.mParticles[j];
      particle += snowForces[force];
      particle.mZ &= 0x3fff;
      force = (force + 1) & 0xff;
    }
  }
}

void CEnvFxManager::UpdateDriftingParticles(
    float dt, rstl::reserved_vector< CVectorFixed8_8, 256 >& snowForces,
    const CTransform4f& invXf) {
  // The target retains the dt and transform arguments, but uses the snow update body.
  UpdateSnowParticles(snowForces);
}

void CEnvFxManager::UpdateParticleTrails(float dt, const CVectorFixed8_8& zVec) {
  // TODO: Advance normalized lifetimes, frame counters and eight-point trail histories.
}

void CEnvFxManager::UpdateDarkWorldParticles(
    float dt, rstl::reserved_vector< CVectorFixed8_8, 256 >& snowForces,
    const CTransform4f& invXf) {
  // TODO: Apply lifetime-dependent forces and respawn particles at the blocking height.
}

void CEnvFxManager::UpdateRainParticles(const CVectorFixed8_8& zVec, const CVector3f& inverseScale,
                                        float dt) {
  // TODO: Apply rainfall speed and camera displacement to visible grids.
}

void CEnvFxManager::UpdateUnderwaterParticles(const CVectorFixed8_8& zVec) {
  for (int i = mGrids.size() - 1; i >= 0; --i) {
    rstl::vector< CVectorFixed8_8 >& particles = mGrids[i].mParticles;
    for (int j = particles.size() - 1; j >= 0; --j) {
      particles[j].mZ = (particles[j].mZ + zVec.GetZ()) & 0x3fff;
    }
  }
}

void CEnvFxManager::UpdateVisorSplash(CStateManager& mgr, float dt, const CTransform4f& camXf) {
  // TODO: Relocate each player's billboard and derive the rain rate from view and velocity.
}

void CEnvFxManager::SetSplashEffectRate(float rate, CStateManager& mgr) {
  // TODO: Set the generator rate on each active visor-rain billboard.
}

CTransform4f CEnvFxManager::GetParticleBoundsToWorldTransform() const {
  return CTransform4f::Translate(mFocusCellPosition) *
         CTransform4f::Translate(CVector3f(-31.75f, -31.75f, -31.75f)) *
         CTransform4f::Scale(GetParticleBoundsToWorldScale());
}

void CEnvFxManager::BlankFirstSnowflakeMip(CTexture& tex) {
  // TODO: Clear and flush the texture's first mip once before rendering.
}

void CEnvFxManager::SetupSnowTevs(CStateManager& mgr) {
  // TODO: Configure snow texture, fog, blending and ceiling clipping.
}

void CEnvFxManager::SetupDriftingParticleTevs(CStateManager& mgr) {
  // TODO: Configure the effect-5 snow-texture variant.
}

void CEnvFxManager::SetupDarkWorldTevs() {
  // TODO: Configure additive dark-world particle rendering.
}

void CEnvFxManager::SetupUnderwaterTevs(const CTransform4f& invXf, CStateManager& mgr) {
  // TODO: Configure underwater texture blending and water-surface clipping.
}

void CEnvFxManager::SetupDefaultTevSwapMode() {
  // TODO: Restore the default TEV swap mode after underwater rendering.
}

void CEnvFxManager::SetupRainTevs() {
  // TODO: Configure rain line rendering and the environment gradient.
}

void CEnvFxManager::SetupParticleTrailTevs(CStateManager& mgr) {
  // TODO: Configure fog, line width, gradient texture and additive trail blending.
}

void CEnvFxManager::Render(const CStateManager& mgr) {
  // TODO: Select the effect setup and render the grids in camera space.
}

static int CalcRainVolume(float density) {
  if (density < 0.1f) {
    return static_cast< int >(74.f * (density / 0.1f));
  }
  return static_cast< int >(21.f * (density / 0.9f) + 74.f);
}

static short CalcRainPitch(float density) { return static_cast< short >(8192.f * density); }

void CEnvFxManager::UpdateRainSounds(float dt, CStateManager& mgr) {
  // TODO: Fade rain audio and maintain the two camera-relative emitters, volume and pitch.
}

void CEnvFxManager::FadeDensity(float density, int speed) {
  mTargetFxDensity = density;
  mMaxDensityDeltaSpeed = speed;
}

void CEnvFxManager::StopRainSounds() { mRainSoundsStopped = true; }

void CEnvFxManager::PlayRainSounds() { mRainSoundsStopped = false; }

void CEnvFxManager::BuildBlockObjectList(rstl::reserved_vector< TUniqueId, 1024 >& list,
                                         CStateManager& mgr) {
  // TODO: Collect triggers with the environment-blocking flag from the object list.
}

void CEnvFxManager::AreaLoaded() {
  for (int i = 0; i < mGrids.size(); ++i) {
    mGrids[i].SetDirty(true);
  }
}
