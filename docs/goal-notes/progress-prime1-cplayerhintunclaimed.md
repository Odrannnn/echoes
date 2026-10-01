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