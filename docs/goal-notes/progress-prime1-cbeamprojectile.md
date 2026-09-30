# progress-prime1-cbeamprojectile

Target: `main/MetroidPrime/Weapons/CBeamProjectile` (kind `progress`, stays `NonMatching`).
One source file changed, `src/MetroidPrime/Weapons/CBeamProjectile.cpp`, two lines of substance
plus a comment. No asm, no config, no `tools/`, nothing under `build/goal/` except this file.

## Result, measured

`build/report.json` against `build/goal/judge/report.base.json` (the head the driver queued me on):

| | before | after |
|---|---|---|
| unit `matched_functions` | **4** / 7 | **6** / 7 |
| unit `fuzzy_match_percent` | 94.648350 | **99.315544** |
| unit `matched_code_percent` | 47.566720 | **95.290420** |
| `All:` fuzzy | 31.325006 | **31.326828** |
| `All:` matched functions | 10319 / 28465 | **10321** / 28465 |
| `All:` complete units | 741 | 741 (the unit is `NonMatching`, so nothing is newly linked) |

Per function (address order):

| function | size | before | after |
|---|---|---|---|
| `SetMaxLength(float)` | 44 | 100.000 | 100.000 |
| `UpdateFx(const CTransform4f&, float, CStateManager&)` | 1040 | 98.769 | **100.000** |
| `SetCollisionResultData(...)` | 120 | 85.467 | 85.467 (see the wall) |
| `ResetBeam(CStateManager&, bool)` | 24 | 100.000 | 100.000 |
| `PreRenderAllViewports(CStateManager&)` | 176 | 39.705 | **100.000** |
| `GetTouchBounds() const` | 168 | 100.000 | 100.000 |
| `CBeamProjectile::CBeamProjectile(...)` | 976 | 100.000 | 100.000 |

Swept every `(unit, function)` `fuzzy_match_percent` in both reports: **0 worse, 2 better, 0 gone,
0 new**, and `gate.sh`'s own per-function diff says `+2 functions at 100%, 0 units newly linked`.

## 1. `UpdateFx` 98.769 -> 100.000: a `const` on an extern global let the load be hoisted

`objdiff-cli diff -p . -u main/MetroidPrime/Weapons/CBeamProjectile -o - UpdateFx...` said the whole
function was one block: objdiff reported 15 `DIFF_ARG_MISMATCH` (register renumbering), one
`DIFF_INSERT` and one `DIFF_REPLACE`, all inside a 20-instruction window after the
`GetTransformedAABox` call. The only *structural* difference was where one instruction sat:

```
retail  0x80131638 lhz r6,kInvalidUniqueId@sda21
        0x8013163c lwz r5,lbl_80418360@sda21     <- the exclude material, 20
        0x80131640 sth r6,0x1c(r1)
ours    0x208     lwz r5,lbl_80418360@sda21     <- hoisted 12 instructions earlier
        ...the 24-byte copy of the box, in r7/r6 instead of r6/r5...
        0x238     lhz r6,kInvalidUniqueId@sda21
        0x23c     stw r0,0x1c0(r1)             <- the two stores swapped
        0x240     sth r6,0x1c(r1)
```

That `lwz` is the shift count for `__shl2i` in `CMaterialList(lbl_80418360)` ->
`value |= u64(1) << material` (MWCC's 3-word `__shl2i(hi, lo, count)`, hence `li r3,0; li r4,1`
and the load). Our build hoisted it because the file declared the symbol `const`, which makes the
load a *pure* read the optimiser may place anywhere; taking `r5` early pushed the box copy onto
r6/r7 and reordered the two stores with it.

**Retail's own symbol is not const.** `build/binutils/powerpc-eabi-nm build/G2ME01/main.elf`:

```
80418360 D lbl_80418360
8041a7c0 D lbl_8041A7C0
8041c024 D lbl_8041C024
```

`D` - writable `.data`. So `extern "C" const EMaterialTypes lbl_80418360;` was a lie that bought the
optimiser freedom retail does not have. Dropping `const` on that one declaration is the whole fix:

```c
-extern "C" const EMaterialTypes lbl_80418360; // kMT_NoPlatformCollision (20) in retail.
+extern "C" EMaterialTypes lbl_80418360; // kMT_NoPlatformCollision (20) in retail.
```

General rule worth keeping: **before blaming the scheduler, check whether a `const` you wrote is
lying.** `nm` on `main.elf` says `D` for writable data and `r`/nothing for read-only; a `const`
extern for a `D` symbol is a licence for MWCC to hoist, CSE and reorder loads across stores to
`this`, and the diff then reads as a dozen register mismatches instead of one misplaced load.
`lbl_8041A7C0` and `lbl_8041C024` are `D` too and are still declared `const` - their functions are
already 100% and I did not touch them (a `const` there can only have been load-bearing if the score
is 100% today; re-measure before changing one).

## 2. `PreRenderAllViewports` 39.705 -> 100.000: bind the aggregate return by reference

This function has no Prime 1 counterpart (Prime 1 has `CalculateRenderBounds` and a `Touch`), so
nothing to copy - but the diff said retail is 48-byte-frame and does *no* float staging:

```
retail  bl GetTransformedAABox      -> result at 8(r1)
        lwz r5,8(r1) / lwz r0,12(r1) / stw r5,0xcc(r30) / stw r0,0xd0(r30)   ... 6 words
        lwz r5,8(r1) / lwz r0,12(r1) / stw r5,0xe4(r30) / stw r0,0xe8(r30)   ... 6 words again
```

i.e. both inlined 24-byte copies read the call's return slot. Ours copied the box *through the stack
as floats* first (frame 80, `lfs f1,20(r1)` ... `stfs f3,52(r1)`, then 6 word loads out of 44(r1)),
because the source copied the returned aggregate into a **value** local. Binding the same temporary
to a `const&` makes the compiler use the return slot directly:

```c
-  const CAABox bounds = mLocalBounds.GetTransformedAABox(mXf);
+  const CAABox& bounds = mLocalBounds.GetTransformedAABox(mXf);
```

Spelling is load-bearing: `const CAABox bounds` = 39.705, `const CAABox& bounds` = **100.000**,
`SetOtherBounds(mLocalBounds.GetTransformedAABox(mXf)); SetRenderBounds(mWorldBounds);` = 99.182,
and swapping the two setters = 39.636. General rule: **when retail copies a returned struct twice
from one slot, do not name it by value** - `const T&` keeps the temp in the callee's return slot,
`const T` makes MWCC stage it (as floats, for a 24-byte `CAABox`) before the copies.

## 3. Prime 1's source, per function (what the item asked for)

- `SetCollisionResultData` - **Prime 1's body is character-for-character what this file already
  had**, and it did not match. Prime 1 compiles the same seven stores to 100% under GC/1.3.2; the
  remaining diff is a GC/2.7 scheduling artefact (see the wall).
- `UpdateFx` - Prime 1's body needed one real edit for this repo (its `MakeExclude` /
  `CMaterialFilter` shape and `MakeScaledForTime` are already adapted in the file) and then the
  declaration fix above. Prime 1's own `CMaterialFilter::MakeExclude(CMaterialList(kMT_...))` is
  *not* what Echoes retail has: Echoes builds the filter with the explicit 3-argument form
  (`include = 0x00000000FFFFFFFF`, `type = kFT_Exclude`), which is what the file already did.
- `PreRenderAllViewports`, `GetTouchBounds`, the ctor, `ResetBeam`, `SetMaxLength` - already at
  100% before this run; untouched (except `PreRenderAllViewports`, above).

## Wall: `SetCollisionResultData`, 85.47%

120 bytes, 30 instructions, 15 of them differ. Same instruction multiset, same order of loads
(`0,4,8,12,16,20,24(r5)` = time, point.xyz, plane normal) and same order of stores
(`0x430,0x444,0x448,0x44c,0x438,0x43c,0x440(r3)`). Ours emits seven back-to-back `lfs`/`stfs` pairs
through `f0`. Retail pipelines them two deep, alternating `f0`/`f1`, each load issued two
instructions ahead of its store:

```
retail  lfs f0,0(r5) | lfs f1,4(r5) | stfs f0,0x430 | lfs f0,8(r5) | stfs f1,0x444 | lfs f1,12(r5) | ...
ours    lfs f0,0(r5) | stfs f0,0x430 | lfs f0,4(r5) | stfs f0,0x444 | lfs f0,8(r5) | stfs f0,0x448 | ...
```

Everything else in the function matches, including the `lhz` of `kInvalidUniqueId@sda21` and the
`addi r4,r5,4` for `SetTranslation`. The 15 differing instructions are the f-register choice and
the load/store interleave - no missing work, no wrong offsets. Twenty spellings tried in this run,
all measured with `tools/try_edit.py` (score = that function's `fuzzy_match_percent`):

| spelling | score |
|---|---|
| as-is (control, = Prime 1's body verbatim) | **85.467** |
| one comma statement for the three copies | 85.467 |
| `const CRayCastResult& cres = res;` alias | 85.467 |
| `static_cast<const CVector3f&>` on the plane normal | 85.467 |
| destinations taken by pointer | 85.467 |
| `mBeamLength` moved last | 85.433 |
| normal copied before point | 85.267 |
| `res.mPoint` / `res.mPlane.mNormal` directly | 85.267 |
| `const CVector3f& pt/n` locals | 79.767 |
| `SetX/SetY/SetZ` member-wise | 79.767 |
| `const CVector3f pt(res.GetPoint())` | 65.500 |
| `CVector3f n(res.GetPlane().GetNormal())` only | 71.800 |
| both vectors from ctor temporaries | 52.000 |
| both ctor temporaries + `const float t` | 52.000 |
| ctor temporaries declared before `mDamageType` | 58.767 |
| `CVector3f(res.GetPlane().GetNormal())` | 78.700 |
| `mCollisionActorId` first | 6.667 |
| by-value temps for time/point/normal | 63.900 |

The one informative result: `const CVector3f pt(res.GetPoint());` **does** make the compiler batch
the loads (`lfs f1,4(r5); lfs f2,8(r5); lfs f3,12(r5)` then the stores, three registers live), so
MWCC/2.7 is capable of grouping here - it just never picks a 2-deep pipeline for the plain
copy-from-`const&` form. Retail's schedule is not reachable from any spelling of this statement I
found.

WALL: SetCollisionResultData__15CBeamProjectileFQ215CBeamProjectile11EDamageTypeR14CRayCastResult9TUniqueId 85.47% - GC/2.7 emits the 7 float copies as 7 back-to-back lfs/stfs pairs through f0 where retail pipelines them 2 deep across f0/f1; same loads, same stores, no spelling tried moves a load above the preceding store.

## Gates, all re-run after the last build

`./tools/goal_check.sh build/goal/item.json` (the judge's own script, in this worktree):

```
goal_check: item progress-prime1-cbeamprojectile (progress) target=MetroidPrime/Weapons/CBeamProjectile
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10319 -> 10321   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.33% fuzzy, 23.73% matched, 11.83% linked (10321 / 28465 functions)
  ok    target rose: main/MetroidPrime/Weapons/CBeamProjectile: 4 -> 6 / 7 functions
  ok    no asm added
goal_check: PASS progress-prime1-cbeamprojectile
```

`gate.sh`'s own log: `GATE PASS 82b23dc1+2 changed`, every sub-check `ok` (configure, ninja +
build.sha1, hashes vs config.yml, report, per-function diff, module wiring, dol_read, docs claims,
gs offsets, raw offsets, decl order, files.cmake, module order, port probe, port link gap) - so the
DOL sha1, all 86 RELs and `build.sha1` are unchanged, as they must be for a `NonMatching` unit.

## Instrumentation worth rebuilding (all in `.tmp/opencode/`, gitignored, not part of the diff)

- `tryasm.py` / `cbeam.py`: apply one spelling, build, print retail-vs-ours instruction table for
  one function (`objdump -d` on `build/G2ME01/main.elf` at the report's `virtual_address`).
- `trydiff.py`: per variant, the fuzzy percent *plus* objdiff's `DIFF_*` list, which names the
  structural difference instead of leaving you to diff 30 lines by eye. This is what found the
  `lwz lbl_80418360` position: the percent alone (98.769) said "3 instructions" and hid that the
  whole window was one misplaced load plus its register shadow.
- `tools/try_edit.py` for scores, `tools/fast_try.sh` for the unit line. Both are in `tools/` and
  were enough for the two wins; the extra scripts only made the *diagnosis* fast.

## Not done, on purpose

- `SetCollisionResultData` (wall above) - no spelling found; not requeued as a `NEW:` item because
  it is a measured wall, not an unexplored one.
- The unit stays `NonMatching` (6/7). Flipping needs `SetCollisionResultData` at 100%; I did not
  touch `configure.py`.
- `kInvalidUniqueId` is `extern const TUniqueId` in the shared `TGameTypes.hpp` and is `B` in
  `main.elf`. It is a *shared* header, so I left it alone: it loads `lhz r6,kInvalidUniqueId@sda21`
  in the right slot in both functions already, and changing it would move 505 units.
