# cmorphball-three-tweak-bodies (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-09-30. **Result: `goal_check` PARTIAL** - the unit's matched count rose **71 -> 73 of
158**, every other check green, and the flip still fails on two *unwritten* functions of the unit
(`CElementGen::GetEmitterTime() const`, `fn_800CD4B8`), unchanged by this diff.

**The item's `reason` was wrong on its central premise and I had to redo its first step.**
`src/MetroidPrime/PortCTweakBall.cpp` does **not** exist in this tree - not at HEAD
(`203d0b28`), not on `goal/decomp`, not on any branch (`git log --all -- src/MetroidPrime/
PortCTweakBall.cpp` is empty). The `cmorphball-touchradius` item that created it returned
PARTIAL and **its file never landed**; only its notes file was committed. So the "four more
accessors added to an existing listed file" step became "create the file, list it in
`files.cmake`, and write the accessors". Both of the other functions it names as still-needed
(`GetBallTouchRadius`, `GetMinimumAlignmentSpeed`) are **still scaffolds at 15.56% and 14.29%**
in this tree, for the same reason: their bodies need the same accessors and this item did not
write them.

## What I changed

Four files, one of them new.

**`src/MetroidPrime/PortCTweakBall.cpp` (new, 77 lines).** Port-only host definitions of **six**
`CTweakBall` accessors, copied character for character from `src/MetroidPrime/Tweaks/CTweakBall.cpp`:
`GetBallTranslationFriction` (:29), `GetBallTranslationMaxSpeed` (:51), `GetBallGravity` (:262),
`GetBallWaterGravity` (:264), `GetScrewAttackGravity` (:330), `GetScrewAttackWallJumpGravity`
(:376). Not five as the item said: `ComputeMaxSpeed` needs `GetBallTranslationMaxSpeed`, which
`docs/goal-notes/progress-prime1-cmorphball.md`'s list of "four more accessors" omitted.

**This is a real duplication and a reviewer should know about it.** `src/MetroidPrime/Tweaks/
CTweakBall.cpp` defines all six too, and a lane cannot list that unit: it is in
`tools/check_files_cmake.py`'s `EXCLUDED` list (`check_files_cmake.py:535`, the 23-TU batch
measurement from the 2026-09-28 sync, 314 -> 373 undefined and 6 duplicates) and that check
fails any path that is both listed and `EXCLUDED` (`:652-654`). `tools/` is the judge's. The
file's header says the two must never be compiled together; no build does. `gpTweakBall` itself
was never missing - `src/MetroidPrime/Tweaks/Tweaks.cpp:139` already constructs it and that unit
**is** in `files.cmake`.

**`files.cmake`** - one entry + a 6-line comment after `src/MetroidPrime/PortModuleManager.cpp`
(`:115-121`).
`check_files_cmake.py` passes (`: every configured DOL object is either in files.cmake or
excluded with a reason`).

**`include/MetroidPrime/Player/CPlayer.hpp:287-294`** - two inline accessors, both **required**:
`mAttachedActor` and `mEnergyDrain` are private and retail's body reads both directly. No class
layout touched (`gsoff` ok, `CHECK_SIZEOF` green). `GetAttachedActor()` returns `mAttachedActor`
(0x2e4, retail's `lhz r4,740(r3)`); `GetEnergyDrainSourceCount()` returns
`mEnergyDrain.GetEnergyDrainSources().size()` (0x2f0, retail's `lwz r0,752(r3)` = mEnergyDrain+4
= `rstl::vector::mCount`). Same arrangement as the `GetControlMapper()` accessor
`progress-prime1-cmorphball` added.

**`src/MetroidPrime/Player/CMorphBall.cpp`** - the three scaffolds written out, plus
`#include "MetroidPrime/Tweaks/CTweakBall.hpp"` (:19). Bodies are at :506-512 (`ComputeMaxSpeed`),
:533-539 (`GetGravityAcceleration`), :552-562 (`CalculateSurfaceFriction`). No `configure.py`, no
`config/`, no `splits.txt`, no `.s`, no asm. Nothing under `tools/` or `build/goal/` was edited.
(`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified; that is
`MP_GATE_DOCS_WRITE=1` inside `tools/gate.sh` rewriting the derived counts.)

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10481 -> 10483   linked 5051 -> 5051   (+2 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: CalculateSurfaceFriction__10CMorphBallCFv
  +100%    main/MetroidPrime/Player/CMorphBall :: GetGravityAcceleration__10CMorphBallCFv
no regression

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10481 -> 10483   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.76% fuzzy, 24.35% matched, 11.84% linked (10483 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
              #   undefined: 'CElementGen::GetEmitterTime() const'
              #   undefined: 'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 71 -> 73 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-three-tweak-bodies - flip_test ...: FAIL, but the target rose; commit it and keep the item
```

Per function, `build/G2ME01/report.json`: `CalculateSurfaceFriction__10CMorphBallCFv` (152 B)
**5.131579 -> 100.0**; `GetGravityAcceleration__10CMorphBallCFv` (144 B) **3.8888888 -> 100.0**;
`ComputeMaxSpeed__10CMorphBallCFv` (152 B) **5.131579 -> 96.84**. Unit `.text` fuzzy
20.722042 -> 21.36.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. All 86 RELs
unchanged (`rel_module_order: 86 modules, unchanged`). `build/gate-probe.log`: `probe: 754 files,
0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)` - **250 against the judge's
baseline of 250, unchanged**, and `link_check: unchanged from baseline (250 undefined, 0
duplicates)`: the 11 new calls are paid for by the 11 new definitions, which is the whole point
of the new file. `docs claims agree with the tree`; `973 unit(s) checked, 31 permuted, all 31
accounted for`. `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` says "would
break on a flip" - **identical at HEAD** (checked by stashing the three edits), so it is
pre-existing on a 73/158 unit, not caused here. `unit_fit.sh` does not apply: the unit is not
declared in any `splits.txt`.

## Two corrections to what the earlier notes recorded

**1. `ComputeMaxSpeed`'s clamps are `max_val` first, then `min_val` - and both operands.**
`docs/goal-notes/progress-prime1-cmorphball.md` says "min_val(maxSpeed, 95.f) **then**
max_val(maxSpeed, 0.01.f)". Retail's `main.elf` (retail for `.text`) at 0x800C193C..0x800C1970
says otherwise. It loads **both** constants before the multiply:

```
800c193c: lfs  f0,-28852(r2)      # 1.5
800c1940: lfs  f2,-28764(r2)      # 0.01
800c1944: fmuls f0,f0,f1
800c1948: fcmpo cr0,f0,f2
800c194c: bge   800c1954          # product >= 0.01 -> f2 = f0   (a MAX)
800c1950: b     800c1958
800c1954: fmr   f2,f0
800c1958: lfs  f0,-28836(r2)      # 95
800c195c: fcmpo cr0,f2,f0
800c1960: bge   800c196c          # clamped >= 95 -> f1 = f0     (a MIN)
800c1964: fmr   f1,f2
800c196c: fmr   f1,f0
```

Constants read out of the retail DOL's `.sdata2` with `tools/dol_read.py` (r2 = 0x804223C0 =
`_SDA2_BASE_`): `-28852 = 1.5`, `-28764 = 0.01`, `-28836 = 95`, `-28784 = 2.0`,
`-28760 = 4503601774854144.0` (= 2^52 + 2^31, the magic constant `CalculateSurfaceFriction` uses).

**2. `GetGravityAcceleration` must be an if/else chain, not a switch.** Retail tests 4 then 5
(`cmpwi r0,4` / `bne` / `cmpwi r0,5` / `bne`). Written as a `switch`, mwcceppc emits a
`cmpwi r0,5` / `beq` / `bge` / `cmpwi r0,4` tree in the **opposite** order and the function
measures **83.03%** with four `DIFF_INSERT`s. The if/else chain reaches 100%.

## `ComputeMaxSpeed` is at 96.84% and cannot reach 100% from the source

**Spellings tried this run, all measured, none reached 100%:**

| spelling | score |
|---|---|
| `max_val(x, 0.01f)` then `min_val(x, 95.f)`, one local `maxSpeed` | **96.84** |
| same, but `const float scaled` / `const float clamped` | 96.84 |
| same, but `const float limit = 95.f` first | 96.84 |
| `min_val(95.f, clamped)` (operands swapped) | 94.34 |
| `min_val(95.f, maxSpeed)` | 94.34 |
| `maxSpeed >= 95.f ? 95.f : maxSpeed` | 94.21 |
| (earlier, wrong order) `min_val(x, 95.f)` then `max_val(x, 0.01f)` | 96.58 |
| (earlier, both mins) `min_val(x, 95.f)` then `min_val(x, 0.01f)` | 96.84 |

**The remaining diff is one register and nothing else.** objdiff, one row per differing
instruction, at 96.84%:

```
DIFF_ARG_MISMATCH 5816  fcmpo cr0, f2, f0      # retail: fcmpo cr0, f2, f0  <- same mnemonic+regs
DIFF_DELETE      5824  fmr f1, f2
DIFF_ARG_MISMATCH 5832  fmr f1, f0
plus the three constant loads, which name the same addresses
```

Reading the two objects side by side, every instruction from 0x36c8 to 0x370c is
instruction-for-instruction and register-for-register identical to retail's 0x800C190C..0x800C1944
including `fcmpo cr0,f0,f2` / `bge` / `fmr f2,f0`. The whole remainder is that retail loads the
95 constant into **f0** and we load it into **f1**, which shifts the compare operands of the
second `fcmpo` and the `fmr` pair. That is register allocation over an expression whose source
shape is now settled by the disassembly, not a spelling I have not tried. **I did not try to
force it with a dummy local or a `volatile`** - those change the source for a percentage, which
is the trade the brief tells me not to make.

## Still open in this unit, measured not guessed

- **86 of the unit's 158 functions have no body.** That, not anything in this diff, is what
  stops the flip: mwldeppc `undefined:` for `CElementGen::GetEmitterTime() const` and
  `fn_800CD4B8`. A carve is the only route and that is `configure.py` + `splits.txt` +
  `files.cmake` + the source's own claim, four files, which is a different item.
- **`GetBallTouchRadius` (36 B, 15.56%) and `GetMinimumAlignmentSpeed` (56 B, 14.29%)** are the
  other two tweak-dependent bodies and are **still scaffolds** - the file that would have made
  them writable did not land (see the top of this note). Retail's `GetBallTouchRadius` is
  9 instructions, `lwz r3,-28244(r13)` + `bl GetBallTouchRadius__10CTweakBallCFv` and nothing
  else, so `return gpTweakBall->GetBallTouchRadius();` is byte-exact and measures 100.00%.
  **This is now one run's work with no `files.cmake` or `tools/` blocker left.**
- **`ComputeMaxSpeed` at 96.84%** - the register wall above.

NEW: cmorphball-touchradius-accessors | match | MetroidPrime/Player/CMorphBall |
src/MetroidPrime/PortCTweakBall.cpp now exists and is listed in files.cmake (added by
cmorphball-three-tweak-bodies, which measured CalculateSurfaceFriction and
GetGravityAcceleration to 100% with it), but it does not yet define CTweakBall::GetBallTouchRadius
or GetMinimumAlignmentSpeed, so CMorphBall::GetBallTouchRadius (15.56%) and
::GetMinimumAlignmentSpeed (14.29%) are still scaffolds. Retail's GetBallTouchRadius is 9
instructions - `lwz r3,-28244(r13)` + `bl GetBallTouchRadius__10CTweakBallCFv` - and
`return gpTweakBall->GetBallTouchRadius();` measured 100.00% in an earlier run; the other is
`mBallState == 2 ? 0.f : gpTweakBall->GetMinimumAlignmentSpeed()`. Two accessor bodies plus two
one-line callers, and the port link stays at its 250 baseline.

## Files

- `src/MetroidPrime/PortCTweakBall.cpp` (new, 77 lines)
- `files.cmake` (one entry + comment, after `src/MetroidPrime/PortModuleManager.cpp`, :115-121)
- `include/MetroidPrime/Player/CPlayer.hpp:287-294` (two inline accessors, no layout change)
- `src/MetroidPrime/Player/CMorphBall.cpp`: include at :19; `ComputeMaxSpeed` :506-512,
  `GetGravityAcceleration` :533-539, `CalculateSurfaceFriction` :552-562

Not committed, per the brief.
---

# Run 2 — lane 2, 2026-09-30. **Result: `goal_check` PARTIAL**, 75 -> 82 of 158.

The item's `reason` is now **stale**: `ComputeMaxSpeed`, `GetGravityAcceleration` and
`CalculateSurfaceFriction` were all landed by the previous run and by
`cmorphball-touchradius-accessors`, and the file the reason points at
(`docs/goal-notes/progress-prime1-cmorphball.md`) describes a tree that is **not** this one.
Re-measured baseline on this branch (`goal/lane-2` @ `80bc1839`):
`main/MetroidPrime/Player/CMorphBall` **75 / 158**, global **10485 / 28465**, unit `.text`
fuzzy 21.47%. So I ignored the reason's list and worked the unit's own unmatched list, using
the same method the earlier notes established (read retail's object with
`build/binutils/powerpc-eabi-objdump -d -r --disassemble=<sym>` on
`build/G2ME01/obj/...` and match it instruction by instruction, then
`./tools/fast_try.sh MetroidPrime/Player/CMorphBall` per spelling - a second or two each).

**`fast_try.sh` does not print a build failure loudly enough.** Two of my early "measurements"
were of a stale object because the compile had failed; the tell is that the unit's fuzzy
percent does not move at all. Always check `ninja build/G2ME01/src/.../CMorphBall.o` when a
score looks like the previous one.

## Seven functions reached 100%, five more moved a long way

| function | before | after |
|---|---|---|
| `ForwardInput` | 5.56 | **100** |
| `BallTurnInput` | 5.56 | **100** |
| `DoUserAnimEvent` | 6.36 | **100** |
| `CalculateSpiderBallAttractionSurfaceForces` | 12.94 | **100** |
| `ApplyFriction` | 36.15 | **100** |
| `CalculateSurfaceToWorld` | 85.23 | **100** |
| `IsClimbable` | 88.98 | **100** |
| `GetSpiderBallControllerMovement` | 2.47 | 95.80 |
| `TransformSpiderBallForcesXZ` / `XY` | 12.71 | 99.71 |
| `SpinToSpeed` | 84.77 | 99.69 |
| `GetRenderBounds` | 93.88 | 96.48 |

Nothing went worse; `DampLinearAndAngularVelocities` is unchanged at 57.27 after two
spellings that measured worse (recorded below, do not retry them).

## The codegen rules this run established (they generalise)

- **`<=` and `&&` at the leaves of a `bool` return are not free.**
  `ApplyFriction`: `velocity.Magnitude() <= friction` emits `fcmpo cr0,f1,f31` /
  `cror eq,lt,eq` / `bne`. Retail has `fcmpo cr0,f31,f31`... specifically
  `fcmpo cr0,f31,f1` / `bge` - i.e. the test is written **`friction <
  velocity.Magnitude()` with the work in the `if` body and `CVector3f::Zero()` in the
  `else`**. `friction >= ...` also matches the operands but costs a `cror`, so `<` is the
  spelling. Swapping the operand order of the multiply to
  `velocity.AsNormalized() * (velocity.Magnitude() - friction)` is what puts
  `Magnitude()` before `AsNormalized()`, as retail has it. Both together: 36.15 -> 100.
- **`return x > a && x < b;` vs `if (x > a && x < b) return true;`** - `IsClimbable`,
  88.98 -> 100. The folded form makes mwcceppc keep the bool in r31, which costs a fourth
  callee-saved register and a 64-byte frame against retail's 48. Retail's
  `li r3,1` / `li r3,0` with one out-of-line block is the `return true` / `return false`
  spelling.
- **`a*b + c*d` is contracted into one `fmadds`** (`-fp_contract on`). Retail is not
  contracted there. Writing the sum as **two `CVector3f * float` results added together**
  (`CVector3f(m00,m10,m20) * f.x + CVector3f(m01,m11,m21) * f.y`) has no add inside a
  multiply, so the compiler emits six `fmuls` and three `fadds` - byte-exact retail.
  `TransformSpiderBallForcesXZ/XY`: 85.45 -> 99.71 from that one change. The operand order
  also matters: `forces.GetX() * camXf.Get00()`, force first, or the `fmuls` operands swap.
- **`CVector3f(x, y, 0.f)` built from a literal is not folded away** where a bare
  subtraction is. `CalculateSurfaceToWorld`'s fourth argument is
  `point + CVector3f(0.f, 0.f, 0.f)`: retail loads the float 0.0 **once** and does
  `fadds` on all three components, which is only what a constructed temporary produces.
  Passing `point` scores 85.23; passing `point + CVector3f(0.f,0.f,0.f)` scores 100.
- **`CControlMapper::GetAnalogInput` is a member** read off the player (`this+5072` in r3,
  CPlayer+0x13D0 = `mControlMapper`), and the mapper *reference* is what makes the address
  hoist into r31. But which of the two shapes wins depends on the function:
  `ForwardInput`/`BallTurnInput` want **no** local (retail reloads `lwz r3,0(r30)` before
  each call - the named reference produced `addi r31,r3,5072; mr r3,r31` and scored 89.97);
  `CalculateSpiderBallAttractionSurfaceForces` and `GetSpiderBallControllerMovement` want
  **one `const CControlMapper&` per pair of calls**, and retail really does reload mPlayer
  between the forward pair and the turn pair, so it needs **two** reference locals
  (91.30 -> 98.43 -> 100 when the second was introduced). Measured, do not re-derive:
  all four call sites = `mPlayer.GetControlMapper().GetAnalogInput(...)` = **81.90**;
  one reference for all four = **91.30**; one reference for 1-2 and `mPlayer.` for 3-4 =
  **98.43**; two references = **100**.
- **An early out of `CVector2f` is `return CVector2f::Zero();`,** not `CVector2f(0.f,0.f)`.
  Retail loads the two floats of `skZeroVector__9CVector2f` through SDA21.
- **The sub-expression order in a chain of `GetAnalogInput` is fixed by the source.**
  Retail keeps *backward* (kC_Backward = 2) in f31 and subtracts *forward* from it, so the
  first call is the one that is kept and the difference is written as
  `GetAnalogInput(kC_Forward, input) - backward` immediately, not via two named locals
  (which keeps four floats live and costs an extra callee-saved f register: 87.34).
- **A `float` that must survive a call is spilled to a stack slot, not a callee-saved f
  register** - but mwcceppc decides that, not the source. See the two failures below.

## What is still blocked, and why (measured this run)

- **The flip still fails on two *unwritten* functions, unchanged by this diff**:
  `CElementGen::GetEmitterTime() const` and `fn_800CD4B8`, mwldeppc `undefined:`. 76 of the
  unit's 158 functions have no body; a carve is the only route and that is a different item.
- **Five functions are blocked by the unit's `.rodata` string pool, and one of those is a
  single instruction.** `CreateBallShadow` (99.97), `UpdateMorphBallTransitionFlash`
  (99.99), `UpdateIceBreakEffect` (99.99) and `InitializeWakeEffects` (99.73) differ from
  retail *only* in where `rs_new`'s `"\?\?(\?\?)"` lands: retail
  `addi r4,r3,0x17a`, ours `addi r4,r4,0x21f`. `GetMorphBallModel` (72.22) is entangled with
  the same pool. This is **not** a spelling problem:
  - retail's pool, read out of the DOL at `0x803A86F0` (`tools/dol_read.py`), is
    12 model/ANCS names (`SamusBallCMDL`, `SamusBallDarkCMDL`, `SamusBallLightCMDL`,
    `SamusBallLowPoly{,Dark,Light}CMDL`, `SamusBoostBall{Dark,DarkCaps,LowPoly,
    LowPolyDark,LowPolyLight}CMDL`, `SamusBallFrozenCMDL`), then `""`, then
    `SamusMultiBallANCS`, then the 6 effect names, the 6 `_DGRP` names, `"\?\?(\?\?)"`,
    `TXTR_BallFade`, `Locomotion`, `BallLight`, then the Swoosh/Jaggy/Spike names.
  - ours is missing **13 of those strings** (all the extra model names, `""`, `Locomotion`,
    `BallLight`) and starts with the wake-effect names. Every one of the missing ones is a
    literal that only `LoadMorphBallModel` (812 B, **0.49%**, still a scaffold) and
    `SelectMorphBallSounds` use.
  - so the pool cannot be re-ordered without writing `LoadMorphBallModel`, and the
    *relative* order of the names we already have is identical in both, which says the
    compiler emits literals in first-use order in *its own* processing order. **WALL on the
    easy route, not on the function**: implement `LoadMorphBallModel` and re-measure.
- **`InitializeWakeEffects` also needs `fn_800C084C` out of line.** Retail calls
  `bl fn_800C084C` where we call `bl resize__Q24rstl21reserved_vectorFiRCi` for
  `sWakeEffectForMaterial.resize(64, -1)`. `fn_800C084C` is in the unit's function list at
  0.00% / 116 B / 29 instructions, so it is a real out-of-line copy of retail's
  `vector<int>::resize(int)`. It has to be emitted as a *named* function or the call
  mismatch stands.
- **`ComputeMaxSpeed` 96.84% - the previous run's wall still stands**, re-measured here and
  unchanged; I did not re-try its spellings.
- **`SpinToSpeed` 99.69% - register allocation only, and not steerable.** One instruction
  pair left: retail multiplies the direction's components in the order y, z, x
  (`lfs f2,4(r31)` / `lfs f1,8(r31)` / `lfs f0,0(r31)`), we do x, y, z. Three spellings
  measured **this run**, all 99.69: `dt * (speed - angularSpeed) * direction`,
  `(speed - angularSpeed) * dt * direction`, `direction * (dt * (speed - angularSpeed))`.
  Our `operator*(const CVector3f&, const float)` walks the members in declaration order
  (`include/Kyoto/Math/CVector3f.hpp:170`), so y, z, x is not expressible through it, and
  `ApplyFriction`'s retail shape is z, x, y - two different orders in the same file, which
  is the allocator's business, not the source's.
- **`TransformSpiderBallForcesXZ`/`XY` 99.71% - the epilogue only.** The bodies match
  instruction for instruction; the last two differ in the order of `lwz r0,68(r1)` and
  `lwz r30,56(r1)`. Two spellings measured: the `x`-then-`y` order (99.71) and the
  `y`-then-`x` order (99.52). Identical in both functions, so it is not the term order.
- **`GetRenderBounds` 96.48% - one unfoldable `- 0.0f`.** Retail is
  `fmuls f2,1/255,f2` / `lfs f0,1e-05` / `fsubs f1,f2,1e-05...` i.e. `|alpha - 0.0f| <
  1e-05f` where the 0.0 is loaded from `.sdata2`; mwcceppc folds `x - 0.f` in our build in
  both the inline-expression and the named-local form. Getting the `1e-05f` epsilon and the
  `!(... < ...)` shape took it 93.88 -> 96.48; the last two instructions need a zero the
  compiler cannot fold, and I did not invent a named constant to get one.
- **`GetSpiderBallControllerMovement` 95.80%** - everything through the `SqrtF` call matches;
  what is left is the block layout of the two threshold tests. Retail
  `fcmpo 125; blt NEG` / `fcmpo -55; ble ZERO` / NEG / ZERO; mine lays the two blocks out the
  other way round. Three spellings measured: `if (angle >= 125 && angle <= -55) return 0;`
  (93.58, two `cror`s), `if (!(angle < 125) && !(angle <= -55)) return 0;` (93.58), and the
  two-separate-`if` form now in the tree (95.80).
- **`DampLinearAndAngularVelocities` 57.27% - reverted after two worse measurements.**
  Naming the world velocity as a local: **45.50** (the three components then sit in
  f29/f30/f31 and the frame grows 80 -> 128; retail spills them to the local at r1+0x18).
  That plus switching the angular half to `CAxisAngle::operator*=` (the `__amu__` call retail
  makes, which *is* reproducible): **54.48**. Both below the original, so the original
  stands and the function keeps its body.

## Files

- `include/MetroidPrime/Player/CPlayer.hpp` - `GetControlMapper()` (2 overloads, 6 lines with
  the comment). No class layout touched; `CControlMapper` is private at CPlayer+0x13D0 and
  retail passes `this+5072`.
- `src/MetroidPrime/Player/CMorphBall.cpp` - two new includes (`CCameraManager.hpp`,
  `Cameras/CGameCamera.hpp`), and 12 function bodies. Nothing under `tools/`, `config/`,
  `build/goal/` or `docs/` edited by hand. `docs/HANDOFF.md` shows as modified; that is
  `MP_GATE_DOCS_WRITE=1` inside `tools/gate.sh` rewriting the derived counts, and the
  driver discards it.
- No `configure.py`, no `splits.txt`, no `files.cmake`, no `.s`, no asm.
- The `include/MetroidPrime/Cameras/CGameCamera.hpp` accessor I first added for the camera
  transform was **removed**: retail reads `this+36` off the `CGameCamera*`, which is
  `CActor::mTransform` (CGameCamera's own `mOrigXf` is at +412), and
  `CActor::GetTransform()` already exists.

## Verified

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10485 -> 10492   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.79% fuzzy, 24.37% matched, 11.84% linked (10492 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CElementGen::GetEmitterTime() const',
            'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 75 -> 82 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-three-tweak-bodies - flip_test ...: FAIL, but the target rose;
commit it and keep the item
```

`sha1sum build/G2ME01/main.dol` and the 86 REL hashes are inside `gate.sh` and were green
(`gate.sh (includes DOL sha1, 86 RELs, ...)`). Unit `.text` fuzzy 21.47 -> 23.61.

NEW: cmorphball-loadmorphballmodel | match | MetroidPrime/Player/CMorphBall |
src/MetroidPrime/Player/CMorphBall.cpp's `.rodata` string pool is missing 13 literals that
retail has, all of them only LoadMorphBallModel (812 B, 0.49%, still a scaffold) and
SelectMorphBallSounds reference: SamusBallDarkCMDL, SamusBallLightCMDL,
SamusBallLowPolyDarkCMDL, SamusBallLowPolyLightCMDL, SamusBoostBallDarkCMDL,
SamusBoostBallDarkCapsCMDL, SamusBoostBallLowPolyCMDL, SamusBoostBallLowPolyDarkCMDL,
SamusBoostBallLowPolyLightCMDL, "", Locomotion, BallLight (and the pool is in a different
order). Because the pool is compared positionally by objdiff, one `addi` offset in
`rs_new`'s `"\?\?(\?\?)"` (retail +0x17a, ours +0x21f) keeps CreateBallShadow at 99.97%,
UpdateMorphBallTransitionFlash and UpdateIceBreakEffect at 99.99% and InitializeWakeEffects
at 99.73% - four functions one instruction from matched. Retail's pool, read with
`tools/dol_read.py 0x803A86F0`, is listed above; write LoadMorphBallModel and the pool
should follow.

NEW: cmorphball-wakeeffects-outofline-resize | match | MetroidPrime/Player/CMorphBall |
InitializeWakeEffects (99.73%) calls `bl fn_800C084C` where we call
`bl resize__Q24rstl21reserved_vectorFiRCi` for `sWakeEffectForMaterial.resize(64, -1)`.
`fn_800C084C` is in the unit's own function list, 0.00% / 116 B / 29 instructions, and is
retail's out-of-line `vector<int>::resize(int)` - a shrink loop
(`addi r5,r5,4` / `cmplw r5,r6` / `bne`) then a grow loop (`subf. r7,r6,r4` / `mtctr r7` /
`stw r0,0(r6)` / `addi r6,r6,4` / `bdnz`) and `stw r4,0(r3)` to store the new count. It has
to be emitted under that name for the call site to match, and writing it correctly would
make the function itself match.

Not committed, per the brief.

## Review rejected run 29 (2026-09-30 19:16:33Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/Player/CMorphBall.cpp:1030-1057` (`GetSpiderBallControllerMovement`) replaces a scaffold with a body that reads retail's `.sdata2` float pool one word early, so the radian→degree scale is `2.4f` (a neighbouring function's constant at `0x8041B498`) instead of `57.29578` (`0x8041B4A0`) and the four thresholds are shifted (`180/M_PIF, -35, 125, -55` instead of retail's `-35, 125, -55, 145`). The result is unsatisfiable-then-unconditional logic: the function always returns `-magnitude` and never executes the magnitude / zero dead-zone branches retail has — a definition that compiles to retail's instruction shape without doing the job, which is exactly what the 95.80% score hides. An acceptable change keeps retail's semantics: scale by `57.29578f` (or `CMath::Rad2Deg`) and write the thresholds as `-35.f / 125.f` for the magnitude window and `-55.f / 145.f` for the negative/zero window, so the constant values are the ones the SDA relocations actually point at. The same commit should also restore the `1/255` factor retail applies to the modulation alpha in `GetRenderBounds` (`:587`).

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/cmorphball-three-tweak-bodies-L2-29.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/cmorphball-three-tweak-bodies-L2-29-review1-20260930T191410.jsonl

## Fix round 1 (worker, after review rejection run 29)

Two of the three reviewer points were real; the third was already satisfied and applying it
would have introduced a bug. Everything else in the commit is untouched.

**1. `GetSpiderBallControllerMovement` - fixed (the real defect).** The previous body read the
`.sdata2` float pool at the wrong addresses. Resolved against `_SDA2_BASE_` = 0x804223C0 (i.e.
`tools/sda.py s2:<disp>`, not the bare `disp` which resolves against the r13 base and gives a
plausible wrong answer), retail `0x800CC758` loads:

| disp(r2) | address | value | `dol_read.py` |
| --- | --- | --- | --- |
| -28448 | 0x8041B4A0 | 57.29578 | radian -> degree scale |
| -28444 | 0x8041B4A4 | -35 | magnitude window |
| -28440 | 0x8041B4A8 | 125 | magnitude window |
| -28436 | 0x8041B4AC | -55 | negative/zero window |
| -28432 | 0x8041B4B0 | 145 | negative/zero window |

So the body now reads

```cpp
const float angle = 57.29578f * static_cast<float>(atan2(turnRightMinusLeft, forwardMinusBackward));
...
if (angle > -35.f && angle < 125.f) { return magnitude; }
if (angle < -55.f)                { return -magnitude; }
if (angle <= 145.f)               { return 0.f; }
return -magnitude;
```

which is what retail's branch chain actually does: `fcmpo f31,-35 / ble` then
`fcmpo f31,125 / bge` skip the window, `b` to the epilogue with f1 = the `SqrtF` result on a hit;
`fcmpo f31,-55 / blt` to the shared `fneg` tail; `fcmpo f31,145 / ble` to `lfs f1,0.f`. The old
`2.4f` + `180.f/M_PIF` made every comparison unsatisfiable and the function returned
`-magnitude` unconditionally. The stale comment above the function was corrected to match (it
named the pool address as `0x8041B498`, which is `1.2`; `2.4` is at `0x8041B49C`).

Verified in the built object: the five new pool words land at `.sdata2` offsets 0x28..0x38 in
retail's order (retail 0x8041B4A0..0x8041B4B0), and the instruction sequence is unchanged from
the pre-fix build, so the objdiff score hides this the same way it hid the defect - which is
why the reviewer's read of the semantics, not the score, was the thing to act on.

Two residual codegen differences from retail remain, both semantically inert and not worth
chasing here: MWCC inverts the `-55` test to `bge` where retail has `blt`, and it emits
`cror eq,lt,eq / bne` where retail has a single `ble` for `<= 145`. That is 2 extra instructions
(83 vs retail's 81). Writing `!(angle > 145.f)` may recover the `ble`, but it moves the source
further from retail's own comparison, so it is left alone.

**2. `GetRenderBounds` `:587` - NOT changed, because the reviewer is wrong here.**
`CColor::GetAlpha()` (`include/Kyoto/Graphics/CColor.hpp:50`) is
`CCast::ToReal32(mA) * (1.f / 255.f)` - the `1/255` factor is already applied there, and the
staged code emits retail's `fmuls f2,f0,f2` against the `1/255` word at 0x8041B38C
(`s2:-28724`, confirmed `0.003921569`). Retail has exactly **one** `fmuls` in this stretch, so
adding `* (1.f / 255.f)` at the call site does not "restore" the factor, it **squares** it:
measured on a scratch build, the expression became two `fmuls f1,f2,f1` back to back
(alpha * (1/255)^2, compared against 1e-05 instead of 1/255). That change was reverted. The
`1e-05f` tolerance at the same line is already right (0x8041B390, `s2:-28720`); the only thing
missing versus retail is its `fsubs f1,f2,f1` against 0.f, which MWCC folds away.

### Gates after this round

```
$ python3 tools/check_raw_offsets.py
  ok: 162 raw-offset site(s) in 69 file(s), all documented in raw_offsets.md
$ sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/probe_sources.sh
  probe: 754 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_symbol_names.py
  checked 505 units; 0 declared names are missing from their object
$ ./tools/decomp_build.sh
  All:  31.79% fuzzy, 24.37% matched, 11.84% linked (10492 / 28465 functions)
  main/MetroidPrime/Player/CMorphBall: 23.60% fuzzy, 12.58% matched (82 / 158 functions)
```

The `All:` line and the 10492 matched count are unmoved, as expected - a semantically correct
body that compiles to the same shape scores the same as a wrong one that does.

Not committed, per the brief.
