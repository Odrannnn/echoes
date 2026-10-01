# progress-unit-cballcameratransitions

`main/MetroidPrime/Cameras/CBallCameraTransitions` went from **0/8 to 2/8** matched functions; the
unit stays `NonMatching`. The two 136-byte failsafe checks are at **100.00%** each
(`build/report.json`). Nothing anywhere got worse, and the diff adds no `asm`.

## What I did

Wrote the real bodies of the two smallest functions in the unit:

- `CBallCamera::CheckFailsafeFromMorphBallState` - retail 0x801AA364, 136 bytes
- `CBallCamera::CheckFailsafeToMorphBallState`   - retail 0x801A9A30, 136 bytes

Both are one call each. From `tools/dis.sh 0x801AA364 0x88`:

```
lwz  r5,1348(r3)     ; mFromBallTransition / mToBallTransition (0x548)
lfs  f31,112(r5)     ; mSpline.mLength (0x70)
stw  r0,12(r1) / stw r0,8(r1)      ; a CMaterialList zeroed on the stack, passed as &arg5
bl   GetCameraManager__11CGameCameraCFRC13CStateManager
lfs  f0, 24.0        ; step divisor, a .sdata2 float at 0x8041CD08
lis/addi r4,r6       ; the filter's address, 0x803DB4A0 (ADDR16 pair)
fdivs f1,f31,f0      ; step = mSpline.GetLength() / 24
lfs  f2, 0.0         ; thickness, .sdata2 at 0x8041CCD8
addi r4,r5,60        ; &mSpline (0x3C)
mr   r7,r31 / addi r8,r1,8 / li r5,0
bl   CheckSplineCollision__14CCameraManagerCFRC13CMotionSplineiRC15CMaterialFilterR13CStateManagerR13CMaterialListff
```

So: `GetCameraManager(mgr).CheckSplineCollision(mSpline, 0, kFilter, mgr, hitMaterial, length/24.f, 0.f)`.

The two members are `mFromBallTransition` (0x544) and `mToBallTransition` (0x548), already named in
`include/MetroidPrime/Cameras/CBallCamera.hpp:249-250`; `mSpline` is at 0x3C inside each
transition struct and `mLength` at 0x70 inside `CMotionSpline` (measured, not recalled - the probe in
`tools/probe_offsets.cpp` style gives mControlPoints 0x04, mKnots 0x14, mKnotDistances 0x24,
sizeof 0x44, mDuration 0x38, so mLength 0x34 + 0x3C = 0x70).

## The filter, and the one thing that cost a gate failure

`0x803DB4A0` is a `CMaterialFilter` (`symbols.txt:19091`, `.bss`, size 0x18) built by `fn_801AA3EC`,
which is this unit's `__sinit_` - retail's `.ctors` entry 0x803A5564 points at it. Disassembling
`fn_801AA3EC` (216 bytes) and reading the material ids it shifts
(`__shl2i(0,1,<id>)`, five calls, ids read out of `.sdata` 0x80418620..0x80418630) gives
**include {kMT_Unknown59}, exclude {kMT_NoPlatformCollision, kMT_Player, kMT_Character,
kMT_CameraPassthrough}, type 3 = kFT_IncludeExclude** - `stw r5,16(r3)` with `li r5,3`. That is
byte-for-byte the same filter `CBallCamera.cpp:17` already builds for its own use, so I declared a
second copy in an anonymous namespace here.

`fn_801AA3EC` itself is **not** matched (still 0.00%) and I did not try: see the wall below.

## The source shape that matters (measured, 8 spellings)

`tools/g2try.sh` / `tools/fn.py`, counting mismatching instructions in the 136-byte body:

| spelling | mismatches |
|---|---|
| inline `mSpline.GetLength() / 24.f` as the argument | 26 |
| `const CMotionSpline& spline = ...; spline.GetLength()/24.f` | 33 |
| `CCameraManager& camMgr = GetCameraManager(mgr);` first | 32 |
| `0.f * 1.f` for thickness | 26 |
| `const float step = ...` **before** the `CMaterialList` | 26 |
| `const float step = ...` **before** the `CMaterialList` declared **first**, i.e. hoisted | **6** |
| `CMaterialList` declared first, then `step` | 26 |
| same but on one line | 26 |

The winner, and the reason the others fail: retail's first seven instructions are
`mr r30,r3 / li r0,0 / lwz r5,1348(r3) / mr r31,r4 / lfs f31,112(r5) / stw r0,12(r1) / stw r0,8(r1)`
- the `step` division happens **after** `GetCameraManager`, but the *load* of `mLength` happens
**before** the `CMaterialList` is zeroed. Declaring `const float step = ...;` as the first statement
is what reproduces that; every spelling that computes it as an argument lets mwcceppc sink the
`lfs` below the two `stw`s and permute r30/r31. With it, the body is instruction-for-instruction
retail's, only the relocated words differ (which objdiff resolves): **100.00%**.

Register note, since it will bite the next function in this unit: retail keeps `this` in **r30** and
`mgr` in **r31** across the call and reloads `1348(r30)` after it - the `mFromBallTransition` read is
*not* hoisted, because the call could in principle write it. Any spelling that lets the compiler
keep `&mSpline` in a callee-saved register across the call scores 26; the one that re-reads the
member after the call scores 6.

## The link gate, and what I did about it

First `tools/goal_check.sh` run: `FAIL gate.sh` - `link_check: STRICT FAIL, 325 undefined against a
baseline of 324 (GREW)`. The new name was
`CGameCamera::GetCameraManager(CStateManager const&) const`. Cause: `CBallCameraTransitions.cpp` is
in `files.cmake:1268`, and the only definition of that member is in
`src/MetroidPrime/Cameras/CGameCamera.cpp:165`, which `files.cmake` excludes
(`tools/check_files_cmake.py:427`, 318 -> 321 measured). Writing the call opened the name.

Fix: the same body in `src/MetroidPrime/PortGlobals.cpp` (a file that is deliberately not a
`configure.py` unit, so it perturbs no object's SDA offsets), documented in place with the retail
address and the reason. Net effect on the port link: **324 undefined, 0 duplicates, unchanged**.
This mirrors what `CGameCameraSetAspectRatio.cpp` already does for `SetAspectRatio`; when
`CGameCamera.cpp` is ever listed, these two copies are what have to go.

## What is still not done, and the walls

- `fn_801AA3EC` (216 B) - the static initialiser for `skFailsafeFilter`. Best attempt: the
  anonymous-namespace `const CMaterialFilter` as written, which emits a 192-byte `__sinit_` against
  retail's 216. The gap is structural: retail loads each material id from `.sdata` and calls
  `__shl2i` five times, accumulating `r30:r31`, while our `CMaterialList` ctor stores each partial
  result to `.bss` and reloads it. Spellings tried, with `__sinit_` size / mismatching
  instructions against retail's 216: named `CMaterialList` include+exclude pair via
  `MakeIncludeExclude` 296/63; the same with `operator|` chaining 304/64; the explicit
  `CMaterialFilter(include, exclude, kFT_IncludeExclude)` 296/63; the inline form actually kept
  **192**/**best**. No spelling found that keeps the running value in registers.
- `TransitionFromMorphBallState` 860 B (0.65%), `TransitionToMorphBallState` 908 B (0.62%),
  `UpdateTransitionFromBallCamera` 1360 B (0.41%), `UpdateTransitionToBallCamera(float,...)`
  2088 B (0.27%), `UpdateTransitionToBallCamera(CStateManager&)` 772 B (0.73%) - all untouched,
  all still `return false;` stubs. These call the real `CPlayer`/spline/tweak bodies and are a
  multi-item job each; out of budget here.

WALL: fn_801AA3EC 0.0% - our `__sinit_` is 192 bytes against retail's 216 because `CMaterialList`'s
ctor stores each partial `1<<id` to `.bss` and reloads it where retail keeps the accumulation in
r30:r31; six spellings of the filter's declaration, none keeps the value in registers.

WALL: TransitionFromMorphBallState 0.65% / TransitionToMorphBallState 0.62% / UpdateTransition* -
not attempted; measured sizes 860-2088 bytes, each a separate item's budget.

## Blockers the next run should know

- **`.bss` at 0x803DB4A0..0x803DB4B8 is unclaimed.** `config/G2ME01/splits.txt:912` gives
  `CBallCamera.cpp` `.bss 0x803DB488..0x803DB4A0` and nothing claims from 0x803DB4A0 on, though
  `symbols.txt` lists `lbl_803DB4A0` (`size:0x18`) and three more filters after it. Our object
  therefore carries 24 bytes of `.bss` plus 60 of `.sdata` and 8 of `.sdata2` that `splits.txt` does
  not claim (`tools/unit_fit.sh` reports all three as "NOT CLAIMED"). This does **not** block a
  `progress` item - it is what stops a flip, and it is a `splits.txt` claim to add when the rest of
  the unit is written, not now.
- **`__sinit_CBallCameraTransitions_cpp` is an extra symbol** (192 B, `t`) that the retail unit
  object does not define under that name. `unit_fit.sh` lists it. Retail names the same function
  `fn_801AA3EC`; objdiff pairs nothing across that difference, which is why the ctor shows 0.00%.
  Renaming ours to match would need a carve, which needs the `.bss` claim above first.

## Gates

```
sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
./tools/link_check.sh --rebuild               324 undefined, 0 duplicates, unchanged
python3 tools/check_symbol_names.py           0 missing names
python3 tools/check_files_cmake.py            every configured DOL object listed or excluded
./tools/decomp_build.sh                       All: 11854/28465 (was 11852); DOL 10306/16726 (was 10304)
./tools/goal_check.sh build/goal/item.json    PASS - gate ok, counts ok, target rose 0 -> 2, no asm
```

Per-function diff against `build/goal/judge/report.base.json`: **0 worse, 2 better, 0 gone**.

`docs/HANDOFF.md`'s state block was updated by `tools/check_docs_claims.py`'s own sync (matched
11852 -> 11854, DOL units 10304 -> 10306); I did not edit it by hand.

NEW: none. The two walls above are on functions whose success raises the count, but both need either
the `.bss` claim or a multi-item budget, so they belong to the same item rather than a new one.