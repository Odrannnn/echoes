# progress-prime1-cdecal — `Weapons/CDecal`

`kind: progress`, target `main/Weapons/CDecal`. Stayed `NonMatching`; no `flip_test.sh` was run
and `configure.py` was not touched. The whole change is in `src/Weapons/CDecal.cpp` (one file, no
header edits, so no other unit's `.text` can move).

## Result

`tools/goal_check.sh build/goal/item.json` → **PASS** (`target rose: main/Weapons/CDecal: 6 -> 8 /
17 functions`; matched 9908 → 9910; 0 regressions; no asm).

`build/report.json` for `main/Weapons/CDecal`, before → after:

| function | bytes | before | after |
|---|---|---|---|
| `Render__6CDecalCFv` | 288 | 91.25 % | **100.0 %** |
| `RenderQuad__6CDecalCFRQ26CDecal10CQuadDecalRCQ217CDecalDescription10SQuadDescr` | 1604 | 95.75 % | **100.0 %** |
| `RenderMdl__6CDecalCFv` | 1220 | 94.17 % | 94.17 % (unchanged) |
| unit `matched_functions` | | 6 / 17 | **8 / 17** |
| unit `fuzzy_match_percent` | | 75.89 % | 76.98 % |

Nothing else in the tree moved: the gate's per-function diff reports `+2 functions at 100%, 0 units
newly linked`, and `All:` went 9908 → 9910 matched with fuzzy flat at 30.50 %.

## Per function, against Metroid Prime 1

Prime 1's `src/Weapons/CDecal.cpp` is the right prior but the two games' engines have forked
(`CModelFlags` is 0x8 bytes in Prime 1 and 0xc here because of `CModelFlags::x0_`; Echoes adds
`CDecal::mDisableAlphaUpdate`, `mEnableClippedGeometry`, `CQuadDecal::mPolygons`, `CDecalPolygon`,
`BuildClippedGeometry`, and the `direction`/`surfaces` ctor parameters). No header was copied and no
class layout was changed.

### `Render__6CDecalCFv` — 91.25 % → 100 % (Prime 1's source needed one small edit)

Same size in both games (0x120), and Prime 1's `Render` shape is right. The only difference was
register allocation: retail keeps `this` in r30 and the `SQuadDescr` address in r31 across the two
`CParticleGlobals` calls, ours put `this` in r31 and reloaded `mDescription` (offset 8) three times.
Binding each description to a local reference reproduces it exactly:

```cpp
const CDecalDescription::SQuadDescr& quad1Desc = mDescription->mQuad1;
if (!quad1Desc.mTEX.null() && !(mFlags & 1)) { ... RenderQuad(mQuad1, quad1Desc); }
const CDecalDescription::SQuadDescr& quad2Desc = mDescription->mQuad2;
if (!quad2Desc.mTEX.null() && !(mFlags & 2)) { ... RenderQuad(mQuad2, quad2Desc); }
```

27 differing instructions → 0. The third block (`mDMDL`) still reloads `mDescription`, exactly as
retail does, so the alias must be per-block, not one hoisted pointer.

Spellings tried (differing-instruction counts, `tools/try_batch.py`):

| spelling | diffs |
|---|---|
| baseline | 27 |
| one `const CDecalDescription* desc = mDescription.operator->();` per block, `desc->mQuad1` etc. | 5 |
| local references **inside** the `if` bodies (after the `.mTEX` test) | 5 |
| local references **before** each `if` (**kept**) | **0** |

The 5-diff variants load `mDescription` into the callee-saved register but then add 28 to get
`&mQuad2` instead of using the register directly, so the binding has to happen before the test.

### `RenderQuad__6CDecalCFR...` — 95.75 % → 100 % (Prime 1's source needed two small edits)

Two independent fixes, both confirmed by disassembly of the retail-derived object
`build/G2ME01/obj/Weapons/CDecal.o`:

1. **A missing call.** Retail has `bl GetObj__6CTokenFv` + `addi r3,r1,28` between
   `CTexture::Load` and `CGraphics::SetTevOp`, with the result discarded. Prime 1 has the same line
   (`tex.GetObj(); // ?`, its `CDecal.cpp:94`). Added verbatim with a comment saying the call is not
   removable.
2. **`decal.mHalfSize` reloaded per vertex.** Retail reads it once into the callee-saved `f30`
   (`lfs f30,24(r29)` at 0x1AF4, reused for all 8 `GXPosition3f32` calls) and uses `fneg f2,f30` for
   the negated vertex. Ours reloaded `24(r29)` for every vertex and had no `f30`, so the frame was
   160 instead of 176 and the f31 save sequence differed. Reading it into a local **after**
   `CGX::Begin(...)` and using `size` in the vertex block reproduces it.

48 differing instructions → 0. Placement matters:

| spelling | diffs |
|---|---|
| baseline | 48 |
| `tex.GetObj()` only | 46 |
| local `size` only | 4 |
| `tex.GetObj()` + `size` declared **before** `CGX::Begin` | 2 (the `lfs f30` scheduled one slot early) |
| `tex.GetObj()` + `size` declared at the top of the `else` branch | 17 (loaded into f31, then f30 conflicts) |
| `tex.GetObj()` + `size` declared **after** `CGX::Begin` (**kept**) | **0** |

### `RenderMdl__6CDecalCFv` — 94.17 %, unchanged. Not solved.

Prime 1's source is nearly identical here and does not transfer: this is 1220 bytes against Prime
1's 1188, the difference is `CModelFlags`' extra `x0_` word plus its `if`-based `DepthCompareUpdate`
versus Prime 1's arithmetic one. Both games emit the same `clrrwi r0,r6,2` / `ori r0,r0,1`, so the
flag arithmetic is not the problem.

The remaining diff is **all register and stack-slot allocation**, not logic. Ours allocates one more
callee-saved register than retail (r28 for `dmrtIsConst` where retail uses r29, and a dead r30 that
is only ever spilled), which shifts every local by 4 bytes and cascades; in the `mDMAB` branch ours
materialises the final `CModelFlags` at 116(r1) (with the dead r30 spilled into its `x0_` field) and
stores the colour a third time, where retail uses 84(r1) and stores it twice.

Baseline 208 differing instructions (`tools/try_batch.py`). Tried, all worse or equal:

| spelling | diffs | objdiff % |
|---|---|---|
| baseline | 208 | 94.17 % |
| `CVector3f offset(0.f, 0.f, 0.f);` (Prime 1's form) | **192** | **92.02 % — worse, reverted** |
| `CVector3f rotation(0.f, 0.f, 0.f);` | 275 | |
| `CVector3f scale(0.f, 0.f, 0.f);` | 212 | |
| all three Prime 1 constructors | 194 | |
| `CVector3f dmrtXf(CTransform4f::Identity());` / `= Identity()` | 208 | |
| `CTransform4f rotXf = CTransform4f::Identity();` | 208 | |
| `bool dmrtIsConst(false);` | 208 | |
| `color.GetAlpha() == 1.0f` | 208 | |
| `CVector3f offset; offset = CVector3f::Zero();` | 208 | |
| `CVector3f rotation;` / `scale;` split initialisation | 192 | |
| `CColor` and `offset` declarations swapped | 196 | |
| `CModelFlags flags(kT_Opaque, 1.f);` assigned in each branch | 203 | |
| `const CModelFlags flags` named in the `AlphaBlended` branch only | 193 | |
| `const CModelFlags flags` named in all three branches | 228 | |
| one `CModelFlags flags;` for all three branches | build failure (no default ctor) | |
| `const TToken<CModel>* mdl = &mDescription->mDMDL;` | build failure (`mDMDL` is a value) | |

Not filed as `NEW:`, per the rule that a measured wall belongs in this file: matching `RenderMdl`
would raise the count, but it needs the exact stack-slot assignment and nothing in Prime 1's source
points at it.

## Lesson worth keeping (this is a `PROCESS_LESSONS.md` candidate, not filed)

**Fewer differing instructions is not a better objdiff score, and only objdiff's score is the
measurement the judge reads.** `CVector3f offset(0.f, 0.f, 0.f);` cut `RenderMdl`'s instruction diff
from 208 to 192 — the *best* result in the whole search — and dropped its fuzzy percentage from
94.17 % to 92.02 %, which would have failed the item's "no function anywhere gets worse" test.
`tools/try_batch.py` is the right tool for finding *what* to change; re-measure with
`objdiff-cli report generate` before keeping a variant.

Second, smaller: the two wins above are the same codegen bug in two places. When retail holds a
value in a register across a call and MWCC reloads it, binding it to a local (a reference for an
address, a `float` for a scalar) is the fix, and the declaration has to sit at the point where retail
loads it, not anywhere convenient.

## Commands run

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/decomp_build.sh                       # All: 30.50% fuzzy, 22.60% matched, 11.74% linked (9910 / 28465)
MP_GOAL_TREE=$PWD MP_GOAL_BASE=$PWD/build/goal/judge/report.base.json \
  ./tools/goal_check.sh $PWD/build/goal/item.json    # PASS
```

`docs/HANDOFF.md` was rewritten by the judge's own `MP_GATE_DOCS_WRITE=1` run (state block only) and
reverted with `git checkout`; `git status` is left with `src/Weapons/CDecal.cpp` as the only
modified file. No commit was made.
