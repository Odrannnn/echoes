# progress-unit-cdecalmanager — `MetroidPrime/CDecalManager` 7/65 -> 8/65

**Result: `OnTriangle` taken to 100.00%. `goal_check.sh`: PASS.**

## What I did

One function, `CDecalTriangleCollector::OnTriangle` in
`src/MetroidPrime/CDecalManager.cpp` (lines 44-64), 31.23% -> **100.00%**.
The unit's `matched_functions` in `build/report.json` went **7 -> 8 of 65**, the
only count a `progress` item is judged on. Two changes, both inside that function:

1. **The three vertex indices are read before any position is fetched.**
   `mPositions[reader.GetVertexIndex(a, GX_VA_POS)]` -> three `const uint indexN`
   locals, then `mPositions[indexN]`. Retail keeps all three indices live across
   the loads (`mr r29,r3` / `mr r30,r3` / `mulli r0,r29,12` after the three calls),
   which needs one more callee-saved register — retail's frame is 128 bytes and
   starts `stmw r27,108(r1)`, ours was 112 and started at r31.
   **31.23% -> 92.01%** (with change 2 already applied).

2. **`rstl::vector::push_back` written out.** Retail inlines it at this call
   site: `lwz r4,8(vec)` / `lwz r0,4(vec)` / `cmpw r0,r4` / `bne` / `slwi r4,r4,1` /
   `bl reserve`, then copy-construct into the slot and bump the count. That is the
   28 bytes the function was short by (ours was 308, retail 336).
   **39.68% -> 100.00%** (with change 1 already applied).

Nothing else in the file changed; no other function in the unit moved.

## Measured

| | before | after |
|---|---|---|
| `OnTriangle` (report `fuzzy_match_percent`) | 31.23% | **100.00%** |
| `OnTriangle` (objdiff `match_percent`) | 31.17% | 99.82% |
| `OnTriangle` compiled size | 308 B | **336 B** (retail 336 B) |
| unit `matched_functions` | 7 / 65 | **8 / 65** |
| unit `matched_code` | 1088 / 8096 | 1424 / 8096 (13.44% -> 17.59%) |
| unit `fuzzy_match_percent` | 34.35% | 37.21% |
| DOL `matched_functions` | 11542 / 28465 | **11543 / 28465** |
| DOL `linked` | 5625 | 5625 (unchanged) |

Gates, all green: `main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`probe_sources.sh` 754 files / 0 failed / 249 undefined / 0 duplicates,
`check_symbol_names.py` 0 missing, `decomp_build.sh` `All: 33.11% fuzzy, 25.96%
matched, 12.24% linked (11543 / 28465 functions)`, and
`./tools/goal_check.sh build/goal/item.json` -> `PASS`.

The unit stays `NonMatching`, as a `progress` item requires: `unit_fit.sh` reports
`.text over by 1100` and 65 functions in our object that retail's unit object does
not define.

## Negative results worth keeping (these cost the most time)

* **objdiff pairs functions by NAME only — there is no address fallback.**
  Measured: of the 65 base functions in this unit, exactly **11** carry a
  `target_symbol` in `objdiff-cli diff`, and all 11 are named on both sides. Every
  `fn_800E7xxx` / `fn_800E8xxx` has none and sits at 0.00% no matter what our object
  emits — `objdiff-cli diff -u main/MetroidPrime/CDecalManager fn_800E75D8` returns
  nothing at all. So **58 of this unit's 65 functions cannot be scored**, and the
  unit's matched count can only ever be raised through these 11:
  `AddToRenderer`, `Update`, `RemoveFromActiveList`, `AddDecal`,
  `GatherWorldSurfaces`, `CDecalTriangleCollector::{__ct__,OnTriangle}`,
  `Reinitialize`, `ShutDown`, `Initialize`, `__sinit_CDecalManager_cpp`.
  Reordering our object to retail's `.text` order therefore buys nothing for
  `matched_functions`, only for a future flip. (`check_decl_order.py` already
  reports this unit as ordered correctly.)

* **MWCC will not inline any function whose body contains a call.** That is why
  `rstl::vector::push_back` and `push_back_unsafe` appear in our object as
  out-of-line symbols called from `OnTriangle`, and `erase` likewise. Retail has
  `push_back` inlined at this site. The only way to reproduce retail's code is to
  write the body out at the call site, which is what change 2 does.

* **`rstl::construct(dest, &src)` is a trap and compiles to a bug.**
  `include/rstl/construct.hpp` has an overload
  `construct_impl(void* dest, T* const& src) { *static_cast<T**>(dest) = src; }`,
  so passing a *pointer* copy-constructs a **pointer** into the slot. That spelling
  built, linked, kept the DOL sha1 and scored **95.54%** while storing 4 bytes where
  a 48-byte `CCollisionSurface` belongs — a green build over a wrong change. The
  correct spelling is `rstl::construct(dest, src)` with the object.

* **Retail's `vector::push_back` growth test is `mCount == mCapacity`, not `>=`,
  and the growth is a bare `mCapacity * 2`** (no `mCapacity != 0 ? ... : 4`).
  `include/rstl/vector.hpp:78` has `>=` and the ternary. I did **not** change it:
  `rstl/vector.hpp` is shared by every unit and re-deciding it is a separate
  measurement, not part of this item. It is why change 2 spells the check out.

## Where the rest of this unit stands

* `AddToRenderer` 99.29%, `AddDecal` 83.59% (retail 756 B, ours 708 B — 48 bytes
  of real work missing), `GatherWorldSurfaces` 69.21% (retail 1020 B, ours 988 B —
  32 bytes missing). Those three are the only remaining reachable functions.
* Retail has a **116-byte function at 0x800E6F14** between `RemoveFromActiveList`
  and `AddDecal` that our object does not emit at all, and retail's 65 emitted
  functions do not line up one-for-one with our 76 (`GatherWorldSurfaces` is
  preceded in retail by six template functions, ours by a different six). Any future
  `match` item on this unit has to close that set difference first.

WALL: AddToRenderer 99.29% - remaining 6 instructions are only register numbers (retail it=r28 pool=r29 decal=r30 end=r31, every spelling below gives it=r28 end=r29 pool=r30 decal=r31); nine source spellings tried, none moved it.

### AddToRenderer spellings tried this run (report `fuzzy_match_percent`)

| spelling | score |
|---|---|
| baseline: hoisted `end` before the `for`, `const CDecal& decal` | 99.29% |
| `it` and `end` both declared before the loop (Prime 1's `AUTO(it)`/`AUTO(end)` shape) | 98.89% |
| `it != mActiveIndexList.end()` written in the `for` condition | 78.16% |
| `while (it != mActiveIndexList.end())` | 78.16% (57 instructions: MWCC re-evaluates `.end()` every iteration) |
| `const CDecal* decal = &*mDecalPool[*it].mDecal;` | 99.29% (byte-identical codegen) |
| `const SDecal& sdecal = mDecalPool[*it]; const CDecal& decal = *sdecal.mDecal;` | 99.29% (byte-identical) |
| `while` loop, hoisted `end`, `++it` in the body | 99.29% (byte-identical) |
| local `const rstl::reserved_vector<int,64>& active = mActiveIndexList;` | 99.29% (byte-identical) |
| named pool base `const CDecal* const pool = &*mDecalPool.begin().mDecal;` | 99.29% (byte-identical) |

The working model that did **not** pan out: MWCC appears to allocate r28..r31 to
locals before temporaries, in reverse declaration order within a scope, and
`Update` (100%) fits that — `it`=r28, then the preheader temporaries r29, r30, r31 in
creation order. Retail's `AddToRenderer` breaks it, because its `end` is allocated
*after* the two loop-body temporaries while being computed *before* them, which
would need a loop shape whose condition is numbered after its body. Nothing I wrote
produced that ordering.

## Nothing filed as `NEW:`

The two candidates are a lesson (objdiff pairs by name) and a measured wall
(`AddToRenderer`), both of which belong here, and the rest of this unit is the
current item's own target.