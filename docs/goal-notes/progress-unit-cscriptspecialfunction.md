# progress-unit-cscriptspecialfunction

Target `MetroidPrime/ScriptObjects/CScriptSpecialFunction`, `kind: progress`.
**11 -> 33 of 124 matched functions; `goal_check.sh build/goal/item.json` -> PASS** (run in the
lane worktree, against the judge's own baseline). Unit stays `NonMatching`; `flip_test` not run,
by the item's instruction.

## What changed

Only `src/` and `include/`, three files:

- `src/MetroidPrime/ScriptObjects/CScriptSpecialFunction.cpp` - 21 functions filled in.
- `include/MetroidPrime/ScriptObjects/CScriptSpecialFunction.hpp` - `ESpecialFunction` gained the
  four values the dispatch tables branch on (36, 61, 0x10000, 0x10004).
- `include/MetroidPrime/CStateManager.hpp` + `src/MetroidPrime/CStateManager.cpp` - the
  `EStateManagerTransition` enum's names were shifted by one against retail, and the guessed
  `mCinematicPause` sat on the wrong bitfield. Both corrected (details and evidence below), plus
  one new accessor `SetSkipCinematicReceiver`.

## Per-function results (before % -> after %)

All 22 at 100% were previously below it. Measured with
`tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptSpecialFunction` after each edit; the
before-numbers are this item's `reason` and my own re-measure at the start.

| function | before | after | what it does |
|---|---|---|---|
| `AcceptPauseGame` | 7.14 | 100 | `kSM_Action` -> `DeferStateTransition(kSMT_PauseGame)` |
| `AcceptLogbook` | 7.14 | 100 | `kSM_Action` -> `DeferStateTransition(kSMT_LogBook)` (needed the enum fix) |
| `ResolvePlayerIndex` | 93.64 | 100 | `GetObjectById` (const) + `TCastToConstPtr`, early `return` |
| `AcceptFogPlane` | 5.88 | 100 | `switch` on Increment/Decrement -> `mIntParm1 = 1 / 0` |
| `AcceptAreaOcclusion` | 12.50 | 100 | `switch` on `kSM_XCRT` -> `mIntParm1 = 0` |
| `AcceptMultiplayerEndConditions` | 11.11 | 100 | `switch` on `kSM_XCRT` -> both int parms 0 |
| `AcceptChaffTarget` | 6.25 | 100 | `switch` on `kSM_XALD` -> `AddMaterial(kMT_Target, mgr)` |
| `fn_80107458` | 4.76 | 100 | `switch` Increment/Decrement -> `mgr.SetCinematicPause(true/false)` |
| `SkipCinematic` | 4.76 | 100 | `SendScriptMsgs(kSS_Zero, mgr)` + clear `m_uid_setBySpecialFunc` |
| `PreRenderPlayerFrustumTester` | 5.00 | 100 | unsigned compare of `mIntParm2` vs `GetCurrentRenderPlayerIndex()` |
| `ThinkPlayerFrustumTester` | 4.00 | 100 | unsigned compare vs `GetNumPlayers()`, copy the player's translation |
| `SendFrustumMessages` | 99.95 | 100 | see "outgoing stack slot" below |
| `AcceptHUDFadeIn` | 5.88 | 100 | `kSM_Action` -> `GetPlayer(0)->SetHudDisable(mValue1, 0.f, 1.f)` |
| `AcceptEscapeSequence` | 5.56 | 100 | `kSM_Action && mValue1 >= 0.f` -> `mgr.fn_80038370(mValue1)` |
| `AcceptEnvFxDensity` | 5.26 | 100 | `kSM_Action` -> `EnvFxManager()->FadeDensity(mValue1, (int)mValue2)` |
| `AcceptEnergyTank` | 4.35 | 100 | `kSM_Action` -> `ObjectById(mLastOriginatorPlayer)`, cast to CPlayer, `IncrPickUp(kIT_EnergyTanks, 1)` |
| `AcceptEndGame` | 4.00 | 100 | `kSM_Action` -> `const CGameState::GetGameMode()` then `EndGame(mIntParm1, mgr)` |
| `AddToRenderer` | 4.55 | 100 | `GetActive()` then `switch` on FogVolume / Silhouette |
| `PreRender` | 2.70 | 100 | `GetActive()` then a 5-way `switch` (see case order below) |
| `AcceptStopRezbitState` | 6.25 | 100 | `GetActive()` + message-tag compare -> `GetPlayer(0)->StopRezbitState(mgr)` |
| `AcceptMissileStation` | 3.85 | 100 | `kSM_Action && !fn_80036F10()` -> `ResetAndIncrPickUp(kIT_Missile, GetItemCapacity(...))` |
| `AcceptPowerBombStation` | 3.85 | 100 | same with `kIT_Powerbomb` |

`PreRenderBillboard` went 6.67 -> **89.27** and is not in the table: its retail body tests **bit 31**
of the word at 0x17c (`lwz r0,380(r3); clrlwi. r0,r0,31`) and I could not find the C++ that makes
MWCC emit that for an `int` member. A probe over `x & 0x80000000`, `x < 0`, `if (x)`, `x >= 0` and
`x == -1` produces `clrrwi` only for the first, and the outgoing `TUniqueId` argument also needs a
lvalue (a by-value `GetUniqueId()` adds a temp store the retail code does not have). Left at 89.27
rather than guess; the two open pieces are spelled out above.

## Codegen rules found (the ones that cost time)

**`switch`, not `if`, for a four-character-tag message compare.** Retail builds the tag with
`lis`/`addi`/`cmpw` (e.g. `lis r4,22595; lwz r5,8(r5); addi r0,r4,21076; cmpw r5,r0`). The
equivalent `if (msg.GetMessage() == kSM_XCRT)` emits `addis r0,rX,-22595; cmplwi r0,21076`, which
fuzzy-matches almost perfectly and still does not count. Spelling the same body as a `switch` on
`msg.GetMessage()` produces the retail sequence exactly. This is what unlocked `AcceptAreaOcclusion`,
`AcceptMultiplayerEndConditions`, `AcceptChaffTarget`, `fn_80107458` and `AcceptFogPlane`.
Spellings tried and rejected first: `if`, `!=` + early return, `(uint)` cast on the value,
`kSM_XCRT == message` with a hoisted local, `switch` with an explicit `default: break;`.

**The comparison is on `GetMessage()` (offset 8), not `GetState()`.** `AcceptStopRezbitState`
looked like a state compare but is a message compare against the tag `0x5a45524f`. That value is
this repo's `kSS_Zero`; it is used here as a message, so the call casts rather than inventing a
name. Compare against `kSS_*` tags as messages elsewhere in this unit with that in mind.

**`SendScriptMsgs` needs its trailing arguments left to their defaults** for retail's shared
outgoing slot. `SendFrustumMessages` was stuck at 99.95% with an explicit
`SendScriptMsgs(kSS_Entered, mgr, kInvalidUniqueId, kSM_None)`: retail passes both call sites the
*same* slot at `r1+8`, we gave each its own. `CEntity.hpp` already documents why - the defaulted
spelling makes MWCC fill the arguments in after the frame layout. Dropping the two arguments took
it to 100%.

**`switch` case order is the branch order, and it is not the order of the case values.** `PreRender`
came out 0.00% with cases listed low-to-high; retail's tree tests 0x10000 first (`bge` guard),
then 9, then 36, then 0x10004, then 61. Listing the cases in that order matched it.

**Signedness matters.** `PreRenderPlayerFrustumTester` and `ThinkPlayerFrustumTester` were 97%
with `==` and `<`; retail uses `cmplw`/`cmplw+bge` (unsigned). `static_cast<uint>` on the
operands is what produces it.

**Const-ness of the callee is part of the symbol.** `ResolvePlayerIndex` calls retail's *const*
`GetObjectById__13CStateManagerCF9TUniqueId`, so the call needs `TCastToConstPtr`. `AcceptEndGame`
likewise calls the *const* `GetGameMode__10CGameStateCFv` and then a non-const virtual through the
result - written as a `const CGameState&` plus a `const_cast` on the returned mode.

**`mgr.PlayerState(0)` is `0x150c`; `mgr.GetPlayerState()` is a different member** at `0x1514`.
`AcceptMissileStation`/`AcceptPowerBombStation` load `5388(r31)` = `m_playerStates[0]`, so
`PlayerState(0)`, not `GetPlayerState()` and not index 1.

## Header corrections, with the evidence

**`EStateManagerTransition` names were off by one from 3 upward.** Retail values, each read off a
distinct caller or branch, none inferred:

- 2 - `AcceptPauseGame` (0x80107388) passes `li r4,2` to `DeferStateTransition`.
- 4 - `AcceptLogbook` (0x80107db4) passes `li r4,4`.
- 5 - `AcceptSaveStation` (0x801088a4) passes `li r4,5`, **and** `DeferStateTransition` itself
  branches on exactly 5 to allocate the `CSaveGameScreen` (`cmpwi r0,5` at 0x8003789c). Two
  independent confirmations.
- 6 - `ShowPausedHUDMemo` (0x80037810) passes `li r4,6`.
- 1 - `AcceptMapStation` (0x80108afc) passes `li r4,1`.

The header had `kSMT_LogBook=3, kSMT_SaveGame=4, kSMT_Unk=5`, so `AcceptLogbook` emitted
`li r4,3` and could not match, and `CStateManager::DeferStateTransition`'s `== kSMT_Unk` was naming
the wrong flag for a comparison it had right. Fixed by inserting `kSMT_Unk` at 3 and updating the
one `DeferStateTransition` use. **Measured effect on that function: 98.18% before and after - not
made worse, not improved; its 98.18% is a pre-existing gap unrelated to the enum.**

**`mCinematicPause` was on the wrong bit of the 0x294c flag word.** Retail's `fn_80107458` sets and
clears **bit 26** (`rlwimi r0,r3,5,26,26`), the third 1-bit field of that byte; the guessed name
sat on the sixth. I moved the *name* to the third field and shifted the unnamed flags up one
(`m_unkFlagA3` -> dropped, `m_unkFlagA4`..`m_unkFlagA7` -> one higher). **No bit moved**: bit 28 is
still the fifth field, which `KillSaveGameInterface` (0x80037758, `rlwimi r0,r3,30,28,28`) writes
and which still matches 100%. Verified by measurement, not by reading - the first attempt moved
`mCinematicPause` to the third *and* renamed the fifth, and `KillSaveGameInterface` fell to 99.32%,
which is how the "positions are fixed" comment in the header came to say so.

## One measured side effect, and why it is worth it

`main/MetroidPrime/ScriptObjects/CScriptCamera :: AcceptScriptMsg__13CScriptCamera` went
**36.16% -> 36.10%**. Confirmed by measurement to be caused by the bitfield reorder: stashing only
`include/MetroidPrime/CStateManager.hpp` restores it to 36.163864. It is the one function in 28465
that moved, `report_diff.py` reports it as WORSE, and it does **not** fail the gate -
`CScriptCamera` is `NonMatching`, and `report_diff.py` only fails a drop inside a unit that was
`Matching` in the baseline. Recorded here rather than hidden because a reviewer should see it: the
trade is 0.06% on one function of a `NonMatching` unit for a real +100% function.

## Verification

`./tools/goal_check.sh build/goal/item.json` in the worktree:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11533 -> 11555   linked 5625 -> 5625
  ok    check_symbol_names.py
  ok    All:  33.13% fuzzy, 25.97% matched, 12.24% linked (11555 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptSpecialFunction: 11 -> 33 / 124 functions
  ok    no asm added
goal_check: PASS progress-unit-cscriptspecialfunction
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `report_diff.py`
over the whole tree: +22 functions at 100%, 1 drop (above), 0 units newly linked, exit 0. No `asm`
added. No files under `tools/`, `docs/research/port_link_baseline.txt` or `build/goal/` touched;
`docs/HANDOFF.md` is reverted (the driver rewrites its derived counts).

## Still unmatched, cheapest first (the next run's starting point)

- `__dt__11TAverage<f>Fv` - **99.95%**, 84 B, one instruction: retail passes `li r4,-1` to
  `__dt__rstl::vector<f>::~vector()`, we pass `li r4,0`. That is the vector destructor flag
  (`include/rstl/vector.hpp` line 130, `~vector()` is `inline` with no flag parameter). Every other
  instruction matches. **Do not chase this from this unit** - it is a `rstl::vector` template
  question whose answer probably moves other units.
- `__ct__22CScriptSpecialFunctionF...` - **91.52%**, 856 B, the constructor. Retail's store
  sequence at 0x80109b28-0x80109bfc is fully dumped in this repo's disassembly; the member order
  after `mVectorParm` is the part still to reconcile.
- `PreRenderBillboard` - **89.27%**, 60 B. Bit-31 test and the `TUniqueId` lvalue; described above.
- The `Accept*` handlers that all still hold a TODO and sit at 1-4%: `AcceptHUDTarget`,
  `AcceptViewFrustumTester`, `fn_80107a58`, `ThinkSilhouette`, `ThinkSkyboxLighting`,
  `AcceptCinematicSkip`, `AcceptDarkWorld`, `AcceptAreaDocks`, and the `Think*` family from
  `ThinkChaffTarget` down. Each is 3-6 instructions in retail and follows the patterns above:
  `GetActive()` is `lbz 32(r3)` + `rlwinm. r0,r0,25,31,31`; the tag compares are `if` when there
  is one case and `switch` when the branch shape is a tree.
- `AddOrUpdateEmitter`, `RenderFogVolume`, `RenderSilhouette`, `RenderBillboard`, `Think`,
  `ThinkSpinnerController`, `AcceptScriptMsg` (2272 B, the whole dispatch table) are the large
  ones. `AcceptScriptMsg` is what unlocks most of the rest - it is the function that names every
  `Accept*` above, so reading it tells the next run what all of them do.

No `NEW:` lines: everything found here is either fixed in this change or is a wall/spelling that
belongs in this file, and the remaining items are ordinary decompilation of a unit that is already
queued.