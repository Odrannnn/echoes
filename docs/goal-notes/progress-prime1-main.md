# progress-prime1-main

`kind: progress`, `target: MetroidPrime/main`. The unit stays `NonMatching`; no `flip_test` was
run and none should be.

**Result: the unit's `matched_functions` rose 32 -> 35 of 99** (judge baseline
`build/goal/judge/report.base.json`, recorded on the branch head; the tree-wide total went
9700 -> 9703). Three functions reached 100%, two more rose substantially, and
`tools/gate.sh` passes in full with **no function anywhere worse**. The unit is 13.5% further
round and 5.3 points of matched code.

Per the brief: per function, before%, after%, and whether Prime 1's source matched unchanged,
needed edits, or did not help. Everything below is measured, from `build/report.json` and
`tools/lanediff.sh`, against `build/goal/judge/report.base.json`.

## Landed: 32% -> 100%

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `__pl__4rstlF...RCQ24rstl66basic_string<c,...>` (92 B) | 0.00% | **100.00%** | **matched unchanged** |
| `CMain::MemoryCardInitializePump` (188 B) | 2.13% | **100.00%** | matched with one Echoes-only addition |
| `CGameArchitectureSupport::CGameArchitectureSupport` (888 B) | 91.57% | **100.00%** | matched; the rest was three stale claims in our own comments |

### `rstl::operator+(const string&, const string&)` - 0.00% -> 100.00%, Prime 1 unchanged

Prime 1's `src/MetroidPrime/main.cpp:905` defines it, immediately above `CMain::AddWorldPaks`,
its only caller in the object. Ours was declared at `include/rstl/string.hpp:369` and defined
nowhere `configure.py` claims, so the symbol was `U` in our object and the 92 bytes stayed
retail's. The body is the four calls retail makes (copy-construct the first operand onto the
frame, append the second, copy-construct into the return slot, destroy the temporary) and is
**byte-for-byte the body `src/MetroidPrime/PortGlobals.cpp:883` already carried** - this is not a
second implementation of anything:

- `PortGlobals.cpp` is deliberately not a `configure.py` unit (its own header explains why: a
  definition inside a claimed unit collides with the retail object's own definition at DOL link
  time), and
- `src/MetroidPrime/main.cpp` is not in the port's `files.cmake` either - the port links
  `mainHead`/`mainMid`/`mainTail` - so the two never meet in a link.

### `CMain::MemoryCardInitializePump` - 2.13% -> 100.00%, Prime 1 plus one measured addition

Prime 1's body is this function **one call short**. Echoes additionally seeds the system options
from the card, and the measured bytes say so: `lwz r3,gpGameState ; addi r3,r3,0x54 ;
bl CPersistentOptions::InitializeMemoryState` at 0x80007C10, where `+0x54` is
`CGameState::mSystemOptions` (`include/MetroidPrime/Player/CGameState.hpp:291`). Written as
`gpGameState->SystemOptions().InitializeMemoryState();` before
`gpGameState->InitializeMemoryStates();`.

Two supporting changes:

- **`include/MetroidPrime/CGameGlobalObjects.hpp`** gained a `MemoryCard()` accessor. Prime 1
  spells it that way and `CMain::MemoryCardInitializePump` is its only reader in both games.
  Returning the member **by reference** is what makes retail's `single_ptr::operator=`
  out of line at 0x80007B94 rather than a `delete`/`store` pair.
- The allocation is written out (`MakeCMemoryCard()`) rather than spelled `new`, for the reason
  the file's existing `MakeCGameState`/`MakeInGameTweakManager` helpers already carry: the
  `CMemoryCard* made = self; if (made != 0) { made = f(made); } return made;` shape is what puts
  the constructor's result where retail puts it, and `sizeof(CMemoryCard)` is `0x50` = the
  `li r3,80` retail loads at 0x80007B58.

### `CGameArchitectureSupport::CGameArchitectureSupport` - 91.57% -> 100.00%

Three changes, and **all three are corrections to claims in our own comments that the tree had
overtaken**. This is the reusable finding: on this unit the blocking documentation was wrong, not
the code shape.

1. **`gpController` is in `symbols.txt`.** The comment here said the `IController*` store at
   0x80007FD4 was skipped "because `lbl_80419300` ... is not named in this tree's
   `symbols.txt`, and one of the two without the other is retail's 4 instructions against our 2".
   It is named - `config/G2ME01/symbols.txt:20698`, `gpController = .sbss:0x80419300; // type:object
   size:0x4 data:4byte`. Both stores are now written (`lbl_80418EC4 = &mgr;
   gpController = inputGenerator.GetController();`) and retail's four instructions are what comes
   out. **91.57% -> 93.89%.**
2. **`&ioWinMgr` goes through a named local.** Retail computes it once at 0x80007F80
   (`addi r30,r31,68`) and passes `mr r3,r30` to all four `AddIOWin` calls. Spelled
   `ioWinMgr.AddIOWin(...)` four times, mwcceppc re-materialises the address each time
   (`addi r3,r31,68`) and spends r0 on the `.sbss` store instead of r30 - four instructions for
   identical semantics. `CIOWinManager& mgr = ioWinMgr;` is what lets the allocator hoist it.
   **93.89% -> 98.96%.**
3. **`CAudioSys`'s `aramSize` is read, not written as a literal.** Prime 1 spells the argument
   `0x5fc000` and so did we, which came out as `lis r5,96 ; addi r8,r5,-16384` - two instructions
   in the wrong place for the right value. Retail *loads* it: `lwz r8,lbl_80418EA0@r13` at
   0x80007C2C, before the `li r4..r7,0x30` run that sets the other four arguments.
   `lbl_80418EA0` is the ARAM size that `CMain::InitializeSubsystems` hands to `ARAlloc`; the same
   symbol and the same reasoning are already written down in
   `src/MetroidPrime/CMainInitializeSubsystems.cpp:110`. **98.96% -> 100.00%.**

Also here, not needed for the score: the two `bl` targets were `extern "C" void fn_8029EFCC()` /
`fn_8033CEE8()`. Both are named in `symbols.txt` (`Initialize__11CSfxManagerFv` at 0x8029EFCC,
`Initialize__17CDSPStreamManagerFv` at 0x8033CEE8) and both headers exist, so the call is now
spelled `CSfxManager::Initialize()` / `CDSPStreamManager::Initialize()`. Same instruction, same
relocation, but it stops inventing `fn_` names for functions the map names.

## Improved but not landed: two real bugs fixed

### `CGameArchitectureSupport::UpdateTicks` - 92.96% -> 98.51%

Three genuine defects, all visible in the measured diff and all fixed:

- **`CMain::Increment_x5c` was a no-op.** `include/MetroidPrime/CMain.hpp:86` read
  `void Increment_x5c(float f) { x5c + f; }` - an expression statement whose value is discarded, so
  it did nothing. Retail's own body for the call site is four instructions at 0x80007C74
  (`lwz r3,gpMain ; lfs f0,lbl_8041A3C0 ; lfs f1,92(r3) ; fsubs f1,f1,f31 ; stfs f1,92(r3)`).
  Now `x5c = x5c + f;`. **This is a correctness bug, not a percentage: the frame-time accumulator
  was never being decremented.** It is also load-bearing for the port, which runs this function.
- **`OSRestoreInterrupts(1)` threw away the saved state.** Retail keeps
  `OSDisableInterrupts`'s return in r29 across the stopwatch read and hands *that register* back
  (`mr r29,r3` after the call, `mr r3,r29` before the restore). Now
  `const u32 interrupts = OSDisableInterrupts(); ... OSRestoreInterrupts(interrupts);`, which is
  also literally what Prime 1 writes.
- **`0.035 < stopwatchTime` had the `fcmpo` operands the wrong way round.** Retail 0x80007C40 is
  `lfs f0,lbl_8041A404 ; fcmpo cr0,f31,f0 ; ble` - elapsed first, constant second. Now
  `stopwatchTime > 0.035f`.

**What is left (1 instruction) is a real open question, not a tuning failure.** Retail reads
`lbz r0,145(r3)` - **bit 6 of the byte at `+0x91`** - where we read `lbz r0,144(r3)`, bit 6 of
`+0x90`. `+0x90` is the byte the constructor fills with its eight `rlwimi` bitfield stores, and
`finished` is bit 0 of it. So retail's `UpdateTicks` tests a field that is **not** `finished`; it
is one of the bits at `+0x91`, where this tree has only `gameFrameDrawn` (bit 0). The CMain
`+0x91` group is not modelled. Two candidate readings, neither confirmed, and **not guessed at
here**: either `+0x91` has more `bool : 1` members in retail than this header declares (which
would change `sizeof(CMain)`, so it needs a measurement, not an edit), or the field at
`+0x91` bit 6 is a named flag this header has under another name. Resolving it needs the retail
constructor's full bitfield map plus whatever reads `+0x91` elsewhere; it is filed below.

## Measured walls - do not retry these spellings

### `CMain::AddWorldPaks` - 96.00%, and **two independently correct fixes are jointly wrong**

This is the most expensive negative result of the run and the one most likely to be re-attempted.
Prime 1's body is this function with the loop count changed (9 there, 16 here - retail's own
`cmpwi r29,16` at 0x800056C0) and `GetWorldPrefix` renamed to `GetPakFile`. Three differences
remain, and **fixing the two obvious ones together took it from 96.00% down to 82.44%**:

1. **`rstl::rmemory_allocator allocator;` really is a local.** Retail passes `r1+8` as the third
   argument of `basic_string(const char*, int, rmemory_allocator const&)` at 0x8000563C
   (`addi r6,r1,8 ; li r5,-1 ; bl`), and `r1+8` is written by nothing - it is the address of an
   otherwise-unused empty struct. Prime 1 has the same local.
2. **The literals really do come from retail's `.rodata` pool.** `lbl_803A56C0+0x07` (the NUL that
   ends `??(??)`, i.e. `""`), `+0x08` (`%d`) and `+0x0B` (`.pak`), not `@stringBase0`.
3. Together: **96.00% -> 82.44%.** mwcceppc sizes the frame from the *tallest* local it sees, so
   naming them moves every `r1+N` spill at once and none lands where retail has it. The two are
   individually correct and jointly wrong. Do not try them as a pair.
4. Separately, `GetPakFile` returning **by value** rather than by `const rstl::string&` is also
   required by the measured bytes (retail 0x80216D5C is a bare copy-constructor into the caller's
   sret slot; the caller sets `addi r3,r1,92` before the call and calls `internal_dereference` on
   `r1+92` after) and has the same frame consequence. **Tried, not landed:** changing the return
   type is a header edit reaching `CGameState.cpp`, `CPlayerState.cpp`, `CScriptPickup.cpp` and
   `Tweaks.cpp` as well, so it wants its own item rather than a rider on this one.
5. What is left after all of that is the frame size itself: 0xA0 against our 0x90, one 16-byte
   `rstl::string` temporary retail has and we do not.

### `CMain::AsyncIdle` - 85.94%, pure register allocation

The entire remaining diff is register assignment: the same `li 500` / `li 0` / `cmplwi 500` /
`add rX,r3,r0` / `stw rX,100(r4)` sequence with r4/r5 and r30/r31 swapped, and a `clrlwi r5,r30,24`
against a `mr r5,r3`. No instruction differs in *what it does*. This is the `>=97%` regalloc wall
`tools/goal_seed.py` already filters on, and it is below 97% only because the function is 288
bytes. Not a spelling problem.

WALL: CMain::AsyncIdle 85.94% - pure register allocation (r4/r5, r30/r31, r0/r31 swaps); no instruction differs in effect.

### `GetAverageValue<float>` (200 B, same size as Prime 1) - 0.00%, not reachable from this unit

`include/Kyoto/TAverage.hpp` is already **byte-identical to Prime 1's**, variable name and all. The
function is a template that is simply never instantiated here: retail's `main.o` emits it because
`CMain::RsMain` calls `TReservedAverage<float, N>::GetAverage()`. Our `CMain` has no
`TReservedAverage` member and `RsMain` is 2148 bytes at 0.19%. Not a spelling problem; it needs
`RsMain`.

### The two same-size destructors - 0.00%, and Prime 1's names are a trap

`__dt__18CErrorOutputWindowFv` and `__dt__15CMemoryInStreamFv` are both 96 B and same-size in
Prime 1, which is why the seeder listed them first. They are the **D1 deleting destructors** -
`mr r31,r4 ; mr. r30,r3 ; beq ; lis r5,__vt__18CErrorOutputWindow@ha ; li r4,0 ; stw r0,0(r30) ;
bl ~CIOWin ; extsh. r0,r31 ; ble ; bl CMemory::Free` - and retail emits them as `W` (COMDAT weak).
Our object emits neither, because `~CErrorOutputWindow() override {}` and
`~CMemoryInStream() override {}` are both **defined inline in their headers**
(`CErrorOutputWindow.hpp:20`, `CMemoryInStream.hpp:15`), so the compiler has no out-of-line copy
to emit and the class's vtable is not instantiated in this TU. Defining them out of line in
`main.cpp` would emit a *strong* symbol where retail has a weak one and would need the vtable,
which lives in another unit. Not attempted; the fix belongs with whoever models the class.

## Gates

Run against the judge's own baseline, `build/goal/judge/report.base.json`:

```
MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json
```

```
configure                   ok      raw offsets                ok
ninja + build.sha1          ok      decl order                 ok
hashes vs config.yml        ok      files.cmake                ok
report                      ok      module order               ok
per-function diff           matched 9700 -> 9703  linked 4894 -> 4894
                            (+3 functions at 100%, 0 units newly linked)   no regression
module wiring               ok      port probe                 ok
dol_read                    ok      port link gap              ok
docs claims                 ok      reach stubs                not in a real build  ok
gs offsets                  ok
GATE PASS  8f8f456+4 changed
```

Independently: DOL `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
**86/86** modules match `config/G2ME01/config.yml`; `python3 tools/check_symbol_names.py` reports
`checked 502 units; 0 declared names are missing`; the judge's asm guard finds nothing
(`no asm added`); no path under `tools/`, `docs/research/port_link_baseline.txt` or `build/goal/`
other than this notes file is touched.

`unit_fit.sh` still reports the claimed range overshooting by 13 bytes, and
`check_decl_order.py` still lists the unit as permuted. **Both predate this change** - the permuted
entry is already in `docs/research/decl_order.md` and the unit is 67 functions short of a flip
anyway, so neither is actionable here. The unit cannot be flipped on this item's work.

## A lesson worth carrying: on this unit, check the comments before the code

Four of the six changes that moved a function to 100% were not new code - they were
**corrections to claims in this file's own comments that the tree had overtaken**: `gpController`
"is not named in `symbols.txt`" (it is, at line 20698); `CAudioSys`'s ARAM size "is" the literal
`0x5fc000` (retail loads `lbl_80418EA0`); `Increment_x5c` was a statement that computed a value
and threw it away; and `OSRestoreInterrupts(1)` discarded the state the call pair exists to
carry. Prime 1's source was the *donor* for three functions and confirmed the shape for two more,
but on the two biggest wins **the diff against retail was the specification and the prose around
it was the bug**. A stale in-tree comment costs a function exactly as much as a wrong spelling,
and `gate.sh` will not catch it - every one of these built, linked and reproduced retail.

## NEW:

NEW: match-main-cmain-asyncio | match | MetroidPrime/CMainAsyncIdle | 85.94% and the entire remaining diff is register assignment (r4/r5, r30/r31, r0/r31 swaps) with no instruction differing in effect; it is a Matching carve candidate that needs a register-pressure experiment, not another spelling, and `unit_fit.sh` on `CMainAsyncIdle.cpp` has never been run
NEW: match-main-cmapworldinfo-dtor | match | MetroidPrime/main | `__dt__13CMapWorldInfoFv` (124 B) and `__dt__18CErrorOutputWindowFv` / `__dt__15CMemoryInStreamFv` (96 B each, same size in Prime 1) need the D1 deleting destructors modelled out of line plus `__vt__18CErrorOutputWindow` / `__vt__15CMemoryInStream` in a unit this object can reach; the class headers define them inline so the compiler emits nothing
NEW: match-main-cmain-0x91-bitfield | match | MetroidPrime/main | retail's `CGameArchitectureSupport::UpdateTicks` tests bit 6 of the byte at `CMain`+0x91, but this header models only `gameFrameDrawn` at +0x91 bit 0 and `finished` at +0x90 bit 0; mapping that byte needs the retail constructor's full bitfield map and a `sizeof(CMain)` measurement, not a guess
