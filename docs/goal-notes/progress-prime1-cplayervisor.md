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

## 2026-10-01 lane L1: the return type of the two single-command accessors, and
## `UpdateRezbitRecoveryInput`

Re-measured this tree first: `build/report.json` showed
`main/MetroidPrime/Player/CPlayerVisor` at **8/22**, fuzzy 16.0612%, exactly the state
lane L5 left it in. The three functions the item queues were still at their recorded
scores (`ResetPlayerHintState` 1.2987, `SetAreaPlayerHint` 0.5907, `UpdatePlayerHints`
0.4386) and the blockers recorded above for them still hold, so I did not re-derive them.

**Result: 8/22 -> 11/22** (fuzzy 16.0612% -> 19.5935%). Three functions at exactly
100.00%; no function anywhere in the tree got worse.

### 1. `fn_8022b974` and `fn_8022b7a8` (99.7368% -> 100.00%): retail returns a **byte**, not a bool

This finishes what lane L5 left one instruction short, and the answer is not a spelling
of the body - it is the **return type**. The header declared both as `bool`:

```cpp
bool CPlayer::fn_8022b974(const CFinalInput& input) const {
  bool result = false;
  if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown15, input)) result = true;
  return result;            // -> mr r3,r31           (96.84%)
}
```

Retail's tail, identical in both functions (0x8022b9ac / 0x8022b7e0):

```
  li r31,0 ; bl GetDigitalInput ; clrlwi. r0,r3,24 ; beq ; li r31,1
  clrlwi  r3,r31,24            <- the one instruction we were missing
```

mwcceppc elides the mask when the returned expression is already `bool` - that is exactly
the codegen rule lane L5 recorded for the two-call functions. Changing **the return type**
to `unsigned char` (header and definition) and leaving the body alone reproduces it:

```
bool result = false;
if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown15, input)) { result = true; }
return result;        // bool -> unsigned char  ==  clrlwi r3,r31,24
```

Both functions hit 100.00% on the first try with that change. `char` and `short` produce
the same bytes (the mask is to 24 bits either way); `unsigned char` is what is committed,
and the header now carries a comment saying so. No other unit references either symbol,
so nothing else moved.

**This is the reusable finding: a lone `clrlwi r3,rX,24` on return, with a 0/1
accumulator in a register, means the function's return type is not `bool`.** The two-call
siblings (`FireBeamHeld` etc.) genuinely are `bool` and genuinely need `!!`; these two are
not, and no amount of `!!`, `& 1`, `? :` or accumulator type reaches the bytes - measured
on this tree, all of these **without** the return-type change:

| spelling | score |
|---|---|
| `bool r=false; if(c) r=true; return r;` | 96.84% |
| `bool r; r=0; if(c) r=1; return r;` (uchar / char / short) | 82.79% |
| `bool r=false; if(c) r=true; return r & 1;` | 99.74% (L5's best) |
| `int/unsigned/long r=0; if(c) r=1; return !!r;` | 88.05% |
| `return !!c;` `return !!(c&&true);` `return !!(c||false);` `if(c) return true; return false;` | 69.95% |
| `bool r = c; return r;` / `bool r = !!c; return r;` | 56.53% |
| `bool r=false; if(c) r=true; return r ? true : false;` | 73.37% |
| `if(!!c) r=true; return r;` | 96.84% |
| via `GetControlMapper()`, or `kFT_Filtered` written out | 96.84% (same as the plain form) |

So: **do not spend another run on the body of a `CPlayerVisor` digital-input accessor.**
Change the return type instead.

### 2. `UpdateRezbitRecoveryInput` (2.0% -> 100.00%): a whole function, no Prime 1 needed

No earlier run listed this one. It is self-contained: its only call is
`CControlMapper::GetPressInput`, which is already `Matching` at 100%. Everything else is
field arithmetic on `mRezbitRecoveryDirection` (CPlayer+0x13c8) and
`mRezbitRecoveryInputCount` (+0x13cc), which already exist in the header.

Read off `./tools/dis.sh 0x8022b6e0 0xc8`: commands **3** and **4** are
`kC_TurnLeft` / `kC_TurnRight`; each accepted press bumps the counter and latches the
direction to 1 (left) / 2 (right), and a turn only counts while the direction is neutral
(0) or already that way. Body as committed:

```cpp
void CPlayer::UpdateRezbitRecoveryInput(const CFinalInput& input) {
  bool turnedLeft = mControlMapper.GetPressInput(CControlMapper::kC_TurnLeft, input);
  if (mControlMapper.GetPressInput(CControlMapper::kC_TurnRight, input)) {
    if (mRezbitRecoveryDirection == 1 || mRezbitRecoveryDirection == 0) { ... = 2; }
  }
  if (turnedLeft) {
    if (mRezbitRecoveryDirection == 2 || mRezbitRecoveryDirection == 0) { ... = 1; }
  }
}
```

Two codegen rules came out of it, both measured, both reusable:

- **`uint` vs `int` decides `cmpwi` vs `cmplwi`.** With the members declared `uint` the
  four comparisons compiled to `cmplwi r0,0` / `cmplwi r0,1` and the function sat at
  99.92%; retail uses `cmpwi r0,0` / `cmpwi r0,1`. Changing the two members to `int`
  (no layout change) fixes all four. So **an `unsigned` member compared against a
  constant is a sign that the member's type is wrong** - check retail's opcode before
  writing arithmetic on a guessed `uint`.
- **`x == 1 || x == 0` must be written in retail's order.** MW emits the compares in
  source order, so `== 0 ||` first scores 99.92% and `== 1 ||` first scores 100.00%.
  Same idiom as `fn_8022b7f4`.

### Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11347 -> 11350   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.63% fuzzy, 25.37% matched, 11.94% linked (11350 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 8 -> 11 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayervisor
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+3 functions at 100%, 0 units newly linked`, **`no regression`** (28465 functions).
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CPlayerVisor`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py`: `checked 514 units; 0 declared names are missing`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/link_check.sh`: `unchanged from baseline (250 undefined, 0 duplicates)`.
- `docs/HANDOFF.md` was rewritten by the gate (it runs with `MP_GATE_DOCS_WRITE=1`) and
  reverted, per the brief. It is not in the diff.

### Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `fn_8022b974` | 99.7368% | **100.00%** | not used - retail's own disassembly has the whole body |
| `fn_8022b7a8` | 99.7368% | **100.00%** | not used |
| `UpdateRezbitRecoveryInput` | 2.0% | **100.00%** | **no counterpart** - Rezbit does not exist in Prime 1 (`grep -rn Rezbit prime-ref/src/MetroidPrime/Player/` is empty). Read off the disassembly |
| `ResetPlayerHintState` | 1.2987% | 1.2987% | untouched (see the 2026-09-30 blocker above) |
| `SetAreaPlayerHint` | 0.5907% | 0.5907% | untouched (see above) |
| `UpdatePlayerHints` | 0.4386% | 0.4386% | untouched (see above) |

### What is still unmatched, and what each one now needs (so nobody re-derives it)

Measured this run with `./tools/dis.sh`; the rest of the unit is unchanged:

- `fn_8022B64C` (0x8022b64c, 100 B, 0%) is **not source at all**: `stwu / mflr / memcpy(r3=r1+8,
  r5=12) / __ptmf_scall(r3=this, r4=r5, r5=r6, r12=r1+8)`. A compiler-generated
  performance-measurement wrapper. Not reachable from C++ source; skip it.
- `ResetRezbitState` (76 B) and `StopRezbitState` (140 B) are small and otherwise fully
  recoverable, but both call **0x8022EA5C**, which is in the *unclaimed* auto unit
  `main/auto_03_8022E13C_text` (100 B, 0%). Calling it adds an undefined symbol and
  `link_check` fails STRICT on a rise, so each needs a stand-in first - that is why they
  were not started here. Bodies for reference: reset = `fn_8022EA5C(&mRezbitEffectToken,
  mgr, mgr+0x13b8); mRezbitState = 0; mRezbitEffectId = kInvalidUniqueId;
  ((uchar*)gpGameState)[0xd8] = 0;`; stop = the same tail guarded by
  `if (mRezbitState && mRezbitEffectId != kInvalidUniqueId) { mgr.DeleteObjectRequest(
  mRezbitEffectId); fn_8022EA5C(...); ... }` (gpGameState resolved with `tools/sda.py`,
  not by hand).
- `UpdateRezbitState` (228 B) and `BeginRezbitRecovery` (120 B) both build a HUD memo:
  `rstl::basic_string<w>` ctor + `CStringTable::GetString` + `CHUDMemoParms` ctor +
  `CSamusHud::DisplayHudMemo` + the string dtor. That needs a string literal in this unit
  (which the brief warns can move a shared unit by 32 bytes) and whatever is still missing
  of `CHUDMemoParms`/`CSamusHud`.
- `StartRezbitState` (832 B), `fn_8022af0c` (460 B) and the three queued functions are as
  characterised above; nothing new was learned about them this run.

No `WALL:` (everything attempted reached 100%), no `NEW:` (each remaining blocker is a
callee already recorded above or a restatement of the item), no `STALE:`.

### Files touched

- `include/MetroidPrime/Player/CPlayer.hpp` - two accessors `bool` -> `unsigned char` with a
  comment; `mRezbitRecoveryDirection`/`mRezbitRecoveryInputCount` `uint` -> `int`.
  No layout change (`CHECK_SIZEOF(CPlayer, 0x14c8)` holds; report_diff shows no unit moved).
- `src/MetroidPrime/Player/CPlayerVisor.cpp` - the two accessor bodies and
  `UpdateRezbitRecoveryInput`. No config change, no `tools/` change, no `.s`.

Not committed, as instructed.

## 2026-10-01 lane L2: `fn_8022c338` and `UpdatePlayerHints` - the two largest functions in the unit

Re-measured this tree first, from `build/report.json`: `main/MetroidPrime/Player/CPlayerVisor` at
**11/22**, fuzzy 19.5935%. The three functions the item queues were at the same scores every
earlier run recorded (`ResetPlayerHintState` 1.2987, `SetAreaPlayerHint` 0.5907,
`UpdatePlayerHints` 0.4386), and the blockers recorded for them above still hold, so I did not
re-derive them. I did **not** repeat any earlier run's work: none of them touched
`fn_8022c338`, and the previous run's claim that `UpdatePlayerHints` "has no counterpart in
Prime 1" is **wrong** - see below.

**Result: 11/22 -> 12/22** (fuzzy 19.5935% -> 42.9029%). `fn_8022c338` reached **100.00%** on
the first try; `UpdatePlayerHints` went 0.44% -> **99.5614%** (one dead `b` short, see the
spelling table). No function anywhere in the tree got worse. `tools/goal_check.sh` printed
`goal_check: PASS progress-prime1-cplayervisor`.

### 1. `fn_8022c338` (0x8022c338, 396 B): 1.0101% -> **100.00%** - Prime 1's `UpdatePlayerControlDirection`

**The name is the only thing Echoes changed.** Prime 1
`src/MetroidPrime/Player/CPlayerDynamics.cpp:722` `CPlayer::UpdatePlayerControlDirection(float,
CStateManager&)` is this function; Echoes' symbol database calls the same body
`fn_8022c338` and the *next* function down `UpdatePlayerHints`. So the earlier runs'
conclusion - "`UpdatePlayerHints` is Echoes' own control-direction code, not decompilable from
Prime 1 at all" - is **superseded**: both of these functions are Prime 1's, under other names,
and both match Prime 1's source essentially unchanged. **Do not spend a run re-deriving the
blocker recorded above; it was a naming miss, not a fork.**

Prime 1's body, adapted only for renamed members, reaches 100% as written:

```cpp
const CVector3f oldDirection = mControlDir;
const CVector3f oldFlatDirection = mControlDirFlat;
UpdatePlayerHints(mgr);                       // Prime 1: CalculatePlayerControlDirection
if (mInterpolatingControlDir && mMorphBallState == kMS_Morphed) {
  mControlDirInterpTime = mControlDirInterpTime + dt;
  if (mControlDirInterpTime > mControlDirInterpDuration) {
    mControlDirInterpTime = mControlDirInterpDuration;
    ResetControlDirectionInterpolation();
  }
  const float blend = CMath::Limit(mControlDirInterpTime / mControlDirInterpDuration, 1.f);
  mControlDir = CVector3f::Lerp(oldDirection, mControlDir, blend);
  mControlDirFlat = CVector3f::Lerp(oldFlatDirection, mControlDir, blend);
}
```

Adaptations, all forced by the measurement: Prime 1's `mControlDirInterpDur` is this header's
`mControlDirInterpDuration`, and `CalculatePlayerControlDirection` is declared here as
`UpdatePlayerHints`. Nothing else changed - including the **`CMath::Limit` clamp with an
inlined `FastFSel`/`fabs`/`frsp`**, which is this repo's own `include/Kyoto/Math/CMath.hpp:63`
and reproduces retail's `fabs`+`frsp`+`fsel`+`fmuls` sequence exactly. That is worth knowing:
retail's clamp is *not* `CMath::Clamp`, and `Limit(v, 1.f)` is what produces it.

Retail's condition is `lbz 0x1269; rlwinm. 30,31,31` - that is the compiler's read of
`mInterpolatingControlDir`, confirmed with a purpose-built probe (below), not a guess.

### 2. `UpdatePlayerHints` (0x8022BFA8, 912 B): 0.4386% -> 99.5614% - Prime 1's `CalculatePlayerControlDirection`

Same story: Prime 1 `CPlayerDynamics.cpp:738` `CPlayer::CalculatePlayerControlDirection(
CStateManager&)`, renamed to `UpdatePlayerHints` in Echoes. The body transfers with these
measured adaptations:

- **The guard is `x1268_30_`, not `mDrawCrosshairs`.** Retail tests `lbz 0x1268; rlwinm.
  31,31,31` (bit 0 of that byte). `mDrawCrosshairs` compiles to `rlwinm. 26,31,31`.
- **The camera is `GetCameraManager()` (CPlayer's own, +0x1318), not `mgr.GetCameraManager(0)`.**
  Retail emits `lwz r3,0x1318(r31)` off `this`; `CStateManager::m_cameraManagers` is at 0x151c.
  `GetCameraManager()->GetCurrentCamera(mgr, true)->GetTranslation()` is the whole expression -
  the `->GetTranslation()` is `CActor::mPosition` at 0x54/0x58/0x5c, inlined.
- `mControlDirOverride` (0x1288) is the override *direction* vector; Prime 1's separate
  `mControlDirOverride` bool is the guard bit above.
- Everything else - the `CVector3f(0,1,0)` fallback, the `SetZ(0)` + `CanBeNormalized` +
  `Normalize` dance, `gpTweakBall->GetBallCameraControlDistance()`, `mFlatMoveSpeed < 0.25f`,
  and the 4-way `switch (mMorphBallState)` with its 0/1/2/3 bounds check - is Prime 1's source
  unchanged. **The `switch` needs all three non-`kMS_Morphed` cases listed** or retail's
  `cmpwi 0 / bge / cmpwi 4 / bge` range check does not appear.

### 3. The bit-field probe (worth keeping; it took four builds to guess otherwise)

A 16-function probe TU compiled with `tools/probe_cc.sh`'s exact flags reads every `bool : 1`
in bytes 0x1268 and 0x1269 and shows which bit each member actually occupies. The mapping is
**not** the one the member names suggest - they are declared MSB-first but named as if
LSB-first:

| byte 0x1268 | bit read | byte 0x1269 | bit read |
|---|---|---|---|
| `x1268_24_` | `clrlwi 31` (bit 7) | `x1269_24_` | `clrlwi 31` (bit 7) |
| `mDrawCrosshairs` | `rlwinm 26` (bit 5) | `mHitWallDuringMove` | `rlwinm 26` (bit 5) |
| `x1268_26_` | `rlwinm 27` (bit 4) | `x1269_26_` | `rlwinm 27` (bit 4) |
| `x1268_27_` | `rlwinm 28` (bit 3) | `x1269_27_` | `rlwinm 28` (bit 3) |
| `x1268_28_` | `rlwinm 29` (bit 2) | `x1269_28_` | `rlwinm 29` (bit 2) |
| `x1268_29_` | `rlwinm 30` (bit 1) | **`mInterpolatingControlDir`** | **`rlwinm 30` (bit 1)** |
| **`x1268_30_`** | **`rlwinm 31` (bit 0)** | `x1269_30_` | `rlwinm 31` (bit 0) |
| `x1268_31_` | `clrlwi 31` (bit 7) | `x1269_31_` | `clrlwi 31` (bit 7) |

`x1268_31_` compiles to the *same* instruction as `x1268_24_` (`clrlwi 31`), so the header's
`xNNNN_31_` names are wrong for both bytes - they are bit 7 in both. Anyone writing a `bool : 1`
test in this struct should compile a probe rather than trust the name. The probe recipe is in
the notes above; `tools/probe_cc.sh` is missing the `-i extern/musyx/include -DMUSY_TARGET=...`
flags the real build passes, so it needs a copy with them added.

### 4. Port host definition for the one new call

`gpTweakBall->GetBallCameraControlDistance()` is a new call in the port, taking the undefined
count 250 -> 251, which `tools/link_check.sh` fails STRICT on. Added the accessor to
`src/MetroidPrime/PortCTweakBall.cpp` - the file that exists for exactly this (its header says
so), body character for character from `src/MetroidPrime/Tweaks/CTweakBall.cpp:258`. Retail
0x80217148 is `lwz r3,0(r3); lfs f1,0x198(r3); blr` - one field read, no test, so this is the
real implementation and not a stand-in. Undefined back to 250.

### 5. What `UpdatePlayerHints` still needs: one dead `b` (measured, 40+ spellings)

99.5614% is **908 of 912 bytes: one 4-byte instruction, a dead `b 0x8022c31c` at +0xbc**,
between the two `CVector3f(0,1,0)` fallback blocks. Retail's layout is
`... ZERO#1; b end; b end; ZERO#2; b end` - the second `b end` is unreachable, and mine emits
only one. This is a jump-threading artifact, not a semantic difference: `objdiff` reports
`DIFF_INSERT` on exactly that one instruction and nothing else. **Every** shape below compiles
to the same 908 bytes; only the score differs, and the ones that change it make things worse.
Measured this run, all on this tree:

| spelling | score |
|---|---|
| `if (guard) { if (can) {A; if (flat) N else ZERO} else ZERO }` (committed) | **99.5614%** |
| inner `if (!flat) ZERO else N` (inverted) | 97.76% |
| inner `else if (true) ZERO` / outer `else if (true) ZERO` / both | 99.5614% |
| outer `if (!guard) CAM else if (can) ... else ZERO` | 99.5614% |
| `if (guard && can) A else if (guard) ZERO else CAM` | 99.5614% |
| a `static const CVector3f kUp` shared by both fallbacks, or a `static inline` setter | 99.5614% |
| a named local per arm, `const CVector3f up(0,1,0); mControlDir = up; ...` | 99.5614% |
| shared tail: the guard arm falls out of the `if` into one ZERO, `return` in the other | 95.59 / 96.47% |
| `do { ... } while (0)`, `while (0) { ... break; }`, `for(;;) { ... break; }`, `while(true)` | 99.5614% |
| `switch (can ? 1 : 0) { case 1: A; break; case 0: ZERO; break; }` | 99.5614% |
| `switch` with an empty `case 0` between the arms, or a `case 0` fall-through into ZERO | 99.5614% |
| `if (flat) { N; return; } else { ZERO; return; }` and the `return` placed in the other arm | 99.5614% |
| `return;` after the override `if/else`, or inside its then-arm only, or in both arms | 99.56 / 99.12% |
| `if (can) A; if (!can) ZERO;` (sequential ifs, not `else`) | 99.5614% |
| `if (!can) { ZERO; return; } A; if (!flat) { ZERO; return; } N;` | 91.60% |
| an `if (false) {}` / `for (i=0;i<0;++i) {}` / `;` / `(void)0;` after the override block | 98.64 - 99.5614% |
| a `goto` out of the guard arm, or a label with `;` after the override `if/else` | 94.41% |
| `if (can) A; if (!flat) ZERO; else N;` (inner inverted, fallthrough ZERO) | 97.76% |
| the override `else` reached by `else if (flat) {} else ZERO` | 99.5614% |
| the `switch (mMorphBallState)` written as nested `if`s / `while (flat) { N; break; }` | 94.41% |
| statements reordered inside the override arm (flat first, then normal) | 95.75% |
| `mControlDirOverride` bound to a `const CVector3f&` local first | 95.27% |
| `x1268_30_` compared as `!= 0` / `static_cast<int>(...) != 0` | 99.5614% |

**Not a `WALL:`** - the item passes and the function rose 0.44% -> 99.56%, but the remaining
instruction is a real, named obstacle, so the spellings are all recorded above rather than a
one-line wall. The next run should attack it from the *other* side: the extra `b end` looks
like a jump-to-jump that a `goto` or a loop-exit idiom would create, and the `goto` spellings
that move it also cost 16 bytes elsewhere - so the shape it needs is one that keeps the two
fallback blocks **separate** while still ending the guard arm with a jump. `break` out of a
`switch` whose cases are the two fallbacks is the untried idea.

### What is now left in this unit, and what each one needs

Measured this run; the rest of the unit is unchanged from the 2026-10-01 lane L1 list above:

- `fn_8022B64C` (100 B, 0%) is **not source**: a compiler-generated performance-measurement
  wrapper (`memcpy` + `__ptmf_scall`). Not reachable from C++ source; skip it.
- `ResetRezbitState` (76 B) and `StopRezbitState` (140 B) both call **0x8022EA5C**, which is in
  the *unclaimed* auto unit `main/auto_03_8022E13C_text` (100 B, 0%); calling it raises the
  undefined count and `link_check` fails STRICT on a rise, so each needs a stand-in first.
  Bodies are in the L1 notes above and are unchanged.
- `UpdateRezbitState` (228 B), `BeginRezbitRecovery` (120 B) and `StartRezbitState` (832 B)
  build a HUD memo: `rstl::basic_string<w>` ctor + `CStringTable::GetString` + `CHUDMemoParms`
  ctor + `CSamusHud::DisplayHudMemo` + the string dtor. Needs a string literal in this unit
  (which the brief warns can move a shared unit by 32 bytes).
- `fn_8022af0c` (460 B) and `SetAreaPlayerHint` (948 B) - as characterised above;
  `SetAreaPlayerHint` is still blocked on the fieldless `CScriptPlayerHint` placeholder, and
  `fn_8022af0c` still needs the shared break-hint enum recovered.

No `NEW:` item this run: the only new obstacle is the dead `b` above, which is four bytes in a
function already at 99.56% and does not meet the bar for a queued item. No `STALE:`.

### Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11392 -> 11393   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.74% fuzzy, 25.47% matched, 11.94% linked (11393 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 11 -> 12 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayervisor
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+1 functions at 100%` (`CPlayerVisor::fn_8022c338` 1.0101 -> 100.00), 0 units newly linked,
  **`no regression`** over 28465 functions.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CPlayerVisor`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py`: `checked 514 units; 0 declared names are missing`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/link_check.sh`: `unchanged from baseline (250 undefined, 0 duplicates)`.
- `docs/HANDOFF.md` was rewritten by the gate (it runs with `MP_GATE_DOCS_WRITE=1`) and
  reverted, per the brief. It is not in the diff.

### Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `fn_8022c338` | 1.0101% | **100.00%** | **matched unchanged** as `CPlayer::UpdatePlayerControlDirection`; two member renames only |
| `UpdatePlayerHints` | 0.4386% | 99.5614% | **matched as `CPlayer::CalculatePlayerControlDirection`**, adapted: guard bit, `GetCameraManager()`, and all three non-`kMS_Morphed` switch cases |
| `ResetPlayerHintState` | 1.2987% | 1.2987% | untouched (blocker above; **do not re-derive, it is a naming miss - see item 1**) |
| `SetAreaPlayerHint` | 0.5907% | 0.5907% | untouched (blocked on the `CScriptPlayerHint` placeholder) |

### Files touched

- `src/MetroidPrime/Player/CPlayerVisor.cpp` - `fn_8022c338` and `UpdatePlayerHints` bodies,
  plus four includes. No header change, no config change, no `tools/` change, no `.s`.
- `src/MetroidPrime/PortCTweakBall.cpp` - the `GetBallCameraControlDistance` host definition.

Not committed, as instructed.

## 2026-10-01 lane L7: `ResetPlayerHintState` - the recipe the first run reached but that never landed

Re-measured this tree first, from `build/report.json`: `main/MetroidPrime/Player/CPlayerVisor` at
**12/22**, fuzzy 42.9029% - exactly the state lane L2 left it in. The three functions the item
queues were at their recorded scores and their recorded blockers still stood, so I did not
re-derive them.

**But the very first run's work was not in this tree.** Its section above claims
`ResetPlayerHintState` reached 100.00%, which needed a `CPlayer.hpp` change (`uchar x126a_` ->
eight `bool`) plus a `CHintManager::RemoveHint` stand-in. Re-measured here: `include/.../CPlayer.hpp`
still had `uchar x126a_;` at line 712, the body was still `// TODO`, and `RemoveHint` had no
definition anywhere - so **that 100% was never committed and the recipe is worth re-deriving**.
Every later run's notes say "untouched (blocked above)" and never noticed it was missing.

**Result: 12/22 -> 13/22** (fuzzy 42.9029% -> 48.37%, matched-code 25.83% -> 31.37%).
`ResetPlayerHintState` 1.2987% -> **100.00%**. No function anywhere in the tree got worse.

### The bit-field probe, re-measured - and it corrects the earlier table

The first run's notes say the flag-byte members "are written through this header's offset names".
I rebuilt the probe (a 7-`bool` struct plus eight one-member setter functions, compiled with
`tools/probe_cc.sh`'s exact flags) and got the mapping directly:

| declaration index | emitted | bit |
|---|---|---|
| m0 (first) | `rlwimi r0,r4,7,24,24` | 0 |
| m1 | `rlwimi r0,r4,6,25,25` | 1 |
| m2 | `rlwimi r0,r4,5,26,26` | 2 |
| ... | ... | ... |
| m7 (eighth) | `rlwimi r0,r4,0,31,31` | 7 |

**So MW packs a `bool : 1` MSB-first: the first-declared flag is bit 0, the eighth is bit 7.**
In a *read*, the same field is tested `rlwinm r3,r0,25+i,31,31` (i = index), which is what
`UpdatePlayerHints`'s guard `rlwinm. r0,r0,31,31,31` is: i = 6, the seventh declaration. The header
names it `x1268_30_`, seventh in the declaration order, so **the names and the bits agree** - the
*earlier* run's table (which said `x1268_24_` is bit 7 and `x1268_31_` is bit 7) was wrong, and its
conclusion "the `xNNNN_31_` names are wrong for both bytes" is **superseded**. Lane L2's separate
probe reached the same conclusion, so the `UpdatePlayerHints` guard is unaffected either way.

Two independent confirmations that the packing is right, both from retail, not from me:
retail's own `CPlayer` constructor writes byte 0x126a as `sh 7,6,5,4,3,2,1,0` in that order
(0x8001BA08-0x8001BA58) - descending sh is ascending declaration index, i.e. MSB-first; and the
`x1268_26_` / `x1268_27_` / `x1268_28_` flags in `ResetPlayerHintState` need sh 5/4/3, i.e.
indices 2/3/4, which is exactly what the header's declaration order predicts.

### The body, and where the first try was wrong

`tools/dis.sh 0x8022BE74 0x134` gives twelve byte RMWs plus four calls. Written in retail's order
(0x1268: idx2,3,4,6; then 0x1269: idx4,6; then 0x1268: idx5; then 0x126a: idx7; 0x1269: idx7;
0x126a: idx0,1; then 0x126b: idx7), it scored **99.87% on the first build** - every byte correct
except one instruction. Retail's `li r6,1` / `li r5,0` at the top say `r6` is the *true* source and
`r5` the *false* one, and the first three writes use `r6`, not `r5`:

```
8022bea8:  50 c0 26 f6   rlwimi  r0,r6,4,27,27    <- true, not false
8022beb4:  50 c0 1f 38   rlwimi  r0,r6,3,28,28    <- true, not false
```

I had written those three as `false` (following the ctor's `x1268_27_(true)` / `x1268_28_(true)`
and reading the *store* as a clear). Changing indices 3 and 4 to `true` gave **100.00%**. The
lesson generalises and is cheap: **when a bool RMW series is one instruction short, diff the
source register, not the mask** - `r5` vs `r6` is the only thing `lbz/rlwimi/stb` can get wrong
here, and it is invisible in the mask fields.

Everything else was read straight off the disassembly and needed no guessing:
`GetMorphBall()->SetBoostEnabled(true)` (0x1174 -> 0x101c, r6, sh 7), `ResetControlDirectionInterpolation()`,
`RemoveMaterial(kMT_Immovable, mgr)` (43), then the guarded
`mControlHintManager->RemoveHint(x14bc_, GetUniqueId(), mgr)`. The `cmplwi` on `mControlHintManager`
and the `lhz -27740(r13)` (`kInvalidUniqueId`, from `tools/sda.py`) fall out of
`if (mControlHintManager && x14bc_ != kInvalidUniqueId)` with no extra source. Retail also
builds three `sth` temporaries for the last call; the by-value `TUniqueId` parameters in
`CHintManager.hpp` reproduce that as written.

### `CPlayer.hpp`: byte 0x126a is eight bools

`uchar x126a_` -> `bool x126a_24_` .. `x126a_31_ : 1`, with the ctor init list set to eight
`false`. Unavoidable: retail read-modify-writes three separate bits in that byte and the
constructor writes all eight, so a `uchar` cannot express it. **Layout is unchanged**
(`CHECK_SIZEOF(CPlayer, 0x14c8)` holds, and `report_diff.py` shows no other unit moved). The
header comment now records why, since the name pattern suggests otherwise.

### Port stand-in for the one new call

`CHintManager::RemoveHint` (retail 0x801B94B8, 0xAC bytes) is in an **unclaimed gap** -
`config/G2ME01/splits.txt` claims up to 0x8022E13C and then 0x8022EB9C, so no unit owns it.
Calling it takes the port 250 -> 251 undefined and `tools/link_check.sh` fails STRICT on a rise.
Defined in `src/MetroidPrime/PortGlobals.cpp` beside the existing `CHintManager::Update`
stand-in: prints its own name once, and says in the comment that it is **not** decompilation
and **not** claimed to match retail. Undefined back to 250.

### Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11406 -> 11407   linked 5514 -> 5514
  ok    check_symbol_names.py
  ok    All:  32.79% fuzzy, 25.53% matched, 11.96% linked (11407 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 12 -> 13 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayervisor
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+1 functions at 100%`, 0 units newly linked, **`no regression`** over 28465 functions.
- Instruction-level check (my `.o` vs `main.elf`, 77 instructions each, byte-identical after
  the fix): the only remaining `bl`/`lhz` differences are unresolved relocations in the `.o`.
- `./tools/link_check.sh`: `unchanged from baseline (250 undefined, 0 duplicates)`.
- `python3 tools/check_symbol_names.py`: part of `goal_check`, passed.
- `docs/HANDOFF.md` was rewritten by the gate (it runs with `MP_GATE_DOCS_WRITE=1`) and
  reverted, per the brief. It is not in the diff.
- `CPlayer` unit: no regression reported, so the ctor did not get worse.

### Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `ResetPlayerHintState` | 1.2987% | **100.00%** | **no counterpart** - Prime 1 has no `CHintManager` at all (`ls include/MetroidPrime/CHintManager.hpp` -> no such file), and Echoes moved this out of Prime 1's `CPlayerDynamics.cpp` into this unit. Read off the disassembly |
| `SetAreaPlayerHint` | 0.5907% | 0.5907% | untouched (blocked on the `CScriptPlayerHint` placeholder, as recorded) |
| `UpdatePlayerHints` | 0.4386% | 99.5614% | untouched - already at that score from lane L2, not re-derived |

### What is still unmatched, and what each one needs

Unchanged from lane L2's list, except that `ResetRezbitState` / `StopRezbitState` have one blocker
**removed** by this run's stand-in work - see the `NEW:` line below:

- `fn_8022B64C` (100 B, 0%) is **not source**: a compiler-generated performance-measurement
  wrapper (`memcpy` + `__ptmf_scall`). Not reachable from C++ source; skip it.
- `ResetRezbitState` (76 B) and `StopRezbitState` (140 B) both call **0x8022EA5C**, which lands in
  the same unclaimed gap (0x8022E13C..0x8022EB9C) that `CHintManager::RemoveHint` does. **One
  announced stand-in for that address unblocks both**, and both bodies are already transcribed in
  lane L1's notes above. That is the cheapest remaining item in this unit.
- `UpdateRezbitState` (228 B), `BeginRezbitRecovery` (120 B) and `StartRezbitState` (832 B)
  build a HUD memo: `rstl::basic_string<w>` ctor + `CStringTable::GetString` + `CHUDMemoParms`
  ctor + `CSamusHud::DisplayHudMemo` + the string dtor. Needs a string literal in this unit
  (which the brief warns can move a shared unit by 32 bytes).
- `fn_8022af0c` (460 B) - still needs the shared break-hint enum recovered.
- `SetAreaPlayerHint` (948 B) - still blocked on the fieldless `CScriptPlayerHint` placeholder.

No `WALL:` (the function attempted this run reached 100%). No `STALE:`.

## NEW:

NEW: progress-prime1-cplayerhintunclaimed | progress | MetroidPrime/Player/CPlayerVisor | ResetRezbitState (76 B) and StopRezbitState (140 B) both call 0x8022EA5C, which is in the unclaimed gap 0x8022E13C..0x8022EB9C; one announced stand-in there unblocks both, and both bodies are already transcribed in these notes.

### Files touched

- `include/MetroidPrime/Player/CPlayer.hpp` - `uchar x126a_` -> eight `bool x126a_NN_ : 1` with a
  comment. No layout change.
- `src/MetroidPrime/Player/CPlayer.cpp` - ctor init list for those eight, all `false`.
- `src/MetroidPrime/Player/CPlayerVisor.cpp` - `ResetPlayerHintState` body; three includes.
- `src/MetroidPrime/PortGlobals.cpp` - announced stand-in for `CHintManager::RemoveHint`.

No config change, no `tools/` change, no `.s`. Not committed, as instructed.

## 2026-10-01 lane L4: the two HUD-memo Rezbit functions - `BeginRezbitRecovery` 100%,
## `UpdateRezbitState` 98.25%

Re-measured this tree first, from `build/report.json`: `main/MetroidPrime/Player/CPlayerVisor`
at **15/22**, fuzzy 52.1115%. The three functions the item queues were at their recorded
scores and their recorded blockers still stand, so I did not re-derive them.

I did not repeat any earlier run's work either. Every prior run listed the two
**HUD-memo Rezbit functions as blocked**, on the grounds recorded in the L1/L2/L7 sections:

> "UpdateRezbitState (228 B) and BeginRezbitRecovery (120 B) both build a HUD memo:
> `rstl::basic_string<w>` ctor + `CStringTable::GetString` + `CHUDMemoParms` ctor +
> `CSamusHud::DisplayHudMemo` + the string dtor. Needs a string literal in this unit
> (which the brief warns can move a shared unit by 32 bytes)."

**That blocker is wrong, and it is worth correcting precisely, because the warning it cites
does not apply here.** Every one of those five pieces is already in this tree:
`rstl::wstring_l` (`include/rstl/string.hpp:416`, defined in `src/MetroidPrime/PortGlobals.cpp:882`),
`CStringTable::GetString` (`include/Kyoto/Text/CStringTable.hpp`, with `extern CStringTable*
gpStringTable`), `CHUDMemoParms`'s ctor (`src/MetroidPrime/HUD/CHUDMemoParms.cpp`, and its unit is
**Matching, 2/2, 100%**), `CSamusHud::DisplayHudMemo` (`src/MetroidPrime/HUD/CSamusHud.cpp:325`),
and the `basic_string` dtor. `UpdateRezbitState` is a 228-byte function and `BeginRezbitRecovery`
a 120-byte one; neither is large. The "moves a shared unit by 32 bytes" warning is about a
**string literal in a unit someone else is decompiling** - `CPlayerVisor.cpp` is this item's own
target unit, nothing else claims it, and `report_diff.py` confirms no other unit moved. Measured,
not assumed: see the verification below.

**Result: 15/22 -> 16/22** (fuzzy 52.1115% -> 58.15%). `BeginRezbitRecovery` 3.3333% ->
**100.00%**, `UpdateRezbitState` 1.7544% -> **98.25%**. `tools/goal_check.sh` printed
`goal_check: PASS progress-prime1-cplayervisor`. No function anywhere in the tree got worse.

### 1. `BeginRezbitRecovery` (0x8022B1B0, 120 B): 3.33% -> **100.00%** on the third spelling

No Prime 1 counterpart and none needed - Rezbit does not exist in Prime 1. Read straight off
`./tools/dis.sh 0x8022b1b0 0x78`. It stores 2 into `mRezbitState` (4452 = 0x1164, which the header
already names), then shows a hint memo whose text is the **empty wide string** - no string-table
lookup and no string content. Display time 0.0f (the `.sdata2` word at -18608(r2) = 0x8041DB10,
read as a float, **not** guessed), `clearMemoWindow=false` / `fadeOutOnly=true` / `hintMemo=false`,
player mask `1 << GetPlayerIndex()`, `fadeInText=true`.

Body as committed:

```cpp
void CPlayer::BeginRezbitRecovery() {
  mRezbitState = kRS_Recovering;
  const int playerIndex = GetPlayerIndex();
  CSamusHud::DisplayHudMemo(rstl::wstring_l(L""),
                            CHUDMemoParms(0.f, false, true, false, 1 << playerIndex, true));
}
```

### 2. The finding: a `GetPlayerIndex()` **inside** the argument reverses retail's call order

This is the whole cost of the function and it is worth more than the function. Retail's order is
**`GetPlayerIndex` first**, result parked in `r31` **across** the `wstring_l` call; mwcceppc
evaluates the *arguments* of the nested expression in the other order, so it builds the text first
and hoists the `L""` pool load to the very top of the frame. One extra statement fixes it, and it
costs nothing - no copy of either temporary, and the stack slots stay retail's (parms at `r1+8`,
text at `r1+0x14`). Measured on this tree, all four spellings:

| spelling | score |
|---|---|
| `1 << GetPlayerIndex()` inline in the `CHUDMemoParms` argument (**one nested expression**) | 67.17% |
| `rstl::wstring text = rstl::wstring_l(L"");` named local, then the nested call | 53.20% |
| `const rstl::wstring text = rstl::wstring_l(L"");` (const) | 52.70% |
| `CHUDMemoParms parms(...); DisplayHudMemo(rstl::wstring_l(L""), parms);` | 46.87% |
| **`const int playerIndex = GetPlayerIndex();` as its own statement, then the nested call** | **100.00%** |

Note what the table says: a named local for the **text** is actively harmful, because
`rstl::wstring text = rstl::wstring_l(L"")` copy-constructs (the temporary is materialised at
`r1+0x14` and then copied to `r1+0x24`, the frame grows to 0x40, and a dtor appears). Naming the
**integer** is the fix, not naming the string. Generalises to any
`CHUDMemoParms(...)` / `DisplayHudMemo(...)` pair in this codebase.

### 3. `UpdateRezbitState` (0x8022B228, 228 B): 1.75% -> **98.25%**, four bytes short

Same two statements, same `GetPlayerIndex`-first shape, so it inherited the fix. Read off
`./tools/dis.sh 0x8022b228 0xe4`; nothing here is a guess:

- While `mRezbitState == kRS_Infected` and `mRezbitRecoveryTimer` (0x1170) is positive, it counts
  the timer down by `dt` and shows the memo the first time it reaches zero or below. Retail's
  `fcmpo` / `cror eq,lt,eq` is `<= 0.f`, written as a negation so the branch skips the block.
- `lwz`/`cmpwi r0,1` confirms the enum is an `int` compare, not a bit test - which is what
  `CPlayer::ERezbitState` already is, so no header change was needed.
- The memo's text is `gpStringTable->GetString("RezbitSuitSoftwareVirus")` - the name string is at
  0x803AD366, read out of `.rodata` (`52657a62 69745375 6974536f 66747761 72655669 72757300`).
- Its display time is **0x7f7fffff** (`lbl_8041DB14`), i.e. FLT_MAX.
- Then, unconditionally, if `mStaticTimer` (0x1148) is below 0.5f it calls
  `SetHudDisable(0.5f, 0.5f, 0.5f)`: `fmr f2,f1` / `fmr f3,f1` put **the constant** into all
  three arguments, not the field just compared.

**The `FLT_MAX` undef is load-bearing and is already a documented finding in this repo.**
libc/float.h's `FLT_MAX` is `(*(float*)__float_max)`, which makes mwcceppc materialise the address
in r3/r4 and load through it (`lis r3 / addi r4,r3 / lfs f1,0(r4)`). Retail reads the constant in
place, and retail's unit references no `__float_max` at all. Measured here: with
`#include <float.h>`'s `FLT_MAX` the function sat at **89.72%**; with
`#undef FLT_MAX` / `#define FLT_MAX 3.402823466e+38f` it reached **98.25%**. This is the same
finding and the same workaround as `src/MetroidPrime/PathFinding/CPathFindArea.cpp:17-22`, so the
notes above are not new - but nobody had connected it to this function, and it is worth 8.5
percentage points on its own.

### 4. What still blocks `UpdateRezbitState`, and why it is a real obstacle (not a spelling)

The remaining 4 bytes are one instruction, and the cause is **this unit's `.rodata` pool layout**,
which is shared with functions that are still unimplemented:

```
mine   lis r4, <pool>        ; addi r4,r4,<pool>     - 2 instructions, string at pool+0
retail lis r4, 0x803B        ; addi r4,r4,0xD348     ; addi r4,r4,0x1e  - 3 instructions
```

Retail's pool base is 0x803AD348 and the string sits at **+0x1e** into it, so the address needs
the extra `addi`. 0x803AD348 is inside a `.rodata` region `splits.txt` does not claim at all
(the nearest claims are `...0x803AC570` and `0x803AD880`), and the bytes there are
`506c6179 65722048 696e7420 64697361 626c6564 20636f6e 74726f6c 7300` = `"Player Hint disabled
controls\0"` - i.e. the pool is shared with `SetAreaPlayerHint` / `fn_8022af0c`, the two functions
the earlier runs recorded as blocked on the fieldless `CScriptPlayerHint` placeholder. So the
pool cannot be laid out correctly until **this unit's** remaining string users exist, no matter
what I write here. `objdiff` reports exactly one `DIFF_DELETE` + one `DIFF_REPLACE` for it; every
other instruction in the 228 bytes is already byte-identical.

**So this is not a wall and it is not a spelling problem** - it is a genuine ordering dependency
inside the unit, and the next run should not spend spellings on it. It resolves when
`SetAreaPlayerHint` or `fn_8022af0c` is written (which is the same `CScriptPlayerHint` blocker the
earlier sections already filed, not a new one). Deliberately **not** a `WALL:` line: the function
this run was asked to move rose 1.75% -> 98.25% and the item passes.

### Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11430 -> 11431   linked 5565 -> 5565
  ok    check_symbol_names.py
  ok    All:  32.82% fuzzy, 25.63% matched, 12.06% linked (11431 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 15 -> 16 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayervisor
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+1 functions at 100%` (`CPlayerVisor::BeginRezbitRecovery` 3.3333 -> 100.00), 0 units newly
  linked, **`no regression`** over 28465 functions. **This is the measurement that retires the
  "a string literal moves a shared unit by 32 bytes" worry for this unit** - no other unit moved.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CPlayerVisor`:
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `python3 tools/check_symbol_names.py`: `checked 514 units; 0 declared names are missing`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/link_check.sh`: `unchanged from baseline (250 undefined, 0 duplicates)` - the two new
  calls are all to symbols that already existed, so **no stand-in was needed** this run.
- Instruction-level check: `BeginRezbitRecovery` is **byte-identical** to retail across all 30
  instructions apart from the five unresolved `bl`/`sda21` relocations in the `.o`.
- `docs/HANDOFF.md` was rewritten by the gate (it runs with `MP_GATE_DOCS_WRITE=1`) and reverted,
  per the brief. It is not in the diff. The diff is **one file**,
  `src/MetroidPrime/Player/CPlayerVisor.cpp` (+58/-3): two function bodies, four includes, and the
  `FLT_MAX` undef. No header change, no config change, no `tools/` change, no `.s`.

### Per-function record (as the item's `reason` asked for)

| function | before | after | Prime 1's source |
|---|---|---|---|
| `BeginRezbitRecovery` | 3.3333% | **100.00%** | **no counterpart** - Rezbit does not exist in Prime 1. Read off the disassembly |
| `UpdateRezbitState` | 1.7544% | 98.25% | **no counterpart**, same. 4 bytes left, for the `.rodata`-pool reason above |
| `ResetPlayerHintState` | 1.2987% | 1.2987% | untouched (blocked as recorded; **do not re-derive - it is a naming miss, see the L2 section**) |
| `SetAreaPlayerHint` | 0.5907% | 0.5907% | untouched (blocked on the fieldless `CScriptPlayerHint` placeholder) |
| `UpdatePlayerHints` | 0.4386% | 99.5614% | untouched - already at that score from lane L2, not re-derived |

Prime 1's source was **not used** for either function: neither exists in Prime 1, and
`grep -rn Rezbit prime-ref/src/MetroidPrime/Player/` is empty.

### What is left in this unit, restated with this run's measurements

Unchanged from the L2/L7 lists except that the two HUD-memo functions are now off the list:

- `fn_8022B64C` (100 B, 0%) is **not source**: a compiler-generated performance-measurement
  wrapper (`memcpy` + `__ptmf_scall`). Not reachable from C++ source; skip it.
- `ResetRezbitState` / `StopRezbitState` are already at 100% (landed by an earlier run).
- `SetAreaPlayerHint` (948 B) - still blocked on the fieldless `CScriptPlayerHint` placeholder.
  **It is now also the thing that unblocks `UpdateRezbitState`'s last 4 bytes** (its
  `"Player Hint disabled controls"` string is at the head of this unit's `.rodata` pool), so the
  cheapest route to both is to lay that class out.
- `StartRezbitState` (832 B) and `fn_8022af0c` (460 B) - unchanged; `fn_8022af0c` is the other
  `.rodata` pool user (it loads the same 0x803AD348 base at 0x8022AF50), and still needs the shared
  break-hint enum recovered.
- `UpdatePlayerHints` at 99.5614% - the one dead `b`, 40+ spellings tried by lane L2. Untouched.

No `WALL:` (the function this run targeted reached 100%, and the second rose 63 points and is
blocked on a structural dependency, not on spellings). No `NEW:` - the only new obstacle is the
`.rodata` pool ordering, which is the *same* `CScriptPlayerHint` blocker already filed, not new
work. No `STALE:`.

### Files touched

- `src/MetroidPrime/Player/CPlayerVisor.cpp` - `BeginRezbitRecovery` and `UpdateRezbitState` bodies,
  four includes, and the `FLT_MAX` undef. Nothing else.

Not committed, as instructed.
