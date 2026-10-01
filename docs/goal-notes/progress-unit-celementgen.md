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
