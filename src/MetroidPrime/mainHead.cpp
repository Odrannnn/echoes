// Port-only: the lower third of the pre-merge main.cpp (globals, AsyncIdle, the world-pak helpers,
// fn_8033CDA0). Upstream main.cpp is a skeleton that overlaps mainMid.cpp/mainTail.cpp and is
// left out of files.cmake until its bodies are folded into it; see docs/HANDOFF.md.

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Audio/CStreamAudioManager.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CFactoryFunctions.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "dolphin/ar.h"
#include "dolphin/arq.h"
#include "dolphin/gx/GXStruct.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSThread.h"

#include "MetaRender/IRenderer.hpp"
#include "MetaRender/CCubeRenderer.hpp"

#include "MetroidPrime/CAudioStateWin.hpp"
#include "MetroidPrime/CConsoleOutputWindow.hpp"
#include "MetroidPrime/CErrorOutputWindow.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CArchitectureMessageParm.hpp"
#include "MetroidPrime/CMainFlow.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/CWorldTransManagerView.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include <stdio.h>

class CCharacterFactoryBuilder;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

extern "C" void fn_8029EFCC();
extern "C" void fn_8033CEE8();
// `fn_8033CDA0`, 0x8033CDA0, 0x148 = 328 bytes - the same size as `fn_8033CEE8` (0x8033CEE8,
// 0x148) and called from the mirrored place: the constructor calls `fn_8033CEE8` after
// `fn_8029EFCC` and the destructor calls `fn_8033CDA0` after `UnloadAudio`. A same-size pair
// called from mirrored sites is what an Initialize/Shutdown pair looks like, and retail's symbol
// table names neither, so the pair is the identification. It is the `CDSPStreamManager::Shutdown`
// the destructor already had written as a comment (`// CDSPStreamManager::Shutdown();`) and the
// last instruction this destructor was missing.
extern "C" void fn_8033CDA0();
IRenderer* AllocateRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys, IFactory& resFactory);

// Retail globals that the decompilation only *declares* - `extern "C" T lbl_...;` plus a use -
// and never defines. In the DOL each one is defined by whichever retail object owns it and the
// linker resolves it against that object; a PC link has no retail object, so every one of them
// needs a real definition or the game cannot link. `tools/link_gap.py` measures the residue.
//
// They are here because this unit is `NonMatching` in configure.py, so nothing in this file can
// move the matching build, and because main.cpp already holds retail's loose game globals
// (gpSimplePool and friends). `config/G2ME01/symbols.txt` says which section and address each
// symbol has; `build/G2ME01/main.elf` says what is at that address; and the width is the one the
// retail instruction implies (`lhz`/`lwz`/`lfs`/`stb`/`stw`), not the one dtk's gap-based `size:`
// field suggests. A symbol in .bss or .sbss has no contents in the ELF at all, so its value at load
// is 0 and these definitions are the zero fill.
//
// Every one of them carries an explicit initializer even where the value is 0, because GCC drops
// an *uninitialised* tentative definition that nothing in the translation unit reads - which
// would leave the symbol undefined in the link this file exists to fix. The `extern` on each
// `const` member is not redundant: inside a linkage-specification block GCC gives a `const`
// declaration internal linkage without it, and an unreferenced internal object is dropped too.
extern "C" {
// .bss 0x803DFA8C, 0xDC bytes = 110 entries of the two-byte retail GXVtxDescList. The count is
// written out rather than computed from sizeof because the port's GXVtxDescList is eight bytes
// wide (aurora models GXAttr/GXAttrType as u32), and retail's byte count is the number that
// matters: CGX's `la` into this array then writes 20 two-byte entries before GXSetVtxDescv reads
// them back, so the zero fill is never observed.
GXVtxDescList lbl_803DFA8C[110] = {};

// .sdata 0x80418D00: 7f7fffff 00000000. CAABox.cpp reads *(float*)lbl_80418D00 as its kFltMax,
// and 0x7F7FFFFF is FLT_MAX exactly, so the declared type and the retail bytes agree. The second
// word belongs to the same object (the next symbol is 8 bytes on) and is zero.
int lbl_80418D00[2] = { 0x7F7FFFFF, 0 };

// .sbss, so zero at load. Compared with `lwz` in fn_80036284 / fn_800362E0.
int lbl_80418FB8 = 0;
int lbl_80418FBC = 0;
// .sbss. `stb` in CGameOptions::fn_80161C7C, so a byte - C++ `bool` is one.
bool lbl_804191E0 = false;
// .sbss. All three are `stb` in CStateManager's fn_8003AD74.
uchar lbl_80419730 = 0;
uchar lbl_80419745 = 0;
// .sbss. `lbz` in CCubeMoviePlayer's SelectMoviePath: false, so the "_pal" film is never tried.
bool lbl_804199CC = false;
// .sbss. `stw` in CStateManager::fn_8003FF74, which writes the same value to both.
int lbl_80419A10 = 0;
int lbl_80419A18 = 0;
// .sbss. `stb` in CStateManager's fn_8003AD74, alongside lbl_80419730/lbl_80419745.
uchar lbl_80419A98 = 0;
// .sbss 0x80418EC4, 4 bytes, and this one has a **writer and a clearer, both retail's**: the
// constructor of `CGameArchitectureSupport` (0x80007EC4) does `addi r30,r31,68 ; stw r30,lbl_80418EC4`
// at 0x80007F80, i.e. it publishes `&this->ioWinMgr` (0x80007EC4+0x44) into this global, and
// `~CGameArchitectureSupport` (0x80007DE8) does `li r0,0 ; stw r0,lbl_80418EC4` at 0x80007E28,
// immediately after `RemoveAllIOWins` and before `UnloadAudio`. It is the one retail global in
// this area that is *not* merely declared-and-never-defined, and it is zero at load, so `= 0` is
// the right initialiser. It is not in `docs/research/port_link_gap_list.md`, so defining it here
// adds nothing to the port's link gap - and it is one word past the end of `mainTail.cpp`'s
// `.sbss` claim (0x80418EA0-0x80418EC4), so dtk still supplies retail's own bytes in the DOL.
CIOWinManager* lbl_80418EC4 = 0;
// .sbss 0x80419300, 4 bytes, written **once in the whole DOL**, by the constructor of
// `CGameArchitectureSupport` at 0x80007FD4: `lwz r0,52(r31) ; stw r0,0(lbl_80419300)`. 0x34 is
// `CGameArchitectureSupport`+0x30+0x04, and `CGameArchitectureSupport`+0x30 is its
// `CInputGenerator` member - whose `+0x04` is `x4_controller`, a `single_ptr<IController>` whose
// first word is the pointer (`rstl/single_ptr.hpp`). So the value is
// `inputGenerator.GetController()`, which is a *named public accessor* on that class and not a raw
// offset, which is what `tools/check_raw_offsets.py` requires.
IController* lbl_80419300 = 0;
// .sbss. Render flags, `stw` in CStateManager::fn_80036650.
uint lbl_80419A9C = 0;
uint lbl_80419AA0 = 0;

// .rodata 0x803A56C0, 0x1C0 bytes - **retail's string pool**, and the reason four functions in
// this file read 99.94-99.98% against their own retail objects while being byte-identical in
// the linked DOL. Retail reaches its strings as `lbl_803A56C0 + <offset>` - `lis r4,0` /
// `R_PPC_ADDR16_HA lbl_803A56C0` / `addi r3,r4,0` / `R_PPC_ADDR16_LO lbl_803A56C0` /
// `addi r3,r3,124` - whereas a literal in this unit goes through *MWCC's* pool as
// `@stringBase0 + 16`. Same four instructions, same linked address (the linker overwrites the
// addend, which is why `tools/gate.sh`'s per-function diff has always reported these four as
// unchanged), and objdiff, which compares the two unlinked objects, counts the differing
// addend. Measured, in this order:
//   CGameGlobalObjects::PostInitialize  retail +0x150 (336)  was @stringBase0+176
//   CGameGlobalObjects::LoadStringTable  retail +0x146 (326)  was @stringBase0+166
//   InfiniteLoopAlarm                   retail +0x133 (307)  was @stringBase0+152
//   CMain::FillInAssetIDs               retail +0x07C (124)  was @stringBase0+16
// Declared, not defined: the four strings are retail's .rodata, which a PC build cannot have,
// and the offsets are retail's addresses rather than anything the port could use. That is one
// new undefined symbol, written into docs/research/port_link_gap_list.md as a cost.
// `MetroidPrime/mainTail.cpp`'s `CMain::InitializeSubsystems` needs the same pool for its two
// printf formats (+0x187 and +0x19D) and declares it there.
extern const char lbl_803A56C0[];

// .sdata2 0x8041A8BC: 00000000. `lfs` in CActor::GetYaw (the value it returns when the transform is
// facing away) and again in ProcessSoundEvent, so one float and one value.
extern const float lbl_8041A8BC = 0.0f;

// .sdata2 0x8041A420: 41200000, i.e. **10.0f**, and `InfiniteLoopAlarm` below is its only reader
// in this file. Retail loads it as a relocation against this symbol (`lfs f0,0(0)` /
// `R_PPC_EMB_SDA21 lbl_8041A420`); the tree's `10.f` literal made mwcceppc put the constant in
// *its own* pool (`@1184`), which is the same value and a different relocation. **Defined**
// rather than declared, because a value is something the port can have - that is what keeps
// this one out of the port's link gap.
extern const float lbl_8041A420 = 10.0f;
// .sdata2 0x8041A8D0: 3a83126f, which is 0.001f exactly. `lfs` in CActor::GetYaw, the threshold
// fn_8001D658(m11*m11 + m01*m01) is compared against.
extern const float lbl_8041A8D0 = 0.001f;

// .sdata2 0x8041D248: 00c6 00c3 25b5 259b. CPowerBeam::Fire computes a `li`'d base plus
// (fn_80036F10() ? 8 : 0) plus chargeStage*2 and does one `lhzx`, so it is four halfwords - the
// power beam's per-charge-stage sound ids, single player then multiplayer.
extern const ushort lbl_8041D248[2][2] = { { 0xC600, 0xC300 }, { 0xB525, 0x9B25 } };

// .sdata2 0x8041D394 / 0x8041D398: 803aadf2 / 803aadfc, `lwz` in CPowerBeam::Unk9. Those addresses
// are the .rodata strings "ShotSmoke" and "Power2nd_1", which is what the pool lookup takes - so
// the value that matters is the string, not the retail address, and a 64-bit host cannot hold the
// guest address anyway. The strings sit in named buffers that the pointers refer to, rather than
// the pointers being initialised from literals directly: a string *literal* added to this unit
// makes mwcceppc re-optimise an unrelated function (CGameArchitectureSupport's constructor grows
// 32 bytes and picks up a __cvt_dbl_usll call) and the gate reports that as two functions going
// WORSE. A named buffer perturbs nothing and leaves this unit's .text byte-identical. Defining
// these two in CPowerBeam.cpp instead, which reads better, costs two 100% functions in that unit.
//
// **Refined 2026-09-26, measured, and it is narrower than the note above says.** Writing
// `CGameGlobalObjects::AddPaksAndFactories` added **13 string literals to this unit** - the eleven
// pak names and the two printf formats of `CMain::InitializeSubsystems` - and `.rodata` grew
// 0x6C -> 0x11A. Of the 81 functions in `main.o`, **75 instruction streams are byte-identical**
// to the build of `4d49561`; three are the ones this change was for; and the other three
// (`InfiniteLoopAlarm`, `LoadStringTable`, `PostInitialize`) each differ in **exactly one
// instruction**, the `addi` that is the low half of an `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair
// against `@stringBase0` - a relocation the linker overwrites, so the linked address does not
// move. `tools/gate.sh`'s per-function diff reports those three unchanged, and the DOL sha1 is
// unchanged. **So a string literal in this unit costs an `addi` addend, not a function**; what
// actually cost two 100% functions in the case above was a string literal changing what an
// *unrelated* function *computes* (a `__cvt_dbl_usll` call appearing), which is a different
// failure and does not follow from the literal alone. Verify with the per-function diff either
// way - it is two seconds - and do not pre-emptively convert a literal to a named buffer.
static const char kShotSmoke[] = "ShotSmoke";
static const char kPower2nd1[] = "Power2nd_1";
extern const char* const lbl_8041D394 = kShotSmoke;
extern const char* const lbl_8041D398 = kPower2nd1;

// .sdata2 0x8041E2E6: ffff. `lhz` + `cmplw` in CPowerBeam::Fire against the caller's sfx id, so
// 0xFFFF is the "caller supplied the id" sentinel.
extern const ushort lbl_8041E2E6 = 0xFFFF;
}

CResFactory* gpResourceFactory;
CSimplePool* gpSimplePool;
CCharacterFactoryBuilder* gpCharacterFactoryBuilder;
CStringTable* gpStringTable;
CMain* gpMain;
IController* gpController;
CGameState* gpGameState;
CMemoryCard* gpMemoryCard;
CInGameTweakManager* gpTweakManager;
float sInfiniteLoopTime;

// `sMainSpace`, `__sys_free` (0x80008A28), `CMain::CMain` (0x80008898), `InvokeCMain`
// (0x80008818) and `CMain::~CMain` (0x800087DC) are **not here any more**: all five are at or
// above 0x80008570 and are claimed by `MetroidPrime/mainTail.cpp`, which is where they went.


// `CMain::SetGameFrameDrawn` (0x800089AC), `CMain::fn_80008A1C` (0x80008A1C) and
// `CMain::SetMaxSpeed` (0x800089BC) are **not here any more**: all three are at or above
// 0x80008570 and are claimed by `MetroidPrime/mainTail.cpp`, which is where they went.

// `CMain::InitializeSubsystems` (0x80008680) and `CMain::ShutdownSubsystems` (0x80008570)
// are **not here any more**: both are at or above 0x80008570 and are claimed by
// `MetroidPrime/mainTail.cpp`, which is where they went. The two `printf` formats, the
// stack-guard constant and the `#ifdef TARGET_PC` split between `CMain::InitializeSubsystems`
// and `PortInitializeSubsystems` went with them - see that file's header for why the cut cannot
// be anywhere else.

// `CGameGlobalObjects::CGameGlobalObjects` is **not here any more**: retail's is
// 0x8000848C-0x80008570, inside this unit's old range, and it is the only writer of `gpGameState`
// in the DOL, so it is a unit of its own - `MetroidPrime/CGameGlobalObjectsCtor.cpp`, `Matching`.
// The range is cut three ways (this unit, that one, `MetroidPrime/mainTail.cpp`), because a unit
// may not claim two ranges in one section; `mainTail.cpp`'s header has the details.

// `CGameGlobalObjects::PostInitialize` (0x800083E0), `LoadStringTable` (0x800082BC),
// `InfiniteLoopAlarm` (0x8000823C), `CGameArchitectureSupport`'s constructor (0x80007EC4),
// destructor (0x80007DE8), `UpdateTicks` (0x80007BC0) and `Update` (0x80007A14), the three
// `MakeMsg::` factories (0x80007AA0-0x80007B38), `CArchitectureQueue::Push` (0x80007A80),
// `CMain::MemoryCardInitializePump` (0x80007958), `CGameGlobalObjects::AddPaksAndFactories`
// (0x80007168), `CMain::DrawDebugMetrics` (0x800070FC), `CMain::CheckTerminate` (0x800070F4),
// `fn_800070A4` (0x800070A4), `fn_80007040` (0x80007040) and `CMain::CheckReset` (0x80006BA4)
// are **not here any more**: all sixteen are at or above 0x80006B80 and are claimed by
// `MetroidPrime/mainMid.cpp`, which is where they went. That file's header has the split's
// mechanics, and the reason the cut cannot be anywhere else.
//
// `CMain::FillInAssetIDs` (0x80006B38, 0x48 = 72 bytes) is **also not here**: it is
// `MetroidPrime/CMainFillInAssetIDs.cpp`, a `Matching` unit. The body is the same one line and
// the same 100.00%; what changed is that it no longer shares a claim with 49 other functions.
// **It is `NonMatching` in this file today and cannot be promoted here** - `dtk` refuses an
// interior carve outright (`Split 3:0x80006B38..3:0x80006B80 overlaps with previous split`), so
// making it a unit required this source split, and the two are one change.

// Retail 0x80005C6C, 0x864 bytes, and the body is unwritten. What that body needs before it
// can be written is measured in docs/research/boot_path.md; the two facts that decide the
// port's shape are there, and both are negative:
//
//   - retail Echoes has no `CMain::OpenWindow`. `config/G2ME01/symbols.txt` names 19 `CMain`
//     methods and OpenWindow is not one of them, the string does not occur anywhere in the
//     DOL's disassembly, and this function - fully disassembled - makes no call on
//     `x0_osContext` at all. The window/VI bring-up lives in the *caller* of `InvokeCMain`,
//     `main` at 0x801EFB00, through its sixth argument - which is the `CGraphicsSys` this
//     port now constructs for real in `platform/main.cpp` (2026-09-29), so the host
//     stand-in that used to sit in `CMain::RsMain` is gone.
//   - the frame loop is unreachable, not merely unwritten: it needs a constructed
//     `CGameArchitectureSupport`, whose constructor dereferences `gpTweakPlayerA` at
//     0x80007F38 with no null test, and `gpGameState` at 0x800081A4.
//
// So the host body lives in src/MetroidPrime/PortBoot.cpp behind `#ifdef TARGET_PC`, in a
// translation unit `configure.py` never claims - which is also why this guard costs the
// matching build nothing: mwcceppc does not define TARGET_PC, so it compiles exactly the
// empty body it compiled before. See docs/research/boot_path.md for the full ordered list.
#ifndef TARGET_PC
// `return 0;` is not retail's - retail's is 2,148 bytes and returns a real code - but the empty
// body without one is undefined behaviour, and it was the only "return value expected" warning
// this unit compiled with. `InvokeCMain` (mainTail.cpp) discards the value, so the answer is
// never read; the line exists to make that true rather than accidental.
int CMain::RsMain(int argc, const char* const* argv) { return 0; }
#endif // TARGET_PC

void CMain::AsyncIdle(uint time) {
  if (time < 500) {
    uint total = 0;
    for (int i = 0; i < frameTimes.capacity(); ++i) {
      total += frameTimes[i];
    }
    if (total < 500 * frameTimes.capacity()) {
      time = 500;
    } else {
      time = 0;
    }
  }
  frameTimes[frameTimeIdx] = time;
  frameTimeIdx = frameTimeIdx + 1;
  if (frameTimeIdx >= frameTimes.capacity()) {
    frameTimeIdx = 0;
  }

  time = (time <= 5000) ? time : 5000;
  if (time < frameTimeMinimum) {
    time = frameTimeMinimum;
  }
  frameTimeMinimum = 0;
  bool flag = fn_80008A1C();
  if (flag) {
    time = 1000000;
  }

  if (time != 0) {
    gpResourceFactory->AsyncIdle(time, flag);
  }
}

void CMain::AddWorldPaks() {
  rstl::string basePath = gpTweakGame->GetPakFile();
  for (int i = 0; i < 16; ++i) {
    rstl::string pak =
        basePath + (i == 0 ? rstl::string_l("") : rstl::string(CBasics::Stringize("%d", i)));
    if (CDvdFile::FileExists((pak + rstl::string_l(".pak")).data())) {
      gpResourceFactory->GetResLoader().AddPakFileAsync(pak, false, true);
    }
  }
}

void CMain::EnsureWorldPaksReady() {
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  for (int i = 0; i < resLoader.GetPakCount(); ++i) {
    CPakFile& file = *resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      file.EnsureWorldPakReady();
    }
  }
}

// Retail 0x800053B8 is `CMain::StreamNewGameState(bool)` (`StreamNewGameState__5CMainFb`); the
// port's body is the `extern "C"` one in `src/MetroidPrime/PortStreamNewGameState.cpp`, called
// from `CMainFlowDtor.cpp`. The pre-merge `(CInputStream&, int)` overload that stood here was
// deleted in the upstream merge (2026-09-28): upstream's `CMain` does not declare it and nothing
// in `files.cmake` called it.

// `CPlayerState::~CPlayerState` (0x8000939C), `CPlayerState::SPersistentState::
// ~SPersistentState` (0x80009508) and `CStaticInterference::~CStaticInterference` (0x80009460)
// are **not here any more**: all three are at or above 0x80008570 and are claimed by
// `MetroidPrime/mainTail.cpp`, which is where they went. See that file's header for why the
// cut cannot be anywhere else.
