# cmorphball-touchradius-accessors (match, `MetroidPrime/Player/CMorphBall`)

Lane 2, 2026-09-30. **Result: `goal_check` PARTIAL** - the unit's matched count rose **73 -> 75 of
158**, every other check green, and the flip still fails for the reason the last three runs
measured (unwritten functions of the unit), not for anything in this diff.

The item as filed by `cmorphball-three-tweak-bodies` is done: both bodies are in the tree at
100.00% and the port's undefined count did not move.

## What I changed

Two files, both in `src/`, four lines of real code plus their comments.

**`src/MetroidPrime/PortCTweakBall.cpp` (80, 82).** The two `CTweakBall` accessors the callers
need, added to the existing host stand-in:

```cpp
float CTweakBall::GetMinimumAlignmentSpeed() const { return mData->movement.minimumAlignmentSpeed; }
float CTweakBall::GetBallTouchRadius() const { return mData->misc.ballTouchRadius; }
```

Character for character `src/MetroidPrime/Tweaks/CTweakBall.cpp:266` and `:290`. Each reads exactly
the field it always read; nothing is stubbed and nothing is skipped. The file's existing header
already explains why these live in a `Port*.cpp` rather than in the unit that decompiles them
(`CTweakBall.cpp` is in `tools/check_files_cmake.py`'s `EXCLUDED` list and `tools/` is the judge's),
and it already records the duplication rule: **the two files must never be compiled together.**

**`src/MetroidPrime/Player/CMorphBall.cpp` (716-721, 1099).** The two scaffolds written out:

```cpp
float CMorphBall::GetMinimumAlignmentSpeed() const {
  if (mBallState == kBS_Spider) {
    return 0.f;
  }
  return gpTweakBall->GetMinimumAlignmentSpeed();
}

float CMorphBall::GetBallTouchRadius() const { return gpTweakBall->GetBallTouchRadius(); }
```

Both shapes are read off retail's disassembly, not inferred from the name:

```
$ ./build/binutils/powerpc-eabi-objdump -d --start-address=0x800ce9a4 --stop-address=0x800ce9c8 \
      build/G2ME01/main.elf
800ce9a4 <GetBallTouchRadius__10CMorphBallCFv>:
800ce9a4: stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / lwz r3,-28244(r13)
800ce9b4: bl   802170a4 <GetBallTouchRadius__10CTweakBallCFv>
800ce9b8: lwz  r0,20(r1) / mtlr r0 / addi r1,r1,16 / blr

$ ./build/binutils/powerpc-eabi-objdump -d --start-address=0x800c55dc --stop-address=0x800c5614 \
      build/G2ME01/main.elf
800c55dc <GetMinimumAlignmentSpeed__10CMorphBallCFv>:
800c55e8: lwz  r0,3200(r3)        # this+0xC80 = mBallState
800c55ec: cmpwi r0,2              # kBS_Spider
800c55f0: bne   800c55fc
800c55f4: lfs  f1,-28856(r2)      # the constant
800c55f8: b    800c5604
800c55fc: lwz  r3,-28244(r13)     # tools/sda.py -28244 = gpTweakBall
800c5600: bl   8021711c <GetMinimumAlignmentSpeed__10CTweakBallCFv>
```

`3200` is `0xC80`, the offset `mBallState` already has in this header (`GetGravityAcceleration`
reads the same field and matches), and `2` is `kBS_Spider`
(`include/MetroidPrime/Player/CMorphBall.hpp:45-52`). `tools/sda.py -28244` resolves retail's
`disp(r13)` to `gpTweakBall`; `-28856` resolves to `0x80418CC8`, the float the Spider branch
returns. The `GetBallTouchRadius` scaffold's old comment claimed the port link would grow to 251
undefined without the stand-in; that is now false, and the comment says so.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake` (the unit was already listed by
`cmorphball-three-tweak-bodies`), no `.s`, no asm. Nothing under `tools/` or `build/goal/` was
edited. `docs/HANDOFF.md` shows as modified; that is `MP_GATE_DOCS_WRITE=1` inside `tools/gate.sh`
rewriting the derived counts, and the only hunks are the `10483 -> 10485` and `8935 -> 8937` lines.

## Measured

```
$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched  10483 -> 10485   linked 5051 -> 5051   (+2 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Player/CMorphBall :: GetBallTouchRadius__10CMorphBallCFv
  +100%    main/MetroidPrime/Player/CMorphBall :: GetMinimumAlignmentSpeed__10CMorphBallCFv
no regression
```

`build/report.json`, `main/MetroidPrime/Player/CMorphBall`: `matched_functions` **73 -> 75**,
`fuzzy_match_percent` 21.355675 -> **21.473392**. `GetBallTouchRadius__10CMorphBallCFv` 36 B was
**15.555555**, now **100.0**; `GetMinimumAlignmentSpeed__10CMorphBallCFv` 56 B was **14.285714**,
now **100.0**. The `0.f` spelling for the Spider branch is what produced the second 100% - the
instruction is `lfs f1,<disp>(r2)` in both objects and only the constant-pool displacement differs.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10483 -> 10485   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.77% fuzzy, 24.35% matched, 11.84% linked (10485 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed:
              ### mwldeppc.exe Linker Error:  #   undefined: 'CElementGen::GetEmitterTime() const'
              ### mwldeppc.exe Linker Error:  #   undefined: 'fn_800CD4B8'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 73 -> 75 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-touchradius-accessors - flip_test ...: FAIL, but the target rose;
  commit it and keep the item
```

`gate.sh` printed `GATE PASS`. `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the unit is `NonMatching`, so its object is not in the
DOL link and the hash is retail's either way). `build/gate-probe.log`: `probe: 754 files, 0 failed,
0 errors; link: LINKED (250 undefined, 0 duplicates)` - **250, the baseline, unchanged**: the two
new calls are paid for by the two new definitions. `python3 tools/check_decl_order.py`:
`973 unit(s) checked, 31 permuted, all 31 accounted for` - unchanged, and this diff moves no
declaration.

`./tools/unit_fit.sh MetroidPrime/Player/CMorphBall.cpp` still reports **50 function(s) present in
ours but not in the retail unit object, 4800 bytes** (COMDAT/template copies plus retail bytes that
dtk placed in a neighbour). Unchanged by this diff; only `flip_test` decides it, and `flip_test`
fails earlier, on the link.

## What still stops the flip

**85 of the unit's 158 functions still have no body**, and the two the link names first -
`CElementGen::GetEmitterTime() const` (an 8-byte accessor this unit's split claims) and
`fn_800CD4B8` - are two of them. That is the whole blocker, and it is not a spelling problem.

## Still open in this unit, measured not guessed

- **`ComputeMaxSpeed` (152 B, 96.8421%)** - a register-allocation wall recorded in
  `docs/goal-notes/cmorphball-three-tweak-bodies.md` (eight spellings tried, all listed there).
  I did not try it this run, so I am not writing a `WALL:` line for it; the next run should try
  different spellings, not re-run that table.
- **Three functions at 99.7-99.99% that cannot reach 100% from the source** -
  `UpdateMorphBallTransitionFlash` and `UpdateIceBreakEffect` (436 B each) and `CreateBallShadow`
  (252 B) differ from retail *only* in the *name* objdiff gives the string-pool relocation target
  (`lbl_803A86F0@ha` vs `@stringBase0@ha`); both sides encode 0x803A86F0. Measured row by row in
  `docs/goal-notes/cmorphball-touchradius.md`. `InitializeWakeEffects` (532 B, 99.729%) is a real
  difference: same naming rows, plus ours calling `bl fn_800C084C` where retail calls its
  out-of-line `resize__Q24rstl21reserved_vector<i,64>FiRCi`, plus one `li r19`/`li r18` swap.
- **Six small scaffolds that are still cheap in principle** - `ForwardInput` (144 B, 5.5556%),
  `BallTurnInput` (144 B, 5.5556%), `IsMovementAllowed` (148 B, 3.7838%), `DoUserAnimEvent`
  (88 B, 6.3636%), `StartScrewAttackSfx` (132 B, 3.0303%), `StartLandingSfx` (212 B, 1.8868%). Each
  is a single `// TODO: ... return 0.f;` / `return false;` in `CMorphBall.cpp`; none of them is
  tweak-dependent, so nothing outside this file is needed to write them.
- **The judge-owned cleanup** - `src/MetroidPrime/PortCTweakBall.cpp` now holds **eight** symbols
  that `src/MetroidPrime/Tweaks/CTweakBall.cpp` also defines. Still safe (no build compiles both)
  and still the existing `Port*.cpp` pattern, but the duplication grows with every accessor added
  here. The clean fix is judge-owned: drop the `CTweakBall.cpp` entry from
  `tools/check_files_cmake.py`'s `EXCLUDED` (its reason is a 23-TU batch measurement from the
  2026-09-28 sync, never re-measured one unit at a time), list the unit, delete the stand-in.

NEW: cmorphball-control-accessors | match | MetroidPrime/Player/CMorphBall | ForwardInput (144 B, 5.56%), BallTurnInput (144 B, 5.56%), IsMovementAllowed (148 B, 3.78%), DoUserAnimEvent (88 B, 6.36%), StartScrewAttackSfx (132 B, 3.03%) and StartLandingSfx (212 B, 1.89%) are still one-line scaffolds returning 0.f/false in src/MetroidPrime/Player/CMorphBall.cpp, and none of them is tweak-dependent, so their bodies are writable from retail's disassembly with no file outside src/MetroidPrime/Player/CMorphBall.cpp

## Files

- `src/MetroidPrime/PortCTweakBall.cpp:80,82` (two accessors; file is 82 lines)
- `src/MetroidPrime/Player/CMorphBall.cpp:712-721` (`GetMinimumAlignmentSpeed`), :1093-1099
  (`GetBallTouchRadius`)

Not committed, per the brief.
