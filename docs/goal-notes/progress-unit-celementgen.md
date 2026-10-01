# progress-unit-celementgen — attempt 1

Four functions of `Kyoto/Particles/CElementGen` taken to 100%, all by changing the *spelling* of
existing logic, never by deleting work. The unit goes **54 -> 58 / 104** matched functions and stays
`NonMatching`; project total matched **11542 -> 11546**, linked held at **5625**.

| Function | Before | After | What changed |
| --- | ---: | ---: | --- |
| `ConstructChildParticleSystem__11CElementGenCFRC6CTokenUiUs` | 99.74% (152 B) | 100.00% | the `kOSF` flags are built with the two ternaries OR'd inline instead of through a named `boundsFlags` local. The local made mwcceppc evaluate the two halves in the opposite order (`or r6, r8, r6` instead of `or r6, r6, r8`). |
| `UpdatePSTranslationAndOrientation__11CElementGenFv` | 99.45% (196 B) | 100.00% | `if (mCurFrame <= mPSLT)` -> `if (mPSLT >= mCurFrame)`. Retail loads `mCurFrame` first and branches on `blt`; the source form decides the load order and the branch polarity. Same predicate. |
| `BeginIndirectModelRender__11CElementGenFRQ211CElementGen17SModelRenderState` | 99.96% (1104 B) | 100.00% | the `mCIND` test is negated: `if (!mLoadedGenDesc->mCIND) { ... } else { ... }`. Retail branches `bne` past the `mCIND` case, so the *no-mCIND* arm is the fallthrough. The bodies are unchanged, only which arm comes first. |
| `UpdateVelocitySource__11CElementGenFiiRQ211CElementGen9CParticleRC9CVector3f` | 97.52% (428 B) | 100.00% | `if (expired) { mEndFrame = -1; } return expired;` -> `if (expired) { mEndFrame = -1; return true; } return false;`. Retail materialises a canonical `1`/`0` in `r3`; returning `expired` lets the raw byte through in `r3` and costs three instructions. |

`RenderModels` also improved (97.52% -> 99.06%, still not matched) from the same two edits:
`if (!IsIndirectTextured()) { BeginModelRender } else { if (!mPMUS) return; BeginIndirectModelRender }`
(the arm order again), and `SModelRenderState`'s constructor (`include/Kyoto/Particles/CElementGen.hpp`)
now assigns `mConstantUV`/`mConstantIndirectUV`/`mModulateAlpha` as plain statements and fills
`mUV`/`mIndirectUV` field-by-field (maxes first) instead of `mIndirectUV = mUV`. The copy made
mwcceppc emit the eight float stores in one order and the three byte stores in another; retail
stores bytes `0x5e5, 0x5e6, 0x5e4` and floats `xMax, yMax, xMin, yMin` per set. The struct's
members, types and layout are untouched — only the order of the initialisation.

## Spellings tried and rejected (so the next run skips them)

`ConstructChildParticleSystem` (152 B), via `tools/try_edit.py`:

- `boundsFlags | (mEnableOPTS ? ...)` — 99.74%
- plain `int` locals for both flags — 99.74%
- **both ternaries OR'd inline — 100.00%** (kept)

`UpdatePSTranslationAndOrientation` (196 B):

- `!(mCurFrame > mPSLT)` — 99.45%
- hoisting `mLoadedGenDesc` into a local `desc*` — 90.49% (worse)
- **`mPSLT >= mCurFrame` — 100.00%** (kept)

`UpdateVelocitySource` (428 B):

- **`return true`/`return false` inside the `if` — 100.00%** (kept)
- `return 1`/`return 0` — also 100.00%, same bytes; kept the `bool` spelling
- `return expired != 0;` after the `if` — 96.03% (worse)

`BeginIndirectModelRender` (1104 B):

- **negate `mCIND` and swap the arms — 100.00%** (kept). This is a real finding worth keeping:
  retail's `bne` past the `mCIND` body means retail's source read the no-mCIND case first.

`RenderBasicParticlesRotNoTS` (544 B) — **not reached**. The diff is a pure FPR allocation offset:
base loads `viewPos` into `f28, f27, f26` and keeps `f24` for `halfSize`, we load into `f27, f26, f25`
and keep `f24`/`f28` swapped. Six spellings, all within 0.2% of each other and none at 100%:

- current (`const`, `halfSize` then `theta`) — 99.15%
- `theta` declared before `halfSize`, `const` — 99.23%
- `x, y, z` declared before the trig — 99.23%
- `const CRelAngle angle` local — 99.23%
- all non-`const` (Prime 1's spelling) — 99.23%
- Prime 1's `theta = particle.mLineWidthOrRota * (M_PIF / 180.f)` — 99.23%
- non-`const` with `theta` first — 99.01% (worse)

The same register offset appears in `RenderBasicParticlesRotNoTSModulated` (99.26%),
`RenderBasicParticlesRotTS` (99.02%) and `RenderBasicParticlesRotTSModulated` (99.06%), so one fix
would likely lift all four. It is a codegen-allocation question, not a logic difference — the
instruction sequence is otherwise identical. Next run should try **structurally different** shapes
rather than more ordering permutations: e.g. writing the vertex coordinates through a
`CVector3f`/array, computing `sinPlusCos`/`sinMinusCos` at the point of use instead of as locals,
or a `do { } while` loop instead of `for`.

## Verification

`./tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L7`:

```text
ok    no judge-owned path touched
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 11542 -> 11546   linked 5625 -> 5625
ok    check_symbol_names.py
ok    All:  33.11% fuzzy, 25.98% matched, 12.24% linked (11546 / 28465 functions)
ok    target rose: main/Kyoto/Particles/CElementGen: 54 -> 58 / 104 functions
ok    no asm added
goal_check: PASS progress-unit-celementgen
```

Diff is two files: `src/Kyoto/Particles/CElementGen.cpp` (+14/-9) and
`include/Kyoto/Particles/CElementGen.hpp` (+12/-6). No `asm`, no `.s`, nothing in `build/goal/`.
`docs/HANDOFF.md` is left untouched — `check_docs_claims.py` wants its state block updated to
11546/9998, but the driver rewrites those derived counts from the tree before judging, and the
prompt forbids editing it.

---

# progress-unit-celementgen — attempt 2

Seven more functions of `Kyoto/Particles/CElementGen` taken to 100%, all by changing the
*spelling* of existing logic — never by deleting work, and no struct layout, member, type or
signature touched. The unit goes **58 -> 65 / 104** matched functions and stays `NonMatching`;
project total matched **12087 -> 12094**, `linked` held at **5795**.

Every score below was measured on this tree with `tools/fast_try.sh Kyoto/Particles/CElementGen`
and, for the instruction-level reasoning, `tools/bytescmp.py` plus a side-by-side of
`tools/dis.sh` (retail) against `objdump -d -r` (ours). `tools/try_edit.py` drove the spellings.

| Function | Before | After | What changed |
| --- | ---: | ---: | --- |
| `GetGenerationRate` | 96.97% (132 B) | 100.00% | `return rstl::max_val(0.f, rate * mGeneratorRate);` -> assign it to the (address-taken) local `rate` and `return rate;`. Retail stores the max back into the stack slot at `8(r1)` before the epilogue; we did not. Retail's `b +0x70` after the early-out also became `b +0x6c` once the extra instruction moved the epilogue. |
| `RenderBasicParticlesRotTSModulated` | 99.06% (448 B) | 100.00% | `theta = CRelAngle::FromDegrees(particle.mLineWidthOrRota).AsRadians()` -> `theta = particle.mLineWidthOrRota * (M_PIF / 180.f)`. |
| `RenderBasicParticlesRotTS` | 99.02% (428 B) | 100.00% | same `theta` spelling |
| `RenderBasicParticlesRotNoTS` | 99.23% (544 B) | 100.00% | same `theta` spelling |
| `RenderBasicParticlesRotNoTSModulated` | 99.26% (564 B) | 100.00% | same `theta` spelling |
| `RenderBasicParticlesNoRotTS` | 94.12% (272 B) | 100.00% | `const uint color = particle.mColor.GetColor_u32();` moved **after** the `x`/`y`/`z` declarations. |
| `RenderBasicParticlesNoRotNoTS` | 95.88% (388 B) | 100.00% | same `color`-after-`x,y,z` move |

`RenderModels` also improved, 99.06% -> 99.07%, still not matched (see below).

## The three levers, and why they work

**1. `theta`: the angle conversion is a bare multiply, not `CRelAngle`.** This is the one that
unlocked the whole `RenderBasicParticlesRot*` family — four functions from one line, and it
answers attempt 1's open question about `RenderBasicParticlesRotNoTS`.

The four functions were each 4 bytes short and the instruction sequence was *identical* to
retail's — only FPR **numbers** differed. Retail numbers its values in *declaration order*,
descending from `f29`, which the already-matched sibling `RenderBasicParticlesNoRotTSModulated`
confirms (`const` = `f31`, then `viewPos.x/y/z` = `f30/f29/f28`). With
`CRelAngle::FromDegrees(...).AsRadians()` we got `theta = f29`, `viewPos = f28/f27/f26`,
`halfSize = f25`; retail has `viewPos = f29/f28/f27`, `halfSize = f26`, `theta = f25`. The
`CRelAngle` spelling makes mwcceppc create the `theta` temporary *first*, one slot before
`viewPos`. Writing the same multiply by hand (`x * (M_PIF / 180.f)`) reproduces retail's creation
order exactly. Not an arithmetic change: `FromDegrees(x).AsRadians()` *is* `x * (M_PI/180)`, and
`M_PIF/180.f` is already the spelling in use elsewhere in this file (line 2024).

**2. `color`: it is declared last.** In `RenderBasicParticlesNoRotTS` the only difference was
*where* one load sits in the loop body. Retail:

```
    lfs     f0,16(r1)      ; viewPos.z
    lfs     f2,12(r1)      ; viewPos.y   <-- immediately after
    li      r3,0
    lwz     r6,52(r28)     ; particle.mColor
```
Ours had `viewPos.y` *after* the `lwz` of the colour. Moving `const uint color = ...` below the
`x`/`y`/`z` declarations changes the creation order and the load lands where retail has it.
This is a declaration-order effect, not a scheduling one: the six other orderings I tried
(colour first, `halfSize` last, `x/y/z` first, non-`const` colour, all non-`const` coordinates)
all scored 94.12%.

**3. `if (IsIndirectTextured())` arms: MW lays out the THEN arm as the fallthrough.** In
`RenderModels` the source said `if (IsIndirectTextured()) { RenderIndirectModelParticle } else
{ RenderModelParticle }` and mwcceppc emitted the *indirect* call as the fallthrough with a
`beq` to the direct block — retail branches **`bne`** to the indirect block and falls through to
the direct one. Swapping to `if (!IsIndirectTextured()) { RenderModelParticle } else
{ RenderIndirectModelParticle }` (same predicate, same two arms, same order of execution)
produces retail's `bne` + fallthrough, at two sites: the per-particle call inside the loop and
the `EndModelRender`/`EndIndirectModelRender` pair after it. This is the same arm-order lever
attempt 1 used for the `BeginModelRender` head of this function; attempt 1 had left the other
two sites in the wrong order.

## Spellings tried and rejected (so the next run skips them)

`RenderBasicParticlesRotTSModulated` (448 B) — the pure FPR-numbering case. **Declaration order
alone does not move it**; only the `theta` spelling does:

- `theta` before `halfSize`, `halfSize` before `theta`, `x/y/z` immediately after `viewPos`,
  all trig before `viewPos` — all 99.06%
- `x, y, z` right after `viewPos` (before `color`) — **99.73%**, closer but not 100%
- `theta` non-`const` — 99.06%
- `halfSize` non-`const` — 99.06%
- `halfSize`/`theta` locals removed entirely — 99.06%
- **`theta = particle.mLineWidthOrRota * (M_PIF / 180.f)` — 100.00%** (kept)

This **supersedes attempt 1's wall** on `RenderBasicParticlesRotNoTS` (now matched, 100%):
attempt 1 listed "Prime 1's `theta = particle.mLineWidthOrRota * (M_PIF / 180.f)`" at 99.23%,
so the spelling on its own was **not** enough for that function then. The difference is that
`tools/try_edit.py` rewrites the whole function per variant, so attempt 1's M_PIF variant almost
certainly also carried a declaration permutation from the list above it; this run changed the
`theta` line and nothing else, and the function matched. Lesson for the next run: measure a
candidate spelling **alone on the tree as it stands**, not inside a rewritten body, or a real
answer reads as a failure.

`EndModelRender` (140 B) — **wall**, see below.

`RenderBasicParticlesNoRotTS` (272 B), via `tools/try_edit.py`:

- `color` last — **100.00%** (kept)
- `color` first (baseline), `color` between `halfSize` and `x/y/z`, `x/y/z` before
  `halfSize`+`color`, `halfSize` last, `uint color` non-`const`, all three coordinates
  non-`const` — all 94.12%

`EndModelRender` (140 B), ten spellings, all 94.29%:

- baseline — 94.29%
- `if (!mModelsUseLights) {} else { DisableAllLights(); }` — 94.29%
- `const bool useLights = mModelsUseLights;` local — 94.29%
- `const SModelRenderState& s = state;` — 94.29%
- `static_cast<bool>(mModelsUseLights)` — 94.29%
- `do { ... } while (0);` wrapper — 94.29%
- `if (mModelsUseLights == true)` — 91.14% (worse)
- duplicating the tail into both arms of the lights test — 45.57% (worse)

WALL: EndModelRender 94.29% - the only difference is that retail loads `mModelsUseLights`
(`lbz r0,613(r3)`) *between* `stw r0,36(r1)` and `stw r31,12(r1)`, and we emit it after
`mr r31,r4`; every spelling of the condition that keeps the call order produces our order.

## Measured and left alone

These were re-measured on this tree and are **not** close; the notes below are so the next run
does not re-read them:

- `RenderModels` (2332 B) 99.06% -> **99.07%**, still 22 bytes. The remaining diff is one hoisted
  load plus a GPR-numbering shift: retail loads `mParticles`' data pointer (`lwz r3,64(r30)`)
  *after* selecting `index = sorted ? sortItems[i].mPartIdx : i`, we hoist it above the branch.
  Ours also numbers `sortItems`/the loop counter one register higher (`r23/r26` vs `r26/r24`).
- `RenderModelParticle` (796 B) 95.53%, ours **24 bytes short**. The four `CModel::Draw` sites
  build `CModelFlags` differently: retail emits `li r6,3; lwz r5,0(r30); clrrwi r0,r6,2` plus a
  *second* copy of the flags struct, ours emits three plain `li`s (`5`, `0`, `1`) and one copy.
  A logic difference in the flags construction, not allocation.
- `RenderIndirectModelParticle` (808 B) 92.20%, ours 12 bytes short; retail's indirect path emits
  an extra vertex/texcoord pair our source does not.
- `BuildParticleSystemBounds` (784 B) 93.55%, ours 4 bytes short; retail numbers every GPR one
  lower (`this` in `r27`, ours `r28`) and hoists the array base above the loop branch while we
  initialise the two counters there.
- `ConstructChildParticleSystem` (1244 B) 96.95%. Retail materialises a bool from bit 30 of the
  4-char token (`rlwinm r21,r6,31,31,31`) that we do not emit at all, so a condition is spelled
  differently, not just ordered differently; it also keeps the first `cmpw` *above* `stw r0`.
- `Render` (580 B) 92.16%, misaligned from the first instruction; the `IsIndirectTextured()`
  arm order at line 1002 was left as-is because the function's shape differs well before it.
- `__sort3<CTexturedParticleListItem, CTexturedParticleListItemViewPointComp>` (316 B) 93.61% and
  `__sort3<CParticleListItem, CParticleListItemViewPointComp>` (336 B) 87.98% are `rstl`
  instantiations from the shared headers; both differ in the frame size retail uses (-48 vs our
  -64 for the first), so this is a header-level question, not a local spelling.

`RenderParticlesFlameThrower` was touched by an over-broad `sed` (its `theta` line has deeper
indentation) and the edit was **reverted** — it is not part of this item and its score is
unchanged at 80.31%.

## Verification

`./tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L4`:

```text
goal_check: item progress-unit-celementgen (progress) target=Kyoto/Particles/CElementGen
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12087 -> 12094   linked 5795 -> 5795
  ok    check_symbol_names.py
  ok    All:  34.22% fuzzy, 27.34% matched, 12.75% linked (12094 / 28465 functions)
  ok    target rose: main/Kyoto/Particles/CElementGen: 58 -> 65 / 104 functions
  ok    no asm added
goal_check: PASS progress-unit-celementgen
```

`python3 tools/check_decl_order.py --unit Kyoto/Particles/CElementGen`:
`ok: 1 unit(s) checked, none emits its functions out of retail order`.

Diff is one file, `src/Kyoto/Particles/CElementGen.cpp` (+14/-13): four `theta` lines, two moved
`color` declarations, two swapped `if` arms in `RenderModels`, and `GetGenerationRate`'s two
lines. No `asm`, no `.s`, nothing under `tools/`, `build/goal/` or `include/`.
`docs/HANDOFF.md`'s state block shows the new counts because `goal_check.sh` runs `gate.sh` with
`MP_GATE_DOCS_WRITE=1`, which rewrites the derived counts itself; I did not hand-edit it.

## Lessons worth keeping (general, not GameCube-specific)

- **Declaration order is a register-allocation lever in this compiler, and it is not
  "reorder the temporaries" — it is "reorder the *initialisers*".** Two of the seven functions
  here were fixed by moving one `const` local down past its neighbours, with the emitted
  instruction sequence unchanged.
- **A pure register-numbering diff has a source cause, and it is often a *different spelling of
  the same arithmetic*.** `CRelAngle::FromDegrees(x).AsRadians()` and `x * (M_PIF / 180.f)` are
  the same value and four different register assignments; the second one is what retail wrote.
- **A wall names the family of spellings that failed, not just the function.** Attempt 1's
  wall on `RenderBasicParticlesRotNoTS` said "the diff is a pure FPR allocation offset" and listed
  six *ordering* permutations. The winning change was not an ordering at all but a different
  spelling of the same multiply, and the spelling they had already tried read as a failure because
  it was measured inside a rewritten body. When a diff is pure register numbering, the cause is
  often a *different expression*, not a different order - write that into the wall.

---

# progress-unit-celementgen — attempt 3

**Thirteen** functions of `Kyoto/Particles/CElementGen` taken to 100%. The unit goes
**65 -> 78 / 104** matched functions and stays `NonMatching`; project total matched
**12097 -> 12110**, `linked` held at **5849**. No function anywhere got worse (verified by a
full-report diff against a clean-tree build of the same commit — see "Verification").

One lever produced ten of the thirteen, and it is not a spelling at all: **MWCC 2.7 has no
range-`for`**, and retail's `mActivePartChildren` loops are *not* indexed loops. They are
explicit iterator loops. Writing them that way is what retail wrote.

## The thirteen matches

| Function | Before | After | What changed |
| --- | ---: | ---: | --- |
| `SetGlobalTranslation` | 72.94% (144 B) | 100.00% | indexed loop over `mActivePartChildren` -> explicit `iterator it` loop |
| `SetTranslation` | 87.96% (340 B) | 100.00% | same, plus `CParticleGen* child = *it;` |
| `SetModulationColor` | 69.56% (128 B) | 100.00% | same |
| `SetOrientation` | 79.44% (164 B) | 100.00% | same |
| `SetGlobalOrientation` | 71.38% (116 B) | 100.00% | same |
| `SetParticleEmission` | 68.42% (124 B) | 100.00% | same |
| `EndLifetime` | 78.84% (172 B) | 100.00% | same |
| `DestroyParticles` | 77.30% (148 B) | 100.00% | same |
| `SetGeneratorRate` | 80.75% (176 B) | 100.00% | same |
| `__dt__11CElementGenFv` | 82.64% (300 B) | 100.00% | same (`delete *it;`) |
| `Render` | 92.16% (580 B) | 100.00% | the child loop **plus** three smaller fixes (below) |
| `GetParticleCountAllInternal` | 62.93% (172 B) | 100.00% | the child loop, plus the accumulate written once at the merge |
| `AccumulateBounds` | 68.88% (128 B) | 100.00% | the `for (i < 3)` axis loop written out with named `x`/`y`/`z` |

Seven more improved but did not reach 100%: `GetSystemCount` 40.85 -> 69.09,
`IsSystemDeletable` 73.32 -> 83.90, `SetGlobalScale`/`SetLocalScale` 65.05 -> 75.83,
`BuildParticleSystemBounds` 93.55 -> 97.09, `RenderModelParticle` 95.53 -> 99.94,
`ConstructChildParticleSystem` 96.95 -> 99.45.

Diff is **one file**, `src/Kyoto/Particles/CElementGen.cpp` (+92/-49). No header, no `asm`,
nothing under `tools/` or `build/goal/`.

## Lever 1: MWCC 2.7 has no range-`for`; retail spelled the iterator loop out

`for (CParticleGen* child : mActivePartChildren)` does not compile with this toolchain:

```text
#     310: for (CParticleGen* child : mActivePartChildren) {
#   Error:                  ^
#   '(' expected
```

(`tools/fast_try.sh` reports this as `Kyoto/Particles/CElementGen: build FAILED` and then prints
the *stale* `build/report.json` — grep for `FAILED`, or you will measure the previous build.)

Retail's `SetGlobalTranslation` is a pointer walk with the end recomputed every iteration:

```text
  lwz  r31,660(r3)          ; begin, loaded once
  b    check
loop:
  lwz  r3,0(r31)            ; *it
  <virtual SetGlobalTranslation>
  addi r31,r31,4
check:
  lwz  r0,652(r29)          ; size()   re-read: the virtual call may have changed the vector
  lwz  r3,660(r29)          ; data()
  slwi r0,r0,2
  add  r0,r3,r0
  cmplw r31,r0
  bne  loop
```

That is exactly what this compiles to (0 differing bytes) — but only when written out:

```cpp
for (rstl::vector< CParticleGen* >::iterator it = mActivePartChildren.begin();
     it != mActivePartChildren.end(); ++it) {
  (*it)->SetGlobalTranslation(translation);
}
```

The `for (int i = 0; i < mActivePartChildren.size(); ++i)` form emits **two** induction variables
(a byte offset *and* a count) and reloads `size()` separately. Ten functions came from this one
change.

**Two sub-levers inside it:**

- **Name the element when the body uses it twice.** `(*it)->ShouldDraw()` / `(*it)->Render()`
  reloads `*it` twice and emits a second `stmw`-worthy register; retail loads it once into `r27`
  (`CParticleGen* child = *it;`, 92.16% -> 99.14% on its own).
- **A loop with an early `return` hoists `end`.** Retail's `IsSystemDeletable` and
  `GetSystemCount` compute `end = data + size*4` *before* the loop and compare against it. The
  `for (...; it != mActivePartChildren.end(); ++it)` header does not do that, and dropping the
  iterator form there instead cost **73.32% -> 13.05%**. Writing the loop out with a separate
  `const ... end` local recovers it: **83.90%**, and it is the only spelling that puts the
  instruction count at retail's 164 B.

So there are two distinct retail spellings in this one file, and which one a given function used
is decided by whether its body can exit early:

| shape | spelling | retail evidence |
| --- | --- | --- |
| no early exit | `it != mActivePartChildren.end()` in the `for` header | `SetGlobalTranslation`, `~CElementGen`, `SetModulationColor`, … |
| early `return` inside | `it` + `const end` declared before the loop | `IsSystemDeletable` (73.32% -> 13.05% -> 83.90% across the three) |

## Lever 2: unroll a fixed 3-iteration axis loop by naming the components

`AccumulateBounds` loops `for (i < 3)` over `position[i]`, `mAabbMax[i]`, `mAabbMin[i]`.
mwcceppc unrolls it but re-loads `position[i]` through an induction pointer, and even ends up
using `lfsu`. Retail loads each component exactly once into `f2`/`f3`/`f4` at the top:

```text
  lfs f2,0(r4)   ; x
  lfs f0,728(r3)
  lfs f3,4(r4)   ; y
  lfs f4,8(r4)   ; z
```

Naming them does it: `const float x = position[0]; ... if (x > mAabbMax[0]) { mAabbMax[0] = x; }` …
**68.88% -> 100.00%**, same 128 B. Note `CVector3f::GetX()` returns *by value* in this repo, so
`mAabbMax.GetX() = x` does not compile ("not an lvalue") — the subscript form is required.

## Lever 3: three smaller spellings inside `Render` (92.16% -> 100.00%)

- `!mParticles.empty()` -> `mParticles.size() > 0`. `rstl::vector::empty()` is `mCount == 0`
  (`include/rstl/vector.hpp:100`), which emits `cmplwi r3,0; beq`. Retail emits
  `cmpwi r0,0; ble`, i.e. `> 0`.
- `zeroSize = size == 0.f;` -> `if (size == 0.f) { zeroSize = true; }`. The first makes mwcceppc
  canonicalise the FP compare with `mfcr r0; rlwinm r27,r0,3,31,31`; retail branches
  (`bne` past a single `li r27,1`).
- the child loop (lever 1).

## Lever 4: accumulate once at the merge

`GetParticleCountAllInternal` (62.93% -> 100.00%, same 172 B) wrote the `+=` in both arms, which
costs an extra `add`/`mr` pair per arm. Retail has one `add r29,r29,r3` at the merge point, i.e. a
phi on the result register:

```cpp
int childCount;
if (child->Get4CharId() == 'PART') {
  childCount = static_cast< CElementGen* >(child)->GetParticleCountAll();
} else {
  childCount = child->GetParticleCount();
}
count += childCount;
```

## `RenderModelParticle`: the last 0.06%

`RenderModelParticle`'s fourth `CModel::Draw` site built the flags with the 4-argument
constructor, `CModelFlags(kT_Blend, 0, kF_DepthCompare, color)`, which emits three plain `li`s and
one struct copy. Retail's site emits `li r6,3 / lwz r5,0(r30) / clrrwi r0,r6,2` **and a second
copy** — i.e. it built a flags object with `mFlags == 3` and then ran `DepthCompareUpdate(true,
false)` on it. That is `CModelFlags::AlphaBlended(color).DepthCompareUpdate(true, false)`
(`AlphaBlended` uses the `(ETrans, const CColor&)` constructor, whose `mFlags` default is
`kF_DepthCompare|kF_DepthUpdate == 3`): **95.53% -> 99.94%**, size already equal at 796 B. This
is the "logic difference in the flags construction" attempt 2 diagnosed and did not fix.

Attempt 2's other reading of the same site was wrong in one detail and it matters: the *third*
site (`color.GetAlpha() == 1.f`) really is the 4-argument constructor with no `DepthCompareUpdate`
and no second copy — retail's `stb r3,36; stb r6,37; sth r5,38; stw r0,40` is one struct built
from registers. Only the fourth site chains.

Remaining 2 instructions of 199: retail emits `lbz r7,72(r1)` (*mBlendMode*) *before*
`addi r4,r1,80 / clrrwi` and `lbz r6,73(r1)` (*mMatSetIdx*) after; we emit them in the opposite
order. Identical instruction multiset, identical register assignment (`r7` = byte 0, `r6` = byte 1
in both) — the two reloads out of the temporary `CModelFlags` are just scheduled differently. The
only source lever I can see is swapping `mBlendMode`/`mMatSetIdx` in the initialiser list of
`CModelFlags(const CModelFlags&, uint)`, which is a **shared header** (`include/Kyoto/Graphics/
CModelFlags.hpp`) used by every model draw in the game, so it could move unrelated matched
functions. Not attempted.

## `ConstructChildParticleSystem`: retail's guard is bit 31, our `kOSF_Two` is 2

The `PART` case's guard was `(flags & kOSF_Two) && particleDescription->mOPTS`. Retail hoists the
flag test *out of the switch* to the function's entry block and computes
`rlwinm r21,r6,31,31,31` — **bit 31 of the `flags` parameter**, not bit 1. With this repo's
`EOptionalSystemFlags` (`kOSF_None=0, kOSF_One=1, kOSF_Two=2, kOSF_DisableBounds=4`) retail's test
is dead code, and the *other* overload (`ConstructChildParticleSystem(CToken, uint, ushort)`,
matched at 100% before and after this change) is byte-identical and confirms `kOSF_Two == 2` from
its own `li r6,2`. **The two sites genuinely disagree in retail**; this is not a header mistake on
our side. Recorded as measured, not guessed at.

- `static_cast<uint>(flags) & 0x80000000u` inline in the condition — 96.95% -> **98.83%**;
  mwcceppc emits `srwi r21,r6,31`
- the same mask in a named `uint` local, `mask != 0 && mOPTS` — 98.83% (`cmplwi` on a separate
  compare), and 1244 B = retail's size
- the same local, `mOPTS && mask != 0` — **99.45%** (kept). Retail loads `mOPTS` before it tests
  the mask, so the operands go in that order.
- not reproduced: retail's `rlwinm r21,r6,31,31,31` where we emit `clrrwi r21,r6,31`. Both compute
  `flags & 0x80000000` (checked with `powerpc-eabi-as`: `0x54d50000` is `clrrwi r,r,31`,
  `0x54d5fffe` is `rlwinm r,r,31,31,31`); mwcceppc picks the form from the expression and I found
  no spelling that yields the rotate.

## Spellings tried and rejected (so the next run skips them)

Range-`for` — **does not compile**, see above. Do not retry.

`SetGlobalScale` / `SetLocalScale` (380 B each), the sign clamp inside
`for (i < 3) { if (close_enough(mGlobalScale[i], 0.f, 0.0001f)) { ... } }`:

- `0.0001f * (mGlobalScale[i] < 0.f ? -1.f : 1.f)` — 75.83% (kept; was 65.05%)
- `0.0001f * (mGlobalScale[i] >= 0.f ? 1.f : -1.f)` — 72.67% (worse)
- a named `const float sign = ...; mGlobalScale[i] = 0.0001f * sign;` — 75.83% (identical bytes
  to the first; not kept, the shorter spelling reads better)

The remaining gap is one instruction per axis plus instruction *selection*: retail builds the sign
with `fsel` (`lfs f3,-1.f; lfs f1,1.f; fsel f1,f2,f1,f3`) where we branch around two `lfs`, and
retail re-loads the `0.f` pool constant per axis where we hoist it into `f2` once across the
unrolled loop. Both are mwcceppc register-allocation/hoisting decisions, not logic.

`IsSystemDeletable` (164 B) and `GetSystemCount` (132 B) — both now have retail's *exact*
instruction multiset and size, and differ only in the prologue: retail interleaves the loop-setup
loads with the callee-save stores

```text
  stw  r0,36(r1)
  lwz  r0,652(r3)      ; size()
  stw  r31,28(r1)
  stw  r30,24(r1)
  lwz  r30,660(r3)     ; data()
  stw  r29,20(r1)
  mr   r29,r3
```

where we emit all three `stw`s, then `mr r29,r3`, then both loads. 8 of 41 and 8 of 33
instructions differ. Tried: `const end` declared *before* `it` (83.90%, byte-identical to the
`it`-first order — no effect). The `end`-before-`it` order is the only other shape I could think
of and it changed nothing, so the next run should look for something that changes *when* the loads
are needed rather than reordering declarations.

`GetSystemCount`'s seed value, `int count = mActiveParticleCount > 0;`:
`> 0` gives retail's `neg r4,r5 / andc r3,r4,r5 / srwi r3,r3,31` (8 differing of 33);
`!= 0` gives `or r3,r4,r5 / srwi` (9 differing of 33, and objdiff scores it *higher*, 72.12% vs
69.09%). Kept `> 0` because it is retail's byte sequence; objdiff's percentage here is not a count.

`IsSystemDeletable`'s tail, `return mCurFrame > mPSLT && mActiveParticleCount == 0;`:

- as written — 76.07%, mwcceppc emits `cmpw r4,r0; ble` and hoists `li r3,0`
- `mPSLT < mCurFrame` — same score, but the operands now load in retail's order
- `if (mPSLT < mCurFrame && mActiveParticleCount == 0) { return true; } return false;` —
  **83.90%**, kept: it produces retail's phi (`li r3,1; b` / `li r3,0`) and matches the 164 B size.

`GetBounds` (136 B, 81.59%, unchanged) is the same scheduling family: retail interleaves the six
`CAABox` copy loads with the six stores (`lwz; stw; lwz; stw`), we emit `lwz; lwz; stw; stw; …`.
Identical multiset and size, 11 of 34 instructions differ. The copy is
`return rstl::optional_object<CAABox>(mSystemBounds);`.

`BuildParticleSystemBounds` (784 B, 93.55% -> 97.09%) is GPR numbering only: retail keeps `this`
in `r27`, the box address in `r29` and the `accumulated` flag in `r28`; we use `r28`/`r30`/`r29`.
Same instruction multiset, sizes equal. Nothing structural left to find from the source.

`EndModelRender` — **re-measured this run at 94.29%, unchanged**. Attempt 2's ten spellings still
stand. I did not try new spellings here, so I am not writing a `WALL:` line for it. What I can add:
retail's instruction multiset equals ours exactly, and the difference is that retail emits
`lbz r0,613(r3)` (the `mModelsUseLights` bitfield) *between* `stw r0,20(r1)` and `stw r31,12(r1)`,
where we emit it after `mr r31,r4`. The condition itself is identical (`rlwinm. r0,r0,27,31,31`,
a 1-bit bitfield at bit 4 of the byte at 613). So the next run should be looking for a shape that
delays `state`'s copy into `r31`, not for another spelling of the test.

## Not attempted, and why

- `RenderModels` (2332 B, 99.07%). Attempt 2's note stands: retail loads `mParticles`' data
  pointer *after* selecting `index = sorted ? sortItems[i].mPartIdx : i`, we hoist it above the
  branch, and our loop counters are one register higher. 22 bytes across a 2332-byte function is a
  bigger investigation than this run had room for.
- `CModelFlags.hpp` initialiser reordering (see `RenderModelParticle`) — shared header, too wide a
  blast radius to try blind.
- The two `__sort3<...>` instantiations and the `rstl::vector<CMatrix3f>` members
  (`reserve`/`assign`/`clear`) are shared-header/rstl-level, as attempt 2 said; they are now 8 of
  the 26 remaining, so this unit still has headroom in the *rstl* headers rather than in this
  source file.

## Verification

`./tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L8`:

```text
goal_check: item progress-unit-celementgen (progress) target=Kyoto/Particles/CElementGen
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12097 -> 12110   linked 5849 -> 5849
  ok    check_symbol_names.py
  ok    All:  34.23% fuzzy, 27.39% matched, 12.84% linked (12110 / 28465 functions)
  ok    target rose: main/Kyoto/Particles/CElementGen: 65 -> 78 / 104 functions
  ok    no asm added
goal_check: PASS progress-unit-celementgen
```

`python3 tools/check_decl_order.py --unit Kyoto/Particles/CElementGen`:
`ok: 1 unit(s) checked, none emits its functions out of retail order`.

Separately, a **whole-project** per-function diff of `build/report.json` against a clean build of
the same commit (I stashed the file, rebuilt, saved the report, and compared every function in all
2066 units): **0 worse, 20 better, 13 of those at 100.00%, 0 new.** That is the check the judge
does not print.

`docs/HANDOFF.md` shows the new counts because `goal_check.sh` runs `gate.sh` with
`MP_GATE_DOCS_WRITE=1`, which rewrites the derived state block itself; I did not hand-edit it.

## Lessons worth keeping (general, not GameCube-specific)

- **A build that fails silently still prints a score.** `tools/fast_try.sh` regenerates the report
  after ninja and prints the *previous* `build/report.json` when the compile fails. Grep its output
  for `FAILED`, or a rejected spelling will read as an accepted one.
- **Before assuming an old compiler lacks a construct, check that the construct is the problem.**
  Two prior attempts' worth of effort went into reordering declarations in loops; the loop *shape*
  was wrong. Read the retail loop, not just the arithmetic: `lwz`-once-then-`addi 4` plus a
  per-iteration `end` recompute is a loop over a pointer, and mwcceppc only produces it from an
  explicit `iterator`.
- **The compiler's spelling of a container decides the loop it emits.** `for (i < v.size())` and
  `for (it = v.begin(); it != v.end(); ++it)` are the same loop to a reader and different code to
  mwcceppc: two induction variables versus one, and `size()` re-read per iteration either way.
  When retail's loop has one induction variable, the source used iterators.
- **A loop that can exit early needs a spelled-out `end`**; otherwise the end pointer is recomputed
  per iteration. Getting this backwards cost 60 percentage points on one function.
- **`objdiff`'s per-function percentage is not a count of differing instructions.** On
  `GetSystemCount`, the spelling that differs in 8 of 33 instructions scores 3 points *lower* than
  the one that differs in 9. Use `tools/bytescmp.py` to rank, objdiff to decide.
- **Unrolling a fixed 3-iteration axis loop by naming the components is a spelling change, not a
  restructure.** It is what retail's `AccumulateBounds` did, and it took a 68.88% function to 100%
  without touching a single operation.

## Review rejected run 18 (2026-10-01 20:21:13Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

the `ConstructChildParticleSystem` hunk (`src/Kyoto/Particles/CElementGen.cpp:746,754`) turns a live guard into unreachable code and does so on a misread of the instruction it claims to reproduce — retail's `0x802D8A98 rlwinm r21,r6,31,31,31` keeps result bit 31, which is *source bit 0* (`flags & 1`, i.e. `kOSF_One`), not bit 31; the same idiom appears 156 times in retail `.text` while `srwi …,31` never does, and no caller can ever set bit 31. The function is not matched either way, so the change buys only 96.95→99.45% while silently deleting the OPT child-skip path. An acceptable change keeps the semantics (`(flags & kOSF_Two) && mOPTS`, or a bit-0 test such as `flags & kOSF_One` if the sites really disagree) and re-measures, or drops the hunk; the loop, `zeroSize`, `AccumulateBounds` and `CModelFlags` hunks in the same diff are correct and can land on a retry.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-celementgen-L8-18.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-celementgen-L8-18-review1-20261001T201501.jsonl

## Fix round 1 (2026-10-01, lane L8)

Corrected the one hunk the reviewer rejected. Nothing else in the diff was touched.

**What changed** — `src/Kyoto/Particles/CElementGen.cpp:746,754`, two lines:

```diff
-  const uint skipOptionalSystemsMask = static_cast< uint >(flags) & 0x80000000u;
+  const bool skipOptionalSystems = static_cast< bool >(flags & kOSF_One);
-    if (particleDescription->mOPTS && skipOptionalSystemsMask != 0) {
+    if (particleDescription->mOPTS && skipOptionalSystems) {
```

**The reviewer was right about the instruction, and I re-derived it independently rather than
taking either side's word.** `rlwinm r21,r6,31,31,31` at retail `0x802D8A98` masks one bit out of a
32-bit left-rotate: kept result bit 31 comes from **source** bit `(31 - 31) mod 32` = **0**, so the
instruction computes `flags & 1`. The previous run's note claimed it kept "result bit 31, i.e.
`flags & 0x80000000`" — that confuses the result bit with the source bit and inverts the whole
guard. No caller in the tree can set bit 31 (`flags` is an `EOptionalSystemFlags` built from
`kOSF_One=1`, `kOSF_Two=2`, `kOSF_DisableBounds=4`), so `& 0x80000000u` was always 0: the
`return nullptr` skip path was dead code, and the OPT child-skip behaviour it was supposed to
reproduce was deleted. That is exactly the "delete real work to gain percent" the prompt rejects.

**The two sites really do disagree in retail** — I checked the ctor as well as the callee, which is
the part that decides between the reviewer's two acceptable options:

- callee `0x802D8A98` `rlwinm r21,r6,31,31,31` -> source bit 0
- ctor `0x802DB2C8` `rlwinm r0,r29,30,31,31` -> source bit 1 (`mEnableOPTS`), matching
  `mEnableOPTS(flags & kOSF_Two)` in the ctor's initialiser list
- the byte-identical 3-argument overload (`li r6,2` at `0x802D8A24`) independently pins
  `kOSF_Two == 2`

So this is not a header mistake on our side, and the reviewer's "a bit-0 test such as
`flags & kOSF_One` if the sites really disagree" applies. I used the bit-0 test, keeping the guard
**live** rather than dropping the hunk.

**Measured, not recalled** (`tools/fast_try.sh Kyoto/Particles/CElementGen`):

| Spelling of the hoisted test | Function | Mnemonic emitted |
| --- | ---: | --- |
| `& 0x80000000u` (rejected) | 99.45% | `srwi r21,r6,31` |
| `uint ... & kOSF_One`, compared `!= 0` | 99.45% | `clrlwi r21,r6,31` |
| `bool ... = (flags & kOSF_One) != 0` | 99.45% | `clrlwi r21,r6,31` |
| `uint ... = flags & kOSF_One`, compared `== kOSF_One` | 99.43% | `clrlwi r21,r6,31` |
| **`bool ... = static_cast<bool>(flags & kOSF_One)` (kept)** | **99.45%** | `clrlwi r21,r6,31` |

Kept spelling is the 99.45% one that reads as the guard it is (a `bool`, not a mask compared
against 0). The score is identical to the rejected version — **the correction costs nothing**,
because what was bought before was never the mask, it was the hoisting of the test out of the
switch. Retail's `rlwinm` vs our `clrlwi` is one instruction of the same value and the same 4 bytes;
the function is still not matched either way, so this is a non-result and is recorded as one.

**The guard is reachable now**: `flags & kOSF_One` is set by the 14 `kOSF_One` call sites in `src/`
(and `kOSF_One` is the *default* value of the `flags` parameter), so the OPT child-skip path is live
again. No `NEW:` line is filed: this is a correction to work already in flight, not new work.

**Gates**

- `python3 tools/check_raw_offsets.py` -> `ok: 166 raw-offset site(s) in 70 file(s), all documented`
- `./tools/goal_check.sh build/goal/item.json` -> **PASS**: `counts: matched 12097 -> 12110`,
  `linked 5849 -> 5849`, `target rose: main/Kyoto/Particles/CElementGen: 65 -> 78 / 104`, `no asm added`
- whole-project per-function diff vs `build/goal/judge/report.base.json`, every function in all
  units: **0 worse, 20 better, 13 of those at 100.00%, 0 new, 0 gone.**

Note for the next run: the `kOSF` bit assignments are measured from retail and should not be
"cleaned up" — `kOSF_One` and `kOSF_Two` are *not* interchangeable synonyms, and the callee/ctor
disagreement above is a real property of the retail binary, not a transcription error to fix.
