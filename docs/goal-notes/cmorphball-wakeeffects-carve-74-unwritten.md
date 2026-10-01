# cmorphball-wakeeffects-carve-74-unwritten (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-10-01. **Result: `goal_check` PARTIAL** - the unit's matched count rose
**97 -> 100 of 158**, every other check green, no asm, no judge-owned path touched. The flip
still fails on the same two **unwritten** symbols (`CElementGen::GetEmitterTime() const`,
`fn_800CD4B8`), unchanged by this diff. **The carve this item asks for is not the route** - see
"The carve cannot flip this unit" below, measured. What landed instead is three functions at
100% and the measurement that says what the route actually is.

## What I changed

One file, `src/MetroidPrime/Player/CMorphBall.cpp`, three hunks. No `configure.py`, no
`config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm, nothing under `tools/` or
`build/goal/`.

1. **`:1074-1084` - `SpinToSpeed`** (retail 0x800C19A4, 192 B, **99.6875 -> 100.00**). The
   scalar product is now a named local: `const float scale = (speed - angularSpeed) * dt;`
   then `ApplyTorqueWR(scale * direction)`.
2. **`:1679-1699` - `TransformSpiderBallForcesXZ`** (retail 0x800CE258, 168 B,
   **99.71429 -> 100.00**) and **`TransformSpiderBallForcesXY`** (0x800CE300, 168 B, same).
   The `row0 * fx + row1 * fy` sum is now a named local `const CVector3f res = ...; return res;`.

Nothing else moved: no string literal added, no member touched, the unit's `.rodata`/`.sdata2`
layouts are identical to HEAD (`create_ballshadow`/`InitializeWakeEffects`/... still sit at
99.73-99.99% on the same `addi` offsets they did before, i.e. the known
`cmorphball-loadmorphballmodel` `.rodata` string pool, untouched here).

## The carve cannot flip this unit - the load-bearing measurement

The item (from `cmorphball-wakeeffects-outofline-resize`'s `NEW:` line) proposes carving the
unwritten functions into their own unit so the retained half could reach `Matching`. That does
not work here, and the reason is measurable rather than a matter of taste:

**A carve can only split a unit into *contiguous* ranges, and this unit's matched code is in
31 separate runs.** Retail's `.text` for the unit is 0x800C02A4..0x800D06CC (66600 B, 158
functions, and *no* gaps between them - every byte belongs to a function, verified by walking
`nm -S` and finding zero uncovered bytes). Walking the 158 functions in address order and
marking each `matched_functions == 100` or not gives **31 maximal matched runs totalling 11628
B, separated by 30 unwritten runs totalling 54972 B**. The longest matched run is 2124 B; the
median is ~150 B. So isolating the written code would need **31 carve boundaries (32 units)**
in one change, not one - and even then every retained half would have to be declared descending
by retail offset, which this unit cannot be (see below). **A carve does not reduce the amount of
code that has to be written; it only relabels it.**

The real state of the unit, re-measured on the clean tree before I touched anything
(`build/report.json` at HEAD, unit `main/MetroidPrime/Player/CMorphBall`):

- `matched_functions` **97 / 158**, `matched_code` **11628 / 66600** (17.46%),
  `.text` fuzzy 28.626066%.
- **61 functions below 100%**, covering **54972 of the unit's 66600 bytes**. The previous run's
  "74 unwritten" is now **61**: 13 of them have since been written or partly written.
- **11 functions have no body in our object at all** (1724 B): `GetEmitterTime__11CElementGenCFv`
  (8 B, 0x800CA558), `fn_800C9380` (48), `fn_800C93B0` (72), `fn_800CD460` (88), `fn_800C33DC`
  (92), `fn_800C88C0` (92), `fn_800CD4B8` (152), `fn_800D0584` (188), `fn_800CD35C` (260),
  `fn_800CD244` (280), `fn_800C5420` (444). `nm -S --defined-only` on
  `build/G2ME01/src/.../CMorphBall.o` finds none of the eleven.
- the biggest unwritten bodies are `UpdateEffects` (4448 B, 0.090% - ours is a 4-byte stub),
  `Render` (4024, 0.099), `__ct__CMorphBall(CPlayer*, bool, float)` (3956, 96.265),
  `CollidedWith` (3940, 0.102), `ComputeBoostBallMovement` (2812, 0.142).
- **`tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still says "would
  break on a flip"** (146 of 158 functions in the wrong order), which is the same at HEAD -
  pre-existing, and listed in `docs/research/decl_order.md:101`. So even a byte-perfect set of
  bodies would not flip until the unit's declaration order is rebuilt descending by retail
  offset. **That is the real blocker after the 54972 unwritten bytes, not the carve.**

The flip failure is also **independent of this diff**: with the unit `Matching`, `configure.py`
links `build/G2ME01/obj/.../CMorphBall.o` instead of compiling the source, so the three bodies
above are not even in that link. `mwldeppc` reports `undefined: 'CElementGen::GetEmitterTime()
const'` and `'fn_800CD4B8'` - both in the unwritten-eleven list - exactly as the previous run
recorded.

**Conclusion for the queue: this unit is a `progress`-shaped unit with ~55 kB to write, not a
carve.** Nothing in the four-file carve protocol can substitute for writing the bodies.

## Two codegen rules this established (both measured)

- **A returned 12-byte struct needs its value in a *named* local.** Written straight from the
  expression, mwcceppc emits `lwz r31,60(r1)` / `lwz r0,68(r1)` / `lwz r30,56(r1)` in the
  epilogue; retail emits `lwz r31,60` / `lwz r30,56` / `lwz r0,68`. The arithmetic is identical
  - it is the named local that moves the last use of `r30` after the link register's, so the
  reloads come out in retail's order. Same shape as the `angularVelocity` finding already in
  `SpinToSpeed`'s comment; this is the return-position case.
- **A `CVector3f * float` product needs its scalar factor in a named local.** Left inline
  (`(speed - angularSpeed) * dt * direction`), mwcceppc allocates the three components
  f2/f1/f0 in x,y,z order and stores them x,y,z (`lfs f2,0` / `lfs f1,4` / `lfs f0,8`, then
  `stfs` to +8/+12/+16). Retail loads `.y, .z, .x` into f2/f1/f0 and stores `.y, .x, .z` - the
  same three `fmuls`, a different allocation. Naming the product fixes it; **both**
  `direction * scale` and `scale * direction` reach 100%, measured.

## Spellings tried and rejected, so the next run does not repeat them

All on this unit, each built and scored with `objdiff-cli report`:

- `ComputeMaxSpeed` (retail 0x800C190C, 152 B, **96.8421**, wall). Retail's `min_val` keeps the
  clamped value in **f2** and reloads `95.f` into **f0**; ours keeps it in f2 and puts `95.f` in
  **f1** (`fcmpo cr0,f1,f2` vs retail's `fcmpo cr0,f2,f0`) - one register number, and the
  `bl`/`fmr` shape matches. Tried: `min_val(maxSpeed,95.f)` 96.8421, `min_val(95.f,maxSpeed)`
  94.07895, `maxSpeed < 95.f ? maxSpeed : 95.f` 94.3421, `maxSpeed >= 95.f ? 95.f : maxSpeed`
  94.210526, `if (maxSpeed > 95.f) maxSpeed = 95.f;` 96.8421, named `kMaxSpeed` 96.8421, named
  `minSpeed` 96.8421, the whole thing as one nested `min_val(max_val(...),95.f)` 96.8421, no
  named local at all 96.8421. **Nothing moves the 95.0 constant out of f1.**
- `GetSpiderBallControllerMovement` (retail 0x800CC758, 324 B, 94.50617 -> 94.62963, wall).
  Retail's `atan2` is `bl 0x80352C70` with **f1 = f30 = `forwardMinusBackward`** and
  **f2 = f31 = `turnRightMinusLeft`** - i.e. retail passes (forward, turn), not (turn, forward).
  Swapping the two arguments gains 0.12% and nothing else; the remaining gap is the `-55`/`145`
  tail layout (retail `blt`/`ble` forward into a shared `fneg`; ours inverts to `bge` +
  `cror eq,lt,eq`). Tried: `if/else if` 94.62963, nested `if (angle >= -55.f) { if (angle <=
  145.f) }` 92.28395, `angle >= -55.f && angle <= 145.f` 92.40741, nested ternary 94.62963,
  named `negMagnitude` 86.358025.
- `GetRenderBounds` (retail 0x800C22F8, 384 B, **96.479164**, wall). Retail computes
  `fabs(frsp((1/255) * alpha - 0.f)) < 1e-05f` - the `.sdata2` words read back as 0.003921569
  (1/255, 0x8041B38C), 0.0 (0x8041B308) and 1e-05 (0x8041B390), via `tools/sda.py s2:-28724`
  etc. Ours folds the `x - 0.f` away and has no `fsubs`. Tried: named `alpha` local, named
  `zero` local, `CAABox& b = bounds` for retail's `r31 = r1+72`, `diff` local - all exactly
  96.479164; `GetAlphau8() * (1.f/255.f)` 92.572914. **MWCC folds `x - 0.f` at `-O4,p` here**
  and no spelling of the source prevents it.
- `fn_800C8CE0` (`erase(iterator)`, retail 0x800C8CE0, 76 B, **74.052635**, wall). Retail spills
  **three** slots: `stw r7,8(r1)`, `stw r7,12(r1)`, `stw r0,16(r1)` with `r7 = *it + 8` - i.e.
  `last` twice and `first` once - and passes `r5 = r1+16` (`&first`), `r6 = r1+12` (`&last`).
  We spill two. Tried: passing `&lastCopy` instead of `&last` 71.947365, `first` declared before
  `last` and `last = first + 1` 71.947365, `lastPtr = &last` 74.052635, two copies 74.052635,
  dropping the dead local entirely 74.052635. **No source spelling produces the third spill.**
- Also measured and left alone: `DampLinearAndAngularVelocities` (0x800C5614, 256 B, 57.266 -
  ours reorders the two `ApplyForceWR`-shaped calls around the `Magnitude()` multiply),
  `IsMovementAllowed` (0x800CE7D0, 148 B, 3.784 - ours is an 8-byte `return false` stub; see
  the `NEW:` line, its body is fully readable and I transcribed it here so nobody re-derives it),
  and the four `.rodata`-pool functions already owned by `cmorphball-loadmorphballmodel`
  (`CreateBallShadow` 99.968, `UpdateIceBreakEffect` 99.991, `UpdateMorphBallTransitionFlash`
  99.991, `InitializeWakeEffects` 99.729 - all four differ only in `addi r4,r3,378` vs our
  `addi r4,r3,682`, i.e. one `.rodata` pool offset, 0x130 apart).

`IsMovementAllowed`'s body, for the next lane: `lwz r3,0(r3)` (mPlayer) /
`bl 0x8000BF7C GetTweakPlayerControls__7CPlayerCFv` / `bl 0x80215860 fn_80215860` /
`clrlwi. r0,r3,24` / `bne +0x4c`; then `lwz r3,0(r31)` / `lbz r0,1521(r3)` /
`cmplwi r0,0` / `bne <return false>` / `lbz r0,1522(r3)` / `cmplwi r0,0` / `beq +0x4c`;
both paths converge on `lwz r3,0(r31)` / `bl 0x80019DF8 IsMorphBallTransitioning__7CPlayerCFv` /
`clrlwi. r0,r3,24` / `beq +0x64` / `li r3,0` / `b +0x80`; and `lfs f1,6240(r31)` /
`lfs f0,0(0)` (0x8041B308 = 0.0f) / `fcmpo` / `mfcr` / `rlwinm r0,r0,2,31,31` / `cntlzw` /
`srwi r3,r0,5` - the standard MWCC float-to-bool, so the return is
`this->x1860 != 0.f`, i.e. `mPlayer.x5F1 == 0 && mPlayer.x5F2 != 0` must hold first.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11406 -> 11409   linked 5514 -> 5514
  ok    check_symbol_names.py
  ok    All:  32.79% fuzzy, 25.53% matched, 11.96% linked (11409 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CElementGen::GetEmitterTime() const',
                                       'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 97 -> 100 / 158 functions
  ok    no asm added
goal_check: PARTIAL - flip_test FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched 11406 -> 11409   linked 5514 -> 5514   (+3 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: SpinToSpeed__10CMorphBallFfRC9CVector3ff
    +100%  main/MetroidPrime/Player/CMorphBall :: TransformSpiderBallForcesXY__...
    +100%  main/MetroidPrime/Player/CMorphBall :: TransformSpiderBallForcesXZ__...
  no regression
```

Per function, `build/report.json`: the three above **99.6875/99.71429/99.71429 -> 100.0**; unit
`matched_code` **11628 -> 12156** of 66600, `.text` fuzzy **28.626066 -> 28.628408**. I diffed
all 158 per-function percentages against the same report generated from the stashed (pristine)
source: **+3, 0 worse, 0 changed**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged). All 86
RELs unchanged (`hashes vs config.yml ok`). `build/gate-probe.log`: `probe: 751 files, 0 failed,
0 errors; link: LINKED (250 undefined, 0 duplicates)`. `docs claims agree with the tree`.
`config/G2ME01/splits.txt` untouched, so its 804 unit blocks and the 28465 `total_functions` are
as they were. `tools/unit_fit.sh`: `.text claimed 66600 ours 24556 SHORT by 42044`, and the
"present in ours but not in the retail unit object" list is **50 functions / 5228 bytes,
unchanged** by this diff (all COMDAT weak template/inline copies, which CAi also carries).

NEW: cmorphball-writemovementallowed | match | MetroidPrime/Player/CMorphBall |
`IsMovementAllowed__10CMorphBallCFv` (retail 0x800CE7D0, 148 B = 37 insns) is an 8-byte
`return false` stub in our object (3.784%). Its body is fully readable and characterisable -
`GetTweakPlayerControls` at 0x8000BF7C, `fn_80215860` at 0x80215860, `IsMorphBallTransitioning`
at 0x80019DF8, two `lbz` flags at CPlayer+0x5F1/+0x5F2 and the `mfcr/rlwinm/cntlzw/srwi`
float-compare tail on CMorphBall+0x1860 - and the full transcription is in this item's notes, so
it is the cheapest of the 61 sub-100% functions to bring to 100%.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:1074-1084` - `SpinToSpeed`, the named `scale`, with the codegen rule recorded
  - `:1679-1699` - `TransformSpiderBallForcesXZ` / `TransformSpiderBallForcesXY`, the named
    `res` local and the epilogue-order rule

Not committed, per the brief.

---

# Run 3 (lane 6, 2026-10-01)

## Result: `goal_check` PARTIAL - the unit's matched count rose **104 -> 105 of 158**

Re-measured on the clean tree first: 104/158 matched, 13428/66600 bytes, `.text` fuzzy
28.866606, and **53 functions below 100%** covering 53172 bytes. The previous run's "61
unwritten" is now 53; 11 functions had no body in our object at all, and 10 still do. The
carve analysis in run 1 is **not retried** and still stands: the unit's matched code is in 31
separate runs, so a carve needs 31 boundaries, and `check_decl_order.py` still reports 146 of
158 in the wrong order. The route that worked this run is the one run 1 identified - write
bodies - applied to the two symbols the flip actually names.

## What I changed

Two files. No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no
asm, nothing under `tools/` or `build/goal/`. `docs/HANDOFF.md` shows as modified after
`goal_check.sh`; that is the judge rewriting its own derived counts, and I reverted it.

1. **`include/Kyoto/Particles/CElementGen.hpp:109`** - `GetEmitterTime` is now **declared**,
   not defined in the class body. One line, `{ return mCurFrame; }` deleted.
2. **`src/MetroidPrime/Player/CMorphBall.cpp:106-118`** - `int CElementGen::GetEmitterTime()
   const { return mCurFrame; }` written out, so the strong definition lands in the object that
   claims its address.
3. **`src/MetroidPrime/Player/CMorphBall.cpp:120-137`** - `rstl_string_eq_c`, retail's
   out-of-line `rstl::operator==(const basic_string&, const char*)` at 0x8008808C.
4. **`src/MetroidPrime/Player/CMorphBall.cpp:991-1015`** - `GetMorphBallModel`: the comparison
   now calls item 3, the `SObjectTag` is a by-value local, and the scale is spelled out at each
   `rs_new` site.

## `CElementGen::GetEmitterTime` is the first flip blocker, and this is the fix - 0 -> 100%

Retail 0x800CA558, 8 bytes, two instructions: `lwz r3,104(r3)` / `blr`. `mCurFrame` is at +0x68
(`include/Kyoto/Particles/CElementGen.hpp:198`).

**The measurement that makes it obvious this belongs in `CMorphBall.o` and not
`CElementGen.o`**, and it is the same evidence for both objects:

```
$ nm build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o | grep GetEmitterTime
0000a2b4 T GetEmitterTime__11CElementGenCFv          <- strong definition, here
$ nm build/G2ME01/obj/Kyoto/Particles/CElementGen.o | grep GetEmitterTime
         U GetEmitterTime__11CElementGenCFv          <- undefined reference
```

and `config/G2ME01/splits.txt` puts 0x800CA558 inside `MetroidPrime/Player/CMorphBall.cpp`'s
`.text` range (0x800C02A4..0x800D06CC), while `Kyoto/Particles/CElementGen.cpp` claims
0x802D0CA4..0x802DCAC0 - nowhere near it. Retail's *own* object has the strong definition in
CMorphBall.o, so this reproduces retail rather than working around it.

Written inline in the class body, mwcceppc emitted it **weak** (`W`) into `CElementGen.o` and
nowhere else - which is why `flip_test.sh` reported `undefined: 'CElementGen::GetEmitterTime()
const'` and why it was one of the two symbols named in this item's own `reason`. After the
change that `undefined:` is **gone from `build/flip-ninja.log`**: the flip's undefined list went
from 9 names to 7.

**Rule this establishes, and it generalises past this function: a virtual defined in a class
body is emitted weak into whichever object needs the vtable, which is not necessarily the object
`config/G2ME01/splits.txt` says owns its address.** When `nm` shows retail's object has the
symbol `T` and another unit's has it `U`, the definition belongs in *this* unit's source as an
out-of-line member definition, and the header must declare rather than define it. Nothing else
in the tree does this yet, so it is worth checking the other inline virtuals the same way.

## `GetMorphBallModel` 84.14 -> 99.9375, three separate measurements

None of this is in the earlier runs' tried lists. All three are properties of the *shape*, not
of the arithmetic, and all three were measured by building and scoring.

1. **The empty-name test is an out-of-line call.** Retail 0x800C12D8 is
   `bl __eq__4rstlF...` = `__eq__4rstlFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>PCc`
   (`config/G2ME01/symbols.txt:2447`), and the result is tested with `clrlwi. r0,r3,24` / `beq`.
   The tree's `include/rstl/string.hpp:411` declares that same free function `inline`, so
   **every C++ spelling inlines the `compare` call** and MWCC emits a `li r5,-1` length
   argument plus a `cmpwi r3,0`, shifting the whole function by one instruction from there.
   Measured: `name == ""` 97.94%, `!::rstl::operator==(name, "")` 98.00%, a `static bool
   (*const)(const string&, const char*)` function pointer 83.81% (worse - the static's own
   store costs more than it saves), and **writing the function out under an `extern "C"` name
   99.9375%** (kept). This is the `fn_800C084C` rule again: when retail's object does not
   resolve the mangled name, the body has to be written out for the call to be a real `bl`.
2. **The `SObjectTag` is a by-value local.** Retail 0x800C1304..0x800C1318 is
   `lwz r4,0(r3)` / `lwz r3,4(r3)` / `stw r4,8(r1)` / `stw r3,12(r1)` and the frame is 96
   bytes; taking `const SObjectTag*` and reading through it scores 84.14%.
3. **The `2.f * radius` scale is written at each `rs_new` site, not hoisted.** Retail loads
   `lfs f0,-28784(r2)` (= 0x8041B350 = 2.0f, read with `tools/sda.py s2:-28784`) and does
   `fmuls f0,f0,f31` **inside each of the two branches**, storing f0 three times. A single
   named `CVector3f scale` hoists the multiply above the `bne` and is a different instruction
   stream.

### The last 0.0625% is not reachable from C++ in this toolchain - measured, not guessed

All 80 of the function's instructions are **byte-identical** to retail's (verified with
`objdump -s` over the function's 320 bytes in both objects: the only diffs are the base-address
offsets). The residual is **one relocation out of 160**: the call site's `R_PPC_REL24` names
`rstl_string_eq_c` where retail's names `__eq__4rstlF...`, and objdiff compares relocation
targets. Two ways to rename were tried and **MWCC's compiler rejects both**:

- `asm("__eq__4rstlF...")` on the function declaration - `type cannot be made into a global
  register variable; only scalers, doubles, floats and vectors are supported`
- namespace-scope `__asm__(".globl ...")` inside `extern "C" { }` - `')' expected`

`#pragma noinline` does not rename a symbol, so it is not a route either. A lane should not
spend time here again.

## Tried and rejected this run, so the next run does not repeat them

- **The two 92-byte vtable destructors `fn_800C88C0` (0x800C88C0) and `fn_800C33DC`
  (0x800C33DC)** - the two are the same shape, each storing one of `lbl_803B36F0` /
  `lbl_803B36FC` and then `lbl_803B1750`, then the usual `extsh.`/`ble` `deleting` flag. All
  three vtables are in **unclaimed `.data` gaps** (verified: `nm` finds none of them defined in
  any of the port's 400+ objects, and none is in `docs/research/port_link_gap_list.md`).
  Written out, both reached **53.91% / 19 instructions against retail's 23** - MWCC will not
  emit retail's shape for two stores to the same address, in three spellings, all 53.91%:
  two assignments to one `*static_cast<void**>(self)` (it drops the first as dead), the same
  through a named `void** vptr` (it hoists both `lwz` above the `extsh.`), and the same with
  both addresses pre-loaded into locals. Retail has `lis`/`addi` per address *at the point of
  its store* with a dead `beq` between them.
  **I wrote them, measured, and then removed them**: defining the three vtables needs a new
  `Port*.cpp` + `files.cmake` entry, and `tools/link_gap.py` then fails `gap grew: lbl_803B1750
  is not in port_link_gap_list.md`, which fails the whole gate. Not worth it for 53.91% on a
  function that was 0% - the honest trade is recorded here rather than banked.
- **`fn_800CD460` (0x800CD460, 88 B) reached 100%** as a `void* link(void* self, short
  deleting)` delegating to `fn_800CD4B8(self + 24, -1)`, and `fn_800CD4B8` (0x800CD4B8,
  152 B) reached **93.03%** as a `CMemory`-style teardown over a flag byte at +0 and a pointer
  at +12 (a 2-bit field decremented, a bit raised when it hits zero, bit 5 choosing between
  `Free` and the allocator's release path). **Both were removed for the same reason**: that
  body calls `fn_8033D2F4` (0x8033D2F4, 0x64), which is undefined in the whole tree and is
  **not** among the port link's 250 tolerated symbols, so writing the call grows the count to
  251 and fails `probe link-gap`. `fn_800CD460` at 100% was the single cheapest remaining
  function and it still is not worth a gate failure - the blocker is one unclaimed-range
  function, and closing it is its own item.
  `fn_800CD4B8` spellings measured, all worse than 93.03%: the count field read as bits 0-1
  and written back at bits 4-5 with a `!= 0` test scores 91.58%; going through `uint*` instead
  of `uchar*` scores 81.84% and makes MWCC emit `clrlwi`/`slwi`/`or` instead of
  `rlwinm`/`rlwimi`. **A C++ bitfield is not the answer** - MWCC gives a `stw`-and-mask pair,
  not the single-byte rotates retail has.

## The route, stated plainly for the next lane

The flip is not reachable from this unit in one item, and run 1's carve analysis is the reason.
What *is* reachable, and what this run did, is:

- **the flip blockers first** - they are the symbols the item's own `reason` names, and fixing
  one is a real 100% function plus one fewer `undefined:` at link time. `GetEmitterTime` was one
  and is done. The others (`fn_800CD4B8`, `fn_800C88C0`, `fn_800C33DC`) are all blocked on
  symbols in **unclaimed ranges**, which is the same wall `CPhysicsActor.cpp:141-147` records
  for `fn_800EB944`.
- **so the next real step is a unit for the unclaimed-range symbols the port needs**:
  `fn_8033D2F4` (0x8033D2F4) and the three `.data` vtables at 0x803B36F0 / 0x803B36FC /
  0x803B1750. One `Port*.cpp` + one `files.cmake` line, exactly as `PortCTweakPlayerControls.cpp`
  does for `fn_80215860`, and then **four functions in this unit** (two at 100%, two above 53%)
  become writable with the gate green. That is the item worth queueing; it is not this one.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11439 -> 11440   linked 5584 -> 5584
  ok    check_symbol_names.py
  ok    All:  32.83% fuzzy, 25.66% matched, 12.15% linked (11440 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'fn_800CD4B8', 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 104 -> 105 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakeeffects-carve-74-unwritten - flip_test ...: FAIL, but the
target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11439 -> 11440   linked 5584 -> 5584   (+1 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: GetEmitterTime__11CElementGenCFv
  no regression
```

Per function, `build/report.json`: `GetEmitterTime__11CElementGenCFv` (8 B) **0.0 -> 100.0**;
`GetMorphBallModel` (320 B) **84.1375 -> 99.9375**. Unit `matched_code` **13428 -> 13436** of
66600, `.text` fuzzy **28.866606 -> 28.954535**, `matched_functions` **104 -> 105**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged). All
86 RELs unchanged (`hashes vs config.yml ok`). `build/gate-probe.log`: `probe: 752 files, 0
failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)` - **250 against the judge
baseline of 250**, unchanged, which is why the `fn_800CD4B8` and vtable work was reverted rather
than kept. `docs claims agree with the tree`. `config/G2ME01/splits.txt` untouched, so its
804 unit blocks and the 28465 `total_functions` are as they were.

`tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` (run before the revert, so with the
extra bodies in): `FAIL -> reverted`, and its `build/flip-ninja.log` `undefined:` list is **7
names, down from 9** - `GetEmitterTime`, `fn_800C88C0` and `fn_800C33DC` are resolved by the
work this run measured, and the two that remain are the port-link-blocked ones.

NEW: cmorphball-unclaimed-vtables-and-8033d2f4 | match | MetroidPrime/Player/CMorphBall |
four functions in this unit are one `Port*.cpp` + one `files.cmake` line away from writable, and
`fn_800CD460` is already written and measured at **100%** (0x800CD460, 88 B, a
`void* link(void* self, short deleting)` that delegates to `fn_800CD4B8(self + 24, -1)` and then
frees on the flag) - they are blocked only because the symbols they need are in **unclaimed
ranges**: `fn_8033D2F4` (0x8033D2F4, 0x64) and the three `.data` vtables at 0x803B36F0,
0x803B36FC and 0x803B1750. `nm` finds none of the three defined in any of the port's 400+
objects, and `tools/link_gap.py` fails `gap grew: lbl_803B1750 is not in
port_link_gap_list.md` as soon as a body references them, which fails the whole gate. With them
defined, `fn_800CD460` is 100% and `fn_800CD4B8` (0x800CD4B8, 152 B) is 93.03%, `fn_800C88C0`
(0x800C88C0) and `fn_800C33DC` (0x800C33DC) are both 53.91% and stop being 0% - and the DOL
flip's `undefined:` list drops by three. This is the `PortCTweakPlayerControls.cpp` arrangement
the repo already uses for `fn_80215860`; the *measurements* for all four bodies are in this
item's notes, so it is transcription work, not investigation.

## Files

- `include/Kyoto/Particles/CElementGen.hpp:109` - `GetEmitterTime` declared, not defined
- `src/MetroidPrime/Player/CMorphBall.cpp:106-118` - `CElementGen::GetEmitterTime`, with the
  `nm`/`splits.txt` evidence for why the definition belongs in this unit
- `src/MetroidPrime/Player/CMorphBall.cpp:120-137` - `rstl_string_eq_c`, retail's out-of-line
  `operator==`, with the two rejected renaming attempts recorded
- `src/MetroidPrime/Player/CMorphBall.cpp:991-1015` - `GetMorphBallModel`, the three
  shape measurements and the rejected spellings

Not committed, per the brief.
