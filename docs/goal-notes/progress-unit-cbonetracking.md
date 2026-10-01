# progress-unit-cbonetracking — CBoneTracking

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` in `../wt-mp2-goal-L3`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11836 -> 11837   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.55% fuzzy, 26.65% matched, 12.64% linked (11837 / 28465 functions)
  ok    target rose: main/MetroidPrime/CBoneTracking: 9 -> 10 / 12 functions
  ok    no asm added
goal_check: PASS progress-unit-cbonetracking
```

**One function taken to 100%** (`PreRender(...bool)`, 376 B, 89.47% -> 100.0%), plus both
remaining functions made substantially closer as a side effect. The unit stays `NonMatching`;
`flip_test.sh` was not run to decide anything.

## Re-measured baseline (clean tree, `build/report.json`)

`main/MetroidPrime/CBoneTracking`: `fuzzy 65.27%`, `matched 9/12`, `matched_code 19.38%`
(620/3200 B). The three unmatched functions and their sizes:

| function | size | before | after |
|---|---|---|---|
| `PreRender__13CBoneTrackingFRC13CStateManagerR9CAnimDataRC12CTransform4fRC9CVector3fb` | 376 B | 89.47% | **100.0%** |
| `UpdateTracking__13CBoneTrackingFRC12CTransform4fRC9CVector3fRC9CVector3fRC15CCharLayoutInfoR24CPoseAsTransforms_Linear` | 1088 B | 76.44% | 87.74% |
| `UpdateInactive__13CBoneTrackingFRC15CCharLayoutInfoR24CPoseAsTransforms_Linear` | 1116 B | 26.92% | 47.47% |

Unit fuzzy 65.27% -> 77.51%; `matched_code` 620 -> 1002 B (19.38% -> 31.12%).

## What was changed (`src/MetroidPrime/CBoneTracking.cpp` only)

1. **`PreRender` — the one that reached 100%.** Two edits, both needed:

   a. **Bind the subtraction to a named local.** `(targetPosition - xf.GetTranslation()).MagSquared()`
      left the `CVector3f` temporary address-taken, so mwcceppc **materialised it on the stack**
      and split the compare across three spill/reload groups (`stfs`/`lfs` at 12/16/20(r1)) with
      `fsubs` in f5/f2/f3. Retail never spills: it loads `targetPosition` and the translation
      components straight into f2/f1/f0/f3 and folds the whole squared distance with two
      `fmadds`. Naming it (`const CVector3f delta = ...; delta.MagSquared()`) lets the
      temporary stay in registers and reproduces retail's instruction stream.
      **89.47% -> 99.57%.**

   b. **Swap the declaration order of `layout` and `pose`.** With (a) applied the only
      remaining difference was `lwz r26,268(r5)` vs `lwz r27,268(r5)` and the mirrored
      `addi r27,r5,688` — the two locals were in the callee-saved registers in the opposite
      order, which then also swapped every `mr r4,r26 / mr r5,r27` argument shuffle into the
      `UpdateTracking`/`UpdateInactive` calls. Declaring `pose` first, then `layout`, matches
      retail's r26/r27. **99.57% -> 100.0%** (the only line left in `lanediff.sh` is the
      `R_PPC_EMB_SDA21` relocation *label* text `lbl_8041BC94` vs `@868`, which is a dtk
      symbol-naming artifact, not a byte difference — objdiff scores the function 100%).

2. **`UpdateTracking`, 76.44% -> 87.74%.** `CMath::Min<T>` returns `const T&`, so mwcceppc
   emitted an **out-of-line `bl Min<f>__5CMathFRCfRCf`** three times (and a weak out-of-line
   copy of the template in the object, which retail's object does not contain at all —
   verified with `nm` on both objects). Retail inlines the min as `fcmpo/bge/fmr`. Spelling
   the three mins as explicit ternaries removed the calls. **76.44% -> 87.48%.** The ternary
   *operand order* then matters: retail's `fcmpo cr0,f0,f1` puts the loaded `mMaxTrackingAngle`
   in f0 and the call's result in f1, so it must read `angleDiff < mMaxTrackingAngle ? angleDiff
   : mMaxTrackingAngle`, not the reverse. Same for `clampedAngle`: retail compares
   `fcmpo cr0,f30,f0` (angle in f30, `mTime*mAngSpeed` in f0), so `angle < maxAngleDelta ?
   angle : maxAngleDelta`. **87.48% -> 87.56% -> 87.74%** over those two swaps.

3. **`UpdateInactive`, 26.92% -> 47.47%.** Retail's prologue is much bigger than ours and does
   three things ours did not: it calls `GetRotation` into a temporary and then **copy-constructs**
   a `CMatrix3f` local (`__ct__9CMatrix3fFRC9CMatrix3f`) before each `FromMatrix`; it
   **`BuildNormalized()`s both quaternions unconditionally, before** the `mHasTrackedRotation`
   test, rather than only on the tracked path; and it **duplicates the whole prologue** for the
   early-return branch. Writing the matrices as named `const CMatrix3f` locals and hoisting both
   `BuildNormalized()` calls into the initialisers reproduced the copy-constructs and the
   unconditional normalisation. Plus the same `CMath::Min` -> ternary fix.
   **26.92% -> 39.19%** (named matrix locals) **-> 47.39%** (hoisting both `BuildNormalized()`
   into the initialisers) **-> 47.47%** (min ternary operand order).

## Measurements of every spelling tried this run

`UpdateTracking` (baseline 76.44%):

| spelling | score |
|---|---|
| `CMath::Min(a, b)` as written (baseline) | 76.44% |
| `mMaxTrackingAngle < angleDiff ? mMaxTrackingAngle : angleDiff` | 87.48% |
| `angleDiff < mMaxTrackingAngle ? angleDiff : mMaxTrackingAngle` | **87.74%** |
| ternary + `trackingXf` position hoisted into a ternary (`? boneXf.GetTranslation() : trackingXf.GetTranslation()`) | 82.63% |
| ternary + `ByElementMultiply` duplicated into both arms of the `mNoParentOrigin` if/else | 84.54% |
| writing `trackingXf.m03/m13/m23` directly | **does not compile** — `illegal access to protected/private member` (`CTransform4f::m03` is protected) |

`UpdateInactive` (baseline 26.92%):

| spelling | score |
|---|---|
| baseline (`FromMatrix(pose.GetRotation(...))`, normalise only on the tracked path) | 26.92% |
| + named `const CMatrix3f` locals | 39.19% |
| + both `BuildNormalized()` hoisted into the initialisers | **47.47%** |
| + early-return branch made fully self-contained (own `parent`/`parentRotation`/`boneRotation`) | 32.72% |
| `if (!(0.5f * maxAngleDelta < clampedAngle))` instead of `<=` | 42.06% |

Note on the last row: retail branches `fcmpo cr0,f2,f0 / ble`, i.e. the *same* `<=` test as
ours, but we emit `cror eq,lt,eq / bne` for it. Negating the comparison does **not** recover
retail's shape and costs 5 points, so this is a codegen difference the source spelling does not
reach, not a comparison-semantics difference.

## What is still blocking the other two functions

**`UpdateTracking`, 87.74%.** The residual is register/stack allocation, not logic. Every
call is now to the right callee with the right arguments; what differs is:

- frame size: retail `stwu r1,-784(r1)`, ours `-736(r1)`; the f30/f31 save sequences differ
  (retail `xvmaddadp`+`xxsel`, ours `psq_st`+`xvmsubmsp`) — i.e. retail keeps one more value
  live in an FPR across a call and we spill it.
- every stack slot is offset by a constant delta (retail's `lfs f1,8(r29)` vs ours at
  `4(r29)` etc.), consistent with a different frame layout rather than different work.
- the `mNoParentOrigin` block: retail loads the translation into **f3/f4/f5 before the branch**
  and phi-selects the source (`lfs f3,632(r1)` when the flag is set), so the
  `ByElementMultiply` consumes registers directly. Ours stores `boneXf`'s translation into
  `trackingXf`'s slot inside the branch and reloads after it (`lfs f2,576(r1)` at the join).
  Three attempts to get the compiler to phi this (ternary on the position, duplicated
  `ByElementMultiply`) both made it *worse*; direct member writes are impossible (protected).
  This is the single biggest remaining item.

**`UpdateInactive`, 47.47%.** Same story plus retail's **duplicated prologue for the
`!mHasTrackedRotation` early return**: retail emits the `GetRotation`/`__ct__`/`FromMatrix`/
`BuildNormalized` sequence twice (once at offset 0x00, again at 0x2ec) with the second copy
ending in the `return`. Ours emits it once and branches around the rest. Making the early-return
branch textually self-contained in the source scored **32.72%**, i.e. worse than sharing — the
duplication is a code-layout decision mwcceppc makes, not one the source can ask for directly.

## Gates

```
tools/goal_check.sh build/goal/item.json   -> PASS (output above)
python3 tools/check_decl_order.py --unit MetroidPrime/CBoneTracking
    ok: 1 unit(s) checked, none emits its functions out of retail order
```

No `asm`, no `configure.py`/`splits.txt`/`files.cmake` change, no unit flipped, nothing
committed. `docs/HANDOFF.md`'s state block was rewritten by the judge's own tooling to the new
derived counts (11836 -> 11837 matched, 10288 -> 10289 DOL); that is `goal_check`'s doing, and
it passes its own `check_docs_claims.py`.

## Lessons for the next run

- **`CMath::Min<T>`/`Max<T>` return `const T&` and therefore always outline.** Any unit that
  needs an inlined min/max must spell the ternary. This is the third unit where this has come
  up (`progress-unit-cscriptdebris`, `progress-unit-cplayer`, now this one). The **operand order
  is not cosmetic**: it decides which operand lands in which FPR and whether the `fcmpo`
  operands are swapped. Read the retail `fcmpo` operand order first, then write the ternary to
  match it.
- **A temporary that is only consumed by an inline member is not free.** `(a - b).MagSquared()`
  cost 10 points because the `CVector3f` got a stack home; naming it removed the spill. Same
  lesson as `progress-unit-cplayer`'s accessors: check whether mwcceppc gave the temporary a
  stack slot before concluding the arithmetic is wrong.
- **Hoisting an out-of-line call into an initialiser reproduces retail's unconditional work.**
  Retail computing `BuildNormalized()` on a path that does not need it is a *fact about retail*,
  and matching it means computing it unconditionally — not "only where needed", which is what a
  reader would otherwise write.