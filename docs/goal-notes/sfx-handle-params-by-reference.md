# sfx-handle-params-by-reference - Kyoto/Audio/CSfxManager (progress)

`item.json` reason: *"found by progress-unit-cplayerdynamics: Retail passes CSfxHandle"*, and the
`NEW:` line it came from claimed:

> Retail passes CSfxHandle *pointers* to CSfxManager::SfxStop and ::SetIgnoreAreaLowPass
> (`addi r3,r1,N` then `lwz r0,0(r3)` in the callee), so the by-value prototypes in
> CSfxManager.hpp are wrong; fixing them touches ~30 call sites and is what blocks the three
> gravity-boost functions in CPlayerDynamics.

**That claim is wrong, and the item was not blocked on it.** Measured here:

```
$ ./tools/dis.sh 0x8029E438 0x30          # SfxStop__11CSfxManagerF10CSfxHandle
8029e438: stwu r1,-16(r1) / mflr / stw r0,20(r1)
8029e444: addi r4,r1,8
8029e448: lwz  r0,0(r3)                   <- reads the handle *out of* r3
8029e450: stw  r0,8(r1)                   <- and stores it to a stack slot
8029e454: bl   8029de60 <StopSound...>
```

`lwz r0,0(r3)` is not a pointer argument - it is MWCC's **invisible-reference** calling convention
for a class passed by value, the same one already relied on throughout this repo
(`include/Kyoto/Audio/CSfxHandle.hpp:13-23` documents it for `CActor`). `SfxStop` is already at
**100%** in `build/report.json` with the by-value prototype, and so are `SfxStop(ESfxChannels,
CSfxHandle)`, `StopSound`, `SetIgnoreAreaLowPass` and every other `CSfxHandle` parameter in the
unit. Changing the prototypes would have broken ten matching functions to fix none, and the
"~30 call sites" are all consistent already. Verified with
`./.tmp/opencode/fdiff.sh SfxStop__11CSfxManagerF10CSfxHandle`: all 12 instructions identical,
ours at `33c8`.

The three gravity-boost functions in CPlayerDynamics (`StartGravityBoost` 1.12%,
`ApplyGravityBoost` 2.0%, `EndGravityBoost` 1.49%) are at ~1-2% - they are not blocked on an
argument-passing convention at all; they are unwritten. Nothing in
`src/MetroidPrime/Player/CPlayerDynamics.cpp` mentions `CSfxManager`, `CSfxHandle` or `SfxStop`.

## What I did instead: the unit's four unwritten functions

`build/report.json` listed 14 functions in this unit with no source at all, 12 of them unnamed in
`config/G2ME01/symbols.txt` (`fn_8029B81C`, `fn_8029B8E8`, eight 60-byte `fn_8029BA7C`-
`fn_8029BC20`, and `fn_8029FC34`/`FCE0`/`FD30`/`FDAC`). I wrote the last four - the
auxiliary-effect record lifecycle chain - from retail's bytes
(`./tools/dis.sh 0x8029FC34 0xAC` etc.). They call only `CMemory::Free` and each other, so they
need no new header types and no new symbols in the port.

`src/Kyoto/Audio/CSfxManager.cpp:40-118`, inserted between `CSfxChannel::CSfxChannel()` and
`CSfxEmitterWrapper::IsEmitter()` in retail-offset-descending order (`check_decl_order.py` is
unhappy about this unit for a **pre-existing** reason - see below - but not about these four).

Measured, per function, against `build/G2ME01/obj/Kyoto/Audio/CSfxManager.o`:

| function | retail | result |
| --- | --- | --- |
| `fn_8029FDAC` | 0x8029FDAC, 15 insns | **100%**, 15/15 instructions identical |
| `fn_8029FCE0` | 0x8029FCE0, 20 insns | **100%**, 20/20 instructions identical |
| `fn_8029FD30` | 0x8029FD30, 31 insns | mnemonics identical 31/31, registers differ |
| `fn_8029FC34` | 0x8029FC34, 43 insns | mnemonics identical 43/43, registers differ |

`build/report.json`, `main/Kyoto/Audio/CSfxManager`: **matched_functions 132 -> 134 / 159**,
`total_functions` 159 unchanged, `matched_data` unchanged at 4/380.

### The two loops, and what they are

Both `fn_8029FD30` and `fn_8029FC34` contain a loop nest whose *bodies are empty in retail's own
bytes*: `addi r3,r3,8 / bdnz` and `subf / mtctr / cmpw / bge / bdnz` with nothing between the
compare and the branch. They only advance a counter. Getting MWCC to emit them took a search over
the spelling, and the results are worth keeping:

- The unsigned-shift trip count `addi r0,r4,7 / srwi r0,r0,3 / mtctr r0` needs a **non-const**
  `unsigned` local and a *separate* `rest > 0` guard - but the guard must be spelled as the loop
  condition `for (; done < rest; done += 8)`, not as an enclosing `if`. With the `if` written
  separately, MWCC hoists the `mtctr` above the guard and the sequence is one instruction longer
  (43 -> 44 and 31 -> 32). The same `for` written as `while (done < rest) { done += 8; }` also
  works; `for (unsigned i = chunks; i > 0; ++i)` does **not** (MWCC unrolls it 8x and emits
  `addi r5,r5,64`).
- `rest` must be declared **outside** the `count > 8` test (`const int rest = count - 8;` before
  the `if`), or the `addi rX,rY,-8` lands after the branch instead of in retail's delay slot.
- Retail tests the **address** `record + 0x1810` for null (`addic. r0,r30,6160` then `beq`) and
  only then loads the count through it. Written as `(uint)record + 0x1810 != 0` that is a
  `cast from 'void*' to 'uint' loses precision` error in the 64-bit port build (caught by
  `tools/probe_sources.sh`); `(char*)record + 0x1810 != nullptr` compiles but changes the codegen
  (address held in r3, `lwz r3,0(r3)`). `(unsigned long)record + 0x1810 != 0` compiles everywhere
  and reproduces retail's `addic.`/`beq` exactly.
- `fn_8029FD30` and `fn_8029FC34` differ from each other in the second loop: retail's
  `fn_8029FD30` bumps `done` by one (`addi r3,r3,1`) and `fn_8029FC34` does not, so the two
  functions cannot share one spelling.
- The register allocation in both is not reproducible from any spelling I tried (~15 variants, see
  the wall below). The two functions that reach 100% use registers r3-r7 the same way retail does,
  so this is not a general codegen difference.

### `GetStudio` - a real bug, fixed, and it costs percentage

The item's premise led me to `CSfxManager::GetStudio(int)`, which was at **59.12%**. Retail:

```
8029b7d8: cmpwi r3,-1 / beq / lwz r0,mCurrentArea / cmpw / bne
8029b7ec: addi r4,r13,-25836        ; &mCurrentStudio is mCurrentArea - 25836... i.e. r4 = base
8029b7f0: addi r3,r2,-16608         ; &sStudios
8029b7f4: lbz  r0,4(r4)             ; mCurrentStudio
8029b7f8: lbzx r3,r3,r0             ; sStudios[mCurrentStudio]
8029b800: ... cntlzw r0,r0 / srwi r0,r0,5 / lbzx    ; sStudios[!mCurrentStudio]
```

The old source said `return studios[!mCurrentStudio];`. **`!` on a `bool` used as an array index
is not what it looks like.** As an `int` return, MWCC gives `cntlzw / srwi r0,r0,5`; as a byte
index it keeps the value in byte form - `cntlzw / rlwinm r0,r0,27,24,31` - which is **0 for
`false` and `0xf8000000` for `true`**. So the old spelling read `sStudios[0xf8000000]`, 4 GB past
a 2-byte array, whenever `mCurrentStudio` was true, and returned studio 0 instead of studio 1
when it was false. `sStudios[mCurrentStudio ? 0 : 1]` is the same intent and the same value, and
emits retail's `srwi r0,r0,5`. I confirmed the three spellings against the compiler directly
(`!b`, `b == 0`, `b ? 0 : 1` as an index vs as an `int`).

This **lowers** the function's fuzzy score, 59.12% -> 57.35%, because our object is now 15
instructions against retail's 17 and the extra `addi r4`/`lbz r0,4(r4)` pair (retail materialises
the `mCurrentStudio` base through `mCurrentArea`'s SDA pair, ours reads it directly) no longer
compensates. `tools/report_diff.py` reports it as a `WORSE` line but **does not fail** - it is in
a unit that was not `Matching` in the baseline, which the report explicitly allows
("The project rule is that a percentage on a NonMatching unit is a signal, not a result"), and
`tools/goal_check.sh` passed. I kept it: the old code was a wild read, and a correct function at
a lower percentage is the right trade. Flagging it here because it is the one line in my diff a
reviewer will ask about.

## Also changed: `docs/research/raw_offsets.md`

`fn_8029FC34` reaches the record list at `+0x1810` from a bare `void*`, which
`tools/check_raw_offsets.py` counts. Rule 4 of that file fails a new file with a raw offset, so I
added a `## src/Kyoto/Audio/CSfxManager.cpp (1 site)` section under **Kind A - opaque receivers**
(measured 167 sites in 71 files, all documented). Without it `gate.sh` fails on `raw-offsets`, and
with it the port probe passes: `748 files, 0 failed, 0 errors; link: LINKED (289 undefined, 0
duplicates)`.

## Verification

`./tools/goal_check.sh build/goal/item.json`, run in this worktree against the driver's own
baselines:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12132 -> 12134   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.28% fuzzy, 27.50% matched, 12.89% linked (12134 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 132 -> 134 / 159 functions
  ok    no asm added
goal_check: PASS sfx-handle-params-by-reference
```

No `asm`, no call added or removed anywhere: the unit's linked count is 5860 -> 5860, unchanged,
and the four functions are leaves over `CMemory::Free`.

## Walls, spelled out so the next run does not repeat them

WALL: fn_8029FD30 100% mnemonics / register allocation only - all 31 instructions are the right
opcode in the right order, but retail puts `count` in r6, the record pointer in r5, `i` in r8, the
loaded `n` in r7 and `done`/`rest` in r3/r4, where ours uses r4/r3/r5/r6/r7/r8. I tried ~15
spellings (listed in the notes above plus `const`/`non-const` locals, `uint` vs `int` for the
count, `rest` inside vs outside the guard, `for` vs `while` vs `do-while`, a `total` alias, a
`for (int i = done; ...)` induction variable). None moved the allocation.

WALL: fn_8029FC34 100% mnemonics / register allocation only - same 43 instructions in the same
order; ours holds `count` in r4 where retail holds it in r5, and the `lwz` reads `6160(r30)` in
both, so the address is right and only the destination register differs.

WALL: fn_8029FC34 43/43 mnemonics but the `count` register is r4 not r5 - depends on the same
allocator question; a `SRecordList` struct with a named member would not help, since the
allocation is decided before the load.

WALL: GetStudio 57.35% - the remaining gap is retail materialising `mCurrentStudio` through
`mCurrentArea`'s SDA base pair (`addi r4,r13,-25836` then `lbz r0,4(r4)`) where our object reads
`mCurrentStudio` directly. That is a **data layout** difference, not a source one: the two statics
are adjacent in retail's `.sdata2` and not in ours. Not a source spelling; it would need the
header's static order changed, which moves every later member.

## Things I did not do, and why

- **Did not change the `CSfxHandle` prototypes.** Ten functions in this unit already match at 100%
  with them; the `NEW:` line's reading of `lwz r0,0(r3)` as a pointer argument is wrong.
- **Did not write the other ten unwritten functions** (`fn_8029B81C`, `fn_8029B8E8`, the eight
  60-byte `fn_8029BA7C`-`fn_8029BC20`). Those call into `0x8033542c`-`0x8033593c`, the Echoes
  auxiliary-effect setters, and the eight are 60 bytes each with a 512-byte frame - a bigger job
  than this item, and the same shape as the four above, so it is the obvious next attempt.
- **`docs/HANDOFF.md` is untouched** in the final tree: `goal_check.sh` runs the gate with
  `MP_GATE_DOCS_WRITE=1`, which rewrote the state block (12132 -> 12134, DOL 10584 -> 10586); I
  reverted it, per "do not edit docs/HANDOFF.md".

## Not a blocker, but a pre-existing problem in this unit

`python3 tools/check_decl_order.py --unit Kyoto/Audio/CSfxManager` reports the unit **permuted**,
121 functions, and it did so before my change (117 then; the four I added are the difference).
The first four functions in our `.text` are in the wrong order: ours emits
`FAudioTranslationTableFactory`, `CFactoryFnReturn::CFactoryFnReturn`, `IsEmitter(CSfxWrapper)`
and `~CSfxWrapper` where retail has `IsEmitter(CSfxWrapper)`, `~CSfxWrapper`,
`FAudioTranslationTableFactory`, `CFactoryFnReturn::CFactoryFnReturn`. Those four are the last
four definitions in the file (lines ~1300+), so the whole file's declaration order is off by a
group. **This unit cannot be flipped until that is fixed**, which is worth knowing before anyone
spends a lane trying. It is already listed in `docs/research/decl_order.md`, so the gate accepts
it.
