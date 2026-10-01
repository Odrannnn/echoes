// The host-only body of CMain::RsMain.
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
// Retail has no CMain::OpenWindow, and the port no longer stands one in.
// ---------------------------------------------------------------------------
//
// The brief this file used to answer asked for `CMain::OpenWindow` on the grounds that "retail
// has it; ours does not exist". It does not. Four independent measurements, all still true:
//
//  1. `config/G2ME01/symbols.txt` names 19 `CMain` methods - `__ct__`, `__dt__`, `RsMain`,
//     `InitializeSubsystems`, `ShutdownSubsystems`, `AsyncIdle`, `CheckReset`,
//     `CheckTerminate`, `DrawDebugMetrics`, `MemoryCardInitializePump`, `AddWorldPaks`,
//     `EnsureWorldPaksReady`, `ResetGameState`, `StreamNewGameState`, `FillInAssetIDs`,
//     `SetFrameTimeMinimum`, `SetThirtyFps`, `SetMaxSpeed`, `GetMaxSpeed` - and
//     `OpenWindow` is not among them. The demangler named every other member of the class.
//  2. The string `OpenWindow` occurs nowhere in `powerpc-eabi-objdump -d` of the whole
//     `build/G2ME01/main.elf` (985,921 lines, every `.text` symbol in the DOL).
//  3. `CMain::RsMain` (0x80005C6C, 0x864 bytes) is fully disassembled and makes *no* call
//     on `x0_osContext`. The only two uses of the pointer are `lwz r4,0(r31)` at 0x80005CDC
//     and 0x80005E20, feeding `CGameGlobalObjects::CGameGlobalObjects` and
//     `CGameArchitectureSupport::CGameArchitectureSupport`.
//  4. Retail's window/VI bring-up is in `main` (0x801EFB00), the caller of `InvokeCMain`.
//     It builds an 8-byte object at r1+8 with `fn_802BE85C` - `CGraphicsSys::CGraphicsSys` -
//     and passes it as `InvokeCMain`'s sixth argument. `fn_802BE85C` -> `fn_802C329C`
//     (`CGraphics::Startup`) -> `fn_802C2FD4` (`CGraphics::ConfigureVideo`), and each has
//     exactly one caller, so that is the whole chain. `fn_802C2FD4` is the function that does
//     `VIGetTvFormat` -> `GXAdjustForOverscan` -> two framebuffers -> `VIConfigure` ->
//     `VIFlush` -> `GXInit` -> `GXSetCopyFilter`.
//
// Note 4 is what the port now does, at the point retail does it. `CGraphicsSys`'s constructor is
// `fn_802BE85C` and its body is `CGraphics::Startup(mOsContext, progressive)` - the chain in
// measurement 4 verbatim - and `platform/main.cpp` builds it after `CMemorySys` and before
// `InvokeCMain`, which is retail's own order in `main`. The bodies are in
// `src/Kyoto/Graphics/CGraphicsHostStartup.cpp`, port-only and listed in `files.cmake`.
//
// **What replaced the stand-in, and why it is not a loss.** Until 2026-09-29 this file called
// `mOsContext->OpenWindow(kWindowTitle, 0, 0, 640, 480, false)` from `RsMain`, which is
// `COsContext::OpenWindow` - an adapter that does `VIGetTvFormat` -> `GXAdjustForOverscan` -> two
// `OSAllocFromArenaLo` framebuffers -> `VIConfigure` -> `VIFlush`. It wrote a render mode into
// `COsContext::mRenderMode`, at `COsContext`+0x30, **which nothing in the DOL ever reads** - the
// renderer reads CGX's own `mRenderModeObj__9CGraphics` at 0x80417264. So the stand-in configured
// an object the game does not look at, and `CGraphicsHostScene.cpp` had to gate its fade quad and
// its `GXCopyDisp` on a zero `fbWidth` because the render mode the game *does* read was never
// filled. `CGraphics::ConfigureVideo` fills that one, allocates the two framebuffers out of
// `COsContext`'s arena block the way retail's `Startup` does, and calls `GXInit`.
//
// `COsContext::OpenWindow` is therefore **no longer called from anywhere**; it is kept because it
// is a written body of the class and because `COsContext`'s own members are still its business,
// but nothing on the boot path reaches it. `include/MetroidPrime/CMain.hpp:58`'s
// `void OpenWindow();` is Metroid Prime carry-over with no counterpart in this DOL; the
// declaration stays, and the definition is gone, so the name is uncalled rather than wrong.
#ifdef TARGET_PC

#include "Kyoto/Basics/COsContext.hpp"

#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "MetroidPrime/CMain.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "dolphin/ar.h"
#include "dolphin/arq.h"

#include "Kyoto/CARAMToken.hpp"
#include "Kyoto/CResFactory.hpp"
#include "MetroidPrime/CMemoryCard.hpp"

#include <stdio.h>
#include <stdlib.h>

// Defined below, after `CMain::RsMain`, which calls it at retail's step 11.
void PortInitializeSubsystems();
// `CARAMManager`'s initialiser and the ARAM base it reads, both in src/Kyoto/CARAMManagerPort.cpp.
extern "C" void fn_80301CC4(uint chunkSize1, uint size1, uint chunkSize0);
extern "C" uint lbl_80418BA8;

// The frame loop's written callees that have no header: `fn_800069AC` is the frame-time
// history push (src/MetroidPrime/Carve800069AC.c), `fn_80003858` is src/MetroidPrime/Carve80003858.c,
// and `gpRelFileManager` (`CGameGlobalObjects`+0x150) is pumped once per phase.
extern "C" void fn_800069AC(void* history, const float* sample);
// `fn_80006954`, retail 0x80006954, 0x58: fills the 8-byte total below out of one of the two
// histories, and it calls `fn_80008B60` (0x80008B60, 0xC8), the mean. Both bodies are in
// src/MetroidPrime/PortFrameTimeHistory.c, which is port-only and claims nothing in the DOL.
// `void*`/`const void*` rather than a named struct, because the C file carries its own local
// copy of the shape and this side only ever has the address - as `fn_800069AC` above.
extern "C" void fn_80006954(void* out, const void* history);

extern "C" void fn_80003858(float f);
// Retail 0x8030172C, 0x20: the frame loop's per-frame DMA cleanup - a wrapper over
// `fn_8030174C`, which walks the active-DMA list `src/Kyoto/CARAMManagerPort.cpp` owns. Both
// bodies, with the disassembly they were read from, are there.
extern "C" void fn_8030172C();

// The 8-byte stack local the frame loop hands to `fn_80006954`: `addi r3,r1,32` at 0x8000610C
// and `addi r3,r1,24` at 0x8000622C, read back as `lfs f0,32(r1)` at 0x80006118 and
// `lfs f0,24(r1)` at 0x80006238. The call fills +0 with the mean and +4 with a flag
// (`stb r0,4(r31)`), and the frame loop stores only +0 - to `CMain`+0x40 and `CMain`+0x44.
// `src/MetroidPrime/PortFrameTimeHistory.c` carries the C-side copy of the same shape; the two
// are deliberately separate, that file being C, and a local here keeps the frame loop immune
// to either struct changing underneath it.
struct SFrameTimeTotal {
  float x0_value;
  unsigned char x4_valid;
};

// A frame-loop callee that is not written. It is a macro, not a function, so the innermost
// repo frame of the abort is `CMain::RsMain` on the line where retail makes the call - which is
// what the goal loop's boot scan names the item after. It aborts rather than calling the
// undefined symbol, so the port's undefined count does not rise. The message and the abort are
// one line on purpose: boot-progress.sh refuses a diff that touches a line printing
// "frame loop stopped", so a stop cannot be quietly turned into a no-op.
#define PORT_FRAME_STOP(name, addr)                                                                          \
  do {                                                                                                       \
    printf("frame loop stopped: %s (retail %s) is not written - frame %ld\n", name, addr, frame); fflush(nullptr); abort(); \
  } while (0)

namespace {
// The marker `PortInitializeSubsystems` writes into Aurora's ARAM length stack before
// `ARInit` so it can report how much of the stack is in use. 0xFFFFFFFF is not a legal
// Aurora length (`AURORA_ASSERT(AR_StackPointer <= mem2Size && length <= mem2Size -
// AR_StackPointer)`), so a slot still holding it has provably never been written by
// `ARAlloc`, which is the only writer.
const uint kAramSlotUnused = 0xFFFFFFFFu;

uint CountUsedAramSlots(const uint* slots, uint count) {
  uint used = 0;
  for (uint i = 0; i < count; ++i) {
    if (slots[i] != kAramSlotUnused) {
      ++used;
    }
  }
  return used;
}
} // namespace

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
// **The loop is below, as step 21**, and a callee that is not written is a stop on the line
// where retail calls it, so the run says which one is next. On the byte at `CMain`+0x90: retail's
// back-edge is `extrwi. r0,r0,1,24` at 0x8000645C, mask 0x80, the first-declared field -
// `finished`, set by `rlwimi r0,r3,7,24,24` at 0x800060C8 when `UpdateTicks` returns false. The
// `clrlwi. r0,r0,31` at 0x80006314 is mask 0x01, the eighth field - `mGameFrameDrawn` - and
// `rlwimi r0,r3,0,31,31` at 0x80006334 clears it after incrementing
// `CGameArchitectureSupport`+0x64. (An earlier version of this comment named the two the other
// way round; that was wrong.)
//
// The window's close event, `AURORA_EXIT`, is not read: `aurora/event.h` needs
// `<SDL3/SDL_events.h>`, which `MP_SDK_HEADERS_ONLY=ON` does not provide (PORT_NOTES.md
// "Building"), and re-declaring Aurora's enum here would be a lie about an ABI this port does
// not own. `MP_PORT_FRAMES` bounds the loop instead.
int CMain::RsMain(int argc, const char* const* argv) {
  (void)argc;
  (void)argv;

  // Retail's step 7: `li r3,356`, `operator new`, and `CGameGlobalObjects::CGameGlobalObjects(
  // *x0_osContext, *x8_memorySys)` at 0x80005CE4, stored at `CMain`+0x54 (`stw r0,84(r31)`). The
  // constructor is `src/MetroidPrime/CGameGlobalObjectsCtor.cpp`, Matching, and it is what fills
  // `gpGameState` (0x80008548), `gpResourceFactory`, `gpSimplePool`,
  // `gpCharacterFactoryBuilder` and `gpTweakManager`. The two globals the frame path needs are
  // still checked below by name, because a constructor that ran is not the same thing as a
  // constructor whose callees all have bodies.
  mGameGlobalObjects = new CGameGlobalObjects(*mOsContext, *mMemorySys);

  if (gpTweakPlayerA.null()) {
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

  // 17. `CGameGlobalObjects::PostInitialize(os, mMemorySys)` - retail's **step 12**, and it comes
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
  printf("%s", "boot: step 12 - CGameGlobalObjects::PostInitialize(*mOsContext, *mMemorySys)\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  mGameGlobalObjects->PostInitialize(*mOsContext, *mMemorySys);
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
  printf("%s", "boot: step 17 - new CGameArchitectureSupport(*mOsContext)\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  CGameArchitectureSupport* architectureSupport = new CGameArchitectureSupport(*mOsContext);
  printf("%s", "boot: step 17 returned - the constructor completed\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  // Retail stores it at `CMain`+0x94 (0x80005E30), and the frame loop reads it from there.
  mArchSupport = architectureSupport;

  // 18. `CIOWinManager`'s constructor, then `PumpMessages`. The manager is boot step 18's
  //     IOWin registry and it is `Matching` (`src/MetroidPrime/CIOWinManager.cpp`), so this
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

  // 21. The frame loop, retail 0x80006034-0x80006460, one statement per call and in retail's
  //     order. The measured body, with every constant, is in docs/research/boot_path.md step 21.
  //     A callee that is not written is a `PORT_FRAME_STOP` on the line where retail calls it:
  //     the run stops there with a stack, so the goal loop's boot scan queues it by itself, and
  //     replacing the stop with the call is the whole of the item. Nothing is skipped: the run
  //     cannot get past a stop.
  //
  //     Two things are the port's, and both are bounded to the host:
  //       - `frame: N` is printed at the top of every frame. It is how boot-progress.sh tells a
  //         run that got further (more frames) from one that did not.
  //       - `MP_PORT_FRAMES=N` ends the loop after N frames and returns without the teardown
  //         (retail's steps 22-24, which are missing or empty). Unset, the loop runs until
  //         `finished`, which is retail's only exit; the window's close event is Aurora's and
  //         cannot be read here (see the comment above this function).
  printf("%s", "boot: step 21 - the frame loop\n");
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  const char* const budgetText = getenv("MP_PORT_FRAMES");
  const long frameBudget = budgetText != nullptr ? strtol(budgetText, nullptr, 10) : 0;
  long frame = 0;
  CGameArchitectureSupport* arch = mArchSupport;
  // f31 at 0x80006028: lbl_8041A3D0, the double 1/60 every frame time is divided by.
  const double kFrameSeconds = 0.01666666753590107;
  while (!mFinished) {
    ++frame;
    if (frameBudget > 0 && frame > frameBudget) {
      printf("frame loop: MP_PORT_FRAMES=%ld frames ran - returning without the teardown\n",
             frameBudget);
      fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
      return 0;
    }
    printf("frame: %ld\n", frame);
    fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy

    arch->GetStopwatch2().Reset();                                       // 0x80006034-0x80006068
    gpResourceFactory->GetResLoader().AsyncIdlePakLoading();             // 0x80006074
    gpRelFileManager->Update();                                           // 0x8000607C
    if (gpMemoryCard == nullptr && gpResourceFactory->GetResLoader().AreAllPaksLoaded()) {
      MemoryCardInitializePump();                                        // 0x800060A4
    }
    fn_8030172C();                                                               // 0x800060A8
    CARAMToken::UpdateAllDMAs();                                         // 0x800060AC
    if (!arch->UpdateTicks()) {                                          // 0x800060B4
      mFinished = true;                                                   // 0x800060C8, mask 0x80 of +0x90
    }
    const float updateSeconds = arch->GetStopwatch2().GetElapsedTime();  // f30, 0x800060F8
    const float updateFrames = static_cast< float >(updateSeconds / kFrameSeconds);
    fn_800069AC(&mTickTimes, &updateFrames);                   // 0x80006108
    // Zero-initialised, which retail does not do: on `count == 0` `fn_80006954` returns
    // without writing +0, so retail stores an uninitialised word to `CMain`+0x40. **That path
    // is unreachable from here** - `fn_800069AC` at 0x80006108 runs first and raises the count,
    // and `CMain` is placement-new'd into `mainTail.cpp`'s `static uchar sMainSpace[]`, so
    // `mTickTimes.count` starts at 0 and is at least 1 by this line - but reading an
    // uninitialised local is undefined behaviour that a host compiler may act on, so it is
    // written down rather than leaned on.
    SFrameTimeTotal updateTotal = { 0.0f, 0 };                     // 0x8000610C, r1+32
    fn_80006954(&updateTotal, &mTickTimes);                      // 0x80006114
    // 0x80006118-0x80006120. Retail reads only the +0 float back out of the local and stores
    // it; the +4 flag the call wrote is never read here. The member is still named `...Total`
    // because tools/sizeprobe_cmain.cpp and tools/read_cmain_layout.sh name it that, and the
    // value is a mean - see src/MetroidPrime/PortFrameTimeHistory.c.
    mAverageTickTime = updateTotal.x0_value;                            // 0x80006120
    arch->GetStopwatch2().Reset();                                       // 0x80006124-0x80006154

    bool draw = true;
    if (GetMaxSpeed()) {                                                 // 0x80006168
      AsyncIdle(1000000);                                                // 0x80006180
      if (mMaxSpeedDrawTimer <= 0.0f) {                                                 // 0x8000618C
        mMaxSpeedDrawTimer = 1.0f;
      } else {
        draw = false;
        CFrameDelayedKiller::FlushAllocationsForFrame();                 // 0x800061A8
        CFrameDelayedKiller::FlushAllocationsForFrame();                 // 0x800061AC
      }
    }

    if (draw) {
      gpRender->BeginScene();                                            // 0x800061C8, vtable +0x94
      arch->GetIOWinManager().Draw();                                    // 0x800061D4
      DrawDebugMetrics(updateSeconds, arch->GetStopwatch2());            // 0x800061E8
      const float drawFrames =
          static_cast< float >(arch->GetStopwatch2().GetElapsedTime() / kFrameSeconds);
      fn_800069AC(&mDrawTimes, &drawFrames);                   // 0x80006228
      SFrameTimeTotal drawTotal = { 0.0f, 0 };                       // 0x8000622C, r1+24
      fn_80006954(&drawTotal, &mDrawTimes);                        // 0x80006234
      mAverageDrawTime = drawTotal.x0_value;                              // 0x8000623C
      gpRelFileManager->Update();                                         // 0x80006244
      const double spare = kFrameSeconds -
                           (updateSeconds + arch->GetStopwatch2().GetElapsedTime()) - 0.00075;
      AsyncIdle(spare > 0.0 ? static_cast< uint >(1000000.0 * spare) : 0);  // 0x800062A4
      if (gpMain->GetThirtyFps()) {                                 // 0x800062B0, mask 0x80 of +0x91
        const float wait = 0.033333335f -
            static_cast< float >(updateSeconds + arch->GetStopwatch2().GetElapsedTime());
        if (wait > 0.0f) {
          CStopwatch::Wait(wait);                                        // 0x800062F8
        }
      }
      gpRender->EndScene();                                              // 0x8000630C, vtable +0x98
      if (mGameFrameDrawn) {                                             // 0x80006314, mask 0x01 of +0x90
        ++arch->GetFramesDrawn();                                        // +0x64
        mGameFrameDrawn = false;
      }
    } else {
      gpResourceFactory->AsyncIdle(1000000, false);                      // 0x80006350
    }

    arch->Update();                                                      // 0x80006358
    CSfxManager::Update(0.016666668f);                                   // 0x80006360
    fn_80003858(0.016666668f);                                           // 0x80006368
    if (CheckTerminate()) {                                              // 0x80006370
      PORT_FRAME_STOP("fn_800068F4(gpGameState + 0x1F4), then leave the loop",
                      "0x800068F4, 0x60");                                     // 0x80006384
    }
    // 0x8000638C-0x800063D4: an IOWin manager with nothing in it is a reset, and so is
    // `CheckReset`, which is not asked when the manager is empty.
    if (arch->GetIOWinManager().IsEmpty() || CheckReset()) {
      mRestartMode = kRM_Default;                                     // 0x800063E4, 6
      PORT_FRAME_STOP("the reset path: fn_803215C8, PADRecalibrate(0xF0000000), fn_802BE8E8(1), "
                      "fn_802C1E60, fn_802C1658, StallAndFlushAllAllocations, then a new CGameArchitectureSupport "
                      "through fn_80008A48",
                      "0x800063E8-0x80006454");
    }
  }
  // Retail's teardown, 0x80006464 onward, is steps 22-24 of docs/research/boot_path.md.
  printf("frame loop: `finished` was set after %ld frames - returning without the teardown\n", frame);
  fflush(nullptr);   // not `stdout`: a data symbol is a copy relocation `--allow-shlib-undefined` cannot satisfy
  return 0;
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
//
// Measured 2026-09-27, because a previous note here blamed Aurora for a SIGSEGV 36 bytes
// into this function and that was wrong. Aurora is not the problem and never was:
//
//   - `ARInit` **never dereferences the array it is handed.** extern/aurora/lib/dolphin/AR.cpp:97-121
//     returns early if `aurora::g_config.mem2Size == 0`, returns early again if AR is already
//     up, `calloc`s the buffer, and then only *stores* `stack_index_addr` into
//     `AR_BlockLength` and `sAllocationStackBase`. The only requirement it has is a non-zero
//     `mem2Size`, and platform/main.cpp:110 sets that to `ARAM_DEFAULT_SIZE` (16 MB) in
//     `AuroraConfig`. So neither "the length array's contract" nor "ARAM needs enabling
//     first" is a live question: both are already satisfied, and `sAramLengthStack` is a real
//     three-entry array, not a guest address.
//   - The fault was `lbl_80418BA8 += ARAlloc(0)`. `lbl_80418BA8` is a **four-byte data
//     object** (config/G2ME01/symbols.txt:20183, `type:object size:0x8 data:4byte`) and the
//     only definition of it in this tree is src/Kyoto/CARAMManagerPort.cpp, which was not in
//     files.cmake. `tools/boot_probe.sh`'s self-heal then emitted
//     `extern "C" void lbl_80418BA8(void) { printf(...); }` - a **function** stub for a data
//     symbol, because its `decl_ok()` only checks that the name is a valid C identifier. The
//     linker resolved the data reference to that function's address in `.text`, and
//     `lbl_80418BA8 += 0x4000` became `add %eax,(%rbx)` on a `PT_LOAD` mapped `R E` - a write
//     to a read-only page. objdump of the probe binary: `PortInitializeSubsystems+0x24` is
//     that `add`, and `nm` reports `lbl_80418BA8` as `T`, not `D`.
//
// So the fix is not in this function at all: `src/Kyoto/CARAMManagerPort.cpp` has to be in
// files.cmake, which also gives the port a real `fn_80301CC4` and therefore real ARAM pools.
// With that listed and the stale stub removed, step 11 completes and the boot reaches step 12.
void PortInitializeSubsystems() {
  // `lbl_80418BA8` (.sdata 0x80418BA8: 00004000) is Aurora's `ARAM_STACK_START` too. It is
  // retail's own global and not a local here any more, because `fn_80301CC4` below reads it.
  static uint sAramLengthStack[3];

  // Aurora does not export its free-block count, so the only way to report how much of the
  // stack is in use is to mark the slots and see which have been written. Without this the
  // diagnostic below was a hard-coded `0`: the one slot Aurora writes before it is
  // `ARAlloc(0)`, and a zero-length allocation stores the value **0**, so "slot is non-zero"
  // was false for every slot Aurora had ever touched. `ARInit` only stores the pointer and
  // `ARAlloc` only writes, so pre-marking the array is safe - nothing reads it first.
  for (uint& slot : sAramLengthStack) {
    slot = kAramSlotUnused;
  }

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
  printf("ARAM stack pointer 0x%8.8x, %u of 3 length slots used\n", (unsigned)lbl_80418BA8,
         (unsigned)CountUsedAramSlots(sAramLengthStack, 3));

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
//     `fn_80218760`, `fn_801F0280`, `fn_801F0308`, `fn_801F0518`, `fn_800DC03C`),
//     so on a PC build the retail body is a link error before it is a run-time hazard.
//     (the REL load has a host body now, upstream's src/MetroidPrime/CRelFile.cpp, but nothing
//     that creates a module record does.)
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
