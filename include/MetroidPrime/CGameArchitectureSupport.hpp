#ifndef _CGAMEARCHITECTURESUPPORT
#define _CGAMEARCHITECTURESUPPORT

#include "types.h"

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/Basics/CStopwatch.hpp"
#include "Kyoto/TOneStatic.hpp"

#include "MetroidPrime/CArchitectureQueue.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "MetroidPrime/CInputGenerator.hpp"

class CGameArchitectureSupport : public TOneStatic< CGameArchitectureSupport > {
public:
  CGameArchitectureSupport(COsContext&);
  ~CGameArchitectureSupport();

  void PreloadAudio();
  bool UpdateTicks();
  void Update();
  // **`static`, and that is measured, not a guess.** Retail's `~CGameArchitectureSupport`
  // (0x80007DE8) calls it at 0x80007E2C with **no argument setup at all** - the instruction
  // before the call is `li r0,0` for the `lbl_80418EC4 = 0` store, and `bl fn_8029EF20` follows it
  // directly. A non-static member call always emits an `mr r3,rN` (or `addi r3,r31,N`) first, and
  // the non-static declaration emitted exactly that, where retail has nothing.
  // `config/G2ME01/symbols.txt:11874` names 0x8029EF20 `Shutdown__11CSfxManagerFv` in this tree
  // (the port renamed it from `fn_8029EF20` and gave the function a real class), so the emitted
  // `bl` carries this header's `_ZN24CGameArchitectureSupport11UnloadAudioEv` against retail's own
  // address and resolves. mwcceppc mangles a static member function exactly as a non-static one,
  // so the name is unchanged; only the caller's register setup disappears.
  static void UnloadAudio();

  inline CStopwatch& GetStopwatch1() { return mTickStopwatch; }
  inline CStopwatch& GetStopwatch2() { return mDrawStopwatch; }
  inline CIOWinManager& GetIOWinManager() { return mIoWinMgr; }
  inline int& GetFramesDrawn() { return mGameFrameCount; }
  OSAlarm& GetInfiniteLoopAlarm() { return mInfiniteLoopAlarm; }
  bool IsInfiniteLoopAlarmSet() const { return mInfiniteLoopAlarmSet; }
  void SetInfiniteLoopAlarmSet(bool set) { mInfiniteLoopAlarmSet = set; }

private:
  CAudioSys mAudioSys;
  CArchitectureQueue mArchQueue;
  CStopwatch mTickStopwatch;
  CStopwatch mDrawStopwatch;
  CInputGenerator mInputGenerator;
  CIOWinManager mIoWinMgr;
  int mGameFrameCount;
  float mTickRemainder;
  float mPreviousTickRemainder2;
  float mPreviousTickRemainder;
  OSAlarm mInfiniteLoopAlarm;
  bool mInfiniteLoopAlarmSet;
};
CHECK_SIZEOF(CGameArchitectureSupport, 0xa8)

#endif // _CGAMEARCHITECTURESUPPORT
