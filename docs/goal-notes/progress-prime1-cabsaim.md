# progress-prime1-cabsaim - MetroidPrime/BodyState/CABSAim

Kind: `progress` (the unit stays `NonMatching`; do not judge this with `flip_test`).
Worktree: `../wt-mp2-goal-L2` on `goal/lane-2`. One file changed:
`src/MetroidPrime/BodyState/CABSAim.cpp`. No headers, no config, no `configure.py`.

## Result

`build/report.json` on the clean tree, before and after (source of truth, not recalled):

| function | before | after |
| --- | --- | --- |
| `__ct__7CABSAimFv` | 100.00% | 100.00% |
| `GetBodyStateTransition__7CABSAimFfR15CBodyController` | 100.00% | 100.00% |
| `Shutdown__7CABSAimFR15CBodyController` | 100.00% | 100.00% |
| `UpdateBody__7CABSAimFfR15CBodyControllerR13CStateManager` | 89.75% (1220 B) | **100.00% (1220 B)** |
| `Start__7CABSAimFR15CBodyControllerR13CStateManager` | 90.13% (504 B) | 99.17% (500 B of 504) |
| unit | 3 / 5 functions, 91.94% fuzzy, 20.48% matched code | **4 / 5 functions, 99.81% fuzzy, 76.75% matched code** |

`All:` went 10373 -> 10374 matched functions of 28465 (31.51% -> 31.52% fuzzy,
23.98% -> 24.00% matched code). Nothing anywhere got worse - `tools/goal_check.sh`
runs the report diff and passed.

## Per function, against Prime 1's implementation

Prime 1's decomp is at `/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref`
(read-only). Echoes' `CABSAim` is a fork: it has `mAimType`, a three-argument
`CPASAnimParmData` ctor, and a no-target `else` branch in `UpdateBody` that Prime 1 does
not have at all. So neither function could be pasted; both needed Echoes-specific edits.

### `UpdateBody` - 89.75% -> 100.00%. Prime 1's shape, with two edits that mattered.

Prime 1's body is directly usable apart from Echoes' extra `else`. Three changes, and
**only the last two changed the bytes** (measured per change with a per-instruction
diff, not guessed):

1. `target.ToVec2f().Magnitude()` -> `CMath::SqrtF(target.GetY() * target.GetY() +
   target.GetX() * target.GetX())`, written inline as a single expression. Retail loads
   `y`, `x`, `fmuls`, `fmuls`, `fadds`, `bl SqrtF` - no `CVector2f` temporary, so
   `ToVec2f()` (which returns by value and needs a stack temp) was costing instructions
   and 16 bytes of frame. Hoisting it into a named local instead **broke the match**
   (2 differing instructions) - keep it inline. 89.75% -> 91.79%.
2. Hoist `fabs(newWeight)` into a named local (`const float absHWeight = CMath::AbsF(newHWeight);`),
   as Prime 1's `absWeight`. Retail computes `fabs`/`frsp` once per weight and reuses the
   rounded value for both the `> 0` test and the `AddAdditiveAnimation` argument; the old
   code called `CMath::AbsF` twice. 91.79% -> 97.15%.
3. The no-target decay, and the shape of the entry test:
   - `if (state == pas::kAS_Invalid) { ... }` + one `return state` at the end, instead of
     `if (state != kAS_Invalid) return state;` at the top. Retail branches once (`bne`
     over the body); the early return costs a second branch.
   - `if (mHWeight < 0.f) { mHWeight = rstl::min_val(mHWeight + dt, 0.f); } else { mHWeight =
     rstl::max_val(mHWeight - dt, 0.f); }` as an **if/else with the assignment in each
     arm**, not the ternary `mHWeight = cond ? min_val(...) : max_val(...)`. This is the
     whole remaining gap: MW tail-merges the ternary's shared store into one `stfs`
     reached by two branches, while retail keeps a `stfs` in each arm plus one `b`. The
     two spellings differ by exactly 2 instructions (1212 B vs 1220 B).
   97.15% -> 100.00%, 0 differing instructions of 305.

Prime 1's `float absWeight = fabsf(newHWeight);` and the `fabsf(mHWeight) > 0.f &&
(mHWeight * newHWeight) <= 0.f` guard transferred **unchanged**; Echoes' `mAimType` in
`GetBodyStateTransition` is why the parm index is 2 here and 1 in Prime 1 - retail
Echoes uses `li r6,2` for `GetAnimParmData(mAnims[i], 2)`, so 2 is correct.

### `Start` - 90.13% -> 99.17%. Prime 1 gave two things, both needed.

1. `mNeedsIdle = bc.CommandMgr().GetCmd(kBSC_AdditiveIdle) != nullptr;` ->
   `mNeedsIdle = false; if (bc.CommandMgr().GetCmd(kBSC_AdditiveIdle)) mNeedsIdle = true;`
   Retail stores `li r0,0; stb` and then conditionally `li r0,1; stb`; the `!= nullptr`
   form made MW emit `neg`/`or`/`srwi` instead, and cost 4 instructions. This is exactly
   Prime 1's spelling. Needed, and it transferred unchanged.
2. Copy the returned parm into a named local before reading it:
   `const CPASAnimParm animParm(aimState->GetAnimParmData(mAnims[i], 2));` and then
   `CRelAngle::FromDegrees(animParm.GetReal32Value()).AsRadians();`. Retail does
   `lwz r4,16(r1); addi r3,r1,96; lwz r0,20(r1); stw r4,96(r1); stw r0,100(r1); bl
   GetReal32Value` - an 8-byte copy into a frame slot, i.e. a named local. Calling
   `GetReal32Value()` on the temporary reads it in place and skips the copy. Needed.

90.13% -> 99.17%. Everything else in `Start` now matches: same frame, same register
assignment (`r29=this, r30=bc, r31=mgr, r27=aimState, r25=animData, r24/r26` loop
pointers), same pool, same `FindBestAnimation` argument setup.

## What is left, and the spellings already tried

`Start` is **2 instructions of pure register allocation** away and stays `NonMatching`.
Retail:

```
mr      r3,r30
bl      GetPASDatabase
mr      r0,r3          <- extra
addi    r3,r1,24
mr      r4,r0          <- retail; ours is a single `mr r4,r3`
addi    r5,r1,104
addi    r6,r31,5860
li      r7,-1
bl      FindBestAnimation
```

Ours emits `mr r4,r3; addi r3,r1,24` and so is 500 B where retail is 504 B. The hidden
struct-return pointer (r3) and `this` (r4) are set up in the opposite order, and retail
routes the `CPASDatabase*` through `r0` to do it. Everything before and after is
instruction-identical, so this is not a logic difference and not a constant-pool
difference (the `lfs f31` is an SDA relocation on our side and a resolved `-24924(r2)`
on retail's, which objdiff ignores).

Spellings tried for that one statement, all measured with a per-instruction diff
(`0` = identical, `2` = the gap above; the count is "differing instructions"):

| spelling | result |
| --- | --- |
| `const rstl::pair<float,int> best = ...; mAnims[i] = best.second;` | 2 (125 instrs) |
| same, pair declared **outside** the loop and assigned in the loop | 2 |
| `mAnims[i] = db.FindBestAnimation(...).second;` (member access, no local) | 2 |
| `mAnims[i] = (db.FindBestAnimation(...)).second;` | 2 |
| `mAnims[i] = static_cast<int>(best.second);` | 2 |
| `const int anim = ....second; mAnims[i] = anim;` | 2 |
| `rstl::pair<float,int> best = ...` (non-const) | 2 |
| `const int maxAnim = -1;` passed to the call | 2 |
| `CRandom16* const rnd = mgr.Random();` / `CRandom16& rnd = *mgr.Random();` | 2 |
| `parms` declared non-const | 2 |
| `const CPASDatabase& db = bc.GetPASDatabase();` then `db.FindBestAnimation(...)` | 24 (126 instrs, right size, all registers shifted) |
| `const CPASDatabase* const db = &bc.GetPASDatabase();` | 24 |
| `const CPASDatabase db = bc.GetPASDatabase();` (by value) | 19 (131 instrs) |
| `parms` built inline as the call's argument | 18 |
| `db` bound before `parms` is built | 14 (127 instrs) |
| using `best.first` as well (to keep the pair live) | 48 |
| splitting the `mAngles` store out as well | 62 |

`db` as a named reference is the only one that reaches retail's instruction count, and
it shifts every register in the function. Nothing I tried reached 0.

WALL: Start__7CABSAimFR15CBodyControllerR13CStateManager 99.17% - 2 instructions, MW
orders the hidden struct-return pointer (r3) and `this` (r4) for the `FindBestAnimation`
call the other way round from retail; 17 spellings tried, none reached 0.

## Also worth recording for whoever flips this unit

`tools/unit_fit.sh MetroidPrime/BodyState/CABSAim.cpp` says the unit **cannot** flip
today, and this is independent of `Start`:

```
.text  claimed 2168  ours 2592  retail 2168  over by 424
extra: +108 __dt__7CABSAimFv        +92 __dt__18CAdditiveBodyStateFv
       +72 __dt__10CBodyStateFv     +60 __dt__16CPASAnimParmDataFv
       +8 x6 weak inline virtuals (ApplyAnimationDeltas, ApplyGravity,
          ApplyHeadTracking x2, CanShoot x2)
```

16 functions, 428 bytes, in an object retail does not have. The four destructors and the
six 8-byte inline virtuals are the usual weak/COMDAT shapes that the retail linker drops
(`CAi` carries 224 bytes of them and still flips), but `.data` is also 116 bytes over and
there is a `.sdata`/`.sbss` the split does not claim - so `.data`/`.sdata`/`.sbss`
placement is a second, separate question from `Start`. `.text` for the five real
functions is now 2112 of retail's 2168, and the 56-byte difference is entirely the two
missing instructions in `Start` (and the four bytes of frame).

## How this was measured

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/decomp_build.sh main/MetroidPrime/BodyState/CABSAim   # All: 10374 / 28465
python3 tools/check_symbol_names.py                          # 0 missing names
./tools/goal_check.sh build/goal/item.json                   # PASS
```

`goal_check.sh` output verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10373 -> 10374   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.52% fuzzy, 24.00% matched, 11.83% linked (10374 / 28465 functions)
  ok    target rose: main/MetroidPrime/BodyState/CABSAim: 3 -> 4 / 5 functions
  ok    no asm added
goal_check: PASS progress-prime1-cabsaim
```

`docs/HANDOFF.md`'s state block is the only other file with a modification in
`git status`; that is `tools/gate.sh` rewriting the derived counts from the new report
(10373 -> 10374, DOL 8825 -> 8826), not a hand edit. Per the brief I did not edit
`docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` or `docs/LANE_BRIEFING.md` myself.
