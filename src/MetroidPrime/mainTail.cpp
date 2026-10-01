/**
 * `MetroidPrime/main.cpp`'s upper half - retail `.text:0x80008680-0x80009880`, 0x1200 = 4,608
 * bytes - plus `.ctors:0x803A54A4-0x803A54A8` and `.sbss:0x80418EA0-0x80418EC4`. **This unit
 * exists only because `CGameGlobalObjects`'s constructor had to become a unit of its own, and
 * then because `CMain::ShutdownSubsystems` had to become one too.**
 *
 * Retail's constructor is 0x8000848C-0x80008570, inside what was `main.cpp`'s single claimed
 * range, and a `configure.py` unit may claim only one range per section, so the range is cut in
 * three: `main.cpp` keeps 0x800053B8-0x8000848C, `MetroidPrime/CGameGlobalObjectsCtor.cpp` takes
 * the 0xE4 bytes in the middle and is `Matching`, and this file takes everything above it.
 *
 * The lower end moved once more since: `CMain::ShutdownSubsystems` (0x80008570, 272 B) is now
 * `src/MetroidPrime/CMainShutdownSubsystems.cpp`, a `Matching` unit, so this file's `.text`
 * starts at 0x80008680. That is the *same* one-range-per-section rule applied to the next
 * function up the address order: a `Matching` carve for `CMain::InitializeSubsystems`
 * (0x80008680, 348 B) needs 0x80008570..0x800087DC, which is this file's first 0x26C bytes, and
 * the 0x110 in front of it would have been a second discontiguous range in one unit. Moving those
 * 272 bytes out is free - it was the worst-scoring function in the range at 1.47%, so nothing
 * leaves `matched` - and it is what makes the `InitializeSubsystems` carve possible at all.
 *
 * ## Why the source moved with the range, and why the cut could not be anywhere else
 *
 * *The source moves*, because objdiff pairs a unit's functions by **name** against **that
 * unit's** retail functions, and a function this object emits that the unit does not claim is
 * unpaired and reads 0.00%. Fifteen functions in the upper range are at 100% in
 * `main/MetroidPrime/main` - `__sys_free`, `CMain::~CMain`, `InvokeCMain`, `SetThirtyFps`,
 * `GetMaxSpeed`, `~CPlayerState`, `~CPlayerState::SPersistentState`, `~CStaticInterference` and
 * six `rstl` template destructors - and leaving their bodies in `main.cpp` would drop fifteen off
 * the project's `matched` total to gain one.
 *
 * *The cut could not be anywhere else*, and that is a `dtk` constraint rather than a choice.
 * `dtk dol split` walks the configured units in order and requires each one's claimed addresses
 * to be non-decreasing, so the three units have to appear in address order, and
 * `main.cpp` referenced the constructor (`CMain::RsMain`'s `bl` at 0x80005CE4) so it has to come
 * first. The constructor is therefore the middle range, and this unit's `.text` has to begin
 * immediately after it at 0x80008570. `.ctors` and `.sbss` have to be on the **last** unit in
 * the list for the same reason - with them on `main.cpp`, dtk fails with "Mismatched splits for
 * .ctors 4:0x803A54A4 (MetroidPrime/main.cpp) and function 3:0x80009864" - so they are here.
 * Since this unit is `NonMatching`, `dtk` supplies retail's bytes for both and the link is
 * unchanged; the definitions themselves stay in `main.cpp`, which is where the **port** wants
 * them.
 *
 * A three-unit cut at 0x80007A14 instead - putting `CGameArchitectureSupport`'s constructor,
 * destructor and `UpdateTicks` here as well - recovers three of the four weak template copies
 * named below and is not done, because it is the same problem one range lower.
 *
 * ## `inline_max_size(125)`
 *
 * The same `-pragma "inline_max_size(125)"` that `main.cpp` carries. It is not decoration: it is
 * what decides whether `MakeMsg::CreateFrameEnd`'s inlined `rc_ptr` copy and the `rstl` template
 * destructors are inlined, and retail's bytes assume retail's setting.
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CFactoryFunctions.hpp"
#include "dolphin/ar.h"
#include "dolphin/arq.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSThread.h"

// `<stdint.h>` is what `uintptr_t` comes from, and `CMain::InitializeSubsystems` below casts
// through it four times. It arrived in `main.cpp` through an include this file does not need
// (`MetroidPrime/CMain.hpp` does not reach it, and neither does `dolphin/types.h`, which guards
// its own `<stdint.h>` behind `TARGET_PC` - the host-only typedefs), so it is named directly.
#include <stdint.h>

#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CInGameTweakManager.hpp"
#include "MetroidPrime/CMainFlow.hpp"
#include "MetroidPrime/CIOWinManager.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Player/CStaticInterference.hpp"

#include <stdio.h>

static uchar sMainSpace[sizeof(CMain)];

extern "C" void __sys_free(const void* ptr) { CMemory::Free(ptr); }

bool CMain::GetMaxSpeed() { return mMaxSpeed; }

void CMain::SetMaxSpeed(bool v) {
  if (v && !mMaxSpeed) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  mMaxSpeedDrawTimer = 0.0f;
  mMaxSpeed = v;
}

void CMain::SetThirtyFps(bool drawn) { mThirtyFps = drawn; }

CMain::CMain(COsContext* context, CSaveRegion* saveRegion, CMemorySys* memorySys,
             CDvdRequestSys* dvdRequestSys)
: mOsContext(context)
, mSaveRegion(saveRegion)
, mMemorySys(memorySys)
, mDvdRequestSys(dvdRequestSys)
// , xe8_(0.0)
// , x118_(0.f)
// , x11c_(0.f)
// , x120_(0.f)
// , x124_(0.f)
, mFrameTimeMinimum(0)
, mSoftResetHoldTime(0.0f)
, mGameGlobalObjects(nullptr)
, mRestartMode(kRM_Default)  // value must be 6, TODO if the correct enum
, mMaxSpeedDrawTimer(1.0f)
, mFrameTimes(0xF4240)
, mFrameTimeIdx(0)
, mFinished(false)
, mMfGameBuilt(false)
, mMaxSpeed(false)
, mResetButtonHeld(false)
, mManageCard(false)
, mResetRequested(false)
, mGameExitReset(false)
, mGameFrameDrawn(false)
{
  gpMain = this;
}

extern "C" void InvokeCMain(int argc, char** argv, COsContext* context,
                            CSaveRegion* saveRegion, CMemorySys* memorySys,
                            CDvdRequestSys* dvdRequestSys) {
  CMain* main = new (&sMainSpace) CMain(context, saveRegion, memorySys, dvdRequestSys);
  main->RsMain(argc, argv);
  main->~CMain();
}

CMain::~CMain() {}

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

// The two printf formats are retail's, at 0x803A5847 and 0x803A585D in .rodata. They are
// **not** literals in this unit and not named buffers either: retail's own object reaches them
// as `lbl_803A56C0 + 0x187` and `lbl_803A56C0 + 0x19D` - `lis r4,0` / `R_PPC_ADDR16_HA
// lbl_803A56C0` / `addi r3,r4,0` / `R_PPC_ADDR16_LO lbl_803A56C0` / `addi r3,r3,391` at
// 0x800086C8-0x800086D4, and the same shape with 413 at 0x80008780-0x80008790. `lbl_803A56C0`
// is `.rodata` 0x803A56C0, 0x1C0 bytes, and it is retail's string pool: the two formats are
// 0x187 and 0x19D into it, and `CGameGlobalObjects::AddPaksAndFactories` in main.cpp reaches its
// pak names at the same base. Declaring the pool and indexing it reproduces the four
// instructions; a literal or a file-scope buffer of our own emits three and relocates against
// MWCC's own `@stringBase0` pool instead. See main.cpp's comment on the same symbol for what that
// costs and why the linked bytes are identical either way.
extern "C" const char lbl_803A56C0[];

// `ARInit`'s first argument. Retail loads it as a **relocation against a 12-byte .bss object**
// (`lis r3,0` / `R_PPC_ADDR16_HA lbl_803C5AB8` / `addi r3,r3,0` / `R_PPC_ADDR16_LO
// lbl_803C5AB8` at 0x80008688/0x80008694), not as the literal `(u32*)0x803c5ab8` this file used,
// which mwcceppc turns into `lis r3,-32708 ; addi r3,r3,23224` - the same value, two
// instructions, and no relocation. `lbl_803C5AB8` is `.bss` 0x803C5AB8, 0xC bytes, and nothing
// else in the DOL names it; the guest address is Aurora's own `AR_StackPointer` slot region.
extern "C" char lbl_803C5AB8[];

// The 8 KB of stack-guard fill. Retail stores one word, **0x7337D00D**, per 4 bytes
// (`lis r5,29496` / `addi r5,r5,-12275` at 0x80008710 / 0x8000871C, which is
// 0x7338_0000 + 0xFFFFD00D = 0x7337D00D - the `addi` immediate is a *negative* 0x2FF3, so the
// sum borrows out of the 0x7338 half and the value is one 0x10000 lower than the `lis`
// literal suggests). It is not a `memset`: the four bytes of that word are 0D D0 37 73, not
// one repeated value, so the source is a `uint` fill loop and MWCC compiled it as a word loop
// unrolled eight-wide with a remainder, which is the shape at 0x80008728-0x80008770.
//
// **This constant was wrong in three places in this tree and is now measured, not guessed.**
// `docs/research/boot_path.md` step 11 says 0x7338D0D0 (a transposition), this file's comment
// said 0x7338D00D (the same transposition with the nibbles in order), and the value in the
// source was 0x7338D00D - 0x10000 too high, which is why the `lis` immediate was 29497 where
// retail's is 29496. `CMain::ShutdownSubsystems` (0x80008570) confirms it independently: its
// stack scan at 0x80008638 is `addis r0,r3,-29495 ; cmplwi r0,53261`, which is
// `word + 0x8CC90000 == 0xD00D`, i.e. `word == 0x7337D00D`. Two instructions 0x2EC bytes
// apart, both agreeing.
static const uint kStackGuardWord = 0x7337D00D;

// The five calls between the two `printf`s and `CFrameDelayedKiller::Initialize`, in retail's
// order, with the arguments retail passes to the third. **None of the five has a body in this
// tree and none is defined in the port.** They cost the port's link *nothing*, though, and the
// reason is worth writing down because the opposite is what the comment this replaces said:
// `TARGET_PC` is `PUBLIC` on `mp_game` (CMakeLists.txt:162), so the port build never compiles the
// retail body below - it compiles `PortInitializeSubsystems` instead, and these declarations go
// unused. For the *matching* build they are exactly what a `Matching` unit needs: relocations
// against retail's own objects, which `dtk dol split` supplies for every range.
// `docs/research/boot_path.md` step 11's "calling them would add five symbols to the link gap and
// close none" was right about a PC link and was being used to justify never writing them, which
// cost this function 20 bytes of a 348-byte Matching candidate. The j1 correction in
// `boot_path.md` already settles the principle: a callee's body is not a precondition for a
// `Matching` unit.
extern "C" void fn_802DAE30();
extern "C" void fn_8002ADC8();
extern "C" void fn_80301CC4(uint, uint, uint);
extern "C" void fn_800E85A8();
extern "C" void fn_800DC0B0();

#ifdef TARGET_PC
// Host: see the block comment below. Declared in PortBoot.cpp.
void PortInitializeSubsystems();
#else
void CMain::InitializeSubsystems() {
  ARInit((u32*)lbl_803C5AB8, 3);
  lbl_80418BA8 = ARAlloc(lbl_80418EA0) + lbl_80418BA8;
  ARQInit();

  OSThread* thread = OSGetCurrentThread();
  printf(lbl_803A56C0 + 0x187);
  uint* guardEnd = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023);
  uint* stackBase = (uint*)thread->stackBase;
  OSProtectRange(3, guardEnd, 1024, 0);
  uint* fillStart = (uint*)((uintptr_t)stackBase - 0x2000);
  for (uint* p = guardEnd + 0x400 / 4; p < fillStart; ++p) {
    *p = kStackGuardWord;
  }
  DCFlushRange(guardEnd + 0x400 / 4, (uint)((uintptr_t)fillStart - (uintptr_t)(guardEnd + 0x400 / 4)));
  printf(lbl_803A56C0 + 0x19D, (unsigned)(uintptr_t)thread->stackBase, (unsigned)(uintptr_t)thread->stackEnd);
  fn_802DAE30();
  fn_8002ADC8();
  fn_80301CC4(2048, 0x600000, 4096);
  fn_800E85A8();
  fn_800DC0B0();
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
// translation unit configure.py never claims - the same arrangement as CMain::RsMain in
// PortBoot.cpp. mwcceppc does not define TARGET_PC, so this guard costs the matching
// build nothing: the object it compiles is byte for byte the retail body.

// `CMain::ShutdownSubsystems` (0x80008570, 0x110 = 272 bytes) used to be here and is now
// `src/MetroidPrime/CMainShutdownSubsystems.cpp`, a `Matching` unit of its own. A unit may not
// claim two discontiguous ranges in one section, and a `Matching` carve for
// `CMain::InitializeSubsystems` (0x80008680) needs the whole 0x80008570..0x800087DC - so this
// function has to move out before that carve is possible. It is the only function this split
// moves, and it was the lowest-scoring one in the range at 1.47%.

CPlayerState::~CPlayerState() {}

CPlayerState::SPersistentState::~SPersistentState() {}

CStaticInterference::~CStaticInterference() {}
// **Four weak out-of-line copies, and why this unit names their types on purpose.**
// `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` (0x80008FA4),
// `ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv` (0x80008F40),
// `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (0x80008A78) and
// `__dt__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>Fv` (0x80009634) are
// all at 100% in `main/MetroidPrime/main`, and all four are emitted by mwcceppc into the
// translation unit that *uses* them: `ioWinMgr.AddIOWin(new CMainFlow(), ...)` and
// `MakeMsg::CreateFrameEnd` in `CGameArchitectureSupport`'s constructor and destructor, and
// `Update`'s `archQueue.Push`. Those are at 0x80007A14-0x8000821C, in `main.cpp`'s range, so
// they are emitted there - and **this** unit is what claims their bytes, so an emitted copy in
// `main.cpp` pairs with nothing.
//
// What makes mwcceppc emit them here is **a namespace-scope object whose implicit constructor and
// destructor it has to generate**: those two are emitted, and they are what instantiate the two
// `ReleaseData` bodies and the list destructor. Inline member functions that nothing calls emit
// nothing - an earlier version of this struct carried `Touch()` and an `AddIOWin` call for the
// purpose, `nm` showed neither in the object, and deleting both left the unit at 15/48. This is
// a measurement aid and nothing else: `SForceTailWeakCopies` is not a retail type, this unit is
// `NonMatching` so its object is not in the DOL link, and the copies are `W` (COMDAT weak) and
// discarded by mwldeppc if anything else emits them. Do not read it as progress.
namespace {
struct SForceTailWeakCopies {
  rstl::rc_ptr< CIOWin > a;
  rstl::rc_ptr< IArchitectureMessageParm > b;
  rstl::list< CArchitectureMessage, rstl::rmemory_allocator > d;
  CIOWinManager* mgr;

  // `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (0x80008A78, 44 bytes). The only caller
  // in the DOL is `~CGameArchitectureSupport` (`bl` at 0x80007E28), which is at 0x80007DE8 in
  // `main.cpp`'s range and **stays there**: moving the destructor here, which is what the first
  // cut of this file did, took it from 95.27% to 0.00%, because this unit does not claim
  // 0x80007DE8 and its copy paired with nothing. The call has to be in a function MWCC actually
  // emits, and this destructor is emitted because `gForceTailWeakCopies` needs it. It is retail's body either way.
  ~SForceTailWeakCopies() { TOneStatic< CGameArchitectureSupport >::operator delete(mgr); }
};
SForceTailWeakCopies gForceTailWeakCopies;
} // namespace

