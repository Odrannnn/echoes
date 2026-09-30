# progress-prime1-cprojectedshadow — `MetroidPrime/CProjectedShadow` 4/8 → 5/8 functions, unit 32.45% → 58.16% fuzzy

**`goal_check.sh`: PASS.** `matched 10380 -> 10381`, `linked 5048 -> 5048` (unchanged — nothing
flipped, correct for a `progress` item), `+1 functions at 100%`, no function anywhere worse, no `asm`
added, full `gate.sh` green (DOL sha1, 86 RELs, docs claims, decl order, port probe, port link gap,
reach stubs).

Both functions the item named are worked. `ExpandBoundsForTexture` is **finished (100%)**;
`RenderShadowBuffer` went 21.62% → **85.59%**. Two files touched plus one header enumerator.

## Starting position, measured on the clean tree (`build/goal/judge/report.base.json`)

```
main/MetroidPrime/CProjectedShadow: 32.454% fuzzy, 4 / 8 functions
  100.0     92 B  ~CProjectedShadow
  100.0     68 B  SetBounds(const CAABox&)
  100.0     60 B  RenderShadowBuffer(CStateManager&, const CModelData&, ...)
  100.0    240 B  ScaleAndTranslateBounds(const CAABox&, const CVector3f&, float)
   98.50   264 B  CProjectedShadow::CProjectedShadow(int, int, uchar, int)
   82.30   252 B  ExpandBoundsForTexture()
   21.62  1480 B  RenderShadowBuffer(CStateManager&, int, const CModelData* const*, ...)
    0.29  1400 B  Render(const CStateManager&) const
```

## `ExpandBoundsForTexture` — 82.30% → **100.00%** (the whole function)

Prime 1's source matched **unchanged** and was not the answer. Prime 1 writes
`texelScale = 1.f / (width - 2)`; this tree wrote `3.f / (width - 2)`. Retail is neither: it
multiplies each extent by 3 *before* scaling, so the numerator is `3.f` but the reciprocal is
`1.f`.

```
retail  lfs  f8, <3.0f>
        fmuls f5, f8, f5      ; f5 = 3 * (max.y - min.y)      -- the *height*, times 3
        fdivs f10, f2, f0     ; f10 = 1.0f / (float)(width - 2)
        fmuls f1, f10, f5     ; offset.y = f10 * 3 * height
        fmuls f0, f10, f0     ; offset.x = f10 * 3 * width
ours    fdivs f9, f7, f2      ; f9 = 3.0f / (float)(width - 2)   -- one reciprocal, 3 baked in
        fmuls f7, f9, f1
```

Same values, different association, so the multiply lands on the other side of the divide. Three
instructions of 63 were the whole difference. The fix:

```cpp
const float texelScale = 1.f / (mTexture.GetWidth() - 2);
const CVector3f offset(3.f * mBounds.GetWidth() * texelScale,
                       3.f * mBounds.GetHeight() * texelScale, 0.f);
```

This is Prime 1's line with `3.f` multiplied into the extents — the shape is Prime 1's, the constant
is Echoes'. No header change.

## `RenderShadowBuffer` (the `int count, ...` overload) — 21.62% → 85.59% (1480 B)

The item's second named function. It was a stub (bounds loop, then a `TODO`); Prime 1's
`prime-ref/src/MetroidPrime/CProjectedShadow.cpp` L68-175 is the same function and was the base.
Prime 1's source needed **adapting, not just copying**: Echoes' retail differs from Prime 1's text
in five measured places, and the diffs are the instructions that moved.

| step | change | before → after |
|---|---|---|
| 1 | Prime 1's L94-175 body, plus the `mOverrideBounds` branch this tree already had | 21.62 → 75.46 |
| 2 | `GetRenderMode().xfbHeight` → `.efbHeight` in `SetViewport` | 75.46 → 75.46 (1 instruction: `lhz r0,6(r4)` vs `8(r4)`) |
| 3 | draw flags as `kDF_Flat \| kDF_Unsorted \| (flags == 0 ? kDF_Sorted : 0)` via a local | 73.46 → **85.59** |

**Step 1 detail — what Echoes' retail has that Prime 1's text does not.** Read off
`build/G2ME01/asm/MetroidPrime/CProjectedShadow.s`:

* `DrawFlat`, not a callback. Prime 1 builds an `SShadowDrawContext` and calls
  `model.Draw(callback, &context)`. Retail calls
  `DolphinDrawWithFlags__13CSkinnedModelCFPC24CPoseAsTransforms_LinearUiRC11CModelFlags` directly
  (0x801922BC) — no `SShadowDrawContext`, no function-local callback. The static branch is
  `PreDrawModel` then `DolphinDrawFlat` (0x80192308, 0x80192320), where Prime 1 has
  `UpdateLastFrame()` + `DrawFlat(...)`.
* The pose is `animData + 0x2b0` (0x801922B4), i.e. `CAnimData::mPose`, reached through a
  `const_cast` because the field is private and `GetAnimationData()` hands back a `const CAnimData*`.
* `SetupRender()` takes no arguments in this tree (`SetupRender__9CAnimDataCFv`), not Prime 1's
  `(model, optional, nullptr)`.
* The colour is `CColor(1,1,1,1)` in **both** branches (retail calls `__ct__6CColorFffff` twice,
  0x80192294 and 0x801922FC); Prime 1 has it only in the animated branch.
* `mEnabled = true` is a **bit-field store with an immediate** — `li r4,1` / `rlwimi r0,r4,7,24,24`
  at 0x80191F84 — not `mEnabled = true`. The tree's own `mEnabled = true` already emits that.

**Step 2.** Retail reads `lhz r0,6(r4)` where `r4` is `mRenderModeObj`; offset 6 is `efbHeight`
(`include/dolphin/gx/GXStruct.h`: `efbHeight` at 0x06, `xfbHeight` at 0x08). `xfbHeight` compiles
to `lhz r0,8(r4)`. **`xfbHeight` is what `CWorldShadow.cpp:164` uses** for the same purpose and is
also wrong there; do not copy it.

**Step 3.** Retail (0x80192298-0x801922B8):

```
cntlzw r0, r31 ; rlwinm r3, r0, 27, 31, 31 ; neg r4, r3 ; li r0, 4 ; and r0, r0, r4
ori   r5, r0, 10          ; r5 = (flags == 0 ? kDF_Sorted : 0) | 0xa
```

i.e. `kDF_Flat | kDF_Unsorted | (flags == 0 ? kDF_Sorted : 0)`, with `kDF_Flat | kDF_Unsorted`
folded to `0xa`. Spelling it as one `?:` over the whole mask emits a branch and scores **73.46%**
(worse — do not retry). Binding the conditional term to its own value first is what produces
MWCC's `cntlzw`/`neg`/`and`/`ori` chain:

```cpp
const uint drawFlags = CSkinnedModel::kDF_Flat | CSkinnedModel::kDF_Unsorted |
                       (flags == 0 ? CSkinnedModel::kDF_Sorted : static_cast< uint >(0));
```

### What still blocks 100% on this function — 14.4%

Three things, all measured, none of them a source spelling I found in the time available.

**(a) The frame is 16 bytes too big and every slot after `r1+40` is shifted by it.** `stwu r1,-496`
vs `-512`, and the whole saved-register area moves (`stmw r17,404` vs `stmw r18,424`). That is one
extra 16-byte stack object. Retail's slot map has `12,13,14` (the `CModelFlags` in the *static*
branch) and `24,25,26` (the one in the *animated* branch) — **two separate 12-byte temporaries,
one per branch**. Ours emits one, and the two `CModelFlags` values land at `20,40` and `28,29,30`.
Each `CModelFlags(CModelFlags::kT_Opaque, CColor(1,1,1,1))` is a distinct temporary in retail's
source (they are in different branches, so they cannot be one object); ours shares a slot, which
is also why ours carries 8 bytes the retail frame does not. The exact source shape that gives
MWCC two distinct slots was not found — this is the largest single item left.

**(b) `oldProjection` / `oldViewport` are copied whole here** (retail stores `0x68..0x7f` and
`0x80..0x98`, all six words each, i.e. both *floats* of `CViewport` and all seven fields of
`CProjectionState`). `CWorldShadow`'s notes record the opposite shape for its function — retail
reads only the four `int`s there. So the two differ per call site and the struct copy is right
here. Ours matches on this; nothing left to do.

**(c) The animated branch's tail.** Retail's `DolphinDrawWithFlags` argument registers are
`r3=model, r4=animData+0x2b0, r5=drawFlags, r6=&flags`; ours agrees. What differs is the
surrounding `CModelFlags` construction: retail materialises the `CColor` into `r1+24` with three
`fmr`s and three byte stores from a *single* `lfs f1, <1.0f>`, ours reloads and lands it at
`r1+16`/`r1+44`. Same known MWCC colour-folding problem `CModelFlags.hpp`'s own comment
describes. Do not retry `CColor(1.f, 1.f, 1.f, AlphaOf(1.f))`-style workarounds without measuring.

`WALL: CProjectedShadow::RenderShadowBuffer(int, ...) 85.59% - retail keeps two distinct 12-byte CModelFlags temporaries (one per branch) and its frame is 16 bytes smaller; four source shapes measured, none reproduce it`

## Also tried, all measured, none kept

| spelling | ctor |
|---|---|
| tree's original | 98.50% |
| `mNextShadow(nullptr)` → `mNextShadow(0)` | 98.50% |
| `mZDistanceAdjust(0.f)` → `(0)` | 98.50% |
| `const`-qualified ctor parameters | 98.50% (no change; the build emitted identical bytes) |
| `mNextShadow` moved to the front of the init list | 98.50% |
| bit-field members initialised in the body instead of the init list | **95.55%** (worse) |

## `CProjectedShadow::CProjectedShadow` — 98.50%, not finished

Not reachable from where it stands, and the gap is **8 instructions of 66, all register choice and
the epilogue's reload order**, no missing work:

```
retail  lis r4,mskInvertedBox@ha ; addi r8, r4, mskInvertedBox@l   ; frame r8
        addi r7, r3, sZeroVector@l ; li r6, 0                       ; frame r7, r6
        lwz r0,0x24(r1) ; lwz r31,0x1c(r1) ; lwz r30,0x18(r1) ; lwz r29,0x14(r1)
ours    lis r4,mskInvertedBox@ha ; addi r7, r4, mskInvertedBox@l   ; frame r7
        addi r6, r3, sZeroVector@l ; li r5, 0                       ; frame r6, r5
        lwz r31,0x1c(r1) ; lwz r30,0x18(r1) ; lwz r29,0x14(r1) ; lwz r0,0x24(r1)
```

Every store target is identical, both save `r29/r30/r31`, both reload all four. MWCC started one
register lower for the two global addresses and the zero, which shifted the epilogue's ordering.
Six spellings (above) did not move it. Not a `WALL:` — it has moved across runs of other work and
the register allocator may respond to a change elsewhere in the unit.

## `Render(const CStateManager&)` — 0.29%, untouched

1400 bytes, retail 0x801917F8. Prime 1 L184-267 is the same function and is a straight port in
shape, but it needs `CLight::BuildDirectional`, `mgr.BuildNearList`, `CMaterialFilter`,
`CCubeRenderer::DrawXRayOutline`, `CGX::LoadTexMtxImm` and `CActor::CanDrawStatic`, none of which
this run got to. Left alone deliberately: a `progress` item is judged on the unit's matched count,
and a half-written 1400-byte function buys nothing and risks the whole change.

## Gates

```
./tools/decomp_build.sh                -> All: 31.55% fuzzy, 24.03% matched (10381 / 28465)
                                          main/MetroidPrime/CProjectedShadow: 58.16% (5 / 8)
./tools/goal_check.sh build/goal/item.json -> PASS progress-prime1-cprojectedshadow
tools/gate.sh (inside the judge)          -> GATE PASS
```

`docs/HANDOFF.md` shows as modified in `git status`: that is `check_docs_claims.py --write` inside
the judge's `gate.sh`, not an edit of mine. Nothing committed.

## Files touched

* `src/MetroidPrime/CProjectedShadow.cpp` — the whole change: `ExpandBoundsForTexture`'s texel
  scale, the full `RenderShadowBuffer` body, the `fn_80036F68` declaration and its includes.
* `include/Kyoto/Graphics/CModel.hpp` — `EDrawFlatFlags` gained `kDF_Unknown1` (value 1) and
  `kDF_All` (value 2). Retail passes 2 for a plain silhouette draw (0x8019230C); the enum had only
  `kDF_Unknown0`, so the call site could not be spelled without a magic number. Enum-only: no
  layout change, and `CHECK_SIZEOF` and every other user are unaffected.
* Scratch, gitignored, not part of the change: `.tmp/opencode/cmpfn.py`.

## New for the queue

`NEW: match-metroidprime-cprojectedshadow-render | match | MetroidPrime/CProjectedShadow | Render(const CStateManager&) is 1400 B at 0.29%, a direct port of prime-ref CProjectedShadow.cpp L184-267; needs CLight::BuildDirectional, mgr.BuildNearList, CCubeRenderer::DrawXRayOutline and CGX::LoadTexMtxImm`