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
