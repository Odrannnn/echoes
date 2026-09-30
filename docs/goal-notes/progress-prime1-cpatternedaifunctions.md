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
