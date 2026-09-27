#ifndef _CMAIN
#define _CMAIN

#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CStopwatch;
class CGameGlobalObjects;
class CGameArchitectureSupport;
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
#ifdef TARGET_PC
  // Read-only reach for `gameGlobalObjects`, which retail reads as `lwz r3,84(r28)` in
  // `StreamNewGameState` (0x80005458, 0x800054CC and three more) and which is a private member
  // further down this class. A member function of `CMain` reaches it directly, but the *port's*
  // body of `StreamNewGameState` cannot: it is
  // `StreamNewGameState__5CMainFR12CInputStreami` in
  // `src/MetroidPrime/PortStreamNewGameState.cpp`, an `extern "C"` free function, because that is
  // the name `CMainFlow::SetGameState` calls it under and no host compiler mangles a member to
  // it. **The offset may not be spelled instead**: `84(r28)` is `CMain`+0x54 in a 32-bit
  // GameCube object, and every pointer in this class is eight bytes wide on the host, so the same
  // byte is somewhere else here. An accessor is the only honest way across.
  //
  // Guarded like every other host-only thing in a header here (`CInputStream.hpp`'s byte-order
  // helpers), so the isolation is structural rather than something the DOL sha1 merely fails to
  // notice: mwcceppc does not define `TARGET_PC`, so this declaration is not in its view of the
  // class at all.
  CGameGlobalObjects* GetGameGlobalObjects() const { return gameGlobalObjects; }
#endif
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
  //   `fn_800069AC`  0x800069AC, 0x134 = 308 bytes (step 10).  **It is written, and it is no
  //   longer "a header job before it is a body job".** It is a bounded, newest-first float
  //   push: `r3` points at `{int n; float v[4];}`, it appends `*(float*)r4` at `v[n]` if
  //   `n < 4`, then shifts `v[i] = v[i-1]` down from `i = n-1` to 1, then does `stfs f0,4(r3)`.
  //   **There is no `fcmpo` or `fcmpu` anywhere in its 308 bytes, so it sorts nothing** -
  //   `docs/research/boot_path.md` row 10's "insertion-sorted" is wrong. `RsMain` calls it six
  //   times: four at 0x80005D0C-0x80005D24 to seed the two histories at `CMain`+0x18 and
  //   `CMain`+0x2C with 0.3f and 0.2f, and two more per frame at 0x80006108 and 0x80006228.
  //   Both histories are modelled in the private section below.
  //   **It is not an unclaimed gap**: 0x800069AC sits inside `MetroidPrime/main.cpp`'s `.text`
  //   claim (0x800053B8-0x8000848C, `config/G2ME01/splits.txt`), so a unit of its own means
  //   cutting main.cpp's claim in two - the split the `mainsplit` lane owns.
  //   `src/MetroidPrime/Carve800069AC.c` holds the body, measured byte-exact; the claim lines it
  //   still needs are in that file's header.
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
  // **`CMain`+0x10..+0x48 is not padding, and the `char x10_pad[0x38]` that used to stand for
  // it cost this class four bytes of `sizeof` as well as the meaning of every member in it.**
  // Measured, not inferred, and the proof is retail's own numbers:
  //
  // - `config/G2ME01/symbols.txt:18933` says `sMainSpace = .bss:0x803C5A20; size:0x98`, and
  //   `:18934` puts the next object (`lbl_803C5AB8`) at 0x803C5AB8. `CMain` is that object:
  //   `InvokeCMain` (0x80008818) reaches it with `lis r9,0x803C ; addic. r31,r9,0x5A20`, and
  //   the constructor at 0x80008898 publishes itself with `stw r3,-28364(r13)`, which is
  //   0x80416E9C, the one global `CMain::RsMain` reads at 0x800062A8. So **retail's
  //   `sizeof(CMain)` is 0x98 = 152**, and this header measured **0x94 = 148** before the
  //   change (tools/sizeprobe_cmain.cpp, compiled with mwcceppc - a host `sizeof` says nothing
  //   about a 32-bit GameCube object).
  // - the last word of the constructor is `stw r8,148(r3)` at 0x800089A0 - `CMain`+0x94 - and
  //   `CMain::RsMain` puts the `CGameArchitectureSupport` it allocated there
  //   (`stw r0,148(r31)` at 0x80005E30, next to the `li r3,356` at 0x80005E08: 356 = 0x164 is
  //   `CGameArchitectureSupport`'s own extent, not `CMain`'s). That member was commented out
  //   here, which is the missing four bytes.
  //
  // What the 0x38 bytes actually are, in the order retail's own stores and loads place them:
  //
  // - **+0x10, 8 bytes, a `double`.** `stfd f2,16(r3)` at 0x800088C0, with
  //   `f2 = lfd -32720(r2)`; r2 is `_SDA2_BASE_` = 0x804223C0 (`tools/sda.py`), so that is
  //   0x8041A3F0, which `objdump -s` shows is eight zero bytes. `RsMain` reads the same field
  //   with the same `lfd -32720(r2)` at 0x80006270 and `fcmpo`s it against the frame's
  //   remaining time, so it is a `double` and not a pointer or an int pair. **Its name is a
  //   guess** - nothing in the DOL says what it holds beyond "0.0, compared as a time".
  // - **+0x18 and +0x2C, twenty bytes each, two bounded frame-time histories.** These are the
  //   four floats and the count the previous revision of this comment called padding. Proof
  //   they are `{int; float[4]}`: the constructor zeroes `+0x18` and `+0x2C` and nothing else
  //   in either window (`stw r8,24(r3)` at 0x800088C4, `stw r8,44(r3)` at 0x800088C8 - the
  //   `v[4]` behind them is deliberately left uninitialised), and `fn_800069AC` (retail
  //   0x800069AC, 0x134 = 308 bytes) then reads `lwz r0,0(r3)` as a count, compares it with 4,
  //   writes `stfs f0,4(r5)` with `r5 = r3 + count*4` and finally `stfs f0,4(r3)` - i.e. an
  //   `int` at +0x0 and four `float`s at +0x4, +0x8, +0xC, +0x10, which is 20 bytes and lands
  //   the second one exactly on 0x2C.
  // - **+0x40 and +0x44, two floats: each history's running total.** `fn_80006954`
  //   (0x80006954, 0x58) returns the sum of a history's `count` entries - it hands
  //   `fn_80008B60` the pair `h->v` and `h->count`, and that function is an unrolled
  //   `fadds` accumulator - and `RsMain` stores the result at +0x40 after pushing into
  //   `+0x18` (0x80006120) and at +0x44 after pushing into `+0x2C` (0x8000623C).
  // - **The per-frame sample** is `float(OSGetTime() - mark) * stopwatch.mData[0x10] / (1/60)`,
  //   at 0x800060EC-0x80006108: `lfs f0,16(r30)` with r30 = 0x80411050 = `mData__10CStopwatch`,
  //   `fmuls f30,f1,f0`, `fdiv f0,f30,f31` with `f31 = lfd -32752(r2)` = the `double`
  //   0.016666668 at 0x8041A3D0, then `frsp`. `mData[0x10]` is written only by
  //   `__sinit_CStopwatch_cpp` (0x8028BCF0, `stfs f0,16(r3)` with 0.0f) and never changed, so
  //   the absolute unit is not derivable from the DOL and is not claimed here.
  // - **The seeds agree with the fields they feed**, which is the strongest single check that
  //   the two histories and the two floats are one mechanism: `RsMain`'s seeding loop at
  //   0x80005D04-0x80005D24 pushes 0.3f (`addi r4,r13,-32760` = 0x80417D88 = `3e99999a`) into
  //   +0x18 and 0.2f (`addi r4,r13,-32756` = 0x80417D8C = `3e4ccccd`) into +0x2C four times
  //   each, and then stores 0.3f to +0x40 and 0.2f to +0x44 (`lfs f1,-32764(r2)` =
  //   0x8041A3C4, `lfs f0,-32760(r2)` = 0x8041A3C8).
  //
  // The one consumer of the two floats is `fn_800597D8` (0x80059928-0x8005993C), which adds
  // `+0x40` and `+0x44` and picks 1000 over 4000 from the result. **`CMain::DrawDebugMetrics`
  // is 0x6C bytes and reads neither** - it toggles a global and calls `CMemory::GetMetrics` -
  // so `docs/research/boot_path.md` row 10's "DrawDebugMetrics reads them" is wrong and the
  // two floats feed a sleep-time heuristic instead.
  struct SFrameTimeHistory {
    int count;
    float values[4];
  };

  COsContext* osContext;
  void* x4_unk1;
  CMemorySys* memorySys;
  void* xc_unk2;
  double x10_unk;
  SFrameTimeHistory x18_frameTimeHistory;
  SFrameTimeHistory x2c_frameTimeHistory;
  float x40_frameTimeTotal;
  float x44_frameTimeTotal;
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
  /** +0x94: the `CGameArchitectureSupport` `CMain::RsMain` allocates at 0x80005E30. */
  CGameArchitectureSupport* x94_cGameArchitectureSupport;
};

extern CMain* gpMain;

#endif // _CMAIN
