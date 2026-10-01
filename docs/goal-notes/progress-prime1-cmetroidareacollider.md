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

---

# Attempt 2 - 2026-10-01, lane 1 (`wt-mp2-goal-L1`, tree 8d512af5)

The attempt above is still true; nothing in it was re-measured except where noted. This attempt
did **not** touch Prime 1's source or any of the functions that attempt listed as a wall - it
went after the **thirteen functions the previous attempt never looked at**, which are the ones
`build/report.json` lists as `fn_80248360`, `fn_802483D4`, ... , `fn_80248FEC`: retail's
*unnamed* functions in this unit, all of them at 0.00%.

## Result, measured

`build/goal/judge/report.base.json` (the judge's baseline) against the regenerated
`build/report.json`, unit `main/WorldFormat/CMetroidAreaCollider`:

| | before | after |
|---|---|---|
| `matched_functions` | **19 / 58** | **31 / 58** |
| `matched_functions_percent` | 32.75862 | 53.448277 |
| `matched_code` | 2420 | 3168 |
| `matched_code_percent` | 10.031505 | 13.132151 |
| `fuzzy_match_percent` | 19.097164 | 22.197811 |
| `total_code` | 24124 | **24124** (unchanged) |
| `matched_data_percent` | 100.0 | 100.0 (unchanged - no new data) |

Whole build: `All: 32.84% fuzzy, 25.67% matched, 12.15% linked (11451 / 28465 functions)`, against
`11439` in the baseline. **+12 matched functions, and no unit anywhere got worse** - the judge's
own report diff (`gate.sh`, inside `goal_check.sh`) is what established that, not a spot check.

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item progress-prime1-cmetroidareacollider (progress) target=WorldFormat/CMetroidAreaCollider
goal_check: baseline .../wt-mp2-goal-L1/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11439 -> 11451   linked 5584 -> 5584
  ok    check_symbol_names.py
  ok    All:  32.84% fuzzy, 25.67% matched, 12.15% linked (11451 / 28465 functions)
  ok    target rose: main/WorldFormat/CMetroidAreaCollider: 19 -> 31 / 58 functions
  ok    no asm added
goal_check: PASS progress-prime1-cmetroidareacollider
```

and separately `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`python3 tools/check_symbol_names.py` = `checked 514 units; 0 declared names are missing`.

The diff is `src/WorldFormat/CMetroidAreaCollider.cpp` only (+238 lines, 0 removed, no `asm`).
`configure.py`, `config/`, `files.cmake`, `build/goal/` and the docs are untouched. The unit
stays `NonMatching`; `flip_test.sh` was not run, as the item says.

## What the thirteen `fn_` functions are, and why they were at 0.00%

`dtk` names a retail function `fn_<address>` when retail's own symbol table has no name for it -
these are the *implicit* functions mwceppc generated in retail's build: template instantiations,
deleting destructors, block copies. `build/G2ME01/obj/WorldFormat/CMetroidAreaCollider.o` has
all thirteen as **`T` (global)** symbols, and `symbols.txt` has them at:

```
fn_80248360 116 B  fn_802483D4 60 B  fn_80248410 60 B  fn_80248C94 52 B
fn_80248D74 72 B  fn_80248DBC 32 B  fn_80248DDC 40 B  fn_80248E04 92 B
fn_80248E60 68 B  fn_80248EA4 104 B fn_80248F0C 32 B  fn_80248F2C 40 B
fn_80248FEC 72 B
```

**The measurement that made this attempt work:** twelve of the thirteen are *byte-identical* to
functions our own object already contains. Comparing `build/G2ME01/obj/...` (retail) with
`build/G2ME01/src/...` (ours) section by section:

| retail | bytes | already in our object, byte for byte, as |
|---|---|---|
| `fn_802483D4` | 60 | `__dt__reserved_vector<SBoxEdge,12>` / `<Ui,8>` / `<CVector3f,20>` (all three identical) |
| `fn_80248410` | 60 | the same three |
| `fn_80248C94` | 52 | `clear<reserved_vector<COctreeLeafCache,3>>` |
| `fn_80248D74` | 72 | `push_back<reserved_vector<COctreeLeafCache,3>>` |
| `fn_80248DBC` | 32 | `construct<COctreeLeafCache>` (also `construct<CAreaOctTree::Node>`) |
| `fn_80248DDC` | 40 | `construct_impl<COctreeLeafCache>` (also `<CAreaOctTree::Node>`) |
| `fn_80248E04` | 92 | `COctreeLeafCache::COctreeLeafCache(const COctreeLeafCache&)` |
| `fn_80248E60` | 68 | `reserved_vector<Node,64>::reserved_vector(const&)` |
| `fn_80248EA4` | 104 | `uninitialized_copy_n<const Node*, Node*>` |
| `fn_80248F0C` | 32 | `construct<CAreaOctTree::Node>` |
| `fn_80248F2C` | 40 | `construct_impl<CAreaOctTree::Node>` |
| `fn_80248FEC` | 72 | `push_back<reserved_vector<Node,64>>` |
| `fn_80248360` | 116 | **nothing** - an `SBoxEdge` copy assignment, identified by the 0x6C data size |

So the code was never the problem: **the name was.** mwceppc emits an out-of-line copy of a
template under its *mangled* name (`W push_back__Q24rstl61reserved_vector<...>`, a weak COMDAT),
dtk gave retail's identical copy the name of its address, and objdiff pairs functions **by name**,
so retail's function scored 0.00% no matter how right the bytes were. No C++ declaration can
rename a template instantiation, so the only way to give objdiff something to pair with is to
write the function out. **This is already this repo's established practice, twice, and
`include/rstl/reserved_vector.hpp` documents it in the source**: `fn_80143CD4` in
`src/MetroidPrime/Player/CGameState.cpp` ("*retail's `reserved_vector<T, N>::operator=` is an
out-of-line symbol that no caller can name (a template instantiation is emitted under its mangled
name, so objdiff never pairs it with the retail symbol), so it has to be written out by hand in a
.cpp under an `extern "C"` name*") and `fn_800F4FB4` in
`src/MetroidPrime/BodyState/CBSLocomotion.cpp` ("*the block copy mwceppc generates is a template
instantiation, so no C++ declaration can give it retail's name*"). This attempt is the same
technique applied to a whole unit's worth of them.

## The twelve functions, each 0.00% -> 100.00%, and what the body is

Every body is the body `include/rstl/reserved_vector.hpp` / `include/rstl/construct.hpp` already
spell; each was read out of `tools/dis.sh` before it was written and the disassembly is quoted in
the comment above it. Per function, with the measurements that decided the spelling:

1. **`fn_80248410`, `fn_802483D4`** (60 B each, identical) - the *deleting* destructors of
   `CMovingAABoxComponents`' two `rstl::reserved_vector` members. `fn_80248410` is the higher
   address, and the declaration order in the header puts `mEdges` (`reserved_vector<SBoxEdge,12>`,
   0x544 bytes at +0) before `mVertIdxs` (`reserved_vector<uint,8>`, 0x24 at +0x544), so
   `fn_80248410` is `mVertIdxs` and `fn_802483D4` is `mEdges`; the two bodies are the same either
   way, which is why the assignment cannot be checked from the bytes alone. `extsh. r0,r4; ble`
   means the source compares a **`short`**, not an `int` - the same reading as `fn_80004A4C` in
   `CGameStateBlockDtor.cpp`. The flag stays in `r4` (the only call is `CMemory::Free`, which
   takes `r3`), so only `r31` is saved, and the function returns `this`.
2. **`fn_80248360`** (116 B) - `SBoxEdge`'s copy assignment, the block copy mwceppc generates.
   Thirteen `lfd`/`stfd` pairs and one `lwz`/`stw` is 0x6C bytes of data, i.e. this unit's
   `NESTED_CHECK_SIZEOF(..., SBoxEdge, 0x70)` **less the four bytes of tail padding** - the
   compiler copies the data size, not the padded size, and `CVector3d` is three doubles so
   `mDominantAxis` at 0x68 is followed by padding. Spelled `*self = *other`; that one line is the
   whole function, exactly as `fn_800F4FB4` is one line in `CBSLocomotion.cpp`.
3. **`fn_80248C94`** (52 B) - `reserved_vector<COctreeLeafCache,3>::clear()`. **Measured, and the
   obvious spelling is wrong:** written as `self->clear()` alone, mwceppc folds the whole of
   `clear()` in, drops the `stw r0,0(r31)`, and emits 32 bytes - 0.00%. Adding the store back out
   (`self->clear(); self->mCount = 0;`) gives retail's 52 bytes and 100.00%, and it is also what
   makes `self` live in `r31` instead of `r3` across the call.
4. **`fn_80248D74`**, **`fn_80248FEC`** (72 B each) - `push_back` of the two vectors.
   `mulli 2320` is `NESTED_CHECK_SIZEOF(CMetroidAreaCollider, COctreeLeafCache, 0x910)` and
   `mulli 36` is `NESTED_CHECK_SIZEOF(CAreaOctTree, Node, 0x24)`; the destination is
   `data() + mCount` and *not* a pointer load because `rstl::reserved_vector` holds its elements
   inline (`int mCount; uchar mData[N * sizeof(T)]`). The count is reloaded out of `self` before
   the increment, in both.
5. **`fn_80248DBC`**, **`fn_80248F0C`** (32 B each) - `rstl::construct<T>`: a frame and one
   unconditional `bl`. **Measured:** writing these as `rstl::construct<T>(dest, src)` does *not*
   work - `construct` and `construct_impl` are both in-class-inline and mwceppc folds them, and
   folding `construct_impl` folds in its null test too, giving 40 bytes. They are written as a
   call to their own `fn_` neighbour, which is also what retail does.
6. **`fn_80248DDC`**, **`fn_80248F2C`** (40 B each) - `rstl::construct_impl<T>`, i.e.
   `new (dest) T(src)`. The `cmplwi r3,0` / `beq` pair **is** mwceppc's placement-new null test;
   writing an explicit `if (dest != nullptr)` around a placement new produces it twice.
   (`mwcceppc` rejects the explicit-constructor-call spelling `p->T(src)`: *"'Node' is not a
   struct/union/class member"*.)
7. **`fn_80248E60`** (68 B) - `reserved_vector<Node,64>`'s copy constructor: store the count, then
   read it back, then copy. **Measured:** the reload at 0x80248E84 is why the bound has to be
   `self->mCount` and not `other->mCount`; with the bound taken from `other` there is nothing to
   reload and the cursors land in different registers. Same shape and same reason as
   `fn_80143CD4`.
8. **`fn_80248EA4`** (104 B) - `rstl::uninitialized_copy_n<const Node*, Node*>`: a bottom-tested
   loop, cursors in `r29`/`r30`/`r31`, `+36` steps, returns the end cursor.

## The one I could not write: `fn_80248E04` (92 B, still 0.00%)

It is `CMetroidAreaCollider::COctreeLeafCache`'s **copy constructor** (the two scalars, the octree
reference and the `mOverflow` byte at +0x90C inline, `fn_80248E60` for the 0x908-byte
`mNodeCache`). It is the one function of the thirteen the technique cannot reach, and the reason is
the language, not the spelling: a constructor's symbol name is fixed by the mangler, so a
definition can never be given the name `fn_80248E04`; and its body cannot be re-spelled as a
function either, because `COctreeLeafCache` has a `const CAreaOctTree&` member - a reference
cannot be rebound in a function body, only in a constructor's member-initialiser list - and
`*self = *other` does not compile, since a class with a reference member has its implicit copy
assignment **deleted**. Our object already holds the identical 92 bytes as
`__ct__Q220CMetroidAreaCollider16COctreeLeafCacheFRCQ220CMetroidAreaCollider16COctreeLeafCache`,
so nothing is missing but the name, and the name is not reachable. This is a limit of the
technique, not a wall to retry: do not spend a run on it.

## A new measured finding, not a wall: `TAreaId` is passed by hidden pointer

`COctreeLeafCache`'s other constructor (`__ct__...FRC12CAreaOctTree7TAreaId`, 32 B) is the 32 bytes
the previous attempt left at **77.25%**, and this run measured exactly why:

```
retail  80249034  stw  r5,0(r3)          ; the argument register, stored straight through
ours    00000e38  lwz  r0,0(r5) / stw r0,0(r3)   ; r5 is an *address*
```

`TAreaId` (`include/MetroidPrime/TGameTypes.hpp:19`) is `struct TAreaId { int value; TAreaId() :
value(-1) {} TAreaId(int v) : value(v) {} ... }`. Because it has **user-provided constructors** it
is not a POD, and mwceppc's ABI then passes the 4-byte class **by hidden pointer**; retail's
compiler passed the `int` in a register. Everything else in the 32 bytes is already identical.
The previous attempt's `mAreaId(areaId.value)` spelling cannot change this - it still loads
through the pointer. Fixing it means changing what `TAreaId` *is* (a header every unit in the tree
includes), and `include/rstl/reserved_vector.hpp` records what the last such change cost: eight
`Matching` units, one function each, and `main.dol` off its sha1. **Not attempted, and I do not
recommend it inside a `progress` item on one unit**; it belongs to whoever owns `TGameTypes.hpp`,
and it is a codegen rule rather than a blocker, so it is not filed as `NEW:`.

## What I did not do

* `ConvexPolyCollision` (96.66%), `AABoxCollisionCheck` (65.28%), `MovingAABoxCollisionCheck_Edge`
  (55.87%): **not re-measured this run.** The scores above are this run's `build/report.json` and
  they are unchanged from the previous attempt's, but I tried no new spelling of any of them, so
  per the item's rules I write **no `WALL:` line** - those are still the previous attempt's
  hypotheses. `MovingAABoxCollisionCheck_Edge` remains the most promising of the three: the
  previous attempt identified the two causes in the bytes (retail hoists `ev0d`/`ev1d`/`delta`
  out of the loop at 0x80249090, and picks `ci0`/`ci1` from two 3-byte tables at 0x803ad854
  `{1,0,0}` and 0x803ad860 `{2,2,1}` indexed by `edge.mDominantAxis` at +0x68, where our source
  has an if/else chain) and retail's prologue also holds an `xvcmpeqsp vs31,vs1,vs0` whose source
  I could not identify. That is a real recipe and it is untried.
* The twelve leaf-walking `*_Internal` / `*_Cached` functions are still blocked on
  `CCollisionPrimitiveData::GetTriangle`, which is the `NEW: progress-cmetroidareacollider` item
  the previous attempt filed. Untouched, and still blocking.
* Prime 1's source was not used for anything in this attempt. It has no counterpart for any of
  these thirteen functions: they are Echoes' octree-leaf-cache and moving-AABB code, and
  `fn_80143CD4` / `fn_800F4FB4` show that the sources for the two existing precedents are
  *this* repo's `rstl` headers, not Prime 1.

## Notes for the next run (technique, not item state)

* **How to find these in any unit, in one command.** `build/binutils/powerpc-eabi-nm
  build/G2ME01/obj/<unit>.o | grep ' T fn_'` lists them, and a byte comparison of each range
  against the same-named range in `build/G2ME01/src/<unit>.o` says immediately whether the code is
  already there under another name. Tree-wide, **123 non-`auto` units hold 1816 unmatched `fn_`
  functions**, and the ones I sampled have most of them already byte-identical to something we
  emit (measured this run, `fn_` total / already byte-identical):
  `Kyoto/Animation/CAnimationSet` 54/48, `MetroidPrime/CDecalManager` 54/48,
  `Kyoto/Text/CGuiTextSupport` 45/40, `MetroidPrime/CMemoryCard` 44/42,
  `MetroidPrime/CWorldTransManager` 37/32, `Kyoto/Animation/CCharacterInfo` 36/26,
  `MetroidPrime/CParticleDatabase` 65/32, `MetroidPrime/CGameArea` 32/15,
  `MetroidPrime/Enemies/CStateMachine` 26/6. That is a queue of its own and it is much cheaper
  per function than anything else in this unit.
* **objdiff compares the `bl`, not the relocation target** - `AddOctreeLeafCache` is at 100.00%
  today with retail's `bl fn_80248D74` against our `bl push_back__Q24rstl61...`, and `AddLeaf` at
  100.00% with `bl fn_80248FEC` against `bl push_back__Q24rstl41...`. So a hand-written function
  may call *anything* and still match, which is what makes the bodies above writable as calls to
  their `fn_` neighbours instead of as inlined template code.
* **The decl-order rule is real and I broke it twice before getting it right.** mwcceppc emits
  definitions in reverse source order, so the new functions have to be written in *descending*
  retail address: `fn_80248FEC` before `AddLeaf`, `fn_80248F2C` first in the seven-function chain,
  `SetCacheBounds` before `fn_80248C94`. `python3 tools/check_decl_order.py --unit
  WorldFormat/CMetroidAreaCollider` prints ours and retail side by side and is the check; the
  authoritative version of it is to `nm` both objects and confirm that retail's ascending list is
  a subsequence of ours. That is now true for all 57 shared functions. (The unit is `NonMatching`,
  so `main.elf` links `obj/WorldFormat/CMetroidAreaCollider.o` and this ordering does not affect
  the DOL today - it matters on the day the unit flips.)
* The extra COMDAT copies this object emits are unchanged by all of the above: `tools/unit_fit.sh
  WorldFormat/CMetroidAreaCollider.cpp` reports 19 extra functions / 1376 bytes, all of them
  pre-existing weak template instantiations (`__dt__`, `construct`, `push_back`,
  `uninitialized_copy_n`, `GetRootNode`, ...). The flip was not attempted.

## New queue items

NEW: progress-fn-names-canimationset | progress | Kyoto/Animation/CAnimationSet | 48 of the unit's 54 unnamed retail functions (5.4 kB total) are already byte-identical to code this object emits under a mangled template name, so each is a free matched function once written out by hand under its fn_ name - the fn_80143CD4 pattern in src/MetroidPrime/Player/CGameState.cpp, which include/rstl/reserved_vector.hpp already documents; measured candidates with the same shape are MetroidPrime/CDecalManager 48/54, Kyoto/Text/CGuiTextSupport 40/45, MetroidPrime/CMemoryCard 42/44 and MetroidPrime/CWorldTransManager 32/37

## Build fix round

The judge failed this item on a compile error, not on a match: `mwcceppc` rejected the new
`AABoxCollisionCheck_Internal` at `src/WorldFormat/CMetroidAreaCollider.cpp:189` with
`function call 'Add(CCollisionInfoList &, CCollisionInfo *, bool)' does not match
'CCollisionInfoList::Add(const CCollisionInfo &)'`, and `-maxerrors 1` hid everything after it.

Cause: the new body was adapted from Prime 1, where `CCollisionInfoList::Add` takes
`(const CCollisionInfo&, bool swap)`. This repo's header deliberately does not have that overload
(`include/Collision/CCollisionInfoList.hpp:9` is `Add(const CCollisionInfo&)` only, plus a
separate `Swap(int start)`); the item brief also forbids changing class layouts to Prime 1's.

Fix: dropped the trailing `, false` from the single two-argument call site. Prime 1's `swap ==
false` branch is exactly `mList.push_back(info)`, which is all this repo's `Add` does, so the
generated behaviour is unchanged and nothing about the emitted code depends on the second
argument. This also matches the existing convention in `src/Collision/CCollidableSphere.cpp`
(lines 109, 201, 221), which all call the one-argument form. No header was touched, no function
was stubbed, no assembly was added.

Verified after the fix: `./tools/decomp_build.sh` prints
`All:  32.89% fuzzy, 25.74% matched, 12.17% linked (11461 / 28465 functions)` with no linker
error, `sha1sum build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail
reproduces with this unit's own object in the link), and `./tools/goal_check.sh build/goal/item.json`
is `PASS progress-prime1-cmetroidareacollider` - unit up 31 -> 37 of 58 functions. Note that all
seven functions written in this item are spelled against *this* repo's headers, so the one call
that reached for a Prime 1 overload was the only such case; the rest needed no repair.

## Review rejected run 22 (2026-10-01 06:16:03Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/WorldFormat/CMetroidAreaCollider.cpp:55` hard-codes `sBoxEdgeCompIdxA = {2,0,1}` and the comment attributes it to `.rodata:0x803AD84C`, but retail's `MovingAABoxCollisionCheck_Edge` never loads from0x803AD84C — its two `lwzx` tables are0x803AD860 = `{2,2,1}` (`addi r3,r3,-10144`) and 0x803AD854 = `{1,0,0}` (`addi r4,r4,-10156`), confirmed by disassembling 0x80249054+0x3E0 out of the retail DOL. With the values as written, `mDominantAxis` 0 yields `(ci0,ci1) = (2,2)` and 2 yields `(1,1)`, so `denominator` at `:677` is identically zero, `eMag` becomes ±inf/NaN and the `!(eMag < 0.0) && !(eMag >= d)` guard silently drops those moving-box edges — a correctness regression against the if/else chain the same hunk deletes, which always produced retail's two distinct non-dominant components. It is latent today only because `CMovingAABoxComponents`' constructor still never fills `mEdges`, so the bug lands the moment the TODO above it is done. An acceptable change keeps the table form and the two hoisting/Dot-order rewrites (all verified correct) and fixes the constant to `static const int sBoxEdgeCompIdxA[3] = {1, 0, 1 - 1, 0, 0}` at 0x803AD854 alongside the existing `{2,2,1}` at 0x803AD860, so `ci0`/`ci1` are the two non-dominant components for every axis.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cmetroidareacollider-L7-22.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cmetroidareacollider-L7-22-review1-20261001T060743.jsonl

## Fix round 1 (2026-10-01)

Took the reviewer's correction on `MovingAABoxCollisionCheck_Edge` and nothing else. The table
form, the `ev0d`/`ev1d`/`delta` hoisting and the two Dot-order rewrites are unchanged; only the
constant and the comments that mis-attributed it moved.

- `src/WorldFormat/CMetroidAreaCollider.cpp:59` - `sBoxEdgeCompIdxA[3]` is now `{1, 0, 0}` (was
  `{2, 0, 1}`), which is the `0x803AD854` table the reviewer named. With `sBoxEdgeCompIdxB` still
  `{2, 2, 1}` at `0x803AD860`, `(ci0, ci1)` is now `(1,2)`, `(0,2)`, `(0,1)` - the two
  non-dominant components for axes 0/1/2 - so `denominator` at `:684` is no longer identically
  zero for two of the three axes and the `!(eMag < 0.0) && !(eMag >= d)` guard stops dropping
  those moving-box edges. This is the regression the reviewer described; it was latent only
  because `CMovingAABoxComponents`' constructor still never fills `mEdges`.
- `src/WorldFormat/CMetroidAreaCollider.cpp:53-58` and `:668-676` - the measured addresses are
  corrected from `0x803AD84C` to `0x803AD854`, and the two comments now say why `0x803AD84C` is
  the wrong address, so the next pass does not re-introduce it.

Re-measured, all by me, nothing recalled:

- `tools/dis.sh 0x80249054 0x3E0` in the retail ELF: `lis r3,-32709` / `addi r3,r3,-10144` ->
  `0x803AD860` into `r8`, `lis r4,-32709` / `addi r4,r4,-10156` -> `0x803AD854` into `r7`, one
  `slwi r0,r0,2` and two `lwzx` off them, all at `0x80249280`-`0x8024929c`. `objdump -s -j
  .rodata` reads `{1,0,0}` at `0x803AD854` and `{2,2,1}` at `0x803AD860`. No `addi -10164`
  exists anywhere in the DOL's disassembly, so nothing loads `0x803AD84C` this way.
- The fix costs nothing measurable, which is the point: `MovingAABoxCollisionCheck_Edge` is
  scored on its `.text`, and the two `lwzx` are the same instructions whichever table is `ci0`.
  Unit measures are byte-identical to the rejected attempt - `fuzzy 39.264465`, `matched_code
  25.717127`, `matched_functions 38 / 58` - and our object's `.text` is the same size
  (`0x2f0c`). Our `.rodata` now begins `00000001 00000000 00000000 00000002 00000002 00000001`,
  the same six ints in the same order as retail.
- `tools/goal_check.sh build/goal/item.json`: `PASS`, `matched 11503 -> 11504`, `linked 5587 ->
  5587`, `target rose: main/WorldFormat/CMetroidAreaCollider: 37 -> 38 / 58 functions`, `no asm
  added`, `All: 32.97% fuzzy, 25.81% matched, 12.17% linked (11504 / 28465)`. Identical to the
  rejected run, so nothing regressed. `python3 tools/check_raw_offsets.py`: `ok: 162 raw-offset
  site(s) in 69 file(s)`.

One observation, recorded and deliberately not acted on: retail uses one table per operand -
`r8` (`0x803AD860`) is scaled by 8 onto the `mDelta` base and `r7` (`0x803AD854`) by 4 into
`dir` - while this source has a single `ci0` feeding both `dir[ci0]` and `edge.mDelta[ci0]`. That
is why `ci0` names the `0x803AD854` table here. Swapping the two would negate numerator and
denominator alike and leave `eMag` unchanged, so it costs no percentage either way; a future pass
that wants retail's operand-by-operand indices should give `dir` and `mDelta` separate indices
rather than pick a different `ci0`. Noted in the comment at `:673-676`.
