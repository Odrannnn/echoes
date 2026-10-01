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

---

# Run 2 (lane 5, 2026-10-02): the eight `fn_8029BAxx` wrappers, +8 functions

Re-measured the clean tree first. The previous run's four functions were in: `main/Kyoto/Audio/
CSfxManager` stood at **134 / 159**, and the eleven still-unmatched functions were

```
fn_8029BC20/BBE4/BBA8/BB6C/BB30/BAF4/BAB8/BA7C   0.00%   60 bytes each
fn_8029B8E8                                        0.00%  404
fn_8029B81C                                        0.00%  204
SetActiveAreas                                    0.38% 1060
```

I took the **eight 60-byte wrappers**, which the previous run had listed as "the obvious next
attempt" and declined as "a bigger job than this item". They are not a bigger job: all eight are
the same 15 instructions and differ only in the constructor they call.

**Result: all eight at 100.00%, 15/15 instructions byte-identical. The unit went 134 -> 142 / 159
and `tools/goal_check.sh build/goal/item.json` printed `PASS`.** `src/Kyoto/Audio/CSfxManager.cpp`
is the only file changed; no call was added or removed anywhere, so the port's undefined count is
286 -> 286 and its `MISSING` gap is 284 -> 284 (measured with `tools/link_gap.py`).

## What they are, from the bytes

```
fn_8029BA7C -> fn_8033542C    fn_8029BB6C -> fn_803356BC
fn_8029BAB8 -> fn_803354AC    fn_8029BBA8 -> fn_803357F4
fn_8029BAF4 -> fn_8033555C    fn_8029BBE4 -> fn_803358A8
fn_8029BB30 -> fn_803355FC    fn_8029BC20 -> fn_8033593C
```

Each builds a 0x1F4-byte record in a 512-byte frame, hands it to that effect type's ctor, and
passes the ctor's **return value** to `fn_8029B8E8`, which finds or appends a slot for it. `0x1F4`
is not a guess: it is the same stride `fn_8029B8E8` and `Shutdown` walk the record list with
(`mulli r0,r0,500` / `addi r27,r27,500`), which is what fixes the record's size.

## The one thing that made them match: the ctor's return value

**Retail's second `bl` never reloads the record's address - it reuses the `r3` the ctor left
behind.** MWCC keeps a call's result in `r3`, and every one of the eight ctors ends in
`mr r3,r31` where `r31` is its own `this`. So the source has to be the ctor call *nested inside*
the argument list of `fn_8029B8E8`:

```cpp
extern "C" int fn_8029BA7C(void* a, void* b, void* c, void* d) {
  SAuxRecord rec;
  return fn_8029B8E8(fn_8033542C(&rec, b, a, c, d));   // 100.00%, 15/15 identical
}
```

Two spellings of the same thing were measured, and both are **93.33%** (14 of 15) - one extra
`addi r3,r1,8` before the second call:

| spelling | score |
| --- | --- |
| `return fn_8029B8E8(ctor(&rec, b, a, c, d));` (nested) | **100.00%**, 15/15 identical |
| `fn_8029B8E8(&rec);` after a statement-form ctor call | 93.33% |
| `SAuxRecord* self = ctor(&rec, ...); return fn_8029B8E8(self);` | 93.33% |

So this is not a register-allocation wall and not a spelling hunt: the three forms are the same
values in the same order, and only the nested one lets MWCC see that the address it already has
in `r3` is the one the second call needs. Worth remembering as a shape - **a ctor that returns
`this`, called for its side effect and then passed along, is a one-instruction difference.**

## The two guards that keep the port and the data sections unchanged

- **`#ifndef TARGET_PC` around the whole block.** The port never constructs the auxiliary-effect
  manager (the file's own `Initialize` says so), nothing in src/ calls these eight, and the eight
  ctors plus `fn_8029B8E8` are retail DOL code with no host implementation. Without the guard the
  port object gains **nine** undefined symbols that no port code can reach; with it the port link
  is bit-for-bit what it was. This is the pattern the file already uses at its `Initialize`,
  `KillAll` and `Update` (lines 433, 708, 1041).
- **`SAuxRecord` is a file-local `uchar[0x1F4]` in an anonymous namespace**, not a named layout.
  Nothing in this file reads a field, and a struct with invented members would be a claim about a
  type no header declares. It also keeps `tools/check_raw_offsets.py` at its documented
  **167 sites in 71 files** - the 0x1F4 is an array *size*, not an offset into a pointer.

The eight are written through a `CSFXMANAGER_AUX_WRAPPER(name, ctor)` macro, `#undef`'d after use,
so the eight bodies cannot drift apart - they are the same function by construction, which is also
what the bytes say.

## Verification

```
tools/dol_fd.py Kyoto/Audio/CSfxManager fn_8029BA7C ... fn_8029BC20
  == fn_8029BA7C (15 retail insns, 15 ours, 0 differing lines)      ... and so on for all eight

./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12186 -> 12194   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.43% fuzzy, 27.69% matched, 12.89% linked (12194 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 134 -> 142 / 159 functions
  ok    no asm added
goal_check: PASS sfx-handle-params-by-reference
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `check_symbol_names.py`
0 missing over 525 units; `check_raw_offsets.py` ok; `link_gap.py` 284 MISSING, all accounted for.
`unit_fit.sh` reports **12 functions present in ours but not the retail object, 1576 bytes** and
**".text" 20 bytes over the claimed range** - *byte-for-byte the same as the clean tree* (measured
by stashing), so this change adds no extra functions. `check_decl_order.py` still reports the unit
permuted, which is the **pre-existing** condition the first run documented (the whole file's
declaration order is off by a group; this unit cannot flip until that is fixed, and it is already
listed in `docs/research/decl_order.md`).

`docs/HANDOFF.md` is **untouched** in the final tree. `goal_check.sh` runs the gate with
`MP_GATE_DOCS_WRITE=1`, which rewrote the state block (12186 -> 12194, DOL 10638 -> 10646); I
reverted it, per "do not edit docs/HANDOFF.md", and re-ran the whole judge on the reverted tree to
confirm the PASS does not depend on it.

## Still unwritten, and what it would take

- **`fn_8029B8E8` (0x8029B8E8, 404 bytes, 101 instructions) - the shared callee of the eight, and
  now the obvious next item for this unit.** It is a find-or-append over the 500-stride record
  list with a six-call epilogue (`fn_80334D8C`, `fn_80334C98`, `fn_8033541C`, `fn_80334C40`,
  `fn_80334CAC`, and a `fn_80340768`/`fn_80334C18` pair behind a `cmpwi r3,-1`). Two details are
  already measured and are why it is a real job rather than a transcription: the record copy is
  **five explicit 4-byte loads/stores plus a 60-iteration `mtctr` loop** (retail unrolls it, so
  `addi r5,r7,16` / `mtctr 60` / `lwz 4(r4)` / `lwzu 8(r4)` / `stw 4(r5)` / `stwu 8(r5)` has to
  come out of a counted copy, not a `memcpy`), and the tail calls eight functions **no unit in
  `splits.txt` claims** - they are in an unclaimed gap, so each needs an `extern` whose only
  justification is retail's own bytes.
- **`fn_8029B81C` (0x8029B81C, 204 bytes)** is the same record-list walk with a different
  predicate pair (`fn_80334C50` then `fn_80334CB4`) and a `SetAreaVolume` call at the end, so
  whoever writes `fn_8029B8E8` has the walk already written and this is then a small delta.
- **The register-allocation near-misses are still where the previous run left them.** I spent part
  of this run on them and measured, none of it moved: `UpdateEmitter` 99.00% (baseline 99.00;
  `int` temp 95.83, `ushort` temp 97.96, a named emitter reference 78.43, floor-first 91.08,
  two stores 89.92, `CGX::SetDstAlpha`'s assign-the-widened-local-back 98.83), `fn_8029FC34`
  98.95% and `fn_8029FD30` 95.65% (the previous run's walls, untouched here), and
  `__ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi` 98.61%, where **retail uses r10/r11 for
  the two constant materialisations and we use r9/r10** - a one-register shift in the constructor's
  bitfield initialisers, which is a different wall from the two above and is **not** yet tried.
- **`Shutdown` is missing real work, not percent**: 43 retail instructions against our 13. The
  absent tail is the record-list walk (`fn_80334C50` / `fn_80334CB4` / `fn_80335408`, stride 500)
  that `fn_8029B8E8` also does, so it becomes writable once the list type exists.
