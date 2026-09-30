#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Audio/CDSPStreamManager.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Audio/CStreamAudioManager.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "dolphin/ar.h"
#include "dolphin/os.h"
#include "dolphin/os/OSThread.h"

// `<stdint.h>` is where `uintptr_t` comes from, and `CMain::ShutdownSubsystems` below casts
// through it four times. `dolphin/types.h` guards its own `<stdint.h>` behind TARGET_PC, and
// mwcceppc does not define TARGET_PC, so it is named directly.
#include <stdint.h>
#include <stdio.h>

#include "MetaRender/CCubeRenderer.hpp"
#include "MetaRender/IRenderer.hpp"

#include "MetroidPrime/CAudioStateWin.hpp"
#include "MetroidPrime/CArchitectureQueue.hpp"
#include "MetroidPrime/CConsoleOutputWindow.hpp"
#include "MetroidPrime/Decode.hpp"
#include "MetroidPrime/CErrorOutputWindow.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CMainFlow.hpp"
#include "MetroidPrime/CMapWorldInfo.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CInGameTweakManager.hpp"
#include "MetroidPrime/CWorldTransManagerView.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

class CCharacterFactoryBuilder;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

extern "C" void fn_8029EFCC();
extern "C" void fn_8033CEE8();
IRenderer* AllocateRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys, IFactory& resFactory);

extern "C" {
// Retail `.rodata` 0x803A56C0, 0x1C0 bytes - retail's own string pool, and the pak names, the
// resource names and the two printf formats all live in it. **Declared, never defined**: a
// literal of our own would be routed through mwcceppc's per-unit `@stringBase0` pool and emit
// three instructions that name *that* pool instead of retail's, which is the whole difference
// between 99.94% and 100% on `CMain::FillInAssetIDs`. Retail reaches each as
// `lis rN, lbl_803A56C0@ha / addi rN,rN, lbl_803A56C0@l / addi rN,rN,<offset>`; naming the pool
// and indexing it reproduces the triple.
extern const char lbl_803A56C0[];
// Retail `.sdata2` 0x8041A420, `data:float`, 0x41200000 = **10.0f**. `InfiniteLoopAlarm` is its
// only reader in this range and retail loads it as a relocation against this symbol, so the bare
// literal `10.f` would come out as a reference to our own `@1260` instead.
extern const float lbl_8041A420;
// Retail `.sbss` 0x80418EC4: `&ioWinMgr`, published by `CGameArchitectureSupport`'s constructor
// (0x80007F80) and cleared by its destructor (0x80007E28). Four bytes, declared only - the port
// defines it in `src/MetroidPrime/PortGlobals.cpp` and this unit is `NonMatching`.
extern CIOWinManager* lbl_80418EC4;
// Retail `.sbss` 0x80419300, the `IController*` published by `CGameArchitectureSupport`'s
// constructor (0x80007FD4) and named `gpController` in `config/G2ME01/symbols.txt:20698`. Four
// bytes, declared only - the port defines it in `src/MetroidPrime/PortGlobals.cpp`.
extern IController* gpController;
// Retail `.sbss` 0x80418EC8, the address of `CGameGlobalObjects`' +0x150 member, written by its
// constructor at 0x80008558 and read by `CMain::ShutdownSubsystems`'s pump loop.
extern void* lbl_80418EC8;
// Retail `.sdata` 0x8033CDA0 = `CDSPStreamManager::Shutdown`, called with no argument setup
// between `CGameArchitectureSupport`'s `UnloadAudio()` and `~CIOWinManager` (0x80007E30).
void fn_8033CDA0();
} // extern "C"

CResFactory* gpResourceFactory;
CSimplePool* gpSimplePool;
CCharacterFactoryBuilder* gpCharacterFactoryBuilder;
CStringTable* gpStringTable;
CMain* gpMain;
CGameState* gpGameState;
CMemoryCard* gpMemoryCard;
CInGameTweakManager* gpTweakManager;
float sInfiniteLoopTime;

static uchar sMainSpace[sizeof(CMain)];

// The three functions above `CMain::CMain` in retail's address order. mwcceppc emits in reverse
// source order and the rest of this file is descending by address, so these go first, also
// descending, and the whole translation unit is one descending run.
extern "C" void __sys_free(const void* ptr) { CMemory::Free(ptr); }

// Retail 0x80008A1C, 0xC = 12 bytes:
//     lbz r0, 0x90(r3) ; extrwi r3, r0, 1, 26 ; blr
// `extrwi r3,r0,1,26` extracts bit 26 of the loaded byte, i.e. **bit 2 of the byte at +0x90** -
// `finished`(0), `mfGameBuilt`(1), `screenFading`(2) - so the answer is `screenFading` and not
// `finished`, which is the test upstream's name would suggest.
bool CMain::fn_80008A1C() { return screenFading; }

// Retail 0x800089AC, 0x10 = 16 bytes:
//     lbz r0, 0x91(r3) ; rlwimi r0, r4, 7, 24, 24 ; stb r0, 0x91(r3) ; blr
// `rlwimi r0,rX,7-n,24+n,24+n` is field *n* counted down, so this writes bit 0 of the byte at
// +0x91 - the `gameFrameDrawn` group the eighth `bool : 1` above does not reach. The accessor and
// the bitfield moved out of `#ifdef TARGET_PC` in `include/MetroidPrime/CMain.hpp`; without that
// the matching build had no member there and emitted nothing at all.
void CMain::SetGameFrameDrawn(bool drawn) { gameFrameDrawn = drawn; }

CMain::CMain(COsContext* context, void* unk1, CMemorySys* memorySys, void* unk2)
: osContext(context)
, mUnk1(unk1)
, memorySys(memorySys)
, mUnk2(unk2)
// , xe8_(0.0)
// , x118_(0.f)
// , x11c_(0.f)
// , x120_(0.f)
// , x124_(0.f)
, frameTimeMinimum(0)
, x4c(0.0f)
, gameGlobalObjects(nullptr)
, restartMode(kRM_Default)
, x5c(1.0f)
, frameTimes(0xF4240)
, frameTimeIdx(0)
, finished(false)
, mfGameBuilt(false)
, screenFading(false)
, x90_27_(false)
, mManageCard(false)
, x90_29_(false)
, x90_30_(false)
, mCardBusy(false)
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

void CMain::InitializeSubsystems() {
  ARInit((u32*) 0x803c5ab8, 3);  // (u32*)(&sMainSpace + 0x98)
  // TODO
}

// Retail 0x80008570, 0x110 = 272 bytes. The ten callees are retail functions this tree has no
// body for and `config/G2ME01/symbols.txt` names at their own addresses, so they are declared and
// called: **a callee's body is not a precondition for reproducing a function**, and declaring them
// costs the matching build nothing because `dtk dol split` supplies retail's bytes for the whole
// claimed range. The 0x801F0xxx family is one class - an 8-byte object `{void* x0; bool x4;}` -
// and `fn_801F03C4` copies its `string` argument into it, so retail's second argument really is an
// `rstl::string const&` and not a `char const*`: the call passes **the address of the temporary**.
//
// **The host does not compile this body at all**, and this is a `#ifdef` rather than a comment
// because retail's tail reads `OSGetCurrentThread()` +0x304/+0x308 as a stack pointer and looks
// for 0x7337D00D in the 8 KB *below* it. Aurora's `OSThread` puts `stackBase`/`stackEnd` at
// exactly those offsets, so the shape compiles and the offsets are right - and what it reads is
// Aurora's allocator's memory rather than a stack. The port's body is `PortShutdownSubsystems()`
// in `src/MetroidPrime/PortBoot.cpp`, a translation unit `configure.py` never claims; mwcceppc
// does not define TARGET_PC, so the matching build still compiles retail's body verbatim.
extern "C" void fn_800E8494();
extern "C" void fn_802DAE24();
extern "C" void fn_8002AD44();
extern "C" void* fn_801F03C4(void* self, const rstl::string& name, bool start);
extern "C" void fn_801F02C4(void* self);
extern "C" int fn_801F025C(void* self);
extern "C" void fn_801F05D0(void* owner);
extern "C" void fn_80218760();
extern "C" void fn_801F0280(void* self);
extern "C" void fn_801F0308(void* self, short value);
extern "C" void fn_801F0518(void* owner);
extern "C" void fn_800DC03C();

// The 8-byte object the 0x801F0xxx class occupies on the stack, at r1+8. It is **not** a retail
// type and it is never constructed here: `fn_801F03C4` is what fills it in, and retail emits no
// store to r1+8 before that call. A local whose address is taken and whose members are never read
// is the only shape that allocates eight bytes and nothing else.
struct STuObject {
  void* x0_owner;
  bool x4_started;
};

void PortShutdownSubsystems();

#ifdef TARGET_PC
void CMain::ShutdownSubsystems() { PortShutdownSubsystems(); }
#else
void CMain::ShutdownSubsystems() {
  CFrameDelayedKiller::ShutDown();
  fn_800E8494();
  fn_802DAE24();
  fn_8002AD44();

  STuObject tu;
  fn_801F03C4(&tu, rstl::string_l(lbl_803A56C0 + 0xCB), true);
  fn_801F02C4(&tu);

  // The condition is a **byte mask on an int**, not a boolean, and the constant is measured rather
  // than guessed. Retail's test is `clrlwi. r0,r3,24` + `beq`, which keeps the low eight bits.
  // Thirteen spellings of the obvious `& 0xFF000000` (and of `!= 0`, `>> 24`, `<< 8`,
  // signed/unsigned, the operands reversed) all compile to `clrrwi. r0,r3,24` and are one
  // instruction wrong; `& 0xFF` is the only mask measured that emits retail's bytes.
  // `& 0xFF != 0` would be `cmpwi r3,0` and one instruction shorter, so the mask is in the source.
  while ((fn_801F025C(&tu) & 0xFF) == 0) {
    fn_801F05D0(lbl_80418EC8);
  }

  fn_80218760();
  fn_801F0280(&tu);
  fn_801F0308(&tu, -1);
  fn_801F0518(lbl_80418EC8);
  fn_800DC03C();

  OSThread* thread = OSGetCurrentThread();
  uint stackBase = (uint)thread->stackBase;
  uint* p = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023);
  // **Two statements, not one expression, and that is load-bearing.** Written as a single
  // `p = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023) + 0x400;` the function is five
  // instructions short of retail and no spelling of it gets closer: mwcceppc's register allocator
  // puts the masked value in r3 and `p` in r4, so `limit` needs a fourth register and lands in r6
  // where retail has it in r3. Written as a separate `p += 0x100` the allocator coalesces the
  // masked value into `p`'s register, r4 serves both, and r3 stays free for `limit`.
  // `0x100` is 256 **words**; mwcceppc scales it to the `addi`'s 1024 bytes.
  p += 0x100;
  for (uint* limit = (uint*)(stackBase - 0x2000); p < limit; ++p) {
    // `addis r0,r3,-29495 ; cmplwi r0,53261`, i.e. `word + 0x8CC90000 == 0xD00D`. The same
    // constant `CMain::InitializeSubsystems` stores at 0x80008710, 0x2EC bytes away, agreeing.
    if (*p + 0x8CC90000 != 0xD00D) {
      break;
    }
  }
  uint used = (uint)(stackBase - 0x2000) - (uint)p + 0x2000;
  OSReport(lbl_803A56C0 + 0x16A, used, used >> 10);
}
#endif // TARGET_PC

// Retail 0x8000848C, 0xE4 = 228 bytes, and **the only writer of `gpGameState` in the DOL** -
// `CGameArchitectureSupport`'s constructor loads it at 0x800081A4 with no null test, and the store
// is 0x8000854C. The body is four member constructors, two allocations and six global stores, and
// it never reads its two parameters (`fn_800084A0`'s prologue is
// `stwu r1,-16(r1); mflr r0; stw r0,20(r1); stw r31,12(r1); mr r31,r3` and r4/r5 are untouched for
// the whole 0xE4).
//
// **The two allocations are written out rather than spelled `new`,** and that is load-bearing:
// they are the two sites that pass retail's `.rodata` pool as `operator new`'s file operand, and
// the `CGameState* made = self; if (made != 0) { made = f(made); } return made;` shape is what
// puts the callee's result in **r0** instead of leaving it in r3, which is what retail does
// (`mr r0,r3 ; stw r0,304(r31)` against `stw r3,304(r31)` for a ternary or a `static_cast`).
// Measured, four variants; only the named temporary fixes it, and it fixes both allocations.
extern "C" CGameState* fn_801449C8(CGameState* self);
extern "C" CInGameTweakManager* fn_8016C230(CInGameTweakManager* self);

static inline CGameState* MakeCGameState() {
  CGameState* self = static_cast< CGameState* >(::operator new(sizeof(CGameState)));
  CGameState* made = self;
  if (made != 0) {
    made = fn_801449C8(made);
  }
  return made;
}

static inline CInGameTweakManager* MakeInGameTweakManager() {
  CInGameTweakManager* self =
      static_cast< CInGameTweakManager* >(::operator new(sizeof(CInGameTweakManager)));
  CInGameTweakManager* made = self;
  if (made != 0) {
    made = fn_8016C230(made);
  }
  return made;
}

CGameGlobalObjects::CGameGlobalObjects(COsContext& osContext, CMemorySys& memorySys)
    : pad0()
    , resFactory()
    , simplePool(resFactory)
    , characterFactoryBuilder()
    , gameState(MakeCGameState())
    , inGameTweakManager(MakeInGameTweakManager()) {
  // The six stores at 0x80008534-0x80008558. `_SDA_BASE_` is 0x8041FD80 and the displacements are
  // the full signed ones, so -28380 is `gpResourceFactory`, -28376 `gpSimplePool`, -28372
  // `gpCharacterFactoryBuilder`, -28360 `gpGameState`, -28352 `gpTweakManager` and -28344 is
  // 0x80418EC8. `resFactory`, `simplePool` and `characterFactoryBuilder` are the *members'*
  // addresses; `gameState` and `inGameTweakManager` are read back out of their `single_ptr`s with
  // `lwz`, because the constructor above stored the result there.
  //
  // The two parameters are named because the signature is retail's, and are unused because retail
  // never reads them.
  (void)osContext;
  (void)memorySys;
  gpResourceFactory = &resFactory;
  gpSimplePool = &simplePool;
  gpCharacterFactoryBuilder = &characterFactoryBuilder;
  gpGameState = gameState.get();
  gpTweakManager = inGameTweakManager.get();
  lbl_80418EC8 = &x150_tail;
}

void CGameGlobalObjects::PostInitialize(COsContext& osContext, CMemorySys& memorySys) {
  AddPaksAndFactories();
  LoadStringTable();
  printf(lbl_803A56C0 + 0x150);
  renderer = AllocateRenderer(simplePool, osContext, memorySys, resFactory);
  // Retail stores the renderer into +0x148 and reads it back there, then writes the result into
  // `gpRender` - a separate store, and the reason the vtable load below is `lwz r0, 0x148(r29)`.
  gpRender = reinterpret_cast< CCubeRenderer* >(renderer.get());
  CEnvFxManager::Initialize();
}

void CGameGlobalObjects::LoadStringTable() {
  stringTable = gpSimplePool->GetObj(lbl_803A56C0 + 0x146);
  gpStringTable = **stringTable;
}

// Retail 0x8000823C. `sInfiniteLoopTime >= lbl_8041A420` and not `>= 10.f`: retail loads the
// constant as a relocation against `.sdata2` 0x8041A420, and the bare literal would come out as a
// reference to our own pool. The format string is at +0x133 into `lbl_803A56C0` for the same reason.
void InfiniteLoopAlarm(OSAlarm* alarm, OSContext* context) {
  if (sInfiniteLoopTime >= lbl_8041A420) {
    OSCancelAlarm(alarm);
    rs_debugger_printf(lbl_803A56C0 + 0x133);
  }
  sInfiniteLoopTime += alarm->period / OS_TIMER_CLOCK;
}

// Retail `.sbss` 0x80418EA0, the four bytes `CMain::InitializeSubsystems` hands to `ARAlloc` and
// which `CGameArchitectureSupport`'s constructor passes as `CAudioSys`'s `aramSize`. Zero at
// load; the one writer is `fn_80009864`, which computes it as `*(u32*)0x80415980 * 14`. **Read
// here rather than written as the literal `0x5fc000`**, because retail loads it
// (`lwz r8,lbl_80418EA0@r13` at 0x80007C2C, before the `li r4..r7,0x30` run that sets up the other
// four arguments) and a `0x5fc000` literal comes out as `lis r5,96 ; addi r8,r5,-16384` - two
// instructions in the wrong place for the same value. `src/MetroidPrime/CMainInitializeSubsystems.cpp`
// declares the same symbol with the same reasoning.
extern "C" uint lbl_80418EA0;

CGameArchitectureSupport::CGameArchitectureSupport(COsContext& osContext)
: audioSys(0x30, 0x30, 0x30, 0x30, lbl_80418EA0)
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
  // The two `bl` targets are `CSfxManager::Initialize` (0x8029EFCC, 0x54 bytes) and
  // `CDSPStreamManager::Initialize` (0x8033CEE8, 0x148), both of which `config/G2ME01/symbols.txt`
  // now names. They were `extern "C" void fn_8029EFCC()` / `fn_8033CEE8()` before, which emits
  // the identical instruction and the identical relocation target (retail's own address) - the
  // names here match the map rather than inventing `fn_` names for functions it names.
  CSfxManager::Initialize();
  CDSPStreamManager::Initialize();
  CStreamAudioManager::SetMusicVolume(0x7F);
  CAudioSys::TrkSetSampleRate(kTSR_One);
  gpMain->SetMaxSpeed(false);
  gpMain->ResetGameState();
  // 0x80007F80, between `ResetGameState` and the first `AddIOWin`. Retail publishes `&ioWinMgr`
  // into `.sbss` 0x80418EC4 here and the destructor clears it; 0x80007FD4 stores
  // `inputGenerator.GetController()` into `.sbss` 0x80419300 (`gpController`).
  //
  // **Both were skipped, on a claim about `symbols.txt` that is no longer true**: the comment
  // here used to say `gpController` "is not named in this tree's `symbols.txt`", so writing one
  // without the other was retail's 4 instructions against our 2. It is named -
  // `config/G2ME01/symbols.txt:20698`, `gpController = .sbss:0x80419300; // type:object size:0x4` -
  // so both are written, and retail's four instructions are what comes out.
  // **Written through a named local, and that is load-bearing.** Retail computes `&ioWinMgr` once
  // at 0x80007F80 (`addi r30,r31,68`) and every one of the four `AddIOWin` calls passes it as
  // `mr r3,r30`. Spelled `ioWinMgr.AddIOWin(...)` four times, mwcceppc re-materialises the address
  // each time (`addi r3,r31,68`) and spends r0 on the `.sbss` store instead of r30 - four
  // instructions of difference for the identical semantics. A local reference is what lets the
  // allocator hoist it.
  CIOWinManager& mgr = ioWinMgr;
  lbl_80418EC4 = &mgr;
  gpController = inputGenerator.GetController();
  mgr.AddIOWin(new CMainFlow(), 0, 0);
  mgr.AddIOWin(new CConsoleOutputWindow(8, 5.f, 0.75f), 100, 0);
  mgr.AddIOWin(new CAudioStateWin(), 100, -1);
  mgr.AddIOWin(new CErrorOutputWindow(CErrorOutputWindow::kF_Zero), 10000, 100000);
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
  // 0x80007E28: `li r0,0 ; stw r0,lbl_80418EC4`, between `RemoveAllIOWins` and `UnloadAudio`. The
  // counterpart of the store the constructor does.
  lbl_80418EC4 = 0;
  // `UnloadAudio` is declared `static` in `include/MetroidPrime/CGameArchitectureSupport.hpp`
  // precisely so that retail's `bl` at 0x80007E2C has no `mr r3,rN` in front of it.
  UnloadAudio();
  // 0x80007E30, immediately after `UnloadAudio` and before `~CIOWinManager`.
  fn_8033CDA0();
  // CSfxManager::Shutdown();
  // CDSPStreamManager::Shutdown();
}

bool CGameArchitectureSupport::UpdateTicks() {
  bool result = false;
  // **The saved value is what is restored, not a literal `1`.** Retail keeps `OSDisableInterrupts`'s
  // return in r29 across the stopwatch read and hands *that* register back at 0x80007CBC
  // (`mr r29,r3` after the call, `mr r3,r29` before the restore). Passing `1` is the same
  // instruction count but a different register, and it loses the value - which is the one thing
  // the pair exists for. Prime 1 spells it `const BOOL interrupts = ...; OSRestoreInterrupts(interrupts);`
  // and that is the whole fix.
  const u32 interrupts = OSDisableInterrupts();
  float stopwatchTime = stopwatch1.GetElapsedTime();
  stopwatch1.Reset();
  OSRestoreInterrupts(interrupts);
  sInfiniteLoopTime = 0.0f;
  x68_ += stopwatchTime;
  if (gpMain->GetFinished()) {
    x68_ = 0.033333335f;
  }
  bool flag = gpMain->fn_80008A1C();
  // `elapsed > 0.035f`, not `0.035 < elapsed`. Retail 0x80007C40 is
  // `lfs f0,lbl_8041A404 ; fcmpo cr0,f31,f0 ; ble` - the **elapsed** value is the first operand of
  // `fcmpo` and the branch is `ble`, so the operands are the other way round from ours and the
  // constant is the second. Spelled as written above mwcceppc emits `fcmpo cr0,f0,f31 ; bge`, which
  // is the same predicate with the operands swapped.
  if (flag || stopwatchTime > 0.035f) {
    gpMain->Increment_x5c(-stopwatchTime);
    x68_ = 0.016666668f;
  }
  archQueue.Push(MakeMsg::CreateFrameBegin(kAMT_Game, gameFrameCount));

  bool keepLooping = true;
  // `>=`: retail 0x80007D40 is `fcmpo` + `cror eq,gt,eq`.
  while (keepLooping || x68_ >= 0.016666668f) {
    keepLooping = false;
    if (!inputGenerator.Update(0.016666668f, archQueue)) {
      result = true;
    }
    archQueue.Push(MakeMsg::CreateTimerTick(kAMT_Game, 0.016666668f));
    x68_ -= 0.016666668f;
    ioWinMgr.PumpMessages(archQueue);
  }

  // Retail's epsilon is `lbl_8041A408` = 0.00005f, not `Real32::Epsilon()`.
  if (close_enough((x6c_ - x70_) + (x70_ - x68_), 0.0f, 0.00005f)) {
    x68_ = 0.0f;
  }

  x6c_ = x70_;
  x70_ = x68_;
  ioWinMgr.PumpMessages(archQueue);
  // **Not quitting**, which `RsMain` tests: retail ends `cntlzw r0,r0 ; srwi r3,r0,5` on r31, the
  // "input generator failed" flag - `return !result`. Returned uninverted, `RsMain` set `finished`
  // on the first frame whose messages were actually pumped (found 2026-09-29).
  return !result;
}

// Retail 0x80007A14, 0x70 = 112 bytes, and this is its body one-for-one.
//
// `gpGameState->GetWorldState()->Update()` is the two calls retail makes - `bl` on
// `CGameState::GetWorldState` (0x80142520), the `lwz r3,0(r3)` that dereferences the reference it
// returns, and `bl` on `CWorldTransManagerView::Update` (0x8015B9B0). **No null test on the world state**,
// unlike `CWorldTransManagerView::Update`'s own guard: retail's `CGameState` constructor always fills +0x3C,
// and adding a test here drops the function from 100% to 84.78%.
//
// Both callees are left undefined here: retail's bodies are in other units' ranges, and this unit
// is `NonMatching`, so `dtk dol split` supplies retail's bytes for the whole claim and the two
// relocations land on retail's own addresses.
void CGameArchitectureSupport::Update() {
  gpGameState->GetWorldState()->Update();
  archQueue.Push(MakeMsg::CreateFrameEnd(kAMT_Game, gameFrameCount));
  ioWinMgr.PumpMessages(archQueue);
}

// Retail 0x80007A80, 0x20 = 32 bytes: a frame, the one call, the frame out. `push_back` on the
// `rstl::list` is out of line in retail (`fn_80007AA0`) and mwcceppc inlines the member without
// expanding it, so the whole function is the call.
void CArchitectureQueue::Push(const CArchitectureMessage& msg) { mQueue.push_back(msg); }

// Retail 0x80007B20, 0xBC = 188 bytes. Prime 1's `CMain::MemoryCardInitializePump` is this
// function one call short of it: Echoes additionally seeds the system options from the card
// before `CGameState::InitializeMemoryStates`, and the measured bytes at 0x80007C10 are
// `lwz r3,gpGameState ; addi r3,r3,0x54 ; bl CPersistentOptions::InitializeMemoryState` -
// `+0x54` is `CGameState::mSystemOptions` (`include/MetroidPrime/Player/CGameState.hpp:291`).
//
// The allocation is written out rather than spelled `new`, for the reason the two
// `MakeCGameState`/`MakeInGameTweakManager` helpers above already carry: the `__nw__FUlPCcPCc`
// call site is one of the two that passes retail's `.rodata` pool as `operator new`'s file
// operand, and the `CMemoryCard* made = self; if (made != 0) { made = f(made); } return made;`
// shape is what puts the constructor's result in **r30** rather than leaving it in r3, which is
// what retail does at 0x80007C10 (`mr r30,r3` after the null test).
extern "C" CMemoryCard* fn_80177FF0(CMemoryCard* self);
static inline CMemoryCard* MakeCMemoryCard() {
  CMemoryCard* self = static_cast< CMemoryCard* >(::operator new(sizeof(CMemoryCard)));
  CMemoryCard* made = self;
  if (made != 0) {
    made = fn_80177FF0(made);
  }
  return made;
}

void CMain::MemoryCardInitializePump() {
  if (gpMemoryCard == nullptr) {
    if (gameGlobalObjects->MemoryCard().get() == nullptr) {
      gameGlobalObjects->MemoryCard() = MakeCMemoryCard();
    }
    CMemoryCard* card = gameGlobalObjects->MemoryCard().get();
    if (card->InitializePump()) {
      gpMemoryCard = card;
      gpGameState->SystemOptions().InitializeMemoryState();
      gpGameState->InitializeMemoryStates();
    }
  }
}

// Retail 0x800078F8, 0x60 = 96 bytes, and the whole body is what the compiler generates for a
// deleting destructor of a class whose only member is a base: store the derived vtable pointer back
// over the object's first word, call the base destructor with the flag zeroed, then release the
// object when the incoming flag is positive. `li r4,0` before the base call is exactly the "not
// deleting" flag the base's own D1 test reads.
//
// The store's `@ha`/`@l` pair relocates against `__vt__18CErrorOutputWindow`, which is
// `MetroidPrime/CErrorOutputWindow.cpp`'s `.data` object at 0x803B5910 - the same symbol retail's
// constructor reaches. mwcceppc also lays a 28-byte copy of that vtable down in this object,
// because the class's key functions are all undefined here; that `.data` is not claimed by
// `config/G2ME01/splits.txt` for this unit, which is one of the reasons it is not a flip candidate
// (see `docs/research/decl_order.md`).
CErrorOutputWindow::~CErrorOutputWindow() {}

void CGameGlobalObjects::AddPaksAndFactories() {}

// Retail 0x800070FC, 0x6C = 108 bytes. The first call arms `lbl_80418ED4` and every call after it
// returns immediately, so the counter below only ever runs once per load; the `extsb.`/`bne` pair
// is that test and the `stb r0(=1)` is the arm. `cntlzw`/`srwi r4,5` is `counter == 0`.
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

// Retail 0x80006B38, 0x48 = 72 bytes, one line. The resource name is **+0x7C into
// `lbl_803A56C0`**, not a literal of ours own: retail reaches it with
// `lis r4, lbl_803A56C0@ha / addi r4,r4, lbl_803A56C0@l / addi r4,r4, 0x7c`, and a literal comes
// out as three instructions naming mwcceppc's `@stringBase0` instead. That is the whole
// difference between 99.94% and 100%.
void CMain::FillInAssetIDs() {
  gpSimplePool->fn_8029c7e8(*gpResourceFactory->GetResourceIdByName(lbl_803A56C0 + 0x07C));
}

// Retail 0x80005C64, 0x8 = 8 bytes: `stw r4, 0x48(r3) ; blr`. The only writer of
// `frameTimeMinimum` other than `CMain::AsyncIdle`, which clamps against it and clears it.
// Declared in `include/MetroidPrime/CMain.hpp` and **not** inline: nothing in the port calls
// it, so an inline body is never emitted and the function stayed at 0% in the matching build.
// Placed immediately before `CMain::RsMain` because 0x80005C64 is retail's order between
// `CMain::AsyncIdle` (0x80005B44) and `CMain::RsMain` (0x80005C6C).
void CMain::SetFrameTimeMinimum(int time) { frameTimeMinimum = time; }

int CMain::RsMain(int argc, const char* const* argv) {}

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

// `__pl__4rstlFRCQ24rstl66basic_string<c,...>RCQ24rstl66basic_string<c,...>`
//   = .text:0x80005AE8, 0x5C = 92 bytes, weak.
//
// The Itanium mangling of `operator+` is `pl`, so this is `rstl::operator+(const string&,
// const string&)` - declared at `include/rstl/string.hpp:369` and never defined by any unit
// `configure.py` claims, which is why the symbol is `U` here and the 92 bytes stay retail's.
// Metroid Prime 1's `src/MetroidPrime/main.cpp` defines exactly this function in exactly this
// place (immediately above `CMain::AddWorldPaks`, its only caller in the object), and its body
// is the four calls retail makes: copy-construct the first operand onto the frame, append the
// second, copy-construct the result into the return slot, then destroy the temporary.
//
// The body is **identical** to the one `src/MetroidPrime/PortGlobals.cpp:883` already carries
// for the PC link, so this is not a second implementation of anything: `PortGlobals.cpp` is
// deliberately not a `configure.py` unit (its header explains why - a definition in a claimed
// unit would collide with the retail object), and `src/MetroidPrime/main.cpp` is not in the
// port's `files.cmake` either (the port links `mainHead`/`mainMid`/`mainTail`), so the two never
// meet in a link.
namespace rstl {
string operator+(const string& a, const string& b) {
  string result(a);
  result.append(b);
  return result;
}
} // namespace rstl

// Retail 0x800057A8, 0x180 = 384 bytes, 96.00% here. Prime 1's `CMain::AddWorldPaks` is this
// function with the loop count changed (9 there, 16 here - retail's own `cmpwi r29,16` at
// 0x800056C0) and `GetWorldPrefix` renamed to `GetPakFile`.
//
// **Three measured differences are left, and two of the three obvious fixes make it worse.**
// Recorded here so the next attempt does not repeat them (all three were tried, 2026-09-30):
//
//  1. **`rstl::rmemory_allocator allocator;` and naming the pool literals are both right and both
//     cost 13.5 points** (96.00% -> 82.44%). Retail does pass `r1+8` as `rmemory_allocator const&`
//     at 0x8000563C and does reach `.pak` and `%d` through `lbl_803A56C0`, but mwcceppc allocates
//     the frame from the *tallest* local it sees, so naming them changes every spill offset at
//     once: all 16 `r1+N` displacements move together and none of them lands where retail has it.
//     The two changes are individually correct and jointly wrong.
//  2. **`GetPakFile` returning by value instead of by const reference** is likewise required by
//     the measured bytes (retail 0x80216D5C is a bare copy-constructor into the caller's sret
//     slot, and `CMain::AddWorldPaks` sets `addi r3,r1,92` before the call and calls
//     `internal_dereference` on `r1+92` after). Same frame-size consequence. Changing the return
//     type of `CTweakGame::GetPakFile` is a header edit affecting `CGameState.cpp`,
//     `CPlayerState.cpp`, `CScriptPickup.cpp` and `Tweaks.cpp` as well, so it wants its own
//     item, not a rider on this one.
//  3. What is left after (1) and (2) is the frame size itself: 0xA0 against our 0x90, i.e. one
//     16-byte `rstl::string` temporary that retail has and we do not.
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

// Retail 0x800090A8, 0x7C = 124 bytes, and the whole body is what the compiler generates for
// the class's four members plus the deleting-destructor tail: the two `rstl::vector`s and the two
// `rstl::bit_vector`s are destroyed in **reverse declaration order** (the vectors at +0x38 and
// +0x28, the bit_vectors at +0x14 and +0x00), each with `li r4,-1` so the member destructors run
// their bodies and skip their own `operator delete`, and the object itself is released by
// `CMemory::Free` when the incoming flag is positive. `rstl::vector<rstl::pair<TEditorId, bool>>`
// is 0x14 bytes and `rstl::bit_vector<>` is 0x14 bytes, so +0x38/+0x28/+0x14/+0x00 and the 0x4C
// object size the header's `CHECK_SIZEOF` pins both fall out of the header's member order.
//
// **The class had no destructor before this**, so every `rstl::rc_ptr<CMapWorldInfo>` holder inlined
// the four member teardowns. Retail keeps one out-of-line copy of the deleting destructor, and the
// member types' own destructors are declared rather than defined (`rstl::bit_vector`'s is implicit
// but still out of line), so each stays a `bl` here instead of expanding.
CMapWorldInfo::~CMapWorldInfo() {}

void CMain::StreamNewGameState(bool) {
  // TODO
  gameGlobalObjects->GameState() = nullptr;
  gpGameState = nullptr;
  gameGlobalObjects->GameState() = new CGameState();
  gpGameState = gameGlobalObjects->GameState().get();
  // gpGameState->HintOptions().SetHintNextTime();
}

CPlayerState::~CPlayerState() {}

CPlayerState::SPersistentState::~SPersistentState() {}

CStaticInterference::~CStaticInterference() {}
