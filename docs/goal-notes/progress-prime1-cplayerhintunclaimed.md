# progress-prime1-cplayerhintunclaimed

Both functions the item queued are done: `ResetRezbitState` (0x8022B164, 76 B) and
`StopRezbitState` (0x8022B0D8, 140 B) are both at **100.00%**, and the unit rose **13 -> 15 / 22**.
One announced stand-in for `fn_8022EA5C` was all the gap-function blocker needed, as the item's
`reason` predicted.

## What I did

1. **`fn_8022EA5C` declared** at the end of `include/MetroidPrime/Player/CPlayer.hpp` (after
   `CHECK_SIZEOF`, before `#endif`), as `extern "C" void fn_8022EA5C(uint&, CStateManager&, int)`,
   with the gap and the body recorded in the comment. Retail's symbol carries no mangling
   (`fn_8022EA5C = .text:0x8022EA5C` in `symbols.txt`), so `extern "C"` is what binds it - the same
   reasoning `CPlayerStateRefRelease.cpp` gives for `fn_8000934C`.
2. **The stand-in** in `src/MetroidPrime/PortGlobals.cpp`, immediately after the existing
   `CHintManager::RemoveHint` stand-in - the same gap, and this is the established place for
   "retail symbol in an unclaimed gap, referenced by a unit we do compile". Not decompilation, not
   claimed to match retail, announces itself on first call, and deliberately does **not** clear the
   bit or touch the gun: those are the observable effects, so half of them silently is worse than
   none.
3. **`ResetRezbitState` and `StopRezbitState` written** in `src/MetroidPrime/Player/CPlayerVisor.cpp`
   from `tools/dis.sh`. Added `#include "MetroidPrime/Player/CGameState.hpp"` for `gpGameState`.

## The bodies, and what each field is

Both read off the disassembly, not inferred:

```cpp
void CPlayer::ResetRezbitState(CStateManager& mgr) {
  fn_8022EA5C(mRezbitEffectToken, mgr, mPlayerIndex);
  mRezbitState = kRS_None;
  mRezbitEffectId = kInvalidUniqueId;
  reinterpret_cast< char* >(gpGameState)[0xD8] = 0;
}

void CPlayer::StopRezbitState(CStateManager& mgr) {
  if (mRezbitState != kRS_None && mRezbitEffectId != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mRezbitEffectId);
    fn_8022EA5C(mRezbitEffectToken, mgr, mPlayerIndex);
    mRezbitState = kRS_None;
    mRezbitEffectId = kInvalidUniqueId;
    reinterpret_cast< char* >(gpGameState)[0xD8] = 0;
  }
}
```

Field resolutions worth keeping:

- **`lwz r5,5048(r3)` is `mPlayerIndex` (CPlayer+0x13B8), not `mgr+0x13B8`.** lane L1's notes
  transcribed the third argument as `mgr+0x13b8`; the register is `r3` = `this` in
  `ResetRezbitState` and `r30` = `this` in `StopRezbitState`, in both cases loaded *before*
  `addi r3,r30,4456` replaces `r3`. It is a `CPlayer` field, and the header's own layout puts
  `mPlayerIndex` at 0x13b8 (it is four pointers before `mRezbitRecoveryDirection` at 0x13c8).
  This also settles the gap function's meaning: `fn_8022EA5C` tests the caller's player-index bit
  and then indexes `mgr.m_players[playerIndex]`, which is what `mPlayerIndex` is for.
- **`+4456` = 0x1168 = `&mRezbitEffectToken`**, `+4452` = 0x1164 = `mRezbitState` (a word store),
  `+4460` = 0x116c = `mRezbitEffectId` (a halfword store). All three already in the header.
- **`li r4,0` is shared** by `stw r4,4452(r30)` and `stb r4,216(r3)`, so the two zeroes come out of
  one register: `= kRS_None` and `= 0`, no second constant. That falls out of the source as written.
- **`-27740(r13)` is `kInvalidUniqueId`** and **`-28360(r13)` is `gpGameState`**; `216 = 0xD8`.
  The neighbouring `0xD9` byte is `CScanDisplay`'s (`CScanDisplay.cpp:75` writes
  `reinterpret_cast< char* >(gpGameState)[0xD9]`), so `0xD8` is a distinct flag.
- **`StopRezbitState`'s guard is `&&`, not nested.** Both `beq`s target `0x8022b14c`, which is the
  epilogue, so nothing of the body runs unguarded.
- **`mRezbitEffectId` is loaded twice** - once for the compare, once for the by-value `TUniqueId`
  temporary `DeleteObjectRequest` takes (`addi r4,r1,8 ; sth r0,8(r1)`). mwceppc reproduces both
  loads from `mgr.DeleteObjectRequest(mRezbitEffectId)` with the existing by-value declaration; no
  header change was needed for that.

## Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11412 -> 11414   linked 5523 -> 5523
  ok    check_symbol_names.py
  ok    All:  32.80% fuzzy, 25.54% matched, 11.98% linked (11414 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 13 -> 15 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayerhintunclaimed
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+100% ResetRezbitState`, `+100% StopRezbitState`, 0 units newly linked, **`no regression`**.
- `build/gate-linkcheck.log`: `unchanged from baseline (250 undefined, 0 duplicates)` - the
  stand-in is what keeps it at 250.
- `build/gate-probe.log`: `751 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`.
- `python3 tools/check_decl_order.py`: `977 unit(s) checked, 31 permuted, all 31 accounted for` -
  `CPlayerVisor` is not one of them. Its definitions were already in descending retail order and I
  did not move them (`StopRezbitState` 0x8022B0D8 before `ResetRezbitState` 0x8022B164).
- No config change, no `tools/` change, no `.s`, no `configure.py` edit. `CPlayer`'s layout is
  unchanged: I added a free-function declaration outside the class, and `CHECK_SIZEOF(CPlayer,
  0x14c8)` still holds.
- The unit stays `NonMatching`; nothing here claims otherwise.

## Files touched

- `include/MetroidPrime/Player/CPlayer.hpp` - the `fn_8022EA5C` declaration and its comment, after
  `CHECK_SIZEOF`. No layout change.
- `src/MetroidPrime/PortGlobals.cpp` - the announced stand-in, beside `CHintManager::RemoveHint`.
- `src/MetroidPrime/Player/CPlayerVisor.cpp` - the two bodies, plus the `CGameState.hpp` include.

Not committed, as instructed.

## What is left in this unit, and what each one needs

Unchanged from lane L1/L2/L3's lists apart from the two functions above:

- `fn_8022B64C` (100 B, 0%) is **not source**: a compiler-generated performance-measurement
  wrapper (`memcpy` + `__ptmf_scall`). Not reachable from C++ source; skip it.
- `UpdateRezbitState` (228 B), `BeginRezbitRecovery` (120 B) and `StartRezbitState` (832 B) build a
  HUD memo: `rstl::basic_string<w>` ctor + `CStringTable::GetString` + `CHUDMemoParms` ctor +
  `CSamusHud::DisplayHudMemo` + the string dtor. Needs a string literal in this unit, which the
  brief warns can move a shared unit by 32 bytes.
- `fn_8022af0c` (460 B) still needs the shared break-hint enum recovered.
- `SetAreaPlayerHint` (948 B) still needs the fieldless `CScriptPlayerHint` placeholder replaced.

**The gap function itself is still owed.** `fn_8022EA5C` is 0x64 bytes and its body is now fully
transcribed in the header comment, but it cannot be decompiled here: claiming 0x8022E13C..0x8022EB9C
means carving a unit out of dtk's `auto_03_8022E13C_text.o`, which holds ten functions
(`fn_8022E13C`, `fn_8022E5F4`, `fn_8022E6E8`, `fn_8022E87C`, `fn_8022E8DC`, `fn_8022E9F0`,
`fn_8022EA5C`, `fn_8022EAC0`, `fn_8022EB54`, `fn_8022EB90`) - a separate item.

No `WALL:` (both functions attempted this run reached 100%), no `NEW:` (the remaining blockers are
the ones already recorded above, and the gap carve is not this item's shape), no `STALE:`.

### One correction for whoever reads lane L1's notes next

L1 recorded the third argument of `fn_8022EA5C` as `mgr+0x13b8`. It is `this->mPlayerIndex`
(`CPlayer+0x13B8`). The header's layout is the authority and it is unambiguous here.
---

# Run 2 (lane L4, `goal/lane-4`, 2026-10-01) — `UpdateRezbitState` 98.25% -> 100%

Re-measured the branch head first: `build/goal/judge/report.base.json` had the unit at **16 / 22**,
so the item's two queued functions (`ResetRezbitState`, `StopRezbitState`) were indeed already
100% and this run had to find something else. It did: **`UpdateRezbitState` is now 100.00%** and
the unit is **16 -> 17 / 22**. Nothing got worse anywhere.

## The finding: mwcceppc pools string literals in *emission* order

`UpdateRezbitState` (0x8022B228, 228 B) was the closest thing in the unit to a flip and it sat at
98.24561% - exactly **4 bytes**, one instruction. Measured, not guessed:

```
retail   8022b278  lis  r4,lbl_803AD348@ha
         8022b280  addi r4,r4,lbl_803AD348@l
         8022b288  addi r4,r4,30            <- the missing instruction
         8022b28c  bl   GetString
ours     8022...   lis  r4,@stringBase0@ha
         8022...   addi r4,r4,@stringBase0@l
                  (no +30)
```

`lbl_803AD348` is the **base of this unit's string pool**, not the string itself. Read out of
`main.elf` with correct offsets:

| address | contents |
|---------|----------|
| 0x803AD348 | `"Player Hint disabled controls"` (29 + NUL = 30) |
| 0x803AD366 | `"RezbitSuitSoftwareVirus"`  = base + **30** |
| 0x803AD37E | `"Rezbit Control Hint"` |
| 0x803AD392 | `"?\(??\)"` |

The pool is ordered by **the order the functions are emitted**, which is reverse source order. So
`GetString("RezbitSuitSoftwareVirus")` is `base + 30` only because some *earlier-emitted* function
in the unit contributes the 30-byte string at offset 0. Retail's earlier-emitted function is
`fn_8022af0c` (0x8022AF0C, the **first** function in `.text`, hence the **last** definition in the
source), and it uses `"Player Hint disabled controls"` with **no** displacement - confirmed by its
own `lis r3,0x803b / addi r4,r3,-11448` at 0x8022AF50/0x8022AF58. Before this change our unit's
`.rodata` held only `"RezbitSuitSoftwareVirus"`, so the base *was* that string and the `+30` could
not appear. Nothing about `UpdateRezbitState`'s source is wrong; the pool it draws from was
incomplete.

**Proved by experiment, not reasoned about:** a throwaway edit that put the literal in
`fn_8022af0c` and used it via an already-undefined symbol produced `.rodata` =
`"Player Hint disabled controls\0RezbitSuitSoftwareVirus\0"` byte-identical to retail's prefix,
and `UpdateRezbitState` grew the `addi r4,r4,30` with the rest of the function already aligned.
`objdiff-cli diff` on the two objects then went from 96.84% to 99.47% raw, and 100.00% in
`report.json` (the raw score leaves the SDA21 and pool relocations unresolved; `report generate`
resolves them - the same reason `BeginRezbitRecovery` reads 99.67% raw and 100.00% reported).

## What I changed

1. **`include/MetroidPrime/Player/CPlayer.hpp`** - `#include "rstl/string.hpp"`, and after the
   `fn_8022EA5C` declaration, a declaration of **`fn_8022A640`** (retail 0x8022A640, 0x22C bytes,
   in the unclaimed gap 0x8022A5AC..0x8022AF0C - `splits.txt` ends
   `MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` at 0x8022A5AC and `CPlayerVisor.cpp` starts at
   0x8022AF0C). Its parameter list is read off two places and nothing else: the argument setup at
   0x8022AFF8..0x8022B054, and the callee's own prologue (`mr r22,r3` .. `mr r29,r10`,
   `fmr f29,f1 / f30,f2 / f31,f3`, then `lwz r30,248(r1)` .. `lwz r19,264(r1)` for five stack
   words). Those five stack words are **not** recovered, so the declaration stops at the register
   parameters. `CHECK_SIZEOF(CPlayer, 0x14c8)` still holds; nothing in the class changed.
2. **`src/MetroidPrime/Player/CPlayerVisor.cpp`** - `fn_8022af0c` written from `tools/dis.sh
   0x8022AF0C 0x1CC`: the `mControlHintManager == nullptr` guard returning `kInvalidUniqueId`,
   the `rstl::string("Player Hint disabled controls")` construction, and the `fn_8022A640` call.
   **2.57% -> 30.56%.**
3. **`src/MetroidPrime/PortGlobals.cpp`** - the announced stand-in for `fn_8022A640`, beside
   `fn_8022EA5C` and `CHintManager::RemoveHint`, for the same reason: a symbol in an unclaimed gap
   that a unit we compile now calls would take the port from 250 to 251 undefined, which
   `link_check.sh --strict` fails. It answers `kInvalidUniqueId` and announces itself; retail's
   0x22C-byte body creates the hint, and a plausible id would be worse than the invalid one.

### One ABI fact worth keeping (it settles `fn_8022af0c`'s register layout)

This toolchain returns a **class-typed** value through a hidden pointer in r3, and passes class
types **by reference**. Three independent confirmations in this unit and its neighbours:
`AllocateUniqueId(mgr)` is called as `addi r3,r1,36 ; mr r4,r30` with the result read back by
`lhz r0,36(r1)`; `CStateManager::DeleteObjectRequest(mRezbitEffectId)` takes `addi r4,r1,8` with
the `TUniqueId` materialised at r1+8; `CHintManager::RemoveHint(a, b, mgr)` takes
`addi r4,r1,16 ; addi r5,r1,12`. So `fn_8022af0c`'s r3 is the return slot, r4 is `this`,
r5 `mgr`, r6 `controls`, r7 `&source`, r8 `breakType`, f1 `duration` - which is exactly the header's
declared parameter list, and `fn_8022A640` receives the same pointer in r3, so
`return fn_8022A640(...)` with the string's destructor running after the call is what retail does.

## Verification

`./tools/goal_check.sh build/goal/item.json`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11435 -> 11436   linked 5572 -> 5572
  ok    check_symbol_names.py
  ok    All:  32.82% fuzzy, 25.65% matched, 12.11% linked (11436 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerVisor: 16 -> 17 / 22 functions
  ok    no asm added
  goal_check: PASS progress-prime1-cplayerhintunclaimed
```

- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:
  `+100% UpdateRezbitState`, 0 units newly linked, **`no regression`** (fn_8022af0c's
  2.57 -> 30.56 is not a regression and is not counted as a match).
- `build/gate-linkcheck.log`: `unique undefined symbols 250` (base 250) - the new stand-in is what
  holds it there.
- `build/gate-probe.log`: `752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Player/CPlayerVisor`: `none emits its
  functions out of retail order`. `fn_8022af0c` grew, but it is the first function in `.text`, so
  nothing after it moved.
- No `configure.py` / `config/` / `tools/` / `build/goal/` change except this notes file, no `.s`,
  no new `src/` file. Not committed, as instructed.

## What is left in this unit, measured this run

- `UpdatePlayerHints` (912 B) is at **99.5614%** - **4 bytes, one instruction**: retail has a
  third `b` to the epilogue inside the `if (x1268_30_)` block (at +0xBC, between the merged
  `mControlDir = mControlDirFlat = CVector3f(0,1,0)` block and the *dead* copy of that block that
  follows it) that we do not emit. Our source already produces the dead copy at the same place, so
  the difference is purely mwcceppc's tail-merging of the inner-else and outer-else bodies; I did
  not find a spelling that keeps the redundant branch and did not chase it. **Not a `WALL:`** - I
  measured the cause, I did not try several spellings.
- `fn_8022B64C` (100 B, 0%) - still the compiler-generated PTMF wrapper; not source.
- `StartRezbitState` (832 B, 0.48%), `SetAreaPlayerHint` (948 B, 0.59%) - untouched.
- `fn_8022af0c` (460 B, 30.56%) - now owes the five stack words `{breakType, 0, &local, &local, 0}`
  at the caller's r1+8..r1+27 and the rest of `fn_8022A640`'s 0x22C bytes.

No `WALL:`, no `NEW:` (the remaining blockers are the ones already recorded above, and this run's
other finding - the pool ordering rule - is a lesson, not an item), no `STALE:`.

## Rules worth carrying to the next unit (general, measured here)

1. **A sub-100% function that is 4 bytes short is usually a missing data-pool sibling, not a
   missing statement.** Count the bytes: `100 - pct` times the size is how many bytes differ, and
   if it is one instruction it is a relocation/displacement, not logic.
2. **Resolve the pool base before rewriting the function that uses a literal.** `tools/sda.py`
   plus a read of the pool in `main.elf` gives the base, the offsets and the neighbours; the
   neighbour that owns offset 0 is in the function emitted *before* the one you are fixing, i.e.
   the one defined *later* in the source.
3. **mwcceppc's string pool is ordered by emission, not by source position.** A unit whose
   relocations carry a non-zero displacement needs every literal that retail pooled ahead of it,
   even in a function that is otherwise unrelated - which is why a `progress` item on one function
   can be blocked on a literal that belongs to a different, much larger function.
