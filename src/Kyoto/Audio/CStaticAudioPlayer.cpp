// Retail calls rstl::destroy(It, It) out of line from vector::~vector and clear, but inlines
// it into reserve; 105..120 reproduces that (125 inlines it everywhere).
#pragma inline_max_size(120)
#include "Kyoto/Audio/CAudioSys.hpp"
#include "rstl/algorithm.hpp"
#include <Kyoto/Alloc/CMemory.hpp>
#include <Kyoto/Audio/CStaticAudioPlayer.hpp>

#include <Kyoto/CDvdFile.hpp>
#include <Kyoto/CDvdRequest.hpp>

#include <rstl/math.hpp>

#include <dolphin/ai.h>
#include <dolphin/os.h>
#include <stdint.h>

class CInterruptGuard {
  bool mEnabled;

public:
  CInterruptGuard() : mEnabled(OSDisableInterrupts()) {}
  ~CInterruptGuard() { OSRestoreInterrupts(mEnabled); }
};

static CStaticAudioPlayer* sCurrentPlayer = nullptr;
static rstl::reserved_vector< FAudioCallback, 4 > sAICallbacks;
static bool sDMACallbackInstalled ATTRIBUTE_ALIGN(8) = false;
static FAudioCallback sOldDMACallback = nullptr;

void CStaticAudioPlayer::InstallAICallback() {
  bool old = CAudioSys::IsAICallbackEnabled();
  CAudioSys::EnableAICallback(true);

  if (!sDMACallbackInstalled && sAICallbacks.size() != 0) {
    sOldDMACallback = AIRegisterDMACallback(AICallback);
    sDMACallbackInstalled = true;
  } else if (sDMACallbackInstalled && sAICallbacks.size() == 0) {
    AIRegisterDMACallback(sOldDMACallback);
    sOldDMACallback = 0;
    sDMACallbackInstalled = false;
  }

  CAudioSys::EnableAICallback(old);
}

void CStaticAudioPlayer::AICallback() {
  sOldDMACallback();

  for (int i = 0; i < sAICallbacks.size(); ++i) {
    sAICallbacks[i]();
  }
}

void CStaticAudioPlayer::RunDMACallback(const FAudioCallback callback) {
  // Upstream wraps this in a CInterruptGuard whose ctor/dtor are emitted as a weak pair;
  // retail disables and restores around the body inside the one function.
  volatile const bool old = OSDisableInterrupts();
  if (rstl::find(sAICallbacks.begin(), sAICallbacks.end(), callback) == sAICallbacks.end()) {
    sAICallbacks.push_back(callback);
  }

  InstallAICallback();
  OSRestoreInterrupts(old);
}

void CStaticAudioPlayer::CancelDMACallback(FAudioCallback callback) {
  volatile const bool old = OSDisableInterrupts();

  const rstl::reserved_vector< FAudioCallback, 4 >::iterator it =
      rstl::find(sAICallbacks.begin(), sAICallbacks.end(), callback);
  if (it != sAICallbacks.end()) {
    sAICallbacks.erase(it);
  }

  InstallAICallback();
  OSRestoreInterrupts(old);
}

CStaticAudioPlayer::CStaticAudioPlayer(const rstl::string& filepath, const int loopStart,
                                       const int loopEnd)
: mFilepath(filepath)
, mRsfRem(-1)
, mCurSamp(0)
, mLoopStartSamp(loopStart & ~1)
, mLoopEndSamp(loopEnd & ~1)
, mCurBuf(0)
, mDmaBufferA(static_cast< uchar* >(CMemory::Alloc(640, IAllocator::kHI_RoundUpLen)))
, mDmaBufferB(static_cast< uchar* >(CMemory::Alloc(640, IAllocator::kHI_RoundUpLen)))
, mVolume(32768) {
  CDvdFile dvdFile(filepath.data());
  mRsfRem = dvdFile.GetFileSize();
  mRsfLength = mRsfRem;
  int bufferCount = ((mRsfRem - 1) + 0x4000) / 0x4000;
  mBuffers.reserve(bufferCount);
  mDvdRequests.reserve(bufferCount);

  for (int i = mRsfRem; i > 0; i -= 0x4000) {
    // Upstream hoists the `i <= 0x4000` test into an `if`; retail's ternary is one basic block
    // and it is worth the whole register allocation of bufferCount (r27 vs r29) downstream.
    uint bufferSize = i <= 0x4000 ? (i + 31) & ~31 : 0x4000;

    rstl::auto_ptr< uchar > buf(
        static_cast< uchar* >(CMemory::Alloc(bufferSize, IAllocator::kHI_RoundUpLen)));
    mBuffers.push_back_unsafe(buf);
    mDvdRequests.push_back_unsafe(dvdFile.SyncRead(buf.get(), bufferSize));
  }
}

CStaticAudioPlayer::~CStaticAudioPlayer() { StopMixOut(); }

const bool CStaticAudioPlayer::IsReady() const {
  return !mDvdRequests.empty() ? mDvdRequests.back()->IsComplete() : true;
}

void CStaticAudioPlayer::StartMixOut() {
  if (sCurrentPlayer == this) {
    return;
  }

  mDvdRequests = rstl::vector< rstl::auto_ptr< CDvdRequest > >();
  mCurSamp = 0;
  g72x_init_state(&mLeftState);
  g72x_init_state(&mRightState);
  sCurrentPlayer = this;
  RunDMACallback(MixCallback);
}

void CStaticAudioPlayer::StopMixOut() {
  if (sCurrentPlayer == this) {
    CancelDMACallback(MixCallback);
    sCurrentPlayer = nullptr;
  }
}

void CStaticAudioPlayer::MixCallback() { sCurrentPlayer->DoMix(); }

void CStaticAudioPlayer::DoMix() {
  const ushort* aiStart = static_cast< const ushort* >(OSPhysicalToCached(AIGetDMAStartAddr()));
  mCurBuf ^= 1;
  uintptr_t buf =
      reinterpret_cast< uintptr_t >(mCurBuf != 0 ? mDmaBufferB.get() : mDmaBufferA.get());

  AIInitDMA(buf, 0x280);
  u32 cookie = OSEnableInterrupts();
  if (aiStart != 0) {
    DCInvalidateRange(const_cast< ushort* >(aiStart), 0x280);
  }

  Decode(reinterpret_cast< ushort* >(buf), aiStart, 160);
  DCFlushRange(reinterpret_cast< void* >(buf), 0x280);
  OSRestoreInterrupts(cookie);
}

// Forward declaration only. mwcceppc emits definitions in reverse source order and retail's .text
// has MixToMono at 0x803267ac *before* Decode at 0x80326810, so MixToMono's definition has to sit
// after Decode's. Moving it back is a silent permutation, not a compile error - the unit builds,
// every function still scores 100%, and the module's hash breaks on a few bytes.
static void MixToMono(ushort* data, int numSamples);

void CStaticAudioPlayer::Decode(ushort* out, const ushort* in, int numSamples) {
  int curSamp = mCurSamp / 2;
  int loopEndSamp = mLoopEndSamp / 2;
  int loopStartSamp = mLoopStartSamp / 2;
  // The `const` local is not cosmetic: with `numSamples` used directly in both calls MWCC hands
  // r31 to `in` and r25 to `numSamples`, and retail does the opposite. Routing the two calls
  // through this one-statement `const` local reproduces retail's allocation exactly.
  const int ns = numSamples;
  DecodeMonoAndMix(out, in, ns, curSamp, loopEndSamp, loopStartSamp, mVolume, mLeftState);

  int halfLen = mRsfLength / 2;
  DecodeMonoAndMix(out + 1, in + 1, ns, curSamp + halfLen, loopEndSamp + halfLen,
                   loopStartSamp + halfLen, mVolume, mRightState);

  if (CAudioSys::GetSurroundMode() == CAudioSys::kSM_Mono) {
    MixToMono(out, numSamples);
  }

  int remSamples = numSamples;
  while (remSamples != 0) {
    int remTillLoop = mLoopEndSamp - mCurSamp;
    int rs = remSamples;
    int consumed = rstl::min_val(rs, remTillLoop);
    mCurSamp += consumed;
    remSamples -= consumed;
    if (mCurSamp == mLoopEndSamp) {
      mCurSamp = mLoopStartSamp;
    }
  }
}

static void MixToMono(ushort* data, int numSamples) {
  short* samples = reinterpret_cast< short* >(data);
  for (int i = 0; i < numSamples * 2; i += 2) {
    int sample = (samples[0] + samples[1]) / 2;
    short clamped;
    if (sample < -32768) {
      clamped = -32768;
    } else if (sample > 32767) {
      clamped = 32767;
    } else {
      clamped = sample;
    }
    samples[0] = clamped;
    samples[1] = clamped;
    samples += 2;
  }
}

void CStaticAudioPlayer::DecodeMonoAndMix(ushort* out, const ushort* in, int numSamples,
                                          int startSample, const int sampleEnd,
                                          const int sampleStart, int vol, g72x_state& state) {
  // The order of these three declarations, and the `const` on the two `sample*` parameters above,
  // are both there for MWCC's register allocator and are worth 52 -> 18 differing instructions
  // (see docs/RUNNING_THE_DECOMP.md). They are semantics-neutral; do not "tidy" them.
  ushort* outCursor = out;
  int curSample = startSample;
  const ushort* inCursor = in;
  for (int remBytes = numSamples / 2; remBytes != 0;) {
    int rb = remBytes;
    int curBuf = curSample / 0x4000;
    int thisBytes = ((curBuf + 1) * 0x4000) - curSample;
    thisBytes = rstl::min_val(rb, thisBytes);

    int remTillLoop = sampleEnd - curSample;
    thisBytes = rstl::min_val(thisBytes, remTillLoop);

    uchar* byte = mBuffers[curBuf].get() + (curSample - (curBuf * 0x4000));
    int i = 0;
    while (i < thisBytes) {
      int samp1 = reinterpret_cast< const short* >(inCursor)[0] +
                  ((vol * g721_decoder(*byte & 0xf, &state)) >> 15);
      int samp2 = reinterpret_cast< const short* >(inCursor)[2] +
                  ((vol * g721_decoder(*byte >> 4, &state)) >> 15);

      short clamped1;
      if (samp1 < -0x8000) {
        clamped1 = -0x8000;
      } else if (samp1 > 0x7fff) {
        clamped1 = 0x7fff;
      } else {
        clamped1 = samp1;
      }
      outCursor[0] = clamped1;

      short clamped2;
      if (samp2 < -0x8000) {
        clamped2 = -0x8000;
      } else if (samp2 > 0x7fff) {
        clamped2 = 0x7fff;
      } else {
        clamped2 = samp2;
      }
      outCursor[2] = clamped2;

      outCursor += 4;
      ++byte;
      inCursor += 4;
      ++i;
    }

    curSample += thisBytes;
    remBytes -= thisBytes;
    if (curSample == sampleEnd) {
      curSample = sampleStart;
    }
  }
}

void CStaticAudioPlayer::SetVolume(uchar vol) {
  if (static_cast< uchar >(vol) > 127) {
    vol = 127;
  }
  mVolume = CAudioSys::kVolumeTable[static_cast< uchar >(vol)];
}
