#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Audio/CStreamAudioManager.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "dolphin/ar.h"
#include "dolphin/gx/GXStruct.h"

#include "MetaRender/IRenderer.hpp"
#include "MetaRender/CCubeRenderer.hpp"

#include "MetroidPrime/CAudioStateWin.hpp"
#include "MetroidPrime/CConsoleOutputWindow.hpp"
#include "MetroidPrime/CErrorOutputWindow.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CMainFlow.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/CWorldState.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include <stdio.h>

class CCharacterFactoryBuilder;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

extern "C" void fn_8029EFCC();
extern "C" void fn_8033CEE8();
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
// .sbss. Render flags, `stw` in CStateManager::fn_80036650.
uint lbl_80419A9C = 0;
uint lbl_80419AA0 = 0;

// .sdata2 0x8041A8BC: 00000000. `lfs` in CActor::GetYaw (the value it returns when the transform is
// facing away) and again in ProcessSoundEvent, so one float and one value.
extern const float lbl_8041A8BC = 0.0f;
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
unkptr gpController;
CGameState* gpGameState;
CMemoryCard* gpMemoryCard;
CInGameTweakManager* gpTweakManager;
float sInfiniteLoopTime;

static uchar sMainSpace[sizeof(CMain)];

extern "C" void __sys_free(const void* ptr) { CMemory::Free(ptr); }

CMain::CMain(COsContext* context, void* unk1, CMemorySys* memorySys, void* unk2)
: osContext(context)
, x4_unk1(unk1)
, memorySys(memorySys)
, xc_unk2(unk2)
// , xe8_(0.0)
// , x118_(0.f)
// , x11c_(0.f)
// , x120_(0.f)
// , x124_(0.f)
, frameTimeMinimum(0)
, x4c(0.0f)
, gameGlobalObjects(nullptr)
, restartMode(kRM_StateSetter)  // value must be 6, TODO if the correct enum
, x5c(1.0f)
, frameTimes(0xF4240)
, frameTimeIdx(0)
, finished(false)
, mfGameBuilt(false)
, screenFading(false)
, x90_27_(false)
, x90_28_manageCard(false)
, x90_29_(false)
, x90_30_(false)
, x90_31_cardBusy(false)
{
  gpMain = this;
}

extern "C" void InvokeCMain(int argc, char** argv, COsContext* context, void* unk1,
                            CMemorySys* memorySys, void* unk2) {
  CMain* main = new (&sMainSpace) CMain(context, unk1, memorySys, unk2);
  main->RsMain(argc, argv);
  main->~CMain();
}

CMain::~CMain() {}

void CMain::SetFrameTimeMinimum(int time) { frameTimeMinimum = time; }

void CMain::SetGameFrameDrawn(bool drawn) { x91_24_gameFrameDrawn = drawn; }

bool CMain::fn_80008A1C() { return screenFading; }

void CMain::SetMaxSpeed(bool v) {
  if (v && !screenFading) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  x5c = 0.0f;
  screenFading = v;
}

void CMain::InitializeSubsystems() {
  ARInit((u32*) 0x803c5ab8, 3);  // (u32*)(&sMainSpace + 0x98)
  // TODO
}

void CMain::ShutdownSubsystems() {}

CGameGlobalObjects::CGameGlobalObjects(COsContext& osContext, CMemorySys& memorySys)
: simplePool(resFactory) {}

void CGameGlobalObjects::PostInitialize(COsContext& osContext, CMemorySys& memorySys) {
  AddPaksAndFactories();
  LoadStringTable();
  printf("Initializing renderer...\n");
  renderer = AllocateRenderer(simplePool, osContext, memorySys, resFactory);                            
  gpRender = reinterpret_cast< CCubeRenderer* >(renderer.get());
  CEnvFxManager::Initialize();
}

void CGameGlobalObjects::LoadStringTable() {
  stringTable = gpSimplePool->GetObj("STRG_Main");
  gpStringTable = **stringTable;
}

void InfiniteLoopAlarm(OSAlarm* alarm, OSContext* context) {
  if (sInfiniteLoopTime >= 10.f) {
    OSCancelAlarm(alarm);
    rs_debugger_printf("INFINITE LOOP");
  }
  sInfiniteLoopTime += alarm->period / OS_TIMER_CLOCK;
}

CGameArchitectureSupport::CGameArchitectureSupport(COsContext& osContext)
: audioSys(0x30, 0x30, 0x30, 0x30, 0x5fc000)
, inputGenerator(&osContext, gpTweakPlayerA->GetLeftAnalogMax(),
                 gpTweakPlayerA->GetRightAnalogMax())
, gameFrameCount(0)
, x68_(0.f)
, x6c_(0.f)
, x70_(0.f)
// , x74_(2)
, infiniteLoopAlarmSet(false) {
  CAudioSys::SysSetVolume(0x7F, 0, 0xFF);
  CAudioSys::SetDefaultVolumeScale(0x75);
  CAudioSys::SetVolumeScale(CAudioSys::GetDefaultVolumeScale());
  // CDSPStreamManager::Initialize();
  fn_8029EFCC();
  fn_8033CEE8();
  CStreamAudioManager::SetMusicVolume(0x7F);
  CAudioSys::TrkSetSampleRate(kTSR_One);
  gpMain->SetMaxSpeed(false);
  gpMain->ResetGameState();
  ioWinMgr.AddIOWin(new CMainFlow(), 0, 0);
  ioWinMgr.AddIOWin(new CConsoleOutputWindow(8, 5.f, 0.75f), 100, 0);
  ioWinMgr.AddIOWin(new CAudioStateWin(), 100, -1);
  ioWinMgr.AddIOWin(new CErrorOutputWindow(false), 10000, 100000);
  gpGameState->GameOptions().EnsureOptions();
  sInfiniteLoopTime = 0.f;
  OSSetPeriodicAlarm(&infiniteLoopAlarm, OSGetTime(), (float)OS_TIMER_CLOCK, InfiniteLoopAlarm);
  infiniteLoopAlarmSet = true;
}

CGameArchitectureSupport::~CGameArchitectureSupport() {
  if (infiniteLoopAlarmSet) {
    OSCancelAlarm(&infiniteLoopAlarm);
    infiniteLoopAlarmSet = false;
  }
  ioWinMgr.RemoveAllIOWins();
  UnloadAudio();
  // CSfxManager::Shutdown();
  // CDSPStreamManager::Shutdown();
}

bool CGameArchitectureSupport::UpdateTicks() {
  bool result = false;
  const BOOL interrupts = OSDisableInterrupts();
  float stopwatchTime = stopwatch1.GetElapsedTime();
  stopwatch1.Reset();
  OSRestoreInterrupts(interrupts);
  sInfiniteLoopTime = 0.0f;
  x68_ += stopwatchTime;
  if (gpMain->GetFinished()) {
    x68_ = 0.033333335f;
  }
  bool flag = gpMain->fn_80008A1C();
  if (flag || 0.035 < stopwatchTime) {
    gpMain->Increment_x5c(-stopwatchTime);
    x68_ = 0.016666668f;
  }
  archQueue.Push(MakeMsg::CreateFrameBegin(kAMT_Game, gameFrameCount));

  bool keepLooping = true;
  while (keepLooping || x68_ > 0.016666668f) {
    keepLooping = false;
    if (!inputGenerator.Update(0.016666668f, archQueue)) {
      result = true;
    }
    archQueue.Push(MakeMsg::CreateTimerTick(kAMT_Game, 0.016666668f));
    x68_ -= 0.016666668f;
    ioWinMgr.PumpMessages(archQueue);
  }

  if (close_enough((x6c_ - x70_) + (x70_ - x68_), 0.0f)) {
    x68_ = 0.0f;
  }

  x6c_ = x70_;
  x70_ = x68_;
  ioWinMgr.PumpMessages(archQueue);
  return result;
}

void CWorldState::Update() {
  // Retail 0x8015B9B0, 0x374 bytes, reached from CGameArchitectureSupport::Update (0x80007A34).
  //
  // Retail's body is: release the object at +0x4A8 (fn_80230A20) if it is set, return if +0x04
  // (the per-world model-data object) is null, otherwise walk it - three blocks that turn a
  // pending asset request into a CModelData (+0x1E4/+0x1F0/+0x1FC/+0x208, gated on a byte at
  // +0x8 of each slot and on `+0x18` of the token at +0x00) and five blocks that unload a
  // CModelData whose reference count and flag are both clear (+0x1C, +0xB4, +0x100, +0x14C,
  // +0x198; the sixth slot at +0x68 is skipped).
  //
  // The release is not attempted either: fn_80230A20 is 840 bytes and unwritten, and dropping
  // the pointer instead would leak it every frame, which is worse than not pretending.
  //
  // Only the shape is reproduced. Every one of those eight blocks ends in a call this port does
  // not have - fn_800E6B68, fn_8007BBB8, fn_800E6900, fn_80029904, fn_8015AEE8, fn_800E5D20, plus
  // ~CModelData and ~CToken - and the blocks also need `SWorldModelData`, the per-world object,
  // which is not modelled. Writing them as calls would trade one unresolved symbol for nine;
  // declaring the eight as extern instead is strictly worse, growing the link gap rather than
  // shrinking it. So this is the guard, and nothing past it: writing the eight blocks as calls
  // to functions that do not exist would move the problem, not solve it.
  if (x4_modelData == nullptr) {
    return;
  }
}

void CGameArchitectureSupport::Update() {
  // Retail 0x80007A14, 0x70 bytes, and this is its body one-for-one.
  //
  // No null test on the world state, unlike CWorldState::Update's own guard: retail's
  // CGameState constructor always fills +0x3C, and adding a test here drops the function from
  // 100% to 84.78% against retail 0x80007A14. It happens to be unreachable today - nothing in
  // the port calls this yet, and the port's CGameState constructor is unwritten so +0x3C would
  // still be empty. Whoever wires up the caller has to fill CGameState's +0x3C first.
  gpGameState->GetWorldState()->Update();
  archQueue.Push(MakeMsg::CreateFrameEnd(kAMT_Game, gameFrameCount));
  ioWinMgr.PumpMessages(archQueue);
}

namespace MakeMsg {
namespace {
class CFrameMsgParm : public IArchitectureMessageParm {
public:
  explicit CFrameMsgParm(int frameCount) : x4_frameCount(frameCount) {}

private:
  int x4_frameCount;
};

class CTimerMsgParm : public IArchitectureMessageParm {
public:
  explicit CTimerMsgParm(float deltaTime) : x4_deltaTime(deltaTime) {}

private:
  float x4_deltaTime;
};
} // namespace

// The three factories are 0xCC bytes each and identical bar the type constant and the parm:
// `new(8)`, the parm's constructor, `new(4)` with `*refCount = 1`, the four stores into the
// returned message, then AddRef on the stack copy and ReleaseData on it. See the comment on
// CArchitectureMessage for why the fourth store is the rc_ptr's refcount and not a parameter.
CArchitectureMessage CreateFrameEnd(EArchMsgTarget target, const int& frameCount) {
  return CArchitectureMessage(target, kAM_FrameEnd,
                             rstl::rc_ptr< IArchitectureMessageParm >(new CFrameMsgParm(frameCount)));
}

CArchitectureMessage CreateFrameBegin(EArchMsgTarget target, int frameCount) {
  return CArchitectureMessage(target, kAM_FrameBegin,
                             rstl::rc_ptr< IArchitectureMessageParm >(new CFrameMsgParm(frameCount)));
}

CArchitectureMessage CreateTimerTick(EArchMsgTarget target, const float& deltaTime) {
  return CArchitectureMessage(target, kAM_TimerTick,
                             rstl::rc_ptr< IArchitectureMessageParm >(new CTimerMsgParm(deltaTime)));
}
} // namespace MakeMsg

void CArchitectureQueue::Push(const CArchitectureMessage& msg) { x0_queue.push_back(msg); }

// Retail 0x80142520, 8 bytes. `inline_max_size(0)` because retail's definition is in
// CGameState.cpp and its only caller therefore cannot inline it - see CGameState.hpp.
#pragma inline_max_size(0)
CWorldState*& CGameState::GetWorldState() { return x3c_worldState; }
#pragma inline_max_size(125)

void CMain::MemoryCardInitializePump() {}

void CGameGlobalObjects::AddPaksAndFactories() {}

void CMain::DrawDebugMetrics(double, CStopwatch&) {
  static uint counter = 0;
  ++counter;
  if (counter == 1800) {
    counter = 0;
  }
  CMemory::GetMetrics(counter == 0, false);
}

bool CMain::CheckTerminate() { return false; }

extern "C" void fn_800070A4() {}

extern "C" void fn_80007040() {}

bool CMain::CheckReset() {}

void CMain::FillInAssetIDs() {
  gpSimplePool->fn_8029c7e8(*gpResourceFactory->GetResourceIdByName("sound_lookup_ATBL"));
}

// Retail 0x80005C6C, 0x864 bytes, and the body is unwritten. What that body needs before it
// can be written is measured in docs/research/boot_path.md; the two facts that decide the
// port's shape are there, and both are negative:
//
//   - retail Echoes has no `CMain::OpenWindow`. `config/G2ME01/symbols.txt` names 19 `CMain`
//     methods and OpenWindow is not one of them, the string does not occur anywhere in the
//     DOL's disassembly, and this function - fully disassembled - makes no call on
//     `x0_osContext` at all. The window/VI bring-up lives in the *caller* of `InvokeCMain`,
//     `main` at 0x801EFB00, through its sixth argument.
//   - the frame loop is unreachable, not merely unwritten: it needs a constructed
//     `CGameArchitectureSupport`, whose constructor dereferences `gpTweakPlayerA` at
//     0x80007F38 with no null test, and `gpGameState` at 0x800081A4.
//
// So the host body lives in src/MetroidPrime/PortBoot.cpp behind `#ifdef TARGET_PC`, in a
// translation unit `configure.py` never claims - which is also why this guard costs the
// matching build nothing: mwcceppc does not define TARGET_PC, so it compiles exactly the
// empty body it compiled before. See docs/research/boot_path.md for the full ordered list.
#ifndef TARGET_PC
int CMain::RsMain(int argc, const char* const* argv) {}
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
    CPakFile& file = resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      file.EnsureWorldPakReady();
    }
  }
}

void CMain::StreamNewGameState(CInputStream& in, int saveIdx) {
  // TODO
  gameGlobalObjects->GameState() = nullptr;
  gpGameState = nullptr;
  gameGlobalObjects->GameState() = new CGameState(in, saveIdx);
  gpGameState = gameGlobalObjects->GameState().get();
  // gpGameState->HintOptions().SetHintNextTime();
}

CPlayerState::~CPlayerState() {}

CPlayerState::SPersistentState::~SPersistentState() {}

CStaticInterference::~CStaticInterference() {}
