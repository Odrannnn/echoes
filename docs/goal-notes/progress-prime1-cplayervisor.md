# progress-prime1-cplayervisor

Item: `progress`, target `MetroidPrime/Player/CPlayerVisor`
(judged unit name `main/MetroidPrime/Player/CPlayerVisor`). Three unmatched functions were queued:
`UpdatePlayerHints__7CPlayerFR13CStateManager`, `ResetPlayerHintState__7CPlayerFR13CStateManager`,
`SetAreaPlayerHint__7CPlayerFRC17CScriptPlayerHintR13CStateManager`.

**Result: the unit's `matched_functions` went 2 -> 3 and `tools/goal_check.sh` printed
`goal_check: PASS`.** One of the three functions is matched exactly (100.00%); the other two are
unchanged and untouched. Unit stays `NonMatching`.

## Re-measured before acting (from `build/report.json`, not from `reason`)

```
main/MetroidPrime/Player/CPlayerVisor
  matched 2 / 22 functions, fuzzy 2.2410%
  already matched: GetRezbitState (100), SetRezbitState (100)
  the three queued ones before: ResetPlayerHintState 1.30, SetAreaPlayerHint 0.59, UpdatePlayerHints 0.44
```

## What I did

### 1. `ResetPlayerHintState__7CPlayerFR13CStateManager` — 1.30% -> **100.00%**

Prime 1's `CPlayerDynamics.cpp` version was **not** usable unchanged, and the reason is worth
recording because it is a general trap: Echoes moved these three functions out of Prime 1's
`CPlayerDynamics.cpp` and into the **CPlayerVisor** unit, and in the fork they are not the
hint-list bookkeeping Prime 1 has. Read off `tools/dis.sh 0x8022BE74 0x134`:

- Prime 1's list-of-hints work (`mPlayerHints`, `mPlayerHintsToRemove`, `mPlayerHintsToAdd`,
  `rstl::sort`, `mPlayerHint`/`mPlayerHintPriority`) is **gone entirely**. Echoes delegates the
  hint list to `CHintManager` (it exists here at `include/MetroidPrime/CHintManager.hpp`), and
  `CPlayer` only carries `CHintManager* mControlHintManager` (0x14b8) + `TUniqueId x14bc_` (0x14bc).
  So Prime 1's `UpdatePlayerHints` body has no counterpart here at all.
- What Echoes actually does: 12 bit read-modify-writes on the four flag bytes at
  0x1268-0x126b, `mMorphBall->SetBoostEnabled(true)`, `ResetControlDirectionInterpolation()`,
  `RemoveMaterial(43 /* kMT_Immovable */, mgr)`, then
  `if (mControlHintManager && x14bc_ != kInvalidUniqueId) mControlHintManager->RemoveHint(x14bc_, GetUniqueId(), mgr);`
  (`kInvalidUniqueId` confirmed via `tools/sda.py`: `lhz r0,-27740(r13)` = 0x80419124 =
  `kInvalidUniqueId` in `.sbss`).
- The 12 bits map to Prime 1's flags in a different order, so Prime 1's *statement order* does
  not reproduce retail either. Retail emits one `lbz`/`rlwimi`/`stb` per bit, interleaved across
  all four bytes (0x1268:26,27,28,30, then 0x1269:28,30, then 0x1268:29, then 0x126a:31, 0x1269:31,
  0x126a:24,25, then 0x126b:31). Written in that order it matches on the first try; grouping the
  stores by byte, or writing them in Prime 1's order, does not (they are separate byte objects, so
  each is its own load-modify-store and the *sequence* of the stores is what differs).
- Flag-byte members are written through this header's offset names (`x1268_26_` etc.). I did not
  rename them: the header's names are load-bearing for other units and renaming 40+ members is a
  separate job.

### 2. `CPlayer`'s byte at 0x126a is eight bools, not a `uchar` (needed to make the above compile)

`include/MetroidPrime/Player/CPlayer.hpp` had `uchar x126a_;` where the neighbours are eight
`bool x126b_N_ : 1`. **Retail read-modify-writes three separate bits in that byte**
(`0x126a`: bit 31, bit 24, bit 25, at `0x8022BEFC`, `0x8022BEF0`, `0x8022BEF4`) and Echoes' ctor
initialises all eight one at a time (`0x8001BA08`-`0x8001BA58`: eight `lbz`/`rlwimi`/`stb`
triples, all `false`). So it is eight `bool : 1`, changed to
`x126a_24_` .. `x126a_31_`, and `CPlayer.cpp`'s ctor init list updated to match. Layout is
unchanged (`CHECK_SIZEOF(CPlayer, 0x14c8)` still holds; no other unit moved).
Side effect: `CPlayer`'s ctor went **41.85% -> 42.57%** — it was already emitting a plain
`stb 0` for that byte and now emits the eight retail emits.

This is a header change beyond the item's own unit, so worth stating why it was necessary: it
**was not** — `ResetPlayerHintState` can be written against a `uchar` with three mask
read-modify-writes, but that produces ~3 extra instructions per byte and will not reach 100%.
The byte is genuinely eight bools in retail, so this is the measured fix, not a guess.

### 3. Port stand-in for the one symbol my new call needed

`CHintManager::RemoveHint(TUniqueId, TUniqueId, CStateManager&)` (retail 0x801B94B8, 0xAC bytes)
is in an **unclaimed gap** of the DOL: `config/G2ME01/splits.txt` has
`Carve801B94B4.c .text 0x801B94B4..0x801B94B8` and the next claim starts after it, so no unit
owns 0x801B94B8. Calling it took the port from **250 to 251 undefined** and
`tools/link_check.sh` fails STRICT on a rise. Defined it in `src/MetroidPrime/PortGlobals.cpp`
next to the existing `CHintManager::Update` stand-in, announcing itself by name on first call and
saying in the comment that it is **not** decompilation and not claimed to match retail — the same
convention as `ReportedCameraManagerStandIn` next to it. Undefined back to 250.

## Verification

`tools/goal_check.sh build/goal/item.json` against `build/goal/judge/report.base.json`:

```
ok    no judge-owned path touched
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 10006 -> 10007   linked 4896 -> 4896
ok    check_symbol_names.py
ok    All:  30.83% fuzzy, 23.10% matched, 11.74% linked (10007 / 28465 functions)
ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 2 -> 3 / 22 functions
ok    no asm added
goal_check: PASS progress-prime1-cplayervisor
```

Whole-tree per-function comparison against a pre-change snapshot of `build/report.json`
(28465 functions, `tools/report_diff.py`'s own definition of a regression):
**0 functions worse, 2 better** — `CPlayerVisor|ResetPlayerHintState` 1.30 -> 100.00 and
`CPlayer|__ct__...` 41.85 -> 42.57. `CPlayer` unit: matched 52 -> 52, fuzzy 8.8546 -> 8.9105.

`docs/HANDOFF.md` is **not** in the diff: the gate rewrote its state block (it runs with
`MP_GATE_DOCS_WRITE=1`) and I reverted it, per the brief.

## Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `ResetPlayerHintState__7CPlayerFR13CStateManager` | 1.30% | **100.00%** | structure only; flags, hint bookkeeping and the `CHintManager` call all differ. Flag **order** differs too. |
| `SetAreaPlayerHint__7CPlayerFRC17CScriptPlayerHintR13CStateManager` | 0.59% | 0.59% | not attempted, see below |
| `UpdatePlayerHints__7CPlayerFR13CStateManager` | 0.44% | 0.44% | **no counterpart** — see below |

## What is blocked, and why I stopped

### `UpdatePlayerHints` (0x8022BFA8, 0x390 bytes) — Prime 1's version is the wrong function

Prime 1's `CPlayer::UpdatePlayerHints` (`CPlayerDynamics.cpp:1094`) prunes `mPlayerHints`, drains
`mPlayerHintsToRemove`/`ToAdd`, sorts by priority and picks the best hint. **Echoes has none of
that here.** Retail's 0x390 bytes at 0x8022BFA8 are a vector-math routine: `CanBeNormalized` /
`AsNormalized` / `Magnitude` / `Normalize` on `CVector3f`, a call to
`CTweakBall::GetBallCameraControlDistance`, a `CCameraManager::GetCurrentCamera`, and a switch on
`lwz r0,908(r31)` (0x38c) against 0/1/4. It reads and writes floats at **0x1010-0x1024**, which is
`mControlDir` (0x1010) and `mControlDirFlat` (0x101c) — measured with a `tools/probe_cc.sh`
offsetof probe, not read off the header. This is **not decompilable from Prime 1 at all**: it is
Echoes' own control-direction code that happens to sit at an address the symbol database names
`UpdatePlayerHints` because of a same-named neighbouring function in Prime 1's layout. Writing
Prime 1's body here would produce a function that matches nothing.

Do not spend a run re-deriving this from Prime 1. It has to come out of
`CVector3f`/`CTweakBall`/`CCameraManager` recovery, on its own terms.

### `SetAreaPlayerHint` (0x8022BAC0, 0x3B4 bytes) — reachable but blocked on a missing class

Worth a run, so recording what I measured. Retail's shape, all of it read off
`tools/dis.sh 0x8022BAC0 0x3B4`:

- `lwz r4,424(r4)` is `hint->mOverrideFlags` at **0x1a8** of the hint, i.e. `CGameHint` is
  0x1a8 here (`CHECK_SIZEOF(CGameHint, 0x1a8)`) and the flags live on `CScriptPlayerHint` past it.
  **`CScriptPlayerHint` is declared in this repo as only `class CScriptPlayerHint : public
  CGameHint { CEntity* TypesMatch(int) const; };`** (`src/MetroidPrime/TypesMatch.cpp:391`) — a
  placeholder, with no fields. So the body cannot be written until that class's 0x1a8+ tail
  (`mOverrideFlags`, `GetTransform()` at 0x28/0x38/0x48, and whatever 0x1b0 is) is laid out.
- Flag bits read: `rlwinm r4,r4,0,26,26` + `cntlzw` for `!(flags & 1)` (bit 26 of the word),
  `rlwimi r0,r4,5,26,26` for `flags & 0x2`, `rlwinm r0,r3,0,22,22` / `0,21,21` / `0,20,20` /
  `0,19,19` for bits 22-19 gating the four `HasPowerUp` + `StartTransitionToVisor` pairs with
  `EItemType` 8/9/10/11 and `EPlayerVisor` 0/2/3/1, `rlwimi r0,r0,0,23,23` + `cntlzw` for
  `!(flags & 0x100)` (the morph-ball boost), `rlwinm r0,r3,0,18,18` for `AddMaterial(43)`.
  These are **not** Prime 1's masks (Prime 1 uses 0x1/0x2/0x4/0x8/0x10/0x20/0x40/0x80/0x100/
  0x200-0x1000/0x4000); Echoes renumbered them, so Prime 1's constants are actively wrong here.
- The tail also calls `fn_8022af0c` (a `CHintManager` add, with a 1-byte struct written to 0x14bc),
  `CPlayerGunBase::Holster`, `SetOrbitRequest`, `EnterMorphBallState`, `LeaveMorphBallState`,
  `ActivateMorphBallCamera`, two virtual calls and
  `CPlayer::CreateTransformFromMovementDirection`. Two of those
  (`fn_801843d0`, `fn_80184ba4`) are unnamed in this header.

So `SetAreaPlayerHint` is **not** blocked by a wall — it is blocked on the `CScriptPlayerHint`
placeholder, which is a real class with a real layout to recover. Filed as a `NEW:` item.

## NEW:

NEW: progress-prime1-cscriptplayerhint | progress | MetroidPrime/CGameHint | CScriptPlayerHint is a fieldless placeholder on CGameHint; SetAreaPlayerHint (0x8022BAC0) needs mOverrideFlags at 0x1a8 plus GetTransform() at 0x28/0x38/0x48 before its body can be written.

## Files touched

- `include/MetroidPrime/Player/CPlayer.hpp` — `uchar x126a_` -> eight `bool x126a_NN_ : 1`
- `src/MetroidPrime/Player/CPlayer.cpp` — ctor init list for those eight, all `false`
- `src/MetroidPrime/Player/CPlayerVisor.cpp` — `ResetPlayerHintState` body; two includes
- `src/MetroidPrime/PortGlobals.cpp` — announced stand-in for `CHintManager::RemoveHint`

Not committed, as instructed.
## 2026-09-30 lane L9 follow-up

Re-measured this worktree before editing: `build/report.json` showed
`main/MetroidPrime/Player/CPlayerVisor` at **2/22**. The queued functions were still
`ResetPlayerHintState` 1.2987%, `SetAreaPlayerHint` 0.5907%, and `UpdatePlayerHints` 0.4386%.
The earlier run's edits were not present in this worktree. I did not repeat its Reset/SetArea
approach; instead, I found a short unimplemented helper in the same target unit that the earlier
notes did not cover.

### `fn_8022B6D0` — 0%/unmatched -> **100.00%**

Measured with `./tools/dis.sh 0x8022b6d0 0x10`: retail is exactly `li r0,0; stw r0,0x13c8(r3);
stw r0,0x13cc(r3); blr`. `objdump -d build/G2ME01/main.elf | grep -B4 -A2 '8022b6d0'` shows
`CPlayer::Freeze` passes its `CPlayer*` at 0x8001451c. The header's consecutive members at those
offsets are `mRezbitRecoveryDirection` and `mRezbitRecoveryInputCount`; a small inline
`CPlayer::ResetRezbitRecoveryState()` assigns both by name, and the C-linkage `fn_8022B6D0` wrapper
calls it. This avoids an undocumented raw-offset access. The compiled function is 16 bytes and
matches retail exactly.

The first draft used byte-pointer arithmetic and `goal_check` correctly rejected it at the
`raw-offsets` gate. Replacing it with named fields resolved that failure; no policy document was
changed.

### Verification

- `./tools/decomp_build.sh main/MetroidPrime/Player/CPlayerVisor`: `CPlayerVisor ... (3 / 22)`;
  overall matched functions **10118 -> 10119**. Report entry for `fn_8022B6D0`: **100.0%, 16 bytes**.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CPlayerVisor`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `./tools/goal_check.sh build/goal/item.json`: `goal_check: PASS progress-prime1-cplayervisor`;
  gate, no-regression report diff, symbol names, no-asm, and target rise all passed. DOL sha1:
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

The three queued functions above remain unmodified at those measured scores. No new blocker or
`NEW:` item was established in this run; no `WALL:` is warranted.

## 2026-09-30 lane L5: the seven bool input accessors

Re-measured this tree before acting. `build/report.json` showed
`main/MetroidPrime/Player/CPlayerVisor` at **3/22**, fuzzy 2.5288%. The three
functions the item queues were still at the same scores as every earlier run
(`ResetPlayerHintState` 1.2987, `SetAreaPlayerHint` 0.5907, `UpdatePlayerHints`
0.4386) — see the blockers already recorded above; I did not re-derive them.

Instead I took the seven `bool CPlayer::...(const CFinalInput&) const` helpers in
this same unit, which **no earlier run touched** and which nobody has listed.

### Result: 3/22 -> **8/22**. Five functions at exactly 100.00%, two at 99.74%.

All seven are the same shape: `CControlMapper::GetDigitalInput` or
`GetPressInput` on `mControlMapper` (CPlayer+0x13d0, `GetControlMapper()`'s
member), `EFilterType` r6=0 = `kFT_Filtered` (the default, so it is not written),
the result normalised to bool and returned. The command words are literally in
the disassembly, and all seven match this header's `ECommands` enum exactly -
**no Prime 1 lookup needed, and none used**:

| function | retail | body |
|---|---|---|
| `FireBeamHeld` | `GetDigitalInput`, `li r4,13` / `li r4,14` | `kC_FireOrBomb \|\| kC_FireOrBomb2` |
| `FireBeamPressed` | `GetPressInput`, `13`/`14` | same two, press |
| `JumpHeld` | `GetDigitalInput`, `11`/`12` | `kC_JumpOrBoost \|\| kC_JumpOrBoost2` |
| `JumpPressed` | `GetPressInput`, `11`/`12` | same two, press |
| `fn_8022b7f4` | `GetDigitalInput`, `16`/`17` | `kC_ChargeBeam \|\| kC_ChargeBeam2` |
| `fn_8022b974` | `GetDigitalInput`, `15` | `kC_Unknown15` |
| `fn_8022b7a8` | `GetDigitalInput`, `73` | `kC_Unknown73` |

The enum values 15 and 73 are `kC_Unknown15` / `kC_Unknown73` in
`include/MetroidPrime/CControlMapper.hpp` (counted out of the enum: 73 lands on
`kC_Unknown73`, one before `kC_MorphIntoBall`), so these two unnamed functions
are *named* by the measurement. **A `CControlMapper::` qualifier is required** -
the enums are class members and the unqualified spelling does not compile.

### The `!!` finding (this is the reusable part)

Written as a plain `a || b`, all five two-call functions compile to **98.12%** -
every byte correct except the return value: `mr r3,r31` where retail has
`clrlwi r3,r31,24`. Wrapping the expression in `!!(...)` reproduces retail's
mask exactly and takes all five to **100.00%** on the first try.

**Rule for this codebase: a `bool` return of an already-`bool` expression needs an
explicit `!!` to match retail.** mwcceppc 2.7 elides the conversion otherwise.
This is a codegen rule, not a wall - do not spend a run rediscovering it.

### The two single-call functions: 99.74%, one instruction short

`fn_8022b974` and `fn_8022b7a8` are down to a **single** differing instruction,
the return mask:

```
mine  0x8022b9ac  57 e3 07 fe   clrlwi r3,r31,31
retail 0x8022b9ac 57 e3 06 3e   clrlwi r3,r31,24
```

`result & 1` reaches this (99.74%) but shifts the mask field to 31; `!!` on its own
produces `,24` but then appends a redundant `neg/or/srwi` triple, because `!!` of
a `bool` local is a no-op the optimiser half-applies. `bool result = false; if
(...) result = true; return result;` gives `mr r3,r31`. Best kept is the `& 1`
form at 99.74%.

Spellings measured this run (all on `fn_8022b974`, this tree):

- 99.74%: `bool result=false; if(c) result=true; return result & 1;` and ~15
  equivalent spellings - `(result & 1) == 1`, `(result & 1) ? true : false`,
  `1 & result`, `(unsigned char)result & 1`, `int r = result; return r & 1;`,
  `!!result` after an `if/else` accumulator, `unsigned char/short/int/long`
  accumulator with `& 1` or `== 1`. All converge on `clrlwi ...,31`.
- 88.05%: `int result=0; if(c) result=1; return !!result;` (and `unsigned int`,
  `long`) - emits `neg r0,r31 / or / srwi`.
- 82.79%: `bool result=false; if(c) result=true; return !!result;` - emits
  `clrlwi r3,r31,24` (correct!) **plus** a trailing `neg/or/srwi`.
- 69.95%: `return !!c;`, `return !!(int)c;`, `c ? 1 : 0`, `!!(c || false)`,
  `!!(c && true)`, `!!(c|0)`, `int r = c; return !!r;` - all fold to a bare
  `r3` with no `r31` frame at all, which is the wrong shape.
- 61.53%: `!!(c ? true : false)`, `unsigned char r = c ? 1 : 0; return !!r;`.
- Changing the header's return type to `const bool` **regressed** it to 69.95%.
  Reverted.

I did not find the spelling that gives a lone `clrlwi r3,r31,24` from a value
already in `r31`. The two-call functions prove the compiler emits `,24` when the
mask is applied to an expression the optimiser has *not* proved is 0/1, so the
shape probably exists, but I did not reach it. Not a `WALL:` - one instruction on
two functions, and the next run should try the *callee*'s side rather than the
return: e.g. `EFilterType` written explicitly, `GetControlMapper()` vs the member
(it is a reference-returning inline, which changes what the optimiser knows), or
a `bool`-returning `CControlMapper` helper.

### Verification

`./tools/goal_check.sh build/goal/item.json`:

```
ok    no judge-owned path touched
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 11317 -> 11322   linked 5507 -> 5507
ok    check_symbol_names.py
ok    All:  32.58% fuzzy, 25.24% matched, 11.94% linked (11322 / 28465 functions)
ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 3 -> 8 / 22 functions
ok    no asm added
goal_check: PASS progress-prime1-cplayervisor
```

- Unit: `CPlayerVisor` fuzzy **2.5288% -> 16.06%**, matched 3 -> 8. Function
  sizes all match retail exactly (128 / 128 / 128 / 128 / 128 / 76 / 76).
- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+5 functions at 100%, 0 units newly linked`, **`no regression`**.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CPlayerVisor`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py`: `514 units; 0 declared names are missing`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `docs/HANDOFF.md` was rewritten by the gate (runs with `MP_GATE_DOCS_WRITE=1`)
  and I reverted it, per the brief. It is not in the diff.

### Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `FireBeamHeld` | 4.375% | **100.00%** | not used - the body is in retail's own disassembly |
| `FireBeamPressed` | 4.375% | **100.00%** | not used |
| `JumpHeld` | 4.375% | **100.00%** | not used |
| `JumpPressed` | 4.375% | **100.00%** | not used |
| `fn_8022b7f4` | 4.375% | **100.00%** | not used |
| `fn_8022b974` | 7.3684% | 99.74% | not used |
| `fn_8022b7a8` | 7.3684% | 99.74% | not used |
| `ResetPlayerHintState` | 1.2987% | 1.2987% | untouched (blocked above) |
| `SetAreaPlayerHint` | 0.5907% | 0.5907% | untouched (blocked above) |
| `UpdatePlayerHints` | 0.4386% | 0.4386% | untouched (no counterpart, above) |

No new blocker and no `NEW:` item: the two 99.74% functions are ordinary
`CControlMapper` accessors whose remaining instruction is a codegen idiom, not a
missing class or an unclaimed range, so it does not meet the bar for a queued
item.

### Files touched

`src/MetroidPrime/Player/CPlayerVisor.cpp` - the seven bool accessor bodies only.
No header change, no config change, no `tools/` change, no new `.s`.

Not committed, as instructed.
