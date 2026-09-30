# progress-prime1-cbsjump

`MetroidPrime/BodyState/CBSJump` — 10/16 → **14/16** matched functions. The unit stays
`NonMatching`; `flip_test` was not run to decide anything.

## Measured

| | before | after |
|---|---|---|
| unit `matched_functions` | 10 / 16 | **14 / 16** |
| unit `matched_code_percent` | 21.05% | **53.88%** |
| unit `fuzzy_match_percent` | 90.92% | **99.46%** |
| global `matched_functions` | 9943 | **9947** |

Per function, before% → after%, and whether Prime 1's source carried over:

| function | before | after | Prime 1 source |
|---|---|---|---|
| `GetBodyStateTransition` | 87.66% | **100%** | needed edits (see below) |
| `Start` | 95.03% | **100%** | matched unchanged except one hoisted local |
| `CheckForWallJump` | 88.99% | **100%** | **matched unchanged**, once written as Prime 1 has it |
| `UpdateExitJump` | 71.64% | **100%** | no Prime 1 counterpart (Echoes-only); structure reversed by hand |
| `PlayJumpLoop` | 93.05% | 99.83% | **mostly** — two edits, see below |
| `UpdateBody` | 87.78% | 98.37% | **no** — Echoes' engine forked; only the target-vector idiom carried over |

The four functions that reached 100% are exact objdiff matches, not percentages.

## What actually fixed each one

**`GetBodyStateTransition` 87.66 → 100%.** Prime 1 writes
`bc.GetCommandMgr().GetCmd(kBSC_Hurled)` and casts the `const CBodyStateCmd*` back with
`const_cast`; the scaffold called `bc.CommandMgr().GetCmd(...)`, which selects the
**non-const** `GetCmd` overload and cost the wrong callee. The second fix was declaring
`CBodyStateCmdMgr& cmdMgr = bc.CommandMgr();` *before* the first `GetCmd` rather than
after the Hurled test — the compiler then hoists `addi r31, r30, 4` to the top of the
function and reuses r31 for all four calls, as retail does. Both are Prime 1's spelling.

**`Start` 95.03 → 100%.** Prime 1's source is byte-for-byte right; the only difference was
`mWallBounceRight = Dot(cross, mWaypoint2 - mWaypoint1) < 0.f` inline versus Prime 1's
named `toFinal` local. The named local is what retail compiled from: without it the
temporaries get 8-byte slots and the frame is 208 bytes instead of 192.

**`CheckForWallJump` 88.99 → 100%.** Prime 1's `int ret = false; … ret = true; return ret;`
and its `pas::EJumpState state = kJS_WallBounceLeft; if (mWallBounceRight) state = …;`
were the whole fix. The scaffold returned `true`/`false` at two return points, which
needs only four callee-saved registers; retail keeps `ret` in r31 across the function
(`stmw r27,204(r1)`), so it needs five. Prime 1's source transfers unchanged.

**`UpdateExitJump` 71.64 → 100%.** Echoes-only, no Prime 1 reference. Two changes: invert
the test to `if (mState != kJS_ExitJump) { … } else if (bc.IsAnimationOver()) { … }`, and
hoist `const CPASDatabase& db = bc.GetPASDatabase();` to the top of the `if`. The
inversion matters: retail tests `mState` once (`cmpwi r0,6; beq`) and keeps the whole
IsAnimationOver tail out of line; the scaffold's `==` form put it in line. Hoisting
`db` is what makes `GetPASDatabase` be called once, before the parm temporaries, matching
retail's `mr r3,r28; bl GetPASDatabase; mr r0,r3; mr r30,r0`.

**`PlayJumpLoop` 93.05 → 99.83%.** Prime 1's two idioms transfer, with two adaptations:
- `CPASAnimParm::FromEnum(1)` in Prime 1 becomes `FromEnum(pas::kJS_AmbushJump)` — same
  value, and retail's `li r4,1` confirms the literal.
- Prime 1's `const CVector3f vel = …; mApplyLaunchVel = false; mVelocity = vel;` order
  is what retail does (`lfs f0,424(r29); li r3,0; lbz r0,56(r30); …`).
The remaining 0.17% is the register order inside the two `CScriptMsg` constructions.
Retail loads `kInvalidUniqueId` into r7 *before* the actor's uid into r6; the compiler
picks the opposite order from the ctor arguments. Binding both to named locals *in
retail's order* (`const TUniqueId inv = …; const TUniqueId uid = …;`) recovered 8 of the
43 differing instructions. Six other spellings were tried and are all worse — see the
spellings list below.

**`UpdateBody` 87.78 → 98.37%.** Prime 1's source does not transfer; Echoes added the
Unknown19/exit-jump path, `mFacingFlags`, `mAnimationVariant` and the third
`ForceLand` call. What did transfer is the *layout* idiom:
- `if (mExitJumpRequested == true)` rather than `if (mExitJumpRequested)`. Worth 100
  differing instructions: the bare form makes the compiler reuse the extracted bit
  register and emit `rlwinm.`+`beq`, where retail re-loads the byte, extracts, and does
  `cmplwi r0,1` + `bne`.
- `if (state == kAS_Invalid) { switch … }` instead of the early `return state;`. The
  early return makes the switch the fall-through; retail branches over it.
- **Case order: ExitJump, WallBounce, OutOfJump.** Retail's block addresses are
  ExitJump 0x910, WallBounce 0x928, OutOfJump 0xab4, and the switch dispatcher tests
  `cmpwi r0,6` *before* reaching the WallBounce arm. Declaring the cases in that order is
  worth 90 differing instructions on its own — by far the largest single win in this
  function, and the one that is easy to miss because the behaviour is identical.
- Binding `CBodyStateCmdMgr&` per case (retail keeps `&cmdMgr` in r27 across the Loop
  case's target-vector test and the following `GetCmd(10)`), worth 16 and 16 in the
  AmbushJump and IntoJump cases.
- `CVector3f(factor * d.GetX(), factor * d.GetY(), 0.f)` instead of
  `CVector3f(factor * d.ToVec2f(), 0.f)`. Prime 1 has the `ToVec2f` form; it compiles to
  a real `__ct__9CVector2fFff` and `__ml__FRCfRC9CVector2f` call here and a 32-byte-larger
  frame. Retail inlines the three multiplies. **Prime 1's version does not transfer.**

The remaining 1.63% is `fmr f1,f31` that retail emits before each `UpdateExitJump` call
and ours does not — a register-liveness artefact of the same tail, not a source-level
difference I could find a spelling for.

## Spellings tried and rejected (do not repeat)

For `PlayJumpLoop`'s CScriptMsg register order, all *worse* than the 43-instruction
baseline: named `const CScriptMsg msg(…)` local (172 insns, −4); `const TUniqueId uid`
alone (174, −1); `static_cast<TUniqueId>(kInvalidUniqueId)` (43, no change); uid as the
first ctor argument (43, no change); `const TUniqueId inv` alone (43, no change);
uid-then-inv (45, worse); one shared uid for both messages (173, −2). The winning form
is `inv` then `uid`, separately per message (175 insns, 35 differing).

For `UpdateBody`: `tv.GetX() != 0.f || tv.GetY() != 0.f || tv.GetZ() != 0.f` written
out instead of `IsNonZero()` (362 insns, −15 — worse); a named `const CVector3f& tv`
(374, no change); `return UpdateExitJump(bc, mgr)` at any of the three call sites
(375–376, worse everywhere — `state = …; break;` is correct); `while (state == kAS_Invalid)`
(384, much worse); CPhysicsActor TCast written as Prime 1's
`CPhysicsActor* actor = …; if (actor != nullptr)` (no change); the launch-velocity block
as a named `const CVector3f vel` (43 → 52, worse than Prime 1's `mVelocity = actor->GetVelocityWR()`).

## Codegen rules worth keeping

- A bitfield read written `if (flag)` and `if (flag == true)` differ by a reload of the
  whole byte plus `cmplwi r0,1`+`bne` versus `rlwinm.`+`beq`. ~100 instructions in a
  1500-byte function.
- Switch **case declaration order** decides block addresses. Matching retail's *layout*
  order, not the enum's order, is what closes the gap when the behaviour is already right.
- A function that keeps a named return value in a callee-saved register across its body
  needs one more saved register than one that returns constants at two points; that shows
  up as `stmw r27,…` + `ret` in r31 versus five separate `stw`.
- A `const T& x = <expr>;` local of a 12-byte type forces a stack slot where inlining the
  expression would have kept it in registers. `ToVec2f()` inside a `CVector3f` ctor is the
  case that cost a 32-byte frame here.

## Gates

```
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9943 -> 9947   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.63% fuzzy, 22.77% matched, 11.74% linked (9947 / 28465 functions)
  ok    target rose: main/MetroidPrime/BodyState/CBSJump: 10 -> 14 / 16 functions
  ok    no asm added
goal_check: PASS progress-prime1-cbsjump
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`python3 tools/check_symbol_names.py` = 0 missing. `check_decl_order.py` = ok.
`tools/unit_fit.sh` reports 14 functions present in ours but not in the retail unit
object (316 bytes); all are inline virtuals / COMDAT weak copies (`ApplyHeadTracking__10CBodyStateCFv`,
`GetGravityConstant__10CPatternedCFv`, …), which both linkers discard, so they are not
the cause of a failed flip. Diff touches `src/MetroidPrime/BodyState/CBSJump.cpp` only.

## Not done

`PlayJumpLoop` at 99.83% and `UpdateBody` at 98.37% are one and two byte-level
scheduling decisions away from 100%, and I could not find a source spelling for either:
the PlayJumpLoop residue is which of two equal-typed `TUniqueId` values the allocator
puts in r7 versus r6, and the UpdateBody residue is a `fmr f1,f31` reload before three
calls. Both are the "remaining diff is only register allocation" wall, so I stopped
rather than spend the item on them; the four 100% functions are the counted result.
