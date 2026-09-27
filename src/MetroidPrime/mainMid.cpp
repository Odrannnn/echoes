/**
 * `MetroidPrime/main.cpp`'s upper third - retail `.text:0x80006B80-0x8000848C`, 0x190C = 6,412
 * bytes, and the ten functions in it. **This file exists because `CMain::FillInAssetIDs` had to
 * become a unit of its own**, and a `configure.py` unit may claim only one range per section -
 * `dtk dol split` rejects a second, discontiguous one in the same section with a link-order
 * cycle, *before anything compiles*. So the range is cut in three:
 *
 *   `main.cpp`                    0x800053B8-0x80006B38  (keeps the retail globals, `NonMatching`)
 *   `CMainFillInAssetIDs.cpp`     0x80006B38-0x80006B80  (72 B, `Matching`)
 *   **`mainMid.cpp`**             0x80006B80-0x8000848C  (this file, `NonMatching`)
 *   `CGameGlobalObjectsCtor.cpp`  0x8000848C-0x80008570  (`Matching`)
 *   `CMainShutdownSubsystems.cpp` 0x80008570-0x80008680  (`Matching`)
 *   `mainTail.cpp`                0x80008680-0x80009880  plus `.ctors` and `.sbss`
 *
 * Both new boundaries are **function boundaries that are not another unit's boundary**, which is
 * the property that decides whether `dtk` has a link order to violate. 0x80006B80 is
 * `CResFactory::GetResourceIdByName`'s first byte - retail put that forwarder in the middle of
 * unrelated `CGame` code - and 0x80006B38 is `CMain::FillInAssetIDs`'s. The pre-existing
 * 0x8000848C edge *is* `CGameGlobalObjectsCtor.cpp`'s, and it is what `mainTail.cpp`'s header
 * documents failing with "Mismatched splits" when `.ctors` sat on the wrong side of it.
 * **A cut adjacent to a `Matching` unit's boundary is the case that produces the cycle.**
 *
 * ## What the cut costs, and it is measured rather than free
 *
 * mwcceppc's `@stringBase0` string pool is **per translation unit and ordered by first use in
 * emission order**, so cutting `main.cpp` in three re-orders the pool of this unit, which is the
 * one that received `CGameArchitectureSupport`'s constructor (0x80007EC4, four `operator new`
 * sites) and `CGameGlobalObjects::AddPaksAndFactories` (0x80007168, thirteen pak literals). The
 * constructor's `new` sites reference `@stringBase0 + 0` while the whole range is one unit and
 * `@stringBase0 + 0x76` once it is not, because the first string in *this* unit's emission order
 * is now one of `AddPaksAndFactories`' pak names rather than the `"??"` that
 * `CMain::StreamNewGameState` put at offset 0. **`StreamNewGameState` staying in `main.cpp` does
 * not help: the pool it seeds is a different pool from this one.** The cost is a percentage on a
 * `NonMatching` unit, and the gate's rule is that a percentage inside a `NonMatching` unit is a
 * signal, not a failure. The numbers are in `docs/HANDOFF.md`.
 *
 * ## The source order in this file is DESCENDING by address
 *
 * mwcceppc emits functions in *reverse* source order, so an ascending file is a permuted
 * `.text`: 100.00% per function and still a different DOL. The block below is a **cut and paste**
 * out of `main.cpp`, which was already descending - `PostInitialize` (0x800083E0) first,
 * `CMain::CheckReset` (0x80006BA4) last - so nothing was reordered. `CWorldState::Update`
 * (0x8015B9B0) and `CGameState::GetWorldState` (0x80142520) are outside the claim entirely and
 * travel with the block, exactly as they sat in `main.cpp`.
 *
 * ## `.ctors` and `.sbss` are not claimed here and cannot be
 *
 * `.ctors:0x803A54A4` and `.sbss:0x80418EA0` have to sit on the **last** unit in the
 * address-ordered list, which is `mainTail.cpp`. This unit is `NonMatching`, so `dtk` supplies
 * retail's bytes for the whole claim and the DOL is unchanged.
 *
 * **The retail globals stay defined in `main.cpp`** - the `extern "C"` block, the `lbl_*` objects,
 * `gpResourceFactory` and the rest. `sInfiniteLoopTime` and the three `extern "C" void fn_...()`
 * declarations come with them, and the definitions are `extern` here. `gpSimplePool`,
 * `gpResourceFactory`, `gpGameState`, `gpMain` and `gpTweakPlayerA` need no declaration at all:
 * their headers already declare them `extern`
 * (`include/Kyoto/CSimplePool.hpp:62`, `include/Kyoto/CResFactory.hpp:183`,
 * `include/MetroidPrime/CMain.hpp:158`, `include/MetroidPrime/Player/CGameState.hpp:295`,
 * `include/MetroidPrime/Tweaks/CTweakPlayer.hpp:59`). That is the arrangement `mainTail.cpp`'s
 * header already records, and it is also the one the port wants: the port builds this file too
 * (`files.cmake`) and needs the globals to resolve.
 */
#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Audio/CStreamAudioManager.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CFactoryFunctions.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Text/CStringTable.hpp"
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

// Defined in `main.cpp`, which kept the low range. See the header above.
extern float sInfiniteLoopTime;
extern CIOWinManager* lbl_80418EC4;
extern IController* lbl_80419300;
extern "C" void fn_8029EFCC();
extern "C" void fn_8033CEE8();
extern "C" void fn_8033CDA0();
IRenderer* AllocateRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys, IFactory& resFactory);

extern "C" {
// Retail `.rodata` 0x803A56C0 - the string pool. Declared, never defined, in `main.cpp` too; it
// is retail's own `.rodata` and a PC build cannot have it, so it is one undefined symbol the
// port's link gap already counts.
extern const char lbl_803A56C0[];
// Retail `.sdata2` 0x8041A420: 41200000, i.e. **10.0f**. `InfiniteLoopAlarm` below is its only
// reader in this range, and retail loads it as a relocation against this symbol.
extern const float lbl_8041A420;
}

void CGameGlobalObjects::PostInitialize(COsContext& osContext, CMemorySys& memorySys) {
  AddPaksAndFactories();
  LoadStringTable();
  printf(lbl_803A56C0 + 0x150);
  renderer = AllocateRenderer(simplePool, osContext, memorySys, resFactory);                            
  gpRender = reinterpret_cast< CCubeRenderer* >(renderer.get());
  CEnvFxManager::Initialize();
}

void CGameGlobalObjects::LoadStringTable() {
  stringTable = gpSimplePool->GetObj(lbl_803A56C0 + 0x146);
  gpStringTable = **stringTable;
}

void InfiniteLoopAlarm(OSAlarm* alarm, OSContext* context) {
  if (sInfiniteLoopTime >= lbl_8041A420) {
    OSCancelAlarm(alarm);
    rs_debugger_printf(lbl_803A56C0 + 0x133);
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
  // 0x80007F80: `addi r30,r31,68 ; stw r30,lbl_80418EC4`. Retail publishes `&ioWinMgr` into a
  // global here, between `ResetGameState` and the first `AddIOWin`, and clears it in the
  // destructor; it is the only global this constructor writes besides the tweak reads. It is
  // also why retail hoists `&ioWinMgr` into r30 and uses `mr r3,r30` for all four `AddIOWin`
  // calls, where this file recomputed `addi r3,r31,68` each time.
  lbl_80418EC4 = &ioWinMgr;
  // 0x80007FD4, two instructions after the store above and before the first `operator new`:
  // `lwz r0,52(r31) ; stw r0,0(lbl_80419300)`. 0x34 is `inputGenerator.x4_controller`'s pointer,
  // so the whole of retail's line is `GetController()` - a public accessor on `CInputGenerator`.
  lbl_80419300 = inputGenerator.GetController();
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
  // 0x80007E28: `li r0,0 ; stw r0,lbl_80418EC4`, between `RemoveAllIOWins` and `UnloadAudio`.
  // The counterpart of the store the constructor does, and the reason `UnloadAudio` is `static`:
  // retail's `bl fn_8029EF20` at 0x80007E2C has no argument setup at all.
  lbl_80418EC4 = 0;
  UnloadAudio();
  // 0x80007E30, the instruction that was the last one missing: `bl fn_8033CDA0`, immediately
  // after `UnloadAudio` and before `~CIOWinManager`. The comment below used to stand in for it.
  fn_8033CDA0();
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
  // `GetGameFrameDrawn()`, not `GetFinished()`: retail tests bit 0 of `CMain`+0x91 here
  // (`lbz r0,145(r3)` at 0x80007C40) and `finished` is bit 0 of +0x90. See the accessor.
  if (gpMain->GetGameFrameDrawn()) {
    x68_ = 0.033333335f;
  }
  bool flag = gpMain->fn_80008A1C();
  // **`stopwatchTime > 0.035f`, and both halves of that matter.** `0.035 < stopwatchTime` was
  // the spelling here and it is wrong twice over: the bare literal `0.035` is a **double**, so
  // the comparison was done in double and mwcceppc emitted `lfd f0,0(0)` where retail emits
  // `lfs f0,0(0)` (`lbl_8041A404`); and mwcceppc keeps a comparison's source operand order, so
  // the constant on the left gave `fcmpo cr0,f0,f31 ; bge` where retail has
  // `fcmpo cr0,f31,f0 ; ble` - the short-circuit of `||` branching *out* on the negated second
  // test. `docs/PROCESS_LESSONS.md`'s operand-order rule and a literal's type, on one line.
  if (flag || stopwatchTime > 0.035f) {
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

// Retail 0x80006BA4, 0x49C = 1,180 bytes, and it is **not written**: it is the reset path, and
// nothing in the port can reach it (`CMain::RsMain` returns before the frame loop, and step 17
// stops the boot first). What was here before was `bool CMain::CheckReset() {}` - a non-void
// function with no `return`, which is undefined behaviour, and it is on the frame loop's *exit*
// path, so the host loop's behaviour was undefined the moment the loop existed. It compiled only
// because `CMakeLists.txt:90` passes `-Wno-error=return-type`.
//
// The one-line fix is this `return false`, which is also the honest body: retail's 1,180 bytes
// re-read the memory card, rebuild the world state and reset the renderer, all of which need
// steps 12-13 of `docs/research/boot_path.md` and none of which can run here. `false` is the same
// answer `CMain::CheckTerminate` (0x800070F4, 8 bytes, `li r3,0 ; blr`) gives one line below, and
// it is what makes the loop's exit condition well-defined rather than accidental.
bool CMain::CheckReset() { return false; }
