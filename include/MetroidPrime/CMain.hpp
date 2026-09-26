#ifndef _CMAIN
#define _CMAIN

#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CStopwatch;
class CGameGlobalObjects;
class CMemorySys;

class CMain {
public:
  enum ERestartMode {
    kRM_None,
    kRM_WinBad,
    kRM_WinGood,
    kRM_WinBest,
    kRM_LoseGame,
    kRM_Default,
    kRM_StateSetter,
    kRM_PreFrontEnd,
    kRM_FrontEnd,
    kRM_Game,
    kRM_GameExit,
  };

  CMain(COsContext* context, void* unk1, CMemorySys* memorySys, void* unk2);
  ~CMain();

  bool LoadAudio();
  void UpdateStreamedAudio();
  void RegisterResourceTweaks();
  void ResetGameState();
  void StreamNewGameState(CInputStream& in, int saveIdx);
  void RefreshGameState();
  void AddWorldPaks();
  void AsyncIdle(uint time);
  void SetFrameTimeMinimum(int time);
  void SetGameFrameDrawn(bool drawn);
  int RsMain(int argc, const char* const* argv);
  void InitializeSubsystems();
  void FillInAssetIDs();
  void ShutdownSubsystems();
  void MemoryCardInitializePump();
  void DoPredrawMetrics();
  void DrawDebugMetrics(double dt, CStopwatch& stopWatch);
  bool CheckTerminate();
  bool CheckReset();
  // Metroid Prime carry-over. **Retail Echoes has no `CMain::OpenWindow`**: the 19 `CMain`
  // methods in `config/G2ME01/symbols.txt` do not include it, the name occurs nowhere in the
  // DOL, and `CMain::RsMain` (0x80005C6C) makes no call on `osContext` at all. Retail's
  // window/VI bring-up is in `main` (0x801EFB00) -> `fn_802BE85C` -> `fn_802C329C` ->
  // `fn_802C2FD4`. The only definition is host-only, in `src/MetroidPrime/PortBoot.cpp`, and
  // the measurements are in `docs/research/boot_path.md`. Do not write a body for this and
  // call it retail's.
  void OpenWindow();
  void SetRestartMode(ERestartMode s) { restartMode = s; }
  ERestartMode GetRestartMode() const { return restartMode; }
  // void SetCardBusy(bool v) { x160_31_cardBusy = v; }

  void SetMaxSpeed(bool v); // {x160_26_screenFading = v; }

  // **The two unnamed functions `CMain::RsMain` calls are now identified** (lane `cmain2`,
  // 2026-09-26). `boot_path.md` steps 8 and 10 are both on the retail entry point's path and
  // both were "unidentified"; neither is any more, and neither is declared here because retail's
  // symbol table names neither, so a C++ declaration would mangle and objdiff would pair
  // nothing. They are free functions on `CMain*` in retail and are written as such:
  //
  //   `fn_80003A18`  0x80003A18, 0x30 = 48 bytes (step 8).  Twelve instructions:
  //   `lwz r3,0(r3)` (word 0 of `this` is `osContext`, CMain.hpp:107), then
  //   `fn_8028BFF4` - itself three instructions, `lwz r3,20(r3) ; blr`, i.e. a four-byte-field
  //   accessor at **`COsContext`+0x14**, which is Aurora's `OSGetConsoleType()` slot - then
  //   `cmpwi r3,5 ; bne ; li r3,0`. So it reads a `u32` at `osContext+0x14` and maps the value
  //   5 to 0. **Not carved, and this is the measured reason:** fifteen source spellings all
  //   produce the same two instructions in the wrong order - retail emits
  //   `stwu ; mflr r0 ; lwz r3,0(r3) ; stw r0,20(r1) ; bl` and mwcceppc 2.7 always emits
  //   `stwu ; mflr r0 ; stw r0,20(r1) ; lwz r3,0(r3) ; bl`, sinking the argument load below the
  //   link-register spill. A `Matching` unit has to be byte-exact, so the carve was withdrawn
  //   rather than left claiming a range it does not reproduce. The source is kept at
  //   `/tmp/opencode/cmain2-out/Carve80003A18.c.blocked` for the next lane.
  //
  //   `fn_800069AC`  0x800069AC, 0x134 = 308 bytes (step 10).  Still unwritten. It is a bounded,
  //   insertion-sorted float push: `r3` points at `{int n; float v[4];}` and it appends
  //   `*(float*)r4` and re-sorts. `RsMain` calls it six times, four at 0x80005D0C-0x80005D24 to
  //   seed two of them from two `.sdata2` constants and two more per frame at 0x80006108 and
  //   0x80006228, into `CMain`+0x18 and `CMain`+0x2C - twenty bytes each, i.e. inside the
  //   `char x10_pad[0x38]` below, which is why modelling them is a `CMain.hpp` layout job
  //   before it is a body job.
  bool fn_80008A1C();

  // void SetX30(bool v) { x160_30_ = v; }

  // **Written out, the assignment is a no-op, and retail has it.** `lbz r0,144(r3) ; li r4,1 ;
  // rlwimi r0,r4,1,30,30 ; stb r0,144(r3)` at 0x8001DF08 masks word bit 30, which lives in byte
  // 0x93, and then stores byte 0x90. mwcceppc 2.7 allocates this `bool : 1` at word bit 30 but
  // still addresses the *containing byte* as 0x90, and every one of the nine bitfields behaves
  // that way - measured with probe/bf.cpp, not assumed. `src/MetroidPrime/
  // CMainFlowAdvanceGameState.cpp` is the only caller, and reproducing the four instructions
  // rather than "fixing" the store is the point: the bytes are the specification.
  void SetX90_30(bool v) { x90_30_ = v; }

  static void EnsureWorldPaksReady();
  static void EnsureWorldPakReady(CAssetId id);

  // **The `+ f` was a no-op and retail stores.** The body was `x5c + f;`, which computes a value
  // and throws it away, so `CGameArchitectureSupport::UpdateTicks` never wrote `x5c` at all. Retail
  // does, at 0x80007C48: `lwz r3,gpMain ; lfs f0,1/60 ; lfs f1,92(r3) ; fsubs f1,f1,f31 ;
  // stfs f1,92(r3)` - i.e. `x5c = x5c - stopwatchTime`, which is exactly
  // `Increment_x5c(-stopwatchTime)`. The frame loop's time debt is therefore one statement, and
  // the tree had silently dropped it. `main.cpp` is the only caller in the tree, so no other
  // object can move.
  void Increment_x5c(float f) { x5c = x5c + f; }
  bool GetFinished() const { return finished; }
  // `x91_24_gameFrameDrawn` read back. **`UpdateTicks` needs this and not `GetFinished()`**, which
  // is what the disassembly says and the two are not the same field: `finished` is bit 0 of the
  // byte at `CMain`+0x90, and retail's `UpdateTicks` tests **bit 0 of the byte at +0x91** -
  // `lbz r0,145(r3) ; rlwinm. r0,r0,25,31,31` at 0x80007C40. `SetGameFrameDrawn` (0x800089AC) is
  // the proof of which field that is: it is `lbz 145(r3) ; rlwimi r0,r4,7,24,24 ; stb 145(r3)`,
  // and `rlwimi r0,rX,7-n,24+n,24+n` is field *n* counted down, so `+0x91`'s bit 0 is field 0 of
  // its group, which is `x91_24_gameFrameDrawn`. The semantics agree: clamping the accumulated
  // tick debt to 2/60 when a frame was drawn is a debt clamp, and it is not a "finished" test.
  bool GetGameFrameDrawn() const { return x91_24_gameFrameDrawn; }

  // // TODO
  // COsContext& InitOsContext() {
  //   OpenWindow();
  //   return x0_osContext;
  // }

private:
  COsContext* osContext;
  void* x4_unk1;
  CMemorySys* memorySys;
  void* xc_unk2;
  char x10_pad[0x38];
  int frameTimeMinimum;
  float x4c;
  float x50;
  CGameGlobalObjects* gameGlobalObjects;
  ERestartMode restartMode;
  float x5c;
  rstl::reserved_vector< uint, 10 > frameTimes;
  int frameTimeIdx;
  bool finished : 1;
  bool mfGameBuilt : 1;
  bool screenFading : 1;
  bool x90_27_ : 1;
  bool x90_28_manageCard : 1;
  bool x90_29_ : 1;
  bool x90_30_ : 1;
  bool x90_31_cardBusy : 1;
  bool x91_24_gameFrameDrawn : 1;
  // CGameArchitectureSupport* x164_;
};

extern CMain* gpMain;

#endif // _CMAIN
