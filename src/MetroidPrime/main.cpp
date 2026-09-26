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

// Retail 0x80008680, 0x15C = 348 bytes. See the block comment at the bottom for the two
// parts of it that are *harmful* on a host and therefore live behind TARGET_PC, and for the
// five callees that are not written.
//
// The two retail globals this reads are `extern` only, and deliberately so:
// `lbl_80418BA8` is .sdata at 0x80418BA8 holding 0x4000 - which is Aurora's own
// `ARAM_STACK_START` (extern/aurora/lib/dolphin/AR.cpp:17) - and it is the ARAM bump pointer,
// `lbl_80418BA8 = lbl_80418BA8 + ARAlloc(lbl_80418EA0)`. `lbl_80418EA0` is the four bytes at
// 0x80418EA0, .sbss, so zero at load; the one writer is `fn_80009864` (0x80009864, in this unit
// and in the ldscript's FORCEACTIVE list), which computes it as `*(u32*)0x80415980 * 14`.
// Both are declared rather than defined because a definition in this `NonMatching` unit
// costs: adding one `static` below moved `__ct__CGameArchitectureSupport` 84.51% -> 81.54%
// and `AddWorldPaks` 96.00% -> 95.97% (measured, and recorded at the head of
// src/MetroidPrime/PortGlobals.cpp). They are also only reachable on the non-TARGET_PC path,
// so no port build needs a definition of either.
extern "C" uint lbl_80418BA8;
extern "C" uint lbl_80418EA0;

// The two printf formats are named buffers, not literals, for the reason given at
// `kShotSmoke` above: a string literal in this unit makes mwcceppc re-optimise an unrelated
// function. Both strings are retail's, at 0x803A5847 and 0x803A585D in .rodata.
static const char kProtectingStack[] = "Protecting stack...  ";
static const char kStackRange[] = "Stack: 0x%8.8x down to 0x%8.8x\n";

// The 8 KB of stack-guard fill. Retail stores one word, 0x7338D00D, per 4 bytes
// (0x3CA07338 / 0x38A5D00D at 0x80008710 and 0x8000871C). It is not a `memset`: the four
// bytes of that word are 0D D0 38 73, not one repeated value, so the source is a `uint`
// fill loop and MWCC compiled it as a word loop unrolled eight-wide with a remainder,
// which is the shape at 0x80008728-0x80008770. boot_path.md step 11 records this constant as
// 0x7338D0D0; that is a transposition, and the bytes above are what the DOL contains.
static const uint kStackGuardWord = 0x7338D00D;

#ifdef TARGET_PC
// Host: see the block comment below. Declared in PortBoot.cpp.
void PortInitializeSubsystems();
#else
void CMain::InitializeSubsystems() {
  ARInit((u32*) 0x803c5ab8, 3);
  lbl_80418BA8 = lbl_80418BA8 + ARAlloc(lbl_80418EA0);
  ARQInit();

  OSThread* thread = OSGetCurrentThread();
  printf(kProtectingStack);
  uint* guardEnd = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023);
  OSProtectRange(3, guardEnd, 1024, 0);
  uint* fillStart = (uint*)(((uintptr_t)thread->stackBase) - 0x2000);
  uint* fillEnd = guardEnd + 0x400 / 4;
  for (uint* p = fillStart; p < fillEnd; ++p) {
    *p = kStackGuardWord;
  }
  DCFlushRange(fillEnd, (uint)((uintptr_t)fillStart - (uintptr_t)fillEnd));
  printf(kStackRange, (unsigned)(uintptr_t)thread->stackBase, (unsigned)(uintptr_t)thread->stackEnd);

  // Not written, and deliberately not faked with calls: fn_802DAE30, fn_8002ADC8,
  // fn_80301CC4(2048, 0x600000, 4096), fn_800E85A8 and fn_800DC0B0 are five unwritten
  // functions. Calling them would add five symbols to the port's link gap and close none,
  // which is the trap src/MetroidPrime/CMiscTableInit.cpp documents. Their order and
  // arguments are measured above and in docs/research/boot_path.md step 11.
  CFrameDelayedKiller::Initialize();
}
#endif // TARGET_PC

// Why the host path is a different function, in one paragraph, because "reproducing retail
// here is correct for the matching build and actively dangerous for the port" is the claim.
//
// Aurora's `ARInit` (extern/aurora/lib/dolphin/AR.cpp:98) only *stores* the pointer it is
// handed - `AR_BlockLength = stack_index_addr; sAllocationStackBase = stack_index_addr;` -
// and the very next call dereferences it: `*AR_BlockLength = length; AR_BlockLength += 1;`
// (lines 71-73 of the same file). Retail's pointer is the guest address 0x803C5AB8, three
// words of .bss in the DOL, so on a PC build `ARAlloc` writes through 0x803C5AB8 and
// faults. `ARAlloc` faults *before* that as well, on its own assert:
// `AURORA_ASSERT(AR_init_flag && !(length & 0x1f), ...)`, and the length retail passes is
// the guest word at 0x80418E90, so the assert fires on any PC. Aurora is not the problem -
// it defines ARInit/ARAlloc/ARQInit - so this is not a link error to fix elsewhere; it is
// one line that has to differ on PC.
//
// The second host difference is the stack-guard block. Aurora's `OSThread` puts
// `stackBase` at +0x304 and `stackEnd` at +0x308 - the *same* offsets retail reads
// (extern/aurora/include/dolphin/os/OSThread.h:53-54) - so the shape compiles and the
// offsets are right, and that is exactly why it is dangerous: the block fills 8 KB *below*
// `stackBase`, which on a PC is not this thread's stack at all but whatever Aurora's
// allocator put there, and then calls `OSProtectRange(3, ..., 1024, 0)` and `DCFlushRange`
// over it. Aurora even has `OSClearStack(u8 val)` for the intended purpose. Retail's own
// value 0x7338D00D is not a byte, so it cannot be reproduced through `OSClearStack`.
//
// So the host body is `PortInitializeSubsystems()` in src/MetroidPrime/PortBoot.cpp, a
// translation unit configure.py never claims - the same arrangement as CMain::OpenWindow
// and CMain::RsMain. mwcceppc does not define TARGET_PC, so this guard costs the matching
// build nothing: the object it compiles is byte for byte the retail body.

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
// The two parm classes used to be defined here, in an anonymous namespace, which gave their vtables
// **local** symbols - so `CMainFlow::OnMessage`, whose bytes store the derived one's address
// (0x803B1B60) literally, could not name, claim or place them. They are in
// include/MetroidPrime/CArchitectureMessageParm.hpp now, with their destructors out of line in
// MetroidPrime/CFrameMsgParmDtor.cpp and MetroidPrime/CTimerMsgParmDtor.cpp; see
// docs/research/boot_probe.md's closing section.

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

// Retail 0x80007168, 0x790 = 1,936 bytes, and this is the keystone of the resource system:
// it is what puts the paks and the entity factories into the game, and it is why
// `CMain::StreamNewGameState` cannot be reached - the new-game state is read out of a pak.
// The full block-by-block map, with every address and every unidentified callee, is in
// docs/research/paks.md. The short version of what is and is not written here:
//
//   written     the two identity-matrix calls, **all eleven** `AddPakFileAsync` calls, and
//               **all 36 factory registrations** (0x80007504-0x80007864, 864 bytes) - 1,344
//               of the function's 1,936 bytes.
//   not written the `Standard.NTWK` ARAM read (0x800071E8-0x80007278) - its six callees are
//               unnamed and unwritten; block 6's `CErrorOutputWindow` and viewport; the
//               controller create and the load loop at 0x80007418-0x800074BC; the game-state
//               record choice; and the teardown at 0x80007864-0x800078F8.
//
// **The 36 registrations cost the port's link gap nothing, and that is measured, not assumed.**
// Writing them as bare calls is a 34-symbol REGRESSION (33 unnamed `fn_*` factories plus
// `FStringTableFactory`) - gross closed 0, gross opened 34. They are written anyway because
// `include/Kyoto/CFactoryFunctions.hpp` declares all 36 and
// `src/Kyoto/CFactoryFunctionsPort.cpp` gives the 33 that retail leaves unnamed a body, so the
// symbols resolve and the net is 0. The full FourCC -> factory-address table is in
// docs/research/paks.md; this lane's report adds the per-factory `operator new` size, the
// `??(??)` literal's retail symbol and the resource's stream constructor to it.
void CGameGlobalObjects::AddPaksAndFactories() {
  // 0x80007170 / 0x80007194. `sIdentity__12CTransform4f` is .bss 0x804173D4, and both
  // CGraphics methods are static, which is why there is no `this` load before either call.
  // `Identity()` is the public accessor for that static - `sIdentity` itself is private.
  CGraphics::SetViewPointMatrix(CTransform4f::Identity());
  CGraphics::SetModelMatrix(CTransform4f::Identity());

  // 0x800071A0-0x800071E4. "Strings.pak" is the probe, "aram:Strings" the pak: retail
  // keeps the ARAM copy in a file named after the pak and loads that instead. The pak
  // base 0x803A56C0 is a pool of concatenated strings - "??(??)..", "%d", ".pak" and every
  // name in this function - and the eleven below are at +112, +176, +214, +221, +230, +244,
  // +258, +272, +285 and +298 from it.
  if (CDvdFile::FileExists("Strings.pak")) {
    gpResourceFactory->GetResLoader().AddPakFileAsync(rstl::string_l("aram:Strings"), false, false);
  }

  // 0x80007280 through 0x800073A0, six unconditional adds. Only "aram:TestAnim" sets the
  // second argument (r5 = 1 at 0x8000732C); the rest pass 0, and all six pass 0 for the
  // third. These are the ARAM-side paks: the first two are plain names, the rest carry the
  // "aram:" prefix because they are looked up inside the ARAM image.
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  resLoader.AddPakFileAsync(rstl::string_l("NoARAM"), false, false);
  resLoader.AddPakFileAsync(rstl::string_l("AudioGrp"), false, false);
  resLoader.AddPakFileAsync(rstl::string_l("aram:MiscData"), false, false);
  resLoader.AddPakFileAsync(rstl::string_l("aram:TestAnim"), true, false);
  resLoader.AddPakFileAsync(rstl::string_l("aram:MidiData"), false, false);
  resLoader.AddPakFileAsync(rstl::string_l("aram:GGuiSys"), false, false);

  // 0x800073A0-0x800073E4. "FrontEnd.pak" is the probe and "FrontEnd" the pak, and this
  // one *is* a world pak: r6 = 1 at 0x800073D8, where every block above passed 0.
  if (CDvdFile::FileExists("FrontEnd.pak")) {
    resLoader.AddPakFileAsync(rstl::string_l("FrontEnd"), false, true);
  }

  // 0x80007504-0x80007864, 864 bytes, 36 registrations of exactly 24 bytes each:
  //
  //   lis r3 / lis r4 / addi r5,r3,N / addi r3,r31,116 / addi r4,r4,N / bl
  //
  // r5 is the FourCC as a 32-bit immediate - retail folds the four characters into one word and
  // never materialises a string, so this block adds **no data** and a `Matching` unit could
  // contain it. r3 is `gpResourceFactory`+0x74, which is `CFactoryMgr`+0x00, and r4 is the
  // address of the factory function. `addi r3,r31,116` appears 36 times, which is what fixes
  // `CFactoryMgr` at `CResFactory`+0x74 and is why `Kyoto/CResFactory.hpp` carries that offset.
  //
  // Two registrars, and the choice between them is not a style choice: `fn_802F96E0`
  // (0x802F96E0) inserts into the FourCC-keyed map and 33 of the 36 use it, while
  // `fn_802F963C` (0x802F963C) inserts into the owner-keyed one and **only CMDL, AGSC and PATH**
  // use it - the only three whose factory functions take a fourth argument. Both are
  // "insert if absent" over an `rstl::map`; `Kyoto/CFactoryMgrRegistrars.cpp` is their body and
  // is port-only, because retail has them unnamed and renaming them in `symbols.txt` is a
  // DOL-wide change this file should not make on its own.
  // 0x80007508. `STRG` is the only one of the 36 whose factory retail's own symbol table names.
  CFactoryMgr& factoryMgr = gpResourceFactory->GetFactoryMgr();
  factoryMgr.RegisterFactoryByTypeIdx('STRG', FStringTableFactory);
  factoryMgr.RegisterFactoryByOwner('CMDL', fn_80311340);
  factoryMgr.RegisterFactoryByTypeIdx('TXTR', fn_802C4878);
  factoryMgr.RegisterFactoryByTypeIdx('CSKR', fn_8030FEAC);
  factoryMgr.RegisterFactoryByTypeIdx('ANIM', fn_802B3200);
  factoryMgr.RegisterFactoryByTypeIdx('CINF', fn_802AC2D8);
  factoryMgr.RegisterFactoryByTypeIdx('ANCS', fn_8028E7BC);
  factoryMgr.RegisterFactoryByTypeIdx('CRSC', fn_8025DD1C);
  factoryMgr.RegisterFactoryByTypeIdx('SWHC', fn_802ED864);
  factoryMgr.RegisterFactoryByTypeIdx('PART', fn_802E7A78);
  factoryMgr.RegisterFactoryByTypeIdx('ELSC', fn_8031B4D8);
  factoryMgr.RegisterFactoryByTypeIdx('SPSC', fn_8032B5DC);
  factoryMgr.RegisterFactoryByTypeIdx('SRSC', fn_8032F0D4);
  factoryMgr.RegisterFactoryByTypeIdx('WPSC', fn_8025DB38);
  factoryMgr.RegisterFactoryByTypeIdx('FRME', fn_80274FD4);
  factoryMgr.RegisterFactoryByTypeIdx('FONT', fn_802B514C);
  factoryMgr.RegisterFactoryByTypeIdx('SCAN', fn_80110B18);
  factoryMgr.RegisterFactoryByTypeIdx('AFSM', fn_8019405C);
  factoryMgr.RegisterFactoryByTypeIdx('FSM2', fn_801FD314);
  factoryMgr.RegisterFactoryByOwner('AGSC', fn_80307544);
  factoryMgr.RegisterFactoryByTypeIdx('DCLN', fn_80254414);
  factoryMgr.RegisterFactoryByTypeIdx('DPSC', fn_802601D0);
  factoryMgr.RegisterFactoryByTypeIdx('ATBL', fn_8029AB80);
  factoryMgr.RegisterFactoryByOwner('PATH', fn_8013FDB8);
  factoryMgr.RegisterFactoryByTypeIdx('MAPW', fn_80093638);
  factoryMgr.RegisterFactoryByTypeIdx('MAPA', fn_8007E32C);
  factoryMgr.RegisterFactoryByTypeIdx('MAPU', fn_801545F0);
  factoryMgr.RegisterFactoryByTypeIdx('CSNG', fn_80314DC4);
  factoryMgr.RegisterFactoryByTypeIdx('DGRP', FDependencyGroupFactory);
  factoryMgr.RegisterFactoryByTypeIdx('SAVW', fn_80182830);
  factoryMgr.RegisterFactoryByTypeIdx('HINT', fn_8017F988);
  factoryMgr.RegisterFactoryByTypeIdx('CSPP', fn_8028B1F4);
  factoryMgr.RegisterFactoryByTypeIdx('PTLA', fn_80255600);
  factoryMgr.RegisterFactoryByTypeIdx('STLC', fn_802FF4BC);
  factoryMgr.RegisterFactoryByTypeIdx('EGMC', fn_801EF598);
  factoryMgr.RegisterFactoryByTypeIdx('RULE', FRuleSetFactory);
}

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

// Retail 0x800053B8, 0x214 = 532 bytes. This is the function that makes the new game state
// reachable, and it is unreachable without `CGameGlobalObjects::AddPaksAndFactories`, because
// the record it reads is inside a pak. The order is measured, block by block, and the
// dependency is a cycle worth stating plainly:
//
//   AddPaksAndFactories (step 13)  ->  gpResourceFactory, gpSimplePool
//   StreamNewGameState  (this)     ->  a record out of a pak, via the factory above
//   CGameArchitectureSupport ctor  ->  gpGameState, which only this function sets
//
// so step 17 faults on a null `gpGameState` until step 13 exists, and step 13 needs the
// sixteen symbols in correction 3 of docs/research/boot_path.md to be reachable at all. The
// full block map, with every CGameState offset retail reads, is in docs/research/paks.md.
//
// What retail does, in order, and what is written here:
//
//   0x800053D0  construct a local at r1+0xB0 from gpGameState+0x54      fn_80005108    (unwritten)
//   0x800053E0  construct a local at r1+0x7C from gpGameState+0x110     fn_80004C90    (unwritten)
//   0x800053FC  construct a local at r1+0x24 from gpGameState+0x188     fn_80004AA0    (unwritten)
//   0x8000540C  flag = (that local's first word != 0)                  written
//   0x80005428  construct a local at r1+0x48 from gpGameState+0x144     fn_80004C90    (unwritten)
//   0x80005438  construct a local at r1+0x14 from gpGameState+0x178     fn_80004AA0    (unwritten)
//   0x80005448  construct a local at r1+0xDC from gpGameState+0x80      fn_80004E84    (unwritten)
//   0x80005458  release the old CGameState (single_ptr::operator=(0))  written
//   0x80005470  gpGameState = 0                                        written
//   0x80005474  pick the record: the r1+0x24 local if flag, else        written (shape)
//               records[r1+0x7C+0x5C] out of an array at r1+0x80,
//               16 bytes each, data at +0x04 and size at +0x0C
//   0x80005494  CMemoryInStream(data, size)                            written (shape)
//   0x800054A0  CBitStreamReader(that stream)                          written (shape)
//   0x800054B4  ::operator new(752, "??(??)..", 0)                     written (shape)
//   0x800054C4  fn_80144140(bitStreamReader) - the CGameState ctor,    (unwritten, 0x684)
//               1,668 bytes (0x684), and the only writer of the fields below
//   0x800054D4  publish it into gameGlobalObjects' single_ptr          written
//   0x80005500  gpGameState = the new one                              written
//   0x8000550C  copy-assign the r1+0xB0 local into the new +0x54       fn_80003F08    (unwritten)
//   0x80005518  fn_80142FA4(new, r1+0x7C local)                         (unwritten)
//   0x80005524  fn_80142920(new, r1+0x48 local)                         (unwritten)
//   0x80005530  fn_801427DC(new, r1+0x14 local)                         (unwritten)
//   0x80005538  fn_80003D00(&new->x80, r1+0xDC local)                   (unwritten)
//   0x80005550  CGameOptions::EnsureOptions()                          **written**
//   0x80005558  new->x10C = the old x10C, new->x108 = the old x108      written (shape)
//   0x80005568  fn_80142FEC(new), only when the flag is set             (unwritten)
//   0x80005570  six destructors, in reverse order                       (unwritten)
//
// The seventeen `(unwritten)` callees are all unnamed in `config/G2ME01/symbols.txt` and none
// is defined in the port, so writing them as calls would add seventeen symbols to the
// link-gap ratchet and close none. `CGameOptions::EnsureOptions` is the one real behaviour
// in this function that the port can already do, and it is written.
void CMain::StreamNewGameState(CInputStream& in, int saveIdx) {
  gameGlobalObjects->GameState() = nullptr;
  gpGameState = nullptr;
  gameGlobalObjects->GameState() = new CGameState(in, saveIdx);
  gpGameState = gameGlobalObjects->GameState().get();
  // 0x80005550: `gpGameState + 0x80`, and `CGameState::gameOptions` is at +0x80 in
  // include/MetroidPrime/Player/CGameState.hpp - pad1b ends at 0x80. This is the same call
  // CGameArchitectureSupport's constructor makes at 0x800081AC, so it costs the ratchet
  // nothing, and it is the step that turns a freshly-read options block into a usable one.
  gpGameState->GameOptions().EnsureOptions();
  // gpGameState->HintOptions().SetHintNextTime();
}

CPlayerState::~CPlayerState() {}

CPlayerState::SPersistentState::~SPersistentState() {}

CStaticInterference::~CStaticInterference() {}
