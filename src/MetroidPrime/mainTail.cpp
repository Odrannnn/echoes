/**
 * `MetroidPrime/main.cpp`'s upper half - retail `.text:0x80008570-0x80009880`, 0x1310 = 4,880
 * bytes - plus `.ctors:0x803A54A4-0x803A54A8` and `.sbss:0x80418EA0-0x80418EC4`. **This unit
 * exists only because `CGameGlobalObjects`'s constructor had to become a unit of its own.**
 *
 * Retail's constructor is 0x8000848C-0x80008570, inside what was `main.cpp`'s single claimed
 * range, and a `configure.py` unit may claim only one range per section, so the range is cut in
 * three: `main.cpp` keeps 0x800053B8-0x8000848C, `MetroidPrime/CGameGlobalObjectsCtor.cpp` takes
 * the 0xE4 bytes in the middle and is `Matching`, and this file takes everything above it.
 *
 * ## Why the source moved with the range, and why the cut could not be anywhere else
 *
 * *The source moves*, because objdiff pairs a unit's functions by **name** against **that
 * unit's** retail functions, and a function this object emits that the unit does not claim is
 * unpaired and reads 0.00%. Fifteen functions in the upper range are at 100% in
 * `main/MetroidPrime/main` - `__sys_free`, `CMain::~CMain`, `InvokeCMain`, `SetGameFrameDrawn`,
 * `fn_80008A1C`, `~CPlayerState`, `~CPlayerState::SPersistentState`, `~CStaticInterference` and
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

bool CMain::fn_80008A1C() { return screenFading; }

void CMain::SetMaxSpeed(bool v) {
  if (v && !screenFading) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  x5c = 0.0f;
  screenFading = v;
}

void CMain::SetGameFrameDrawn(bool drawn) { x91_24_gameFrameDrawn = drawn; }

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

