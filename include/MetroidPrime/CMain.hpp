#ifndef _CMAIN
#define _CMAIN

#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CStopwatch;
class CGameGlobalObjects;
class CGameArchitectureSupport;
class CMemorySys;

#ifdef TARGET_PC
// Port: retail's two frame-time histories, `CMain`+0x18 and +0x2C, twenty bytes each
// (`fn_800069AC`: an `int` count at +0, four `float`s from +4).
struct SFrameTimeHistory {
  int count;
  float values[4];
};
#endif

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

  CMain(COsContext* context, void* unk1, CMemorySys* memorySys, void* unk2);
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
  void OpenWindow();
  void SetRestartMode(ERestartMode s) { restartMode = s; }
  ERestartMode GetRestartMode() const { return restartMode; }
  // Retail 0x80005C64, 8 bytes: `stw r4, 0x48(r3) ; blr` - one store of the argument into
  // +0x48, which is `frameTimeMinimum`. **Declared and defined out of line, not inline**:
  // nothing in the port calls it, so an inline body is never emitted, and the matching build
  // left `SetFrameTimeMinimum__5CMainFi` at 0% forever. The definition is in
  // `src/MetroidPrime/main.cpp`, immediately before `CMain::RsMain` (0x80005C6C) because
  // 0x80005C64 is retail's function order between the two.
  void SetFrameTimeMinimum(int time);
  // void SetCardBusy(bool v) { x160_31_cardBusy = v; }

  void SetMaxSpeed(bool v); // {x160_26_screenFading = v; }

  bool fn_80008A1C();

  void SetX30(bool v) { x90_30_ = v; }

  static void EnsureWorldPaksReady();
  static void EnsureWorldPakReady(CAssetId id);

  void Increment_x5c(float f) { x5c + f; }
  bool GetFinished() const { return finished; }

  // `SetGameFrameDrawn` (0x800089AC) is `lbz 145(r3) ; rlwimi r0,r4,7,24,24 ; stb 145(r3)`, i.e.
  // bit 0 of the byte at **+0x91** - field 0 of that byte's group, and it is *not* `finished`
  // (bit 0 of +0x90, mask 0x80). `CMain::RsMain` reads it back at 0x800062B0 to decide whether to
  // sleep, which is a frame-tick debt clamp and not a quit test. `rlwimi r0,rX,7-n,24+n,24+n` is
  // field *n* counted down, so +0x91's bit 0 is field 0 of the group the `gameFrameDrawn` bitfield
  // opens - which is why the bitfield is the ninth one and not part of the eight at +0x90.
  //
  // **Declared unconditionally, and that is a fix.** Both this pair and the `gameFrameDrawn` bit
  // were `TARGET_PC`-only, so the matching build neither declared nor emitted
  // `SetGameFrameDrawn__5CMainFb` and it read 0.00% against retail's 16 bytes.
  void SetGameFrameDrawn(bool drawn);
  bool GetGameFrameDrawn() const { return gameFrameDrawn; }

#ifdef TARGET_PC
  // Port. Upstream models +0x10..+0x48 as `char mPad[0x38]`; retail's is a `double`, two 20-byte
  // frame-time histories and their two running totals, and `CMain::RsMain`'s port body
  // (`src/MetroidPrime/PortBoot.cpp`) pushes a sample into each history every frame through
  // `fn_800069AC`. **The shape is `{int n; float v[4]}`** - `fn_800069AC` reads `lwz r0,0(r3)` as a
  // count, compares it with 4, writes `stfs f0,4(r5)` with `r5 = r3 + count*4` and finally
  // `stfs f0,4(r3)` - so it is an `int` at +0 and four `float`s at +4/+8/+0xC/+0x10, 20 bytes, and
  // the second one lands exactly on 0x2C. 8 + 20 + 20 + 4 + 4 = **0x38**, so every offset and the
  // total size are exactly what the `char mPad[0x38]` gave; the derivation is at the members.
  // See `docs/research/boot_path.md`, "CMain offsets, as this path reads them".
  // `SetFrameTimeMinimum` used to be an inline here. It is declared unconditionally and defined
  // in `src/MetroidPrime/main.cpp` now, so the MWCC build emits it; see the declaration above.
  // `SetGameFrameDrawn` and `GetGameFrameDrawn` moved up out of this block for the same reason.
  // `gameGlobalObjects` is private and `src/MetroidPrime/PortStreamNewGameState.cpp` is an
  // `extern "C"` free function, so it needs an accessor. **The offset may not be spelled
  // instead**: retail reads it as `lwz r3,84(r28)`, which is `CMain`+0x54 in a 32-bit GameCube
  // object, and every pointer in this class is eight bytes wide on the host.
  CGameGlobalObjects* GetGameGlobalObjects() const { return gameGlobalObjects; }
#endif

  // // TODO
  // COsContext& InitOsContext() {
  //   OpenWindow();
  //   return x0_osContext;
  // }

private:
  COsContext* osContext;
  void* mUnk1;
  CMemorySys* memorySys;
  void* mUnk2;
#ifdef TARGET_PC
  // Port: retail's +0x10..+0x48, which upstream models as `char mPad[0x38]` (kept below for the
  // matching build). Named at the offsets retail's own accesses give: `stfd f2,16(r3)` (+0x10, a
  // `double`), the two histories (+0x18, +0x2C) that `fn_800069AC` pushes into, and the two totals
  // at +0x40/+0x44 (`fn_80006954` sums a history and `RsMain` stores the result). 8+20+20+4+4 =
  // 0x38. The port's `RsMain` (src/MetroidPrime/PortBoot.cpp) is the only reader and writer; on
  // the host the pointers are eight bytes, so nothing here is addressed by a spelled offset.
  double x10_unk;
  SFrameTimeHistory updateFrameTimeHistory;
  SFrameTimeHistory drawFrameTimeHistory;
  float x40_frameTimeTotal;
  float x44_frameTimeTotal;
#else
  char mPad[0x38];
#endif
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
  bool mManageCard : 1;
  bool x90_29_ : 1;
  bool x90_30_ : 1;
  bool mCardBusy : 1;
  // +0x91, bit 0 - the ninth one-bit group, and the reason the eight above end at +0x90. See
  // `SetGameFrameDrawn` above. **Not `TARGET_PC`-only**: with it inside that block the matching
  // build had no member at +0x91 and could not emit `SetGameFrameDrawn__5CMainFb` at all. It costs
  // the matching build's `sizeof(CMain)` nothing - nine bits still round up to the same 0x94 - and
  // the port's `CMain::RsMain` (`src/MetroidPrime/PortBoot.cpp`) is host-only either way.
  bool gameFrameDrawn : 1;
#ifdef TARGET_PC
  // Upstream has both of these commented out. Retail's `sizeof(CMain)` is 0x98 and the
  // constructor's last store is `stw r8,148(r3)` = +0x94, which is the `CGameArchitectureSupport`
  // that `CMain::RsMain` allocates with `li r3,356` at 0x80005E08 and stores at 0x80005E30 - the
  // missing four bytes.
  // TARGET_PC only: the matching build compiles upstream's 0x94-byte object and the port's `RsMain`
  // is host-only (see `src/MetroidPrime/PortBoot.cpp`), so nothing there reads the field.
  CGameArchitectureSupport* mGameArchitectureSupport;
#endif
};

extern CMain* gpMain;

#endif // _CMAIN
