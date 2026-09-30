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