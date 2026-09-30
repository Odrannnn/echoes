#include "MetroidPrime/ScriptObjects/CScriptSound.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "Collision/CMaterialFilter.hpp"
#include "Collision/CRayCastResult.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"

bool CScriptSound::sFirstInFrame;

static int fn_8009dce8(int volume) {
  // TODO: scale volume with the music-volume tweak spline.
  return volume;
}

static CVector3f fn_8009dd94(const CStateManager& mgr, const rstl::vector< TUniqueId >& sources) {
  // TODO: find the closest point in the connected sound volumes to any player's listener.
  return CVector3f::Zero();
}

CScriptSound::CScriptSound(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                           const CTransform4f& xf, ushort soundId, float maxDist, float distComp,
                           float startDelay, short minVolume, short volume, short unknown198,
                           short darkVisorVolume, short priority, short pan, short surroundPan,
                           short unknown1a2, bool looped, bool nonEmitter, bool playerRelativePan,
                           bool autoStart, bool occlusionTest, bool acoustics, bool worldSfx,
                           bool allowDuplicates, bool allAreas, bool scaleByMusicVolume, int pitch)
: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(kMT_Trigger), CActorParameters(),
         kInvalidUniqueId)
, mOcclusionUpdateTimer(0.f)
, mSfxHandle()
, mMaxVolume(0)
, mCurrentMaxVolume(0)
, mVolumeDelta(0)
, mEmitterPosition(xf.GetTranslation())
, mStartDelay(startDelay)
, mSoundId(soundId)
, mMaxDistance(maxDist)
, mDistanceCompensation(distComp)
, mMinVolume(minVolume)
, mVolume(volume)
, x198_(unknown198)
, mDarkVisorVolume(darkVisorVolume)
, mPriority(priority)
, mPan(pan)
, mSurroundPan(surroundPan)
, x1a2_(unknown1a2)
, mPitch(pitch + 8192)
, mPlayRequested(false)
, mLooped(looped)
, mNonEmitter(nonEmitter)
, mAutoStart(autoStart)
, mOcclusionTest(occlusionTest)
, mAcoustics(acoustics)
, mWorldSfx(worldSfx)
, mSelfFree(false)
, mAllowDuplicates(allowDuplicates)
, mProcessedThisFrame(false)
, mPlayerRelativePan(playerRelativePan)
, x1a9_27_(false)
, mAllAreas(allAreas)
, mScaleByMusicVolume(scaleByMusicVolume) {
  if (mWorldSfx && !mNonEmitter) {
    mWorldSfx = false;
  }
}

void CScriptSound::PreThink(float dt, CStateManager& mgr) {
  CEntity::PreThink(dt, mgr);
  sFirstInFrame = true;
  mProcessedThisFrame = false;
}

CScriptSound::~CScriptSound() {}

void CScriptSound::Think(float dt, CStateManager& mgr) {
  // TODO: lifetime, moving emitters, throttled occlusion, pitch and visor-volume interpolation.
}

void CScriptSound::SetMaxVolume(short volume) {
  mVolume = volume;
  // TODO: update the live emitter/non-emitter handle.
}

void CScriptSound::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CActor::AcceptScriptMsg(mgr, msg);
  switch (msg.GetMessage()) {
  case kSM_Activate:
    if (mAutoStart) {
      mPlayRequested = true;
    }
    break;
  case kSM_Deactivate:
    StopSound(mgr);
    break;
  case kSM_Play:
    if (GetActive()) {
      PlaySound(mgr, &msg);
    }
    break;
  case kSM_Stop:
    if (GetActive()) {
      StopSound(mgr);
    }
    break;
  case kSM_XCRT:
    if (GetActive() && mAutoStart) {
      mPlayRequested = true;
    }
    // TODO: manager generation flag controls self-free behavior.
    break;
  case kSM_XDelete:
    if (!mWorldSfx) {
      StopSound(mgr);
    }
    break;
  case kSM_XALD:
    // TODO: resolve connected sound-position sources.
    break;
  default:
    break;
  }
}

void CScriptSound::PlaySound(CStateManager& mgr, const CScriptMsg* msg) {
  // TODO: emitter/non-emitter setup, multiplayer panning and duplicate suppression.
}

void CScriptSound::StopSound(CStateManager& mgr) {
  mPlayRequested = false;
  if (mWorldSfx && mNonEmitter) {
    mgr.World()->StopGlobalSound(GetSoundId());
    mSfxHandle.Clear();
  } else if (mSfxHandle) {
    CSfxManager::RemoveEmitter(mSfxHandle);
    mSfxHandle.Clear();
  }
}

float CScriptSound::GetOccludedVolumeAmount(const CVector3f& pos, const CStateManager& mgr) {
  if (mgr.fn_80036F10()) {
    return 1.f;
  }
  const CTransform4f camXf = mgr.GetCameraManager(0)->GetCurrentCameraTransform(mgr, true);
  const CVector3f soundToCam = camXf.GetTranslation() - pos;
  const float soundToCamMag = soundToCam.Magnitude();
  const float invMag = 1.f / soundToCamMag;
  const CVector3f soundToCamNorm = soundToCam * invMag;
  const CVector3f up = CVector3f::Up();
  const CVector3f thirdEdge = up - soundToCamNorm * CVector3f::Dot(up, soundToCamNorm);
  const CVector3f cross = CVector3f::Cross(soundToCamNorm, thirdEdge);
  static const float kInfluenceAmount = 3.f / soundToCamMag;
  static const float kInfluenceIncrement = kInfluenceAmount;
  static CMaterialFilter kSolidFilter = CMaterialFilter::MakeIncludeExclude(
      CMaterialList(kMT_Unknown59), CMaterialList(kMT_NoPlatformCollision));
  int totalCount = 0;
  int invalCount = 0;
  for (float i = -kInfluenceAmount; i <= kInfluenceAmount; i += kInfluenceIncrement) {
    for (float j = -kInfluenceAmount; j <= kInfluenceAmount; j += kInfluenceIncrement) {
      ++totalCount;
      const CVector3f rayDir = (soundToCamNorm + i * thirdEdge) + j * cross;
      const CRayCastResult result =
          mgr.RayStaticIntersection(pos, rayDir.AsNormalized(), soundToCamMag, kSolidFilter);
      if (!result.IsValid()) {
        ++invalCount;
      }
    }
  }
  return invalCount / static_cast< float >(totalCount) * (1.f - 0.58f) + 0.58f;
}
