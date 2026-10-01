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
