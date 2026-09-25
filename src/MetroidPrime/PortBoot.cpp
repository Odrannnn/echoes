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

#include "MetroidPrime/CMain.hpp"

#include "dolphin/ar.h"
#include "dolphin/arq.h"

#include <stdio.h>

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
//   7. `new CGameGlobalObjects(os, mem)`  - src/MetroidPrime/main.cpp:198, a stub: it
//                                            initialises `simplePool` from an uninitialised
//                                            `resFactory`.
//   8. `fn_80003A18(this)`                - not in this tree.
//  11. `InitializeSubsystems()`           - ARInit plus a TODO.
//  12. `CGameGlobalObjects::PostInitialize`- calls an unwritten `AllocateRenderer`.
//  13. `CGameGlobalObjects::AddPaksAndFactories` - empty, and 1,936 bytes in retail.
//  14/16. `AddWorldPaks()` / `FillInAssetIDs()` - written; need step 13's factory.
//  17. `new CGameArchitectureSupport(os)` - written, and it faults on the host.
//  21. the frame loop                     - written, and unreachable without step 17.
//
// Step 17 is the wall, and it is a wall of null dereferences rather than of missing code.
// `CGameArchitectureSupport::CGameArchitectureSupport` (0x80007EC4) does
// `lwz r29,-28220(r13)` at 0x80007F38 - `gpTweakPlayerA`, 0x80418F44, which
// src/MetroidPrime/PortGlobals.cpp:129 defines as `nullptr` and which retail only ever fills
// from the Tweaks REL module - and calls `GetRightAnalogMax`/`GetLeftAnalogMax` on it at
// 0x80007F40 and 0x80007F4C with no null test. Further down it does
// `lwz r3,-28360(r13)` at 0x800081A4 - `gpGameState`, 0x80418EB8 - and calls
// `CGameOptions::EnsureOptions` on it at 0x800081AC; `gpGameState` is null too, until
// `CMain::StreamNewGameState` runs, which needs the paks from step 13. Constructing one on
// the host would fault on the first of those.
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
void PortInitializeSubsystems() {
  // Aurora's `ARAM_STACK_START`, which is also the initial value of retail's own ARAM bump
  // pointer (`lbl_80418BA8`, .sdata 0x80418BA8: 00004000).
  static uint sAramLengthStack[3];
  static uint sAramStackPointer = 0x4000;

  ARInit(sAramLengthStack, 3);
  // `ARAlloc`'s argument is zero. Retail passes the guest word at 0x80418EA0, which
  // `fn_80009864` fills in with `*(u32*)0x80415980 * 14` and which is zero in the DOL as
  // built (.sbss), and `ARAlloc(0)` is legal on both the hardware bump allocator and
  // Aurora's - Aurora asserts only that the length is 32-byte aligned and that the request
  // fits inside ARAM.
  sAramStackPointer += ARAlloc(0);
  ARQInit();

  printf("%s", "Initializing subsystems");
  printf("Stack: 0x%8.8x down to 0x%8.8x\n", (unsigned)sAramStackPointer, (unsigned)sAramStackPointer);
  printf("ARAM stack pointer 0x%8.8x, %u of 3 length slots used\n", sAramStackPointer,
         (unsigned)((sAramLengthStack[0] != 0) ? 1u : 0u));
}

#endif // TARGET_PC
