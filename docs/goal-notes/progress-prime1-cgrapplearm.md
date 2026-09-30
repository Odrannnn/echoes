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
