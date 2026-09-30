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
#include "MetroidPrime/CWorldLayerState.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
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
// Retail `.sdata2`, all three read by `CMain::CMain` and by nothing else in this range:
// `lbl_8041A3D8` = **0.0f**, `lbl_8041A3DC` = **1.0f**, `lbl_8041A3F0` = **0.0 as a double**
// (`data:double`, 8 bytes at 0x8041A3F0). Retail loads `f1` from `lbl_8041A3D8` once and stores it
// four times - `stfs f1,0x40/0x44/0x4C/0x50` - and `f0` from `lbl_8041A3DC` once for
// `stfs f0,0x5C` (0x800088B8-0x800088D4). Declared, never defined: `0.0f`/`1.0f` spelled as
// literals come out as relocations against our own `@N` pool entries, and objdiff cannot tell
// those agree with retail's. All three resolve from
// `build/G2ME01/obj/auto_11_8041A3C0_sdata2.o` at DOL link time.
extern const float lbl_8041A3D8;
extern const float lbl_8041A3DC;
extern const double lbl_8041A3F0;
// Retail `.sdata` 0x80417D84, `data:4byte`, **0x000F4240** - retail's spelling of the value
// `rstl::reserved_vector<uint, 10>`'s one-argument constructor fills its ten slots with, which
// the constructor reaches as ten `lwz r0,lbl_80417D84@sda21 ; stw r0,0x64+N*4(r3)` pairs
// (0x800088F4-0x80008924). The literal `0xF4240` is the same number and came out as ten loads of
// our own `@634`, which objdiff cannot tell agree with retail's.
extern const uint lbl_80417D84;
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

// Retail 0x800089BC, 0x60 = 96 bytes. Three things, and the header's sketch
// (`{x160_26_screenFading = v;}`) was only one of them - the offset it names, 0x26, is not
// `CMain`'s at all, and the bit it does write is bit 2 of the byte at +0x90.
//
//  * `clrlwi. r0,r4,24 ; beq` is the test of the `bool` argument, and
//    `lbz r0,0x90(r30) ; rlwinm. r0,r0,27,31,31 ; bne` is **mask 31** of that byte. Mask 31 is
//    field 7 counted from the byte's first bit, i.e. `screenFading` again (see the `rlwinm`
//    arithmetic at `CMain::fn_80008A1C` above). So the guard is "switching max speed on while the
//    screen is not already fading", and the store below sets the same member.
//  * `lfs f0,lbl_8041A3DC ; stfs f0,0x5c(r30)` resets `x5c` to 1.0f - the **same** `.sdata2`
//    symbol the constructor loads it from, not a literal.
//  * `rlwimi r0,r31,5,26,26` is `SH=5`, i.e. field 2 counted down from `finished`, which is
//    `screenFading`. `r31` is the argument, so the store is after the call and the allocator
//    has to keep `v` live across it.
//
// Declared in `include/MetroidPrime/CMain.hpp` since before this, with no definition anywhere,
// so our object did not define the symbol at all and the 96 bytes read 0.00%.
//
// **`const` on the parameter is load-bearing and is the whole 99.25%.** The prologue and all
// twenty body instructions are already byte-identical to retail; the three epilogue reloads were
// not, and that is what the percentage was:
//
//   retail  80008a04: lwz r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1) ; mtlr r0
//   ours    00002180: lwz r31,12(r1); lwz r30,8(r1) ; lwz r0,20(r1) ; mtlr r0
//
// A void function with no `mr r3,rN` in its epilogue leaves the order of the three reloads free,
// and mwcceppc's tie-break depends on how the argument is treated. `const bool v` puts the
// argument in the same "named, never written" class as retail's and the order comes out right.
// Measured on this unit, twelve spellings, all with the other 88 bytes unchanged
// (`.tmp/opencode/sms/h3.py` re-runs them; only the diff count against the DOL's 96 bytes is
// shown, with the two relocation fields masked):
//
//   `const bool v`                                    0 diff bytes   <- this
//   `const bool fading = v;` before the bitfield write 0 diff bytes   (same effect, one more name)
//   `bool v`                                           8
//   `bool v` + `CMain* const self = this;`            8
//   `bool v` + `!!v`                                   8
//   `bool v` + `screenFading == 0`                     8
//   `bool v` + `const float one = lbl_8041A3DC;`       8
//   `bool arg` (renamed parameter)                     8
//   `bool v` + `(!screenFading)`                       8
//   `bool v` + `if (v) { if (screenFading) {} else {} }` 8
//   `bool v` + `screenFading = (v != 0)`              wrong size (108 B)
//   `bool v` + `if (!(v && !screenFading)) .. else ..` wrong size (104 B)
//
// The top-level `const` is not part of the signature, so `CMain.hpp`'s `void SetMaxSpeed(bool)`
// declaration still matches and is left alone; `src/MetroidPrime/mainTail.cpp` is the port's
// copy of this function and is not a DOL unit, so nothing else defines it.
void CMain::SetMaxSpeed(const bool v) {
  if (v && !screenFading) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  x5c = lbl_8041A3DC;
  screenFading = v;
}

// Retail 0x80008898, 0x114 = 276 bytes. Nineteen of its twenty stores are the member
// initialiser list in declaration order; the other two are the `gpMain = this` epilogue. Every
// constant is retail's own `.sdata2` symbol rather than a literal (see the declarations above) -
// `lbl_8041A3D8` is loaded once into `f1` and stored four times, `lbl_8041A3DC` once into `f0`,
// `lbl_8041A3F0` once into `f2` - which is what makes the register allocation come out at all.
CMain::CMain(COsContext* context, void* unk1, CMemorySys* memorySys, void* unk2)
: osContext(context)
, mUnk1(unk1)
, memorySys(memorySys)
, mUnk2(unk2)
, x10_unk(lbl_8041A3F0)
, updateFrameTimeHistory()
, drawFrameTimeHistory()
, mAverageTickTime(lbl_8041A3D8)
, mAverageDrawTime(lbl_8041A3D8)
// **`frameTimeMinimum` (+0x48) is deliberately absent**: retail's constructor has no store at
// +0x48 at all, so it is left for `CMain::SetFrameTimeMinimum` (0x80005C64, the only writer) and
// `CMain::AsyncIdle` (which reads it, then clears it). Naming it here costs one instruction the
// retail object does not have.
, x4c(lbl_8041A3D8)
, x50(lbl_8041A3D8)
, gameGlobalObjects(nullptr)
, restartMode(kRM_Default)
, x5c(lbl_8041A3DC)
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
, mGameArchitectureSupport(nullptr)
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
// load; the one writer is `fn_80009864`, which computes it as `*(u32*)lbl_8041EE00 * 14`.
// (`lbl_8041EE00` is 0x00008F00, so what lands here is 0x7D000 = 512512. The `0x80415980` this
// comment used to name is in `.rodata` and is not what the load reaches - the measurement is on
// `fn_80009864` at the end of this file.) **Read
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
  // **`GetGameFrameDrawn()`, not `GetFinished()`**, and the two are one `lbz` apart. Retail
  // 0x80007C64 is `lwz r3,gpMain ; lbz r0,145(r3) ; rlwinm. r0,r0,25,31,31` - it loads **+0x91**,
  // where `GetFinished()` (the first of the eight `bool : 1` at +0x90) makes us emit `144(r3)`.
  //
  // **The `rlwinm 25,31,31` is the same opcode in both cases and does not tell the two apart.**
  // `rlwinm rA,rS,25,31,31` tests bit `31-25`=6 of whatever `rS` holds, but the probe in
  // `docs/goal-notes/match-main-cmain-0x91-bitfield.md` shows mwcceppc emits exactly this pair
  // for *both* the first and the ninth one-bit field:
  //
  //     bool b0 : 1;  ->  lbz r0,0(r3) ; rlwinm r3,r0,25,31,31
  //     bool b8 : 1;  ->  lbz r0,1(r3) ; rlwinm r3,r0,25,31,31
  //
  // so "bit 6 of the byte" is not a distinct field - it is the *first* field of whichever byte
  // was loaded, and only the displacement separates +0x90 from +0x91. Reading the rotate mask
  // as a bit index (which is what this comment's predecessor did, and what the goal item
  // `match-main-cmain-0x91-bitfield` was filed on) invents a ninth/other field that retail's
  // constructor never writes. `CMain`'s bitfield map needs no change: the constructor at
  // 0x80008940-0x8000899C writes exactly the eight fields at +0x90 and then `stw r8,148(r3)`,
  // and `gameFrameDrawn` is the ninth, which is why `SetGameFrameDrawn` is the ninth too.
  if (gpMain->GetGameFrameDrawn()) {
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
  // **Declared before the `Push`, and that is load-bearing.** Retail 0x80007CB4 is
  // `li r28,1` and it comes *before* `bl CreateFrameBegin` at 0x80007CBC, not after the
  // `Push` that follows it. Spelled after the call, `bool keepLooping = true;` is
  // materialised at 0x80007CDC instead and the whole tail of the function shifts by one
  // instruction: 0x80007CD8's `lfs f31` lands after the `addi r29,r1,16` instead of before
  // it, and the `bl` targets walk one slot out of step for the rest of the body.
  bool keepLooping = true;
  archQueue.Push(MakeMsg::CreateFrameBegin(kAMT_Game, gameFrameCount));

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

// The `{count, records}` pair and the 16-byte record `fn_800070A4` below copies. Together they are
// `CGameState`'s `+0x1A0` block, whose extent (0x54) `include/MetroidPrime/Player/
// CGameStateBlocks.hpp:103-111` already measures and whose record array it names `x14_rec[4][16]`
// as a **view** onto `CGameState`'s own `char x1a4_[0x50]` - so these two are written against
// their own type rather than against the view. Every offset here is retail's:
//   * `fn_800070A4` (0x800070A4, 0x50) reads `0/4/8` as words and `12/13` as bytes out of the
//     source and writes the same five fields back at the loop's `addi r10,r10,16` stride, so the
//     record is `{u32,u32,u32,u8,u8}` with two bytes of tail padding;
//   * `fn_80007040` (0x80007040) zeroes the four words/bytes ahead of the count and then asks for
//     four records, so `+0x00` is the count and `+0x04` is where the first record sits.
// Both were `{}` bodies until now, so retail's 80 and 100 bytes read 5.00% and 4.00%.
struct SGameStateRecord {
  u32 x00;
  u32 x04;
  u32 x08;
  u8 x0c;
  u8 x0d;
};
CHECK_SIZEOF(SGameStateRecord, 0x10)

struct SGameStateRecords {
  u32 x00_count;
  SGameStateRecord x04_recs[4];
};
CHECK_SIZEOF(SGameStateRecords, 0x44)

// Retail 0x800070A4, 0x50 = 80 bytes. The count is stored **before** the copy loop rather than
// after it, so it is a member write and not the loop's induction variable; `mtctr r4 / cmpwi
// r4,0 / blelr` is mwcceppc's strength reduction of the counted loop, which is why the guard is
// a `blelr` and not a branch around the body.
//
// The records are **inline at +0x04**, not behind a pointer: the cursor starts at `addi
// r10,r3,4` and steps by 16, and `cmplwi r10,0 / beq` tests that cursor - not a loaded word - so
// the test mwcceppc emits is on the address of the array itself. Both halves of that are
// load-bearing and both were measured: writing the member as a pointer makes the word reload
// inside the loop and the copy loop unroll to 336 bytes, and writing the test out gives one
// straight unrolled copy loop with no `cmplwi` at all.
extern "C" void fn_800070A4(SGameStateRecords* self, int n, const SGameStateRecord& value) {
  self->x00_count = n;
  SGameStateRecord* rec = self->x04_recs;
  for (int i = 0; i < n; ++i) {
    if (rec) {
      *rec = value;
    }
    ++rec;
  }
}

// Retail 0x80007040, 0x64 = 100 bytes. Returns `this`: the `mr r3,r31` between the `lwz r0,20(r1)`
// and the restores is the same return-this tail `fn_80144924` (0x80144924, `SGameStateSlots`'s
// constructor) has. The four zero stores are in the struct's declaration order (+0x00, +0x04,
// +0x08, +0x0C) and the temporary is the same 14-byte record `fn_800070A4` copies, zeroed with
// three word stores and two byte stores rather than a block clear.
extern "C" SGameStateWorlds* fn_80007040(SGameStateWorlds* self) {
  self->x00 = 0;
  self->x04 = 0;
  self->x08 = 0;
  self->x0c = 0;
  SGameStateRecord value;
  value.x00 = 0;
  value.x04 = 0;
  value.x08 = 0;
  value.x0c = 0;
  value.x0d = 0;
  fn_800070A4(reinterpret_cast< SGameStateRecords* >(&self->x10_count), 4, value);
  return self;
}

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

// Retail 0x80005C6C, 0x868 = 2152 bytes, **0.19% here** - the function is unwritten apart from
// the two allocations below, and this is the first of them.
//
// **Why this one function carries the item's other four matches.** `TOneStatic<T>`'s
// `operator new` and `GetAllocSpace` are template member functions: nothing but an allocation
// site brings them into a translation unit. Retail has exactly two allocation sites for the two
// `TOneStatic` classes, and they are both in this function:
//
//   0x80005CC0  lis r4,.. / li r3,356 / addi r4,r4,.. / li r5,0 / bl 0x80008AD4
//               mr. r0,r3 / beq / lwz r4,0(r31) / lwz r5,8(r31) / bl 0x8000848C   <- CGameGlobalObjects
//   0x80005E08  lis r4,.. / li r3,168 / addi r4,r4,.. / li r5,0 / bl 0x80008A48
//               mr. r0,r3 / beq / lwz r4,0(r31)                 / bl 0x80007EC4   <- CGameArchitectureSupport
//
// `li r3,356` and `li r3,168` are `sizeof` retail passes, and 0x8000848C / 0x80007EC4 are
// `CGameGlobalObjects::CGameGlobalObjects` and `CGameArchitectureSupport::CGameArchitectureSupport`,
// the two constructors this file already defines. So the allocation is spelled
// `TOneStatic<T>::operator new(sizeof(T), <file>, 0)` with an explicit `sizeof` and explicit
// arguments, because an ordinary `new T` would select the one-argument overload
// (`include/Kyoto/TOneStatic.hpp:34`) and retail has no such body in this range.
//
// **The arguments retail passes are not a filename.** `addi r4,r4,22208` after
// `lis r4,-32710` gives 0x802A56C0, which is inside `.text` (`readelf -S`: 0x80003840 +
// 0x3A1C54) and holds `li r4,0x1924 ; li r28,0x100`, and the same value is passed to both
// allocation sites, so it is not the class name either. `operator new` ignores both arguments -
// 0x80008AD4 reads only r3 - so they are passed as null here rather than given a value this
// measurement does not identify. **Naming that is the open question for a run that writes the
// rest of the function**; it costs the two 48-byte bodies nothing either way.
//
// The construction of the object over the storage `operator new` returns is retail's next
// instruction and is **not** written here: it needs a placement `operator new`, which this tree
// does not declare. `CGameGlobalObjects`'s constructor is in this file at line 338 and
// `CGameArchitectureSupport`'s at line 401, and both are `Matching`-shaped already, so the pair is
// in place when the rest of `RsMain` is written.
int CMain::RsMain(int argc, const char* const* argv) {
  CGameGlobalObjects* gameGlobalObjects = static_cast< CGameGlobalObjects* >(
      TOneStatic< CGameGlobalObjects >::operator new(sizeof(CGameGlobalObjects), nullptr, nullptr));
  CGameArchitectureSupport* architectureSupport = static_cast< CGameArchitectureSupport* >(
      TOneStatic< CGameArchitectureSupport >::operator new(sizeof(CGameArchitectureSupport),
                                                            nullptr, nullptr));
  (void)argc;
  (void)argv;
  (void)gameGlobalObjects;
  (void)architectureSupport;
  return 0;
}

// Retail 0x80005B44, 0x120 = 288 bytes, **99.17%** (was 85.94%). The whole body is
// instruction-for-instruction retail's except the last argument setup, and the two
// decompositions below are what make it so. Both were measured; neither is a cosmetic rewrite.
//
//  1. The clamp result must be a **separate variable** from the parameter, and its `5000` arm
//     must be the *fall-through* with `time` as the branch target. Retail 0x80005BF0 is
//     `cmplwi r4,5000 / li r31,5000 / bgt / mr r31,r4`, which only the initialiser spelling
//     lays out that way: `t = (time <= 5000) ? time : 5000` gives 11 differing instructions and
//     `time = time; if (time > 5000) { time = 5000; }` gives 5. It is also what lets the
//     parameter stay in `r4` and the clamp live in `r31` across `fn_80008A1C()`'s call.
//  2. The flag must be **initialised before the test**, not assigned from it.
//     `bool flag = fn_80008A1C();` scores 18 differing instructions because mwcceppc then keeps
//     the result in a volatile and never spills, so retail's `r30` leaves the prologue and the
//     epilogue entirely; `bool flag = false;` with the assignment inside the `if` reproduces
//     retail's `li r30,0` / `li r30,1` and the `stw r30,8(r1)` spill.
//
// **The one instruction left is not reachable from the source, and this is the measurement.**
// Retail 0x80005C44 is `clrlwi r5,r30,24` where this is `mr r5,r30` - a narrowing of the `bool`
// argument to a **one-byte** type. Seventeen argument- and local-type spellings were compiled
// and measured; every one of them is worse, and the ones that narrow land on exactly the two
// scores a `uchar` conversion predicts because mwcceppc also has to normalise:
//
//   bool (retail's and ours) 99.17   uchar local 94.79   char local 94.79   uint local 96.18
//   (uchar)flag 94.79   (char)flag 94.79   (bool)(uchar)flag 94.79   flag | 0 94.79
//   flag != 0 94.79   bool flag = fn_80008A1C() != 0 87.68   uchar flag = (uchar)fn() 89.49
//
// Declaring `CResFactory::AsyncIdle`'s second parameter `unsigned char` **is** byte-exact
// (100.00%, measured) and is still wrong: it renames the callee to
// `AsyncIdle__11CResFactoryFUiUc`, and `main/Kyoto/CResFactory` is a `NonMatching` unit whose
// own `AsyncIdle__11CResFactoryFUib` body is at 100.0% (268 bytes) under the name symbols.txt
// gives it. Measured cost of taking the point: `main/Kyoto/CResFactory` 35 -> 34 functions,
// `matched_code` 5532 -> 5408, against `main/MetroidPrime/main` 62 -> 63. Net negative, and it
// leaves `FUiUc` undefined at DOL link. Retail's own mangling says the parameter is `bool`, so
// the clrlwi is mwcceppc's narrowing of an argument it already knows is 0 or 1.
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

  uint t = 5000;
  if (time <= 5000) {
    t = time;
  }
  if (t < frameTimeMinimum) {
    t = frameTimeMinimum;
  }
  frameTimeMinimum = 0;
  bool flag = false;
  if (fn_80008A1C()) {
    flag = true;
    t = 1000000;
  }

  if (t != 0) {
    gpResourceFactory->AsyncIdle(t, flag);
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

// Retail 0x80005698 (`main.o` +0x2E0), 0xD0 = 208 bytes. Every loaded **world** pak gets its name
// list **copied** (`CPakFile`+0x58, the `rstl::vector` copy constructor at +0x4C) and scanned for
// `id`. A pak that does **not** have it is told to fetch what it is missing -
// `CPakFile::sub_80323554`; one that does is told to finish loading -
// `CPakFile::EnsureWorldPakReady`. Both tails are retail's own relocations against defined
// `CPakFile` members; neither is a stand-in. The copy is destroyed with `li r4,-1` at +0xA0, and
// writing the function at all is what first pairs the vector's copy constructor and destructor in
// this tree - they were unpaired COMDATs at 0.00% because nothing instantiated them.
//
// Three spellings of the same algorithm were measured; all three differ from retail only in
// register allocation and scheduling, which mwcceppc does not normalise, so each one is load-bearing.
//  1. The scan must be written with **iterators** (100.00% against **94.13%** for the index
//     spelling). mwcceppc strength-reduces `for (int j = 0; j < names.size(); ++j)` to a **counted**
//     loop - it emits `mtctr r0 / cmpwi r0,0 / ble / ... / bdnz` - where retail's bytes are the
//     pointer form `mulli r0,r0,24 / add r3,r4,r0 / cmplw r4,r3 / bne` (stride 24 =
//     `sizeof(rstl::pair<rstl::string, SObjectTag>)`).
//  2. The flag's **initialisation has to precede the `GetPakFile` call**, so it is declared on the
//     line *above* `CPakFile& file` (100.00% against **95.58%** declared below it). Retail's
//     `li r29,1` sits at +0x2C, between the argument setup and the `bl` at +0x30. Declared below
//     it, mwcceppc sinks the `li` past the call and past `lbz r0,40(r3)`, its live range then fits
//     entirely between two calls, and it allocates the flag to a scratch register (r28) and the pak
//     pointer to r29 - retail has those the other way round. The live range, not the declaration
//     order, is what picks the register.
//  3. The flag's **polarity** is the last single instruction: `bool found = false` with the arms in
//     their natural order is **99.96%**, differing in exactly `li r29,1` against `li r29,0` and back.
//     Retail stores "not yet seen" and clears it on a match, so the flag is named `notFound` and the
//     two arms are written in that sense. The control flow is identical either way.
void CMain::EnsureWorldPakReady(CAssetId id) {
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  for (int i = 0; i < resLoader.GetPakCount(); ++i) {
    bool notFound = true;
    CPakFile& file = *resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      rstl::vector< rstl::pair< rstl::string, SObjectTag > > names = file.NameList();
      for (rstl::vector< rstl::pair< rstl::string, SObjectTag > >::iterator it = names.begin();
           it != names.end(); ++it) {
        if (it->second.id == id) {
          notFound = false;
        }
      }
      if (notFound) {
        file.sub_80323554();
      } else {
        file.EnsureWorldPakReady();
      }
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

// Retail 0x80009274, 0x84 = 132 bytes, and it is the same arrangement one class along: retail
// keeps ONE out-of-line deleting destructor for `CWorldLayerState` and every `rc_ptr` holder of it
// goes through `rstl::rc_ptr<CWorldLayerState>::ReleaseData` (0x80009224, whose only caller is
// this), so **the class had no destructor at all** and the four member teardowns were inlined at
// every use. The body is `{}` and the compiler generates all 132 bytes:
//   +0x2C `mLayerNameOffsets` (`rc_ptr<vector<int>>`) and +0x24 `mLayerNames`
//   (`rc_ptr<vector<string>>`), each a `addic. r0,off / beq` null guard then `bl ReleaseData`,
//   then +0x10 `mSaveLayers` (`rstl::bit_vector`, `li r4,-1`) and +0x00 `mAreaLayers`
//   (`rstl::vector<CWorldLayers::Area>`, `li r4,-1`), i.e. reverse declaration order read straight
//   back off the header, and the `extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free` tail.
// 0x34 is what the header's `CHECK_SIZEOF` already asserted, so the member order is unchanged -
// only the linkage is. Nothing here is invented; `CWorldState::mLayerState` is the retail holder
// (`include/MetroidPrime/Player/CWorldState.hpp:46`) and it already calls retail's out-of-line
// `ReleaseData`, so taking the destructor out of line moves no other unit's bytes.
CWorldLayerState::~CWorldLayerState() {}

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

// Retail `.sdata2` 0x8041EE00, `data:4byte`, **0x00008F00** (`config/G2ME01/symbols.txt:25502`,
// `size:0x8` - retail names the pair). Declared, never defined; the DOL's `auto_*_sdata2.o` that
// holds `.sdata2` supplies it.
extern const uint lbl_8041EE00;

// Retail 0x80009864, 0x1C = 28 bytes, seven instructions, and the only writer of
// `lbl_80418EA0` (`.sbss` 0x80418EA0) - the ARAM size `CGameArchitectureSupport`'s constructor
// hands to `CAudioSys`:
//
//   lwz   r0,-13760(r2) ; mulli r0,r0,28 ; srawi r0,r0,3 ; addze r0,r0 ; slwi r0,r0,2 ;
//   stw   r0,-28384(r13) ; blr
//
// `(v * 28) / 8 * 4` is `v * 14` arithmetically, and **that is the only spelling of it that emits
// all five of those instructions**: mwcceppc folds a bare `* 14` to one `mulli` and keeps the
// `/ 8 * 4` as a `srawi`/`addze`/`slwi` run. Five tried, diff count against the DOL's 28 bytes
// with the two relocation fields masked:
//
//   `(*(const int*)&lbl_8041EE00 * 28) / 8 * 4`               0   <- this
//   the same with the value in a `const int` local first        0   (same code)
//   `*(const int*)&lbl_8041EE00 * 14`                        16 B, wrong shape
//   `((*v * 28) / 8) << 1`                                     1 word out (slwi 1 not 2)
//   `((*(const uint*)&lbl_8041EE00 * 28) / 8) * 4`            20 B, wrong shape
//   `(v * 28) / 2 / 2 / 2 * 4`                               56 B, wrong shape
//
// **`-13760` is an r2 displacement, and r2 is not `_SDA_BASE_`.** Retail's `__init_registers`
// (0x80003464-0x80003470) loads **two** small-data bases, and they are 0x2640 apart:
//
//   3c 40 80 42  lis r2,0x8042  /  60 42 23 c0  ori r2,r2,0x23C0   ->  r2  = 0x804223C0
//   3d a0 80 41  lis r13,0x8041 /  61 ad fd 80  ori r13,r13,0xFD80 ->  r13 = 0x8041FD80
//
// 0x8041FD80 is the value `powerpc-eabi-nm` reports for `_SDA_BASE_` **and the one `tools/sda.py`
// uses**, so **`tools/sda.py` answers an r2-relative displacement with the wrong address**: it
// says `lbl_8041C7C0` for `-13760`, and `.sdata2` 0x8041C7C0 is 0x3F7D70A4 = 0.99f. The right
// answer is `0x804223C0 - 0x35C0 = 0x8041EE00`, which is also what dtk names in retail's own
// object (`build/G2ME01/obj/MetroidPrime/main.o`: `R_PPC_EMB_SDA21 lbl_8041EE00` at 0x44AC). The
// `stw` below is an **r13** displacement, where 0x8041FD80 *is* the right base, so this one
// function needs both: r2-relative addresses `.sdata2`, r13-relative addresses `.sdata`/`.sbss`.
extern "C" void fn_80009864() {
  lbl_80418EA0 = (*(const int*)&lbl_8041EE00 * 28) / 8 * 4;
}
