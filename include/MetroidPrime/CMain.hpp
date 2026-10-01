#ifndef _CMAIN
#define _CMAIN

#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TReservedAverage.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CStopwatch;
class CGameGlobalObjects;
class CGameArchitectureSupport;
class CMemorySys;
class CDvdRequestSys;
class CSaveRegion;
class COsContext;
class CGameArchitectureSupport;

class CMain {
public:
  enum ERestartMode {
    // Echoes values; 1-5 end the game (names inferred from CMainFlow's handling)
    kRM_None,
    kRM_Credits1,
    kRM_Credits2,
    kRM_EndMovie1,
    kRM_EndAutoSave,
    kRM_EndMovie2,
    kRM_Default,
    kRM_StateSetter,
  };

  CMain(COsContext* context, CSaveRegion* saveRegion, CMemorySys* memorySys,
        CDvdRequestSys* dvdRequestSys);
  ~CMain();

  bool LoadAudio();
  void UpdateStreamedAudio();
  void RegisterResourceTweaks();
  void ResetGameState();
  void StreamNewGameState(bool);
  int GetLanguage() const; // Guessed name
  void RefreshGameState();
  void AddWorldPaks();
  void AsyncIdle(uint time);
  int RsMain(int argc, const char* const* argv);
  void InitializeSubsystems();
  void FillInAssetIDs();
  void ShutdownSubsystems();
  void MemoryCardInitializePump();
  void DoPredrawMetrics();
  void DrawDebugMetrics(double dt, CStopwatch& stopWatch);
  bool CheckTerminate();
  bool CheckReset();
  // **Uncalled, and with no definition in the port.** Metroid Prime carry-over: this DOL has no
  // `CMain::OpenWindow` at all (see the note on the `InitOsContext` sketch near the bottom of this
  // header, and `src/MetroidPrime/PortBoot.cpp`'s header for the measurements). The host stand-in
  // that used to implement it was removed on 2026-09-29 when `CGraphicsSys` - retail's own
  // `fn_802BE85C`, constructed in `platform/main.cpp` before `InvokeCMain` - took over the VI
  // bring-up. The declaration stays because removing it would be a header change against
  // upstream's file; nothing calls it.
  void OpenWindow();
  void SetRestartMode(ERestartMode mode) { mRestartMode = mode; }
  ERestartMode GetRestartMode() const { return mRestartMode; }

  void SetMaxSpeed(bool enabled);

  bool GetMaxSpeed();

  void SetGameExitReset(bool reset) { mGameExitReset = reset; }
  void SetGameFrameDrawn(bool drawn) { mGameFrameDrawn = drawn; }
  // Guessed names; the native flag forces two ticks and a 30-FPS frame wait.
  void SetThirtyFps(bool enabled);
  bool GetThirtyFps() const { return mThirtyFps; }
  // Guessed name; minimum asynchronous resource budget for the next frame.
  void SetFrameTimeMinimum(uint time);

  static void EnsureWorldPaksReady();
  static void EnsureWorldPakReady(CAssetId id);

  void DecrementMaxSpeedDrawTimer(float dt) { mMaxSpeedDrawTimer -= dt; }
  bool GetFinished() const { return mFinished; }
  float GetAverageTickTime() const { return mAverageTickTime; }
  float GetAverageDrawTime() const { return mAverageDrawTime; }

#ifdef TARGET_PC
  // Port. `mGameGlobalObjects` is private and `src/MetroidPrime/PortStreamNewGameState.cpp` is an
  // `extern "C"` free function, so it needs an accessor. **The offset may not be spelled
  // instead**: retail reads it as `lwz r3,84(r28)`, which is `CMain`+0x54 in a 32-bit GameCube
  // object, and every pointer in this class is eight bytes wide on the host.
  CGameGlobalObjects* GetGameGlobalObjects() const { return mGameGlobalObjects; }
#endif

  // `OpenWindow()` above is Metroid Prime carry-over, not Echoes: it has no counterpart in this
  // DOL and **the port defines no body for it**. Retail's VI bring-up is `CGraphicsSys`'s
  // constructor - `fn_802BE85C`, which `main` builds before `InvokeCMain` - and that is
  // constructed for real in `platform/main.cpp`. See `src/MetroidPrime/PortBoot.cpp`'s header.
private:
  COsContext* mOsContext;
  CSaveRegion* mSaveRegion;
  CMemorySys* mMemorySys;
  CDvdRequestSys* mDvdRequestSys;
  double x10_;
  TReservedAverage< float, 4 > mTickTimes;
  TReservedAverage< float, 4 > mDrawTimes;
  float mAverageTickTime;
  float mAverageDrawTime;
  uint mFrameTimeMinimum;
  float mSoftResetHoldTime;
  float mResetInputDelay;
  CGameGlobalObjects* mGameGlobalObjects;
  ERestartMode mRestartMode;
  float mMaxSpeedDrawTimer; // Guessed name.
  rstl::reserved_vector< uint, 10 > mFrameTimes;
  int mFrameTimeIdx;
  bool mFinished : 1;
  bool mMfGameBuilt : 1; // Inherited name; no semantic use identified in this TU.
  bool mMaxSpeed : 1;    // Guessed name: cinematic-skip fast-forward.
  bool mResetButtonHeld : 1;
  bool mManageCard : 1;
  bool mResetRequested : 1;
  bool mGameExitReset : 1;
  bool mGameFrameDrawn : 1;
  // +0x91, bit 0 - the ninth one-bit group. `SetThirtyFps` (0x800089AC) is `lbz 145(r3) ;
  // rlwimi r0,r4,7,24,24 ; stb 145(r3)` and `CMain::RsMain` reads it back at 0x800062B0. This
  // tree called it `gameFrameDrawn` until the eighth upstream sync; upstream's `mGameFrameDrawn`
  // is the last bit of +0x90, which this tree called `mCardBusy`.
  bool mThirtyFps : 1;
  CGameArchitectureSupport* mArchSupport;

public:
  // CSaveGameScreen::DoAdvance writes this bit inline rather than calling (0x8017C7E4 is
  // `lwz r3,-28364(r13); li r4,1; lbz r0,144(r3); rlwimi r0,r4,3,28,28; stb r0,144(r3)`), so
  // the setter has to be inline too. 144 is 0x90, and mManageCard is bit 4 of that byte.
  void SetManageCard(bool manage) { mManageCard = manage; }
};
CHECK_SIZEOF(CMain, 0x98)

extern CMain* gpMain;

#endif // _CMAIN
