# port-boot-stub-zn23cgamestateenvvarmanager10loadfieldse-38068b1

`kind: port`, target `_ZN23CGameStateEnvVarManager10LoadFieldsEv`, `verify: boot-progress.sh`.
PASS - `./tools/goal_check.sh build/goal/item.json` exits 0 in the worktree.

## What the stub was hiding

Retail 0x80145C98 (756 bytes, `symbols.txt`'s `fn_80145C98`) is `CGameStateEnvVarManager::LoadFields`
- the header already declares it, both constructors in `CGameState.cpp` already call it, and
`fn_80146154` (`CPersistentOptionsCtor.cpp`) is nothing but the stores plus one call to it. **The body
was written and fully matching at 100.00%, but under `extern "C" fn_80145C98`**, so the port's
mangled `_ZN23CGameStateEnvVarManager10LoadFieldsEv` had no definition anywhere and both
constructions reached `PortReachStubs.cpp`'s print-a-line alias
(`build-boot-probe/run.log`, `[reach-stub 0006]`, twice per boot). The eleven default persistent
options were never inserted.

## The change

- `src/MetroidPrime/Player/CGameState.cpp:288-330` - the definition becomes
  `CGameStateEnvVarManager::LoadFields()`. The body is unchanged, including the order: `self` is
  bound **before** the scope branch, because after it mwcceppc hoists `mr r31,r3` to its first use
  and swaps two instructions out of retail's order. `self` is a `reinterpret_cast` of `this` to
  `CPersistentOptions*` because `fn_80145ACC` is declared that way.
- `include/MetroidPrime/Player/CGameStateEnvVarManager.hpp` - `LoadFields` moved from `private` to
  public. It has to be: `CPersistentOptionsCtor.cpp` is a *different class* and calls it, and with
  it private the only way to reach it from there was the `extern "C"` alias that caused this.
- `src/MetroidPrime/Player/CPersistentOptionsCtor.cpp:50-95` - calls `self->LoadFields()` through the
  header. It previously declared `extern "C" void fn_80145C98(CPersistentOptions*)`; that file is in
  `files.cmake` but **not** a `configure.py` unit (the range is claimed by `CGameState.cpp`), so it is
  compiled only by the port, by the host compiler - where a C-linkage retail spelling names a symbol
  nothing defines. With `LoadFields` reachable through the header both spellings agree.
- `src/MetroidPrime/PortReachStubs.cpp:1669` - `reachstub_522` deleted, replaced by a comment naming
  where the body is. Keeping the alias would be a duplicate definition under `MP_BOOT_STUBS=ON`.
- `config/G2ME01/symbols.txt:5429` - `fn_80145C98` renamed to
  `LoadFields__23CGameStateEnvVarManagerFv` so objdiff pairs the member. **Intended config change,
  one line, no address or size touched.** Without it the function drops out of pairing entirely.
- `docs/research/port_link_gap_list.md` + `port_link_gap.md` - two entries removed by
  `link_gap.py --write-list`, and the gap table's `other game methods` count and total corrected.
  `_ZN23CGameStateEnvVarManager10LoadFieldsEv` closed by this item. `_ZN15CGMSinglePlayerC1Ev` was
  **already stale on the clean tree** (`PortCGMSinglePlayer.cpp:35` defines it and nothing had removed
  the entry), so the gate failed before my change too; measured with the change stashed:
  `python3 tools/link_gap.py --rebuild` -> `317 MISSING`, one stale entry, `_ZN15CGMSinglePlayerC1Ev`.

## Measured

- `ninja build/G2ME01/src/MetroidPrime/Player/CGameState.o`, then `cmp` of
  `objdump -s -j .text` before/after: **identical**, 0x6914 bytes. The rename moves only the symbol.
- `./tools/gate.sh build/goal/judge/report.base.json`: `GATE PASS 38068b13+7 changed`, main.dol
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs vs `config.yml`, per-function diff
  `matched 11959 -> 11959, linked 5728 -> 5728`, `LoadFields__23CGameStateEnvVarManagerFv` at 100.00%
  and reported `RENAMED fn_80145C98 -> LoadFields__23CGameStateEnvVarManagerFv (100.00% -> 100.00%)`.
- `python3 tools/check_symbol_names.py`: 516 units, 0 missing.
- `python3 tools/check_decl_order.py --unit CGameState`: ok, none permuted.
- `./tools/goal_verify/boot-progress.sh`: `BOOT_PROGRESS PASS: all 2 runs got further than all 2 head
  runs`. Every marker kept, exit code 0, the judge's own frame budget reached. The verdict names
  the two newly reached
  stubs, which is retail's behaviour appearing: `fn_80145ACC` (the map insert) and
  `_ZN23SPersistentOptionsValueC1Eiii` (the value's constructor), each hit per row, because
  `LoadFields` now does the work instead of printing.
- `./tools/link_check.sh`: `unique undefined symbols 322 -> 321`, `duplicate definitions 0`,
  compile errors 0. `NOT LINKED` is the baseline state (see `build/goal/judge/record-link.log`).
- `goal_check`: `port undefined 322 -> 321`, `probe: 749 files, 0 failed, 0 errors; link: LINKED
  (321 undefined, 0 duplicates)`.

## Next

`fn_80145ACC` (`CPersistentOptionsMapInsert.cpp`, retail 0x80145ACC, NonMatching 71.37%) is now the
most-hit stub on the boot path, and `SPersistentOptionsValue`'s constructor with it. Both are the
natural next items; the map insert relocates against `fn_80146338` (retail 0x80146338, the rbtree node
insert), which nothing in the tree implements, so closing it needs that first.

NEW: port-boot-stub-fn-80145acc-38068b1 | port | fn_80145ACC | LoadFields now runs and hits this stub once per row; CPersistentOptionsMapInsert.cpp is written at 71.37% and is NonMatching because it relocates against fn_80146338, the rbtree node insert, which nothing implements
NEW: port | SPersistentOptionsValue::SPersistentOptionsValue | _ZN23SPersistentOptionsValueC1Eiii | hit per row by the same new call path; SPersistentOptionsValueCtor.cpp is Matching at 100% but is deliberately not listed in files.cmake because listing it takes 325 -> 325 and closes nothing

## Lane 1: passed, then failed on the moved tip (2026-10-01 19:19:38Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 9f2d849f224a; re-do it against the current tip.

## Lane 1, second attempt on 9f2d849: PASS (2026-10-01)

`./tools/goal_check.sh build/goal/item.json` exits 0 on the current tip. The change is the same
five-file one the first attempt made, plus the gap-list regen; nothing had to be re-derived about
the code, because `git diff 38068b13..9f2d849f -- src/MetroidPrime/Player/CGameState.cpp` touches
`fn_80143CD4`/`CFrontEndGameMode` only and leaves the `fn_80145C98` region untouched.

### Why the re-judge failed last time - both causes are now measured, and one is not the code

`run_goal.sh`'s `rebase_onto_tip` stages the item's **notes file as
`docs/goal-notes/<id>.md`** before it carries the change, and `boot-progress.sh` greps
`git diff -U0 HEAD` for its five marker patterns (the numbered boot-step lines, the renderer's
initialising line, the per-frame counter line, the frame budget and the loop-stop message) after
stripping `//` comments and `/*`/`*` lines. The first attempt's notes quoted the verdict back,
including the last frame marker it had reached - so the guard matched a line **in the notes**,
not in any code. **A notes file for a `boot-progress.sh` item must not quote a marker line:
name it instead** ("the last frame marker", "the renderer's initialising line"). Check before
finishing:

    tail -n +<this section's first line> "$NOTES" | sed -E 's#//.*##' \
      | grep -vE '^[[:space:]]*(/\*|\*)' | grep -E "$MARK"

That is the whole of the `verify boot-progress.sh failed` line in `build/goal/run.log`; the change
itself passed on 38068b1 and passes here.

The other failing check was `gate.sh docs claims`, four `stale:` lines about the gap table. Same
cause class: the first attempt's table edit was written from the 38068b1 numbers and the tip
moved under it. This time the numbers are derived on this tree, in this order:
`python3 tools/link_gap.py --rebuild` (285 MISSING, one stale entry, the target) ->
`--write-list` -> the `other game methods` row 214 -> 213. `python3 tools/check_docs_claims.py`:
`docs claims agree with the tree`.

### Measured on 9f2d849

- `./tools/decomp_build.sh -r main/MetroidPrime/Player/CGameState`: `All: 34.22% fuzzy, 27.29%
  matched, 12.75% linked (12087 / 28465 functions)`, the unit `89.08% fuzzy, 62.80% matched
  (105 / 116 functions)` - **identical to the baseline**, as it must be: the rename moves a symbol,
  not a byte.
- `objdump -s -j .text` of `CGameState.o` before and after, compared from line 3 (the first two
  lines are the file name): **identical, 100948 bytes of dump**. Only
  `powerpc-eabi-nm` changes: `fn_80145C98` -> `LoadFields__23CGameStateEnvVarManagerFv`.
- `./tools/gate.sh build/goal/judge/report.base.json`: `GATE PASS 9f2d849f+8 changed`, every step
  ok, `hashes vs config.yml ok` (all 86 RELs), `per-function diff matched 12087 -> 12087
  linked 5795 -> 5795 (+1 functions at 100%, 0 units newly linked)`,
  `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing`.
  (The 38068b1 run recorded 516 units - the eighth sync added nine.)
- `python3 tools/check_decl_order.py --unit CGameState`: `ok: 4 unit(s) checked, none emits its
  functions out of retail order`.
- `./tools/link_check.sh`: `compile errors 0`, `unique undefined symbols 290`,
  `duplicate definitions 0`. `NOT LINKED` is the baseline state (see
  `build/goal/judge/record-link.log`). `goal_check`: `port undefined 291 -> 290`, `probe: 747
  files, 0 failed, 0 errors; link: LINKED (290 undefined, 0 duplicates)`.
- `./tools/goal_verify/boot-progress.sh`: `BOOT_PROGRESS PASS: all 2 runs got further than all 2
  head runs`. Both runs beat both head runs by the stub rule: the target stub is gone from the set,
  and the two **newly reached** stubs are `fn_80145ACC` (the map insert) and
  `_ZN23SPersistentOptionsValueC1Eiii` (the value's constructor) - retail's behaviour appearing,
  because the body runs instead of printing.
- Marker guard replayed by hand on the diff before running the judge: no marker line.

### Corrections to the first attempt's notes

- **`_ZN15CGMSinglePlayerC1Ev` was NOT a stale entry on this tree.** The first attempt measured
  that on 38068b1 (`python3 tools/link_gap.py --rebuild` -> 317 MISSING, one stale entry) and the
  242/244 gap-table row counts came from there too. On 9f2d849 `--rebuild` reports **285 MISSING
  and one stale entry, the target itself**. `CGMSinglePlayer`'s constructor was closed by
  38068b13 ("port: real CGMSinglePlayer"), which is in this tip. So the gap-table edit here is one
  row, 214 -> 213, not two entries plus a count correction.
- The 2026-09-25 note in `check_files_cmake.py` claiming `CPersistentOptionsInit.cpp` is
  `Excluded until those three have bodies` is still true and this item does not change it: the file
  is still not listed, and `fn_80145ACC` still has no body. The `LoadFields` body that now runs is
  the one in `CGameState.cpp`, which is a `configure.py` unit, so the port link gets the eleven
  rows and stops at the two stubs below.

### Next

Unchanged from the first attempt and re-measured here: `fn_80145ACC` is now the most-hit stub on
the boot path (once per row) with `SPersistentOptionsValue`'s constructor beside it. Both are hit by
the new call path, both are real retail bodies, and both are the natural next items. `fn_80145ACC`
relocates against `fn_80146338` (the rbtree node insert, 440 bytes, 0.00% in `CGameState`'s own
report above), which nothing in the tree implements, so closing it needs that first.

NEW: port-boot-stub-fn-80145acc-9f2d849 | port | fn_80145ACC | LoadFields now runs and hits this stub once per row on this tip too; CPersistentOptionsMapInsert.cpp is written at 71.37% and is NonMatching because it relocates against fn_80146338, the rbtree node insert, which nothing implements (0.00% in main/MetroidPrime/Player/CGameState)

## Lane 1: passed, then failed on the moved tip (2026-10-01 19:53:01Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 847cb312486f; re-do it against the current tip.

## Lane 1, third attempt on 847cb312: PASS (2026-10-01)

`./tools/goal_check.sh build/goal/item.json` exits 0 on the current tip. The change is the same
five files the first two attempts made; nothing about the code had to be re-derived, because
`git diff 9f2d849..847cb312 -- src/MetroidPrime/Player/CGameState.cpp
include/MetroidPrime/Player/CGameStateEnvVarManager.hpp src/MetroidPrime/Player/CPersistentOptionsCtor.cpp
src/MetroidPrime/PortReachStubs.cpp config/G2ME01/symbols.txt` touches only `fn_80143CD4` /
`CFrontEndGameMode` and leaves the 0x80145C98 region untouched.

### Why the re-judge failed on 847cb31 - measured here, and it is only the notes

`build/goal/run.log`'s failing line is again `verify: the change edits a boot marker line - the
judge measures with those - BOOT_PROGRESS FAIL`. The second attempt diagnosed this correctly and
replayed the guard by hand **on its own new section only**; the marker line was in the *first*
attempt's section, which the replay skipped (`tail -n +<this section's first line>`). One line of
this file quoted the judge's frame-budget marker verbatim, and `stage_change` copies the whole
notes file to `docs/goal-notes/<id>.md` before `git add`, so it is in the diff the guard reads.
That line is now reworded to *name* the marker instead of quoting it, and the whole-file replay is
clean:

    sed -E 's#//.*##' "$NOTES" | grep -vE '^[[:space:]]*(/\*|\*)' | grep -E "$MARK"

with MARK the five patterns `tools/goal_verify/boot-progress.sh` line 35 builds, copied from that
script rather than written out here - a literal copy of them in this file would itself trip it.

Nothing else failed on the moved tip. The second attempt's `docs claims` failure was the gap table,
and the row it corrected has drifted again, so the numbers here are derived on this tree in this
order: `python3 tools/link_gap.py --rebuild` -> **286 MISSING and one stale entry, the target
itself** -> `--write-list` (285 entries) -> `docs/research/port_link_gap.md`'s `other game
methods` row 214 -> 213. `python3 tools/check_docs_claims.py`: `docs claims agree with the tree`.

### Measured on 847cb312

- `./tools/decomp_build.sh main/MetroidPrime/Player/CGameState`: `All: 34.22% fuzzy, 27.35% matched,
  12.84% linked (12097 / 28465 functions)`, the unit `89.08% fuzzy, 62.80% matched (105 / 116
  functions)` - **identical to the baseline**, as it must be: the rename moves a symbol, not a byte.
- Object compared against a **clean-tree** rebuild (stash the change, rebuild, `cp` the object,
  restore): `objdump -s -j .text` **identical, 100948 bytes of dump**. `cmp` of the two `.o` files
  differs **only in the symbol table**, and `powerpc-eabi-nm`'s whole-output diff is two lines:
  `U LoadFields__23CGameStateEnvVarManagerFv` -> `T LoadFields__23CGameStateEnvVarManagerFv` and
  `T fn_80145C98` gone. That `U` is the bug in one line: the clean object *referenced* the
  mangled member name and *defined* the C one, which is why nothing in the port resolved it.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/gate.sh build/goal/judge/report.base.json`: `GATE PASS 847cb312+7 changed`, every step
  ok, `hashes vs config.yml ok`, `per-function diff matched 12097 -> 12097 linked 5849 -> 5849
  (+1 functions at 100%, 0 units newly linked)`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing from
  their object`.
- `python3 tools/check_decl_order.py --unit CGameState`: `ok: 4 unit(s) checked, none emits its
  functions out of retail order`.
- `./tools/link_gap.py --rebuild`: `286 MISSING` before, `285 entries in 4 groups` after.
- `./tools/goal_verify/boot-progress.sh`: `BOOT_PROGRESS PASS: all 2 runs got further than all 2
  head runs`. `build-boot-probe/run.log` shows the target stub **gone** from the set and the two
  **newly reached** ones are `fn_80145ACC` (the map insert) and
  `_ZN23SPersistentOptionsValueC1Eiii` (the value's constructor), each hit once per row - retail's
  behaviour appearing, because the body runs instead of printing.
- `goal_check`: `port undefined 291 -> 290`, `probe: 747 files, 0 failed, 0 errors; link: LINKED
  (290 undefined, 0 duplicates)`, `4 path(s) changed under src/ or include/`, exit 0.

### Unchanged from the earlier attempts and re-measured here

`fn_80145ACC` is still the most-hit stub on the boot path (once per row, eleven rows) with
`SPersistentOptionsValue`'s constructor beside it. `fn_80145ACC` relocates against `fn_80146338`
(the rbtree node insert, retail 0x80146338, 440 bytes), which the `CGameState` report above still
scores at 0.00% and which nothing in the tree implements, so closing it needs that first.
`tools/check_files_cmake.py`'s 2026-09-25 note (`CPersistentOptionsInit.cpp` excluded "until those
three have bodies") is still true and this item does not change it: that file is still unlisted,
`fn_80145ACC` still has no body, and the `LoadFields` body that now runs is the one in
`CGameState.cpp`, a `configure.py` unit.

NEW: port-boot-stub-fn-80145acc-847cb312 | port | fn_80145ACC | LoadFields now runs and hits this stub once per row on this tip too; CPersistentOptionsMapInsert.cpp is written at 71.37% and is NonMatching because it relocates against fn_80146338, the rbtree node insert, which nothing implements (0.00% in main/MetroidPrime/Player/CGameState)

## Lane 1: passed, then failed on the moved tip (2026-10-01 20:07:59Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 2c9e52ca3dd8; re-do it against the current tip.

## Lane 1, fourth attempt on 2c9e52ca: PASS (2026-10-01)

`./tools/goal_check.sh build/goal/item.json` exits 0 on the current tip. The change is the same
seven files the third attempt carried; it was re-applied from `build/goal/rebase.patch`, which
`git apply --check` accepts unchanged on this tip, so nothing about the code had to be re-derived.
`git diff 847cb312..2c9e52ca -- <those seven files>` touches only `CMappableObject`,
`rstl::operator==` and `DolphinCGraphics`; the 0x80145C98 region is untouched.

### The re-judge failure on 2c9e52c was the baseline, not the change - measured here

The third attempt's `goal_check` **passed on 847cb312** (`build/goal/check.out`, still on disk:
`ok verify boot-progress.sh: BOOT_PROGRESS PASS`, then `goal_check: PASS`). The re-judge at 2c9e52c
failed with a line the earlier attempts never saw:

    verify: the boot baseline is for 847cb31, the branch head is 2c9e52c - BOOT_PROGRESS FAIL

`tools/goal_verify/boot-progress.sh` reads `build/goal/judge/boot.base.json` and compares it to
the branch head, and `run_goal.sh` only re-records that baseline between *items*. When the driver
carried a judged change onto a tip that moved **after** the item started, the baseline was still
the old commit's and the script refused before booting. So the third attempt's `run.log` failure
has nothing to do with the diff, the notes or the markers - all three were clean. This run, the
driver re-recorded the baseline at 2c9e52ca before starting (`build/goal/run.log`:
`recording the boot baseline at 2c9e52c (boot-progress.sh --record)`, and
`build/goal/judge/boot.base.json` now carries `"head": "2c9e52ca3dd86428b2a27021bf8d371623f3d627"`,
matching `git rev-parse HEAD`), so the same change passes here.

**The lesson for the next attempt on a `boot-progress.sh` item: if the re-judge fails with the
baseline/head mismatch line, the change is not implicated.** Check that line first and, if it is
the failure, do not re-derive anything - re-apply and re-judge, because the baseline is only
re-recorded when a new item starts.

### Measured on 2c9e52ca (all re-derived on this tree, none recalled)

- `./tools/decomp_build.sh main/MetroidPrime/Player/CGameState`: `All: 34.22% fuzzy, 27.39% matched,
  12.89% linked (12104 / 28465 functions)`, the unit `89.08% fuzzy, 62.80% matched (105 / 116
  functions)` - identical to the judge's own baseline for this tip, as it must be: the rename moves
  a symbol, not a byte.
- Object compared against a **clean-tree** rebuild (stash, rebuild, `cp`, restore):
  `objdump -s -j .text` **identical, 100948 bytes of dump** on both sides. `nm`'s whole-output diff
  is exactly three lines: `U LoadFields__23CGameStateEnvVarManagerFv` becomes `T` at the same
  address `00005568`, and `T fn_80145C98` is gone. That `U` was the whole bug in one line - the
  clean object *referenced* the mangled member name and *defined* the C one, so nothing in the
  port link could resolve it.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/gate.sh build/goal/judge/report.base.json` (via `goal_check`): `GATE PASS 2c9e52ca+7
  changed`, `ninja + build.sha1 ok`, `hashes vs config.yml ok` (all 86 RELs), `docs claims ok`,
  `per-function diff matched 12104 -> 12104 linked 5860 -> 5860 (+1 functions at 100%, 0 units
  newly linked)`.
- `python3 tools/check_symbol_names.py`: `checked 525 units; 0 declared names are missing from
  their object` - same 525 as on 847cb312.
- `python3 tools/check_decl_order.py --unit CGameState`: `ok: 4 unit(s) checked, none emits its
  functions out of retail order`.
- `python3 tools/link_gap.py --rebuild`: **285 MISSING**, `all accounted for in
  port_link_gap_list.md` with the applied edit. The `other game methods` row and heading are
  **213 -> 213** on this tip, i.e. the number carried from 847cb312 is correct here; the list has
  213 bullet rows and the target symbol no longer appears in it. (`grep -c` on the list: 213, 0.)
- `./tools/goal_verify/boot-progress.sh` (via `goal_check`): `BOOT_PROGRESS PASS: all 2 runs got
  further than all 2 head runs`. `build-boot-probe/run.log` has **zero** occurrences of the target
  stub, and the newly reached stubs are `fn_80145ACC` and `_ZN23SPersistentOptionsValueC1Eiii`,
  **eleven times each per run** - the eleven default persistent-option rows, retail's behaviour
  appearing because the body runs instead of printing. Exit code 0, every marker kept.
- Marker guard replayed by hand on both things `stage_change` stages, before running the judge:
  the diff (`git diff -U0 HEAD -- . ':(exclude)build*'`, minus the `+++`/`---` headers, `cut -c2-`,
  `//`-stripped, block-comment lines dropped) and **the whole notes file**, since `stage_change`
  copies it to `docs/goal-notes/<id>.md`. Both clean. This file is now marker-clean end to end,
  which is what the second and third attempts established.
- `goal_check`: `port undefined 291 -> 290`, `probe: 747 files, 0 failed, 0 errors; link: LINKED
  (290 undefined, 0 duplicates)`, `4 path(s) changed under src/ or include/`, exit 0.

### Unchanged from the earlier attempts and re-measured here

`fn_80145ACC` is still the most-hit stub on the boot path - eleven per run, the same count as the
eleven rows `LoadFields` inserts - with `SPersistentOptionsValue`'s constructor beside it. It
relocates against `fn_80146338` (the rbtree node insert, retail 0x80146338, 440 bytes), which this
tip's `CGameState` report still scores at 0.00% and which nothing in the tree implements, so closing
it needs that first. `tools/check_files_cmake.py`'s 2026-09-25 note (`CPersistentOptionsInit.cpp`
excluded "until those three have bodies") is still true and unchanged by this item: that file is
still unlisted, `fn_80145ACC` still has no body, and the `LoadFields` body that now runs is the one
in `CGameState.cpp`, a `configure.py` unit.

NEW: port-boot-stub-fn-80145acc-2c9e52ca | port | fn_80145ACC | LoadFields now runs and hits this stub eleven times per row on this tip too; CPersistentOptionsMapInsert.cpp is written at 71.37% and is NonMatching because it relocates against fn_80146338, the rbtree node insert, which nothing implements (0.00% in main/MetroidPrime/Player/CGameState)
