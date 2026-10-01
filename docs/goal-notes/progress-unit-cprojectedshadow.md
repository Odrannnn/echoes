# progress-unit-cprojectedshadow

`kind: progress`, target `MetroidPrime/CProjectedShadow`. The unit stays `NonMatching`; no
`flip_test` was run. Prime 1 (`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref`)
was read as a donor for the logic; no Prime 1 header was copied and no class layout was changed.

## Result

| | before | after |
|---|---|---|
| `matched_functions` | 5 / 8 | **6 / 8** |
| `matched_code` | 712 / 3856 (18.46%) | **976 / 3856 (25.31%)** |
| `fuzzy_match_percent` | 58.47% | **58.57%** |

Whole build, from `./tools/goal_check.sh build/goal/item.json`:

```
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 11838 -> 11839   linked 5727 -> 5727
ok    target rose: main/MetroidPrime/CProjectedShadow: 5 -> 6 / 8 functions
ok    no asm added
goal_check: PASS progress-unit-cprojectedshadow
```

`linked 5727 -> 5727` is expected: the unit is `NonMatching`, so its functions are measured, not
linked. The whole-DOL count rose by exactly 1, so no function anywhere got worse.

## What changed

One line, `src/MetroidPrime/CProjectedShadow.cpp:28`:

```diff
-, mPersistent(persistent)
+, mPersistent(static_cast< uchar >(persistent))
```

### `__ct__16CProjectedShadowFiiUci` (264 B): 98.50% -> **100%**

This was the whole item. The function was already semantically complete and **26 of 264 bytes
differed**, every one of them a register *number*, never an instruction or an immediate:

```
0044  R 39 04 00 00  addi r8,r4,0   | O 38 e4 00 00  addi r7,r4,0
004c  R c0 08 00 00  lfs  f0,0(r8)   | O c0 07 00 00  lfs  f0,0(r7)
0050  R 38 e3 00 00  addi r7,r3,0   | O 38 c3 00 00  addi r6,r3,0
0058  R 38 c0 00 00  li   r6,0      | O 38 a0 00 00  li   r5,0
...  (13 more, the same pattern, plus the epilogue's load order)
00e8  R 90 dd 00 9c  stw  r6,156(r29)| O 90 bd 00 9c  stw  r5,156(r29)
```

Retail holds `&mskInvertedBox` in r8, `&sZeroVector` in r7 and the zero constant in r6; our object
held them in r7, r6 and r5. Identical instruction stream, allocator numbered one lower.

**The spelling that fixes it: `static_cast< uchar >(persistent)` in the member-init list.** MWCC
gives the cast its own value number, which consumes one allocator slot, and every subsequent temp
in the function shifts up by one register - to exactly retail's numbering. It is a no-op at
runtime (`persistent` is already a `uchar` parameter; the member is `uchar mPersistent : 1`) and it
deletes nothing: the initialisation still happens, with the same value.

This is a codegen lever worth remembering on its own, and it is **not specific to this unit**: a
member-init list where one entry has an explicit cast of the type it already has is a cheap way to
move mwcceppc's temp numbering by one. See the wall below for what does *not* work.

## Not reached

`RenderShadowBuffer__...iPCPC...` (1480 B) stays at 86.38% and `Render__16CProjectedShadow...`
(1400 B) at 0.29%. Neither is a candidate for this run: see below.

### `RenderShadowBuffer` (1480 B, 86.38%) - measured, not a wall

Both objects contain the **same 51 call sites, in the same order, with the same callee names**
(`GetBounds` ... `fn_80036F68`), so the shape of the function is already right and the diff is
allocation, not logic. 370 retail instructions against 374 ours. Two things do not line up:

- **Frame layout.** Retail's saved-register window starts at `404(r1)` (`stmw r17,404(r1)`) and its
  scratch slots run `12, 13, 14, 24, 25, 26, 32, 36, 40, 44 ... 152`; ours starts at `408(r1)`
  (`stmw r18,408(r1)`) and uses `8, 12, 20, 21, 22, 24, 32, 33, 34, 36, 40, 44, 48, 52, 56, 60 ...
  160`. Retail uses `r17`, ours begins at `r18`, and every scratch slot is 4-8 bytes higher. One
  extra long-lived register in retail shifts the whole frame up by 8.
- **A duplicated colour materialisation.** In the draw loop retail emits `lfs f1` once and three
  `fmr`s from it for the `CModelFlags` colour; ours materialises the alpha three times
  (`fmr f2,f1` / `fmr f3,f1` / `fmr f4,f1`) plus a separate `cntlzw`/`rlwinm`/`and`/`ori` chain for
  `drawFlags`. Rewriting the colour as `CColor::White()` cut the function by 20 bytes
  (1480 -> 1460) and raised it 86.38% -> **88.68%**, which says the colour spelling is part of the
  story, but it is a step, not the answer: the frame offsets and the extra saved register are
  untouched by it.

### `Render` (1400 B, 0.29%) - not attempted

It is a `// TODO: Project the texture onto world geometry...` stub (4 bytes of `.text` against
retail's 1400). Prime 1 has a full donor at `prime-ref/src/MetroidPrime/CProjectedShadow.cpp:184`,
but Echoes' version differs where it matters: this class has `mProjectOnActors`, and the receiver
loop has to honour it. Writing that is a real porting job, not a spelling change, and it needs
`CStateManager::BuildNearList` and `CCubeRenderer::DrawXRayOutline` verified rather than assumed.

## Codegen rules measured this run (reusable, not unit-specific)

The constructor was 26 register-number bytes from a match. Spellings tried, all measured with
`tools/fast_try.sh`:

| spelling | result |
|---|---|
| **`mPersistent(static_cast< uchar >(persistent))`** | **100% - the fix** |
| `mProjectOnActors(!projectionMode)` / `0 == projectionMode` | 98.50%, no change |
| `const` on all four ctor parameters | 98.50%, no change |
| `short width, short height` parameters | 98.50%, no change |
| `mNextShadow(nullptr)` -> `mNextShadow(0)` | 98.50%, no change |
| `mNextShadow(static_cast< CProjectedShadow* >(nullptr))` | 98.50%, no change |
| reordering the init list (persistent first, next-shadow first, scale before bounds) | 98.50%, no change |
| `mPersistent(persistent != 0)` / `static_cast< bool >(persistent)` | 98.50% / **70.30%, size 280 - worse** |
| moving the four bitfield inits into the ctor body | 95.55%, 64 bytes differ - worse |
| dropping `mNextShadow` from the list | 97.06% - worse |
| `mTranslation(CVector3f(0.f, 0.f, 0.f))` instead of `CVector3f::Zero()` | **82.14% - worse** |
| `mBounds(CAABox(CAABox::MakeMaxInvertedBox()))` / a local `CAABox` | 98.50% / 53.95% |
| `mTexture(kTF_I4, height, width, 1)` (args swapped) | 90.68% |
| one artificial live temp in the body (bool / int / CVector3f / CAABox / float / pointer) | 84.26% / 93.95% / 89.41% / 53.95% / 96.98% / 96.98% |

Two lessons worth keeping:

1. **A redundant cast of a value to the type it already has is a register-allocation lever.** It
   worked here (`uchar` -> `uchar`, +1 slot). Casting *up* a type does not: `static_cast< bool >`
   on the same initialiser made the function 280 bytes instead of 264.
2. **Do not try to add register pressure to close a register-numbering gap.** All six dummy
   temporaries moved the score the wrong way, several badly. The gap here closed by *consuming* a
   value number, not by adding one.

## New blockers

None filed. The constructor is done and the item passes; `RenderShadowBuffer` at 86.38% and `Render`
at 0.29% are the same unit and same functions this item already names, so re-queuing them here
would be a restatement of the item rather than new work.