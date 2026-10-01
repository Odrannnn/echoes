# `match-careaocttree_tests` — `WorldFormat/CAreaOctTree_Tests`, 7/9 → 8/9

`tools/goal_check.sh build/goal/item.json` → **`goal_check: PARTIAL match-careaocttree_tests`**
(exit 3): `target rose: main/WorldFormat/CAreaOctTree_Tests: 7 -> 8 / 9 functions`,
`counts: matched 11943 -> 11944   linked 5728 -> 5728`, and a clean `gate.sh`.

The flip itself still **fails** — `LineTestExInternal` is 94.88% and is the known wall
(documented at goal item `progress-unit-careaocttreetests`, reproduced below). So this is partial
progress on a unit that cannot yet be `Matching`, not a completed unit.

## What landed

Three files, no `configure.py` change, no `splits.txt` change, no asm.

### 1. `config/G2ME01/symbols.txt` line 10271 — one rename

```
-fn_80246258 = .text:0x80246258; // type:function size:0xC4
+__as__Q24rstl36optional_object<17CCollisionSurface>FRCQ24rstl36optional_object<17CCollisionSurface> = .text:0x80246258; // type:function size:0xC4
```

Same case the previous run used for `fn_802461A0`/`fn_802461EC`: dtk cannot name a TU-local weak
template instantiation, so the retail side calls it `fn_80246258` while our object emits the
mangled name, and objdiff pairs **by name** — so a byte-identical function scored 0.00%. The
previous run measured it as "~99% and not a match" and deliberately left it unnamed. That reading
was wrong about the cause: the 0.00% was the name, and the bytes were *not* close (23 of 196 bytes
differ). With the rename it is **91.55%**, and the residual is real codegen, fixed below.

### 2. `include/WorldFormat/CCollisionSurface.hpp` — three separate vertex members

```cpp
-  CVector3f mVertices[3];
+  const CVector3f* mVertices() const { return &mVert0; }
+  CVector3f mVert0;
+  CVector3f mVert1;
+  CVector3f mVert2;
```

The rename exposed the actual difference. Retail's copy in
`optional_object<CCollisionSurface>::operator=` (0x80246258) moves the vertices as **three
12-byte groups** — `lwz/stw` at 0x0/0x4/0x8, then 0xc/0x10/0x14, then 0x18/0x1c/0x20 — while our
`CVector3f mVertices[3]` made mwcceppc flatten all 0x30 bytes into 8-byte word pairs and pair
**0x8 with 0xc**. That was 5 of the 23 differing bytes, and it is a source-shape difference, not
register allocation: **91.55% → 99.90%** from the layout change alone, with no other function in
the unit moving.

`mVertices()` returns `&mVert0` and the subscript arithmetic is identical, so every call site keeps
its old spelling and its old codegen.

### 3. `src/WorldFormat/CCollisionSurface.cpp` — `IsDegenerate` only

```cpp
-  return mVertices()[0] == mVertices()[1] || mVertices()[1] == mVertices()[2] ||
-         mVertices()[0] == mVertices()[2];
+  return mVert0 == mVert1 || mVert1 == mVert2 || mVert0 == mVert2;
```

**This was a real regression I introduced and caught by measurement, and it is worth recording.**
Routing `IsDegenerate` through `mVertices()` dropped it from **100.00% (152 B) to 53.13% (220 B)** —
the helper defeated an optimization mwcceppc was applying to the array form. Because
`CCollisionSurface.cpp` is a **`Matching`** unit, that broke the DOL sha1 *and* cascaded into a
constant −0x40 address shift across **all 86 RELs** (measured with `cmp -l` on `Shrieker.rel`: 285
bytes, every one an address). Naming the members directly restores 100.00% / 152 B.

The lesson generalises: **a header layout change is not local to the unit you are working on.**
`CCollisionSurface.hpp` is included by 6 sources, and one of them is `Matching`. `unit_fit.sh` on
the target unit said nothing about this; only the full build and the REL comparison did.

### Result: the function matches

`__as__Q24rstl36optional_object<CCollisionSurface>FRCQ24rstl36optional_object<CCollisionSurface>`
(196 B) → **100.00%**, paired and counted. Unit 7/9 → **8/9**; `matched` 11943 → 11944;
`matched_code` unit 56.49% → 59.32%; unit fuzzy 95.09% → 97.92%.

## The remaining function, and why the unit still cannot flip

`LineTestExInternal` (2820 B, **94.88%**) is the same wall the previous run recorded, and this run
reproduced its two dead ends rather than taking them on trust:

- **`#pragma inline_max_size` is TU-global, not positional.** Values 126..133 measure *identical*
  to the default; ≥134 inlines `assign` but also breaks the rest of the TU (`LineTestInternal`
  loses its 100%, the unit drops to ~88%). There is no value in the 125..156 window that inlines
  `assign` and leaves the TU alone.
- **`include/rstl/optional_object.hpp` and `include/rstl/construct.hpp` are frozen.** 92 sources
  include them and 578 DOL units are `Matching`.

I also tried the third route this run, and it is a dead end worth recording: giving
`rstl::construct_impl` a `CCollisionSurface` overload that calls the shared out-of-line
`fn_800E88FC` (the 0x34-byte six-`lfd`/`stfd` 0x30-byte copy that `CTransform4f.hpp` already uses,
and which retail's `optional_object::operator=` calls at 0x80246290). It **does** work locally —
`LineTestExInternal` rose 94.68% → 98.90% and the copy-assign hit 100% — but it changes codegen
in every unit that copies a `CCollisionSurface`, and the full build failed with
`WARNING: 87 computed checksum(s) did NOT match` plus a wrong DOL sha1. Reverted; the notes here
record the measurement so the next run does not spend an hour rediscovering it.

`tools/unit_fit.sh` also reports 7 functions in our object that retail's does not define (488
bytes: `assign`, `__ct__`/`__dt__` for `SRayResult` and `optional_object`, two `construct`
helpers, `__as__...FRC17CCollisionSurface`). As its own output says, those are weak COMDAT copies
that both linkers discard and only `flip_test` decides — and `flip_test` fails, though for the
`LineTestExInternal` reason above rather than for these.

## Verification

- `./tools/decomp_build.sh WorldFormat/CAreaOctTree_Tests` — `All: 33.76% fuzzy, 26.93% matched,
  12.64% linked (11944 / 28465 functions)`.
- `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- All 86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/` (`total=86 differ=0`).
- `CCollisionSurface` unit re-checked after every header edit: all four functions 100.00%, sizes
  152/280/132/152 unchanged from the pre-change object.
- `python3 tools/check_decl_order.py --unit WorldFormat/CAreaOctTree_Tests.cpp` → `ok`.
- `tools/flip_test.sh WorldFormat/CAreaOctTree_Tests.cpp` → **FAIL**, reverted, unit stays
  `NonMatching` (expected; see above).
- `./tools/goal_check.sh build/goal/item.json` → **PARTIAL**, exit 3, gate clean.
- `total_functions` still 28465; `splits.txt` untouched.

## For the next attempt

- The remaining gap is **one** function, `LineTestExInternal` at 94.88%, and it is a codegen wall
  on inlining `rstl::optional_object::assign` (156 B vs the 125 B limit). Fixing it needs a way to
  inline one function in one TU that this compiler does not offer; all three routes tried are
  measured above and all three are walls.
- `include/WorldFormat/CCollisionSurface.hpp` is now known to be **safe to edit for the vertex
  layout** as long as `IsDegenerate` names the members directly — that combination is measured
  green (DOL sha1 + 86 RELs). It is not safe to add `rstl` overloads there.
- `config/G2ME01/symbols.txt` renames for TU-local weak instantiations keep working and are
  hash-safe: the string `fn_80246258` appears **0 times** in `build/G2ME01/main.dol` and the gate
  re-derives the sha1 anyway. Left at the default scope (no `scope:weak`), since a weak symbol
  nothing references is dead-stripped and would move retail's bytes.

WALL: LineTestExInternal 94.88% - retail inlines optional_object::assign in place; ours is 156B
over the 125B inline limit, and all three routes (inline_max_size sweep, optional_object.hpp
reshape, construct_impl override) measured worse or broke 87 REL checksums.
