# progress-prime1-cmetroidareacollider

`kind: progress`, `target: WorldFormat/CMetroidAreaCollider`. Unit stays `NonMatching`; the flip was
not attempted (the item says not to run `flip_test.sh` to decide).

## Result, measured

`build/report.json`, unit `main/WorldFormat/CMetroidAreaCollider`:

| | before | after |
|---|---|---|
| `matched_functions` | **12 / 58** | **19 / 58** |
| `matched_code_percent` | 5.3556623 | 10.03 |
| `fuzzy_match_percent` | 13.990051 | 19.10 |
| `total_code` | 24124 | 24124 (unchanged) |

Whole-build, from `./tools/decomp_build.sh`:

```
before:  All: 30.01% fuzzy, 21.90% matched, 11.74% linked (722 / 2041 files)
                Code: 1431032 / 6535816 bytes (9755 / 28465 functions)
after:   All: 30.03% fuzzy, 21.91% matched, 11.74% linked (722 / 2041 files)
                Code: (9762 / 28465 functions)
```

**+7 matched functions, +7 whole-build, nothing anywhere got worse** (checked every unit in the
regenerated report; no unit's `matched_functions` fell).

Gates, all run on the final tree:

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/probe_sources.sh                 probe: 744 files, 0 failed, 0 errors; link: LINKED
python3 tools/check_symbol_names.py      checked 502 units; 0 declared names are missing
./tools/decomp_build.sh                  All: line rose, did not fall
```

No `asm` was added; the diff touches `src/` and `include/` only. `configure.py`, `splits.txt`,
`files.cmake` and `build/goal/` are untouched.

## What changed, and the seven functions it won

### 1. `CAABoxAreaCache::CAABoxAreaCache` — 66.37% → **100%** (172 B)
### 2. `CBooleanAABoxAreaCache::CBooleanAABoxAreaCache` — 63.85% → **100%** (160 B)

These were the *only* place where the previous source was already correct but codegen went wrong.
`mHalfExtent(aabb.GetHalfExtent())` called the out-of-line `CAABox::GetHalfExtent`, which MWCC emits
as a weak COMDAT copy (`W GetHalfExtent__6CAABoxCFv`) rather than inlining. Retail inlines it: the
retail ctor's half-extent code is the same `fsubs`/`fmuls` sequence with `lfs f4,-18016(r2)` (0.5f)
that `GetHalfExtent`'s own body has, inlined. Prime 1's source is byte-identical here and matched
there, so the difference is that GC/2.7 will not inline a `CVector3f`-returning member here where
GC/1.3.2 did. Spelling the expression at the call site reproduces it:

```cpp
, mHalfExtent((aabb.GetMaxPoint() - aabb.GetMinPoint()) * 0.5f) {}
```

Prime 1's source matched **unchanged** in spirit; it needed this one small edit, and it applied to
both cache constructors. **Lesson: in this unit, prefer spelling `CAABox`'s inline accessors out
rather than relying on the inline keyword** — see also `ConvexPolyCollision` below.

### 3. `PlaneIntersectionFraction` — 98.21% → **100%** (112 B)

The only remaining byte diff was multiply operand order in the numerator: retail computes
`fmadds f0,f5,f6,f0` (i.e. `plane[0] * vec[0]`), ours computed `fmadds f0,f6,f5,f0`. `CPlane::GetHeight`
is `Dot(GetNormal(), pos) - GetConstant()`, which puts the plane first. Retail's `CPlane::GetHeight`
must be `Dot(pos, GetNormal())`, or the source inlines the dot by hand. Writing the numerator as

```cpp
-(CVector3f::Dot(start, plane.GetNormal()) - plane.GetConstant())
```

gives 100%. **The obvious alternative — `plane.GetConstant() - CVector3f::Dot(start, ...)` — scores
7.25%, far worse**; the sign has to sit on the outside. Prime 1's source uses the *inline* expression
(not `GetHeight` at all), and it needed no further edits once the operand order was `start` first.
This suggests **`CPlane::GetHeight`'s argument order in `include/Kyoto/Math/CPlane.hpp` may be
reversed** — see the `NEW:` line.

### 4. `MovingAABoxCollisionCheck_TriVertexBox` — 2.06% → **100%** (272 B)
### 5. `MovingAABoxCollisionCheck_BoxVertexTri` — 2.03% → **100%** (276 B)

Prime 1's implementations verbatim, needing only the new includes (`Collision/CMRay.hpp`,
`Collision/CollisionUtil.hpp`). Both are one of the three "same size as Prime 1" functions the item
flagged, and both landed at 100% on the first try. Retail's disassembly confirms the shape exactly:
`CMRay(vert, -dir, (float)dOut)` inlined to three `fneg`s into a stack temp, then
`CollisionUtil::RayAABoxIntersection_Double` with `CVector3f::Zero()` loaded from
`0x802474b0` (a `sZeroVector`-style constant), `cmpwi r3,2`. `BoxVertexTri` likewise: `GetPoint`,
`RayTriangleIntersection_Double`, then `point + dir * (float)d` written as
`fmuls`+`fadds` per component and `GetNormal()` into the out-param.

### 6. `AABoxCollisionCheckBoolean` — 94.44% → **100%** (72 B)
### 7. `SphereCollisionCheckBoolean` — 61.06% → **100%** (64 B)

The previous source called `ResetInternalCounters()` before recursing. **Retail's two boolean entry
points do not** — `tools/dis.sh 0x8024c2ac 0x48` and `0x8024b8f8 0x40` show the ctor, then
`fn_8012753C` (the `CAreaOctTree::Node` construction), then straight to the `_Internal` call. The
non-boolean `SphereCollisionCheck` *does* call it (`0x8024bccc` proves it), which is why that one
already matched. Removing the two spurious calls flipped both. Prime 1's source agrees: neither
boolean entry point calls `ResetInternalCounters`.

## `MovingAABoxCollisionCheck_Edge` — 0.56% → 55.87% (992 B), not matched

Prime 1's implementation transcribed essentially verbatim, but it **could not compile**: this repo's
`include/Kyoto/Math/CVector3d.hpp` was missing declarations for two operators that
`src/Kyoto/Math/CVector3d.cpp` already *defines*:

* `CVector3d operator+(const CVector3d& other);` — **declared with one parameter**. The `.cpp`
  definition and retail's symbol `__pl__FRC9CVector3dRC9CVector3d` (config/G2ME01/symbols.txt:12917)
  are both two-argument, so the declaration was simply wrong and no caller could use `+`.
* `operator-(const CVector3d&)` (unary) and `operator*(double, const CVector3d&)` — defined in the
  `.cpp` but never declared. Retail has all three: `__mi__FRC9CVector3d` (12915),
  `__ml__FdRC9CVector3d` (12914), `__mi__FRC9CVector3dRC9CVector3d` (12916).

Fixing the three declarations is a real bug fix, confirmed against retail, and it took Edge from
0.56% to 55.87% (it went from not-compiling to compiling first). It did not reach 100%: the
remaining diff is register allocation and loop layout — retail hoists the `CVector3d ev0d/ev1d/delta`
construction **out of** the loop (it computes them once before the loop header at `0x80249090`,
while ours rebuilds them on entry), and uses a two-entry lookup table for `ci0`/`ci1`
(`0x803ad854` = `{1,0,0}`, `0x803ad860` = `{2,2,1}`, indexed by `edge.mDominantAxis` at offset
`0x68`) where Prime 1 spells out the same choice as an if/else chain. Both are Echoes-specific
rewrites of Prime 1's source. **Not a `WALL:` line** — 55.87% with a known remaining cause is not
"stuck across several spellings"; a follow-up should try the table form and the hoisting.

## Functions left at 0-2% and why: `CCollisionPrimitiveData::GetTriangle` is undefined

Every remaining `*_Internal` and `*_Cached` leaf-walking function (12 of them: both
`AABoxCollisionCheck_Internal`/`SphereCollisionCheck_Internal`, all four `*_Cached` boolean
overloads, both `AABoxCollisionCheck_Cached`, both `SphereCollisionCheck_Cached`, both
`Moving*_Cached`) has the identical shape in retail, and every one of them calls the same helper at
the top of its triangle loop:

```
8024bc14:  bl 80257a14 <fn_80257A14>       # build the CCollisionSurface for triangle index j
8024bc20:  bl 8013a0e4 <fn_8013A0E4>       # 24-byte copy (the CAABox bounds)
8024bc3c:  bl 8028993c <Passes__15CMaterialFilterCFRC13CMaterialList>
8024bc58:  bl 80284450 <TriSphereOverlap__13CollisionUtilFRC7CSphereRC9CVector3fRC9CVector3fRC9CVector3f>
```

`fn_80257A14` is Prime 1's `CAreaOctTree::GetMasterListTriangle(ushort)` — it reads
`mPolyEdges`/`mEdges`/`mPolyMats`/`mMaterials`/`mVertices` and builds a `CCollisionSurface`. **In this
repo `CAreaOctTree` has no such method, and `CCollisionPrimitiveData::GetTriangle(ushort)` — declared
at `include/WorldFormat/CCollisionPrimitiveData.hpp:22` — has no definition anywhere.** I confirmed
this by grep: no `.cpp` in the tree defines it. So the leaf-walking bodies cannot be written at all
yet; they would not link. I wrote the one function whose shape I could verify
(`AABoxCollisionCheckBoolean_Internal`, straight from Prime 1) and it failed to compile on exactly
this, so I reverted it and left the TODO in place with a pointer to this note.

I traced `fn_80257A14`'s disassembly far enough to be sure of its identity, which is what makes
this fileable:

```
80257a1c:  lwz r6,28(r4)          # mPolyMats      (offset 0x1c in CCollisionPrimitiveData)
80257a24:  clrlwi r0,r5,16         # idx (2 bytes)
80257a28:  mulli r8,r0,3          # start = idx * 3
80257a2c:  lwz r7,16(r4)          # mEdges         (offset 0x10)
80257a30:  lbzx r0,r6,r0          # ... triangle material byte
80257a38:  lwz r10,36(r4)         # mSurfaceIndices (offset 0x24)
80257a4c:  lhzx r0,r10,r9         # edgeIndex0 = mSurfaceIndices[start]
80257a9c:  lwz r9,44(r4)          # mVertices      (offset 0x2c)
```

Prime 1's `GetMasterListTriangle` walks exactly these. This is a **real, well-scoped piece of work
whose success raises counts** (it unblocks 12 functions in this unit alone), so it is filed as
`NEW:`. It is a *dependency*, not part of this item's target, so I did not attempt it here.

## Other measured walls (spellings tried, so the next run skips them)

* `ConvexPolyCollision` — **96.66%**, stuck. The mnemonic stream is identical to retail; the
  difference is purely **register allocation** (retail uses `r21`/`r28`/`r29`/`r30`/`r31`, ours uses
  `r20`/`r27`/`r28`/`r29`/`r30` — ours is uniformly one lower) and retail allocates `r24`/`r25` for
  the two `ClipVec`s where ours allocates `r23`/`r24`. 197 retail instructions vs 198 ours: retail
  has `li r23,0` where ours has `addi r29,r28,12`, i.e. retail keeps a loop counter in a register
  and ours keeps a derived pointer. Same Prime 1 source, same shapes, only allocation — a
  GC/1.3.2-vs-GC/2.7 scheduling difference. **One extra saved FPR in ours** (`f28`, with
  `xxsel`/`xsmaddadp` instead of retail's `xvcmpeqsp`/`xsmsubmsp`) is the tell.
* `AABoxCollisionCheck` — **65.28%**. Tried Prime 1's exact form with `const CVector3f min` **by
  value** instead of this repo's `const CVector3f&`: **65.15%, slightly worse**. Retail's
  disassembly stores the six `CPlane`s to `88(r1)..184(r1)` (values `f7=1.0, f8=0.0, f31=-1.0,
  f12=-0.0` read from SDA2 constants `0x8041dd70` and `0x8041dd4c`), so the plane array is at
  `88(r1)` and the `CAABoxAreaCache` (0x2c bytes) at `44(r1)` — which is what both forms produce.
  The remaining 35% is again FPR allocation: retail saves `f29/f30/f31` and uses `f8..f13` as
  temporaries; ours saves `f28..f31` and uses `f6..f13`. One more live float in ours.
* `COctreeLeafCache::COctreeLeafCache` — **77.25%** (32 B). Retail is 8 instructions including
  `rlwimi r0,r6,7,24,24` to clear the `mOverflow:1` bitfield at `2316(r3)`; ours is the same shape
  but starts with `lwz r0,0(r5)` where retail has `stw r5,0(r3)`. Retail stores the `TAreaId`
  *argument register* straight through, so Echoes passes it differently than our declaration
  implies. Tried `mAreaId(areaId.value)` (forcing the int): **still 77.25%**, no change. Not
  pursued further — 32 bytes, and the call convention question is a header-level issue like
  `CPlane::GetHeight` below.

## Notes for the next run

* **`CVector3d` was a live bug and is now fixed**; check whether `CPlane` and `TAreaId` have the same
  class of problem before trusting any header in this area.
* The two `*_Cached` cache-writer functions (`ReserveTriangles`, `AddTriangle`, `CacheAllNodes`,
  `CacheNodes`, `BuildCollisionCache`) are **Echoes-only** — they have no Prime 1 counterpart at all
  (Prime 1's `CMetroidAreaCollider.cpp` has no packed-cache code). They cannot be lifted from
  Prime 1 and are the wrong target for a "port Prime 1" item.
* `check_docs_claims.py` correctly reports the HANDOFF state block is now stale
  (`9762 / 28465` and `8351 / 16726`). The driver rewrites those from the tree, so no doc was
  edited here.

## New queue items

NEW: progress-cmetroidareacollider | match | WorldFormat/CCollisionPrimitiveData | define GetTriangle(ushort) - it is declared at include/WorldFormat/CCollisionPrimitiveData.hpp:22 with no definition anywhere in the tree, and it is the helper every one of the 12 leaf-walking collision-query functions in WorldFormat/CMetroidAreaCollider calls (retail fn_80257A14, reached by all of AABoxCollisionCheck_Internal, SphereCollisionCheck_Internal, AABoxCollisionCheckBoolean_Internal, SphereCollisionCheckBoolean_Internal, all four *_Cached boolean overloads, both AABoxCollisionCheck_Cached, both SphereCollisionCheck_Cached and both Moving*_Cached), so none of them can be written until it exists

NEW: progress-cvector3d | match | Kyoto/Math/CVector3d | CPlane::GetHeight argument order is likely reversed - CPlane.hpp computes Dot(GetNormal(), pos) but the retail PlaneIntersectionFraction in WorldFormat/CMetroidAreaCollider multiplies plane-component-first, and spelling it Dot(start, GetNormal()) - GetConstant() takes that 112-byte function from 98.21% to 100%; worth checking every other GetHeight caller against the retail operand order
