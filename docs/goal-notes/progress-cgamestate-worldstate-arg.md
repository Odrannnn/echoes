# progress-cgamestate-worldstate-arg

Item: `progress-cgamestate-worldstate-arg` (`kind: progress`), target
`MetroidPrime/Player/CGameState`. Worktree `wt-mp2-goal-L3`, branch `goal/lane-3`, head
`23a843aa`. Nothing committed.

## The decision the item asked for

The previous run filed this because reproducing retail's extra `saveWorld` argument "would mean
adding a second overload to a `Matching` unit, which changes the linked DOL". **That premise is
wrong, and the measurement that settles it is one instruction wide.** Both callees leave the
incoming `r5` unread:

```
$ ./tools/dis.sh 0x801721FC 0xF0 | grep -E 'r5'      # PutTo__16CWorldLayerState
80172298:  stw r28,12(r1)      ... only scratch uses of r5; no read of the argument
$ ./tools/dis.sh 0x801722EC 0x114 | grep -E '\br5\b' # __ct__16CWorldLayerState
80172348:  lwz  r5,0(r6)       ... first write to r5 is scratch, long after the prologue
```

while both *callers* set `r5` on purpose:

```
80145020  mr   r5,r31 ; 80145024  bl 0x801721FC   (PutTo__11CWorldState)
8014523C  mr   r5,r31 ; 80145240  bl 0x801722EC   (__ct__11CWorldState, saveWorld in r31 from r6)
```

So the parameter is invisible inside `CWorldLayerState` and only exists at the call site. Adding
it to the existing one-parameter signatures emits **no new function and moves no byte** of either
callee. The 2-parameter version does not have to "live somewhere else": it *is* the same
function, and the only thing that changes outside the code is the **mangled name**, which is a
`config/G2ME01/symbols.txt` rename, not a DOL change.

## The change (four files)

- `include/MetroidPrime/CWorldLayerState.hpp` - forward-declare `CWorldSaveGameInfo`; add
  `const CWorldSaveGameInfo&` to `explicit CWorldLayerState(CBitStreamReader&, ...)` and to
  `PutTo(CBitStreamWriter&, ...)`. No new overload, so the object still emits exactly retail's 15
  functions.
- `src/MetroidPrime/CWorldLayerState.cpp:18,26` - the two definitions take the parameter
  **unnamed** (`const CWorldSaveGameInfo&`), because retail never reads it and naming it would
  invite a future `-Wunused-parameter` edit that changes nothing.
- `src/MetroidPrime/Player/CGameState.cpp:515,523` - the only two callers in the tree pass
  `saveWorld`: `rs_new CWorldLayerState(in, saveWorld)` and `mLayerState->PutTo(out, saveWorld)`.
  (Grepped `src/` and `include/`: `CWorldLayerState.cpp:16` `rs_new CWorldLayerState` and
  `CWorld.cpp`/`CGameArea.cpp`/`CMemoryCard.cpp`/`main.cpp`/`PortGlobals.cpp` never call the ctor
  or `PutTo`.)
- `config/G2ME01/symbols.txt:6086-6087` - the two renames. Addresses and sizes are untouched;
  only the names gain `RC18CWorldSaveGameInfo`:

  ```
  PutTo__16CWorldLayerStateCFR16CBitStreamWriter  -> PutTo__16CWorldLayerStateCFR16CBitStreamWriterRC18CWorldSaveGameInfo  = .text:0x801721FC
  __ct__16CWorldLayerStateFR16CBitStreamReader   -> __ct__16CWorldLayerStateFR16CBitStreamReaderRC18CWorldSaveGameInfo   = .text:0x801722EC
  ```

  The mangled spellings were **measured, not guessed**: `tools/probe_cc.sh` on the edited
  `CWorldLayerState.cpp` and `powerpc-eabi-nm` on the result. `check_symbol_names.py` is the tool
  that exists to police exactly this rename, and it is clean (see the gates).

### Why the rename is safe for the RELs

Nothing outside the DOL refers to the old names, checked two ways: no REL object under
`build/G2ME01/*/*.o` has an undefined reference to either name (`grep -rl` over all 86 modules),
and the gate links all 86 and compares every sha1 against `config.yml`. dtk regenerates
`build/G2ME01/asm/MetroidPrime/Player/CGameState.s` from `symbols.txt` each configure, so the
retail-derived base object carries the new names too - visible at
`build/G2ME01/asm/MetroidPrime/Player/CGameState.s:3542` and `:3680`.

## Measured result

`PutTo__11CWorldState` reaches 100% and counts as matched. `__ct__11CWorldState` does **not**,
for a reason unrelated to this change (below).

```
main/MetroidPrime/Player/CGameState   matched_functions 106 -> 107 / 116
main/MetroidPrime/CWorldLayerState    15 / 15, still complete (Matching), fuzzy 100.00% -> 100.00%
All:                                   matched 12519 -> 12520, linked 5892 -> 5892
```

`tools/report_diff.py` (gate step 4):

```
matched  12519 -> 12520   linked 5892 -> 5892   (+3 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CWorldLayerState :: PutTo__16CWorldLayerStateCFR16CBitStreamWriterRC18CWorldSaveGameInfo
  +100%    main/MetroidPrime/CWorldLayerState :: __ct__16CWorldLayerStateFR16CBitStreamReaderRC18CWorldSaveGameInfo
  +100%    main/MetroidPrime/Player/CGameState :: PutTo__11CWorldStateCFR16CBitStreamWriterRC18CWorldSaveGameInfo
  RENAMED  ...PutTo__16CWorldLayerStateCFR16CBitStreamWriter -> ...RC18CWorldSaveGameInfo (100.00% -> 100.00%)
  RENAMED  ...__ct__16CWorldLayerStateFR16CBitStreamReader -> ...RC18CWorldSaveGameInfo (100.00% -> 100.00%)
  WORSE    main/MetroidPrime/Player/CGameState :: __ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo 97.38% -> 97.17%
no regression
```

The two `+100%` lines on `CWorldLayerState` are the rename read as a new name; both stay at 100%
and the unit stays `Matching`, which is the point - **the DOL did not move**.

### The `WORSE` line is not a regression, and the bytes say why

`__ct__11CWorldState` went 97.38% -> 97.17%, which `report_diff.py` classifies as a *signal*
(the unit was not `Matching` in the baseline) rather than a failure, and the judge passed it.
The honest reading is that the function did not get worse:

```
$ python3 tools/bytescmp.py build/G2ME01/obj/MetroidPrime/Player/CGameState.o \
      __ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo 0x80145068 0x238
...
35 differing instructions of 142 (568 bytes ours vs 568 retail)
```

All 35 are `bl` / `lis`+`addi` pairs carrying a **relocation**, which `bytescmp.py` counts as
differences and which are filled in by the linker. Every non-relocated byte matches, size is
568 on both sides, and the sequence around the changed call site is identical to retail
(`mr r4,r30 ; mr r5,r31 ; bl __ct__16CWorldLayerState... ; mr r28,r3`, retail 0x80145238..44,
ours `30b0..30bc` in the object). The residual percentage is objdiff scoring *relocation target
names*: `fn_800B8CF4` vs `__ct__13CRelayTrackerFR16CBitStreamReaderRC18CWorldSaveGameInfo`,
`fn_80009008` vs `ReleaseData__Q24rstl23rc_ptr<13CRelayTracker>Fv`, `fn_80009224` vs
`ReleaseData__Q24rstl26rc_ptr<16CWorldLayerState>Fv`. That is the same known limitation the
previous run recorded, not a code change.

`objdiff-cli diff` also prints a phantom `lwz r5, kInvalidAreaId@sda21` in the retail stream of
this symbol; the retail source of truth does not contain it -
`build/G2ME01/asm/MetroidPrime/Player/CGameState.s:3557` reads `li r7, -0x1` and the DOL at
0x80145070 is `38 e0 ff ff`. It is alignment noise from the greedy stream matcher after the
name mismatches above, and the `.s` is the better instrument.

## Gates (all on this tree, this run)

```
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12519 -> 12520   linked 5892 -> 5892
  ok    check_symbol_names.py
  ok    All:  35.33% fuzzy, 29.17% matched, 12.91% linked (12520 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 106 -> 107 / 116 functions
  ok    no asm added
goal_check: PASS progress-cgamestate-worldstate-arg

sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
build/gate-probe.log  probe: 763 files, 0 failed, 0 errors; LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py        checked 525 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit main/MetroidPrime/CWorldLayerState
                                           ok: none emits its functions out of retail order
```

`docs/HANDOFF.md` is modified in the tree only because `goal_check.sh` runs
`check_docs_claims.py --write`; the driver discards that. No commit was made.

## Notes for the next run on this unit

- **A `Matching` unit can take a signature change.** The rule that matters is "no *new* symbol in
  the object", not "the header text is frozen". If the added parameter is unread by the body,
  mwcceppc emits the same bytes; verify with `unit_fit.sh` + the DOL sha1 rather than assuming.
- **`__ct__11CWorldState` cannot reach 100% from `CGameState.cpp`.** Its 35 differing
  instructions are all relocations to callees named `fn_800B8CF4`, `fn_80009008` and
  `fn_80009224`, which are `CRelayTracker`'s ctor and two `rstl::rc_ptr::ReleaseData`
  instantiations that this unit does not own. Closing it means renaming those three in
  `symbols.txt` **and** making the owning units define them with those names - a different unit's
  work, not this one's.
- The same three-name residue keeps `main/MetroidPrime/Player/CGameState` below 100% overall
  (107/116); the remaining functions there are the same story. The unit is far from flipping.

---

# Run 2 — lane L6, 2026-10-02, head `6787750e`, nothing committed

**The change above did not survive.** It was never committed and lane L3's tree was discarded,
so this run started from a clean `6787750e` and re-applied it from scratch. Everything above is a
record of a run whose work is gone; the numbers below are the only ones measured on a tree that
still exists. Treat the sections above as prior context and this section as the verdict.

## Re-measured baseline (before the change, on 6787750e)

```
main/MetroidPrime/Player/CGameState   matched_functions 106 / 116, fuzzy 89.3957
main/MetroidPrime/CWorldLayerState    matched_functions 15 / 15, fuzzy 100.00 (Matching)
```

The item was **not** stale when claimed — `PutTo__11CWorldState` was still sub-100 on this tree.

## The decision the item asked for, and the evidence

The item asked "where may a 2-parameter overload live without changing the linked DOL". Answer:
**it does not have to live anywhere else.** The parameter is unread inside `CWorldLayerState` and
exists only at the call site, so it belongs on the existing one-parameter functions. The rule that
matters is "no *new* symbol in the object", not "the header text is frozen". The only thing that
changes outside the code is the **mangled name**, which is a `config/G2ME01/symbols.txt` rename,
not a DOL change.

## The change (four files, same shape as run 1)

- `include/MetroidPrime/CWorldLayerState.hpp:15,21-23,26` - forward-declare `CWorldSaveGameInfo`;
  add `const CWorldSaveGameInfo&` to the explicit ctor and to `PutTo`. No new overload, so the
  object still emits exactly retail's 15 functions.
- `src/MetroidPrime/CWorldLayerState.cpp:18-19,28` - both definitions take the parameter
  **unnamed**; retail never reads it and naming it would only invite a `-Wunused-parameter` edit.
- `src/MetroidPrime/Player/CGameState.cpp:515,523` - the only two callers in the tree pass
  `saveWorld`.
- `config/G2ME01/symbols.txt:6086,6087` - the two renames, addresses and sizes untouched.

The mangled spellings were **measured on this tree**, not copied: `report.json` lists the unit's
15 functions under the new names and all 15 read 100%.

## Measured result

```
main/MetroidPrime/Player/CGameState   matched_functions 106 -> 107 / 116
main/MetroidPrime/CWorldLayerState    15 / 15, still Matching, fuzzy 100.00%
```

`PutTo__11CWorldState` reached 100% and counts as matched. The `__ct__11CWorldState` residue is
97.17% (from 97.38%), which `report_diff.py` prints as a `WORSE` line but classifies as a *signal*
on a `NonMatching` unit rather than a regression — see the next section for why it is not one.

## `__ct__11CWorldState`: what actually blocks it (supersedes run 1's stated cause)

Run 1 recorded the residue as objdiff scoring **callee names** that differ from retail's, and
advised closing it by renaming `fn_800B8CF4` / `fn_80009008` / `fn_80009224` plus their owners.
**That cause is stale and the advice is wrong work.** On this tree our object's relocations in
that function already carry retail's own names:

```
$ build/binutils/powerpc-eabi-objdump -r --section=.text \
    build/G2ME01/obj/MetroidPrime/Player/CGameState.o    # __ct__11CWorldState sits at 0x2ee0
00002ff4 R_PPC_REL24  fn_800B8CF4
00003000 R_PPC_REL24  fn_80009008
000030c4 R_PPC_REL24  fn_80009224
000030b8 R_PPC_REL24  __ct__16CWorldLayerStateFR16CBitStreamReaderRC18CWorldSaveGameInfo
```

and the dtk-regenerated retail `.s` for the same function
(`build/G2ME01/asm/MetroidPrime/Player/CGameState.s:3554-3706`) contains exactly those same
names — `bl fn_800B8CF4`, `bl fn_80009008`, `bl fn_80009224`. **Nothing needs renaming.**
`src/MetroidPrime/main.cpp:1903-1920` already documents why those names are retail's own
placeholders and must stay: objdiff pairs by name, and the weak COMDAT
`ReleaseData__Q24rstl23rc_ptr<16CWorldLayerState>Fv` the template would emit would be scored
against nothing.

What the residue actually is, measured here rather than asserted:

```
$ python3 tools/bytescmp.py build/G2ME01/obj/MetroidPrime/Player/CGameState.o \
      __ct__11CWorldStateFR16CBitStreamReaderUiRC18CWorldSaveGameInfo 0x80145068 0x238
35 differing instructions of 142 (568 bytes ours vs 568 retail)
```

All 35 are `bl`, or the `lis`/`addi` half of a `lbl_803A9208@ha/@l` address pair. I did not take
that on the instruction text's authority: I cross-referenced the 35 offsets against all 679
relocations in `.text` and every one falls inside a relocated field (the `lis` halves are covered
by the `R_PPC_ADDR16_HA` at `+2`). Sizes are equal at 568 bytes on both sides, and the call
sequence around the changed site is byte-identical to retail
(`mr r4,r30 / mr r5,r31 / bl __ct__16CWorldLayerState... / mr r28,r3`, retail 0x80145238..44).

**So there is no register-allocation or scheduling difference left to chase — every remaining
byte is linker-filled.** No C++ spelling can move it, which is why this run did not write a
`WALL:` line: it is not a wall of spellings, it is an objdiff scoring artefact. Leave 97.17%.

STALE: run 1's diagnosis that `__ct__11CWorldState` is blocked by callee *names* differing from
retail, and its advice to rename `fn_800B8CF4` / `fn_80009008` / `fn_80009224` in a third unit.
Re-measured on 6787750e: our relocations and the regenerated `.s` both read those exact retail
names. The 35 differing instructions of 142 are all relocated fields, 568 bytes on both sides.

## Gates (all on this tree, this run)

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12582 -> 12583   linked 5944 -> 5944
  ok    check_symbol_names.py
  ok    All:  35.44% fuzzy, 29.26% matched, 12.97% linked (12583 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 106 -> 107 / 116 functions
  ok    no asm added
goal_check: PASS progress-cgamestate-worldstate-arg

sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (retail)
gate.sh internal: GATE PASS 6787750e+5 changed
python3 tools/check_symbol_names.py        checked 528 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit main/MetroidPrime/CWorldLayerState
                                           ok: 1 unit(s) checked, none emits its functions out of retail order
```

`docs/HANDOFF.md` is modified in the tree only because `goal_check.sh` runs
`check_docs_claims.py --write`; I reverted it, and the driver discards it anyway. No commit made.

## Notes for the next run on this unit

- **Do not re-apply this item.** It is landed and measured: 106 -> 107 with a clean gate. A
  further run that finds `PutTo__11CWorldState` already at 100% should write `STALE:` and stop.
- **Do not chase the three-name residue.** Run 1's rename advice is wrong; the names are already
  retail's, and `main.cpp:1903-1920` explains why they must stay that way.
- `main/MetroidPrime/CWorldLayerState` is a `Matching` unit that has now survived a signature
  change. If you ever add an *unused* parameter to one, this is the worked example.
- `main/MetroidPrime/Player/CGameState` is 107/116 and far from flipping; the remaining functions
  are other stories.
