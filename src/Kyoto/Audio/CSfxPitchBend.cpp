#include "Kyoto/Audio/CSfxPitchBend.hpp"

// Retail code for this class sits in the asm-only split auto_03_8032AFC8_text.s
// (0x8032AFC8..0x8032B080), so this file is port-only for now.

bool CSfxPitchBend::IsFinished() const { return mTimeRemaining <= 0.f; } // 0x8032AFC8

// 0x8032AFE4: moves mPitch toward mTargetPitch by the share of the remaining distance
// that dt covers, never overshooting.
void CSfxPitchBend::Update(float dt) {
  if (mTimeRemaining > 0.f) {
    const short diff = static_cast< short >(mTargetPitch - mPitch);
    const short step = static_cast< short >(static_cast< float >(diff) / mTimeRemaining * dt);
    if (mPitch < mTargetPitch) {
      int pitch = mPitch + step;
      if (mTargetPitch < pitch) {
        pitch = mTargetPitch;
      }
      mPitch = pitch;
    } else if (mPitch > mTargetPitch) {
      int pitch = mPitch + step;
      if (pitch < mTargetPitch) {
        pitch = mTargetPitch;
      }
      mPitch = pitch;
    }
    mTimeRemaining -= dt;
  }
}

CSfxPitchBend::CSfxPitchBend(const CSfxHandle& handle, ushort start, ushort target,
                             float duration) // 0x8032B068
: mHandle(handle), mPitch(start), mTargetPitch(target), mTimeRemaining(duration) {}
