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
---

# Run 2 (2026-10-02, lane 5)

`UpdateTransitionToBallCamera__11CBallCameraFR13CStateManager` (retail 0x801A8B78, 772 bytes) is
**now at 100.00%** (`build/report.json`). The unit goes **2/8 -> 3/8** matched and stays
`NonMatching`. Nothing anywhere got worse and the diff adds no `asm`. The previous run's
`WALL:` on `fn_801AA3EC` still stands - I did not re-attack it and it is still 0.00%.

## What the function is

Reads nothing from the player's transform; it is a *camera-side* routine that has to be written
from the disassembly, not inferred from the name:

```
mLookAtBall = false;                       // bitfield byte 517, bit 2 (0x205:2) - rlwimi 5,26,26
const CPlayer& player = Player(mgr);
CVector3f dir = player.GetTransform().GetForward();   // DEAD, but its 3 stores are in retail
dir = mLookPos - GetTranslation();                    // 0x250 minus 0x54
const CVector3f pos = GetTranslation();
if (dir.IsMagnitudeSafe()) {                          // the 0.65%-scoring stub skipped this entirely
  dir.Normalize();
  CVector3f up = GetTransform().GetForward();
  up.SetZ(0.f);
  up.Normalize();
  const float absProjection = CMath::AbsF(CMath::Limit(CVector3f::Dot(up, dir), 1.f));
  if (absProjection < 0.99999f) {
    const float progress = 0.f == player.GetMorphDuration()
                             ? 0.f
                             : CMath::Clamp(0.f, player.GetMorphTime() / player.GetMorphDuration(), 1.f);
    const float fraction = 1.5f * progress;
    const float angle = CMath::Limit(fraction, 1.f);
    const CRelAngle step = CRelAngle::FromRadians(angle * acosf(absProjection));
    const CQuaternion rotation = CQuaternion::LookAt(CUnitVector3f(up), CUnitVector3f(dir), step);
    SetTransform(rotation.BuildTransform4f() * CTransform4f::LookAt(pos, pos + up, CVector3f::Up()));
  } else {
    SetTransform(CTransform4f::LookAt(pos, pos + dir, CVector3f::Up()));
  }
}
SetTranslation(pos);
TeleportCamera(pos, mgr);
return false;
```

## The four things that took the whole budget

Every one of these is a *source shape* fact, not a codegen fact. `tools/g2try.sh`'s byte
percentage hid all four, because each one is worth 99.x% -> 100.00%.

1. **`acosf` takes the absolute value, and the same absolute value is what the branch tests.**
   `0.801A8C74` computes `|dot|`, `0.801A8C9C` compares it to `0.99999f`, and `0.801A8D10` calls
   `acos` - on `f1`, which still holds **that absolute value**, not the signed dot. One `const
   float absProjection = CMath::AbsF(CMath::Limit(...))` used by both the test and the call takes
   the function from 97.64% to 99.43%. Written as `Limit(dot,1)` + `AbsF(projection)` for the
   test + `acosf(projection)`, it sticks at 97.64% forever: two spellings, one number.

2. **`0.99999f`, not `0.999999f`.** The `.sdata2` word at 0x8041CCE4 is `3f7fff58` =
   0.99998998...f, which is `0.99999f`. `0.999999f` is `3f7fffef`, one ulp away, and looks
   identical on screen. Measured, not guessed - `CMath::Limit`'s `0.999999f` in
   `CInterpolationCamera.cpp:100` is a *different* constant from this function's.

3. **`1.5f * progress` has to be its own named `const float fraction`.** Retail
   `fmuls f3,f2,f3` puts the product in the *same* register the Clamp result occupied; inline,
   the product lands in `f31` and everything below shifts. Naming the temp reproduces the
   register. Same for the dead first `dir` assignment: retail builds a `CVector3f` from the
   player's transform forward, stores it at 116(r1), and then overwrites the same slot - so the
   declaration *and* the reassignment both have to stay.

4. **The zero-duration test is written `0.f == dur ? 0.f : Clamp(...)`, not `!=`.** This is the
   last 0.57%. Retail branches *around* the ratio computation with `fcmpu cr0,f3,f2 / bne / b`;
   the `!=` spelling emits `beq` straight to the join and drops an instruction. **The ternary's
   arms are not interchangeable in MWCC codegen** - swapping them is the whole difference between
   99.43% and 100.00%, with identical C++ semantics.

Spellings measured, all reaching the same 99.43% ceiling unless noted (each is a real
distinction, none is a guess): `progress` via `if`/`else` vs ternary (29-41 differing lines vs 5
for the winning ternary); `duration` hoisted into a local or re-read at each use (23-46);
`Clamp(0.f, ratio, 1.f)` vs two hand-written compares (18-45); `Limit(progress * 1.5f, 1.f)` vs
`Limit(1.5f * progress, 1.f)` (both stuck); `progress *= 1.5f` in place (41). The winning shape is
the **only** one tried that both names `fraction` and writes the test as `0.f == dur`. ~40
variants swept mechanically; `.tmp/opencode/norm.py` is the differ (it resolves SDA2 float
relocations to their values on the object side so a relocated constant does not read as a
difference) and the per-variant sweeps are `sweep*.py` beside it. **Do not re-sweep these: the
answer is the shape above, verbatim.**

## The link gate, and what it cost

`goal_check.sh` failed its first run on `link-gap`, not on the count: the two new calls
(`CGameCamera::Player`, `CBallCamera::TeleportCamera`) put two undefined names into
`CBallCameraTransitions.o`, which `files.cmake` *does* list, and both bodies live in
`NonMatching` units the port does not (`CGameCamera.cpp:256`, `CBallCamera.cpp:151`). Same trap
the previous run documented for `GetCameraManager`, and the same fix: the bodies go in
`src/MetroidPrime/PortGlobals.cpp` beside the `GetCameraManager` one, each with the reason.

`TeleportCamera` is not free - it is a net *zero*, not a net win:

- it calls `CCameraColliderGroup::TeleportColliders` (retail 0x801F94DC, 0x78 bytes), which no
  unit in this tree owns, and
- it calls `TCastToPtr<CCollisionActor>` (retail 0x8009A498, `li r4,18` = `kET_CollisionActor`),
  whose only would-be definer is `CCollisionActor.cpp` - also excluded.

So the change carries **three** extra bodies, not one: `CGameCamera::Player`,
`CCameraColliderGroup::TeleportColliders` and the `PORT_CAST_TO_PTR(CCollisionActor, ...)` line
(the existing macro and the six `TCastToPtr` cases above it are the precedent, and the type id
was read out of retail's `li r4`, not from the enum). Measured after: **283 MISSING, unchanged,
`all accounted for`**. Adding only `Player` and `TeleportCamera` left 285 with two names the gap
list had never seen - a failure, not a pass, which is why the count alone is not the test.

## Still not done, and the walls

- `fn_801AA3EC` 0.00%, 216 bytes. **Unchanged from run 1 and still walled**; I did not re-attack
  it. The previous run's six spellings and its analysis (our `__sinit_` is 192 bytes because
  `CMaterialList`'s ctor spills each partial `1<<id` to `.bss` where retail keeps the
  accumulation in r30:r31) all still stand. It also cannot be *paired* by objdiff under the name
  `__sinit_CBallCameraTransitions_cpp` - retail's unit object has no such symbol - so reaching
  216 bytes is necessary but not sufficient.
- `TransitionFromMorphBallState` 860 B (0.65%), `UpdateTransitionFromBallCamera` 1360 B (0.41%),
  `TransitionToMorphBallState` 908 B (0.62%), `UpdateTransitionToBallCamera(float, CStateManager&)`
  2088 B (0.27%) - all still `return false;` stubs, untouched. This run's budget went entirely
  into the one function above; the other four are each a separate item's work, and the 2088-byte
  one is the largest single item in the queue.
- **`.bss` 0x803DB4A0..0x803DB4B8 is still unclaimed** and `__sinit_CBallCameraTransitions_cpp`
  is still an extra 192-byte symbol. Unchanged, and still the thing that stops a *flip* rather
  than a match.

## Gates

```
sha1sum build/G2ME01/main.dol           6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
./tools/probe_sources.sh                752 files, 0 failed; LINKED, 288 undefined, 0 duplicates
python3 tools/check_symbol_names.py     0 missing names
python3 tools/check_files_cmake.py      every configured DOL object listed or excluded
tools/link_gap.py --rebuild             283 MISSING, all accounted for (was 283)
./tools/decomp_build.sh                 All: 12349/28465 (was 12348), DOL 10801/16726
./tools/goal_check.sh build/goal/item.json   PASS
```

Per-function diff against `build/goal/judge/report.base.json`: **0 worse, 1 better, 0 gone**, and
the better one is the intended function at 100.00%. `docs/HANDOFF.md` was updated by
`tools/check_docs_claims.py`'s own sync (12348 -> 12349, DOL 10800 -> 10801); I did not edit it.

NEW: none. The one function this item wanted is done; the four remaining are large multi-item
jobs and `fn_801AA3EC` is the same wall run 1 measured.
