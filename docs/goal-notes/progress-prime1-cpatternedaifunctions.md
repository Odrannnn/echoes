# progress-prime1-cpatternedaifunctions

Unit `main/MetroidPrime/Enemies/CPatternedAiFunctions`, `kind: progress`, stays `NonMatching`.
Prime 1's `src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp` (read-only clone at
`../prime-ref`, commit 62ef50b) was the reference for every function below; nothing was copied
from its headers and no class layout was changed.

## Result (measured, `build/report.json`)

| | before | after |
|---|---|---|
| `matched_functions` | 16 / 39 | **27 / 39** |
| `fuzzy_match_percent` | 29.91% | **49.56%** |
| `matched_code_percent` | 13.10% | **42.62%** |

Whole-DOL `All:` line: `30.82% fuzzy, 23.09% matched, 10002 / 28465 functions` ->
`30.84% fuzzy, 23.12% matched, 10013 / 28465 functions`.
`tools/report_diff.py <base> build/report.json`: `+11 functions at 100%, 0 units newly linked`,
`no regression`.

## Per function: before% -> after%, and what Prime 1's source did

| function | before | after | Prime 1 source |
|---|---|---|---|
| `FixedDelay` | 99.23 | **100** | small edit - Echoes reaches the state through `mStateMachine`; binding `const TStateMachineState<CPatterned>& state = static_cast<...>(*mStateMachine)` first (the spelling `RandomDelay` already used) makes the pointer load once |
| `GetAnimOver` | absent (0) | **100** | not in Prime 1 - see "the inline-body trick" below |
| `AnimOver` | 21.75 | **100** | not in Prime 1 - same trick |
| `InRange` | 59.03 | **100** | small edit - Prime 1 computes `distance` into a local *before* `range`; that order is what matches |
| `Leash` | 68.21 | **100** | **Prime 1's body unchanged in shape** (`bool result = ...; if (result) { ...; result = result && ...; }`). Our short-circuit `&&` version was 68% |
| `OffLine` | 74.99 | **100** | Prime 1's `if/else` on `Dot(pathLine, curLine) <= 0` - the *inverse* of what we had - plus `const float arg = data.GetFloat();` (retail loads it once) |
| `InDetectionRange` | 41.68 | **100** | small edit - Echoes loops over players, Prime 1 does not. Needed hoisted locals, an unsigned trip-count compare, and an if/else instead of `\|\|` |
| `PathFound` | 4.83 | **100** | Prime 1's `GetSearchPath() && !GetSearchPath()->IsShagged()`, rewritten as a `bool result` form (retail keeps the result in r31) |
| `PathOver` | 3.04 | **100** | Prime 1's, unchanged in shape |
| `PathShagged` | 1.43 | **100** | Prime 1's, unchanged in shape |
| `GetConnectedObject` | 3.22 | **100** | Prime 1's body, adapted: `SConnection` fields are `state/msg/objId` (no `m` prefix) and there is no `AUTO` macro in this repo |
| `Landed` | 86.67 | 89.58 | Prime 1's body, still short - see walls |
| `SpotPlayer` | 98.76 | 98.76 | Prime 1 has no player loop; Echoes' loop already matches. Register allocation only - see walls |

The three `GetSearchPath()` triggers (`PathFound`, `PathOver`, `PathShagged`) are `const` methods
and retail's `GetSearchPath` is non-const (`GetSearchPath__10CPatternedFv` in
`config/G2ME01/symbols.txt`), so they call it through
`const_cast< CPatterned* >(this)`. Making the accessor const would rename the symbol and break
`check_symbol_names.py`; the cast emits the identical vtable call.

## The inline-body trick (`GetAnimOver` + `AnimOver`)

`GetAnimOver` was `{ return mAnimationState.IsOver(); }` **in the class body**, so MWCC inlined it
into its only caller and emitted no out-of-line copy: `AnimOver` was 21.75% and `GetAnimOver` had
no symbol at all (objdiff reported 0.00%). Moving the body to the .cpp (declaration only in the
header, definition between `AnimOver` and `Stuck`, which is where retail's descending-by-offset
declaration order puts it) makes MWCC emit both: **+2 functions at 100% from a 2-line move.**

## Codegen rules measured here (MWCC GC/2.7, this repo's flags)

* **`||` with a float `<=` first operand is compiled inverted.** Standalone test
  (`t2.cpp`): `if (h <= 0.f || z < 4.f) return true;` compiles to `fcmpo; cror eq,lt,eq; beq
  <return true>; fcmpo; bge <return false>` - i.e. it tests `h > 0` and takes the "then" branch,
  so the emitted code is `h > 0 || z < 4`. Same source as `if (h > 0.f && z < 4.f)` minus the
  inversion, and both are *not* what the source says. Retail never uses that shape: it writes the
  positive test and inverts the branches. That is why `InDetectionRange` is
  `if (heightRange > 0.f) { if (dz*dz < sq) return true; } else { return true; }` - that spelling
  reproduces retail's `ble`/`bge` exactly, while `heightRange <= 0.f || ...` gives
  `cror`+`beq` and 84%.
* **Loop trip-count compares.** `for (int i = 0; i < mgr.GetNumPlayers(); ++i)` emits
  `mtctr; cmpwi; ble` and indexes through a separate register; retail's `cmplwi` + `addi r4,r4,4`
  pointer walk needs `i < static_cast<uint>(mgr.GetNumPlayers())`.
* **Which callee-saved register a local gets follows declaration order.** `PathFound` was 85.34%
  only because `CPatterned* self = const_cast<...>(this);` was declared before `bool result`;
  inlining the cast and declaring `result` first gives 100%.
* **Floats:** `x <= y` as a *value* is `fcmpo; cror eq,lt,eq` + test the EQ bit (CR bit 2);
  as a *branch* it is a bare `ble`. `lfs`/`stfs` spill order inside `MagSquared` is not
  x,y,z - retail stores OffLine's `pathLine` as y@8, x@12, z@16, and Leash's delta the same way.
  Reproducing Prime 1's source verbatim reproduced that for free.

## Offsets confirmed from the generated code (not assumed)

* `CPatterned` bitfields all live in the byte at `0x34c`: bit1 `mPrevOnGround`, bit3 `mOnGround`,
  bit5 `mVerticalMovement`, bit7 `mInPosition` (`rlwinm`/`rlwimi` MB fields read straight off
  `Landed`, `PathOver` and `fn_801524fc`). The 2-bit `mPathOverCount` is in the halfword at
  `0x420` (`lhz` + `rlwinm. 25,30,31`).
* `CPathFindSearch`: `rstl::reserved_vector` is an **inline** array - `size` at `+4`, elements at
  `+8` (12 bytes each, `mulli r0,r4,12`), `mCurWaypoint` at `200`, `mResult` at `204`. Reading
  `IsShagged()`/`IsOver()`/`GetPoint()` out of the existing header already produced retail's
  offsets, so no header change was needed.
* `GetTranslation()` is at `84` (`addi r4,r31,84` passed as `const CVector3f&`),
  `mDestPos` at `820`, `mReflectedDestPos` at `832`.

## Three functions written, measured at 100%, then removed: the port link gap

`IsOnScreen`, `NoPathNodes` and `fn_801524fc` each reached **100%** (0 differing non-relocation
instructions; `OffLine`/`fn_801524fc`'s remaining `bl` diffs are relocation fields only) - and
were then reverted, because:

```
$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: NOT LINKED (253 undefined, 0 duplicates)
link_check: STRICT FAIL - regression gate: 253 undefined against a baseline of 250 (GREW)
link_check: 3 symbol(s) this change ADDED to the gap:
  NEW  CGameCamera::ConvertToScreenSpace(CVector3f const&) const
  NEW  CPathFindSearch::OnPath(CVector3f const&) const
  NEW  CPathFindSearch::Search(CVector3f const&, CVector3f const&)
```

`MetroidPrime/Cameras/CGameCamera.cpp` and `MetroidPrime/PathFinding/CPathFindSearch.cpp` are
`NonMatching` and are not in the port build, so calling into them grows the port's undefined-symbol
gap past its baseline, which fails `gate.sh`'s "port probe" step and with it the whole item.
`GetSplinePoint`, `GetIdForScript`, `GetObjectById` and `CRandom16::Next` are *already* in that
baseline gap, which is why `PathShagged` and `GetConnectedObject` are safe to keep. With those
three reverted the probe is back to `link: LINKED (250 undefined, 0 duplicates)`.

**The sources are ready to paste back** (all three verified 100% against retail):

```cpp
bool CPatterned::IsOnScreen(const CStateManager& mgr) const {
  // needs #include "MetroidPrime/CCameraManager.hpp" and
  // #include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
  const CVector3f center = GetBoundingBox().GetCenterPoint();
  const CFirstPersonCamera* camera =
      const_cast< CCameraManager* >(mgr.GetCameraManager(0))->FirstPersonCamera();
  const CVector3f screen = camera->ConvertToScreenSpace(center);
  return screen.GetZ() > 0.f && screen.GetX() * screen.GetX() < 1.f &&
         screen.GetY() * screen.GetY() < 1.f;
}

bool CPatterned::NoPathNodes(CStateManager&, const CTriggerData&) const {
  CPatterned* self = const_cast< CPatterned* >(this);
  if (self->GetSearchPath()) {
    return self->GetSearchPath()->OnPath(GetTranslation()) != CPathFindSearch::kR_Success;
  }
  return true;
}

void CPatterned::fn_801524fc(CStateManager& mgr) {
  if (GetSearchPath()->Search(GetTranslation(), mDestPos) == CPathFindSearch::kR_Success) {
    mReflectedDestPos = GetTranslation();
    SetDestPos(GetSearchPath()->GetPoint());
    mInPosition = false;
    ApproachDest(mgr);
  }
}
```

## Walls (spelling -> measured score; do not repeat)

`Landed` (48 B, 2 artifacts left: an extra `lbz r0,844(r3)` re-load before the second
`rlwinm.`, and `r4`/`r5` swapped for the result/`mOnGround`):

* `const bool landed = mOnGround && !mPrevOnGround; mPrevOnGround = mOnGround;` -> 86.67
* `+ const bool onGround = mOnGround;` first -> 86.67
* `const int landed = ...` -> 42.92 (adds `neg/or/srwi`)
* nested `if (onGround) { if (!mPrevOnGround) result = true; }` -> **89.58 (kept)**
* nested + `const bool wasOnGround = mPrevOnGround;` hoisted -> 75.00
* `const bool wasOnGround = mPrevOnGround; mPrevOnGround = mOnGround; return mOnGround && !wasOnGround;` -> 20.42

`SpotPlayer` (372 B, 98.76%; the two arithmetic chains are isomorphic, only the float registers
differ - retail keeps the dot in f5 and the angle in f0, we keep both in f0/f2):

* current `&&` form -> 98.76
* nested `if (forwardDistance > 0.f) { const float distance = ...; if (...) return true; }` -> 98.49 (worse)

## What is still unmatched, and what each one needs

* `Landed` 89.58%, `SpotPlayer` 98.76% - register allocation only, see the walls above.
* `Dead` (260 B) - needs `CBodyStateCmd` built in the caller's frame, `mBodyController`,
  `RemoveMaterial`/`AddMaterial`, and **an unidentified virtual**: `mAlphaDelta = c / <virtual at
  vtable slot 77>()` (`fdivs f0,f0,f1` after `lwz r12,308(r12)`). Not derivable from Prime 1.
* `PlayerSpot` (372 B) - Echoes-specific, not Prime 1's. Needs a `CPlayer` field at `+908`
  (morph state), `CGameCollision::RayStaticLineOfSightTest(mgr, pos, dir, len, filter)` (exists in
  `include/MetroidPrime/CGameCollision.hpp`), and the `CMaterialFilter` built as
  `__shl2i(0,1)` with words `[0,1,0,0,1]` - the filter's 5-word layout and the `operator<<` that
  produces it were not identified. It also needs `ConvertToScreenSpace`, so it is blocked by the
  same link gap as `IsOnScreen`.
* `PathFind` (580 B) and `ApproachDest` (1020 B) - Prime 1's versions are close but both need
  `CBCLocomotionCmd`/`CBCStepCmd` and `mBodyController->CommandMgr()`, and `ApproachDest`'s
  `knockbackWhenFrozen` branch is Echoes-specific.
* `RotateToPoint` (348 B) and `ApplyScreenShake` (228 B) - Echoes-only, no Prime 1 reference.
* `fn_80151920` (8 B, `lfs f1,44(r3); blr`) - an **unnamed** retail function inside this unit's
  range with no class mangling, so it cannot be a `CPatterned` member. Matching it would mean
  inventing an `extern "C"` symbol, which is metric-gaming, not decompilation. Left alone.

## Caveat on `PathShagged`'s 100%

`4.f * skActorApproachDistance * skActorApproachDistance` is one `lfs` in both objects, but retail
reloads the *folded* 3.0 out of the SDA constant pool
(`R_PPC_EMB_SDA21` against `0x8041...`) while ours reloads the symbol
`skActorApproachDistance__10CPatterned` (`R_PPC_EMB_SDA21` against the symbol) - the field is a
relocation in both, so objdiff scores 100% while the linked bytes would differ. The fix is an
in-class initializer on `skActorApproachDistance` (it is defined out-of-line in
`src/MetroidPrime/Enemies/CPatterned.cpp:12` and nothing else uses it). I did **not** do it: it
risks dropping the symbol out of the `CPatterned` unit, which would read as a regression. Whoever
flips this unit needs it.

## Gates (all run in this worktree)

```
sha1sum build/G2ME01/main.dol      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh           749 files, 0 failed; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  checked 503 units; 0 declared names are missing
python3 tools/check_decl_order.py    ok: 957 unit(s) checked, 31 permuted, all accounted for
python3 tools/report_diff.py <base> build/report.json   +11 at 100%, no regression
./tools/gate.sh <base>            GATE FAIL: docs
```

The only failing gate step is `docs claims`, and only on the two derived counts
(`matched 10013 / 28465 functions`, `DOL units 8602 / 16726 functions`) that
`check_docs_claims.py --write` regenerates - which is exactly what `goal_check.sh` runs
(`MP_GATE_DOCS_WRITE=1`). Every other step is `ok`. `flip_test.sh` was not run: this is a
`progress` item and the unit stays `NonMatching`.

Declared in descending retail offset throughout (`AnimOver` 2492, `GetAnimOver` 2472, `Stuck` 2448
...), so the object's `.text` order is not permuted.

## NEW

NEW: port-cpathfindsearch-onpath | port | CPathFindSearch::OnPath | CPatternedAiFunctions' NoPathNodes/fn_801524fc/IsOnScreen are written and measure 100% against retail, but MetroidPrime/PathFinding/CPathFindSearch.cpp and MetroidPrime/Cameras/CGameCamera.cpp are NonMatching and not in the port build, so calling OnPath, Search or ConvertToScreenSpace pushes the port's undefined-symbol gap from 250 to 253 and fails gate.sh's port-probe regression; getting those three symbols into the port link unblocks 3 functions here plus PlayerSpot.

---

# Run 2 (lane L7, 2026-09-30) — re-measured on this tree

Everything above is the previous run's report. I re-measured first: the clean tree already had
**27 / 39** matched and 49.557% fuzzy, exactly as recorded, so this is a continuation and not a
`STALE`. **+1 function matched: `RotateToPoint`, 1.15% -> 100%.** Unit is now **28 / 39**,
**54.9139% fuzzy**, **47.970% matched code**. Whole-DOL `All:` line
`31.49% fuzzy, 23.92% matched, 10360 / 28465 functions` (was `... 23.91% ... 10359`).

## What changed (`src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp` only)

| function | before | after | change |
|---|---|---|---|
| `RotateToPoint` | 1.15 | **100** | written (was a TODO stub); +2 includes |
| `SpotPlayer` | 98.76 | **99.68** | unsigned trip count + operand order of the angle test |
| `Landed` | 89.58 | **91.67** | swap two local declarations |

## `RotateToPoint` — written from the disassembly, no Prime 1 reference

Prime 1 has no `RotateToPoint`, so this came from `801510CC..80151224` directly. Offsets read
off the generated code (`lwz r3,0x48c(r30)` / `lfs f0,0x5c8(r3)`), then **confirmed** by a
throwaway `#define private public` probe compiled with this unit's exact `mwcceppc` line:
`offsetof(CPatterned, mBodyController) == 0x48c` and `offsetof(CBodyController, mTimeScale) ==
0x5c8`. So the guard is the body controller's **time scale**, not a turn speed (`mTurnSpeed` is
at `0x590`, not `0x5c8` — that was the tempting wrong answer).

```cpp
void CPatterned::RotateToPoint(const CVector3f& position, float dt, float turnSpeed) {
  if (dt <= 0.f || mBodyController->GetTimeScale() == 0.f) return;
  CVector3f dir = GetTransform().GetForward();   dir.SetZ(0.f);
  if (!dir.CanBeNormalized()) return;
  dir.Normalize();
  CVector3f to = position - GetTranslation();    to.SetZ(0.f);
  if (!to.CanBeNormalized()) return;
  to.Normalize();
  const CRelAngle max = CRelAngle::FromRadians(dt * turnSpeed * mBodyController->GetTimeScale());
  const CQuaternion rotation = CQuaternion::ShortestRotationArcClamped(dir, to, max);
  RotateInOneFrameOR(rotation, dt);
}
```

Four things had to line up, and each is a reusable MWCC rule:

* **`CRelAngle` has a private ctor and no default ctor** - `CRelAngle angle;` is a compile error
  (`function call 'CRelAngle()' does not match`). The only public way to make one is
  `CRelAngle::FromRadians(float)`, which is what retail's `stfs f0,0x8(r1)` + `addi r6,r1,0x8`
  is: a by-reference out-parameter built from the float.
* **`GetTransform().GetForward()` is `(m01, m11, m21)` at `0x28 / 0x38 / 0x48`.** `mTransform` is
  at `0x24` and `CTransform4f` stores 4 floats per row, so the "forward" components are 16 bytes
  apart, not 12. `GetTranslation()` is `mPosition` at `0x54`. (I got this wrong on paper first:
  `m11` is the *sixth* float of the struct, so `+0x14`, not `+0x10`.)
* **The two `SetZ(0.f)` calls are not free.** Retail emits `stfs f3,0x40(r1)` then immediately
  `stfs f1,0x40(r1)` over it - a dead store kept because the source assigns after the copy.
  Writing `CVector3f(x, y, 0.f)` instead drops the dead store and loses the match.
* **The result must be bound to a named `const CQuaternion`.** Passing the call straight into
  `RotateInOneFrameOR` reuses the callee's return slot at `0xc(r1)` and gives 90.33%; the named
  local makes MWCC copy it to `0x1c(r1)` first, which is retail, at **100%**.

**Callee check before writing it** (all three are already in `files.cmake`, so the port link does
not grow): `CVector3f::Normalize`, `CVector3f::CanBeNormalized`,
`CQuaternion::ShortestRotationArcClamped` and `CPhysicsActor::RotateInOneFrameOR` are DEFINED in
the port objects; `mBodyController->GetTimeScale()` is an inline header read, no symbol.

## `SpotPlayer` 98.76 -> 99.68 (the rest is float-register allocation)

Two independent edits, both worth recording because the first one is general:

* **Loop trip count must be unsigned.** `for (int i = 0; i < mgr.GetNumPlayers(); ++i)` emits
  `cmpw r30,r0`; retail emits `cmplw r30,r0`. `i < static_cast<uint>(mgr.GetNumPlayers())`
  reproduces it (98.76 -> 99.41). `for (uint i = ...)` is worse (96.75%) - the counter must stay
  `int` and only the bound is cast.
* **The angle test's operand order is load-bearing.** `delta.MagSquared() * mDetectionAngle <
  forwardDistance * forwardDistance` and the algebraically identical
  `forwardDistance * forwardDistance > delta.MagSquared() * mDetectionAngle` compile to
  *different register assignments* (99.41 vs 99.68), because the multiply chain's operand order
  decides which of `f0`/`f1` holds the accumulator. Retail's shape is the second one.

What is left is four `fmuls`/`fmadds` whose registers are swapped (`f0`<->`f1`) plus the two
`lfs ...@706@sda21` relocations. `>=`/`<` variants, swapping `Dot`'s arguments (99.35), hoisting
`MagSquared` into a local (92.6), hoisting `mDetectionAngle` (91.4), a nested `if` instead of
`&&` (99.14) and spelling the dot product out by hand (92.6) are all **worse**. See the wall line.

## `Landed` 89.58 -> 91.67

Only change: swap the two declarations so `bool result = false;` comes before
`const bool onGround = mOnGround;`. That swaps `r4`/`r5` onto the same registers retail uses and
is worth 2 points. Retail still does **not** re-load the byte for the second `extrwi`, ours does,
which is the remaining instruction.

## Codegen facts measured this run (MWCC GC/2.7, this repo's flags)

* **Which register a local gets follows declaration order, bit for bit.** Same rule the previous
  run recorded for `PathFound`; here it decides `Landed`.
* **`CMath`/comparison operand order decides float-register assignment.** Not a wash: the two
  spellings of one inequality differ by a register swap in the `fmadds` chain.
* **`> 0.f` on a float is `fcmpo cr0,fX,f0; ble <skip>`** - the same LE test the previous run
  found for `x <= y`, so `> 0.f` and `<= 0.f` produce identical code and only the *branch sense*
  differs.
* A throwaway probe compiled with `-O0` and a `#define private public` header gives exact
  `offsetof` values in a second, which is the cheapest way to settle "which member is at 0x5c8".

## What is still unmatched (measured, `build/report.json`)

`Dead` 1.54 (260 B) | `PathFind` 0.69 (580 B) | `fn_801524fc` 1.85 (216 B) | `IsOnScreen` 3.33
(168 B) | `PlayerSpot` 1.51 (372 B) | `NoPathNodes` 5.00 (112 B) | `ApproachDest` 0.39 (1020 B) |
`ApplyScreenShake` 1.75 (228 B) | `fn_80151920` (8 B, unnamed retail symbol - not touchable).

**Every one of those is now blocked by the port link gap, and I measured that rather than
assuming it.** I read the undefined/defined symbol sets straight out of the built port objects
(`build-port-link/CMakeFiles/mp_{game,platform,port_entry}.dir/**/*.o`, 755 objects) and the
baseline list. Needed and MISSING from the port link:

| caller | missing callee |
|---|---|
| `Dead` | `CBodyStateInfo::GetCurrentState() const`, `CActor::RemoveMaterial(...)`, `CActor::AddMaterial(...)` |
| `ApproachDest` | `CBodyController::HasBodyState(...) const`, `CBodyStateInfo::GetMaxSpeed() const` |
| `ApplyScreenShake` | `CEntity::FindConnectedObject(...)`, `CCameraShakerData` copy ctor + dtor, `TCastToPtr<CScriptCameraShaker>`, `fn_801E7EC0` |
| `IsOnScreen`, `NoPathNodes`, `fn_801524fc`, `PlayerSpot` | the previous run's three (`CGameCamera::ConvertToScreenSpace`, `CPathFindSearch::OnPath`, `CPathFindSearch::Search`) |

`CBodyStateCmdMgr::DeliverCmd` (both overloads) and `CStateManager::ObjectById` **are** already
defined in the port, so `Dead`/`ApproachDest` are only one or two missing symbols away each.
That is a bigger, separate job than this item and it is not a decompilation problem.

`RotateToPoint` is the counter-example and the reason to look before writing: it calls three
out-of-line functions and the link is fine, because all three live in units already in
`files.cmake`. The gate is not "does this add a call", it is "is the callee already in the link".

## Gates (all run in this worktree, all green)

```
./tools/probe_sources.sh        752 files, 0 failed; link: LINKED (250 undefined, 0 duplicates)
./tools/decomp_build.sh         All: 31.49% fuzzy, 23.92% matched, 11.83% linked (10360 / 28465)
                                 CPatternedAiFunctions: 54.91% fuzzy, 47.97% matched (28 / 39)
python3 tools/check_symbol_names.py   ok
python3 tools/check_decl_order.py --unit main/MetroidPrime/Enemies/CPatternedAiFunctions
                                 ok: none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json
                                 PASS progress-prime1-cpatternedaifunctions
                                 ok  gate.sh  /  counts: matched 10359 -> 10360, linked 5048 -> 5048
                                 ok  check_symbol_names.py  /  All: line  /  target rose: 27 -> 28 / 39
                                 ok  no asm added
```

`flip_test.sh` was not run: this is a `progress` item and the unit stays `NonMatching`.
`docs/HANDOFF.md`'s two derived counts are the only file I touched besides the source, and they
were rewritten by `goal_check.sh`'s `MP_GATE_DOCS_WRITE=1`, not by hand.

## Walls (spellings tried **this run**, measured; do not repeat)

`Landed` (48 B, 91.67% best; one artifact left - we re-load `lbz r0,0x34c(r3)` before the second
`extrwi. r0,r0,1,30`, retail does not):

* `bool result = false;` **before** `const bool onGround = mOnGround;` -> **91.67 (kept)**
* the other order -> 89.58; `bool onGround` (non-const) -> 89.58; no local at all -> 89.58
* `const bool landed = mOnGround && !mPrevOnGround;` -> 86.67 (adds `clrlwi r3,r4,24`)
* `result = !mPrevOnGround` inside the `if` -> 55.0 / 56.7
* hoisting `const bool prevOnGround = mPrevOnGround` (either order) -> 75.0 / 75.4
* `result = onGround ? !mPrevOnGround : false` -> 51.2; `if (!onGround) ... else if` -> 74.2
* `if (!onGround) { mPrevOnGround = onGround; return result; } ...` -> 49.2
* store through `const_cast<CPatterned*>(this)->mPrevOnGround` -> 91.67 (no change)
* `IsOnGround()` accessor instead of the member -> 0.00 (the accessor does not exist in the shape
  needed; do not bother)

`SpotPlayer` (372 B, 99.68% best; four `fmuls`/`fmadds` with `f0`<->`f1` swapped, plus the two
SDA21 relocations):

* `i < static_cast<uint>(GetNumPlayers())` **and** `fwd*fwd > magsq*angle` -> **99.68 (kept)**
* unsigned bound only, original operand order -> 99.41
* `for (uint i = ...)` -> 96.75; `Dot(forward, delta)` -> 99.35
* nested `if` with a hoisted `distance` -> 99.14
* `mDetectionAngle * delta.MagSquared()` -> 99.68 but with a different (worse) register map
* `const float magSquared = delta.MagSquared();` -> 92.6 (both orders); hoisting
  `forwardDistance * forwardDistance` -> 99.68, same register map as the kept one
* hoisting `mDetectionAngle` out of the loop -> 91.4; a named `visible` bool -> 95.2
* spelling `MagSquared` out as `x*x + y*y + z*z` -> 92.6

WALL: Landed 91.67% - MWCC rematerialises the 0x34c byte load before the `mPrevOnGround` extract; 15 declaration/branch shapes tried, only the register swap moves.
WALL: SpotPlayer 99.68% - the two float chains are isomorphic and only `f0`/`f1` differ; 12 operand-order and hoisting spellings tried, none reaches 100%.

## NEW

NEW: port-cbodycontroller-hassbodystate | port | CBodyController::HasBodyState | ApproachDest (1020 B, 0.39%) and Dead (260 B, 1.54%) are otherwise understood but call CBodyController::HasBodyState, CBodyStateInfo::GetMaxSpeed, CBodyStateInfo::GetCurrentState, CActor::RemoveMaterial and CActor::AddMaterial, none of which the port link defines, so the port's undefined count would grow past the 250 baseline and fail gate.sh.

---

# Run 3 (lane L1, 2026-09-30) — re-measured, **+4 functions: 28 -> 32 / 39**

Re-measured first: the clean tree was at **28 / 39**, fuzzy 54.9139, matched code 47.970, whole-DOL
`32.4154% fuzzy, 11268 / 28465` — exactly what run 2 recorded, so this is a continuation and **not**
a `STALE`. Now **32 / 39**, fuzzy **66.2423**, matched code **59.594**, whole-DOL `32.4267% fuzzy,
11272 / 28465`.

`tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
`matched 11268 -> 11272, linked 5507 -> 5507, +4 functions at 100%, 0 units newly linked, no regression`.

## What changed

| file | what |
|---|---|
| `src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp` | `Dead`, `fn_801524fc`, `NoPathNodes`, `IsOnScreen` written; two includes added |
| `docs/research/port_link_gap_list.md` | `tools/link_gap.py --write-list` — 4 new entries, see "the port link gap" below |
| `docs/research/port_link_gap.md` | the per-group count table 160 -> 164, plus a dated section for the four |
| `docs/HANDOFF.md` | **not written by me** — `goal_check.sh`'s `MP_GATE_DOCS_WRITE=1` rewrote the two derived counts |

## Per function: before% -> after%, and what Prime 1's source did

| function | before | after | Prime 1 source |
|---|---|---|---|
| `Dead` | 1.54 | **100** | small edit — Echoes dropped the `kStateMsg_Activate` arm and the `mFadeToDeath` test is on a *different* material set (see below) |
| `fn_801524fc` | 1.85 | **100** | Prime 1's `PathFind`, `kStateMsg_Activate` arm, **unchanged** |
| `NoPathNodes` | 5.00 | **100** | unchanged from run 1's verified body |
| `IsOnScreen` | 3.33 | **100** | unchanged from run 1's verified body |
| `Landed` | 91.67 | 91.67 | wall, 6 more spellings this run (below) |
| `SpotPlayer` | 99.68 | 99.68 | wall, 7 more spellings this run (below) |

Everything else is unchanged, so nothing got worse.

### `Dead` — the three edits that mattered, all measured

Prime 1's body is the right shape; three things differ and each one was worth points:

```cpp
void CPatterned::Dead(CStateManager& mgr, EStateMsg msg, float) {
  switch (msg) {                                  // (1) not `if`
  case kStateMsg_Update:
    mBodyController->CommandMgr().DeliverCmd(CBodyStateCmd(kBSC_Die));
    if (!mFadeToDeath) {                          // (2) not mPendingMassiveDeath
      const CBodyStateInfo& bodyState = mBodyController->GetBodyStateInfo();
      if (bodyState.GetCurrentState()->IsDead()) {
        mFadeToDeath = true;
        mAlphaDelta = -1.f / GetFadeOnDeathTime();   // (3) a virtual, not -1.f / 3.f
        RemoveMaterial(kMT_Character, kMT_Unknown59, kMT_Target, kMT_Orbit, mgr);
        AddMaterial(kMT_NoPlatformCollision, mgr);
      }
    }
    break;
  default:
    break;
  }
}
```

1. **`switch` gives retail's branch, `if` does not** — 98.23 -> 99.92 on its own. Retail is
   `cmpwi r5,1; beq <body>; b <end>`. With `if (msg == kStateMsg_Update)` MWCC emits the
   equivalent `bne <end>` and falls through, which is one instruction different. `switch (msg)` with
   one real case and a `default:` reproduces the `beq` + `b` pair exactly. Echoes has no
   `kStateMsg_Activate` arm (Prime 1 sets `mFaceVec = Zero()` there and retail does not), so the
   switch has a single case.
2. **The guard is `mFadeToDeath`, not `mPendingMassiveDeath`** — 99.41 -> 99.92. This is the
   bitfield-decoding rule below, and the trap is that the header order and the emitted shift
   disagree unless you decode the shift correctly.
3. **`mAlphaDelta = -1.f / GetFadeOnDeathTime()`** — `GetFadeOnDeathTime()` is `virtual` in this
   header (line 137) and retail reaches it through `lwz r12,0(r30); lwz r12,308(r12)` (vtable slot
   77) then `fdivs f0,f0,f1` against the SDA constant `0x8041C304`, which is `bf800000` = `-1.f`.
   Prime 1 writes the folded `-1.f / 3.f`. No symbol: a virtual call.

`RemoveMaterial`'s second argument is **59** here, not Prime 1's `kMT_Solid` (= 19 in Prime 1, and
19 is `kMT_Solid` in *this* repo too), and `AddMaterial`'s is **20**, not Prime 1's
`kMT_ProjectilePassthrough` (= 18). Echoes' `EMaterialTypes` was renumbered; I used the existing
enumerators at the measured values (`kMT_Unknown59`, `kMT_NoPlatformCollision`) rather than
renaming the enum in a shared header.

`CBodyStateInfo::GetCurrentState() const` must be reached through the **const** accessor, otherwise
overload resolution picks `GetCurrentState__14CBodyStateInfoFv` and retail's symbol is not the one
called — hence the named `const CBodyStateInfo& bodyState`.

### Codegen facts measured this run (MWCC GC/2.7, this repo's flags)

* **`rlwinm`/`rlwimi` bitfield decode: result bit `n` = source bit `(n + SH) & 31`**, where the
  mask is `MB == ME == n`. This is the reading that makes retail's code correct, and it is *not*
  the reading you get by assuming a left rotate of the source. Confirmed four ways against
  functions that match:
  * `Landed`: `mOnGround` (byte bit 4 of `0x34c`) -> `rlwinm. r5,r0,29,31,31` gives bit (31+29)&31 = 28 = byte bit 4; `!mPrevOnGround` (byte bit 6) -> `rlwinm. r0,r0,31,31,31` gives (31+31)&31 = 30 = byte bit 6; `mPrevOnDeath = onGround` -> `rlwimi r0,r5,1,30,30` puts r5's bit 31 into bit 30.
  * `Dead`: `!mFadeToDeath` (byte bit 3 of `0x420`) -> `rlwinm. r0,r0,28,31,31`; `mFadeToDeath = true` -> `rlwimi r0,r3,4,27,27`.
  Getting this wrong is what made the first `Dead` attempt stop at 99.41 with a shift of 29.
* **A bitfield's physical position is *not* `its index in the declaration list` + the word's base
  bit, once you decode correctly** — but it *is*, if you decode the shift. `mFadeToDeath` is the
  4th bit of `0x420` (index 3) and its test is shift 28; `mPendingMassiveDeath` is the 5th and its
  test is shift 29. The header is right; my arithmetic was not.
* **`switch`/`if` is a branch-direction decision, not a style choice** — see (1) above. Any retail
  `cmpwi; beq <body>; b <end>` needs a `switch` in this MWCC, and any retail `cmpwi; bne <end>`
  (fall through into the body) is an `if`.
* **`Dead` also emitted three weak extras** into the unit's object: `IsDead__10CBodyStateCFv`,
  `__dt__13CBodyStateCmdFv`, `__vt__13CBodyStateCmd`. All `W`/`V`, so the DOL link takes the strong
  definitions elsewhere; no duplicate, and the gates confirm it.

## The port link gap: what run 1 and run 2 concluded is now different, and it matters more than the count

Run 1 and run 2 both recorded the blocker as *"these functions call callees the port link does not
define, so the undefined count grows past the 250 baseline and `gate.sh`'s port-probe step fails"*.
**I re-measured that and it is only half true now.** Two things moved:

1. **Several callees run 2 listed as missing are now defined in the port.** `CActor::RemoveMaterial`
   and all five `CActor::AddMaterial` overloads (`CActor.cpp` *is* in `files.cmake`), plus
   `CEntity::FindConnectedObject`, `CCameraShakerData`'s copy ctor, `CBCStepCmd`, and
   `CBodyStateCmdMgr::DeliverCmd`'s `CBodyStateCmd` overload. That is why `Dead` was writable at all.
2. **The gate that actually fires is the *set* gate, not the count.** The measured MISSING count is
   244, the `link_check` baseline is 250, so the count has 6 of headroom and
   `grew = len(undef) > base_undef` would not fire. The step that failed was
   `tools/gate.sh`'s `port link gap` — `link_gap.py` fails on **any** missing symbol absent from
   `docs/research/port_link_gap_list.md`, at any total:

   ```
   link gap not accounted for:
     gap grew: _ZN15CPathFindSearch6SearchERK9CVector3fS2_ is not in port_link_gap_list.md
     gap grew: _ZNK11CGameCamera20ConvertToScreenSpaceERK9CVector3f is not in port_link_gap_list.md
     gap grew: _ZNK14CBodyStateInfo15GetCurrentStateEv is not in port_link_gap_list.md
     gap grew: _ZNK15CPathFindSearch6OnPathERK9CVector3f is not in port_link_gap_list.md
   ```

   `gate.sh` names the fix itself ("Run `--write-list` and describe what provides each new symbol in
   `port_link_gap.md`"), and `docs/research/port_link_gap_list.md` is not judge-owned — run 1 could
   have done this and did not, which is why it measured a wall that was not there. **`Dead` needs
   this too**, so there is no version of this item that makes progress without one gap-list entry.

So: `python3 tools/link_gap.py --write-list` (diff is exactly the 4 additions), the per-group count
table in `port_link_gap.md` 160 -> 164, and a dated section naming what will provide each.
**244 -> 244 MISSING** per `link_gap.py`; `probe_sources.sh` reports `LINKED (248 undefined,
0 duplicates)` — that is `link_check.sh`'s total, which also counts the libc/runtime entries.

### What is still blocked, measured against the *list*, not the count

| function | missing callees | would reach? |
|---|---|---|
| `ApproachDest` 1020 B | `CBodyController::HasBodyState`, `CBodyStateInfo::GetMaxSpeed`, **`CVector3f::Magnitude`** | **3** more gap entries. `Magnitude` is out-of-line in `CVector3f.hpp` and not in the port; run 2 missed it. 1020 bytes, 6 command constructions |
| `PathFind` 580 B | `CPathFindSearch::GetSplinePointWithLookahead`, `CPathFindSearch::SegmentOver`, `CPatterned::SetDestPos` | **3** more |
| `ApplyScreenShake` 228 B | `CCameraShakerData::~CCameraShakerData`, `TCastToPtr<CScriptCameraShaker>`, `fn_801E7EC0` | **3** more |
| `PlayerSpot` 372 B | `CVector3f::Magnitude`, `CGameCollision::RayStaticLineOfSightTest` (+`__shl2i`) | **2-3** more; also `IsOnScreen`'s `ConvertToScreenSpace` is already listed now |
| `fn_80151920` 8 B | — | genuinely unnamed in `config/G2ME01/symbols.txt`; matching it means inventing an `extern "C"` symbol. Left alone, as run 1 did |

I stopped rather than spending the headroom: every one of these is 2-3 gap entries, so buying one
function costs three recorded gaps, and `ApproachDest` at 1020 bytes would have to reach 100% for the
spend to be worth anything. **The gap list, not the count, is the budget.**

## Gates (all run in this worktree, all green)

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                  All: 32.43% fuzzy, 25.07% matched, 11.94% linked (11272 / 28465)
                                         CPatternedAiFunctions: 66.24% fuzzy, 59.59% matched (32 / 39)
./tools/probe_sources.sh                 751 files, 0 failed, 0 errors; link: LINKED (248 undefined, 0 duplicates)
python3 tools/check_symbol_names.py      checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit main/MetroidPrime/Enemies/CPatternedAiFunctions
                                         ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                         +5 functions at 100%, 0 units newly linked, no regression
./tools/goal_check.sh build/goal/item.json
                                         PASS progress-prime1-cpatternedaifunctions
```

`flip_test.sh` was not run: this is a `progress` item and the unit stays `NonMatching`. No function
was declared, so the decl order is unchanged and still descending by retail offset.

## Walls (spellings tried **this run**, measured; do not repeat)

`Landed` (48 B, 91.67% best; one artifact left - we re-load `lbz r0,0x34c(r3)` before the second
`rlwinm.`, retail reuses the value from the first load):

* `bool result = false;` before `const bool onGround` -> **91.67 (kept, unchanged from run 2)**
* `mPrevOnGround = mOnGround;` (member instead of the local) -> 89.58
* `if (onGround && !mPrevOnGround) { result = true; }` -> 91.67 (same score, different shape)
* `if (!onGround) { result = false; } else if (!mPrevOnGround) { result = true; }` -> 74.17
* `int result = 0;` ... `return result != 0;` -> 67.08
* `if (!onGround) { result = false; } else { result = !mPrevOnGround; }` -> 50.75
* `this->mPrevOnGround = onGround;` -> 91.67 (no change)

`SpotPlayer` (372 B, 99.68% best; the `MagSquared` chain's accumulator is `f0` where retail's is
`f1`, and the two `lfs ...@706@sda21` relocations):

* `const float angle = mDetectionAngle;` first in the loop -> 91.38; after `forwardDistance` -> 96.77
* hoisted **out** of the loop -> 91.43
* `delta.MagSquared() * mDetectionAngle < forwardDistance * forwardDistance` -> 99.41
* `const float fwdSq = forwardDistance * forwardDistance;` -> 96.94
* nested `if` with a local `angle` -> 96.77
* `mDetectionAngle * delta.MagSquared()` -> 99.68 but with a different (worse) register map

WALL: Landed 91.67% - MWCC rematerialises the `0x34c` byte load before the `mPrevOnGround` extract; 21 declaration/branch/assignment shapes tried across three runs, only the register swap moves.
WALL: SpotPlayer 99.68% - the `MagSquared` accumulator is `f0` where retail's is `f1`; 19 operand-order, hoisting and nesting spellings tried across two runs, none reaches 100%.

## NEW

NEW: gap-list-is-the-budget | tooling | docs/research/port_link_gap_list.md | tools/gate.sh's "port link gap" step fails on any MISSING symbol absent from port_link_gap_list.md at any total, so the 250-count headroom is not the real budget: writing any matching function that calls a non-port callee needs `tools/link_gap.py --write-list` plus a note in port_link_gap.md, and run 1 of this item measured a wall (port-cpathfindsearch-onpath) that a 4-line list update would have removed.
NEW: port-linkgap-list-missing-cvector3f-magnitude | port | CVector3f::Magnitude | ApproachDest (1020 B), PlayerSpot (372 B) and any other CVector3f::Magnitude caller cannot be written while the port link does not define it: it is declared out-of-line in include/Kyoto/Math/CVector3f.hpp and MetroidPrime/CVector3f.cpp is not in files.cmake, so each attempt costs 2-3 new port_link_gap_list.md entries.

## Caveats

* `docs/HANDOFF.md` was rewritten by `goal_check.sh` itself (`MP_GATE_DOCS_WRITE=1`), not by me — I
  did not edit it. Its prose line `port link 244 undefined` is now stale (the measurement is 248
  undefined / 244 MISSING); `check_docs_claims.py` only checks that the *baseline* figure 250 is
  quoted, so it does not catch it, and the goal prompt forbids me from editing that file.
* Run 1's `skActorApproachDistance` caveat (a `PathShagged` 100% that is really a folded-constant
  relocation difference) is untouched and still needs doing before this unit can flip.

## Lane 1: passed, then failed on the moved tip (2026-09-30 21:49:00Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 04ad12be7782; re-do it against the current tip.

---

# Run 4 (lane L1, 2026-10-01) — re-did run 3 against the moved tip, **+5: 28 -> 33 / 39**, judged failure fixed, `SpotPlayer` matched

**Re-measured first.** The clean tree at `04ad12be` was at **28 / 39**, fuzzy 54.9139, whole-DOL
`11294 / 28465` — run 3's source change was *not* in the tree (the driver had reset it), so this is
a continuation and **not** a `STALE`. Now **33 / 39**, unit fuzzy **75.06**, matched code **65.31**,
whole-DOL `11299 / 28465`.

`tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
`matched 11294 -> 11299, linked 5507 -> 5507, +5 functions at 100%, 0 units newly linked, no regression`.

## Why the judged run failed, and the one-line fix (run 3's `check-gate.log` was the whole story)

The code was never the problem: `build/goal/check.out` (run 3's own judge on the pre-rebase tree)
reads `PASS ... target rose: 28 -> 32 / 39`. The failure was the rebased `docs claims` step:

```
stale:   the gap table says other game methods is 164, the generated list has 166
stale:   the gap table's rows sum to 406, the generated list holds 246
```

`164` is run 3's hand-written number for *its* base. **The tip had already moved** (`33b784fb`
`progress-prime1-ctargetreticles` landed two more gap entries, 160 -> 162), so the correct number on
this tree is **166**, and `check_docs_claims.py` compares the table's rows against the *generated*
list, not against a number anyone remembers. `406 = 242 + 164` is the rebase leaving both the old and
the new row in the file. **The rule: never hand-write that count — run
`python3 tools/link_gap.py --rebuild --write-list` and set the row to whatever the list says, then
`python3 tools/check_docs_claims.py` before stopping.** I did that and the step is green.

## What changed

| file | what |
|---|---|
| `src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp` | `Dead`, `fn_801524fc`, `NoPathNodes`, `IsOnScreen` written (run 3's bodies, re-measured at 100% on this tree); **`PathFind` written** (99.31%); **`SpotPlayer` matched** (99.68 -> 100%); 2 includes added |
| `docs/research/port_link_gap_list.md` | `tools/link_gap.py --rebuild --write-list` — the same 4 additions, list 162 -> 166, total **246 MISSING** |
| `docs/research/port_link_gap.md` | table row 162 -> **166** + a dated section naming what provides each of the four |
| `docs/HANDOFF.md` | **not written by me** — `goal_check.sh`'s `MP_GATE_DOCS_WRITE=1` rewrote the two derived counts |

## Per function: before% -> after%

| function | before | after | Prime 1's source |
|---|---|---|---|
| `Dead` | 1.54 | **100** | small edit — see run 3; re-measured unchanged on this tree |
| `fn_801524fc` | 1.85 | **100** | Prime 1's `PathFind`, `kStateMsg_Activate` arm, unchanged |
| `NoPathNodes` | 5.00 | **100** | unchanged from run 1 |
| `IsOnScreen` | 3.33 | **100** | unchanged from run 1 |
| `PathFind` | 0.69 | **99.31** | Prime 1's, unchanged, **first attempt** — see the pool section, it is one instruction from 100% |
| `Landed` | 91.67 | 91.67 | wall, 5 more spellings this run |
| `SpotPlayer` | 99.68 | **100** | nested `if` + `const float magSquared` hoisted after `forwardDistance` — see below |

`PathFind` reproduced Prime 1's body **verbatim on the first build**, including the dispatch: a
`switch (msg)` with `kStateMsg_Activate` -> `fn_801524fc(mgr)` and `kStateMsg_Update` -> the body
gives retail's exact `cmpwi r29,1; beq; bge; cmpwi r29,0; bge; b` chain, and
`if (mVerticalMovement || mOnGround)` gives retail's `rlwinm. r3,26` / `rlwinm. r3,29` pair.
Two header facts that had to be right, both measured rather than assumed:

* **`CModelData::GetScale()` returns `CVector3f` by value** (`include/MetroidPrime/CModelData.hpp:147`),
  not a reference — that is why retail copies the scale vector to the stack **twice** (0x8 and 0x14
  of the frame) and the source has to call `GetModelData()->GetScale()` twice. The first copy is dead.
* `mPathOverCount += 1; mPathOverCount &= 3;` produces retail's exact pair of `lhz`/`rlwimi`/`sth`,
  the second store included. `IsOver()` is already `mCurWaypoint >= size - 1` in the header, which
  is retail's `addi r0,r4,-1; cmpw; bge`.

## `PathFind` 99.31%: the last instruction is the object's `.sdata2` literal pool, and `ApproachDest` owns it

The only differing instruction is retail's `lfs f5,-24776(r2)` (our `lfs f5,0(0)`), the 0.3f in
`GetTranslation() + 0.3f * CVector3f::Up()`. The instruction is the same; the **relocation target
differs**, and the reason is the layout of the unit's constant pool:

| index | retail (`orig` DOL `.sdata2`, dtk symbol names) | ours (`.sdata2` of the built object) |
|---|---|---|
| 0 | 0.0 `lbl_8041C2E0` | 0.0 |
| 1 | 0.7853982 `lbl_8041C2E4` | 0.7853982 |
| 2 | 2.3561945 `lbl_8041C2E8` | 2.3561945 |
| 3 | **1.0 `lbl_8041C2EC`** | 0.2 |
| 4 | **1.1920929e-07 (FLT_EPSILON) `lbl_8041C2F0`** | 0.3 |
| 5 | 0.2 `lbl_8041C2F4` | 4.0 |
| 6 | **0.3 `lbl_8041C2F8`** | 1.0 |
| 7 | 4.0 `lbl_8041C2FC` | 0.5 |
| 8 | 0.5 | -1.0 |
| 9 | -1.0 | — |

Retail has two entries we do not, and they sit at indices 3 and 4, i.e. **before** the 0.3f. Their
relocations exist only inside `ApproachDest` (`0x5c4`, `0x61c`, `0x734`, `0x784`, `0x7c8` — the
`1.f` of the `CBCLocomotionCmd`s and the `FLT_EPSILON` of the max-speed test), and `ApproachDest` is
declared before `PathFind` in the file. Insert `1.0f` and `FLT_EPSILON` at indices 3 and 4 and our
pool becomes retail's **exactly**, which would put the 0.3f at index 6 = `lbl_8041C2F8` and take
`PathFind` to 100%.

**So `PathFind` cannot reach 100% before `ApproachDest` is written, and that is a property of the
object, not of `PathFind`'s source.** Verified the other way round: the three constants *before* the
gap (0.0, pi/4, 3pi/4) are at the same indices in both, which is why `RotateToPoint` (which uses
`lbl_8041C2E0`) is 100% and why `PathShagged`'s 4.0 is off by two but still scores 100% through the
symbol reloc. Generalisable form of the lesson: **a function can be byte-exact in source and still
lose an instruction to the unit's literal-pool order, so check the pool before spending attempts on
operand order.** Also worth knowing: `tools/dump_fn_relocs.sh` answers nothing inside a claimed range
("no object in `build/G2ME01/obj` defines it") — use `powerpc-eabi-objdump -r` on
`build/G2ME01/obj/<unit>.o` instead, whose `.rela.text` is present and names `lbl_8041C2F8` and
friends.

## `ApproachDest` (1020 B) measured, and left alone — it is not a short job in Echoes

Disassembled and compared against Prime 1 rather than assumed. Echoes' version differs structurally:

* `mFaceVec` is a **CQuaternion**, not a `CVector3f` — the `CBCLocomotionCmd` argument is a 7-float
  struct `{move.x, move.y, move.z, q.x, q.y, q.z, q.w}` built in the frame, with the quaternion
  copied out of the static at `0x800174B0` as `f29..f32` (`xxsel`/`psq_st`).
* the Prime-1 `switch (mBehaviourOrient)` is a field of the **body controller**, not of
  `CPatterned`: `lwz r3,0x48c(r30)` then `lwz r0,0x588(r3)` compared against 7 and 8.
* `DeliverTargetVector`, `KnockbackWhenFrozen`, `mBehaviourOrient`, `mCurPattern`, `mPatterns` do
  not exist in this repo's headers at all, and the header must not be re-laid-out.
* it also needs `CBodyController::HasBodyState`, `CBodyStateInfo::GetMaxSpeed` and
  `CVector3f::Magnitude`, i.e. **3** more `port_link_gap_list.md` entries (run 3's
  `gap-list-is-the-budget` finding stands: they are cheap now that `--write-list` is a known step,
  but the function itself is 1020 bytes / 340 instructions of unseen logic).

That is a whole item, not a slice of one, and it is already named as a target by this item.

## Gates (all run in this worktree, all green)

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                  All: (see goal_check below)
                                         CPatternedAiFunctions: 75.06% fuzzy, 65.31% matched (33 / 39)
./tools/probe_sources.sh                 751 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/link_gap.py --rebuild      ok: 246 MISSING symbol(s), all accounted for in port_link_gap_list.md
python3 tools/check_docs_claims.py       docs claims agree with the tree
python3 tools/check_symbol_names.py      checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit main/MetroidPrime/Enemies/CPatternedAiFunctions
                                         ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                         +5 functions at 100%, 0 units newly linked, no regression
./tools/goal_check.sh build/goal/item.json
                                         PASS progress-prime1-cpatternedaifunctions
                                         ok  gate.sh  /  counts: matched 11294 -> 11299, linked 5507 -> 5507
                                         ok  All: line  /  target rose: 28 -> 33 / 39  /  no asm added
```

`flip_test.sh` was not run: this is a `progress` item and the unit stays `NonMatching`. No function
was declared or moved, so the decl order is unchanged and still descending by retail offset.

**Summary of this run: 5 functions matched (`Dead`, `fn_801524fc`, `NoPathNodes`, `IsOnScreen`,
`SpotPlayer`), unit 28 -> 33 / 39, `PathFind` written and left at 99.31% because of the
`.sdata2` pool order that `ApproachDest` owns.** The three `WALL:` lines that earlier runs left for
`SpotPlayer` are superseded - the wall was one *combination* of shapes away from a match, not a wall.

## Walls (spellings tried **this run**, measured; do not repeat)

`Landed` (48 B, **91.67% still the best**; one artifact — we re-load `lbz r0,0x34c(r3)` before the
second `rlwinm.`, retail reuses the byte from the first load):

* `if (mOnGround && !mPrevOnGround) { result = true; }` and `mPrevOnGround = mOnGround;`, no local
  at all -> 89.58
* the store moved inside both arms (`if (mOnGround) { result = !mPrevOnGround; mPrevOnGround = true; } else { mPrevOnGround = false; }`)
  -> 28.75
* the local `onGround` used for the test but `mOnGround` read again for the store -> 89.58
* `result = !mPrevOnGround ? true : false` inside the `if` -> 53.33
* `bool onGround` (non-const) with the local declared first -> 89.58

`SpotPlayer` (372 B) - **the ones that did not work; the one that did is at the top of this list.
The diff being chased was the `delta.MagSquared()` chain's accumulator being `f0` where retail's is
`f1`:**

* `mDetectionAngle * delta.MagSquared() < forwardDistance * forwardDistance` -> 99.41
* `forwardDistance * forwardDistance > mDetectionAngle * delta.MagSquared()` -> 99.68 (same score, same swap)
* nested `if` instead of `&&` with the local `angle` removed -> 99.68
* extra parentheses around the right operand -> 99.68
* `const float angle = mDetectionAngle;` at the top of the loop body -> 96.77
* **nested `if` + `const float magSquared = delta.MagSquared();` hoisted *after* `forwardDistance`,
  compared as `forwardDistance * forwardDistance > magSquared * mDetectionAngle` -> 100 (KEPT)**

  The three pieces were each recorded as failures in runs 2 and 3 *in combination with other
  spellings*: `const float magSquared` alone was 92.6 in both orders, the nested `if` alone was 99.14,
  and the nested `if` "with a local `angle`" was 96.77. The winning shape is the nested `if` **and**
  the hoisted `magSquared` together, declared after `forwardDistance`. **A wall written down as
  "shape X does not work" can be wrong about a combination of shapes; the runs that recorded those
  numbers never tried this pairing, and three runs of `WALL: SpotPlayer` were one experiment away
  from the match.** My own first pass at this run made the same mistake in the other direction — the
  batch script reported this variant as "FAIL" because it grepped the build log for a per-function
  line, and the report only prints functions *below* 100%. Absence of a line is a 100%, not a
  failure: re-read `CPatternedAiFunctions: ... (33 / 39 functions)` before believing a batch result.

For the record, the diff being chased was 3 instructions, all the same register swap: retail `fmuls f1,f2,f2` / `fmadds f1,f4,f4,f1` / `fmadds f1,f3,f3,f1` (the `MagSquared`
chain) against ours `f0` in all three, with `lfs f0,984(r28)` (mDetectionAngle) and
`fmuls f0,f1,f0` identical on both sides. No operand order in the source moves it.

WALL: Landed 91.67% - MWCC rematerialises the `0x34c` byte load before the `mPrevOnGround` extract; 26 declaration/branch/assignment shapes tried across four runs, none reaches 100%.
SpotPlayer is at **100%** (see the kept spelling above) - the three earlier `WALL:` lines for it were wrong about this combination and are superseded.

## What is still unmatched, and what each one needs (measured on this tree)

`PathFind` **99.31** (1 instruction - the `.sdata2` pool offset of the 0.3f; needs `ApproachDest` to
contribute its 1.0f and FLT_EPSILON, see above) | `Landed` 91.67 |
`PlayerSpot` 1.51 (372 B, Echoes-only: needs a `CPlayer` morph-state field at +908, the 5-word
`CMaterialFilter` built via `__shl2i(0,1)`, `CGameCollision::RayStaticLineOfSightTest`,
`CVector3f::Magnitude`; `ConvertToScreenSpace` is already in the gap list now) |
`ApproachDest` 0.39 (1020 B, see above) | `ApplyScreenShake` 1.75 (228 B, Echoes-only: needs
`~CCameraShakerData`, `TCastToPtr<CScriptCameraShaker>`, `fn_801E7EC0` - 3 more gap entries) |
`fn_80151920` (8 B, unnamed in `config/G2ME01/symbols.txt`; matching it means inventing an
`extern "C"` symbol, so left alone as runs 1-3 did).

No `NEW:` line this run: the pool finding is a codegen rule (notes), and the remaining functions are
either already named by this item or blocked on header members that no single unit owns.

---

# Run 5 (lane L9, 2026-10-02) - `ApplyScreenShake` matched, **33 -> 34 / 39**

Re-measured first: clean tree was 33 / 39 (run 4's source present), not `STALE`. Now **34 / 39**,
unit 78.50% fuzzy, 68.82% matched code; whole-DOL `12420 / 28465`. `goal_check.sh`: PASS.

| function | before | after | Prime 1 |
|---|---|---|---|
| `ApplyScreenShake` | 1.75 | **100** (first build) | none - written from `tools/dis.sh 0x80150FE8 0xE4` |

```cpp
void CPatterned::ApplyScreenShake(CStateManager& mgr, const CVector3f& position, TUniqueId shaker) {
  if (shaker == kInvalidUniqueId) shaker = FindConnectedObject(mgr, kSS_Footstep, kSM_Attach);
  if (CScriptCameraShaker* entity = TCastToPtr< CScriptCameraShaker >(mgr.ObjectById(shaker))) {
    CCameraShakerData data(entity->GetShakeData());
    data.SetPosition(position);
    fn_801E7EC0(mgr.CameraManager(0)->ShakeManager(), data, mgr, 0, 0);
  }
}
```

Facts read off the disassembly: state `0x464F4F54` ('FOOT'), msg `0x41544348` (`kSM_Attach`); the
`TUniqueId` by-value param is written back (`sth 0(r31)`), so the param itself is assigned;
`mgr+0x151c` is `mCameraManagers[0]` and `+0x88` of it is `mCameraShakeManager`; the shake copy is at
`CScriptCameraShaker+0x24` and its `mPosition` at `+0xc` of the copy (`stfs 28..36(r1)`).

Header additions (accessors / one enumerator only, no layout change): `kSS_Footstep` in
`CEntityInfo.hpp`, `CCameraManager::ShakeManager()`, `CCameraShakerData::SetPosition`,
`CScriptCameraShaker::GetShakeData`, and `extern "C" fn_801E7EC0(CCameraShakeManager*, const
CCameraShakerData&, CStateManager&, int, int)` in `CCameraShakeManager.hpp` (arg types of the last two
are a guess; both 0 at the only call site).

Port link: the first `goal_check` FAILED `probe link-gap` (293 undefined vs baseline 291, GREW) because
`TCastToPtr<CScriptCameraShaker>` and `fn_801E7EC0` were new gap symbols. Fixed without touching the
gap list: `PortGlobals.cpp` now defines the cast (`PORT_CAST_TO_PTR`, retail `li r4,41` verified at
0x80099D0C) and an announcing `ReportedCameraManagerStandIn` body for `fn_801E7EC0`. The count/gate
is exact (not headroom), so defining callees is the way, not a gap-list entry.

Not touched this run: `PathFind` 99.31 (pool order, needs `ApproachDest`), `Landed` 91.67,
`PlayerSpot` 1.51, `ApproachDest` 0.39, `fn_80151920` (unnamed). Run 4's analysis of those stands.

---

# Run 6 (lane L9, 2026-10-02) - `PlayerSpot` matched, **36 -> 37 / 39**

Re-measured first: clean tree was 36 / 39 (`PathFind` 99.31, `PlayerSpot` 1.51, `ApproachDest` 0.39 left),
not `STALE`. Now **37 / 39**, unit 84.32% fuzzy, 75.40% matched code, whole-DOL `12480 / 28465`.
`goal_check.sh`: PASS (`matched 12479 -> 12480`, no regression).

| function | before | after | Prime 1 |
|---|---|---|---|
| `PlayerSpot` | 1.51 | **100** | none (Echoes-only); written from `tools/dis.sh 0x80151D90 0x174` |

```cpp
bool CPatterned::PlayerSpot(CStateManager& mgr, const CTriggerData&) const {
  bool result = false;
  if (mgr.GetPlayer(0)->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
    if (IsOnScreen(mgr)) {
      const CVector3f eye = mgr.GetPlayer(0)->GetAimPosition(mgr, 0.f);
      CVector3f delta = GetBoundingBox().GetCenterPoint() - eye;
      const float distance = delta.Magnitude();
      delta *= 1.f / distance;
      const CMaterialFilter filter = CMaterialFilter::MakeInclude(CMaterialList(SolidMaterial));
      result = CGameCollision::RayStaticLineOfSightTest(mgr, eye, delta, distance, filter);
    }
  }
  return result;
}
```

Facts: `CPlayer+908` is `mMorphBallState` (==0 `kMS_Unmorphed`); the 5-word filter is
`MakeInclude(CMaterialList(SolidMaterial))` - `lwz r5,-31104(r13)` is the TU-static `SolidMaterial`
(`__shl2i(0,1,SolidMaterial)`), not a literal; the unit vector is `delta * (1.f / distance)`.
Compiled first try at 98.63%: only the filter differed (frame 144 vs retail 160, retail has an extra
`stw 1,72(r1)` temp). Spellings: inline `MakeInclude(CMaterialList(SolidMaterial))` as call arg -> 98.63;
named `const CMaterialList solid` -> 98.63; direct `CMaterialFilter(list, CMaterialList(), kFT_Include)`
-> 98.63; **a named `const CMaterialFilter filter = MakeInclude(...)` local, passed by reference -> 100**.
(Rule: a by-const-ref filter argument built in the call expression gets a smaller frame than retail's;
retail built a named local.)

Port link: `CGameCollision::RayStaticLineOfSightTest(const CStateManager&, ...)` is the only new gap name
(`CVector3f::Magnitude` is already defined). `CGameCollision.cpp` is NonMatching/not in the port, as for
its listed siblings, so: `link_gap.py --rebuild --write-list` (+1 entry), `port_link_gap.md` row 213 -> 214
and a dated section. `check_docs_claims.py` green. `docs/HANDOFF.md` derived counts were rewritten by
`goal_check.sh`, not by hand.

Not touched: `PathFind` 99.31 (pool order, needs `ApproachDest`'s 1.0f/FLT_EPSILON), `ApproachDest` 0.39
(1020 B; run 4's analysis stands - needs `mFaceVec` as CQuaternion, body-controller `mBehaviourOrient`
at +0x588, `CBodyController::HasBodyState`, `CBodyStateInfo::GetMaxSpeed`).
