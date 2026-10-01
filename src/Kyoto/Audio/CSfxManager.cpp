#include "Kyoto/Audio/CSfxManager.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CToken.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/math.hpp"

CSfxManager::CSfxChannel CSfxManager::mChannels[4];
rstl::reserved_vector< CSfxManager::SLowPassFilter, 8 > CSfxManager::mAreaLowPassFilters;
rstl::reserved_vector< CSfxManager::SLowPassFilter, 8 > CSfxManager::mLowPassFilters;
CSfxManager::ESfxChannels CSfxManager::mCurrentChannel = kSC_Default;
bool CSfxManager::mDoUpdate = false;
bool CSfxManager::mMuted = false;
rstl::vector< short >* CSfxManager::mpTranslationTable = nullptr;
rstl::auto_ptr< CToken > CSfxManager::mpTranslationTableToken;
rstl::reserved_vector< CSfxManager::CSfxEmitterWrapper, 64 > CSfxManager::mEmitterWrapperPool;
rstl::reserved_vector< CSfxManager::CSfxWrapper, 64 > CSfxManager::mWrapperPool;
rstl::reserved_vector< CSfxPitchBend, 8 > CSfxManager::mPitchBends; // lbl_80413E98
int CSfxManager::mNextAreaFilterId = 0;
int CSfxManager::mNextFilterId = 0;
int CSfxManager::mAreaLowPassFrequency = 16000;
int CSfxManager::mLowPassFrequency = 16000;
rstl::reserved_vector< CSfxManager::SAreaVolume, 10 > CSfxManager::mAreaVolumes(10, SAreaVolume());
int CSfxManager::mCurrentArea = -1;
bool CSfxManager::mCurrentStudio = false;

const short CSfxManager::kMaxPriority = 255;
const short CSfxManager::kMedPriority = 127;
const ushort CSfxManager::kInternalInvalidSfxId = 0xffff;
const int CSfxManager::kAllAreas = -1;

CSfxManager::SListener::SListener() : mActive(false) {}

CSfxManager::CSfxChannel::CSfxChannel() : mListeners(4, SListener()) {}

bool CSfxManager::CSfxEmitterWrapper::IsEmitter() const { return true; }

CSfxManager::CBaseSfxWrapper::CBaseSfxWrapper(bool looped, short priority, CSfxHandle handle,
                                              bool useAcoustics, int area)
: mTimeRemaining(15.f)
, mRank(0)
, mPriority(priority)
, mPitchBend(0x2000)
, mHandle(handle)
, mArea(area)
, mActive(true)
, mPlaying(false)
, mLooped(looped)
, mInArea(true)
, mReleased(false)
, mUseAcoustics(useAcoustics)
, mIgnoreAreaLowPass(false) {}

bool CSfxManager::CBaseSfxWrapper::Available() const { return mReleased; }

void CSfxManager::CBaseSfxWrapper::Release() {
  mReleased = true;
  mTimeRemaining = 15.f;
}

float CSfxManager::CBaseSfxWrapper::GetTimeRemaining() { return mTimeRemaining; }

void CSfxManager::CBaseSfxWrapper::SetTimeRemaining(float time) { mTimeRemaining = time; }

void CSfxManager::CBaseSfxWrapper::SetActive(bool active) { mActive = active; }

void CSfxManager::CBaseSfxWrapper::SetPlaying(bool playing) { mPlaying = playing; }

void CSfxManager::CBaseSfxWrapper::SetInArea(bool inArea) { mInArea = inArea; }

void CSfxManager::CBaseSfxWrapper::SetRank(short rank) { mRank = rank; }

bool CSfxManager::CBaseSfxWrapper::IsLooped() const { return mLooped; }

bool CSfxManager::CBaseSfxWrapper::IsInArea() const { return mInArea; }

bool CSfxManager::CBaseSfxWrapper::IsPlaying() const { return mPlaying; }

bool CSfxManager::CBaseSfxWrapper::IsActive() const { return mActive; }

bool CSfxManager::CBaseSfxWrapper::UseAcoustics() const { return mUseAcoustics; }

int CSfxManager::CBaseSfxWrapper::GetRank() const { return mRank; }

int CSfxManager::CBaseSfxWrapper::GetPriority() const { return mPriority; }

CSfxHandle CSfxManager::CBaseSfxWrapper::GetSfxHandle() const { return mHandle; }

int CSfxManager::CBaseSfxWrapper::GetArea() const { return mArea; }

void CSfxManager::CBaseSfxWrapper::SetPitchBend(ushort pitch) { mPitchBend = pitch; }

ushort CSfxManager::CBaseSfxWrapper::GetPitchBend() const { return mPitchBend; }

bool CSfxManager::CBaseSfxWrapper::GetIgnoreAreaLowPass() const { return mIgnoreAreaLowPass; }

void CSfxManager::CBaseSfxWrapper::SetIgnoreAreaLowPass(bool ignore) {
  mIgnoreAreaLowPass = ignore;
}

CSfxManager::CSfxEmitterWrapper::CSfxEmitterWrapper(bool looped, short priority,
                                                    CAudioSys::C3DEmitterParmData& emitter,
                                                    CSfxHandle handle, bool useAcoustics, int area)
: CBaseSfxWrapper(looped, priority, handle, useAcoustics, area)
, mEmitterData(emitter)
, mEmitterHandle(SND_ID_ERROR)
, mReady(true)
, mUpdatePending(false) {}

void CSfxManager::CSfxEmitterWrapper::SetReverb(char reverb) {
  if (UseAcoustics()) {
    mParameters[mParameterInfo.numPara - 1].paraData.value7 = reverb;
  }
}

void CSfxManager::CSfxEmitterWrapper::Play() {
  mParameterInfo.numPara = 0;
  mParameterInfo.paraArray = mParameters;
  mEmitterData.mStudio = UseAcoustics() ? GetStudio(GetArea()) : 0;
  mParameters[mParameterInfo.numPara].ctrl = SND_MIDICTRL_REVERB;
  if (UseAcoustics()) {
    mParameters[mParameterInfo.numPara].paraData.value7 = GetReverbAmount();
  } else {
    mParameters[mParameterInfo.numPara].paraData.value7 = 0;
  }
  ++mParameterInfo.numPara;

  mEmitterHandle = CAudioSys::S3dAddEmitterParaEx(mEmitterData, GetSfxHandle().GetIndex() & 0xff,
                                                  &mParameterInfo);
  if (mEmitterHandle != SND_ID_ERROR) {
    SetPlaying(true);
  }
  mReady = false;
}

ushort CSfxManager::CSfxEmitterWrapper::GetSfxId() { return mEmitterData.mSfxId; }

bool CSfxManager::CSfxEmitterWrapper::IsSilent() const { return GetEmitter().mMaxVol == 1; }

void CSfxManager::CSfxEmitterWrapper::Stop() {
  if (mEmitterHandle != SND_ID_ERROR) {
    CAudioSys::S3dRemoveEmitter(mEmitterHandle);
    SetPlaying(false);
    mEmitterHandle = SND_ID_ERROR;
  }
}

CAudioSys::C3DEmitterParmData& CSfxManager::CSfxEmitterWrapper::GetEmitter() {
  return mEmitterData;
}

const CAudioSys::C3DEmitterParmData& CSfxManager::CSfxEmitterWrapper::GetEmitter() const {
  return mEmitterData;
}

uint CSfxManager::CSfxEmitterWrapper::GetHandle() const { return mEmitterHandle; }

bool CSfxManager::CSfxEmitterWrapper::IsPlaying() const {
  if (!IsLooped()) {
    bool ret = false;
    if (CBaseSfxWrapper::IsPlaying() && CAudioSys::S3dCheckEmitter(mEmitterHandle)) {
      ret = true;
    }
    return ret;
  }
  return CBaseSfxWrapper::IsPlaying();
}

bool CSfxManager::CSfxEmitterWrapper::Ready() { return IsLooped() || mReady; }

short CSfxManager::CSfxEmitterWrapper::GetAudible(const CVector3f& position) {
  const float distanceSquared = (mEmitterData.mPos - position).MagSquared();
  const float maxDistanceSquared = mEmitterData.mMaxDist * mEmitterData.mMaxDist;
  short audible = kSA_Aud0;
  if (distanceSquared < maxDistanceSquared * 0.25f) {
    audible = kSA_Aud3;
  } else if (distanceSquared < maxDistanceSquared * 0.5f) {
    audible = kSA_Aud2;
  } else {
    const int near = distanceSquared < maxDistanceSquared;
    audible = static_cast< short >(near);
  }
  return audible;
}

SND_VOICEID CSfxManager::CSfxEmitterWrapper::GetVoice() const {
  return IsPlaying() ? CAudioSys::S3dEmitterVoiceID(mEmitterHandle) : SND_ID_ERROR;
}

void CSfxManager::CSfxEmitterWrapper::UpdateEmitterSilent() {
  if (mEmitterData.mMaxVol != 1) {
    mCachedMaxVolume = mEmitterData.mMaxVol;
    mEmitterData.mMaxVol = 1;
    CAudioSys::S3dUpdateEmitter(mEmitterHandle, mEmitterData.mPos, mEmitterData.mDir, 1);
  }
}

void CSfxManager::CSfxEmitterWrapper::UpdateEmitter() { mUpdatePending = true; }

CSfxManager::CSfxWrapper::CSfxWrapper(bool looped, short priority, ushort sfxId, short volume,
                                      short pan, CSfxHandle handle, bool useAcoustics, int area)
: CBaseSfxWrapper(looped, priority, handle, useAcoustics, area)
, mSfxId(sfxId)
, mVoiceHandle(SND_ID_ERROR)
, mVolume(static_cast< short >(volume))
, mPan(static_cast< short >(pan))
, mReady(true) {}

void CSfxManager::CSfxWrapper::SetReverb(char reverb) {
  if (UseAcoustics()) {
    CAudioSys::SfxCtrl(mVoiceHandle, SND_MIDICTRL_REVERB, reverb);
  }
}

void CSfxManager::CSfxWrapper::Play() {
  const uchar studio = UseAcoustics() ? GetStudio(GetArea()) : 0;
  mVoiceHandle = CAudioSys::SfxStart(mSfxId, 127, mPan, studio);
  CAudioSys::SfxVolume(mVoiceHandle, mVolume);
  if (mVoiceHandle != SND_ID_ERROR) {
    if (UseAcoustics()) {
      CAudioSys::SfxCtrl(mVoiceHandle, SND_MIDICTRL_REVERB, GetReverbAmount());
    }
    SetPlaying(true);
    if (IsLowPassAreaFilterEnabled() && GetArea() != kAllAreas) {
      CAudioSys::SfxSetFilter(mVoiceHandle, 1, GetLowPassAreaFrequency());
    }
  }
  mReady = false;
}

ushort CSfxManager::CSfxWrapper::GetSfxId() { return mSfxId; }

void CSfxManager::CSfxWrapper::Stop() {
  if (mVoiceHandle != SND_ID_ERROR) {
    CAudioSys::SfxStop(mVoiceHandle);
    SetPlaying(false);
    mVoiceHandle = SND_ID_ERROR;
  }
}

bool CSfxManager::CSfxWrapper::IsPlaying() const {
  return CBaseSfxWrapper::IsPlaying() && CAudioSys::SfxCheck(mVoiceHandle) != SND_ID_ERROR;
}

bool CSfxManager::CSfxWrapper::Ready() { return IsLooped() || mReady; }

short CSfxManager::CSfxWrapper::GetAudible(const CVector3f&) { return kSA_Aud3; }

SND_VOICEID CSfxManager::CSfxWrapper::GetVoice() const { return mVoiceHandle; }

void CSfxManager::CSfxWrapper::SetVolume(short volume) { mVolume = volume; }

void CSfxManager::CSfxWrapper::UpdateEmitterSilent() { CAudioSys::SfxVolume(mVoiceHandle, 1); }

void CSfxManager::CSfxWrapper::UpdateEmitter() { CAudioSys::SfxVolume(mVoiceHandle, mVolume); }

/**
 * `config/G2ME01/symbols.txt` gives this name to `.text 0x8029EFCC`, `size:0x54` = 84 bytes, and
 * the 22 instructions there are not an `Initialize`:
 *
 * ```
 * 8029efd4:  lis r4,lbl_80411068 ; lis r3,lbl_804152DC
 * 8029efe0:  addi r4,r4,4200    ; 0x80411068
 * 8029efe4:  addi r6,r4,876     ; 0x804113D4 = &lbl_80411068[0x36C/4]
 * 8029efe8:  li   r5,0
 * 8029efec:  lwz  r0,876(r4)    ; the count
 * 8029eff4:  slwi r0,r0,2
 * 8029eff8:  add  r4,r6,r0
 * 8029effc:  stw  r5,4(r4)      ; table[count + 1] = 0
 * 8029f000:  lwz  r4,0(r6)
 * 8029f004:  addi r0,r4,1
 * 8029f008:  stw  r0,0(r6)      ; ++count
 * 8029f00c:  bl   fn_80340F9C   ; fn_80340F9C(lbl_804152DC)
 * ```
 *
 * It bumps a counter at the end of a `.data` table, clears the following word and registers a
 * name with `fn_80340F9C` - the shape of a C++ **static initialiser**, not of anything a manager
 * does. The name is upstream's guess and the bytes do not support it, so `CSfxManager::Initialize`
 * is **still unwritten**; what is written here is the function the matching build has at
 * 0x8029EFCC, which is what recovers the address. It was `src/MetroidPrime/CMiscTableInit.cpp` on
 * master (named `fn_8029EFCC` there), and that file keeps its own copy for the port build, which
 * does not compile this one.
 *
 * Nothing calls `CSfxManager::Initialize` - `include/Kyoto/Audio/CSfxManager.hpp` marks it
 * "Guessed name" - so the host keeps the body it had.
 */
#ifdef TARGET_PC
void CSfxManager::Initialize() {
  mChannels[kSC_Game].mSounds.push_back(nullptr);
  // TODO: initialize the Echoes auxiliary-effect manager.
}
#else
extern "C" {
extern int lbl_80411068[];
extern char lbl_804152DC[];
extern void fn_80340F9C(void*);
extern void fn_80340C80(void*);
extern void fn_80340E08(void*);
}
enum { kTableCount = 0x36C / 4 };

void CSfxManager::Initialize() {
  int* count = &lbl_80411068[kTableCount];
  count[1 + *count] = 0;
  *count = *count + 1;
  fn_80340F9C(lbl_804152DC);
}
#endif // TARGET_PC

void CSfxManager::Shutdown() {
  delete mpTranslationTable;
  mpTranslationTable = nullptr;
  StopAndRemoveAllEmitters();
  // TODO: shut down auxiliary effects and release active effect records.
}

void CSfxManager::StopAndRemoveAllEmitters() {
  for (int i = 0; i < 4; ++i) {
    CSfxChannel& channel = mChannels[i];
    for (int j = 0; j < channel.mSounds.size(); ++j) {
      if (channel.mSounds[j] != nullptr) {
        if (channel.mSounds[j]->IsPlaying()) {
          channel.mSounds[j]->Stop();
        }
        channel.mSounds[j]->Release();
        channel.mSounds[j] = nullptr;
      }
    }
  }
}

CSfxManager::CSfxListener::CSfxListener(CVector3f position, CVector3f direction, CVector3f heading,
                                        CVector3f up, float frontSur, float backSur,
                                        float soundSpeed, uint flags, uchar maxVolume)
: mPosition(position)
, mDirection(direction)
, mHeading(heading)
, mUp(up)
, mFrontSur(frontSur)
, mBackSur(backSur)
, mSoundSpeed(soundSpeed)
, mFlags(flags)
, mMaxVolume(maxVolume) {}

void CSfxManager::AddListener(ESfxChannels channel, const CVector3f& position,
                              const CVector3f& direction, const CVector3f& heading,
                              const CVector3f& up, float frontSur, float backSur, float soundSpeed,
                              uint flags, uchar maxVolume, int listener) {
  SListener& entry = mChannels[channel].mListeners[listener];
  entry.mListener = CSfxListener(position, direction, heading, up, frontSur, backSur, soundSpeed,
                                 flags, maxVolume);
  entry.mActive = true;
  CAudioSys::S3dAddListener(position, direction, heading, up, frontSur, backSur, soundSpeed, flags,
                            maxVolume);
}

void CSfxManager::UpdateListener(const CVector3f& position, const CVector3f& direction,
                                 const CVector3f& heading, const CVector3f& up, uchar maxVolume,
                                 int listener) {
  SListener& entry = mChannels[mCurrentChannel].mListeners[listener];
  entry.mListener.mPosition = position;
  entry.mListener.mDirection = direction;
  entry.mListener.mHeading = heading;
  entry.mListener.mUp = up;
  entry.mListener.mMaxVolume = maxVolume;
  entry.mActive = true;
}

CSfxHandle CSfxManager::AddEmitter(ushort id, const CVector3f& position, int area,
                                   bool useAcoustics, bool looped, short priority) {
  CAudioSys::C3DEmitterParmData params(150.f, 0.1f, 1, 127, 20);
  params.mPos = position;
  params.mDir = CVector3f::Zero();
  params.mSfxId = id;
  return AddEmitter(params, area, useAcoustics, looped, priority);
}

CSfxHandle CSfxManager::AddEmitter(ushort id, const CVector3f& position, uchar volume, int area,
                                   bool useAcoustics, bool looped, short priority) {
  CAudioSys::C3DEmitterParmData params(150.f, 0.1f, 1, volume > 20 ? volume : 21, 20);
  params.mPos = position;
  params.mDir = CVector3f::Zero();
  params.mSfxId = id;
  return AddEmitter(params, area, useAcoustics, looped, priority);
}

CSfxManager::CSfxEmitterWrapper::~CSfxEmitterWrapper() {}

CSfxHandle CSfxManager::AddEmitter(CAudioSys::C3DEmitterParmData& params, int area,
                                   bool useAcoustics, bool looped, short priority) {
  if ((mMuted && !looped) || params.mSfxId == 0xFFFFFFFF ||
      params.mSfxId == kInternalInvalidSfxId) {
    return CSfxHandle::NullHandle();
  }
  CAudioSys::C3DEmitterParmData emitter(params);
  if (looped) {
    emitter.mFlags |= 6;
  }
  emitter.mSfxId = TranslateSFXID(params.mSfxId);
  const uchar areaVolume = GetAreaVolume(area);
  if (areaVolume != 127) {
    emitter.mMaxVol = areaVolume * rstl::min_val(int(emitter.mMaxVol), 127) / 127;
  }
  if (emitter.mSfxId == 0xFFFF) {
    return CSfxHandle::NullHandle();
  }

  mDoUpdate = true;
  CSfxChannel& channel = mChannels[mCurrentChannel];
  const CSfxHandle handle = LocateHandle();
  if (handle) {
    CSfxEmitterWrapper wrapper(looped, priority, emitter, handle, useAcoustics, area);
    CSfxEmitterWrapper* sound = AllocateCSfxEmitterWrapper(wrapper);
    if (mMuted) {
      sound->UpdateEmitterSilent();
    }
    channel.mSounds[handle.GetIndex()] = sound;
  }
  return handle;
}

void CSfxManager::UpdateEmitter(CSfxHandle handle, const CVector3f& position,
                                const CVector3f& direction, uchar maxVolume) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CSfxEmitterWrapper* sound =
      static_cast< CSfxEmitterWrapper* >(channel.mSounds[handle.GetIndex()]);
  if (sound == nullptr || handle != sound->GetSfxHandle() || !sound->IsPlaying()) {
    return;
  }
  mDoUpdate = true;
  if (sound->IsSilent()) {
    sound->GetEmitter().mPos = position;
    sound->GetEmitter().mDir = direction;
  } else {
    sound->GetEmitter().mPos = position;
    sound->GetEmitter().mDir = direction;
    const uchar areaVolume = GetAreaVolume(sound->GetArea());
    // Retail tests the neutral 127 first and clamps the result to a byte, then floors at 2.
    // The unsigned compares and the uchar/short narrowing are load-bearing: signed ones and
    // wider types change which of the two clamps MWCC can fold.
    uchar scaled = maxVolume;
    if (areaVolume == 127) {
      scaled = maxVolume;
    } else {
      const int capped = maxVolume > 127u ? 127 : int(maxVolume);
      scaled = uchar(areaVolume * capped / 127);
    }
    sound->GetEmitter().mMaxVol = uint(scaled) > 2u ? scaled : uchar(2);
  }
}

void CSfxManager::RemoveEmitter(CSfxHandle handle) { StopSound(mCurrentChannel, handle); }

CSfxHandle CSfxManager::SfxStart(ushort id, short volume, short pan, int area, bool useAcoustics,
                                 bool looped, short priority) {
  if ((mMuted && !looped) || id == 0xFFFFFFFF || id == kInternalInvalidSfxId) {
    return CSfxHandle::NullHandle();
  }
  mDoUpdate = true;
  const CSfxHandle handle = LocateHandle();
  if (handle) {
    const ushort translatedId = TranslateSFXID(id);
    if (translatedId == kInternalInvalidSfxId) {
      return CSfxHandle::NullHandle();
    }
    const uchar areaVolume = GetAreaVolume(area);
    if (areaVolume != 127) {
      volume = areaVolume * rstl::min_val(int(uchar(volume)), 127) / 127;
    }
    mChannels[mCurrentChannel].mSounds[handle.GetIndex()] = AllocateCSfxWrapper(CSfxWrapper(
        looped, priority, translatedId, uchar(volume), pan, handle, useAcoustics, area));
  }
  return handle;
}

void CSfxManager::SfxStop(CSfxHandle handle) { StopSound(mCurrentChannel, handle); }

void CSfxManager::SfxStop(ESfxChannels channel, CSfxHandle handle) { StopSound(channel, handle); }

void CSfxManager::SfxVolume(CSfxHandle handle, uchar volume) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CSfxWrapper* sound = static_cast< CSfxWrapper* >(channel.mSounds[handle.GetIndex()]);
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  const uchar areaVolume = GetAreaVolume(sound->GetArea());
  // As in UpdateEmitter: the neutral-127 test comes first, the scaled value is narrowed to a
  // short, and the 1..127 clamp below uses unsigned compares.
  short scaled = volume;
  if (areaVolume == 127) {
    scaled = volume;
  } else {
    const int capped = volume > 127u ? 127 : int(volume);
    scaled = short(areaVolume * capped / 127);
  }
  volume = scaled;
  uchar vol = volume < 1u ? uchar(1) : (volume > 127u ? uchar(127) : volume);
  sound->SetVolume(vol);
  if (!mMuted && sound->IsPlaying()) {
    CAudioSys::SfxVolume(sound->GetVoice(), vol);
  }
}

void CSfxManager::SfxPan(CSfxHandle handle, uchar pan) {
  if (!handle) {
    return;
  }
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  if (!sound->IsPlaying()) {
    Update(0.f);
  }
  if (sound->IsPlaying()) {
    CAudioSys::SfxPan(sound->GetVoice(), pan);
  }
}

void CSfxManager::SfxSpan(CSfxHandle handle, uchar span) {
  if (!handle) {
    return;
  }
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  if (!sound->IsPlaying()) {
    Update(0.f);
  }
  if (sound->IsPlaying()) {
    CAudioSys::SfxSpan(sound->GetVoice(), span);
  }
}

void CSfxManager::KillAll(ESfxChannels channel) {
  CSfxChannel& sounds = mChannels[channel];
  for (int i = 0; i < sounds.mSounds.size(); ++i) {
    CBaseSfxWrapper* sound = sounds.mSounds[i];
    if (sound != nullptr && sound->IsPlaying()) {
      sound->Stop();
    }
    if (sound != nullptr) {
      sound->Release();
    }
    sounds.mSounds[i] = nullptr;
  }
#ifndef TARGET_PC
  // Retail tears the game channel's auxiliary-effect records down and re-registers the
  // auxiliary-effect manager (lbl_804152DC, the same receiver fn_80340F9C is constructed
  // with in Initialize) when the killed channel is the game channel.
  if (channel == kSC_Game) {
    fn_80340E08(lbl_804152DC);
    fn_80340F9C(lbl_804152DC);
  }
#else
  // The port never constructs the auxiliary-effect manager (see Initialize), so there are
  // no records to tear down and retail would find nothing to do here.
#endif
}

void CSfxManager::StopSound(ESfxChannels channel, CSfxHandle handle) {
  CSfxChannel& sounds = mChannels[channel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= sounds.mSounds.size()) {
    if (channel != kSC_Game) {
      StopSound(kSC_Game, handle);
    }
    return;
  }
  CBaseSfxWrapper* sound = sounds.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    if (channel != kSC_Game) {
      StopSound(kSC_Game, handle);
    }
    return;
  }
  mDoUpdate = true;
  if (sound->IsPlaying()) {
    sound->Stop();
  }
  sound->Release();
  sounds.mSounds[handle.GetIndex()] = nullptr;
}

void CSfxManager::SetDuration(CSfxHandle handle, float duration) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  sound->SetTimeRemaining(duration);
}

void CSfxManager::SetChannel(ESfxChannels channel) {
  const ESfxChannels current = mCurrentChannel;
  if (channel == current) {
    return;
  }
  if (channel == kSC_Default) {
    mAreaLowPassFilters.clear();
    mLowPassFilters.clear();
  }
  if (current != kSC_Invalid) {
    TurnOffChannel(current);
  }
  TurnOnChannel(channel);
  mCurrentChannel = channel;
}

CSfxManager::ESfxChannels CSfxManager::GetChannel() { return mCurrentChannel; }

void CSfxManager::TurnOffChannel(ESfxChannels channel) {
  CSfxChannel& sounds = mChannels[channel];
  for (int i = 0; i < sounds.mSounds.size(); ++i) {
    if (sounds.mSounds[i] == nullptr) {
      continue;
    }
    if (sounds.mSounds[i]->IsLooped()) {
      sounds.mSounds[i]->UpdateEmitterSilent();
    } else {
      sounds.mSounds[i]->Stop();
    }
  }
  for (int i = 0; i < sounds.mSounds.size(); ++i) {
    if (sounds.mSounds[i] == nullptr || sounds.mSounds[i]->IsLooped()) {
      continue;
    }
    sounds.mSounds[i]->Release();
    sounds.mSounds[i] = nullptr;
  }
}

void CSfxManager::TurnOnChannel(ESfxChannels channel) {
  CSfxChannel& sounds = mChannels[channel];
  mCurrentChannel = channel;
  mDoUpdate = true;
  // Retail tests all four listener slots, so the update runs when any of them is active.
  bool anyActive = false;
  for (int i = 0; i < 4; ++i) {
    if (sounds.mListeners[i].mActive) {
      anyActive = true;
      break;
    }
  }
  if (anyActive) {
    for (int j = 0; j < sounds.mSounds.size(); ++j) {
      if (sounds.mSounds[j] != nullptr) {
        sounds.mSounds[j]->UpdateEmitter();
      }
    }
  }
}

CSfxHandle CSfxManager::LocateHandle() {
  CSfxChannel& channel = mChannels[mCurrentChannel];
  for (int i = 0; i < channel.mSounds.size(); ++i) {
    if (channel.mSounds[i] == nullptr) {
      return CSfxHandle(i);
    }
  }
  if (channel.mSounds.size() == channel.mSounds.capacity()) {
    return CSfxHandle::NullHandle();
  }
  channel.mSounds.push_back(nullptr);
  return CSfxHandle(channel.mSounds.size() - 1);
}

// 0x8029CD44
void CSfxManager::Update(float dt) {
  CSfxChannel& chan = mChannels[mCurrentChannel];
  ushort count = 0;
  ushort order[72];

  // Expire timed one-shots. Retail re-indexes mSounds at every use instead of keeping the
  // element live across a call, so there is deliberately no cached `sound` local here.
  for (short i = 0; i < chan.mSounds.size(); ++i) {
    if (chan.mSounds[i] == nullptr || chan.mSounds[i]->IsLooped()) {
      continue;
    }
    const float remaining = chan.mSounds[i]->GetTimeRemaining();
    chan.mSounds[i]->SetTimeRemaining(remaining - dt);
    if (remaining < 0.f) {
      chan.mSounds[i]->Stop();
      mDoUpdate = true;
    }
  }

  if (mDoUpdate) {
    for (short i = 0; i < chan.mSounds.size(); ++i) {
      if (chan.mSounds[i] != nullptr) {
        order[count++] = i;
        chan.mSounds[i]->SetRank(GetRank(chan.mSounds[i]));
      }
    }

    // Bubble sort, highest rank first.
    for (short i = 0; i < count; ++i) {
      bool sorted = true;
      for (int j = 0; j < count - 1; ++j) {
        if (chan.mSounds[order[j]]->GetRank() < chan.mSounds[order[j + 1]]->GetRank()) {
          sorted = false;
          const ushort tmp = order[j];
          order[j] = order[j + 1];
          order[j + 1] = tmp;
        }
      }
      if (sorted) {
        break;
      }
    }

    // Only the 48 best-ranked sounds may keep a voice.
    for (short i = 48; i < count; ++i) {
      if (chan.mSounds[order[i]] != nullptr && chan.mSounds[order[i]]->IsPlaying()) {
        chan.mSounds[order[i]]->Stop();
      }
    }
    for (short i = 0; i < count; ++i) {
      if (chan.mSounds[order[i]] != nullptr && chan.mSounds[order[i]]->IsPlaying() &&
          !chan.mSounds[order[i]]->IsInArea()) {
        chan.mSounds[order[i]]->Stop();
      }
    }
  }

  CAudioSys::S3dFlushUnusedEmitters();

  if (mDoUpdate && !mMuted) {
    int slots = 48;
    for (int i = 0; i < count && slots != 0; ++i) {
      if (chan.mSounds[order[i]] == nullptr) {
        continue;
      }
      if (chan.mSounds[order[i]]->IsPlaying()) {
        --slots;
      } else if (chan.mSounds[order[i]]->Ready() && chan.mSounds[order[i]]->IsInArea()) {
        chan.mSounds[order[i]]->Play();
        --slots;
      }
    }
    mDoUpdate = false;
  }

  // Release finished one-shots.
  for (int i = 0; i < chan.mSounds.size(); ++i) {
    if (chan.mSounds[i] != nullptr && !chan.mSounds[i]->IsPlaying() &&
        !chan.mSounds[i]->IsLooped()) {
      chan.mSounds[i]->Release();
      chan.mSounds[i] = nullptr;
      mDoUpdate = true;
    }
  }

  // Finish a pending LoadTranslationTable once its token has loaded.
  if (mpTranslationTableToken.get() != nullptr && mpTranslationTableToken->HasLock() &&
      mpTranslationTableToken->IsLoaded()) {
    if (mpTranslationTable == nullptr) {
      CToken token(*mpTranslationTableToken);
      mpTranslationTable = rs_new rstl::vector< short >(
          *static_cast< rstl::vector< short >* >(token.GetObj()->GetContents()));
    }
    mpTranslationTableToken = rstl::auto_ptr< CToken >();
  }

  if (!(CMath::AbsF(dt - 0.f) < 0.00001f)) {
    UpdatePitchBends(dt);
  }

  // The first active listener drives MusyX's single 3D listener. Retail scans all four
  // slots regardless of mListeners.size().
  SListener* listeners = chan.mListeners.data();
  int first = -1;
  for (int i = 0; i < 4; ++i) {
    if (listeners[i].mActive) {
      first = i;
      const CSfxListener& listener = listeners[i].mListener;
      CAudioSys::S3dUpdateListener(listener.mPosition, listener.mDirection, listener.mHeading,
                                   listener.mUp, listener.mMaxVolume);
      break;
    }
  }

  if (first != -1) {
    rstl::reserved_vector< CVector3f, 4 > rights;
    for (int i = first; i < 4; ++i) {
      if (listeners[i].mActive) {
        rights.push_back(
            CVector3f::Cross(listeners[i].mListener.mHeading, listeners[i].mListener.mUp)
                .AsNormalized());
      } else {
        rights.push_back(CVector3f::Right());
      }
    }

    // Retail bug, kept: rights[] is filled starting at listener `first` but indexed below by
    // absolute listener number. Identical whenever listener 0 is the active one.
    const CSfxListener& primary = listeners[first].mListener;
    const CVector3f& mainRight = rights.data()[first];

    // An emitter nearer to another active listener is re-expressed relative to the primary
    // listener, keeping its heading/right/up offsets from the nearer one.
    for (int i = 0; i < chan.mSounds.size(); ++i) {
      CBaseSfxWrapper* sound = chan.mSounds[i];
      if (sound == nullptr || sound->IsEmitter() != true || !sound->IsPlaying()) {
        continue;
      }
      CSfxEmitterWrapper* emitter = static_cast< CSfxEmitterWrapper* >(sound);
      if (emitter->IsSilent() && !emitter->mUpdatePending) {
        continue;
      }
      const CVector3f& pos = emitter->GetEmitter().mPos;
      const CVector3f& dir = emitter->GetEmitter().mDir;
      uchar maxVol = emitter->GetEmitter().mMaxVol;
      if (emitter->mUpdatePending) {
        maxVol = emitter->mCachedMaxVolume;
        emitter->mUpdatePending = false;
        emitter->GetEmitter().mMaxVol = maxVol;
      }

      int nearest = -1;
      float minDistSq = 3.4028235e38f;
      for (int j = first; j < 4; ++j) {
        if (listeners[j].mActive) {
          const float distSq = (listeners[j].mListener.mPosition - pos).MagSquared();
          if (distSq < minDistSq) {
            minDistSq = distSq;
            nearest = j;
          }
        }
      }

      if (nearest == first || nearest == -1) {
        CAudioSys::S3dUpdateEmitter(emitter->GetHandle(), pos, dir, maxVol);
      } else {
        const CSfxListener& closer = listeners[nearest].mListener;
        const CVector3f& nearRight = rights.data()[nearest];
        const CVector3f rel = pos - closer.mPosition;
        const float alongHeading = CVector3f::Dot(closer.mHeading, rel);
        const float alongRight = CVector3f::Dot(nearRight, rel);
        const float alongUp = CVector3f::Dot(closer.mUp, rel);
        const CVector3f newPos = primary.mPosition + primary.mHeading * alongHeading +
                                 mainRight * alongRight + primary.mUp * alongUp;
        CVector3f newDir;
        if (dir.IsNonZero()) {
          newDir = primary.mHeading * CVector3f::Dot(closer.mHeading, dir) +
                   mainRight * CVector3f::Dot(nearRight, dir) +
                   primary.mUp * CVector3f::Dot(closer.mUp, dir);
        } else {
          newDir = CVector3f::Zero();
        }
        CAudioSys::S3dUpdateEmitter(emitter->GetHandle(), newPos, newDir, maxVol);
      }
    }
  }

  UpdateLowPassAreaFilters(dt);
  UpdateLowPassFilters(dt);

  // Retail reads mChannels[kSC_Game] directly here, not `chan`.
  if (mCurrentChannel == kSC_Game) {
    CSfxChannel& game = mChannels[kSC_Game];
    for (int i = 0; i < game.mSounds.size(); ++i) {
      CBaseSfxWrapper* sound = game.mSounds[i];
      if (sound == nullptr || !sound->IsPlaying()) {
        continue;
      }
      const int area = sound->GetArea();
      const bool useAcoustics = sound->UseAcoustics();
      if (area != kAllAreas || useAcoustics) {
        const uint lowPass = ShouldApplyLowPass(sound) ? 1 : 0;
        const int frequency = GetLowPassFrequency(sound);
        CAudioSys::SfxSetFilter(sound->GetVoice(), lowPass, frequency);
      }
      CAudioSys::SfxPitchBend(sound->GetVoice(), sound->GetPitchBend());
    }
  }

#ifndef TARGET_PC
  fn_80340C80(lbl_804152DC); // Echoes auxiliary-effect manager update
#else
  // Retail updates the auxiliary-effect manager (fn_80340C80(lbl_804152DC)): it disables a
  // bus once none of its effects is active. The port never constructs that manager (see
  // Initialize), so its pending flags stay clear and retail would do nothing here.
#endif
}

// 0x8029B608
void CSfxManager::AddPitchBend(const CSfxPitchBend& pitchBend) {
  if (mPitchBends.size() < mPitchBends.capacity()) {
    mPitchBends.push_back(pitchBend);
  }
}

// 0x8029B664
void CSfxManager::UpdatePitchBends(float dt) {
  if (mCurrentChannel != kSC_Game) {
    return;
  }
  rstl::reserved_vector< CSfxPitchBend, 8 >::iterator it = mPitchBends.begin();
  while (it != mPitchBends.end()) {
    it->Update(dt);
    PitchBend(it->GetHandle(), it->GetPitch());
    const bool queued = IsQueued(it->GetHandle());
    if (it->IsFinished() || !queued) {
      it = mPitchBends.erase(it);
    } else {
      ++it;
    }
  }
}

void CSfxManager::PitchBend(CSfxHandle handle, int pitch) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (!handle) {
    return;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  sound->SetPitchBend(pitch);
  mDoUpdate = true;
}

bool CSfxManager::IsPlaying(CSfxHandle handle) {
  if (!handle) {
    return false;
  }
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return false;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle() || !sound->IsPlaying()) {
    return false;
  }
  return sound->IsPlaying();
}

bool CSfxManager::IsQueued(CSfxHandle handle) {
  if (!handle) {
    return false;
  }
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return false;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return false;
  }
  return true;
}

int CSfxManager::GetRank(CBaseSfxWrapper* sound) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (!sound->IsInArea()) {
    return 0;
  }
  int rank = sound->GetPriority() >> 2;
  if (sound->IsPlaying()) {
    ++rank;
  }
  if (sound->IsLooped()) {
    rank -= 2;
  }
  if (sound->Ready() && !sound->IsPlaying()) {
    rank += 3;
  }
  // Retail scans all four listener slots regardless of mListeners.size().
  for (int i = 0; i < 4; ++i) {
    if (!channel.mListeners[i].mActive) {
      continue;
    }
    const int audible = sound->GetAudible(channel.mListeners[i].mListener.mPosition);
    if (audible == kSA_Aud0) {
      rank = 0;
    } else {
      rank += audible * 2;
    }
  }
  return rank;
}

bool CSfxManager::LoadTranslationTable(CSimplePool* pool, const SObjectTag* tag) {
  if (!tag) {
    return false;
  }
  if (mpTranslationTable) {
    delete mpTranslationTable;
  }
  mpTranslationTable = nullptr;
  mpTranslationTableToken = rs_new CToken(pool->GetObj(*tag));
  mpTranslationTableToken->Lock();
  return true;
}

ushort CSfxManager::TranslateSFXID(ushort id) {
  if (mpTranslationTable == nullptr || id >= mpTranslationTable->size()) {
    return kInternalInvalidSfxId;
  }
  short translated = (*mpTranslationTable)[id];
  if (translated < 0) {
    return kInternalInvalidSfxId;
  }
  return static_cast< ushort >(translated);
}

void CSfxManager::SetActiveAreas(const rstl::reserved_vector< int, 10 >& areas, int currentArea) {
  // TODO: reconcile auxiliary effects and per-area volumes, swap studios, then update
  // each sound's in-area flag. The auxiliary-effect record is not yet reconstructed.
}

CSfxManager::CSfxEmitterWrapper*
CSfxManager::AllocateCSfxEmitterWrapper(const CSfxEmitterWrapper& sound) {
  CSfxEmitterWrapper* ret = nullptr;
  for (int i = 0; i < mEmitterWrapperPool.size(); ++i) {
    if (mEmitterWrapperPool[i].Available()) {
      mEmitterWrapperPool[i] = sound;
      ret = &mEmitterWrapperPool[i];
      break;
    }
  }
  if (ret == nullptr && mEmitterWrapperPool.size() != mEmitterWrapperPool.capacity()) {
    mEmitterWrapperPool.push_back(sound);
    ret = &mEmitterWrapperPool.back();
  }
  return ret;
}

CSfxManager::CSfxWrapper* CSfxManager::AllocateCSfxWrapper(const CSfxWrapper& sound) {
  CSfxWrapper* ret = nullptr;
  for (int i = 0; i < mWrapperPool.size(); ++i) {
    if (mWrapperPool[i].Available()) {
      mWrapperPool[i] = sound;
      ret = &mWrapperPool[i];
      break;
    }
  }
  if (ret == nullptr && mWrapperPool.size() != mWrapperPool.capacity()) {
    mWrapperPool.push_back(sound);
    ret = &mWrapperPool.back();
  }
  return ret;
}

void CSfxManager::SetMuted(bool muted) {
  CSfxChannel& channel = mChannels[mCurrentChannel];
  mDoUpdate = true;
  mMuted = muted;
  if (muted) {
    for (int i = 0; i < channel.mSounds.size(); ++i) {
      if (channel.mSounds[i] == nullptr) {
        continue;
      }
      if (channel.mSounds[i]->IsLooped()) {
        channel.mSounds[i]->UpdateEmitterSilent();
      } else {
        channel.mSounds[i]->Stop();
      }
    }
    for (int i = 0; i < channel.mSounds.size(); ++i) {
      if (channel.mSounds[i] != nullptr && !channel.mSounds[i]->IsLooped()) {
        channel.mSounds[i]->Release();
        channel.mSounds[i] = nullptr;
      }
    }
  } else {
    for (int i = 0; i < channel.mSounds.size(); ++i) {
      if (channel.mSounds[i] != nullptr) {
        channel.mSounds[i]->UpdateEmitter();
      }
    }
  }
}

short CSfxManager::GetReverbAmount() { return 127; }

static const uchar sStudios[] = {1, 2};

int CSfxManager::GetStudio(int area) {
  const uchar* studios = sStudios;
  if (area == kAllAreas || area == mCurrentArea) {
    return studios[mCurrentStudio];
  }
  return studios[!mCurrentStudio];
}

int CSfxManager::AddLowPassAreaFilter(int frequency, float duration) {
  if (mAreaLowPassFilters.size() < mAreaLowPassFilters.capacity()) {
    ++mNextAreaFilterId;
    if (mNextAreaFilterId == 0) {
      mNextAreaFilterId = 1;
    }
    mAreaLowPassFilters.push_back(SLowPassFilter(frequency, duration, mNextAreaFilterId));
    return mNextAreaFilterId;
  }
  return 0;
}

void CSfxManager::UpdateLowPassAreaFilters(float dt) {
  mAreaLowPassFrequency = 44100;
  bool haveFrequency = false;
  rstl::reserved_vector< SLowPassFilter, 8 >::iterator it = mAreaLowPassFilters.begin();
  while (it != mAreaLowPassFilters.end()) {
    it->mTimeRemaining -= dt;
    const bool keep = !it->mTimed || it->mTimeRemaining > 0.f;
    if (!keep) {
      it = mAreaLowPassFilters.erase(it);
      continue;
    }
    if (!haveFrequency || it->mFrequency < mAreaLowPassFrequency) {
      mAreaLowPassFrequency = it->mFrequency;
      haveFrequency = true;
    }
    ++it;
  }
}

void CSfxManager::RemoveLowPassAreaFilter(int id) {
  if (id == 0) {
    return;
  }
  for (rstl::reserved_vector< SLowPassFilter, 8 >::iterator it = mAreaLowPassFilters.begin();
       it != mAreaLowPassFilters.end(); ++it) {
    if (id == it->mId) {
      mAreaLowPassFilters.erase(it);
      return;
    }
  }
}

bool CSfxManager::IsLowPassAreaFilterEnabled() {
  return mAreaLowPassFilters.size() > 0 && mCurrentChannel == kSC_Game;
}

int CSfxManager::GetLowPassAreaFrequency() { return mAreaLowPassFrequency; }

int CSfxManager::AddLowPassFilter(int frequency, float duration) {
  if (mLowPassFilters.size() < mLowPassFilters.capacity()) {
    ++mNextFilterId;
    if (mNextFilterId == 0) {
      mNextFilterId = 1;
    }
    mLowPassFilters.push_back(SLowPassFilter(frequency, duration, mNextFilterId));
    return mNextFilterId;
  }
  return 0;
}

void CSfxManager::UpdateLowPassFilters(float dt) {
  mLowPassFrequency = 44100;
  bool haveFrequency = false;
  rstl::reserved_vector< SLowPassFilter, 8 >::iterator it = mLowPassFilters.begin();
  while (it != mLowPassFilters.end()) {
    it->mTimeRemaining -= dt;
    const bool keep = !it->mTimed || it->mTimeRemaining > 0.f;
    if (!keep) {
      it = mLowPassFilters.erase(it);
      continue;
    }
    if (!haveFrequency || it->mFrequency < mLowPassFrequency) {
      mLowPassFrequency = it->mFrequency;
      haveFrequency = true;
    }
    ++it;
  }
}

void CSfxManager::RemoveLowPassFilter(int id) {
  if (id == 0) {
    return;
  }
  for (rstl::reserved_vector< SLowPassFilter, 8 >::iterator it = mLowPassFilters.begin();
       it != mLowPassFilters.end(); ++it) {
    if (id == it->mId) {
      mLowPassFilters.erase(it);
      return;
    }
  }
}

bool CSfxManager::IsLowPassEnabled() {
  return mLowPassFilters.size() > 0 && mCurrentChannel == kSC_Game;
}

int CSfxManager::GetLowPassFrequency() { return mLowPassFrequency; }

bool CSfxManager::ShouldApplyLowPass(CBaseSfxWrapper* sound) {
  if (sound->GetArea() != kAllAreas && IsLowPassAreaFilterEnabled() &&
      !sound->GetIgnoreAreaLowPass()) {
    return true;
  }
  bool ret = false;
  if (sound->UseAcoustics() && IsLowPassEnabled()) {
    ret = true;
  }
  return ret;
}

int CSfxManager::GetLowPassFrequency(CBaseSfxWrapper* sound) {
  int frequency = mAreaLowPassFrequency;
  if (sound->GetArea() != kAllAreas && IsLowPassAreaFilterEnabled() &&
      !sound->GetIgnoreAreaLowPass()) {
    frequency = GetLowPassAreaFrequency();
  }
  if (sound->UseAcoustics() && IsLowPassEnabled()) {
    frequency = rstl::min_val(frequency, GetLowPassFrequency());
  }
  return frequency;
}

uchar CSfxManager::GetAreaVolume(int area) {
  if (area != kAllAreas) {
    for (rstl::reserved_vector< SAreaVolume, 10 >::iterator it = mAreaVolumes.begin();
         it != mAreaVolumes.end(); ++it) {
      if (area == it->mArea) {
        return it->mVolume;
      }
    }
  }
  return 127;
}

void CSfxManager::SetAreaVolume(int area, uchar volume) {
  rstl::reserved_vector< SAreaVolume, 10 >::iterator it = mAreaVolumes.begin();
  for (; it != mAreaVolumes.end(); ++it) {
    if (area == it->mArea) {
      it->mVolume = volume;
      break;
    }
  }
  if (it == mAreaVolumes.end()) {
    for (it = mAreaVolumes.begin(); it != mAreaVolumes.end(); ++it) {
      if (it->mArea == kAllAreas) {
        it->mArea = area;
        it->mVolume = volume;
        return;
      }
    }
  }
}

void CSfxManager::SetIgnoreAreaLowPass(CSfxHandle handle, bool ignore) {
  const CSfxChannel& channel = mChannels[mCurrentChannel];
  if (handle.GetIndex() < 0 || handle.GetIndex() >= channel.mSounds.size()) {
    return;
  }
  CBaseSfxWrapper* sound = channel.mSounds[handle.GetIndex()];
  if (sound == nullptr || handle != sound->GetSfxHandle()) {
    return;
  }
  sound->SetIgnoreAreaLowPass(ignore);
}

CSfxManager::CSfxWrapper::~CSfxWrapper() {}

bool CSfxManager::CSfxWrapper::IsEmitter() const { return false; }

CFactoryFnReturn FAudioTranslationTableFactory(const SObjectTag&, CInputStream& in,
                                                     const CVParamTransfer&) {
  return rs_new rstl::vector< short >(in);
}
