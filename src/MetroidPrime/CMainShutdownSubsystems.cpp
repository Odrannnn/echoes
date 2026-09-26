/**
 * `CMain::ShutdownSubsystems` - retail `.text:0x80008570`, `size:0x110` = 272 bytes.
 *
 * Carved out of `MetroidPrime/mainTail.cpp`, which used to claim 0x80008570-0x80009880 in one
 * range. A `Matching` carve for `CMain::InitializeSubsystems` (0x80008680) needs the whole
 * 0x80008570..0x800087DC, and a unit may not claim two discontiguous ranges in one section, so
 * the only way to reach it is to move *this* function into a unit of its own - which is what this
 * file is. `mainTail.cpp` keeps 0x80008680-0x80009880 plus its `.ctors` and `.sbss`; its header
 * has the split's reasoning and the other constraint (`.ctors`/`.sbss` must sit on the **last**
 * unit in `splits.txt`, or `dtk dol split` fails with "Mismatched splits for .ctors").
 *
 * ## Nothing here has a name in the tree
 *
 * Retail's symbol table names only `CMain::ShutdownSubsystems` itself out of this function, so
 * all ten callees and the one global it reads are dtk labels. They are declared, never defined:
 *
 * | symbol | retail | what retail's own bytes say it is |
 * | --- | --- | --- |
 * | `fn_800E8494` | 0x800E8494, 0x3C | clears a 0x870-byte .sbss object and a byte |
 * | `fn_802DAE24` | 0x802DAE24, 0xC | `stb 0` to one byte, then `blr` |
 * | `fn_8002AD44` | 0x8002AD44, 0x84 | walks a table at 0x8040D058, four bytes at a time |
 * | `fn_801F03C4` | 0x801F03C4, 0x70 | ctor: `(this, string const&, bool)`, returns `this` |
 * | `fn_801F02C4` | 0x801F02C4, 0x44 | `if (!this->x4) { f(this->x0); this->x4 = 1; }` |
 * | `fn_801F025C` | 0x801F025C, 0x24 | returns `fn_80213678(this->x0)`, which returns 0 or 1 |
 * | `fn_801F05D0` | 0x801F05D0, 0xF8 | walks `x0->x8` and calls itself |
 * | `fn_80218760` | 0x80218760, 0x2C | `x8(g_804193F8)` through a loaded vtable slot |
 * | `fn_801F0280` | 0x801F0280, 0x44 | `if (this->x4) { f(this->x0); this->x4 = 0; }` |
 * | `fn_801F0308` | 0x801F0308, 0x58 | `setPriority(short)`: `this->x0` then `extsh` the argument |
 * | `fn_801F0518` | 0x801F0518, 0xB8 | walks `x8` and calls `fn_801F05D0` on each entry |
 * | `fn_800DC03C` | 0x800DC03C, 0x74 | walks a table at 0x8040B47C, four bytes at a time |
 *
 * The 0x801F0xxx family is one class - an 8-byte object `{void* x0; bool x4;}` whose `x0` is a
 * record with a `short` at +0x22 and a `word` at +0x24 - and `fn_801F03C4` copies the `string`
 * argument into it, so retail's second argument really is a `rstl::string const&` and not a
 * `char const*`: the call passes **the address of the temporary** (`addi r4,r1,16` at 0x800085A4),
 * and `fn_801F03C4` goes on to build a second string from it at 0x801F03E0.
 *
 * `lbl_80418EC8` is .sbss 0x80418EC8: `_SDA_BASE_` is 0x8041FD80, and `-28344` is 0x80418EC8 -
 * the address of `CGameGlobalObjects`' +0x150 member, stored by its constructor at 0x80008558.
 * `src/MetroidPrime/PortGlobals.cpp` is where the port defines it, and
 * `src/MetroidPrime/CGameGlobalObjectsCtor.cpp` is where it is written.
 *
 * ## The stack-guard scan, and what it agrees with `InitializeSubsystems`
 *
 * `0x80008638` is `addis r0,r3,-29495 ; cmplwi r0,53261`, i.e. `word + 0x8CC90000 == 0xD00D`,
 * i.e. `word == 0x7337D00D` - the same constant `InitializeSubsystems` stores at 0x80008710 in the
 * other direction, 0x2EC bytes away. Two instructions in two different functions, agreeing.
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/CFrameDelayedKiller.hpp"
#include "rstl/string.hpp"
#include "types.h"

#include "dolphin/os/OSThread.h"
#include "dolphin/os.h"

// `<stdint.h>` is where `uintptr_t` comes from; `dolphin/types.h` guards its own `<stdint.h>`
// behind `TARGET_PC`, and `CMain.hpp` does not reach it, so it is named directly - the same
// arrangement `MetroidPrime/mainTail.cpp` records in its own header.
#include <stdint.h>

// Retail's own string pool, `.rodata` 0x803A56C0, 0x1C0 bytes. The two literals below are at
// **0xCB** and **0x16A** into it - "Tweaks.rel" and "Stack usage: %d bytes (%dk)\n" - and
// retail reaches each as a `lis`/`addi`/`addi` triple with `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO`
// relocations against the pool (0x8000858C-0x80008598 and 0x80008650-0x80008660); the same
// arrangement `CMain::InitializeSubsystems` uses for its two formats at 0x187 and 0x19D.
// Naming the pool and indexing it is what
// reproduces the three instructions; a literal of our own would emit MWCC's `@stringBase0` pool
// and a `.rodata` section a `Matching` unit may not own.
extern "C" const char lbl_803A56C0[];

// .sbss 0x80418EC8, declared only: `src/MetroidPrime/PortGlobals.cpp` defines it and
// `src/MetroidPrime/CGameGlobalObjectsCtor.cpp` writes it, and a definition here would duplicate
// both.
extern "C" void* lbl_80418EC8;

extern "C" void fn_800E8494();
extern "C" void fn_802DAE24();
extern "C" void fn_8002AD44();

// The three-argument constructor of the 0x801F0xxx class. Retail's copy returns `this`
// (`mr r3,r30` at 0x801F0410), so it is declared as returning a pointer; nothing here uses the
// value, and no `mr` is emitted at the call site either way.
extern "C" void* fn_801F03C4(void* self, const rstl::string& name, bool start);
extern "C" void fn_801F02C4(void* self);
// Returns 0 or 1 (0x801F025C forwards `fn_80213678`, which returns 0 or 1), and the loop
// below tests the low byte of it - see the comment there.
extern "C" int fn_801F025C(void* self);
extern "C" void fn_801F05D0(void* owner);
extern "C" void fn_80218760();
extern "C" void fn_801F0280(void* self);
// The second argument is a `short`: `fn_801F0308` does `extsh. r0,r31` on it at 0x801F0334.
extern "C" void fn_801F0308(void* self, short value);
extern "C" void fn_801F0518(void* owner);
extern "C" void fn_800DC03C();

// The 8-byte object the 0x801F0xxx class occupies on the stack, at r1+8. It is **not** a retail
// type and it is never constructed here: `fn_801F03C4` is what fills it in, and retail emits no
// store to r1+8 before that call. A local of a struct whose address is taken and whose members
// are never read is the only shape that allocates eight bytes and nothing else - the same
// "local duplicate shape" trick `src/MetroidPrime/Player/CGameStateStreamCtor.cpp` uses.
struct STuObject {
  void* x0_owner;
  bool x4_started;
};

#ifdef TARGET_PC
// Host: see the block comment below. Declared in PortBoot.cpp, next to
// `PortInitializeSubsystems`, for the same reason.
void PortShutdownSubsystems();
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
  // than guessed. Retail's test is `54 60 06 3f` + `beq` at 0x800085D4 - `clrlwi. r0,r3,24`, which
  // keeps the low eight bits. Thirteen spellings of the obvious `& 0xFF000000` (and of `!= 0`,
  // `>> 24`, `<< 8`, signed/unsigned, the operands reversed) all compile to `54 60 00 0f`,
  // `clrrwi. r0,r3,24`, and are one instruction wrong; `& 0xFF` is the only mask measured that
  // emits retail's bytes. **`& 0xFF != 0` would be `cmpwi r3,0` and one instruction shorter**, so
  // the mask is in the source and not an artefact.
  //
  // `fn_801F025C` returns 0 or 1 in the version in this tree, so with a `bool` return the loop
  // would be live and without the mask it is dead either way; what has to match is the two
  // instructions and the `b` over the body at 0x800085C0.
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
  // instructions short of retail and no spelling of it gets closer: mwcceppc's register
  // allocator puts the masked value in r3 and `p` in r4, so `limit` needs a fourth register and
  // lands in r6 where retail has it in r3. Written as a separate `p += 0x100` the allocator
  // coalesces the masked value into `p`'s register, r4 serves both, and r3 stays free for
  // `limit` - which is retail's `clrrwi r4,r0,10 ; addi r3,r5,-8192 ; addi r4,r4,1024` exactly.
  // `0x100` is 256 **words**; mwcceppc scales it to the `addi`'s 1024 bytes.
  p += 0x100;
  for (uint* limit = (uint*)(stackBase - 0x2000); p < limit; ++p) {
    if (*p + 0x8CC90000 != 0xD00D) {
      break;
    }
  }
  uint used = (uint)(stackBase - 0x2000) - (uint)p + 0x2000;
  OSReport(lbl_803A56C0 + 0x16A, used, used >> 10);
}
#endif // TARGET_PC

// Why the host path is a different function, in one paragraph, because "reproducing retail here
// is correct for the matching build and cannot be done for the port" is the claim.
//
// Retail's body cannot be compiled for a PC at all, and the first reason is not a hazard but a
// link error: **eleven of its twelve callees are retail functions this tree has no body for**.
// `CFrameDelayedKiller::ShutDown` is written (`src/Kyoto/CFrameDelayedKiller.cpp`, `Matching`);
// the other eleven are dtk labels with declarations only, so a host build of the body below
// would fail to link on ten undefined references. The second reason is the one
// `src/MetroidPrime/mainTail.cpp` documents for `InitializeSubsystems`, and it is the same block
// of memory: the scan at the end reads `OSGetCurrentThread()` +0x304/+0x308 as a stack pointer
// and looks for 0x7337D00D in the 8 KB *below* it. Aurora's `OSThread` puts `stackBase` and
// `stackEnd` at exactly those offsets, so the shape compiles, the offsets are right, and what it
// reads is Aurora's allocator's memory rather than a stack - and the `OSReport` after it would
// print a host address through a guest format. Retail's guard word is not a byte, so there is
// nothing to reproduce through a host equivalent either.
//
// So the host body is `PortShutdownSubsystems()` in `src/MetroidPrime/PortBoot.cpp`, a
// translation unit configure.py never claims - the same arrangement as
// `CMain::InitializeSubsystems` and `CMain::RsMain`, and for the same reason. mwcceppc does not
// define TARGET_PC, so this guard costs the matching build nothing: the object it compiles is
// byte for byte the retail body, which is what the `Matching` claim in `configure.py` needs.
