#ifndef _CGAMEARCHITECTURESUPPORT
#define _CGAMEARCHITECTURESUPPORT

#include "types.h"

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/Basics/CStopwatch.hpp"
#include "Kyoto/TOneStatic.hpp"
#include "CGuiSys/CGuiSys.hpp"

#include "MetroidPrime/CArchitectureQueue.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "MetroidPrime/CInputGenerator.hpp"

#include "rstl/vector.hpp"

class CToken;

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

  inline CStopwatch& GetStopwatch1() { return stopwatch1; }
  inline CStopwatch& GetStopwatch2() { return stopwatch2; }
  inline CIOWinManager& GetIOWinManager() { return ioWinMgr; }
  inline int& GetFramesDrawn() { return gameFrameCount; }

private:
  CAudioSys audioSys;
  CArchitectureQueue archQueue;
  CStopwatch stopwatch1;
  CStopwatch stopwatch2;
  CInputGenerator inputGenerator;
  CIOWinManager ioWinMgr;
  // CGuiSys guiSys;
  int gameFrameCount;
  float x68_;
  float x6c_;
  float x70_;
  uint x74_;
  // rstl::vector< CToken > x90_;
  OSAlarm infiniteLoopAlarm;
  bool infiniteLoopAlarmSet;
};
// CHECK_SIZEOF(CGameArchitectureSupport, 0xd0)

#endif // _CGAMEARCHITECTURESUPPORT
