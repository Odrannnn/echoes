# progress-prime1-cpathfindregion

`kind: progress`, target `main/MetroidPrime/PathFinding/CPathFindRegion`. The unit stays
`NonMatching`; I did not run `flip_test.sh` and did not touch `configure.py`. Two files changed:
`src/MetroidPrime/PathFinding/CPathFindRegion.cpp` and
`include/MetroidPrime/PathFinding/CPathFindRegion.hpp` (one member's type, `CPFPoint::mNumLinks`
`int` -> `uint`; the class stays `0x1c` and no other unit references the member). No `.s`, no
`tools/`, no `build/goal/` file other than this one.

**The item passes.** `./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11506 -> 11511   linked 5590 -> 5590
  ok    check_symbol_names.py
  ok    All:  32.98% fuzzy, 25.86% matched, 12.18% linked (11511 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindRegion: 12 -> 17 / 17 functions
  ok    no asm added
goal_check: PASS progress-prime1-cpathfindregion
```

Independently: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`python3 tools/check_symbol_names.py` = `checked 515 units; 0 declared names are missing from their
object`. The gate covers the 86 REL sha1s and `probe_sources.sh`.

## Measured result

Unit: **75.84% -> 100.00% fuzzy**, matched functions **12 -> 17 of 17**, matched code 42.87% ->
100.00%. Every one of the five unmatched functions reached 100%. Global `build/report.json`:
matched 11506 -> 11511, linked 5590 -> 5590, `report_diff.py` reports no regression.

| function | before | after | what actually moved it |
| --- | --- | --- | --- |
| `FitThroughLink2d` | 99.30% | **100%** | Prime 1's `const float maxT = link.Get2dWidth() - radius;` local; retail keeps the clamp's upper bound in `f3` and does `fmr f26,f3`, ours recomputed `Get2dWidth()` after the branch |
| `CPFPoint::Fixup` | 94.29% | **100%** | not Prime 1 - retail `cmplwi`, ours `cmpwi`. `mNumLinks` is compared unsigned; changing the member to `uint` gives `cmplwi` |
| `FindBestPoint` | 44.94% | **100%** | `push_back` -> `push_back_unsafe` (retail emits no capacity test and no `reserve` call anywhere in the function) + moving `bool found` **above** `int i` |
| `FitThroughLink3d` | 45.63% | **100%** | Prime 1's whole discarded horizontal-interpolation block (it is dead but the compiler must schedule it); plus `const CVector3f&` for `sourceDelta`/`destinationDelta`, and `minZ/maxZ` mutated by `rootPosition` instead of a ternary |
| `Intersects` | 43.33% | **100%** | no Prime 1 version exists (Echoes added it). Loop as `break` + `if (i == GetNumNodes())`, an `intersects` result variable, and every `CVector3f` subtraction bound to a named local |

Prime 1's source was the whole answer for two functions and unusable for one; see below.

## What Prime 1 gave and what it did not

**`FindBestPoint`: Prime 1's flag test is wrong for Echoes.** Prime 1 gates both extra passes with
`if (!(flags & 2))`. Echoes' retail tests the whole `flags & 6` mask once
(`rlwinm. r28,r6,0,29,30` at 0x6ec) and re-tests it after the middle call with `cmplwi r28,0`. The
tree already had `flags & 6`, which is right; keep it. The real difference was the vector calls -
retail's object has **zero** `bl reserve` and no capacity compare, ours had 11. With
`push_back_unsafe` the reserve calls disappear and the body is instruction-for-instruction.

**`FitThroughLink3d`: Prime 1's source is 90% of it, but the tree had thrown the dead part away.**
Prime 1 computes an interpolation value `t` inside `if (radius < 0.5f * link.Get2dWidth())`,
discards it (its own comment says so), and then uses the outer `t = 0.5f`. The tree had collapsed
that to `CVector3f result = node.GetPos() + edge * 0.5f;`, which is 45.63% - the block is 198
instructions of scheduling the compiler still performs. Restoring it as Prime 1 writes it (with
`ToVec2f()` in place of Prime 1's `DropZ()`, which returns `CVector3f` here, not `CVector2f`)
reached 78.61%. `ERootPosition` is the Echoes-only addition; spelling it as
`if (rootPosition == kRP_Bottom) { minZ -= halfHeight; maxZ -= halfHeight; }` before the clamp,
instead of the ternary on the clamp arguments, took it to 100%.

**`Intersects` does not exist in Prime 1.** Prime 1's `CPathFindRegion.cpp` has no
`Intersects(const CAABox&)` at all, so this one was written from the object. Five spellings, each
measured:

1. the tree's shape (early `return false`) - 43.33%
2. wrap the whole body in `if (mBounds.DoBoundsOverlap(box)) { ... } return false;` - 43.81%
3. `break` out of the node loop + `if (i == GetNumNodes())` - 46.66%
4. + a `bool intersects` result variable instead of `return` - 58.69%
5. + bind `GetHeight() * CVector3f::Up()` to `up`, `ceilingPoint - firstPos - up` to `ceiling`,
   `floorPoint - firstPos` to `floor`, and make it `if (Dot(...) <= 0.f) intersects = true;` -
   **100%**

The naming of each intermediate is load-bearing, not cosmetic: retail keeps `f29/f30/f31` for the
`height * Up` components across the `ClosestPointAlongVector` call, and mwcceppc only hoists those
three multiplies out of the dot product when `up` is a named local. Naming `ceiling` and `floor`
separately stopped the last spill.

## Notes for the next run

- `Intersects` and `FitThroughLink3d` are now 100% and can be flipped; this is a `progress` item so
  I left `configure.py` alone.
- **`rstl::vector::push_back` vs `push_back_unsafe` is the single biggest lever in this unit.**
  Retail's `FindBestPoint` has no capacity test at all. `CPFArea::Initialize` reserves
  `mPolyPoints.reserve(maxRegionNodes)` (with `maxRegionNodes` floored at 4), which is why the
  unchecked form is correct here rather than a shortcut. If another unit's `push_back` is costing
  you double digits, check whether retail's object calls `reserve` at all before assuming the
  checked form.
- Declaration order of two locals decided `FindBestPoint`: `bool found` above `int i`. Same trick
  worked on `Intersects`. If a function is at 99.x and only register assignment differs, try
  reordering the declarations of the loop counter and the accumulator.
- Prime 1's `CPFRegionData::CPFRegionData` has no `mAvoidanceFlags` and Prime 1's `CPFRegion`
  ctor has no `memset(mObstructionCounts, ...)`; both are Echoes additions and the tree's versions
  are already 100%.

NEW: match | MetroidPrime/PathFinding/CPathFindRegion | all 17 functions measured at 100% fuzzy, only the flip is left (run tools/flip_test.sh MetroidPrime/PathFinding/CPathFindRegion.cpp)