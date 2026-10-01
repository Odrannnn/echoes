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

---

# Run 3 (lane 6, 2026-10-02): `fn_8029B81C` at 100%, `fn_8029B8E8` at 93.88%, +1 function

Re-measured the clean tree first, and both earlier runs' work is already in it:
`main/Kyoto/Audio/CSfxManager` stood at **142 / 159** and the seventeen unmatched functions
were

```
fn_8029B8E8    0.00%  404 B      SetActiveAreas        0.38%  1060 B
fn_8029B81C    0.00%  204 B      Shutdown             30.05%   172 B
UpdateEmitter 99.00%   480 B      SfxVolume            96.99%   392 B
fn_8029FC34   98.95%   172 B      __sinit_CSfxManager  66.00%   468 B
__ct__...CBaseSfxWrapperFbs10CSfxHandlebi  98.61%  144 B
fn_8029FD30   95.65%   124 B      GetStudio            57.35%    68 B
AddListener   93.00%   456 B      Play(CSfxWrapper)    91.25%   304 B
UpdateListener 86.58%  144 B      Update               87.59%  2988 B
AddEmitter    82.74%   452 B      SfxStart             58.86%   404 B
```

So the item's premise (the `CSfxHandle` by-value prototypes) is still wrong - run 1 measured
that and nothing has changed since - and the unit's remaining work is the two auxiliary-effect
record helpers. I took both.

**Result: `fn_8029B81C` is at 100.00%, 51/51 instructions byte-identical. The unit went
142 -> 143 / 159** and `tools/goal_check.sh build/goal/item.json` printed `PASS`
(matched 12219 -> 12220, linked 5860 -> 5860 unchanged).
`src/Kyoto/Audio/CSfxManager.cpp` is the only file changed.

## `fn_8029B81C` - 51 instructions, 51 identical, first try

0x8029B81C, 0xCC bytes. It is a pure walk of the ten-slot auxiliary-effect record list with no
record copy in it, and it is a **free function, not a member** - retail's relocations (read out
of `build/G2ME01/obj/Kyoto/Audio/CSfxManager.o`, which is the *retail* object, not ours) show it
calling `SetAreaVolume__11CSfxManagerFiUc` with no `this` in r3, which is only possible because
`SetAreaVolume` is a `static` member. So `extern "C" void fn_8029B81C(int area)` calling
`CSfxManager::SetAreaVolume(...)` is the same call.

```cpp
extern "C" void fn_8029B81C(int area) {
  int* count = (int*)lbl_80413EFC;
  uchar* first = (uchar*)count + 4;
  for (uchar* it = first; it != first + *count * 500; it += 500) {
    if (fn_80334C50(it) && fn_80334C48(it) == area) {
      if (fn_80334CB4(it)) {
        fn_8034066C(lbl_804152DC, fn_80334C20(it));
        CSfxManager::SetAreaVolume(fn_80334CAC(it), 127);
      }
      fn_80334C5C(it);
    }
  }
}
```

Four things had to be right and each is measurable from retail's bytes alone:

- **The list base is `lbl_80413EFC`, and its size says what it is.** `config/G2ME01/symbols.txt`
  gives it `size:0x138C` = 5004 = **4 + 10 * 500**, i.e. `{ int count; SAuxRecord recs[10]; }`.
  That is the same 500 stride run 2 derived from the wrappers, and it is why the bound test is
  `it != first + *count * 500` and not a length the function was handed. The 10 is confirmed by
  `cmpwi r0,10 / blt` in `fn_8029B8E8`'s append path.
- **The bound is re-read every iteration.** Retail's `lwz r0,0(r31)` is *inside* the loop, so
  the bound is written as an expression rather than hoisted into a local; hoisting it removes an
  instruction.
- **The loop is a `for`, not a `while`.** Retail opens with an unconditional `b` to the
  condition (`8029b84c: b 8029b8b4`) and closes with `bne` back to the body - MWCC's shape for
  `for (init; cond; incr)` with the test at the bottom.
- **The two short-circuits are `&&`, and the third test is `if`, not part of them.** Retail
  branches `beq 8029b8a8` - *past* the `SetAreaVolume` pair but *into* the `fn_80334C5C` call -
  so `fn_80334C5C` runs whenever the first two tests pass, and the third only guards the body.
  Nesting it the other way round puts `fn_80334C5C` inside the third test and costs the call.

The eight callees are all in an **unclaimed gap** (`config/G2ME01/splits.txt` claims nothing in
0x80334C18-0x80334C5C, 0x8033541C or 0x8034066C-0x80340768), so they are `extern` declarations
whose types come from each callee's own eight bytes, tabulated in the source comment:
`fn_80334C50` = `lbz 0x12 / rlwinm 30` (bool), `fn_80334C48` = `lwz 8` (int),
`fn_80334CB4` = `lbz 0x12 / rlwinm 29` (bool), `fn_80334C20` = `lwz 0xC` (pointer),
`fn_80334CAC` = `lwz 4` (int). The data addresses are retail's own relocation targets
`lbl_80413EFC` and `lbl_804152DC`; the second is `0x804152DC`, not `0x804150DC` - the `lis`
field is `0x8041` and the `addi` immediate is `0x52DC` (21212), which is easy to mis-add.

## `fn_8029B8E8` - 93.88%, 101 instructions on both sides, and the copy is what is left

0x8029B8E8, 0x194 bytes, 101 instructions - the find-or-append the eight wrappers call. It went
**0.00% -> 93.88%** and does **not** count; 101 instructions on each side, every one the right
opcode, and the only differences are the *order* of five independent loads and five independent
stores in the record copy's prologue, plus which of r3/r6 each of them lands in. Retail:

```
lwz r3,0(r30) / li r0,60 / lwz r6,4(r30) / addi r5,r7,16 / stw r3,0(r7) / addi r4,r30,16
lwz r3,8(r30) / stw r6,4(r7) / lwz r6,12(r30) / stw r3,8(r7) / lwz r3,16(r30) / stw r6,12(r7)
stw r3,16(r7) / mtctr r0
```

**What the measurements changed, and they are worth more than the score:**

1. The record copy **must be a struct assignment**, `*(SAuxRecord*)slot = *record`. A hand-written
   `for (i = 0; i < 125; ++i) dst[i] = src[i]` is **55.07%** and 149 instructions: MWCC peels 5
   and then runs a *one*-word loop with an `addi r6,r6,24` induction variable. Split into an
   explicit 5 + a 2-word `for (i = 5; i < 125; i += 2)` is 55.07% too, and a per-member loop over
   a `uchar mTail[480]` is **56.94%**. Only the struct assignment produces retail's shape, and it
   took the function from 35.76% to 84.66% in one step.
2. `struct SAuxRecord` had to become `{ int mHead[5]; uchar mTail[480]; }` (run 2 deliberately
   left it one opaque `uchar[0x1F4]`). 500 = 20 + 480: five words copied individually, then a
   480-byte tail as a 60-iteration two-word `mtctr` loop. The single opaque array gave
   `li r0,62` and a four-byte tail remainder that retail does not have, because 500 is not a
   multiple of 8. The tail's element type is irrelevant - `uchar`, `uint[120]` and `short[240]`
   all give byte-identical objects and all score **93.88%**.
3. The count must be re-read **through the global** in the second half, not through the loop's
   pointer. Retail re-derives the address (`lis r3,0 / lwz r0,OFFSET(r3)`) where an
   `int& count = *(int*)lbl_80413EFC` reference keeps it in a register. Writing
   `const int n = lbl_80413EFC[0];` took it 89.97% -> 93.88% and also fixed a 3-cycle
   mis-allocation of `record`/`&count`/`first` across r29/r30/r31 in the prologue.
4. `int& id = lbl_80419880;` (a reference to the .sbss2 counter) is **wrong**: MWCC puts the
   address in a register and emits `stw r0,OFFSET(r30)` where retail has `stw r0,OFFSET(0)`.
   The global has to be named directly.

Fourteen spellings of the copy and of the layout were measured this run; the table is below.

| spelling | `fn_8029B8E8` |
| --- | --- |
| `*(SAuxRecord*)slot = *record` with `{int[5]; uchar[480]}` | **93.88%** |
| `SAuxRecord* dst = (SAuxRecord*)slot; *dst = *record;` | 93.88% |
| head as 5 named `int` members (`int mA, mB, ...`) | 95.51% (but the slot lands in r6, not r7) |
| head `uint[5]` / `short[240]` tail / nested head struct / `__attribute__((aligned(4)))` | 93.88% |
| `int& count = *(int*)lbl_80413EFC` (reference, not the global) | 89.97% |
| `int& id = lbl_80419880` | 35.76% |
| `memcpy(slot, record, 0x1F4)` | does not compile (no `memcpy` in this build) |
| `for (i = 0; i < 125; ++i) dst[i] = src[i]` | 55.07% |
| explicit 5 words + `for (i = 5; i < 125; i += 2)` | 55.07% |
| per-member: 5 head words + `for (i = 0; i < 480; ++i) mTail[i] = ...` | 56.94% |

## Walls, spelled out so the next run does not repeat them

WALL: fn_8029B8E8 93.88% - 101 instructions on both sides, all the right opcodes; the only
difference is the interleaving of five independent `lwz` and five independent `stw` in the
record copy's prologue and the r3/r6 choice between them. Fourteen spellings and four struct
layouts measured this run (table above); none moved the schedule. The *shape* is solved, so a
next run should look at MWCC's scheduler priorities, not at the loop or the list.

WALL: __ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi 98.61% - 36 instructions on both
sides and every store offset and every `rlwimi` shift already matches; the only difference is
which registers hold the three constants. Retail: `0 -> r11`, `8192 -> r10`, `1 -> r9`, i.e. a
contiguous triple. Ours: `0 -> r10`, `8192 -> r9`, `1 -> r6`, i.e. the window starts one lower
and the third constant falls out of the constant registers entirely. **I reproduced this in a
2-second standalone probe** (`.tmp/opencode/probe_ctor.cpp` + `pc.sh` in this run's scratch, a
~40-line copy of the class and ctor that compiles to the identical 36 instructions) and it is a
real finding for the next run, because **the window's top is not a source property**:

| probe variant | constants land in |
| --- | --- |
| the mem-init list as the repo ships it | `r10, r9, r6` |
| mem-init list reordered (`pb` before `r`) | `r10, r9, r6` (unchanged) |
| bitfields set in the body, not the mem-init list | `r10, r9, r6` (unchanged) |
| bitfield init order reversed | `r10, r9, r6` (unchanged) |
| `short`/`ushort` casts on the literals | `r10, r9, r6` (unchanged) |
| everything in the body, no mem-init list | `r10, r9, r6` (unchanged) |
| **one extra member initialised with a 4th constant** | **`r11, r10, r6`, 4th in `r0`** |
| **one dead `if` in the body** | **`r11, r10, r6`** |
| **one bitfield initialised from `area != 12345`** | **`r12, r11, r6`**, the bool in `r9` |

So the window top tracks register pressure (r10 / r11 / r12 in the three cases above) and the
third constant consistently lands in `r6` - retail needs a **three**-wide constant window. Nobody
has yet found a source-level way to widen it; the probe is the cheap place to keep looking.
Do not spend a full unit build on this one: the probe is a ~2-second loop.

## Verification

`./tools/goal_check.sh build/goal/item.json`, run in this worktree against the driver's own
baselines, on the final tree with `docs/HANDOFF.md` reverted:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12219 -> 12220   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.50% fuzzy, 27.83% matched, 12.89% linked (12220 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 142 -> 143 / 159 functions
  ok    no asm added
goal_check: PASS sfx-handle-params-by-reference
```

- `tools/dol_fd.py Kyoto/Audio/CSfxManager fn_8029B81C` -> `51 retail insns, 51 ours,
  0 differing lines`; `fn_8029B8E8` -> `101 retail insns, 101 ours, 23 differing lines`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py` -> 0 declared names missing, 525 units.
- `python3 tools/check_raw_offsets.py` -> **ok: 167 raw-offset site(s) in 71 file(s)**, unchanged:
  the `500` and the `+ 4` here are an array stride and the head of the list, not offsets into a
  pointer, so `docs/research/raw_offsets.md` needed no new section.
- **The port build is untouched, measured rather than asserted.** Every line of this change is
  inside the file's existing `#ifndef TARGET_PC` guard. Built `build-port-link`'s
  `CSfxManager.cpp.o` from the clean source and from the changed source: identical `.text` size
  (`0x42d1`), identical section table, and `nm -u | sort | md5sum` is
  `32b8c017677527f93a5e79d74cb63f1d` for both. (The files' sha1s differ only by an 8-byte shift
  in a DWARF ref section - compiler debug-info noise, not code.)
  `python3 tools/link_gap.py` -> **282 MISSING, all accounted for**. Run 2 recorded 284; that is
  a different tree state, not a regression - see the object comparison above.
- `tools/unit_fit.sh Kyoto/Audio/CSfxManager.cpp` -> 12 functions present in ours but not the
  retail object, 1576 bytes, and 240 bytes over the claimed range - **both identical to the
  clean tree** (measured by restoring `HEAD`'s source and re-running), so this change adds no
  extra functions. Run 2's note of "20 bytes over" no longer matches this tree; 240 is what it
  measures on both.
- `python3 tools/check_decl_order.py --unit Kyoto/Audio/CSfxManager` still reports the unit
  permuted, which is the **pre-existing** condition run 1 documented (the whole file's
  declaration order is off by a group; already listed in `docs/research/decl_order.md`, so the
  gate accepts it). The output is byte-identical before and after this change. The two new
  definitions are placed *after* the eight wrappers, which is the correct descending-retail-offset
  order relative to them, so the local group got no worse.
- `docs/HANDOFF.md` is **untouched** in the final tree. `goal_check.sh` runs the gate with
  `MP_GATE_DOCS_WRITE=1`, which rewrote the state block; I reverted it and re-ran the whole judge
  on the reverted tree to confirm the PASS does not depend on it.

## Still unwritten, and what it would take

- **`SetActiveAreas` (0.38%, 1060 B) is the biggest thing left in this unit** and it is now much
  more approachable: the list walk, the record's own id (`fn_80334CAC`), its parameter block
  (`fn_80334C20`), its flags (`fn_80334C50` / `fn_80334CB4` / `fn_80334C30`) and the
  `fn_8034066C(lbl_804152DC, ...)` reset all have measured declarations in this file now, and
  `CSfxManager::SetAreaVolume(int, uchar)` is a real static member. Its disassembly is
  `0x8029C378`, 0x424 bytes, and it contains two copies of the record walk with different
  predicates (`fn_80334C50` / `fn_80334CB4` / `fn_80334CAC`, and `fn_80334C50` / `fn_80334CAC` /
  `fn_80334C30`).
- **`Shutdown` (30.05%, 172 B) is missing real work, not percent**: 43 retail instructions against
  our 13, and the absent tail is the same 500-stride record walk (`fn_80334C50` /
  `fn_80334CB4` / `fn_80335408`) that `fn_8029B81C` now does, so the walk exists to copy.
- **`UpdateEmitter` 99.00% and `SfxVolume` 96.99% are pure register allocation.** Retail puts the
  `handle` parameter in `r31` and the wrapper pointer in `r30`; ours has them the other way round
  (`UpdateEmitter`). `SfxVolume`'s 61 differing lines are the same r29/r30/r31 shuffle across
  four variables. Run 2 measured ~8 spellings of `UpdateEmitter`; neither of these has been
  attacked with the **pressure** lever that the constructor probe above turned out to respond to.
- **`GetStudio` 57.35%** is still the data-layout wall run 1 measured (retail materialises
  `mCurrentStudio` through `mCurrentArea`'s SDA base pair; ours reads it directly).
