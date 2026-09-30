# progress-prime1-cmorphball

`MetroidPrime/Player/CMorphBall` **48 -> 55 matched functions** (158 total), unit fuzzy
**17.46% -> 19.66%**. Global `matched 9512 -> 9519`, `linked 4852 -> 4852`. No function went
worse, no asm added, `check_symbol_names.py` clean (484 units, 0 missing).

Two files: `src/MetroidPrime/Player/CMorphBall.cpp` and one 3-line accessor in
`include/MetroidPrime/Player/CPlayer.hpp`. No class layout touched, no `configure.py` /
`splits.txt` / `files.cmake` change, no docs claim restated by hand.

## Per function: before -> after, and whether Prime 1's source was enough

All bodies were written from **retail's disassembly**, not from Prime 1. Prime 1 was read first
and used as the shape; every divergence below is a measured one, not a guess. Retail's object is
`build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` (objdiff's `target_path`), so
`powerpc-eabi-objdump -d --start-address=... ` on it is the ground truth. Note the branch
divides: `build/G2ME01/src/...` is *our* object, `build/G2ME01/obj/...` is retail's.

| function | before | after | Prime 1's source |
|---|---|---|---|
| `ForwardInput` | 5.56% | **100%** | needed edits |
| `BallTurnInput` | 5.56% | **100%** | needed edits |
| `ApplyFriction` | 36.15% | **100%** | small edits |
| `UpdateSpiderBallSwingControllerMovementTimer` | 67.30% | **100%** | small edits |
| `CalculateBallContactInfo` | 78.42% | **100%** | small edits |
| `SpinToSpeed` | 84.77% | **100%** | small edits |
| `CalculateSpiderBallAttractionSurfaceForces` | 12.94% | **100%** | needed edits |
| `IsClimbable` | 88.98% | 99.68% | needed edits |
| `TransformSpiderBallForcesXZ` | 12.71% | 99.64% | needed edits |
| `TransformSpiderBallForcesXY` | 12.71% | 99.64% | needed edits |
| `DampLinearAndAngularVelocities` | 57.27% | 99.53% | small edits |
| `GetSpiderBallControllerMovement` | 2.47% | 99.75% | matched unchanged in shape |

**"needed edits" = Prime 1's text is GC/1.3.2 and this is GC/2.7.** The four map-input functions
are the clearest case: Prime 1 calls `ControlMapper::GetAnalogInput(...)` as a **static**, but
here `CControlMapper::GetAnalogInput` is a **non-static member** (retail passes `this+5072` in
`r3`), so every call has to be `mPlayer.GetControlMapper().GetAnalogInput(...)`. `mControlMapper`
was private with no accessor, so `CPlayer::GetControlMapper()` was added (3 lines, inline, no
layout change; the retail `addi r3,r3,5072` it mirrors is in the comment).

Echoes-only member renames needed in the same functions: `kC_TurnLeft`/`kC_TurnRight` are
already this game's names (2/3/4 in the enum, and retail's `li r4,1..4` confirms the order), so
only the *static-vs-member* and the `*_MP` model names differ.

The two force transforms read the camera manager off the **player** (`CPlayer+0x1318` =
4888 in retail's `lwz r3,4888(r4)`), not off `CStateManager`; `CPlayer::GetCameraManager()`
already existed and is what they call.

## Remaining sub-100% functions and why (measured, not a wall)

None of these is a spelling problem left to retry; each is register allocation or a stack frame
the compiler chose. Recorded so the next run does not re-try them:

- **`IsClimbable` 99.68%** - instruction-for-instruction identical to retail (verified by diffing
  the two disassemblies). Only the frame size differs: 80 bytes vs retail's 48, because our
  `GetBallToWorld` is not inlined at the call site. Retail's own `GetBallToWorld` (0.90% fuzzy)
  inlines `GetBallPosition` and reads `this+12` (`mRadius`) directly rather than calling
  `GetBallRadius()`, so fixing that one function would likely fix this one too.
- **`DampLinearAndAngularVelocities` 99.53%** - Prime 1 has a **2-argument** signature
  (`linDamp, angDamp`, `vel *= 1.f - linDamp`, no `dt`, no `pow`). Echoes' retail object is
  256 bytes and takes three floats, uses `pow()` twice and `CAxisAngle` scaling, so Prime 1's
  body is *not* this function. Writing Echoes' shape gets 99.53%; the rest is the frame.
- **`GetSpiderBallControllerMovement` 99.75%** - everything matches except the register pair in
  the rad->deg step: retail does `frsp f2,f1; lfs f1,const; fmuls f31,f1,f2` and we emit
  `frsp f1,f1; lfs f2,const; fmuls f31,f2,f1`. Three spellings tried, all 99.75%:
  `(180.f/M_PIF) * static_cast<float>(atan2(...))`; `static_cast<float>(...) * (180.f/M_PIF)`;
  and `CMath::Rad2Deg(static_cast<float>(...))`. A fourth, splitting it into
  `angleRad` then `angle = angleRad * (180.f/M_PIF)`, also gave 99.75%. The assignment is the
  compiler's, not the source's.
- **`TransformSpiderBallForcesXZ/XY` 99.64%** - a local `CTransform4f camXf` at `r1+8` vs
  retail's `r1+20`; the camera manager expression is now right (`mPlayer.GetCameraManager()`),
  only the temp's slot differs.
- **`GetBallTouchRadius`, `GetGravityAcceleration`, `CalculateSurfaceFriction`,
  `ComputeMaxSpeed`, `GetMinimumAlignmentSpeed`** - see the blocker below; the correct bodies
  need `gpTweakBall`.

## Measured, for whoever picks up the tweak-dependent five

These five have their **exact** shapes recovered and written into the comment above each
scaffold, so they only need the port to link `CTweakBall`:

- `GetBallTouchRadius` - retail loads the `gpTweakBall` SDA global and tail-calls
  `CTweakBall::GetBallTouchRadius`. **This one reaches 100%** from the one line
  `return gpTweakBall->GetBallTouchRadius();`.
- `GetGravityAcceleration` (99.86% when written) - `CheckSubmerged()`, then
  `GetPlayerState()->HasPowerUp(kIT_GravityBoost)`. **Prime 1's `kIT_GravitySuit` does not exist
  here**; Echoes' 0x19 is `kIT_GravityBoost`, which retail's `li r4,25` confirms. Then
  `mBallState == 4` -> `GetScrewAttackGravity`, `== 5` -> `GetScrewAttackWallJumpGravity`,
  else `GetBallWaterGravity` / `GetBallGravity`.
- `CalculateSurfaceFriction` (100% when written) -
  `GetBallTranslationFriction(GetSurfaceRestraint())`, `*= 2.f` while
  `mPlayer.mAttachedActor != kInvalidUniqueId` (`CPlayer+0x2e4`), then `*= count * 1.5.f` where
  count is `mPlayer.mEnergyDrain.GetEnergyDrainSources().size()` (`CPlayer+0x2ec`, count at
  `+0x2f0`). Both offsets match retail's `lhz r4,740(r3)` / `lwz r0,752(r3)`.
- `GetMinimumAlignmentSpeed` (100% when written) - `mBallState == 2` (`kBS_Spider`) -> `0.f`,
  else `gpTweakBall->GetMinimumAlignmentSpeed()`.
- `ComputeMaxSpeed` (96.71%) - half-pipe branch is
  `GetVelocityWR().Magnitude() * 1.5f`, then `rstl::min_val(maxSpeed, 95.f)` **then**
  `rstl::max_val(maxSpeed, 0.01.f)`. The operand order matters: retail compares `f0`(95)
  against `f2`(result), so it is `min_val(maxSpeed, k)` with maxSpeed first, *not* Prime 1's
  `min_val(k, maxSpeed)`. Constants confirmed by reading `.sdata2` out of the DOL
  (`dol_read.read`): 1.5, 0.01, 95.

## NEW

NEW: fix-port-ctweakball-accessors | port | symbol:CTweakBall::GetBallTouchRadius | the five
tweak-dependent CMorphBall bodies are fully recovered but `src/MetroidPrime/Tweaks/CTweakBall.cpp`
is `linked True` for the DOL only, so calling its 8 accessors grows the host link gap
(254 -> 262) and `gate.sh`'s `port probe` step fails on it. `GetBallTouchRadius` alone is one
line and reaches 100% on `CMorphBall` once it lands. All 8 already have real bodies in
`CTweakBall.cpp` (lines 29, 51, 262, 264, 266, 290, 330, 376); this is a `files.cmake` entry
plus loading the `mData` block, not 8 new algorithms, and it is the same shape as the five
`CTweakPlayer` accessors already on the gap list.

NEW: cmorphball-getballtoworld-inline | progress | MetroidPrime/Player/CMorphBall |
retail's `GetBallToWorld` (0.90% fuzzy) inlines `GetBallPosition` and reads `mRadius` at
`this+12` directly instead of calling `GetBallRadius()`; ours calls out twice, which is what
holds `IsClimbable` at 99.68% with a 80-byte frame against retail's 48.

## Pre-existing, not caused by this item

`gate.sh` ends at `GATE FAIL: ninja` for one reason only, and it is not this change:

    #   undefined: 'sndStreamMixParameter'
    #   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o

Reproduced at **clean HEAD** (`git stash`, then `ninja`) -> same error. Cause and the fix are
already characterised in `docs/goal-notes/progress-cgamestate-map-lowerbound.md`: `match-stream`
(ada6d97) flipped `musyx/runtime/stream.c` to `Matching` and `run_goal.sh:446`'s `stage_change`
cannot stage `extern/`, so the four `MUSY_VERSION` guards that keep
`sndStreamMixParameter` in the build were dropped. A duplicate of that `NEW:` is filed there.
Everything else in the gate is green: `per-function diff +7 functions at 100%`, `port probe ok`,
`port link gap ok`, `docs claims ok`, `module wiring`, `dol_read`, `gs offsets`, `raw offsets`,
`decl order`, `files.cmake`, `module order` all ok.

`python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` says "would break
on a flip", identically before and after this change (checked by stashing). The unit stays
`NonMatching`; it is 55/158, nowhere near a flip.

## Run on lane-9 — 2026-09-30

This worktree's baseline differs from the preceding run above; I re-measured it before editing.
`export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort;
./tools/decomp_build.sh main/MetroidPrime/Player/CMorphBall` reported **51/158** and global
**10278/28465**. `SwitchToMarble` was **76.33898%**. I adapted Prime 1's materialized `lookDir`
and `tiltQ` local shape, using Echoes' `GetLookDir()` accessor and its required
`CRelAngle::FromRadians(mBallTiltAngle)` conversion. After that edit, the same build reported
**52/158**, global **10279/28465**, and `SwitchToMarble` no longer appeared in the unmatched list
(100%). Prime 1's source needed small Echoes API edits.

Exact acceptance command: `./tools/goal_check.sh build/goal/item.json`. Result: `PASS
progress-prime1-cmorphball`; counts **10278 -> 10279**, linked **5043 -> 5043**, target **51 ->
52 / 158**, no asm added. The gate passed (including DOL/REL hashes, report diff, wiring, docs
claims, port probe and symbol-name check); no other function regressed. This progress item leaves
the unit `NonMatching`; no flip was attempted.
