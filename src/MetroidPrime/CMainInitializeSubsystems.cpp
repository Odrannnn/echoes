/**
 * `CMain::InitializeSubsystems` - retail `.text:0x80008680-0x800087DC`, 0x15C = 348 bytes.
 *
 * **This file exists because of the one-range-per-section rule.** `CMain::InitializeSubsystems` is
 * the *first* function of `src/MetroidPrime/mainTail.cpp`'s claim (0x80008680-0x80009880 plus
 * `.ctors` and `.sbss`), so a `Matching` unit for it has to take 0x80008680-0x800087DC away and
 * leave `mainTail` the range above it - and `dtk dol split` rejects a unit that claims two
 * discontiguous ranges in one section ("Cyclic dependency ... link order"), before anything
 * compiles. `mainTail` keeps `.ctors` and `.sbss` because those have to sit on the **last** unit in
 * the list (with them on the earlier one dtk fails with "Mismatched splits for .ctors").
 *
 * The split moves exactly one function, and it is not in the class that costs something: the
 * measured mechanism for an expensive cut is mwcceppc's `@stringBase0` pool, which is ordered by
 * first use in emission order, so a cut that moves a function with string literals out of a unit
 * re-orders that pool and rewrites the `lis`/`addi` pairs of everything emitted after it. The
 * gate prints the exact count (`1 function(s) moved ... exact count match - a split, not a loss`)
 * and every other `mainTail` function held its percentage, which is what makes this cut free.
 *
 * **Source order inside a unit is descending by address** (mwcceppc emits functions in reverse
 * source order). This file holds one function, so it is descending trivially - but a second one
 * added here has to go *above* this one.
 *
 * ## What is left: one register transposition, and why it is a wall rather than a spelling
 *
 * 99.08% (objdiff, 348/348 claimed, `unit_fit` clean, no extra functions). The 15 instructions
 * that differ are one transposition: mwcceppc puts the loop **bound** `stackBase - 0x2000` in r5
 * and the **stack-guard word** in r4, where retail has r4 and r5 respectively. Nothing else
 * differs - the `& ~1023` mask (`54 1e 00 2a`), the `+3`/`>>2` trip count, the eight-wide
 * unroll and the remainder loop are all byte-exact, and `54 60 06 3f` is not in this function at
 * all (it decodes to `rlwinm r0,r3,0,12,31` = `& 0xFFFFF000`).
 *
 * **The cause is register *priority*, and it is measured, not guessed.** The guard word is live
 * across the loop with ten machine references: one `lis`/`addi` materialisation plus nine `stw`s,
 * eight of them from the unrolled body. The bound has four (`addi r3,r4,3`, `cmplw r6,r4`, the
 * `subf` for `DCFlushRange`, and its own definition). mwcceppc weights a reference by the loop
 * depth it sits at, so the word wins the lower register and the transposition is what we get.
 *
 * **27 variants were measured and every one of them lands on the same 15 instructions**: a
 * function-local `const uint` for the word before the loop, after the loop, and at the top of the
 * function; `for` vs `while` vs an empty increment slot; `p < fillStart` vs `fillStart > p` vs
 * `!(p >= fillStart)` vs a doubled condition; `void*`/`uchar*`/`(uint)` casts on the bound; a
 * named `uint* const` limit; `p[0] =` instead of `*p =`; `p += 1` instead of `++p`;
 * `const uint* stackBase`; `stackBase - 0x800` instead of the `uintptr_t` cast; the byte count
 * spelled in bytes rather than words; naming `fillFrom`; and moving the `DCFlushRange` call
 * before the fill. The `fillStart > p` and doubled-condition forms are *worse* - they lose the
 * eight-wide unroll and drop to 272-280 bytes. A fill written as an inlined
 * `static void FillGuardWords(uint*, uint*, uint)` helper, so the word arrives as a parameter and
 * the inlined body allocates differently, is worse still: 72.91%.
 *
 * **The one lever that does flip it costs an instruction, which is why it is not taken.** Naming
 * the byte count *before* the loop - `uint bytes = (uint)((uintptr_t)fillStart - (uintptr_t)
 * (guardEnd + 0x400 / 4));` - gives the bound the extra pre-loop reference it needs and the
 * pairing becomes retail's: `addi r4,r29,-8192` and `lis r5,29496`. It is 6 differing
 * instructions instead of 15, and it is **worse**: the named count keeps `guardEnd + 0x400/4`
 * alive across the loop, so the fill loses its `mr r6,r0` fold, `DCFlushRange` loses its
 * rematerialised `addi r3,r30,1024`, and the object becomes 0x160 = 352 bytes - four over the
 * 348-byte claim, 96.38% instead of 99.08%, and no longer able to fit its range at all. Every
 * shape that names the count (12 of them, including the byte-spelled and `const`-qualified
 * variants) produces exactly those 352 bytes. `tools/try_batch.py` ranks that variant *first* at
 * 6 differing instructions, which is the trap: it counts instructions and not size, and a unit
 * that has outgrown its claim is worth nothing.
 *
 * So the honest state is a narrow `NonMatching` unit at 99.08% whose object already fits its
 * range exactly, one register transposition from `Matching`.
 *
 * ## What this unit is worth to the port
 *
 * Nothing, and it is better to say so than to quote a percentage. `TARGET_PC` is `PUBLIC` on
 * `mp_game` (CMakeLists.txt:162), so the port build compiles `PortInitializeSubsystems` from
 * `src/MetroidPrime/PortBoot.cpp` instead of the body below and **this translation unit's host
 * object defines no symbols at all** (measured with `nm`). There is therefore no `PortReachStubs`
 * alias for it, none is needed, and adding one would only duplicate a symbol the port does not
 * want. `matched`/`linked` are not expected to move when this unit lands; the port reaches boot
 * step 17 and `docs/research/boot_path.md` step 11 already records why this one line has to
 * differ on a PC (Aurora's `ARAlloc` faults on its own assert, and the 8 KB stack-guard block
 * lands on whatever Aurora's allocator put below the thread's stack base).
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/CFrameDelayedKiller.hpp"
#include "dolphin/ar.h"
#include "dolphin/arq.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSThread.h"

// `<stdint.h>` is what `uintptr_t` comes from, and the body below casts through it four times.
// It is named directly because `MetroidPrime/CMain.hpp` does not reach it, and neither does
// `dolphin/types.h`, which guards its own `<stdint.h>` behind `TARGET_PC` (the host-only
// typedefs).
#include <stdint.h>
#include <stdio.h>

// The two retail globals this reads are `extern` only, and deliberately so:
// `lbl_80418BA8` is .sdata at 0x80418BA8 holding 0x4000 - which is Aurora's own
// `ARAM_STACK_START` (extern/aurora/lib/dolphin/AR.cpp:17) - and it is the ARAM bump pointer,
// `lbl_80418BA8 = lbl_80418BA8 + lbl_80418EA0`. `lbl_80418EA0` is the four bytes at 0x80418EA0,
// .sbss, so zero at load; the one writer is `fn_80009864` (0x80009864, in
// `src/MetroidPrime/mainTail.cpp` and in the ldscript's FORCEACTIVE list), which computes it as
// `*(u32*)0x80415980 * 14`.
//
// The two are declared rather than defined because a definition in a `NonMatching` unit costs:
// adding one `static` below moved `__ct__CGameArchitectureSupport` 84.51% -> 81.54% and
// `AddWorldPaks` 96.00% -> 95.97% (measured, and recorded at the head of
// `src/MetroidPrime/PortGlobals.cpp`). They are also only reachable on the non-TARGET_PC path,
// so no port build needs a definition of either - and the host object of this file defines
// nothing at all, which is the point recorded in the file header.
extern "C" uint lbl_80418BA8;
extern "C" uint lbl_80418EA0;

// The two printf formats are retail's, at 0x803A5847 and 0x803A585D in .rodata. They are
// **not** literals in this unit and not named buffers either: retail's own object reaches them
// as `lbl_803A56C0 + 0x187` and `lbl_803A56C0 + 0x19D` - `lis r4,0` / `R_PPC_ADDR16_HA
// lbl_803A56C0` / `addi r3,r4,0` / `R_PPC_ADDR16_LO lbl_803A56C0` / `addi r3,r3,391` at
// 0x800086C8-0x800086D4, and the same shape with 413 at 0x80008780-0x80008790. `lbl_803A56C0`
// is `.rodata` 0x803A56C0, 0x1C0 bytes, and it is retail's string pool: the two formats are
// 0x187 and 0x19D into it, and `CGameGlobalObjects::AddPaksAndFactories` in
// `src/MetroidPrime/main.cpp` reaches its pak names at the same base. Declaring the pool and
// indexing it reproduces the four instructions; a literal or a file-scope buffer of our own emits
// three and relocates against MWCC's own `@stringBase0` pool instead. See main.cpp's comment on
// the same symbol for what that costs and why the linked bytes are identical either way.
extern "C" const char lbl_803A56C0[];

// `ARInit`'s first argument. Retail loads it as a **relocation against a 12-byte .bss object**
// (`lis r3,0` / `R_PPC_ADDR16_HA lbl_803C5AB8` / `addi r3,r3,0` / `R_PPC_ADDR16_LO
// lbl_803C5AB8` at 0x80008688/0x80008694), not as the literal `(u32*)0x803c5ab8` the source
// first used, which mwcceppc turns into `lis r3,-32708 ; addi r3,r3,23224` - the same value, two
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
// `docs/research/boot_path.md` step 11 says 0x7338D0D0 (a transposition), an earlier version of
// the comment here said 0x7338D00D (the same transposition with the nibbles in order), and the
// value in the source was 0x7338D00D - 0x10000 too high, which is why the `lis` immediate was
// 29497 where retail's is 29496. `CMain::ShutdownSubsystems` (0x80008570) confirms it
// independently: its stack scan at 0x80008638 is `addis r0,r3,-29495 ; cmplwi r0,53261`, which
// is `word + 0x8CC90000 == 0xD00D`, i.e. `word == 0x7337D00D`. Two instructions 0x2EC bytes
// apart, both agreeing.
static const uint kStackGuardWord = 0x7337D00D;

// The five calls between the second `printf` and `CFrameDelayedKiller::Initialize`, in retail's
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
  ARAlloc(lbl_80418EA0);
  lbl_80418BA8 = lbl_80418BA8 + lbl_80418EA0;
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

// Why the host path is a different function, in one paragraph, because "reproducing retail here
// is correct for the matching build and actively dangerous for the port" is the claim.
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
// value 0x7337D00D is not a byte, so it cannot be reproduced through `OSClearStack`.
//
// So the host body is `PortInitializeSubsystems()` in src/MetroidPrime/PortBoot.cpp, a
// translation unit configure.py never claims - the same arrangement as CMain::RsMain in
// PortBoot.cpp. mwcceppc does not define TARGET_PC, so this guard costs the matching
// build nothing: the object it compiles is byte for byte the retail body.
