// The host-only bodies of CMain::OpenWindow and CMain::RsMain.
//
// Like src/MetroidPrime/PortGlobals.cpp, this file is deliberately NOT a unit in
// `configure.py`, for the same reason stated there: a definition inside a unit shifts that
// unit's small-data offsets and re-optimises unrelated functions, which `tools/gate.sh`
// reports as a regression. `MetroidPrime/main.cpp` is a `NonMatching` unit full of functions
// other lanes are working on (20 of its 99 are at 100% inside a unit whose bytes are not in
// the link), so the one function in it that cannot be written for the console is written
// here instead, and main.cpp keeps only the `#ifndef TARGET_PC` guard around its empty body.
// mwcceppc does not define TARGET_PC, so main.cpp compiles byte for byte what it compiled
// before this file existed, and the matching build cannot move.
//
// The short version of what is below, so nobody has to read the whole file to know what it
// is: **this does not reach a frame, and it does not pretend to.** The port does not link
// (724 unaccounted symbols, measured by `tools/link_gap.py`), so nothing here can be
// executed or observed; what it does is make the bring-up *sequence* real and measurable,
// and name the exact place the sequence stops. docs/research/boot_path.md is the full map.

// ---------------------------------------------------------------------------
// Retail has no CMain::OpenWindow. Measured, not assumed.
// ---------------------------------------------------------------------------
//
// The brief this file answers asked for `CMain::OpenWindow` on the grounds that "retail has
// it; ours does not exist". It does not. Four independent measurements:
//
//  1. `config/G2ME01/symbols.txt` names 19 `CMain` methods - `__ct__`, `__dt__`, `RsMain`,
//     `InitializeSubsystems`, `ShutdownSubsystems`, `AsyncIdle`, `CheckReset`,
//     `CheckTerminate`, `DrawDebugMetrics`, `MemoryCardInitializePump`, `AddWorldPaks`,
//     `EnsureWorldPaksReady`, `ResetGameState`, `StreamNewGameState`, `FillInAssetIDs`,
//     `SetFrameTimeMinimum`, `SetGameFrameDrawn`, `SetMaxSpeed`, `fn_80008A1C` - and
//     `OpenWindow` is not among them. The demangler named every other member of the class.
//  2. The string `OpenWindow` occurs nowhere in `powerpc-eabi-objdump -d` of the whole
//     `build/G2ME01/main.elf` (985,921 lines, every `.text` symbol in the DOL).
//  3. `CMain::RsMain` (0x80005C6C, 0x864 bytes) is fully disassembled and makes *no* call
//     on `x0_osContext`. The only two uses of the pointer are `lwz r4,0(r31)` at 0x80005CDC
//     and 0x80005E20, feeding `CGameGlobalObjects::CGameGlobalObjects` and
//     `CGameArchitectureSupport::CGameArchitectureSupport`.
//  4. Retail's window/VI bring-up is in `main` (0x801EFB00), the caller of `InvokeCMain`.
//     It builds an 8-byte object at r1+8 with `fn_802BE85C` and passes it as `InvokeCMain`'s
//     sixth argument. `fn_802BE85C` -> `fn_802C329C` -> `fn_802C2FD4`, and each has exactly
//     one caller, so that is the whole chain. `fn_802C2FD4` is the function that does
//     `VIGetTvFormat` -> `GXAdjustForOverscan` -> two framebuffers -> `VIConfigure` ->
//     `VIFlush` -> `GXInit` -> `GXSetCopyFilter`.
//
// Note 4 is *our* `COsContext::OpenWindow`'s shape, arrived at from Metroid Prime's port
// rather than from this DOL, and it acts on a render mode at 0x80417264 that belongs to CGX,
// not on a `COsContext` member: nothing in the DOL ever reads `COsContext` +0x30. The only
// `COsContext` fields retail does read through that chain are +0x24 and +0x2C (the first
// external framebuffer and its size), by `fn_802C33F8`. `include/MetroidPrime/CMain.hpp:51`'s
// `void OpenWindow();` is Metroid Prime carry-over and has no counterpart here.
//
// So this definition exists for one reason: the port needs the VI bring-up to happen at a
// point the port controls, and `CMain::RsMain` is that point. It is host-only and it is
// *not* retail's behaviour, and it must not grow into something that pretends otherwise.
#ifdef TARGET_PC

#include "Kyoto/Basics/COsContext.hpp"

#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "MetroidPrime/CMain.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "dolphin/ar.h"
#include "dolphin/arq.h"

#include <stdio.h>

// Defined below, after `CMain::RsMain`, which calls it at retail's step 11.
void PortInitializeSubsystems();
// `CARAMManager`'s initialiser and the ARAM base it reads, both in src/Kyoto/CARAMManagerPort.cpp.
extern "C" void fn_80301CC4(uint chunkSize1, uint size1, uint chunkSize0);
extern "C" uint lbl_80418BA8;

namespace {
// The title and size are retail's own defaults, read off the retail chain: the VI mode
// chosen at 0x802C2FD4 is GXNtsc480IntDf (640x480, the value `GXNtsc480IntDf` carries) and
// the console's is opened fullscreen=false. Aurora ignores both - `aurora_initialize` has
// already created and titled the window from `AuroraConfig::appName` before the game is
// entered, and `COsContext::OpenWindow` declines to set the title for exactly that reason
// (src/Kyoto/Basics/COsContext.cpp:178) - but they are passed because the seam takes them,
// and because whoever widens the render mode for widescreen does it through the w/h
// arguments of this one call.
const char kWindowTitle[] = "Metroid Prime 2: Echoes";
const int kWindowWidth = 640;
const int kWindowHeight = 480;
} // namespace

// What retail does here, and where. Retail's equivalent is the chain in note 4 above:
// `fn_802C2FD4`, reached from `main` before `InvokeCMain` is ever called, on CGX's render
// mode rather than a `COsContext` member. This is the same bring-up moved to the point the
// port controls, and it is the *only* thing in this file that is real work rather than a
// placeholder: `COsContext::OpenWindow` is written (src/Kyoto/Basics/COsContext.cpp), it is
// an Aurora VI adapter, and `VIConfigure` is the one call in it Aurora acts on. So after
// this returns, Aurora knows the EFB/XFB shape the game's GX work will produce.
//
// The return value is discarded because retail's only caller discards it too.
void CMain::OpenWindow() {
  osContext->OpenWindow(kWindowTitle, 0, 0, kWindowWidth, kWindowHeight, false);
}

// The bring-up, and the point where the sequence stops.
//
// Retail's `RsMain` is 2,148 bytes (0x80005C6C, 0x864) and its order is measured, step by
// step, in docs/research/boot_path.md. The parts of it that exist today in this tree, using
// that document's step numbers:
//
//   6. `CMain::RsMain`                    - this function.
//   7. `new CGameGlobalObjects(os, mem)`  - the retail constructor, Matching, in
//                                            src/MetroidPrime/CGameGlobalObjectsCtor.cpp, with
//                                            the `CGameState` chain it allocates (2026-09-26,
//                                            lane `chain`; docs/research/port_link_gap.md).
//   8. `fn_80003A18(this)`                - not in this tree.
//  11. `InitializeSubsystems()`           - ARInit plus a TODO.
//  12. `CGameGlobalObjects::PostInitialize`- calls an unwritten `AllocateRenderer`.
//  13. `CGameGlobalObjects::AddPaksAndFactories` - empty, and 1,936 bytes in retail.
//  14/16. `AddWorldPaks()` / `FillInAssetIDs()` - written; need step 13's factory.
//  17. `new CGameArchitectureSupport(os)` - written, and it faults on the host.
//  21. the frame loop                     - written, and unreachable without step 17.
//
// Step 17 is the wall, and it is a wall of null dereferences rather than of missing code.
// `CGameArchitectureSupport::CGameArchitectureSupport` (0x80007EC4, **0x3F8 = 1016 bytes**,
// 0x80007EC4..0x800082BC) makes two unguarded global dereferences and no others of
// that shape:
//
//   1. 0x80007F38 `lwz r29,-28220(r13)` = **0x80418F44 `gpTweakPlayerA`**, then
//      `mr r3,r29; bl GetRightAnalogMax` at 0x80007F40 and `bl GetLeftAnalogMax` at
//      0x80007F4C. **Fixed on the port**: `port::tweaks::CreateStandInTweakPlayers()`
//      (src/MetroidPrime/PortTweakGlobals.cpp, called from platform/main.cpp right
//      after `port::modules::InitAll()`) gives both player slots real 4-byte cells
//      over a zeroed `SLdrTweakPlayer`, so all five accessors answer 0.0f. The data
//      behind them is `Standard.NTWK` in a pak, and the paks are step 13, so this is
//      a stand-in with no tweak data behind it and is named as one.
//   2. 0x800081A4 `lwz r3,-28360(r13)` = **0x80418EB8 `gpGameState`**, then
//      `addi r3,r3,128; bl EnsureOptions__12CGameOptionsFv` at 0x800081AC.
//      **Not fixable on the port today, and the reason is not the paks.** That claim
//      was in this comment and in boot_path.md; it is wrong, and the disassembly is
//      what refutes it. `gpGameState` is written by
//      `CGameGlobalObjects::CGameGlobalObjects` at **0x80008548**
//      (`lwz r4,304(r31); stw r4,-28360(r13)`, r31+0x130 = the
//      `rstl::single_ptr<CGameState>`), which fills that member itself at 0x800084D0
//      with `operator new(752)` and 0x800084DC with `fn_801449C8` - `sizeof(CGameState)`
//      is retail's 0x2F0. `CMain::RsMain` calls that constructor at 0x80005CE4, i.e.
//      **step 7**, before `PostInitialize` (12) and long before `AddPaksAndFactories`
//      (13). So `gpGameState` needs step 7 and not step 13, and
//      `CMain::StreamNewGameState` is not on its path at all. What it needs instead is
//      `CGameState::CGameState()` - `fn_801449C8`, past 0x80144B3C, a real DOL unit
//      with eight nested constructors in it (`fn_8015C34C` for a 1200-byte
//      `CWorldState`, `__ct__12CGameOptionsFv`, `fn_80180738`, `fn_80146154`, two
//      `fn_80144924` + `fn_80004A4C` pairs, `fn_80193E08` and more past 0x80144B40) -
//      and that function had no body in this tree. (Superseded twice: it is now written and
//      `Matching` in src/MetroidPrime/Player/CGameStateCtor.cpp, and since 2026-09-26 it is in
//      the port build, called from the retail `CGameGlobalObjects` constructor below. Six of
//      its nested callees are still unwritten; none of the six runs before step 17 except
//      `fn_80145C98`. See docs/research/port_link_gap.md.) It cannot be stood in for either: the object is
//      0x2F0 bytes of nested state and `EnsureOptions` would then run against whatever
//      the stand-in left in it.
//
// So the second dereference stays, and the check below is what stands in front of
// it. A boot that null-derefs on frame 0 tells nobody anything; a boot that stops
// here and names the two globals and the two functions it is waiting for is a
// better state than a crash, and it is the state this file can honestly reach.
//
// **There is therefore no loop below, and that is the finding, not an omission.** Retail's
// loop begins at 0x80006034 and its body is, in order: a `CStopwatch` update,
// `CGameArchitectureSupport::UpdateTicks`, a virtual draw through `gpRender`'s vtable slot
// +0x94, `CMain::DrawDebugMetrics`, `CGameArchitectureSupport::Update`, `fn_80003858`,
// `CMain::CheckTerminate` and `CMain::CheckReset`. `UpdateTicks`, `DrawDebugMetrics`, `Update`,
// `CheckTerminate` and `CheckReset` are all written; the two that are not - the draw and
// `fn_80003858` - are missing, and the draw needs `gpRender`, which is null until step 12
// succeeds. A loop with no frame in it would spin, and both ways to bound it are worse than
// not having one:
//
//   - the exit event is `AURORA_EXIT`, and `aurora/event.h` needs `<SDL3/SDL_events.h>`,
//     which `MP_SDK_HEADERS_ONLY=ON` does not provide (the only verified port configuration,
//     PORT_NOTES.md "Building"). Re-declaring Aurora's event enum inside the game to get
//     around a missing header would be a lie about an ABI this port does not own. The
//     Metroid Prime port reads the event in the game's own main loop and can, because it
//     builds with Aurora linked.
//   - the bit retail's loop tests is `CMain`+0x90 bit 7 - `rlwinm. r0,r0,25,31,31` at
//     0x8000645C, which the header calls `x90_31_cardBusy` and which is private with no
//     writer in this tree. (`finished` is bit 0 of the same byte: `clrlwi r0,r0,31` at
//     0x80006314 tests it and `rlwimi r0,r3,0,31,31` at 0x80006338 clears it, after
//     incrementing `CGameArchitectureSupport`+0x64 once.)
//
// So: the bring-up that exists is done, and the function returns. When steps 7-13 land, the
// loop goes here, with the same calls in the same order.
int CMain::RsMain(int argc, const char* const* argv) {
  (void)argc;
  (void)argv;

  OpenWindow();

  // Retail's step 7: `li r3,356`, `operator new`, and `CGameGlobalObjects::CGameGlobalObjects(
  // *x0_osContext, *x8_memorySys)` at 0x80005CE4, stored at `CMain`+0x54 (`stw r0,84(r31)`). The
  // constructor is `src/MetroidPrime/CGameGlobalObjectsCtor.cpp`, Matching, and it is what fills
  // `gpGameState` (0x80008548), `gpResourceFactory`, `gpSimplePool`,
  // `gpCharacterFactoryBuilder` and `gpTweakManager`. The two globals the frame path needs are
  // still checked below by name, because a constructor that ran is not the same thing as a
  // constructor whose callees all have bodies.
  gameGlobalObjects = new CGameGlobalObjects(*osContext, *memorySys);

  if (gpTweakPlayerA == nullptr) {
    printf("%s",
           "boot stopped: gpTweakPlayerA (DOL 0x80418F44) is null.\n"
           "  Written only by Tweaks.rel REL_CreateTweakGlobals (module .text 0x78C), which needs\n"
           "  REL_LoadTweaks and therefore a pak. The port's stand-in is\n"
           "  port::tweaks::CreateStandInTweakPlayers() (src/MetroidPrime/PortTweakGlobals.cpp).\n");
    return 1;
  }
  if (gpGameState == nullptr) {
    printf("%s",
           "boot stopped: gpGameState (DOL 0x80418EB8) is null after\n"
           "  CGameGlobalObjects::CGameGlobalObjects ran. It stores gameState.get() at 0x80008548,\n"
           "  and gameState is `new CGameState` through fn_801449C8 - so CGameState's operator new\n"
           "  or its constructor (src/MetroidPrime/Player/CGameStateCtor.cpp) returned null.\n"
           "  CGameArchitectureSupport's constructor dereferences gpGameState at 0x800081A4 with\n"
           "  no null test.\n");
    return 1;
  }

  // 11. `CMain::InitializeSubsystems()` - retail calls it at 0x80005D3C, between the
  //     `CGameGlobalObjects` constructor (0x80005CE4) and `PostInitialize` (0x80005D4C). This
  //     ladder skipped it, and on the host that meant **ARAM never came up**: no `ARInit`, and no
  //     `fn_80301CC4`, so `CARAMManager` had no pools. `AddPaksAndFactories` puts four of its six
  //     paks in ARAM (`aram:MiscData`, `aram:TestAnim`, `aram:MidiData`, `aram:GGuiSys`) and
  //     `STRG_Main` is in MiscData, so step 12 cannot find its string table without this.
  printf("%s", "boot: step 11 - CMain::InitializeSubsystems (host body)\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  PortInitializeSubsystems();

  // 17. `CGameGlobalObjects::PostInitialize(os, memorySys)` - retail's **step 12**, and it comes
  //     *before* step 17. `CGameGlobalObjects::PostInitialize` is `Matching` 100.00% and is in the
  //     port build, and nothing was calling it: this ladder stopped after step 16, so the boot
  //     never reached the code that assigns `gpRender`.
  //
  //     That is worth stating plainly, because a lane had already reported "`gpRender` is no
  //     longer null" - **true of the object file and false of the running program.** A renderer
  //     pointer that nothing ever assigns is still null at step 21c, where the frame loop
  //     dereferences `gpRender`'s vtable with no null test.
  //
  //     Retail's order inside it (src/MetroidPrime/main.cpp:235-241): `AddPaksAndFactories()`,
  //     `LoadStringTable()`, `AllocateRenderer(...)`, then `gpRender = renderer.get()`, then
  //     `CEnvFxManager::Initialize()`. **`gpRender` is assigned in the middle**, so if this
  //     returns, the frame loop's vtable call has a real target for the first time.
  printf("%s", "boot: step 12 - CGameGlobalObjects::PostInitialize(*osContext, *memorySys)\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  gameGlobalObjects->PostInitialize(*osContext, *memorySys);
  printf("%s", "boot: step 12 returned\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  if (gpRender == nullptr) {
    printf("%s",
           "boot stopped: step 12 returned but gpRender is still null, so AllocateRenderer\n"
           "  answered null or the store at main.cpp:240 did not happen. 0x8026EF54 is written\n"
           "  (src/MetaRender/Carve8026EF54.cpp) and NonMatching on a proven mwldeppc alignment\n"
           "  wall, so it runs - but it returns a pointer to an UNCONSTRUCTED object until\n"
           "  fn_80271238 (1436 bytes) is written. That is the next renderer unit.\n");
    return 1;
  }
  printf("%s", "boot: step 12 - gpRender is non-null for the first time\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy

  // Both globals are real, and **that is what makes the next line possible.** The guard that
  // stood here printed a message and returned, on the stated grounds that the constructor
  // "makes two unguarded global dereferences": `gpTweakPlayerA` at 0x80007F38 and `gpGameState`
  // at 0x800081A4. Both conditions have since been met and the probe confirms it -
  // `CreateStandInTweakPlayers()` supplies the first, and `CGameStateCtor.cpp` (Matching, in
  // the port build since 2026-09-26) is what fills the second. The comment above this line
  // says so itself and then declines to act on it, so **the stop was a stale guard, not a
  // wall.**
  //
  // So step 17 is now *attempted* rather than described. The markers around the call are the
  // point: a hard-coded message cannot distinguish "this faults" from "this was never tried",
  // and those need completely different work. If it faults, the backtrace names the callee;
  // if it returns, the boot has moved three steps and the message below says what is next.
  printf("%s", "boot: step 17 - new CGameArchitectureSupport(*osContext)\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  CGameArchitectureSupport* architectureSupport = new CGameArchitectureSupport(*osContext);
  printf("%s", "boot: step 17 returned - the constructor completed\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  (void)architectureSupport;

  // 18. `CIOWinManager`'s constructor, then `PumpMessages`. The manager is boot step 18's
  //     IOWin registry and it is `Matching` (`src/MetroidPrime/CIOWinManagerCtor.cpp`), so this
  //     costs **no new link symbols**. The four IOWin constructors it would hold -
  //     `CMainFlow` is `Matching` and in the port build, while `CConsoleOutputWindow` and
  //     `CAudioStateWin` are deliberately `EXCLUDED` from it - are *not* called here, because
  //     **adding a call to a symbol the port does not define would raise the undefined count**,
  //     and that count is the number the port is being planned against. Steps 18's remaining
  //     constructors are a listing decision, not a ladder decision.
  printf("%s", "boot: step 18 - CIOWinManager\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  CIOWinManager ioWinManager;
  printf("%s", "boot: step 18 returned - CIOWinManager constructed\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy

  // 19. `CGameOptions::EnsureOptions`, retail 0x801612C4, 0x10C. Already written in
  //     `src/MetroidPrime/Player/CGameOptions.cpp:207` and in the port build, and the object
  //     comes from `gpGameState`, which step 7 filled - `CGameState::GameOptions()` returns the
  //     `CGameOptions` member at +0x80. **Retail reads its bitstream out of a pak**, which
  //     needs step 13, so this is expected to do less than retail's does on the port; the
  //     marker is here so that is visible rather than assumed.
  printf("%s", "boot: step 19 - CGameOptions::EnsureOptions via gpGameState\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  gpGameState->GameOptions().EnsureOptions();
  printf("%s", "boot: step 19 returned\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy

  // 20. `CDvdFile::FileExists`, retail 0x8030C04C, `static bool FileExists(const char*)`, in the
  //     port build via `src/Kyoto/DolphinCDvdFile.cpp` and needing nothing. On a PC there is no
  //     disc, so the honest expectation is `false` - **and that is the answer worth printing**,
  //     because a `true` here would mean the port is reading a real retail pak.
  printf("%s", "boot: step 20 - CDvdFile::FileExists\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  const bool saveFileExists = CDvdFile::FileExists("menu/MeleeTitle.mrs");
  printf("boot: step 20 returned - FileExists(\"menu/MeleeTitle.mrs\") = %s\n",
         saveFileExists ? "true" : "false");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy

  printf("%s",
         "boot stopped: steps 12, 17, 18, 19 and 20 all COMPLETE, and the boot stops here.\n"
         "  `CGameArchitectureSupport`'s constructor ran to completion on the host - the two\n"
         "  unguarded global dereferences it makes (gpTweakPlayerA at 0x80007F38, gpGameState at\n"
         "  0x800081A4) are both satisfied. What is left is retail's step 18 onward: the update\n"
         "  and draw calls listed in docs/research/boot_path.md, which are not written yet.\n"
         "  Three functions that were long named as the wall are not it:\n"
         "    CMain::ResetGameState       98.61%  NonMatching - blocked, proven, see HANDOFF\n"
         "    CErrorOutputWindow         78.56%  NonMatching - blocked, proven, see HANDOFF\n"
         "    CConsoleOutputWindow       98.17%  ctor only; the class is 82%% unwritten\n");
  return 1;
}

// `CMain::InitializeSubsystems`, host-only. Retail's is 348 bytes at 0x80008680 and is
// written in src/MetroidPrime/main.cpp behind `#ifndef TARGET_PC`; the full measurement of
// why two of its six blocks cannot run here is in the comment on the retail body, and it
// reduces to two facts:
//
//   - Aurora's `ARInit` only stores the pointer it is given and Aurora's `ARAlloc`
//     dereferences it on the next line (`*AR_BlockLength = length; AR_BlockLength += 1;`,
//     extern/aurora/lib/dolphin/AR.cpp:71-73), and its own `AURORA_ASSERT(!(length & 0x1f))`
//     rejects the guest word retail passes as a length. Retail's 0x803C5AB8 is three words
//     of DOL .bss and cannot be written from a PC process. The array below is the fix, and
//     it is a real 3-entry ARAM length stack because `ARInit`'s second argument is the
//     number of entries the hardware stack holds.
//   - retail's stack-guard block reads `OSGetCurrentThread()` +0x304/+0x308 as a stack
//     pointer, fills 8 KB *below* it with 0x7338D00D, and hands the range to
//     `OSProtectRange`/`DCFlushRange`. Aurora's `OSThread` has `stackBase`/`stackEnd` at
//     those same offsets, so this compiles and looks right, and it would then scribble over
//     8 KB of Aurora's heap that is not a stack. It is skipped, not approximated.
//
// What *is* reproduced is the part that is meaningful on a host: Aurora's ARAM comes up, so
// `ARAlloc`/`ARQInit` work, and the two printf diagnostics retail prints. There is no
// stack-guard fill to reproduce, and no `OSProtectRange`.
void PortInitializeSubsystems() {
  // `lbl_80418BA8` (.sdata 0x80418BA8: 00004000) is Aurora's `ARAM_STACK_START` too. It is
  // retail's own global and not a local here any more, because `fn_80301CC4` below reads it.
  static uint sAramLengthStack[3];

  ARInit(sAramLengthStack, 3);
  // `ARAlloc`'s argument is zero. Retail passes the guest word at 0x80418EA0, which
  // `fn_80009864` fills in with `*(u32*)0x80415980 * 14` and which is zero in the DOL as
  // built (.sbss), and `ARAlloc(0)` is legal on both the hardware bump allocator and
  // Aurora's - Aurora asserts only that the length is 32-byte aligned and that the request
  // fits inside ARAM.
  lbl_80418BA8 += ARAlloc(0);
  ARQInit();

  printf("%s", "Initializing subsystems");
  printf("Stack: 0x%8.8x down to 0x%8.8x\n", (unsigned)lbl_80418BA8, (unsigned)lbl_80418BA8);
  printf("ARAM stack pointer 0x%8.8x, %u of 3 length slots used\n", lbl_80418BA8,
         (unsigned)((sAramLengthStack[0] != 0) ? 1u : 0u));

  // Retail's third of the five (0x800087B0), and the one the pak loader cannot do without:
  // `CARAMManager`'s two pools, 6 MB of 2 KB chunks and the rest of ARAM in 4 KB chunks. It takes
  // the other two `ARInit` length slots. The other four (`fn_802DAE30`, `fn_8002ADC8`,
  // `fn_800E85A8`, `fn_800DC0B0`) are still unwritten and still skipped.
  fn_80301CC4(2048, 0x600000, 4096);

  // Retail's sixth and last call, and **the only one of the six that is written** - the other
  // five (`fn_802DAE30`, `fn_8002ADC8`, `fn_80301CC4`, `fn_800E85A8`, `fn_800DC0B0`) are retail
  // functions this tree has not decompiled. It was missing here, which is the asymmetry that
  // matters: the two blocks that *cannot* run on a host were skipped deliberately and
  // documented, and the one that *can* run was skipped by accident.
  //
  // It belongs after the printfs because that is retail's order, and order is the whole of what
  // a boot sequence is: `CFrameDelayedKiller` collects the objects whose destruction has to be
  // deferred past a stack unwind, so anything allocated before this call and destroyed after it
  // is relying on the killer already existing.
  CFrameDelayedKiller::Initialize();
}

// `CMain::ShutdownSubsystems`'s host body - the counterpart of `PortInitializeSubsystems` above,
// for the same two reasons and with the same shape (see
// `src/MetroidPrime/CMainShutdownSubsystems.cpp` and `src/MetroidPrime/mainTail.cpp`).
//
// What retail's function does that a host cannot do:
//
//   - Nine of its eleven calls are retail functions this tree has **no body for**
//     (`fn_800E8494`, `fn_802DAE24`, `fn_8002AD44`, `fn_801F03C4`, `fn_801F02C4`, `fn_801F025C`,
//     `fn_801F05D0`, `fn_80218760`, `fn_801F0280`, `fn_801F0308`, `fn_801F0518`, `fn_800DC03C`),
//     so on a PC build the retail body is a link error before it is a run-time hazard.
//   - The last block walks `OSGetCurrentThread()` +0x304/+0x308 as a stack pointer, scans 8 KB
//     *below* it for the guard word and `OSReport`s the distance. Aurora's `OSThread` has
//     `stackBase`/`stackEnd` at those offsets, so it compiles and reads something plausible, and
//     what it reads is Aurora's allocator's memory rather than a stack. `PortInitializeSubsystems`
//     skips retail's fill of that same block for the same reason; skipping the scan is the
//     symmetric choice.
//
// What *is* reproduced is the one call in the function that is written, safe and meaningful on a
// host, and that retail makes first: `CFrameDelayedKiller::ShutDown()`, which flushes the
// deferred-destruction lists. Note that nothing in the port calls
// `CMain::ShutdownSubsystems` yet - `InvokeCMain` runs `RsMain` and then `~CMain`, and `RsMain`
// is the port's own stub - so this is here so the unit can be a `Matching` object in the port's
// build at all, and so the one safe call is not lost when it is wired up.
void PortShutdownSubsystems() { CFrameDelayedKiller::ShutDown(); }

#endif // TARGET_PC
