#ifndef _CSFXPITCHBEND
#define _CSFXPITCHBEND

#include "Kyoto/Audio/CSfxHandle.hpp"

// Guessed name: a timed pitch transition owned by CSfxManager.
class CSfxPitchBend {
public:
  CSfxPitchBend(const CSfxHandle& handle, ushort start, ushort target, float duration);

  void Update(float dt);    // Guessed name
  bool IsFinished() const;  // Guessed name
  // Retail reads the handle straight out of the record at each call site; a by-value
  // return makes MWCC materialise an extra copy per argument.
  const CSfxHandle& GetHandle() const { return mHandle; }
  ushort GetPitch() const { return mPitch; }

private:
  CSfxHandle mHandle;
  ushort mPitch;
  ushort mTargetPitch;
  float mTimeRemaining;
};
CHECK_SIZEOF(CSfxPitchBend, 0xc)

#endif // _CSFXPITCHBEND
