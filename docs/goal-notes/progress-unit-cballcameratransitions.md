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

---

# Run 3 (2026-10-02, lane 2)

`TransitionFromMorphBallState` (retail 0x801AA008, 860 bytes) is **now at 100.00%**
(`build/report.json`). The unit goes **3/8 -> 4/8** matched and stays `NonMatching`. Nothing
anywhere got worse and the diff adds no `asm`. `./tools/goal_check.sh build/goal/item.json`
prints `PASS`.

## What the function is

Nothing like the name suggests: it is a *spline builder*, written from the disassembly, not
inferred. Measured out of `tools/dis.sh 0x801AA008 0x35C` and `.sdata2` at `_SDA2_BASE_ =
0x804223C0`:

```cpp
const CTransform4f playerXf = Player(mgr).GetTransform();          // CPlayer/CGameCamera +0x24
const CTransform4f camXf    = <mgr>.CurrentCamera(mgr,false)->GetTransform();  // CCamera +0x24
const CVector3f    lookPos  = <mgr>.CurrentCamera(mgr,false)->GetScanObjectIndicatorPosition(mgr);
mFromBallTransition->mLookPos  = lookPos;     // +0x30
mFromBallTransition->mPlayerXf = playerXf;    // +0x00, via CTransform4f::operator=
const CVector3f eye    = Player(mgr).GetEyePosition();   // returns through a hidden pointer
const CVector3f camPos = camXf.GetTranslation();
const float     dist   = (lookPos - camPos).Magnitude();
const CVector3f endPoint = (0.6f * -dist) * playerXf.GetForward() + eye;   // 0.6f = 0x8041CD00
float distance;
CVector3f point = endPoint;
if (DetectCollision(eye, endPoint, 0.3f, distance, mgr, GetControllerNumber())) {  // 0.3f = 0x8041CD04
  point = -distance * playerXf.GetForward() + eye;
} else {
  distance = dist;
}
rstl::vector<CVector3f> points;  points.reserve(4);
points.push_back_unsafe(camPos); points.push_back_unsafe(point);
points.push_back_unsafe(eye);    points.push_back_unsafe(eye);
mFromBallTransition->mSpline.Initialise(points);
mFromBallTransition->mSpline.SetDuration(0.9999f);   // store straight to +0x74, i.e. mSpline+0x38
mFromBallTransition->mSpline.CalculateLength();
return CheckFailsafeFromMorphBallState(mgr);
```

How the pieces were identified, all measured:

- `lwz r0,476(r3) / slwi r0,r0,2 / add r3,r4,r0 / lwz r3,5404(r3)` is
  `CStateManager::mCameraManagers[mControllerIdx]` - `mControllerIdx` is `CGameCamera`'s field at
  0x1DC (`GetControllerNumber()`) and the manager array is `CStateManager`'s, 0x20 past the
  player array that `CGameCamera::Player` reads (0x14FC vs 0x151C, measured from the two).
- The vtable slot `+0x5C` (index 23) is `CActor::GetScanObjectIndicatorPosition(CStateManager const&)`
  for every camera class - dumped out of `__vt__11CGameCamera` (0x803B66A0) and
  `__vt__11CBallCamera` (0x803B6478) in `.data`. The call passes `&out` in r3, the camera in r4 and
  the manager in r5, i.e. a hidden return pointer for a 12-byte `CVector3f`.
- `CMotionSpline` is `{vtable, vector x3 (16 bytes each), mLength +0x34, mDuration +0x38,
  mClosedLoop bit +0x3C, mType +0x40}`, 0x44 bytes - read out of `__ct__13CMotionSplineFbfQ13...`
  at 0x80334BC4 and confirmed by `ValidateLength` (0x80332BC0) reading 52(r3) = 0x34.
- `mSpline` is at 0x3C inside `SFromBallTransition` (0x30 transform + 0x0C look position), so
  retail's `stfs f0,116(r3)` is the duration. The constant is `0.9999f` (`.sdata2` 0x8041CCEC,
  bits 0x3F7FF3D1).

## The five things that took the whole budget

Every one is a *source shape* fact, and each is worth a few percent on its own. `tools/g2try.sh`
and objdiff's byte percentage hide all five.

1. **`push_back_unsafe`, not `push_back`.** With `reserve(4)` and exactly four pushes, retail's
   inlined push has **no capacity test** at all (`lwz r3,52 / lwz r5,60 / mulli / addi / stw /
   add / stfs x3`) - 55.73% -> 80.01% on this one change. `rstl::vector::push_back_unsafe` in
   `include/rstl/vector.hpp` emits exactly that. The precedent for the container is
   `CScriptSpindleCamera.cpp:80`.
2. **The camera manager has to be spelled through `CStateManager`, not `CGameCamera`.** Retail
   inlines `GetCameraManager` (the 0x1DC/0x151C read above); `CGameCamera::GetCameraManager` is
   declared in the header and defined in `CGameCamera.cpp`, so in this tree the call is a real
   `bl`. `const_cast<CCameraManager*>(mgr.GetCameraManager(GetControllerNumber()))` uses the
   *inline* `CStateManager::GetCameraManager` and reproduces retail's instruction order exactly,
   including the `mr r4,r31 / li r5,0` sitting in the middle of the inlined body. 91.11% ->
   97.72% on its own. Do **not** make `CGameCamera::GetCameraManager` inline: it is a **100% matched
   function** of `main/MetroidPrime/Cameras/CGameCamera` (20 B) and inlining it would delete it.
3. **The sweep vector is `playerXf.GetForward()`, not the camera position.** The tell is the
   offsets: retail loads 140/156/172 for the sweep and 100/116/132 for the delta, and 140/156/172
   is `136 + {4, 20, 36}` = the *second column* of the `CTransform4f` copy of the player's
   transform (`m01, m11, m21`). Reading it as `camXf.GetTranslation()` is a plausible-looking
   wrong answer that sits at 99.81% and is only caught by the offsets. 99.81% -> 100.00%.
4. **`(scale * vector) + eye`, not `eye + (scale * vector)`.** Retail's three adds are
   `fadds f27,f6,f0` - product first, eye second. Semantically identical; the operand order is
   the whole difference between 99.16% and 99.81%.
5. **`endPoint` is const and the corrected point is a *second* variable.** Retail keeps the sweep
   target in f25-f27 across the `DetectCollision` call and does **not** re-store it in the taken
   branch, while the not-taken branch just stores the magnitude. Reassigning one variable makes
   MWCC treat it as a memory object and emit a second store; `const CVector3f endPoint` plus
   `CVector3f point = endPoint;` is what produces retail's shape. 84.78% vs 80.17% for the
   reassignment form (measured with the same everything else).

## Spellings measured this run, do not re-sweep them

`tools/fast_try.sh` for the score, plus a per-instruction differ (see below). All are the same
function; `dist`/`delta`/`camPos` are the only things that move. The frame size is in brackets -
retail's is 304 and a 240/256 frame means the FPR save set is already wrong.

| spelling | score (frame) |
|---|---|
| `eye, delta, dist, endPoint`, `push_back_unsafe` | 80.01% (240) |
| `delta` declared before `eye` | 71.73% (240) |
| `dist` between `delta` and `eye` | 68.31% (240) |
| `delta` via `operator=` | 78.61% (240) |
| `points` declared before the `if` | 74.94% (240) |
| `endPoint` with `endPoint += ...` | 78.20% (240) |
| `const float scale = 0.6f * -dist` as its own temp | 79.95% (240) |
| `if (!DetectCollision(..)) {distance=dist;} else {endPoint=..}` | 79.19% (240) |
| `const bool hit = DetectCollision(..); if (hit)` | 80.17% (240) |
| everything inlined, `distance = 0.f` in the else | 82.43% (288) |
| everything inlined, `distance = <the expression>` recomputed | 77.49% (304) |
| `const endPoint` + `CVector3f point = endPoint` | 84.78% (256) |
| + `const CVector3f camPos` after `dist` | 89.60% (304) |
| + `camPos` before `dist`, `endPoint` reloads the translation | 99.37% (304) |
| + `(scale * xf.GetTranslation()) + eye` | 99.81% (304) |
| + `GetForward()` instead of `GetTranslation()` | **100.00%** (304) |
| same but `camPos` after `dist` | 99.16% (304) |
| same but `eye + (scale * ..)` | 99.53% (304) |
| same but the `if` branch keeps `camPos` | 96.20% (304) |
| same but `dist` inline (no `camPos`) | 92.01% (256) |

**The stack-slot order is not controllable from the source.** For a long stretch of this run the
bump allocator put the `rstl::vector` at `+36` where retail has it at `+48`, and no declaration
order moved it - the order of `{delta, eye, lookPos, vector, endPoint, eyeCopy}` was identical
across nine permutations. What finally fixed it was giving `camPos` its own name (item 5 and the
`camPos` rows above): that is what puts three `CVector3f` slots ahead of the 16-byte one. If a
future function here has a `CVector3f` whose address is never taken, suspect the same thing.

## The link gate, and what it cost

`probe_sources.sh` reported `292 undefined against a baseline of 291 (GREW)` after the first
version of this function: three new names, all opened by calls in `CBallCameraTransitions.o`,
which `files.cmake` *does* list, and none of the three reachable from a port TU:

- `CBallCamera::DetectCollision` - body at `CBallCamera.cpp:343`, still the `return false` TODO,
  and that unit is excluded;
- `CMotionSpline::Initialise` (retail 0x803349D4, 0x174 B) and `CMotionSpline::CalculateLength`
  (retail 0x80332C7C, 0x604 B) - **no definition anywhere in `src/`**, in any unit.

All three are now announced stand-ins in `src/MetroidPrime/PortGlobals.cpp` beside the existing
`GetCameraManager` / `Player` / `TeleportCamera` copies, each with the reason in place, using the
file's existing `ReportedCameraManagerStandIn` helper. `CalculateLength` deliberately leaves
`mLength` at the `0.0f` its constructor writes (the `.sdata2` word at 0x8041ECAC), so the
failsafe's `length / 24.f` is 0 and not a NaN. Measured after: **289 undefined, 0 duplicates,
LINKED** - a net **-2** against the 291 baseline, and `tools/link_gap.py` reports `284 MISSING,
all accounted for`.

`include/Kyoto/Math/CMotionSpline.hpp` gained one line, an inline
`void SetDuration(float) { mDuration = duration; }`. Retail's transition builders overwrite the
duration in place and the field is private, so the port needs a way to spell that; it is inline,
so it adds no symbol to any object.

## Gates

```
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
./tools/probe_sources.sh              752 files, 0 failed; LINKED, 289 undefined, 0 duplicates
python3 tools/check_symbol_names.py   0 missing names
python3 tools/check_files_cmake.py    every configured DOL object listed or excluded
python3 tools/check_decl_order.py     1 unit checked, none out of retail order
tools/link_gap.py                     284 MISSING, all accounted for
./tools/decomp_build.sh               All: 34.94% fuzzy, 28.62% matched (12373 / 28465)
./tools/goal_check.sh build/goal/item.json   PASS
```

`goal_check.sh` line by line: gate ok, `matched 12372 -> 12373`, `linked 5863 -> 5863`,
`target rose: main/MetroidPrime/Cameras/CBallCameraTransitions: 3 -> 4 / 8 functions`, no asm.
`docs/HANDOFF.md` was rewritten by `tools/check_docs_claims.py`'s own sync, not by hand.

## Tooling this run used, and it is worth rebuilding

`tools/g2try.sh` cannot be used on this unit: it disassembles `build/G2ME01/obj/<unit>.o`, which
is a *relocatable* object whose `.text` starts at 0, so `--start-address=0x801AA008` matches
nothing and it reports `retail insns: 0`. A per-instruction differ is what made this tractable;
it was at `.tmp/opencode/nrm.py` (retail function offset = `retail_vaddr - unit .text base`,
`objdump -r` to turn each `lfs fX,N(r2)` into the `.sdata2` float it really loads, branch targets
normalised) with `.tmp/opencode/sweep.py` driving a variants file through `tools/fast_try.sh`.
`.tmp` is not committed, so the next run has to write them again. The one thing worth keeping is
the method: objdiff's percentage is a *byte* match, and 80% vs 100% here was entirely
register-numbering and stack-offset noise that only an instruction-by-instruction diff exposes.

## Still not done

- `TransitionToMorphBallState` 908 B (0.62%), `UpdateTransitionFromBallCamera` 1360 B (0.41%),
  `UpdateTransitionToBallCamera(float, CStateManager&)` 2088 B (0.27%) - all still
  `return false;` stubs, untouched. Each is a separate item's budget; the 2088-byte one is the
  largest single item in the queue. `TransitionToMorphBallState` is the cheapest and is this
  function's mirror image (same camera lookups, same 0.6/0.3 constants, `mToBallTransition`
  instead of `mFromBallTransition`), so it is the one to take next.
- `fn_801AA3EC` 216 B, still 0.00%. **Run 1's `WALL:` on it stands and I did not re-attack it** -
  six spellings, all recorded there. It also cannot be *paired* by objdiff under the name
  `__sinit_CBallCameraTransitions_cpp`, so reaching 216 bytes is necessary but not sufficient.
- `.bss` 0x803DB4A0..0x803DB4B8 is still unclaimed and `__sinit_CBallCameraTransitions_cpp` is
  still an extra 192-byte symbol (`tools/unit_fit.sh`). Unchanged, and still the thing that stops
  a *flip* rather than a match. `reserve` / `~vector` for `rstl::vector<CVector3f>` are COMDAT
  weak copies the retail linker discards, so they are harmless on their own.

NEW: none. The one function this item wanted is done; the three that remain are large multi-item
jobs in the same unit the driver already re-queues, and `fn_801AA3EC` is the wall run 1 measured.
