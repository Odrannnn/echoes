# progress-unit-ccamerashakerdata

`kind: progress`, target `MetroidPrime/Cameras/CCameraShakerData` (DOL unit, stays `NonMatching`).
Result: **`matched_functions` 6 -> 7 of 10**, `matched_code` 44.92% -> 59.40%, unit fuzzy 78.49% -> 90.36%.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate clean, no `asm` added, no judge path touched,
total matched 11883 -> 11884, linked 5727 -> 5727).

One function reached a real 100%: `__ct__17CCameraShakerDataFffUiRC9CVector3f...` (59.73% -> 100%).

## What I changed (all in `src/MetroidPrime/Cameras/CCameraShakerData.cpp`)

1. **Constructor, 59.73% -> 93.58%** - test the `flags` *parameter*, not the `mFlags` member:
   `if ((flags & kF_ExplicitDuration) == 0)`. Retail keeps `flags` live in `r28` from the store
   `stw r28,0(r3)` and branches on `rlwinm. r0,r28,0,29,29` with no reload; reading the member made
   mwceppc spill `this` to `r28`, reload `lwz r0,0(r28)` for the test, and emit four separate
   `stw r31/r30/r29/r28` where retail has one `stmw r27,12(r1)` (retail keeps 5 callee-saved regs:
   `r27`=this, `r28`=flags, `r29..r31`=the three spline args). Semantically identical: `mFlags` is
   initialised from `flags` and never written again.

2. **Constructor, 93.58% -> 99.25%** - swap the `max_val` argument order *and* give the call result a
   name: `const float v = mVerticalMotion.GetMaxTime(); mDuration = rstl::max_val(v, mDuration);`
   (same for the forward spline). Retail re-reads `mDuration` from memory into `f0` and writes the
   result back into `f0` (`lfs f0,4(r27)` / `fcmpo cr0,f1,f0` / `fmr f0,f1` / `stfs f0,4(r27)`).
   A bare swap without the named temporary gave 99.25% but a different shape; a local temporary that
   is then `max_val`'d (not assigned straight back) is what lands on `f0`.

3. **Constructor -> 100%.** Both above together: the only remaining bytes were the float constants,
   which are `lfs f0,0(0)` + `R_PPC_EMB_SDA21` here and `lfs f0,-20320(r2)` in retail. objdiff
   normalises that (the already-matching `UpdateThresholdTimes` uses the same pattern), so those are
   equal.

4. **`FindFirstIntersection`, 40.77% -> 63.59%** - replace the aggregate initialiser with a runtime
   counter: `float times[6]; int count = 0; times[count++] = ...;` x6, then `rstl::sort(times, times + count)`
   and `for (int i = 0; i < count; ++i)`. Retail clearly used a runtime count: it stores the counter to
   `8(r1)`, re-loads it for every `stfsx` and increment, passes `sort` a runtime end pointer, and its
   scan loop has the `cmpwi r0,0 / ble` guard that only appears when the bound is not a literal 6.
   The old spelling also emitted 24 bytes of dead constant-pool copy into the array.

5. **`FindLastIntersection`, 84.51% -> 91.67%** - swap the `max_val` arguments to
   `rstl::max_val(mHorizontalMotion.FindLastIntersection(amplitude), ...(-amplitude))`.
   mwceppc evaluates call arguments right-to-left, so with the `-amplitude` argument second the
   `-amplitude` call is emitted first, which is what retail does. Same for the vertical and forward pairs.

## Walls (spellings tried in this run, all measured)

- **`FindLastIntersection` at 91.67% (288 B).** The whole remaining diff is float-register allocation:
  retail holds `-amplitude` in `f31` and the vertical max in `f30`; we hold `-amplitude` in `f30` and
  the vertical max in `f31`. Everything else (prologue, six calls, three-way max, epilogue) is
  byte-identical. Tried: nested `max_val(+amp, -amp)` **91.67**; nested with a `-amplitude` local
  **91.67**; two-statement accumulate **90.21**; call result in a named temporary then
  `max_val(tmp, mDuration)` **90.21**; the same reversed **84.51**; explicit `if (b > a) a = b`
  **87.01**; nested `max_val(-amp, +amp)` (original) **84.51**.
- **`NewTranslation` at 86.19% (64 B).** Retail emits `lwz r4,0(r4)` (mFlags) *before* `lfs f1,8(r9)`
  and `lfs f2,4(r9)`; we emit the two float loads first. Argument evaluation order for the ctor call:
  retail 5,6,7,**3**,1,2,8; ours 5,6,7,1,2,3,8. Pure load scheduling - tried: `mFlags` inline
  **86.19**; `const uint flags = mFlags` local **86.19**; all three scalars hoisted into locals
  **86.19**.
- **`FindFirstIntersection` at 63.59% (400 B).** Retail keeps `count` in memory (`8(r1)`) and the array
  base in `r31`; mwceppc promotes `count` to a register and re-materialises the base with `addi r1,8`.
  The tail (sort call, scan loop, epilogue) is otherwise identical. Tried: aggregate initialiser
  **40.77**; `int count` **63.59**; `uint count` **63.04**; `size_t count` **63.04**; explicit
  `float* base = times; base[count++]` **63.04**; `times[count] = ...; ++count;` as separate
  statements **54.50**.

WALL: FindLastIntersection 91.67% - remaining diff is float register allocation only (f30/f31 swap), 7 spellings tried.
WALL: NewTranslation 86.19% - remaining diff is one 3-instruction argument-load reordering, 3 spellings tried.
WALL: FindFirstIntersection 63.59% - mwceppc promotes the loop counter to a register where retail keeps it in memory, 6 spellings tried.

## Reusable codegen lessons (not new items)

- **mwcceppc evaluates call arguments right-to-left.** With `max_val(f(+x), f(-x))` the `-x` call is
  emitted first. This decided both `FindLastIntersection` and the ctor's `GetMaxTime` max.
- **`x = max_val(call(), x)` vs `x = max_val(tmp, x)` where `tmp = call()`.** The first keeps the value
  cached in a register across the call (and picks a non-volatile FPR); the second re-reads `x` from
  memory into a volatile FPR, which is what retail did.
- **Reading a member that was just initialised from a parameter costs a reload.** The retail ctor's
  `rlwinm. r0,r28,...` on the live `flags` register (and its `stmw`/`lmw`) is evidence the original
  tested the parameter, not `mFlags`.
- **A `float a[N]` filled by `a[i++] = f()` where `i` is a runtime counter** reproduces retail's
  load/store of the index from the stack; passing a literal bound to `sort` and the scan loop does not.

## Notes

- Unit left `NonMatching`; `flip_test.sh` deliberately not run (this is a `progress` item).
- `tools/unit_fit.sh` still reports the unit's `.text` 748 bytes over the claimed range plus 4 extra
  COMDAT weak/template functions (`CMayaSpline` copy ctor/dtor, `rstl::vector` copy ctor/dtor) and
  `.rodata`/`.sdata` unclaimed by `splits.txt` - unchanged by this item, and the reason the unit cannot
  flip yet. The `.rodata` 24 bytes of dead constant-pool copy disappeared when `FindFirstIntersection`
  stopped using an aggregate initialiser.
- No `configure.py`, `config/` or `splits.txt` change; no `asm` added; nothing committed.