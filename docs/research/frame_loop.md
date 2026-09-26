# The frame loop's thirteen symbols, one at a time

Written 2026-09-26 by lane **e2** from a clean build of commit `4d49561` plus this lane's own
units. Every address, size and percentage below is measured, not recalled: addresses and sizes out
of `config/G2ME01/symbols.txt`, instructions out of `tools/dis.sh` (which wraps
`build/binutils/powerpc-eabi-objdump` on `build/G2ME01/main.elf` and is new in this lane, as is
`tools/sda.py` for resolving a `disp(r13)`/`disp(r2)`), and percentages out of `build/report.json`
after `./tools/decomp_build.sh`.

**What this lane did, in one paragraph:** five of the thirteen are now real, byte-exact, `Matching`
units in the DOL with the port's link gap closed for each; one more is a `NonMatching` unit at
69.16% that closes the sixth ratchet symbol without putting a wrong byte in the binary; one more
(`CIOWin::CIOWin`) was not on the list at all and had to be written for `CMainFlow`'s sake; a
seventh (`UnloadAudio`) was written to 85.16% and then **deleted**, because mwcceppc emits small-data
references for three of its globals that no linker can resolve; and **four of what is left - 1,212
bytes, 47% of the list - were blocked by one cause**: this tree's `rstl::rc_ptr` is one word wide and
retail's is two, so `RemoveAllIOWins`, `PumpMessages`, `AddIOWin` and `CInputGenerator::Update`
could not be written without a change to `include/rstl/rc_ptr.hpp` that would move every unit in the
tree. Details and the measurements are in each section.

**Superseded, 2026-09-26, by lane f1** - see `docs/research/rc_ptr.md`, which carries the evidence
and the change. The width is now retail's, with the DOL's sha1 and all 86 RELs unchanged. Of those
1,212 bytes, **1,084 are unblocked** and 128 still wait on the out-of-line copy constructor; the
real blocker has moved to mwcceppc's `operator new` operands. What follows is e2's original text
and is kept because the reasoning still holds - only the conclusion has moved.

**The frame loop still cannot run, and nothing here changes that.** `CGameArchitectureSupport`'s
constructor dereferences `gpTweakPlayerA` at 0x80007F38 and `gpGameState` at 0x800081A4 with no null
test, and neither is this lane's to fix. What changed is that the code *behind* those two
dereferences is now there.

## The thirteen

Sizes are retail's. "State" is **Matching** (the unit reproduces these bytes and the DOL's sha1
still matches with the unit's object in the link), **NonMatching** (a body exists, none of its bytes
are in the DOL, retail's stay), or **blocked** (nothing written, and the reason is measured).

| # | symbol | retail | bytes | state | unit |
| --- | --- | --- | --- | --- | --- |
| 1 | `CIOWinManager::CIOWinManager` | 0x80049DE8 | 40 | **Matching, 100%** | `src/MetroidPrime/CIOWinManagerCtor.cpp` |
| 2 | `CIOWinManager::~CIOWinManager` | 0x80049D84 | 100 | **Matching, 100%** | same unit |
| 3 | `CStopwatch::CSWData::Initialize` | 0x8028C17C | 124 | **Matching, 100%** | `src/Kyoto/Basics/CStopwatchCSWData.cpp` |
| 4 | `CInputGenerator::CInputGenerator` | 0x8001DA84 | 112 | **Matching, 100%** | `src/MetroidPrime/CInputGeneratorCtor.cpp` |
| 5 | `CMainFlow::CMainFlow` | 0x8001E008 | 104 | **Matching, 100%** | `src/MetroidPrime/CMainFlowCtor.cpp` |
| 6 | `CStopwatch::CSWData::Wait` | 0x8028C1F8 | 148 | **NonMatching, 69.16%** | `src/Kyoto/Basics/CStopwatchCSWDataWait.cpp` |
| 7 | `CIOWinManager::RemoveAllIOWins` | 0x80049A18 | 128 | **written, 100.00%, `NonMatching`** - blocked on four *registers* in the copy ctor, not on the call | `src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp` |
| 8 | `CIOWinManager::PumpMessages` | 0x800496A0 | 196 | **written, 100.00%, `NonMatching`**, with `CArchitectureQueue::Pop` (`fn_800495F0`, 176 B) also 100.00% | `src/MetroidPrime/CIOWinManagerPumpMessages.cpp` |
| 9 | `CIOWinManager::AddIOWin` | 0x80049BDC | 380 | **written, 95.74%, `NonMatching`** - blocked by `new`'s operands | `src/MetroidPrime/CIOWinManagerAddIOWin.cpp` |
| 10 | `CInputGenerator::Update` | 0x8001D888 (unnamed) | 508 | **written, 98.27%, `NonMatching`** - a 16-byte frame slack is all that is left | `src/MetroidPrime/CInputGeneratorUpdate.cpp` |
| 11 | `CGameArchitectureSupport::UnloadAudio` | 0x8029EF20 (unnamed) | 172 | **attempted 85.16%, not kept** - see below | - |
| 12 | `AllocateRenderer` | 0x8026EF54 | 156 | not attempted | - |
| 13 | `CMain::ResetGameState` | 0x80003A48 | 416 | **another lane's** - `src/MetroidPrime/main.cpp` | - |
| - | `CIOWin::CIOWin` | 0x80049E98 (unnamed) | 64 | **Matching, 100%** | `src/MetroidPrime/CIOWinCtor.cpp` |

**A correction to the briefing's arithmetic, because it changes what "twelve" means.** The briefing and
`boot_path.md` both say twelve symbols, and both give a byte breakdown that adds up to **2,584** -
but the breakdown lists **thirteen** functions, because the `CIOWinManager` row covers four methods,
`CInputGenerator` two and `CSWData` two:

```
844 (CIOWinManager x4 = 40+100+128+380)  +  620 (CInputGenerator x2 = 508+112)
+ 272 (CSWData x2 = 124+148)  +  416 (ResetGameState)  +  172 (UnloadAudio)
+ 156 (AllocateRenderer)  +  104 (CMainFlow)  =  2584
```

So it is **thirteen functions in 2,584 bytes**, and this table has thirteen rows. Everything else
about the briefing holds; only the count was off by one, and the fix is to enumerate.

Against those 2,584 bytes:

| | bytes | |
| --- | --- | --- |
| **Matching, in the DOL, 100%** | **480** | rows 1-5 |
| NonMatching, 69.16%, not in the binary | 148 | row 6 |
| written to 85.16%, could not be linked, deleted | 172 | row 11 |
| **blocked by `rstl::rc_ptr`'s layout** (as measured by e2; see the note below) | **1,212** | rows 7-10 |
| not attempted | 156 | row 12 |
| another lane's file | 416 | row 13 |
| | **2,584** | |

So **480 of 2,584 bytes are real and linked, and 1,212 - 47% of the whole list, and the largest
single thing on it - sit behind one modelling change to `include/rstl/rc_ptr.hpp`.** Plus 64 bytes
that were not on the list at all (`CIOWin::CIOWin`), which is the only work this lane did that was
not asked for.

**Where those 1,212 bytes stand after lane f1** (`docs/research/rc_ptr.md`):

| | bytes | |
| --- | --- | --- |
| Matching, in the DOL, 100% | 480 | rows 1-5 |
| NonMatching, 100% | 44 | `IOWinPQNode::IOWinPQNode`, won by the `rc_ptr` width |
| NonMatching, 95.74% - blocked by `operator new`'s operands, **not** by `rc_ptr` | 380 | row 9 |
| **NonMatching, 100.00%** - blocked on four registers inside the copy constructor | 128 | row 7 |
| **NonMatching, 100.00%** | **196** | row 8, `PumpMessages` |
| **NonMatching, 100.00%** | **176** | `CArchitectureQueue::Pop` (`fn_800495F0`), which row 8 calls |
| **NonMatching, 98.27%** | **508** | row 10, `CInputGenerator::Update` |
| not attempted | 156 | row 12 |
| another lane's file | 416 | row 13 |

**After lane g4**, `docs/research/rc_ptr.md` has the copy-constructor detail and this file's own
rows have the per-function ones. `+4` functions reached 100% and `+1` unit
(`Kyoto/Graphics/CModelTouch`, `CModel::Touch`, 76 bytes) went `Matching` and into the DOL.
**The two things still standing between the frame loop and rows 7-10 are both mwcceppc
code-generation differences with no source spelling, and both are characterised**: the
out-of-line copy constructor's register allocation (four instructions; blocks row 7's 128
bytes and `src/rstl/rc_ptr_copy.cpp`'s own 36), and mwcceppc's 16-byte stack-slot slack
(blocks row 10's 508 bytes, which is otherwise instruction-for-instruction retail's).

`CIOWinManagerCtor.cpp` is one unit for rows 1 and 2 because
`0x80049D84 + 0x64 == 0x80049DE8`: the destructor and the constructor are adjacent in the DOL, so
one object claims both, 0x8C bytes.

## Row 1-2, `CIOWinManager`'s constructor and destructor

`src/MetroidPrime/CIOWinManagerCtor.cpp`, 0x80049D84-0x80049E10, `Matching`, 2/2 functions at 100%,
verified with `./tools/flip_test.sh MetroidPrime/CIOWinManagerCtor.cpp` -> `PASS -> kept as
Matching`.

The constructor is 40 bytes and six of the eight stores are not the two list heads: they are
`rstl::list<CArchitectureMessage>`'s constructor inlined into the member initialiser - allocator
word 0, four node pointers all set to `this+20` (the address of the list's own `xc_empty_prev`, which
is `this+8+0xC`), count 0. The body is `CIOWinManager::CIOWinManager() : x0_drawRoot(nullptr),
x4_pumpRoot(nullptr) {}` and nothing else.

The destructor is 100 bytes and does three things: `RemoveAllIOWins()`, the out-of-line
`rstl::list<CArchitectureMessage>` destructor instantiation (0x80009634), and `CMemory::Free(this)`
only when the destructor flag in r4 casts to a positive short, which is CodeWarrior's
deleting-destructor convention. The `addic. r0,r30,8 / beq` in front of the member destructor is the
dead test on the queue's allocator word the compiler emits. So the body is
`CIOWinManager::~CIOWinManager() { RemoveAllIOWins(); }` - an empty destructor would not call
`RemoveAllIOWins`, and the call is in retail's bytes.

Both were 100% on the first build. Nothing here is clever and that is the point: the two cheapest
symbols on the list are also the two that needed no iteration.

## Row 3, `CStopwatch::CSWData::Initialize`

`src/Kyoto/Basics/CStopwatchCSWData.cpp`, 0x8028C17C-0x8028C1F8, `Matching`, 1/1 at 100%.

This is one of the two the briefing calls "reached transitively", and the mechanism is worth writing
down because it is not obvious: `CStopwatch::Reset()` and `GetElapsedTime()` are `inline` in
`include/Kyoto/Basics/CStopwatch.hpp`, and `Reset()` calls `mData.Initialize()`. So
`CGameArchitectureSupport::UpdateTicks` (main.cpp:268) pulls this in *without naming it*, and
`_ZN10CStopwatch7CSWData10InitializeEv` was on the ratchet with nothing in the tree to blame for it.

124 bytes, three fields, one return:

| retail | what it is |
| --- | --- |
| `lwz r5,0xF8(0x80000000)` | the CPU-frequency register. Hardware has it at 0xCC0000F8; in this DOL the read lands at 0x800000F8 |
| `srwi r3,r5,2` | divided by 4 - the timebase ticks at a quarter of the CPU clock. Stored as a 64-bit value: low word first, then the zero high word |
| `__div2i(x0_timerFreq, 1000000)` | the second argument is `li r5,0` / `addi r6,r4,0x4240` with r4 = `lis r4,15`, i.e. 0x000F4240 = 1000000 built as a constant. The result lands in r3:r4. This is `x8_timerFreqO1M`, ticks per microsecond, which is what `GetElapsedMicros()` divides by |
| `__cvt_sll_flt(x0_timerFreq)` then `fdivs` against the float 1.0f at 0x8041E258 | `x10_timerPeriod`, seconds per tick, which is what `GetElapsedTime()` multiplies by |
| `li r3,1` | returns true, so retail's is `bool` - which the header already says |

## Row 4, `CInputGenerator::CInputGenerator`

`src/MetroidPrime/CInputGeneratorCtor.cpp`, 0x8001DA84-0x8001DAF4, `Matching`, 1/1 at 100%.

112 bytes: one call and six stores. `mr r3,r4` at 0x8001DAAC dereferences the pointer the *caller*
passed before the call to 0x8030B4A8, so retail's is `IController::Create(*x0_context)` and
`Create` takes a `const COsContext&`. The `single_ptr<IController>` member is one word wide on
retail, so its initialiser is the bare store. The four `stb r0` with r0 = 0 zero
`x8_connectedControllers[0..3]`.

That last part is the one iteration it took. C++ does not allow an array in a constructor's
mem-init list, so `x8_connectedControllers()` does not compile; the source is an explicit
`for (int i = 0; i < 4; ++i) x8_connectedControllers[i] = false;` in the body, which mwcceppc
unrolls to the four `stb` retail has, in retail's order (after the float stores, not before).

## Row 5, `CMainFlow::CMainFlow`

`src/MetroidPrime/CMainFlowCtor.cpp`, 0x8001E008-0x8001E070, `Matching`, 1/1 at 100%. It is on the
path because `CGameArchitectureSupport`'s constructor calls `new CMainFlow()` (main.cpp:243).

104 bytes: build a temporary `rstl::string` from a literal, hand it to the base constructor,
destroy it, store this class's vptr, store `kCFS_Unspecified`. The order is the Itanium one - the
base constructor first, the most-derived vptr last - and MWCC follows it, so the body is
`CMainFlow::CMainFlow() : CIOWin(rstl::string_l("MainFlow")), x14_gameState(kCFS_Unspecified) {}`.

Two things about it are worth carrying to the next lane.

**The string cannot be a string literal.** Retail points at 0x803A60A7, which is seven bytes into
`lbl_803A60A0`: the bytes in front are `??(??)\0`, the tail of a different literal the linker merged
into the same blob, and the `+7` is a separate `addi`. A literal in the source puts nine bytes in
*this object's* `.rodata`, and a `Matching` object's `.rodata` is linked into the DOL. So the name is
claimed where retail put it and the offset is written out: `rstl::string_l(lbl_803A60A0 + 7)`, with
`extern "C" const char lbl_803A60A0[];`. That reproduces all three instructions
(`lis` / `addi lo` / `addi 7`) and leaves the object with **no `.rodata` section at all**.

**It needed two renames, not one.** `CIOWin::CIOWin(const rstl::string&)` is retail 0x80049E98 and
is *unnamed* in `config/G2ME01/symbols.txt` (`fn_80049E98`), so a mem-init list naming it would not
link. Renaming `fn_80049E98` to `__ct__6CIOWinFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>`
- the name mwcceppc emits, read out of the object with `nm`, not guessed - and writing the 64-byte
constructor is the fix. And the constructor's own `stw <vtable>,0(r3)` needs `__vt__6CIOWin`, so
`lbl_803B1BA0` has to be renamed to that as well; likewise `lbl_803B1770` to `__vt__9CMainFlow` for
CMainFlow's own vptr. All three renames are of symbols nothing in the tree referenced before, and
the DOL link is what proves each one.

## Row 6, `CStopwatch::CSWData::Wait` - the closest of the blocked ones

`src/Kyoto/Basics/CStopwatchCSWDataWait.cpp`, 0x8028C1F8-0x8028C28C, **`NonMatching`, 69.16%**. It is
the second of the two transitive symbols, and `CStopwatch::Wait(float)` in
`src/Kyoto/Basics/CStopwatch.cpp` calls it, so leaving it out keeps a ratchet entry open.

What is left is register allocation, not logic, and both halves are MWCC idiom selection rather than
anything the source says:

- Retail materialises the biased frequency **in memory**: `stw 0x43300000,24(r1)`,
  `stw (freq>>2),28(r1)`, then a single `lfd f0,24(r1)`. That works because 2^52 plus an integer
  below 2^28 *is* a double whose high word is the constant 0x43300000 and whose low word is the
  integer, so the compiler can build it with two stores and load it with one instruction. Written as
  ordinary double arithmetic, mwcceppc keeps it in a register and emits
  `fadd f0,f3,f0 ; fsub f0,f0,f3` for the same round trip. Three source spellings were tried
  (inline in the argument, an intermediate `biased`, an intermediate `period`); the third reached
  69.16% and the other two 44-46%, and none of them changes that pair.
- Retail's spin loop spills each `OSGetTime()` result to the stack, reloads it, subtracts, spills
  *that*, reloads it again, and only then does `cmpwi r0,0 / blt`. mwcceppc here keeps the
  difference in a register and branches on `subfc.` directly.

`0x8041E260` is `0x4330000000000000` as a double, i.e. 2^52 exactly, and `lbl_8041E260` is its retail
name.

The unit is `NonMatching` on purpose: none of its bytes go into the DOL, retail's stay, and
`config/G2ME01/splits.txt` may still name the range - that is what `MetroidPrime/CActor.cpp` does.
The port gets a working busy-wait, which is what the ratchet entry was for.

## Rows 7-10, the four `rc_ptr` users: blocked, and the reason generalises

> **Read `docs/research/rc_ptr.md` first.** This section is e2's diagnosis and it was right about
> the *width* and wrong about the consequence: the out-of-line copy constructor blocks **one** of
> these four, not four, because all 15 of `fn_80049010`'s call sites in the DOL are `CIOWinManager`
> methods and these other three inline the copy. The width is now retail's.

1,212 bytes - `RemoveAllIOWins` 128, `PumpMessages` 196, `AddIOWin` 380, and
`CInputGenerator::Update` 508 - and **all four are blocked by the same thing: this tree's
`rstl::rc_ptr` does not have retail's layout, and fixing it would move every unit in the tree.**
That is 47% of the whole 2,584-byte list, and it is the single most valuable thing a next lane can
do. The first three are described here; the fourth's shape is in "What a next lane should do" below,
and it is blocked for the same reason - it builds two `CArchitectureMessage`s through `rc_ptr`
temporaries that are AddRef'd and released.

Retail's `rstl::rc_ptr<CIOWin>` is **eight bytes**: `{ CIOWin* x0_ptr; u32* x4_refCount; }`. The
evidence is in three places and they agree:

1. `fn_80049010` (0x80049010), which is the copy constructor, is `lwz r5,0(r4) ; lwz r0,4(r4) ;
   stw r5,0(r3) ; stw r0,4(r3)` then an AddRef through the *second* word. Eight bytes, and it is an
   out-of-line function.
2. `AddIOWin` copies **two** words out of its `ncrc_ptr` parameter (`lwz r0,0(r30)` /
   `lwz r8,4(r30)` into the stack slot at r1+16) and then increments `*(r8)`, i.e. the refcount
   lives at the address in the *second* word.
3. `operator new` in `AddIOWin` asks for **16** bytes (`li r3,16`), which is
   `IOWinPQNode { ncrc_ptr x0_iowin; int x4_prio; IOWinPQNode* x8_next; }` only if the
   `ncrc_ptr` is eight bytes.

This tree's is **four bytes**: one `CRefData*` (`include/rstl/rc_ptr.hpp:58`). Its copy
constructor is `inline` in the header. So a body written here copies one word where retail copies
two, and the compiler inlines the copy where retail calls out to `fn_80049010`. There is no spelling
of `RemoveAllIOWins` that produces retail's bytes: the copy has to be a call, and in this tree it
cannot be.

`RemoveIOWin` (retail `fn_80049A98`, 0x80049A98, 0x144 bytes, unnamed) is the other half of the
blocker. Retail passes it a *pointer* to a stack copy of the node's `ncrc_ptr`
(`mr r3,this ; addi r4,r1,16 ; bl fn_80049A98`, then `ReleaseData(r1+16)`), and compares
`*(the argument)` against `*(the copy)` - so it takes a reference, while
`include/MetroidPrime/CIOWinManager.hpp:30` declares it by value. That is a header fix, and it is
safe (`RemoveAllIOWins` is the only caller and `main.cpp` never calls it), but it fixes the wrong
half: the eight-byte layout is the blocker, not the parameter passing.

**What it would take.** Modelling retail's `rc_ptr` as `{ T* x0_ptr; u32* x4_refCount; }` is the
correct fix and it is a *global* change: `rc_ptr` is used by `CArchitectureMessage`, `CGameState`,
`CEntity`, `CDamageInfo`, `TypesMatch` and more, and every one of those units' SDA offsets and
instruction selections depend on its current shape.

**Lane f1 did this, and the premise above turned out to be half wrong.** The full evidence is in
`docs/research/rc_ptr.md`; the correction that matters for planning is this: **the out-of-line copy
constructor blocks one of these four, not four.** `fn_80049010` has 15 call sites in the whole DOL
and every one is a `CIOWinManager` method - `RemoveAllIOWins` is 2 of them. `AddIOWin`,
`PumpMessages` and `CInputGenerator::Update` all *inline* the identical eight instructions. What
those three were blocked by was the **width**, and the width is now retail's. So of the 1,212 bytes
this section called blocked, **1,084 are unblocked today** and 128 still wait on
`fn_80049010`.

The width change is landed, with the DOL's sha1 and all 86 RELs unchanged: `ReleaseData` went
87.84% -> **100.00%**, `main/Kyoto/CObjectReference` 8/10 -> **10/10** (its `void* x20_refData` was
a phantom member; there is nothing at 0x20), and `CSimplePool` 0x20 -> 0x24 and
| **NonMatching, 100.00%** - blocked on four registers inside the copy constructor | 128 | row 7 |
Both are `NonMatching`, and each is one instruction from a wall that has nothing to do with
`rc_ptr`: **mwcceppc materialises `operator new`'s file-string operand as `lis` + *two* `addi`s
where retail's own compiler does the same, and this compiler does `lis` + one `addi` against
relocations** - so no function that allocates can be `Matching`. That is now the biggest single
blocker on the list, and it is bigger than this section was.

## Row 11, `CGameArchitectureSupport::UnloadAudio`: attempted, 85.16%, and **not kept**

`src/MetroidPrime/CGameArchitectureSupportUnloadAudio.cpp` was written, reached **85.16%**, and was
then **deleted rather than committed** (the revert is in this lane's diff), for two reasons that are both measurements. The source is
kept at `/tmp/opencode/UnloadAudio85.cpp` for whoever wants it; the shape is below so it does not
have to be re-derived.

Retail 0x8029EF20, 0xAC = 172 bytes, **unnamed** (`fn_8029EF20`), reached from
`~CGameArchitectureSupport` (main.cpp:259). The body is five calls and a loop:

| retail | what it is |
| --- | --- |
| `li r4,1` first, before the frame is set up, then `lwz r3,-25852(r13)` and `bl fn_80255C00` | read **.sbss 0x80419884**, pass it with a second argument of the literal 1, then set that global to 0 |
| `bl fn_8029EE78` | no arguments |
| `lis r3,0x8041 ; addi r3,r3,0x52DC` -> **0x804152DC**, then `bl fn_80340E08` | `.bss 0x804152DC`, 0x1834 bytes, passed by address and not otherwise touched |
| `lis r3,0x8041 ; addi r31,r3,0x3EFC` -> **0x80413EFC**; `addi r30,r31,4` -> 0x80413F00; `mr r29,r30` | the loop bound is the `int` at **0x80413EFC** and the records start at **0x80413F00**, **500 bytes** each (`addi r29,r29,500`) |
| loop body: `bl fn_80334C50(p)`, `clrlwi. r0,r3,24 / beq`, `bl fn_80334CB4(p)`, `clrlwi. r0,r3,24 / beq`, `bl fn_80335408(p)` | short-circuit: the third call only runs if the first two returned true, and its own result is discarded |
| `lwz r0,0(r31) ; mulli r0,r0,500 ; add r0,r30,r0 ; cmplw r29,r0 ; bne` | the loop is `for (p = base; p != base + count*500; p += 500)`, tested at the **bottom** after the first pass is set up by an unconditional jump to the test |

Note the addresses: **0x80413EFC and 0x80413F00, not 0x804191D8/0x804191DC.** `lis r3,-32703` reloads
0x80410000 before the `addi`, so the two are `0x80410000 + 0x3EFC` and `+4`, not additions onto the
previous `addi` result. Getting that wrong puts the table 0x5200 bytes out.

**Why it was not kept.**

1. **It cannot be linked as written, and the reason is general.** mwcceppc emits
   `lwz r3,offset(r13)` - small data - for a `extern "C"` object in `.bss`, without checking that the
   object is inside the ±32 KB window around `_SDA_BASE_ = 0x8041FD80`. These three are not:

   | object | offset from `_SDA_BASE_` | fits `int16`? |
   | --- | --- | --- |
   | `lbl_80419884` | -25852 | yes |
   | `lbl_804152DC` | -43684 | **no** |
   | `lbl_80413EFC` | -48772 | **no** |
   | `lbl_80413F00` | -48768 | **no** |

   Retail's own compiler used `lis`/`addi` for exactly those two. Written any way I tried, ours asks
   for the SDA form, which no linker can resolve - so the unit is a link failure, not a near-match.
   **See `docs/RUNNING_THE_DECOMP.md`, "mwcceppc emits small-data references it cannot resolve".**
2. **It is a bad trade even as `NonMatching`.** Closing
   `_ZN24CGameArchitectureSupport12UnloadAudioEv` would add **nine** names to the ratchet - the six
   `fn_*` callees plus the three globals - and the port would still not link, because
   `~CGameArchitectureSupport` calls them. The symbol was never closeable on its own.

The order that does work: write the six callees first, then the three globals (which
`src/MetroidPrime/PortGlobals.cpp` can define as zero fills, and which is three of the nine), and
only then this body - by which point the SDA problem has to be solved anyway, because the same three
objects are the problem.

## Row 12, `AllocateRenderer`: not attempted

Retail 0x8026EF54, 0x9C = 156 bytes, and the only one of the twelve whose retail name is already the
C++ one (`_Z16AllocateRendererR12IObjectStoreR10COsContextR10CMemorySysR8IFactory`). Read, not
written; the reason is budget, not a blocker.

1376 bytes (`li r3,1376` = 0x560) come out of `fn_80272958`, which is a refcounted
allocation wrapper - `bl` an allocate, `lwz r4,0(r3) ; +1 ; stw`, `bl` a construct. Then
`fn_80271238(r3, store, osContext, memorySys, resFactory)` is the object's constructor, the result
is stored to 0x80418998 (`lbl_80418998`, `.sdata`) **plus four** when it is non-null, and the
un-plus-four pointer is returned. The two arguments that look like file/line strings - 0x802FE3BC
and 0x802FE412, which is what `lis r7,0x803B ; addi r3,r7,-7236 ; addi r4,r3,86` builds - are **not
in `.rodata` at all**: both addresses are inside `.text`, and the bytes there are an unrolled lookup
table, not a string. That is unexplained, and whoever writes this should work out what those two
operands are before assuming they are `__FILE__`/`__LINE__`. Two undefined callees, no rename needed,
no `rc_ptr`; 0x80418998 is not `gpRender` (that is 0x804192F8, `.sbss`).

## Row 13, `CMain::ResetGameState`: not this lane's

Retail 0x80003A48, 0x1A0 = 416 bytes, `_ZN5CMain10ResetGameStateEv`, on the ratchet, and it belongs
to whichever lane owns `src/MetroidPrime/main.cpp`. **This lane did not touch `main.cpp`, and did not
touch anything under `include/MetroidPrime/Tweaks/`.** Nothing in the eleven above needs either file:
`CGameArchitectureSupport`'s constructor already calls `ResetGameState()` at main.cpp:242, and the
call will resolve as soon as the other lane defines it.

## The trap that cost this lane a full relink: a `Matching` unit may not own data

This is the finding most likely to save another lane an hour, so it is here rather than only in the
source comments.

**A `Matching` object's `.rodata`, `.sdata2` and `.data` are linked into the DOL, and any byte it
adds that `config/G2ME01/splits.txt` does not claim for it grows the section, moves everything above
it, and breaks the DOL's sha1 with every function in every unit still at 100%.**

It happened here for four bytes. `CStopwatch::CSWData::Initialize` was written first with a literal
`1.0f`; the object grew a 4-byte `.sdata2`. The linked ELF's section *sizes* were all still correct
(`.text` 0x3a1c54, `.rodata` 0xb530, `.data` 0x14e10) and every function still read 100%, but the DOL
grew by 32 bytes and `.sdata2` went 0x54C0 -> 0x54E0, which moved the BSS address. objdiff said
everything was fine; `dtk shasum -c` said `main.dol: FAILED`. The fix was to name retail's own
constant instead of writing the literal:

```cpp
extern "C" const float lbl_8041E258;   // .sdata2 0x8041E258, 0x3F800000 = 1.0f
...
x10_timerPeriod = lbl_8041E258 / static_cast< float >(x0_timerFreq);
```

which emits `lfs f0,-16744(r2)` against an `R_PPC_EMB_SDA21` relocation - byte-identical, because
the linker fills the offset in.

Two corollaries:

- **`unit_fit.sh` is not enough and `fast_try.sh` is not either.** Both report 100%. The only thing
  that catches this is the sha1, so run `./tools/gate.sh` (or at least `ninja build/G2ME01/main.dol
  && sha1sum build/G2ME01/main.dol`) before believing a new unit.
- **How to find the offending section in one step**, when the sha1 breaks and every function reads
  100%: `build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o | grep -E 'sdata2|rodata
  |data'`. Anything it prints that `splits.txt` does not claim for that unit is the cause.

The three constants this lane had to name rather than write are `lbl_8041E258` (1.0f),
`lbl_8041E260` (2^52 as a double) and `lbl_803A60A0` (the rodata blob `"MainFlow"` sits seven bytes
into). All three are defined with retail's values in `src/MetroidPrime/PortGlobals.cpp`, which
`configure.py` never claims - the right place, and the only place that is safe. **Undefined, they
would be zero fills in a PC link, and `lbl_8041E258` being 0.0f would make `GetElapsedTime()` return
0.0 for the whole game.**

One more thing the same trap implies: a `NonMatching` unit's data is *not* linked (the object is not
in the link at all), which is why `src/Kyoto/Basics/CStopwatchCSWDataWait.cpp` can claim a range and
still be safe. The same is why the weak template instantiations a `Matching` object emits past its
claimed range are harmless: dtk drops them, and the DOL's `.text` size is unchanged. They would not
be harmless in a unit whose range they fell inside.

**`unit_fit.sh` reports those weak instantiations as "extra functions", and for all five units it is
right and it does not matter.** Measured, and each is the benign COMDAT case the tool's own text
describes:

| unit | claimed `.text` | ours | "extra" |
| --- | --- | --- | --- |
| `MetroidPrime/CIOWinCtor` | 64 | 144 | `__dt__rstl::basic_string<char>`, 80 |
| `MetroidPrime/CMainFlowCtor` | 104 | 184 | the same, 80 |
| `MetroidPrime/CInputGeneratorCtor` | 112 | 220 | `__dt__rstl::single_ptr<IController>`, 108 |
| `MetroidPrime/CIOWinManagerCtor` | 140 | 560 | `__dt__CArchitectureQueue` 84, `__dt__IArchitectureMessageParm` 72, and the `rstl::list` / `rc_ptr` / vtable weak set |
| `Kyoto/Basics/CStopwatchCSWData` | 124 | 124 | none - `fits`, no extra functions |

They come from `rstl::basic_string`'s inline virtual destructor, `single_ptr`'s inline destructor and
`CArchitectureMessage`'s inline virtual destructor being instantiated because a `Matching` unit's
members use them. All five units pass `tools/flip_test.sh` -> `PASS -> kept as Matching`, which is
the only thing that decides, and the DOL's `.text` is 0x3a1c54 with our objects in the link.

## What moved when this was written

Measured, all of it:

- `matched 3115 -> 3121`, `linked 1725 -> 1731` - **+6 functions at 100%, 5 units newly linked**
  (`tools/report_diff.py build/report.base.json build/report.json`, and the same line in
  `tools/gate.sh`). The six are `CIOWinManager::CIOWinManager`, `CIOWinManager::~CIOWinManager`,
  `CStopwatch::CSWData::Initialize`, `CInputGenerator::CInputGenerator`, `CMainFlow::CMainFlow`, and
  `CIOWin::CIOWin` - the last of which was not on anybody's list.
- `CStopwatch::CSWData::Wait` reaches **69.16%** in a `NonMatching` unit. It does not count towards
  `linked`, and it is not claimed to.
- The port's link gap: **559 -> 553 MISSING** over 127 -> **133** objects (five new `mp_game` sources,
  so the object count rises too; only the MISSING count is the work list). Six ratchet symbols closed
  and **no new symbol**, because the three data constants were defined in `PortGlobals.cpp` rather
  than left missing. Closed: `_ZN10CStopwatch7CSWData10InitializeEv`, `_ZN13CIOWinManagerC1Ev`,
  `_ZN13CIOWinManagerD1Ev`, `_ZN15CInputGeneratorC1EP10COsContextff`, `_ZN9CMainFlowC1Ev`,
  `_ZNK10CStopwatch7CSWData4WaitEf`.
- `config/G2ME01/symbols.txt`: three renames, all of previously unreferenced symbols -
  `fn_80049E98` -> `__ct__6CIOWinFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>`,
  `lbl_803B1BA0` -> `__vt__6CIOWin`, `lbl_803B1770` -> `__vt__9CMainFlow`.
- `docs/HANDOFF.md`'s state block: 3115 -> 3121 matched, 1725 -> 1731 linked, DOL 2748 -> 2754.
- **The port still does not link and therefore does not boot.** No frame has been rendered, and
  none can be: the two null dereferences in `CGameArchitectureSupport`'s constructor are still
  there, `CGameArchitectureSupport::UnloadAudio` is still unwritten, and the port is still 553
  symbols short.

## What a next lane should do, in this order

1. **Do not start on row 7. Start on rows 8 and 10** - `PumpMessages` (196 bytes) and
   `CInputGenerator::Update` (508 bytes), 704 bytes together. Lane f1 changed `rstl::rc_ptr` to
   retail's 8-byte layout and neither of them calls `fn_80049010`, so both are unblocked today.
   `docs/research/rc_ptr.md` has both shapes and the measurement that says so.
2. **Row 9 (`AddIOWin`) is written at 95.24% and `NonMatching`, and row 7 (`RemoveAllIOWins`) at
   51.88%.** Each is one instruction from a wall that is *not* `rc_ptr`: `mwcceppc` materialises
   `operator new`'s file-string operand as `lis` plus **one** `addi` against relocations where
   retail's compiler emitted `lis` plus **two**. Until that is explained, no function that
   allocates can be `Matching`, and that is now the largest blocker on this list.
3. **`UnloadAudio` (row 11), but not first.** Its shape is written down and its source is at
   `/tmp/opencode/UnloadAudio85.cpp` at 85.16%. It cannot be linked until the small-data problem is
   solved (`RUNNING_THE_DECOMP.md`, "mwcceppc emits small-data references it cannot resolve"), and
   its six `fn_*` callees have to exist anyway, so write those first. There is a working copy at
   85% in the meantime; do not start from nothing.
4. `AllocateRenderer` (row 12), after working out what 0x802FE3BC and 0x802FE412 are.
5. `CInputGenerator::Update` (row 10, retail `fn_8001D888`, 0x8001D888, **0x1FC = 508 bytes - the
   largest single symbol on the list**; read but not attempted, and **unblocked** since the `rc_ptr`
   width became retail's - see `docs/research/rc_ptr.md`, which supersedes the "blocked for the same
   reason as rows 7-9" this paragraph used to carry). Its shape, so it does not have to be
   re-derived: it returns `bool`; it calls `fn_8028C058(this->x0_context)` and
   returns false if the low byte of the result is zero; it returns true if `x4_controller` is null;
   it then makes two virtual calls on the controller, **vtable words 3 and 4** (measured with
   `_SDA_BASE_ = 0x8041FD80`; the vtable is `[0]=0, [1]=0` then the virtuals, so word 3 is the first
   virtual and word 4 the second, and the *second* one's return value is the loop bound, i.e.
   `GetDeviceCount()`), and loops `for (i = 0; i < count; ++i)` doing
   `fn_80306BB0(&tmp, i, xc_leftDiv, x10_rightDiv, data)` -> `fn_80048CF4` -> `CArchitectureQueue::Push`
   when either `data[0]` or `data[1]` is set, and `fn_80048C08` -> `Push` when
   `x8_connectedControllers[i] != data[0]`, updating `x8_connectedControllers[i]` each time. It
   builds two `CArchitectureMessage`s through temporaries whose `rc_ptr` is AddRef'd and released -
   16 bytes each now that `rc_ptr` is 8, where this paragraph used to say 12.
6. Step 21c, the draw. `gpRender`'s vtable slot +0x94 is **not identified in either tree** - nothing
   in `config/G2ME01/symbols.txt` or `include/MetaRender/` names it, and `boot_path.md` records the
   same. `fn_80049244` (0x80049244, 0x118) is the other half of the draw path and is also unwritten.
   Until step 17's two null dereferences are fixed none of this is observable, so it is worth
   exactly nothing on its own: a loop that pumps input and messages and never draws is not a first
   frame.
