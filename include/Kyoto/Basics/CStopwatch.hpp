#ifndef _CSTOPWATCH
#define _CSTOPWATCH

#include "types.h"

#include "dolphin/os.h"

class CStopwatch {
public:
  class CSWData {
  public:
    CSWData() : x0_timerFreq(0), x8_timerFreqO1M(0), x10_timerPeriod(0.f) {}

    bool Initialize();
    void Wait(float) const;

    s64 GetTimerFreq() const { return x0_timerFreq; }
    s64 GetTimerFreqO1M() const { return x8_timerFreqO1M; }
    float GetTimerPeriod() const { return x10_timerPeriod; }
    s64 GetCPUCycles() const { return OSGetTime(); }

  private:
    s64 x0_timerFreq;
    s64 x8_timerFreqO1M;
    float x10_timerPeriod;
  };

  CStopwatch() : x0_startTime(mData.GetCPUCycles()) {}
  static bool InitGlobalTimer();
  static CStopwatch& GetGlobalTimerObj();
  // What `CResFactory::AsyncIdle` divides its tick delta by. Retail reads the word straight out
  // of the object - `lwz r5,8(r31)` / `lwz r6,12(r31)` at 0x802FA40C with `r31` =
  // `0x80411050` = `mData__10CStopwatch`, i.e. `x8_timerFreqO1M`, an `s64` whose high word sits
  // at +8 and low word at +0xC - and `mData` is private, so this is the public route to the
  // same value. Declared inline here so that the only unit which emits anything new is
  // `src/Kyoto/CResFactoryAsyncIdle.cpp`, which `configure.py` does not compile.
  static s64 GetGlobalTimerFreqO1M() { return mData.GetTimerFreqO1M(); }
  inline void Reset() {
    if (mData.GetTimerFreq() == 0) {
      mData.Initialize();
    }
    x0_startTime = mData.GetCPUCycles();
  }
  inline float GetElapsedTime() const {
    return (mData.GetCPUCycles() - x0_startTime) * mData.GetTimerPeriod();
  }
  inline s64 GetElapsedMicros() const {
    return (mData.GetCPUCycles() - x0_startTime) / mData.GetTimerFreqO1M();
  }

  static void Wait(float);

private:
  static CSWData mData;
  static CStopwatch mGlobalTimer;

  s64 x0_startTime;
};

#endif // _CSTOPWATCH
