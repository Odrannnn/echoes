# progress-prime1-collisionutil

**Result: `Collision/CollisionUtil` 8/29 -> 13/29 functions at 100%.** `matched_code` 17.42% -> 24.22%,
`.text` fuzzy 86.68% -> 89.69%. No function anywhere got worse (`tools/report_diff.py` against
`build/goal/judge/report.base.json`: `matched 9512 -> 9517`, `linked 4852 -> 4852`, `no regression`).
The unit stays `NonMatching`; `flip_test.sh` was not run, as the item says.

## The diff (2 files)

1. `include/Kyoto/Math/CSphere.hpp` - **the whole win.**
   `CVector3f GetCenter() const` -> `const CVector3f& GetCenter() const`. Prime 1 has the reference
   form; this repo returns by value, so every caller that passes a centre on to a `const CVector3f&`
   parameter had mwcceppc materialise a stack temporary. No layout change (`CHECK_SIZEOF` untouched,
   `tools/check_raw_offsets.py` ok, 152 sites in 61 files).
2. `src/Collision/CollisionUtil.cpp` - `LineCircleIntersection2d`: `const CVector3f delta =
   sphere.GetCenter() - point;` -> Prime 1's exact three-statement spelling
   (`delta.SetY/SetZ/SetX(...)`, in that order). Prime 1's source verbatim.

## Per function, before -> after, and whether Prime 1's source was the answer

Measured from `build/report.json`, baseline = the judge's `report.base.json` at `097f0bc`.

| function | before | after | Prime 1 source |
|---|---|---|---|
| `RaySphereIntersection_Double` | 91.05 | **100.00** | already identical; the header fix is what did it |
| `AABoxSphereIntersection` | 78.51 | **100.00** | already identical; header fix |
| `AABoxSphereIntersectionRadius` | 74.19 | **100.00** | already identical; header fix |
| `TriSphereOverlap` | 27.35 | **100.00** | already identical; header fix |
| `LineCircleIntersection2d` | 92.40 | **100.00** | **Prime 1's spelling was required** - the `operator-` form does not match |
| `MovingSphereAABox` | 89.11 | 97.33 | no source difference left (only `CCast::ToReal32` vs `static_cast<float>`); moved only because of the header fix |
| `TriSphereIntersection` | 80.49 | 88.15 | no source difference; moved only because of the header fix |
| `RayAABoxIntersection(CMRay,CAABox,float&,float&)` | 99.78 | 99.78 | identical, blocked on a literal (below) |
| `RayAABoxIntersection_Double` | 98.83 | 98.83 | identical, blocked on a literal |
| `RayAABoxIntersection(CMRay,CAABox,CVector3f&,float&)` | 98.71 | 98.71 | identical, blocked on a literal |
| `AABox_ABBox_Moving` | 96.75 | 96.75 | identical, blocked on a literal |
| `TriPointSqrDist_Float` | 95.15 | 95.15 | no Prime 1 counterpart (MP2-only); 95% is scheduling |
| `TriBoxOverlap` | 82.44 | 82.44 | **byte-identical to Prime 1 and still 82%** - pure GC 1.3.2 vs 2.7 codegen |
| `AABoxAABoxIntersection` (4-arg) | 84.68 | 84.68 | Prime 1's `CCollisionInfo(..., false)`; ours needs `kInvalidUniqueId.value` - structural, not a spelling |
| `AddAverageToFront` | 75.06 | 75.06 | same uniqueId ctor difference |
| `FilterByClosestNormal` | 69.04 | 69.04 | same |
| `FilterOutBackfaces` | 58.18 | 58.18 | same |
| `SphereAABoxIntersection` | 60.61 | 60.61 | **no Prime 1 counterpart**; target-derived guess |
| `RayAABoxIntersection(CVector3f,CVector3f,float,CAABox)` | 52.89 | 52.89 | **no Prime 1 counterpart**; target-derived wrapper |
| `RayTriangleIntersection` | 34.41 | 34.41 | **byte-identical to Prime 1 and still 34%** |
| `AABoxPointSqrDist` | 0.00 | 0.00 | **no Prime 1 counterpart**; see the recipe below |

So: Prime 1's *text* was already in this file for 20 of 29 functions (the whole-file diff against
`prime-ref/src/Collision/CollisionUtil.cpp` is 18 hunks, all of them MP2-only additions or the
`CCollisionInfo` uniqueId difference). Copying Prime 1 across was **not** the lever. The lever was
that this repo's `CSphere::GetCenter()` returns by value where Prime 1 returns a reference - i.e. a
header-shape difference, exactly the thing the item's brief warned about, found from the measured
diff instead of by copying.

## Where the remaining 16 are stuck

**1. The `.sdata2` sentinels (5 functions at 96.75-99.78%).** Retail's `.sdata2` is 140 bytes,
ours is 116. Retail has four constants ours has none of, and uses them as the `tMin`/`tMax`
sentinels where our source writes `-999999.f` / `999999.f`:

```
$ build/binutils/powerpc-eabi-objdump -s -j .sdata2 build/G2ME01/obj/Collision/CollisionUtil.o
 0040 c7efffff e0000000 47efffff e0000000     <- retail, only these four are missing from ours
$ ... -s -j .sdata2 build/G2ME01/src/Collision/CollisionUtil.o   (116 bytes, no such words)
```

`0x47EFFFFF` = `+122879.99f`, `0xE0000000` = `-3.6893488e19f`. Retail `BoxLineTest` opens with
`lfs f0, 0xE0000000 ; stfs f0, 0(r6)` and `lfs f0, 0x47EFFFFF ; stfs f0, 0(r7)` - i.e. `tMin`,
`tMax`. The relocation map says who loads them:

```
$ build/binutils/powerpc-eabi-objdump -r build/G2ME01/obj/Collision/CollisionUtil.o   (offsets -> functions)
lbl_8041E1C0 (-122880)  MovingSphereAABox, LineCircleIntersection2d, AABox_ABBox_Moving
lbl_8041E1C4 (0xE0000000) BoxLineTest, RayAABoxIntersection(CMRay,CAABox,float&,float&)
lbl_8041E1C8 (+122880)  BoxLineTest, RayAABoxIntersection(CMRay,CAABox,float&,float&)
```

**I could not decode those two literals into anything a C++ source would plausibly write.** What is
*not* the answer is `+-999999.f` (`0xC97423F0` / `0x497423F0`), which is what both Prime 1 and this
repo write - and Prime 1's own `BoxLineTest` is Matching *there* with that literal, so the Echoes
fork changed the sentinel. Do not spend another run guessing `1e6f`, `FLT_MAX`, `-1e19f` etc.; the
search space is the whole float range. **`tools/check_raw_offsets.py` has no site for this**, and
`CHECK_SIZEOF` will not catch it. What *will* catch it, cheaply: `objdiff-cli diff` reports these as
`DIFF_ARG_MISMATCH` on `lfs f?, <sym>@sda21`, and a function can still read 100% while its constant
is wrong - see the next paragraph.

**2. A by-value accessor does not always show up in the function's score.** Before the fix, ours and
retail's `BoxLineTest` loaded *different* `tMin`/`tMax` constants (ours `@2771` = `0xC97423F0`,
retail's `lbl_8041E1C4` = `0xE0000000`) and `report.json` still scored `BoxLineTest` **100.0%**. The
constant is compared in the section's `data_diff`, not in the function's `fuzzy_match_percent`. So
"100% on every function" is *not* "the unit is right", and `flip_test.sh` is the only thing that
decides - which is the repo's own rule, now with a measured reason.

**3. `AABoxPointSqrDist` is 0% because retail's body is unrolled.** From
`.tmp/opencode/retail.dis` (`sed -n '/^00003140 <AABoxPointSqrDist/,+40p'`), retail's body is
`cmplwi r5,0 ; lfs f1,0x0f ; beq +0xd0` - one `cmplwi`/branch pair at the top and **no loop at all**
(compare `BoxLineTest`, which does have `li r0,3 ; mtctr r0`). So the 3 axes are written out, and
the whole thing is written out **twice**, once storing into `closestPoint` and once not. Each axis is

```
fcmpo cr0, point[i], min[i] ; bge -> else ; fsubs f2,f2,f0 ; stfs f0, i*4(r5) ; fmadds f1,f2,f2,f1
else: fcmpo cr0, point[i], max[i] ; ble -> else2 ; fsubs f2,f2,f0 ; stfs f0, i*4(r5) ; fmadds ...
else2: stfs f2, i*4(r5)          <- closestPoint[i] = point[i]
```

Our body is a `for (int axis = 0; axis < 3; ++axis)` loop with the same per-axis arithmetic, which is
why it is 0% and not 40%. Writing the unrolled pair is the whole job; no Prime 1 reference exists
(Prime 1 has no `AABoxPointSqrDist`), so this is 89 instructions read off the asm.

**4. Not fixable by spelling: `RayTriangleIntersection` 34.41%, `TriBoxOverlap` 82.44%.** Both bodies
are already byte-identical to Prime 1's, and Prime 1 has both at 100% under GC/1.3.2. Same source,
different compiler. Per the item's own stop rule these are walls, not work.

## Process lessons (not `NEW:` items)

- **`objdiff-cli diff` in one-shot mode is the right instrument and is not in `tools/`.**
  `./build/tools/objdiff-cli --no-color diff -1 build/G2ME01/src/<U>.o -2 build/G2ME01/obj/<U>.o -o - <symbol>`
  prints JSON with `match_percent` and a `diff_kind` per differing instruction. Passing `-p`/`-u`
  as well as `-1`/`-2` makes it refuse (`Either target and base or project and unit`); passing both
  without a symbol gives `Interactive mode requires a symbol name`; without `-o` it wants a TTY
  (`os error 6`). With only `-1 -2 <symbol>` and no `-o` it still wants a TTY - **`-o -` is required**.
- **A header's return type is codegen, and the wrong one is invisible in a percentage.** The by-value
  `GetCenter()` cost 5 functions in this unit and 4 units' worth of stack traffic elsewhere. When a
  Prime-1-derived function is close but not exact, diff the *headers first*: a by-value accessor that
  retail returns by reference shows up as 4-6 extra instructions (`lfs`/`stfs` of a temporary) and
  nothing else.
- **Do not trust the tree you inherit.** This worktree arrived with two files already dirty that were
  not mine: `src/Kyoto/Audio/CDSPStreamManager.cpp` (`(void)0; // TEMP stub for link measurement only`,
  which is what makes the DOL not link - see below) and `src/MetroidPrime/ScriptObjects/CScriptCamera.cpp`
  (79 lines, the previous item's rejected change, written at 22:04:16Z - *after* the driver's
  `resetting the worktree after an agent error` at 22:02:25Z). Both were reverted; the diff above is
  the whole of my change. A leftover change in a lane worktree is indistinguishable from yours in
  `git status`, and the reviewer reads `git diff`.

## Blocker that will make the judge fail this item (pre-existing, not mine)

**The DOL has not linked since `ada6d97` (`match-stream`), so `goal_check.sh` cannot pass on this
lane for any item.** Measured, at HEAD `097f0bc` with a clean tree:

```
$ ./tools/decomp_build.sh Collision/CollisionUtil
[3/7] LINK build/G2ME01/main.elf
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o
```

`build/goal/judge/record-gate.log` (recorded at `097f0bc` by the driver) already reads
`ninja + build.sha1  FAIL` / `undefined: 'sndStreamMixParameter'`. Consequence for a `progress`
item: `gate.sh` fails, so `decomp_build.sh` prints no `All:` line, so goal_check reports
`FAIL ... gate.sh, decomp_build.sh printed no All: line`. `build/goal/run.log` shows
`match-cscriptcamera` failing on exactly this at 21:51Z, and this item's predecessor
`progress-prime1-cmorphball` dying on it at 22:01Z.

`docs/goal-notes/match-cmetaanimrandom.md` diagnosed it correctly and re-applied the four
`MUSY_VERSION` guards, but the fix **cannot survive a commit**: `tools/run_goal.sh:446` stages only

```
git add -A -- src include config docs configure.py files.cmake CMakeLists.txt
```

so `extern/musyx/src/musyx/runtime/stream.c` is dropped from the commit and the DOL is broken again
at the next head. `git show --stat b04f1a9` (the commit that carried the re-applied guards) lists 4
files: `configure.py`, `docs/HANDOFF.md`, `docs/goal-notes/match-cmetaanimrandom.md`,
`src/Kyoto/Animation/CMetaAnimRandom.cpp`. That is the durable bug, and it is in the driver, not in a
unit - so I have not touched it, per the brief.

`NEW: match-musyx-stream-guards | match | musyx/runtime/stream.c | the DOL has not linked since ada6d97 - sndStreamMixParameter is undefined, so gate.sh ninja, decomp_build's All: line and flip_test all fail for every item on the lane; the fix is four MUSY_VERSION guard edits in extern/musyx/src/musyx/runtime/stream.c (lines 336, 422, 431 and two new #if blocks), which run_goal.sh cannot commit because line 446 stages only src include config docs configure.py files.cmake CMakeLists.txt - add extern/ to that pathspec or the fix is lost again`

## One `NEW:` for the unit

`NEW: progress-collisionutil-sentinels | progress | Collision/CollisionUtil | retail's .sdata2 holds 0xE0000000 and 0x47EFFFFF where BoxLineTest, RayAABoxIntersection(CMRay,CAABox,float&,float&), AABox_ABBox_Moving, LineCircleIntersection2d and MovingSphereAABox load their tMin/tMax sentinels, and our source writes -999999.f/999999.f (0xC97423F0/0x497423F0); five functions sit at 96.75-99.78% and cannot reach 100% until the fork's literal is identified, and a 100%-scoring function does not prove its constant is right`

## Gates run in this tree

- `./tools/fast_try.sh Collision/CollisionUtil` after each edit - the numbers above.
- `ninja -k 0` over the whole tree (20 edges: 14 MWCC recompiles of the units that include
  `CSphere.hpp` - `Kyoto/Math/CSphere`, `CFrustumPlanes`, `WorldFormat/CCollidableOBBTree`,
  `CCollidableOBBTreeGroup`, `CMetroidAreaCollider`, `MetroidPrime/CGameCollision`,
  `CScriptCannonBall`, `CPlayerDynamics`, `CRagDoll`, `CBallCamera`, `CScriptCamera`, `CPlayer`,
  `CMorphBall`, `CPlayerGun`). It then fails **only** at `LINK build/G2ME01/main.elf` with the
  pre-existing `sndStreamMixParameter` error above; no compile errors.
- `./build/tools/objdiff-cli report generate` + `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`
  -> `matched 9512 -> 9517   linked 4852 -> 4852   (+5 functions at 100%, 0 units newly linked)`,
  `no regression`. All five `+100%` lines are in `Collision/CollisionUtil`.
- `python3 tools/check_symbol_names.py` -> `checked 484 units; 0 declared names are missing from their object`.
- `python3 tools/check_raw_offsets.py` -> `ok: 152 raw-offset site(s) in 61 file(s)`.
- **Not run, and cannot be:** `gate.sh` step 2 (`ninja + build.sha1`), the 86-REL hash check, and the
  `main.dol` sha1, because the link above fails first. `sha1sum build/G2ME01/main.dol` would read
  the **previous** `main.dol`, which is the stale-artifact trap `gate.sh` step 0 warns about.
  `flip_test.sh` likewise cannot run (it links).

## Not committed, as instructed. Tree state

`include/Kyoto/Math/CSphere.hpp` (+1/-1) and `src/Collision/CollisionUtil.cpp` (+4/-1) are the
whole diff; `git status --porcelain --untracked-files=all` lists only those two. Helper scripts I
wrote are under `.tmp/opencode/` (gitignored) and are not part of the change.

## 2026-09-30 retry in `goal/lane-9` at `a10cad0`

This tree was clean before edits. Re-measured with `./tools/decomp_build.sh Collision/CollisionUtil`:
**12/29 functions**, 89.52% fuzzy; the previous run's `CSphere::GetCenter()` reference-return fix is
already in this tree at `include/Kyoto/Math/CSphere.hpp:13`. This retry changed only
`src/Collision/CollisionUtil.cpp` and gained two exact functions.

| function | before -> after | Prime 1 source / result |
|---|---:|---|
| `RayAABoxIntersection(CVector3f,CVector3f,float,CAABox)` | 52.89 -> 52.89 | MP2-only guessed wrapper; no Prime 1 counterpart |
| `RayAABoxIntersection(CMRay,CAABox,CVector3f&,float&)` | 98.71 -> 98.71 | Prime 1 body already identical; no gain this retry |
| `RayAABoxIntersection_Double` | 98.83 -> 98.83 | Prime 1 body already identical; no gain this retry |
| `RayAABoxIntersection(CMRay,CAABox,float&,float&)` | 99.78 -> 99.78 | Prime 1 body already identical; retail uses different `.sdata2` sentinels |
| `AABoxAABoxIntersection` (materials) | 84.68 -> 84.68 | Prime 1 uses `CCollisionInfo(..., false)`; Echoes requires `kInvalidUniqueId.value` |
| `SphereAABoxIntersection` | 60.61 -> 60.61 | No Prime 1 counterpart; target-derived implementation |
| `AABoxPointSqrDist` | 0.00 -> **100.00** | MP2-only; reconstructed retail's two unrolled axis paths |
| `RayTriangleIntersection` | 34.41 -> 34.41 | Byte-identical Prime 1 source; no codegen gain |
| `FilterByClosestNormal` | 69.04 -> 69.04 | Prime 1 body; constructor's unique-id argument differs |
| `FilterOutBackfaces` | 58.18 -> 58.18 | Prime 1 body; constructor's unique-id argument differs |
| `AddAverageToFront` | 75.06 -> 75.06 | Prime 1 body; constructor's unique-id argument differs |
| `AABox_ABBox_Moving` | 96.75 -> 96.75 | Prime 1 body already equivalent; `.sdata2` sentinel remains |
| `TriBoxOverlap` | 82.44 -> 82.44 | Byte-identical Prime 1 source; GC/2.7 codegen differs |
| `LineCircleIntersection2d` | 92.40 -> **100.00** | Needed Prime 1's explicit `SetY`, `SetZ`, `SetX` delta writes; `operator-` fails |
| `MovingSphereAABox` | 97.33 -> 97.33 | Prime 1 logic already present; reference-return header fix is already upstream here |
| `TriSphereIntersection` | 88.15 -> 88.15 | Prime 1 logic already present; reference-return header fix is already upstream here |
| `TriPointSqrDist_Float` | 95.15 -> 95.15 | MP2-only; no Prime 1 counterpart |

### New work and measurements

- `LineCircleIntersection2d`: the current source had regressed to `sphere.GetCenter() - point`; restoring
  Prime 1's component writes (`SetY`, then `SetZ`, then `SetX`) returned it to 100%.
- `AABoxPointSqrDist`: retail has no loop and duplicates the axis checks for non-null and null
  `closestPoint`. Expanding both paths moved it 0.00 -> 94.78%; changing min/point scalar order moved it
  to 98.65%. Binding the min and point components as `const float&` via `kDX/kDY/kDZ` matched all bytes,
  100%. This verifies the old note's unrolling hypothesis and adds the component-reference spelling.
- Final unit result: **14/29**, 91.55% fuzzy. `report_diff.py` measured `matched 10128 -> 10130`,
  `linked 4917 -> 4917`, exactly these two `+100%` functions, and `no regression`.

Required judge command and output:

```
$ export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-prime1-collisionutil (progress) target=Collision/CollisionUtil
goal_check: baseline .../wt-mp2-goal-L9/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10128 -> 10130   linked 4917 -> 4917
  ok    check_symbol_names.py
  ok    All:  31.17% fuzzy, 23.46% matched, 11.78% linked (10130 / 28465 functions)
  ok    target rose: main/Collision/CollisionUtil: 12 -> 14 / 29 functions
  ok    no asm added
goal_check: PASS progress-prime1-collisionutil
```

`goal_check.sh` refreshed the derived `docs/HANDOFF.md` state block to match the new count. No commit
was made. No current-run `WALL:` is warranted: the one function reconstructed without a Prime 1 body
now matches exactly; remaining sub-100 functions were not re-tried in this run.
