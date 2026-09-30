# progress-prime1-cgrapplearm

`kind: progress`, target `MetroidPrime/Player/CGrappleArm` (unit `main/MetroidPrime/Player/CGrappleArm`,
`src/MetroidPrime/Player/CGrappleArm.cpp`). The unit stays `NonMatching`; `flip_test.sh` was not run
to decide anything.

## Result

`matched_functions` **29 -> 33 of 64** for the unit (measured on `build/report.json` after
`./tools/goal_check.sh build/goal/item.json`, which prints `target rose: ... 29 -> 33 / 64`).
Whole-DOL `matched_functions` 10341 -> 10345, `linked` unchanged at 5048, `All:` fuzzy 31.43% -> 31.44%.
`./tools/goal_check.sh build/goal/item.json` exits 0 with every check green (gate.sh including the
DOL sha1, all 86 RELs, report diff, wiring, docs claims, port probe; `check_symbol_names.py`;
`decomp_build.sh`'s `All:` line; no asm added).

The four functions named in `item.json`, and where each ended up:

| function | before | after | outcome |
| --- | --- | --- | --- |
| `Render__11CGrappleArmCFRC13CStateManagerRC9CVector3fRC11CModelFlagsPC12CActorLights` | 81.93% | **100.00%** | **matched** |
| `UpdateGrappleBeam__11CGrappleArmFfRC12CTransform4fR13CStateManager` | 89.95% | 96.59% | partial |
| `DoUserAnimEvents__11CGrappleArmFR13CStateManager` | 99.06% | 99.06% | **unmoved** - see the wall below |
| `DoUserAnimEvent__11CGrappleArmFR13CStateManagerRC13CInt32POINode14EUserEventType` | 81.92% | 81.92% | **unmoved** - see the wall below |

The other three functions that reached 100% were not named in the item and were found by scanning
`build/report.json` for the closest sub-100% functions: `PreRender` 95.00%,
`RenderGrappleBeam` 97.12%, `TryInitializeStateMachine` 77.16%.

**Prime 1's source did not match unchanged anywhere.** Echoes's `CGrappleArm` is a fork, and its
function set differs enough that `prime-ref/src/MetroidPrime/Player/CGrappleArm.cpp` was only a
guide. Every change below came from reading retail's disassembly, not from copying Prime 1.

## What was measured, and what fixed it

All scores are objdiff's per-function `fuzzy_match_percent` from `build/report.json`, produced by
`ninja build/G2ME01/src/<unit>.o` + `objdiff-cli report generate`. The loop that produced them is
`.tmp/opencode/m.sh` (rebuild + print every function under 100%).

**`Render` 81.93% -> 100.00% (matched).** Retail calls `mgr.MaskUIdNumPlayers(mPlayerId)` *before*
the rain-splash branch and keeps the result in `r27` across it, then materialises the `CModelFlags`
by hand into a stack temporary (`stb r3,16(r1)` / `stb r27,17(r1)` / `sth r8,18(r1)` / `stw r0,20(r1)`).
Our source computed `armFlags` as one expression before the branch, so the whole struct was built
early. Binding just the index to a local and passing `flags.UseShaderSet(playerIndex)` at the call
site reproduces retail's order exactly, 89/89 instructions identical.

**`PreRender` 95.00% -> 100.00% (matched).** Retail emits `cmpwi` (signed); ours emitted `cmplwi`
(unsigned). `mStateFlags` is `uint`, and `if (mStateFlags != 0)` on an unsigned compares unsigned.
`static_cast< int >(mStateFlags) != 0` produces retail's signed compare. Same one-line change fixed
`RenderGrappleBeam` 97.12% -> 100.00% when combined with splitting its `||` into two `if` blocks:
retail short-circuits `!mBeamActive` with a *forward* `beq` over the rest of the body, which
`if (a == 0 || !b) return;` does not produce but two separate guards do.

**`TryInitializeStateMachine` 77.16% -> 100.00% (matched).** Retail reads `mStateMachine`'s state
pointer inline (`lwz r0,348(r3)`); ours made an indirect call to the virtual `HasState()`. The
repo already has the non-virtual fast path for exactly this reason -
`TStateMachineState<T>::HasCurrentState()` in `include/MetroidPrime/TStateMachineState.hpp:83`.
Using it here (and in `ResetStateMachine`, 86.33% -> 97.28%) removes the virtual call. Note this is
a **header already in the tree**; no header was changed and no class layout was changed.

**`SetStateFlags` 40.38% -> 87.88%.** Our source was missing a whole branch. Retail dispatches on
`flags` first (`cmpwi r4,4` / `beq` / `bge`, then `cmpwi r4,1` / `beq`) and has a `kSF_FreeLook`
case that clears `kSF_GunChanging` before the shared tail. Written as a `switch` with
`case kSF_FreeLook` and `case kSF_Default`, that reproduces retail's two-step compare exactly. The
`flags != 0` test at the end also needed `static_cast< int >(flags)` for the same signed-vs-unsigned
reason. 40.38% -> 87.88% came from: the `switch` (40.38% -> 85.58%) and the signed compare
(85.58% -> 87.88%).

**`DownAtSide` 64.82% -> 83.45%.** Retail's guard is a range test on `msg` (`cmpwi r5,2` / `bgelr` /
`cmpwi r5,0` / `bltlr`), not the `||` of two equalities our source wrote, and it is further guarded
by `mStateFlags == kSF_Default` (`clrlwi r0,r4,31` / `cmpwi r0,1` / `bnelr`) before clearing the
flag. `msg >= kStateMsg_Activate && msg <= kStateMsg_Update && mStateFlags == kSF_Default` gives
the range shape; adding the flags test took 69.82% -> 83.45%.

**`HoldingGun` 93.12% -> 95.62%.** Same range-test shape: `msg >= kStateMsg_Activate && msg <=
kStateMsg_Activate` instead of `msg == kStateMsg_Activate`. Two instruction slots remain
(`beq`/`b` vs `blt`/`bgt`).

**`WeaponChange` 82.50% -> 91.67%.** Retail emits `cmpwi r5,0` / `beq` / `blt` / `b` / `bl` - a
`switch` range check with the `blt` the `default` arm contributes. A bare `if` gives 82.50%; the
`switch` with a fallthrough-to-`default` gives 91.67%. Four slots still differ (retail is 48 B, ours
44 B); the redundant `blt` has not been reproduced.

**`UpdateGrappleBeam` 89.95% -> 96.59%.** Two independent fixes. (1) Prime 1 declares
`bool connected = false;` as the *first* statement; ours declared it after computing `beamPos`.
Moving the declaration to the top is what lets retail keep it in `r29` across the whole body
(89.95% -> 94.72%, +1 instruction: ours had an extra `li`). (2) Retail re-fetches the player
(`bl GetPlayer`) inside the `mgr.fn_80036F10()` branch instead of reusing the `player` reference
from the top; naming that second fetch `mpPlayer` reproduces it (94.72% -> 96.59%, instruction
count now matches retail's 232 exactly). Two slots remain.

## Walls (measured, not guessed)

`WALL: DoUserAnimEvents__11CGrappleArmFR13CStateManager 99.06% - 3 of 133 slots are the same three constant loads (lfs f2 / lfs f3 / lbz r9) in a different order; 7 argument spellings tried, none changed it`

The only remaining difference is the *order* of three independent loads before the second
`do_sound_event` call: retail issues `lfs f2` (0.1f), `lfs f3` (150.f), then `lbz r9`
(`CAudioSys::kMaxVolume`); ours issues the `lbz` first. Same instructions, same registers, wrong
order. Tried and measured, all **99.06%**, none helping: named `const float` locals for the two
literals; the same as file-scope `static const float`; a `const uchar maxVol` local; the literal
arguments written inline; `static_cast<float>(0.1)`; all three hoisted to named locals before the
loop; and reflowing the call across lines. The other `do_sound_event` call site in the same
function (the `kPT_Sound` one) matches at 100%, and it has the identical argument shape, so the
order is decided by something this function does earlier that has not been identified. Retail's
`kMaxVolume` byte load is `lbz r9,0(0)` with no relocation difference - it is a real load, not a
pool artefact.

`DoUserAnimEvent` 81.92% - not reached a wall by measurement, but the size gap is structural and
larger than a spelling fix: retail 588 B / 147 instructions, ours 628 B / 157, with the extra
instructions all in the `rs_new CElementGen` sequence. Retail reuses `r28` for the freshly
allocated generator and stores `r29` back; ours keeps a second live pointer and calls
`SetParticleEmission` through a different receiver. This needs the real shape of retail's
`rs_new` expansion, not a re-spelling.

`WALL: WeaponChange__11CGrappleArmFR13CStateManagerif 91.67% - retail's 48 B has a `blt` we cannot reproduce; 9 spellings tried (bare if, early return, both range forms, 3 switch shapes), best 91.67%`

Also measured and left alone, with the spelling that got there:

- `AcceptScriptMsg` 73.31% - retail calls `fn_80036F10()` twice and ours has the same two
  relocations, but retail keeps the `GetPlayer` result in `r3`/`r6` and ours re-loads. Only
  register allocation. Tried: binding the message to a local (does not compile - `EScriptMsg`
  is not the right type name), early return, `const uint` local, an explicit `static_cast<uint>` on
  the second call to defeat CSE, hoisting `fn_80036F10()` to a named `const bool` (68.31%, worse),
  and a `CPlayer&` local. All <= 73.31%.
- `UpdateGrappleModel` 72.99% - a single extra `stw` spill at instruction 9. Tried folding the
  `name` temp into the ternary and hoisting it above the `if`; both 72.99% / 71.10%.
- `UpdateSwingAction` 94.86% - only a stack-frame size difference (retail `-160(r1)`, ours
  `-176(r1)`), which follows from the `PlaySfxForPlayer` argument setup. Tried binding `player` to
  a reference and to a pointer before the call; both 91.97%, worse than doing nothing.
- `AnimOver` 75.36% - the only difference is the merged string pool: retail's `"Whole Body"` is at
  `lbl_803AA7B4 + 162` (a link-time label in a *global* pool, `config/G2ME01/symbols.txt:17309`),
  ours puts the same string in this unit's own `.rodata` at offset 159. This is a data-layout
  difference, not a code one, and it is not reachable from this unit's source.
- `ResetStateMachine` 97.28% - same global string pool (`+56` vs `+88`), plus a `lis`/`addi` shuffle
  in the `strcmp` argument setup. Five spellings tried (De Morgan early return, split if/else-if,
  a `const char*` ternary, empty-block `if/else`); all 97.28% except the split form, which
  collapsed to 59.63%.
- `GrappleBeamConnected` 62.67% - 30 instructions on each side, so this is pure register
  allocation, not structure. Not attempted beyond reading it.
- `UpdateGrappleBeamFX` 54.93%, `BuildBeamDependencyList` 65.43%, `Update` 68.88%,
  `__ct__` 91.58% - not attempted; the first two are the largest remaining real gaps.

**The 14 `fn_*` functions at 0.00% are not in our object at all** and were not attempted.
`side.py` reports "ours has no `<name>`" for each. They are the STL instantiations and the
`TStateMachineState` vtable glue: `destroy_impl<rstl::vector<CToken>>`, `Free(CMemory*, void*)`,
and the `rstl::set` lookup in `fn_801C5E54` (196 B of `cntlzw`/`slw`/`srwi`, i.e. a tree walk).
Giving them names so they pair up is `symbols.txt` work, not a source fix, and the item is a
source-scoped progress item.

## NEW

None. Every remaining gap is either a measured wall with the spellings listed above or a
data-layout difference outside this unit. `NEW:` is for work whose success raises a count, and
none of these qualify as a bounded unit of work I could hand over: the string-pool offset and the
STL instantiations are project-wide, not one unit.

One thing the next run should know, since it cost a rebuild and nearly a bad commit: while trying
spellings I replaced a source region with a marker and a helper script's restore dropped a *whole
`switch` block* in `UpdateSwingAction` without the build failing. `git diff` caught it (a large
unexpected `-` hunk), not the compiler and not `goal_check.sh`, which passed with the function
gutted. Check the diff for deleted logic before committing, not just for added `asm`.

---

# Run 3 (lane 3, 2026-10-01) - re-measured, +4 functions

Started from the run-2 state, which the notes above describe; I re-measured it on this tree first
and it was exactly as recorded: `matched_functions` **33 / 64**, unit fuzzy 78.10%. The 14 `fn_*`
functions are still 0.00% and still absent from our object, and every "not attempted" function from
run 2 is still at the same score, so nothing below is a re-derivation.

## Result

`matched_functions` **33 -> 37 of 64** for the unit, whole-DOL **11316 -> 11320**, unit fuzzy
78.10% -> 78.39%. `./tools/goal_check.sh build/goal/item.json` exits 0, verdict
`PASS progress-prime1-cgrapplearm` (gate.sh with the DOL sha1, all 86 RELs, report diff, wiring,
docs claims, port probe; `check_symbol_names.py`; `All: 32.57% fuzzy, 25.23% matched`; target rose
33 -> 37 / 64; no asm added).

| function | before | after | spelling that did it |
| --- | --- | --- | --- |
| `DownAtSide` | 83.45% | **100.00%** | two changes, see below |
| `WeaponChange` | 91.67% | **100.00%** | inverted `if` with the range test inside the negative arm |
| `HoldingGun` | 95.62% | **100.00%** | one-case `switch` instead of a range test |
| `SetStateFlags` | 87.88% | **100.00%** | ternary instead of `if`/`else` |
| `Update` | 68.88% | 70.39% | side effect of the signed `mStateFlags` |

All four are 0 differing instruction slots out of 11/12/16/26 against the retail object. The three
functions `item.json` named (`UpdateGrappleBeam`, `DoUserAnimEvents`, `DoUserAnimEvent`) are
untouched - they are the run-2 walls and I did not spend this run re-trying them.

## The three findings worth keeping

**1. `(flags & k) == k` on a `uint` member is a *masked compare*, and only if the member is signed.**
This is the single biggest lever in the unit and it was invisible until now. Retail's
`DownAtSide` tail is `clrlwi r0,r4,31` / `cmpwi r0,1` / `bnelr`, i.e. `(mStateFlags & 0x7FFFFFFF)
== 1`. mwcceppc emits exactly that for `(x & k) == k` when `x` is **signed** - the same idiom
`CPlayerGun.cpp:1045` already uses (`(mFidgetAnimBits & 1) == 1`, and that member is declared `int`
at `CPlayerGun.hpp:288` with the comment "signed: retail tests it with cmpwi"). With `uint` it emits
a bare `cmplwi` instead. So the fix is one word in the header:

    -  uint mStateFlags;
    +  int mStateFlags; // signed: retail masks the sign bit before comparing it, in DownAtSide

**This is a header change, so it is not free - check what it moves.** `CGrappleArm.hpp` is included
by `CPlayerGun.cpp` and three `CGrappleArm*.cpp` files. Measured after touching all five: the only
*other* effect in the whole DOL is `CPlayerGun::DrawArm` 83.31% -> 81.83%, a drop of 1.48 points in
a unit that is `NonMatching` in the baseline, which `tools/report_diff.py` reports as `WORSE` but
does not fail (exit 0: "a percentage on a NonMatching unit is a signal, not a result"). It is not
caused by the signedness: I reverted `mStateFlags` to `uint`, rebuilt `CPlayerGun.o` alone and
measured **81.83% again**, i.e. the drop comes from the object being stale in the build dir, not
from my edit. `Update` in the target unit gains 1.51 points from the same change. **A future run
that edits this header should re-verify `DrawArm` rather than assume it.**

**2. mwcc's `if`/`else` and `switch` shapes are not interchangeable, and the *order* of the source's
two tests decides the order of the branches.** Three separate functions wanted the same thing - a
body guarded by `msg`-is-in-a-set - and each needed a different spelling:

- `HoldingGun` (retail `cmpwi r5,0` / `beq body` / `b end`): a `switch` with one case. The
  two-sided range test `msg >= k && msg <= k` gives `blt`/`bgt` (95.62%), and a plain
  `msg == k` gives `bne` + `beq` (93.12%). Only the `switch` gives `beq` + `b`.
- `WeaponChange` (retail `cmpwi r5,0` / `beq call` / `blt end` / `b end`): one compare, branched on
  **twice**. Needs the equality test *first* and a second test on the same value *after* it, i.e.
  `if (msg != kStateMsg_Activate) { if (msg >= kStateMsg_Activate) { return; } } else { EnterIdle(mgr); }`.
  This is counter-intuitive - the code reads as "if not activate and not below activate, return"
  and the `return` looks redundant - but it is the only spelling that keeps both branches. Measured:
  12 more spellings, all worse. `if (msg < k) return;` + `switch` gives 98.33% (right branches,
  swapped order: ours `blt` then `beq`, retail `beq` then `blt`); adding the `switch` *inside*
  `if (msg != k)` gives 99.58% (ours `bge` where retail has `blt`); the winning form has the
  nested `if (msg >= k) return;` inside `if (msg != k)`. A bare `if (msg == k) {...} else if
  (msg < k) {}` collapses to 82.50% - an empty else-arm is deleted before codegen.
- `DownAtSide` (retail `cmpwi r5,2` / `bgelr` / `cmpwi r5,0` / `bltlr`): the **upper** bound is
  tested first, so the source must be `msg < kStateMsg_Deactivate && msg >= kStateMsg_Activate`
  (note `kStateMsg_Deactivate == 2`; the previous run's `msg <= kStateMsg_Update` put the `1` in
  the wrong slot and scored 80.45%). The two comparisons cannot be merged into one
  `cmplwi`-style range check: `if (msg >= lo && msg <= hi)` gives `blt`/`bgt`.

**3. A ternary where retail reuses an argument register instead of emitting a zero.**
`SetStateFlags` 87.88% -> 100%: retail's tail is `cmpwi r4,0` / `beq skip` / `ori r0,r4,1` /
`or r4,r0,r6` / `stw r4,664(r3)` - on the `flags == 0` path it stores **r4 itself**, not a
materialised zero. `if (cond) { mStateFlags = a | b | c; } else { mStateFlags = 0; }` emits
`li r0,0` / `stw r0,664(r3)` and is 12 bytes longer. `mStateFlags = cond ? (a | b | c) : flags;`
makes the false arm store the parameter, which is the same value (the arm is only reached when
`flags == 0`) and lets mwcc drop the `li`. The `or r4,r0,r6` (rather than `or r0,r0,r6`) falls out
of the same change. Not a semantics change: on that path `flags` is 0 by the guard.

## Dead ends measured this run (do not repeat)

All of these are for `DownAtSide`'s flag test, all measured, none above 99.55%:

- `static_cast<int>(mStateFlags) == kSF_Default` -> 94.55% (plain `cmpwi`, no mask)
- `mStateFlags == kSF_Default` (plain unsigned compare) -> 84.55%
- `static_cast<int>(mStateFlags & 0x7FFFFFFFu) == kSF_Default` -> **99.55%**, one slot out: the
  mask is `clrlwi r0,r4,1` (bit 1) where retail has `clrlwi r0,r4,31` (bit 31). Very close and
  still wrong; the header change gets the right mask.
- `(mStateFlags & kSF_Default) == 1` and `== static_cast<uint>(kSF_Default)` -> 94.55%
- `(static_cast<int>(mStateFlags) & 1) == 1` -> 94.55%; hoisting into a local -> 24.55%;
  two range tests `>= 1 && <= 1` -> 87.73%; `(mStateFlags & 0x7FFFFFFFu) == 1u` (unsigned) -> 94.09%
- `static_cast<int>(mStateFlags) >= 0 && ...` / `!(static_cast<int>(mStateFlags) < 0) && ...` -> 71.27%
- `IsActive() { return mStateFlags != 0u; }` in the header: no effect on `DrawArm` either way
  (81.83% with either spelling). Reverted to the tree's `!= 0`.

## Carried over from run 2, still unverified by me

I did not re-attempt `UpdateGrappleBeam` (96.59%), `DoUserAnimEvents` (99.06%), `DoUserAnimEvent`
(81.92%), `AcceptScriptMsg` (73.31%), `UpdateGrappleModel` (72.99%), `UpdateSwingAction` (94.86%),
`GrappleBeamConnected` (62.67%), `ResetStateMachine` (97.28%) or `AnimOver` (75.36%) - all still at
the run-2 scores. Run 2's walls stand as recorded, with one addition: **`mStateFlags` being signed
is a fact about the header now, not a hypothesis**, and any function that tests it against a
constant may have the same masked-compare opportunity (`HoldGun`, `GunChanging`, `AnimOver`,
`FidgetActive`, `GrappleActive` are all at 100% already, so this is spent).

## NEW

None filed. The remaining gaps in this unit are the run-2 walls plus four large functions
(`UpdateGrappleBeamFX` 54.93%, `BuildBeamDependencyList` 65.43%, `UpdateGrappleModel` 72.99%,
`Update` 70.39%) that are not one bounded unit of work I could hand over with a checkable target,
and the 14 `fn_*` STL instantiations, which need `symbols.txt` work outside a source-scoped
progress item.

One process note, since it cost a rebuild: the fast loop is `.tmp/opencode/m.sh` (rebuild one
object, print every function under 100%) and `.tmp/opencode/side.py` (side-by-side instruction
diff for one function). `m.sh` runs in well under a second on a warm tree, so a spelling sweep of
10+ candidates costs about a minute - the run-2 walls were expensive mostly because each spelling
was tried by hand.
