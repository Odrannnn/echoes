# progress-prime1-cbslocomotion — `MetroidPrime/BodyState/CBSLocomotion`

**Result: `matched_functions` 23 → 37 of 49. Unit stays `NonMatching` (its `.text` is 9280
retail bytes and ours is far shorter; the flip is out of reach for this item).**

Measured with `build/report.json` via `tools/fast_try.sh MetroidPrime/BodyState/CBSLocomotion`,
and the whole tree with `./tools/decomp_build.sh`.

## Gates (all run after the change)

    sha1sum build/G2ME01/main.dol     -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
    ./tools/probe_sources.sh          -> 744 files, 0 failed, 0 errors
    python3 tools/check_symbol_names.py -> checked 502 units; 0 missing names
    ./tools/link_check.sh             -> NOT LINKED; unchanged from baseline (250 undefined, 0 duplicates)
    ./tools/decomp_build.sh           -> All: 30.04% fuzzy, 21.81% matched, 11.74% linked
                                        (9699 / 28465 functions)

The before/after comparison is a real one, not a recall: I captured the full per-function report,
`git stash`ed `include/` and `src/`, rebuilt, captured again, then popped. Diffing the two reports:

- **regressions anywhere in the tree: 0**
- functions that improved: 54 — 21 in this unit, the rest in sibling BodyState units that got
  better for free from the `CPASAnimParm` change (CBSJump 7, CBSHurled 5, CBSWallHang 4, CBSTurn 3,
  CGunController 3, CABSReaction 2, and one each in CScriptDoor/CABSFlinch/CABSAim).
- `All:` linked 9678 → 9699 (+21 = +14 here +7 next door).

## Per function: before % → after %

Prime 1's source (read-only clone at `../prime-ref`) was used as the reference; nothing was copied
from its headers and no class layout was changed.

| before | after | Prime 1 spelling? |
|---|---|---|
| 70.11 → **100** | `CBSLocomotion::Start` | needed a small edit: `!= nullptr` → `if (ptr)` + two calls. Retail `cmplwi/beq`, ours emitted `neg/or/srwi` |
| 76.97 → **100** | `CBSBiPedLocomotion::IsStrafing` | unchanged; the win was binding `cmdMgr`/`moveVec`/`faceVec` to named locals first |
| 5.74 → **100** | `CBSLocomotion::GetStartVelocityMagnitude` | unchanged |
| 85.00 → **100** | `CBSLocomotion::ComputeWeightPercentage` | unchanged (`rstl::max_val(rstl::min_val(...))`; `CMath::Clamp` was a stand-in) |
| 0.78 → **100** | `CBSLocomotion::GetBodyStateTransition` | needed the Echoes command order (below) |
| 39.75 → **100** | `CBSRestrictedLocomotion::CBSRestrictedLocomotion` | 14 → 15 |
| 2.64 → **100** | `CBSBiPedLocomotion::UpdateWalk` | unchanged |
| 1.65 → **100** | `CBSBiPedLocomotion::UpdateRun` | one edit: the non-positive-walk fallback is `skMinWalkPercent` (0.5f), not `1.f` |
| 0.94 → **100** | `CBSBiPedLocomotion::UpdateStrafe` | unchanged (strafe table `{5,4,1,3,6,7}` verified byte-identical at `.data:0x803B3EE0`) |
| 3.11 → **100** | `CBSFloaterLocomotion::ApplyLocomotionPhysics` | unchanged; needed `GetRestrictedFlyerMoveSpeed()`, which did not exist here |
| 1.06 → **100** | `CBSWallWalkerLocomotion::ApplyLocomotionPhysics` | one edit: `FaceDirection3D` → `FaceDirectionOnSurface` (the symbol retail calls) |
| 2.03 → **100** | `CBSFlyerLocomotion::ApplyLocomotionPhysics` | unchanged |
| 1.74 → **99.51** | `CBSAiMovedFlyerLocomotion::UpdateLocomotionAnimation` | unchanged (run-strafe table `{5,4,2,3,6,7}` verified at `.data:0x803B3EF8`); one FPR number short |
| 58.84 → **100** | `CBSFloaterLocomotion::~` | n/a (dtors) |
| 71.85 → **100** | `CBSBiPedLocomotion::~` | n/a |
| 58.84 → 83.39 | `~CBSFlyerLocomotion`, `~CBSWallWalkerLocomotion`, `~CBSAiMovedFlyerLocomotion`, `~CBSBlendedLocomotion` | n/a |
| 19.01 → 89.22 | `CBSBiPedLocomotion::CBSBiPedLocomotion` | 14 → 15, plus the ctor spelling fix below |
| 1.22 → 86.78 | `CBSLocomotion::ApplyLocomotionPhysics` | unchanged |
| 99.05 | `CBSFlyerLocomotion::CBSFlyerLocomotion` | unchanged (epilogue restore order) |
| 0.96 | `CBSBiPedLocomotion::UpdateLocomotionAnimation` | blocked, see below |
| 0.76 | `CBSBlendedLocomotion::UpdateLocomotionAnimation` | no reference, Echoes-only class |
| 0 / 0 | `fn_800F4FB4`, `fn_800F4FF8` | see "unnamed" below |

## Echoes' command priority in `GetBodyStateTransition`

Recovered from retail's `li r4,<cmd>` / `li r3,<state>` pairs; Prime 1's order is wrong here.
Top level: `Hurled(16)→kAS_Hurled`, `KnockDown(3)→kAS_Fall`, `LoopHitReaction(9)→kAS_LoopReaction`,
`KnockBack(4)→kAS_KnockBack`, then `kBSC_Locomotion(25)` → `ClearLocomotionCmds()` and skip the rest.
Inside the else: `Slide(20), Generate(15), MeleeAttack(5), ProjectileAttack(6), LoopAttack(7),
LoopReaction(8), Jump(17), Taunt(21), Step(1), Cover(23), WallHang(24), Scripted(22)` — note
**Taunt comes before Step and Scripted is last**, unlike Prime 1. The move/face/`IsMoving()` and
`mLocomotionType` tests sit inside that else, as in Prime 1.

## The three changes that unlocked whole groups

1. **Empty destructors belong in the header** (`~CBSFlyerLocomotion() override {}` etc., matching
   `CBSJump.hpp`/`CBSTurn.hpp`). Retail's 124-byte locomotion destructors inline the whole vtable
   chain instead of calling the base; out-of-line `{}` bodies made mwceppc emit a `bl`. That alone
   took five destructors off 58.84%.
2. **`rstl::pair<int,float>` was modelled as needing construction/destruction.** Retail stores the
   15x8 table with plain `stw`/`stfs`. Declaring it trivially constructible/destructible
   (`include/rstl/pair.hpp`) removed the fill-loop guards in the BiPed constructor (19 → 67) and
   the teardown loops in `~CBSBiPedLocomotion` (71.85 → 89). Together with (3):
3. **`reserved_vector<pair<int,float>,8>` and its 15-element outer vector are trivially
   destructible** — retail's BiPed destructor is 108 bytes with no member teardown at all.
   *Gotcha, cost me two builds:* these specialisations must appear in `CBSLocomotion.hpp`
   **before** the classes, immediately after the `rstl` includes. Placed at the bottom of the
   header, mwceppc 2.7 fails with `struct/union/enum/class tag 'is_trivially_destructible'
   redefined` — it has already instantiated the primary template from `reserved_vector`'s own
   members by then. With them at the top, BiPed's destructor reaches 100%.
   `rstl::is_trivially_destructible<rstl::pair<int,float>>` and `CPASAnimParm` are declared
   the same way; the latter is also why the sibling BodyState units improved.

## Smaller findings (spellings tried, for the next run)

- **`CBSBiPedLocomotion`'s member initialiser is not Prime 1's.** Prime 1 writes
  `mAnims(14, reserved_vector<pair,8>(8, pair(0,0.f)))`; retail fills *N* elements, so it is
  `mAnims(reserved_vector<pair,8>(pair(0,0.f)))` using the single-argument (fill-N) constructor.
  With the two-argument form mwceppc outlined a different loop.
- **`CVector3f::operator/(v, f)` in this repo divides each component (three `fdivs`); retail takes
  one reciprocal and three multiplies.** Rather than change that shared operator I wrote
  `moveImpulse * (1.f / act->GetMass())` in `CBSWallWalkerLocomotion::ApplyLocomotionPhysics`
  (96.21 → 100). *Worth a separate item:* `CVector3f::operator/` and `operator/=` should match
  retail (`v *= 1.f / f`), exactly as Prime 1's `CVector2f` already does.
- `CRelAngle`'s float constructor is **private** here (public in Prime 1), so
  `CRelAngle(x)` will not compile; `CRelAngle::FromRadians(x)` is the equivalent.
  `GetMaximumPitch()` returns `const float&`, which breaks `rstl::min_val`'s deduction — needs
  `rstl::min_val<float>(...)` or a named local.
- `CActor::TransformWorldToLocalRotation` does not exist here; the repo's spelling for that and
  every other call site is `GetTransform().TransposeRotate(v)`.

## What is still blocked, and why

- **`CBSBiPedLocomotion::UpdateLocomotionAnimation` (584 B, 0.96%)** reads a bool at
  `CBodyController + 1388`, i.e. `CBodyStateInfo + 512`, via `lbz` — that is Prime 1's
  `GetLocoAnimChangeAtEndOfAnimOnly()`. This repo's `CBodyStateInfo` has no such member, so
  writing it means adding a field, i.e. a class-layout change, which this item is not allowed to
  do unmeasured. I stopped rather than guess an offset.
- **`CBSBlendedLocomotion::UpdateLocomotionAnimation` (1052 B, 0.76%)** has no Prime 1 counterpart
  (the class is Echoes-only) and no stub in this tree to work back from. Everything downstream of
  it — `CBSAiMovedFlyerLocomotion`, `UpdateStrafe`, `UpdateWalk`, `UpdateRun`, `GetBodyStateTransition`
  — now calls it through the vtable, so the blocked function has no effect on the ones above.
- **The four 83.39% destructors and the 99.05% Flyer constructor are register-materialisation
  differences, not logic.** Retail writes each vtable address with its own `lis r3,0x803b` +
  `addi r0,r3,imm`; mwceppc here emits one `lis` and then derives all four with `addi r0,r3,off`.
  The instruction sequence and every branch target are otherwise identical. The Flyer constructor
  differs only in the order of three spill restores in the epilogue (`lwz r0,20(r1)` first in
  retail). I found no source spelling that changes either.
- **`CBSLocomotion::ApplyLocomotionPhysics` (86.78%)** differs only by mwceppc deciding to
  callee-save `f30` (and emitting a stray `xscmpgedp` prologue artifact) where retail reloads
  `CBodyStateInfo + 512` through a GPR. Same 160 instructions, same stack size, shifted slot
  numbers. Two spellings tried (named `const float& maxPitch`, explicit `rstl::min_val<float>`,
  C-style cast) — all 86.78%.
- **`CBSAiMovedFlyerLocomotion::UpdateLocomotionAnimation` (99.51%)** is one float register number:
  retail holds `localVec.x` in `f0`, ours in `f1`, which shifts `f0/f4/f5` down one. Four
  spellings tried (non-const `localVec`, `const`, no `(void)` casts, hoisted `localVecSq[0]`) —
  all 99.51%.
- **`fn_800F4FB4` / `fn_800F4FF8` (68 B / 12 B) stay at 0%.** They are
  `reserved_vector<pair<int,float>,8>`'s copy constructor and `__sinit_CBSLocomotion_cpp`. We emit
  an instruction-identical `__sinit` (12 bytes, `lfs`/`stfs`/`blr`) and objdiff still does not pair
  it with the unnamed retail symbol, and the copy constructor is now inlined rather than outlined.
  Nothing to change in the source.

## NEW

(no new queue item: everything left is either blocked on a class-layout change or is a measured
wall with the spellings listed above)
