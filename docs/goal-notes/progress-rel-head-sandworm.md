# progress-rel-head-sandworm - Sandworm's (module 56) head, landed

Lane 2, worktree `../wt-mp2-goal-L2`, 2026-09-30. **Done, not blocked.** One new `Matching` unit
with 4 functions, all 100.00%; every gate clean; nothing committed (the driver commits).

## What landed

- `src/MetroidPrime/ScriptObjects/CSandwormRel.cpp` (new) - the module head, `.text 0x0..0xDC`,
  4 functions, all 100.00%: `fn_56_0` (0x0, 0x2C), `RELExit` (0x2C, 0x24), `RELMain` (0x50, 0x20),
  `fn_56_70` (0x70, 0x6C). Written from `CSnakeWeedSwarmRel.cpp`'s spelling.
- `config/G2ME01/rels/Sandworm/splits.txt` - the `CSandwormRel.cpp` entry for `.text
  0x0..0xDC`, ahead of the existing `REL/global_destructor_chain.c` entry.
- `configure.py` - `Rel("Sandworm", [Object(Matching, "MetroidPrime/ScriptObjects/CSandwormRel.cpp")])`
  appended at the end of the `Rel` list, with the comment.
- `config/G2ME01/symbols.txt` - **one token**: the 8-byte DOL setter at 0x8021887C is renamed
  `fn_8021887C` -> `fn_8021887C__FP18SSandworm_FuncPtrs`. No DOL byte moves (see below).

`docs/HANDOFF.md` shows modified in `git status` after `goal_check.sh`; **that is the judge's own
rewrite of the derived state block and module list**, not my edit. `files.cmake` untouched.

## Measured

```
$ ./tools/decomp_build.sh
  All: 31.19% fuzzy, 23.48% matched, 11.80% linked (732 / 2051 files)
  All: 31.19% fuzzy, 23.48% matched, 11.80% linked (10171 / 28465 functions)
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp
   .text      claimed    220   ours    220   retail    220   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/audit_rel_claim.py Sandworm
ok   MetroidPrime/ScriptObjects/CSandwormRel.cpp          0x00000000..0x000000DC  4/4 functions
ok   REL/global_destructor_chain.c                        0x00015890..0x00015904  2/2 functions
ok   REL/REL_Setup.cpp                                    0x00015904..0x00015AA8  5/5 functions
0 claim(s) with a problem
Sandworm: preplf 383 text symbols, plf 383, 0 dropped by -strip_partial
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSandwormRel.cpp
ok: 0 unit(s) checked, none emits its functions out of retail order
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_raw_offsets.py
ok: 152 raw-offset site(s) in 61 file(s), all documented in raw_offsets.md
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-rel-head-sandworm
  ok counts: matched 10167 -> 10171   linked 4955 -> 4959
  ok target rose: module:Sandworm: 7 -> 11 / 383 functions
  ok no asm added
```

All 86 REL sha1s against `config/G2ME01/config.yml`: **0 DIFF**. `cmp` of all 86 against
`orig/G2ME01/files/RelProd/`: **0 diffs**.

**Baseline re-measured on this branch, not recalled** (`git stash push -u`, rebuild, snapshot,
`git stash pop`, rebuild): `All: 31.18% fuzzy, 23.47% matched, 11.80% linked (731 / 2050 files)`,
`matched_functions` 10167, `total_functions` **28465 before and after**, `complete_units` 731 -> 732.
The `report.json` already on disk at the start of the session would have hidden a no-op.

**No function anywhere got worse.** A per-function diff of the two `build/report.json` snapshots
over all 28465 functions: **0 worse, 0 better outside Sandworm**, 376 gone / 376 new. Every one of
the 376 is a rename with an unchanged match value: 375 are the
`Sandworm/auto_00_00000000_text` -> `auto_00_000000DC_text` (371) + `CSandwormRel` (4) split, and
one is the DOL's `main/auto_03_8021887C_text` `fn_8021887C` -> `fn_8021887C__FP18SSandworm_FuncPtrs`
(unmatched before and after: `match` `None`/8 bytes on both sides, unit 0/1 on both sides).

## The three things that were not copies

1. **The head is SnakeWeedSwarm's (module 71) byte for byte, and that was measured before writing
   anything.** Diffing `build/G2ME01/Sandworm/asm/auto_00_00000000_text.s` over 0x0..0xDC against
   `build/G2ME01/SnakeWeedSwarm/asm/auto_00_00000000_text.s` over the same range: **55 instructions
   on each side, identical in every opcode and operand once symbol names are masked, and 11 of the
   55 lines differing only in the symbol they name** - six renames (`fn_71_DC`->`fn_56_DC`,
   `fn_71_70`->`fn_56_70`, `lbl_71_data_18/24`->`lbl_56_data_6E0/6EC`, `lbl_71_bss_40`->`lbl_56_bss_38`,
   `SetLoader_SnakeWeedSwarm`->`fn_8021887C`). **Unlike every other head in this family there is no
   accessor block above them at all** - these are the module's first four functions - so the unit is
   0xDC over 4 functions, not 0x170 over 18. Nothing had to be re-read.
2. **The record's two member-function pointers have readers in the DOL, so their shapes are read,
   not guessed.** `lbl_56_bss_38` is `.bss:0x38`, `size:0x1C` (`auto_05_00000000_bss.s`) - the sixth
   of the module's seven `.bss` objects, and the one `fn_56_70` stores through, so the offset had to
   be read from the dump. The 0x1C is one `FScriptLoader` plus two 12-byte CodeWarrior
   pointer-to-member-functions, the same seven words SnakeWeedSwarm fills. The only reader of each
   is a three-line thunk: `fn_80218818` (0x80218818) `lwz r6, gLoader_Sandworm; addi r12, r6, 0x4;
   bl __ptmf_scall4`, and `fn_802187EC` (0x802187EC) `lwz r4, gLoader_Sandworm; addi r12, r4, 0x10;
   bl __ptmf_scall`. **Both are called only from `fn_8012F3C0` (0x8012F3C0)**, a setup loop that
   passes a stack local / r23 / a loop counter to the +0x4 one and uses the +0x10 one's result as
   its bound (`bl fn_802187EC; cmpw r24, r3; blt`), next to a
   `TCastToPtr<12CSandwormEye>__FP7CEntity`. The *class* those two hang off is not determined by
   the bytes - only the 12-byte size is - and the file says so.
3. **This setter needs a `symbols.txt` rename, unlike Splitter's and Rezbit's.** dtk's first error
   named the import exactly: `Failed to find symbol fn_8021887C__FP18SSandworm_FuncPtrs in any
   module`. Renaming the DOL symbol fixed that, and then a second build failed with
   `..._FP18SSandworm_FuncPtrs__FP18SSandworm_FuncPtrs` - **the declaration has to sit inside the
   file's `extern "C"` block**, or mwcceppc mangles the already-mangled name a second time. The
   function is 8 bytes, `stw r3, gLoader_Sandworm@sda21(r0); blr`, immediately after
   `LoadSandworm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218850 which is 0x2C
   bytes and so ends exactly there; it stays unclaimed, so the DOL sha1 is unchanged. This is the
   same one-line rename `SetLoader_SnakeWeedSwarm` carries at 0x8021BB08.

Also confirmed rather than assumed:

- **`fn_56_0` is vtable entry 0x3C of two different classes' tables** - `lbl_56_data_6F8` (0x98
  bytes, CSandwormEye) and `lbl_56_data_884` (0x240 bytes, CSandworm) - and in both, offset 0x38
  is a `HealthInfo` slot (`HealthInfo__6CActorFv` and `HealthInfo__3CAiFv`). So the call is a
  member call on vtable slot 0x38, written as `CSnakeWeedSwarmRel.cpp` writes it.
- **No dead-strip hazard** - `build/G2ME01/Sandworm/ldscript.lcf` FORCEACTIVE holds `fn_56_0` (and
  `fn_56_DC`), RELMain/RELExit are the module's entry points, and `fn_56_70` is called from
  RELMain, so all four survive and no `force_active:` entry is needed in `config/G2ME01/config.yml`.
- **The built object carries `.text` 0xDC and nothing else** (`powerpc-eabi-objdump -h` on
  `CSandwormRel.o`) - the 14-virtual stand-in class emits no vtable, which is why the split claims
  `.text` only - and `powerpc-eabi-nm -n` shows exactly four defined symbols, `fn_56_0` 0x0,
  `RELExit` 0x2c, `RELMain` 0x50, `fn_56_70` 0x70, i.e. retail order.
- **383 text symbols** on the `audit_rel_claim.py` convention: 4 ours + 5 `REL_Setup` + 2
  `global_destructor_chain` + 372 unclaimed (`auto_00_000000DC_text` 371 plus
  `auto_fn_56_14FA8_text` 1).
- Not added to `files.cmake`, for the reason the other heads measure: it calls `fn_56_DC` and
  `fn_8021887C__FP18SSandworm_FuncPtrs`, which the port cannot link.

## `flip_test.sh` cannot be run for this unit - a pre-existing defect in the tool, not in the unit

`./tools/flip_test.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp` prints
`no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CSandwormRel.cpp) ... FAIL`.
`unit_info()` in that script picks the source root by counting `(` against `)` between the last
`MusyX(` and the `Object(...)` entry, and it counts **inside comments**. Any later comment with an
unbalanced `(` makes every entry after it look like it lives under `extern/musyx/src/`.
**This is not caused by this change and it is not specific to this unit** - the same command on a
committed, landed unit fails identically on this branch:

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp) - ...
  FAIL  -> reverted
```

I cannot fix `tools/`, and `goal_check.sh` does not run `flip_test.sh` for a `progress` item (only
for `match`), so the item passes. What stands in for it, and is the one rule for a REL unit anyway,
is that **the module's `.rel` still hashes to `config/G2ME01/config.yml` and is `cmp`-equal to
`orig/G2ME01/files/RelProd/Sandworm.rel` with our own object in the link** - the build log shows
`[1/9] MWCC build/G2ME01/src/MetroidPrime/ScriptObjects/CSandwormRel.o`, and the split claims that
range for a `Matching` object, so dtk links ours rather than retail's bytes. `goal_check.sh` runs
`gate.sh`, which checks all 86 hashes, and reported `ok`.

## NEW: lines

None. Every piece of this item landed. The three measurements that took work here - the head diff
before writing, the `.bss` slot read, and the `extern "C"` double-mangling of a renamed setter -
are lessons, and they are written into `src/MetroidPrime/ScriptObjects/CSandwormRel.cpp`'s header
and into the `configure.py` comment, not filed as work.

---

# progress-rel-head-sandworm - second run, 2026-09-30 (lane 2, worktree `../wt-mp2-goal-L2`)

**The change above did not reach the tree.** This run started on a clean `wt-mp2-goal-L2` at
`d2ee240`: `git status --porcelain` empty, `src/MetroidPrime/ScriptObjects/CSandwormRel.cpp` absent,
`configure.py` with no `Rel("Sandworm", ...)`, `config/G2ME01/rels/Sandworm/splits.txt` with only
`REL/global_destructor_chain.c` and `REL/REL_Setup.cpp`, and `fn_8021887C` (not the suffixed name) at
line 9461 of `config/G2ME01/symbols.txt`. So this is **not** `STALE:` - the work had to be redone,
and the section above is treated as a recipe and was re-measured, not trusted.

## Re-measured before writing (not recalled from the section above)

Baseline on this tree: `build/goal/judge/report.base.json` = `matched_functions` 10218,
`total_functions` 28465, `complete_units` 734; `Sandworm/` sums **7 / 383** matched (2
`global_destructor_chain` + 5 `REL_Setup`, nothing of ours), and `Sandworm` has **no** `Rel(...)`
block in `configure.py`.

Head bytes: `build/G2ME01/Sandworm/asm/auto_00_00000000_text.s` opens with **`fn_56_0` (0x0, 0x2C),
`RELExit` (0x2C, 0x24), `RELMain` (0x50, 0x20), `fn_56_70` (0x70, 0x6C)** and then `fn_56_DC`
(0xDC, 0xF6C) - **no accessor block above them at all**, so the claim is 0xDC over 4 functions, not
0x170 over 18. Confirmed by reading both dumps: the 0x0..0xDC range of Sandworm's dump and
SnakeWeedSwarm's is the same 55 instructions with six symbol renames
(`fn_71_DC`->`fn_56_DC`, `fn_71_70`->`fn_56_70`, `lbl_71_data_18`->`lbl_56_data_6E0`,
`lbl_71_data_24`->`lbl_56_data_6EC`, `lbl_71_bss_40`->`lbl_56_bss_38`,
`SetLoader_SnakeWeedSwarm`->`fn_8021887C`).

Re-read, not copied: `auto_05_00000000_bss.s` has seven objects and `lbl_56_bss_38` is the sixth,
`size:0x1C`; `auto_04_00000000_data.s` has `lbl_56_data_6E0` = 0 / 0xFFFFFFFF / `fn_56_5938` and
`lbl_56_data_6EC` = 0 / 0xFFFFFFFF / `fn_56_5910`, both 0xC bytes; `fn_56_0` is at vtable offset
0x3C of `lbl_56_data_6F8` (0x98 bytes, CSandwormEye) **and** `lbl_56_data_884` (0x240 bytes,
CSandworm), each with a `HealthInfo` entry at 0x38 (`HealthInfo__6CActorFv`,
`HealthInfo__3CAiFv`). The setter at 0x8021887C is 8 bytes, `stw r3, gLoader_Sandworm@sda21(r0);
blr`, right after `LoadSandworm__...` at 0x80218850 which is 0x2C bytes. Its two readers
`fn_802187EC` (+0x10) and `fn_80218818` (+0x4) are three-line thunks in the unclaimed
`auto_03_802184E4_text.s`, called only from `fn_8012F3C0` (0x8012FA70 / 0x8012FA98, the latter's
result used as a `cmpw` bound), next to `TCastToPtr<12CSandwormEye>__FP7CEntity`. `ldscript.lcf`
FORCEACTIVE holds `fn_56_0`.

## Landed

- `src/MetroidPrime/ScriptObjects/CSandwormRel.cpp` (new, 4 functions, all **100.00%**), written
  from `CSnakeWeedSwarmRel.cpp`'s spelling.
- `config/G2ME01/rels/Sandworm/splits.txt:9-10` - the `CSandwormRel.cpp` entry for `.text
  0x00000000..0x000000DC`, ahead of `REL/global_destructor_chain.c`.
- `configure.py:2554-2603` - `Rel("Sandworm", [Object(Matching,
  "MetroidPrime/ScriptObjects/CSandwormRel.cpp")])` appended at the end of the `Rel` list, with the
  comment.
- `config/G2ME01/symbols.txt:9461` - **one token**: `fn_8021887C` -> `fn_8021887C__FP18SSandworm_FuncPtrs`
  at 0x8021887C. The unit is unclaimed, so no DOL byte moves and the DOL sha1 is unchanged.
- `files.cmake` untouched. `docs/HANDOFF.md` shows modified after `goal_check.sh` - that is
  `gate.sh`'s own `MP_GATE_DOCS_WRITE=1` rewrite of the derived state block and module list, not my
  edit (the diff is exactly 10218->10222, 5004->5008, 1498->1502 and the module list gaining
  `Sandworm`).

## Measured

```
$ ./tools/decomp_build.sh
All:  31.21% fuzzy, 23.50% matched, 11.81% linked (10222 / 28465 functions)
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp
   .text      claimed    220   ours    220   retail    220   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/audit_rel_claim.py Sandworm
ok   MetroidPrime/ScriptObjects/CSandwormRel.cpp          0x00000000..0x000000DC  4/4 functions
ok   REL/global_destructor_chain.c                        0x00015890..0x00015904  2/2 functions
ok   REL/REL_Setup.cpp                                    0x00015904..0x00015AA8  5/5 functions
0 claim(s) with a problem
Sandworm: preplf 383 text symbols, plf 383, 0 dropped by -strip_partial
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSandwormRel.cpp
ok: 0 unit(s) checked, none emits its functions out of retail order
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ python3 tools/check_raw_offsets.py
ok: 155 raw-offset site(s) in 64 file(s), all documented in raw_offsets.md
$ python3 tools/check_module_wiring.py
93 unit(s) of our own code in 73 module(s): ... (Sandworm in the list)
$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json
... report  ok ; per-function diff  SPLIT  Sandworm/auto_00_00000000_text: 375 function(s)
    accounted for across 2 new unit(s) in Sandworm (exact count match - a split, not a loss)
GATE PASS  d2ee240+5 changed
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-rel-head-sandworm
  ok counts: matched 10218 -> 10222   linked 5004 -> 5008
  ok target rose: module:Sandworm: 7 -> 11 / 383 functions
  ok no asm added
```

`gate.sh`'s `hashes vs config.yml` line covers all 86 RELs and is `ok`; `check_module_wiring.py` is
`ok`; `docs claims` is `ok`. **`Sandworm/MetroidPrime/ScriptObjects/CSandwormRel` reports 4/4 at
100.00% fuzzy** in `build/report.json`, and `Sandworm/` sums **11 / 383** (base 7).

`total_functions` **28465 before and after**; `complete_units` 734 -> 735. Per-function diff of the
two `build/report.json` snapshots over all 28465 functions: **0 worse, 0 better outside the
Sandworm split**, 376 gone / 376 new - 371 the `auto_00_00000000_text` -> `auto_00_000000DC_text`
split, 4 the `CSandwormRel` unit, 1 the DOL rename `fn_8021887C` ->
`fn_8021887C__FP18SSandworm_FuncPtrs` (unmatched before and after).

Built object: `powerpc-eabi-nm -n` gives exactly four defined symbols in retail order - `fn_56_0`
0x0, `RELExit` 0x2c, `RELMain` 0x50, `fn_56_70` 0x70 - and `powerpc-eabi-objdump -h` gives `.text`
0xDC and nothing but `.comment` (the 14-virtual stand-in class emits no vtable, which is why the
split claims `.text` only). Not added to `files.cmake`: it calls `fn_56_DC` and
`fn_8021887C__FP18SSandworm_FuncPtrs`, which the port cannot link.

## The one thing this run added to the notes above: the two `symbols.txt` errors, in order

The section above says the setter "needs a `symbols.txt` rename" and warns about double mangling.
This run hit **both, in that order, and they are distinguishable**:

1. declaration left **outside** the `extern "C"` block, after the `symbols.txt` rename was already
   in place: dtk fails with
   `Failed to find symbol fn_8021887C__FP18SSandworm_FuncPtrs__FP18SSandworm_FuncPtrs in any module`.
   The suffix is appended a second time. Fix: move the declaration into `extern "C"`.
2. (already handled above) with the declaration inside `extern "C"` but the DOL symbol still the
   bare `fn_8021887C`, dtk fails with
   `Failed to find symbol fn_8021887C__FP18SSandworm_FuncPtrs in any module`.

So the order is: rename the DOL symbol **and** put the declaration inside `extern "C"`, and neither
alone is enough. Both errors are `dtk`'s, at `[7/11] REL`, and both name the mangled token, so the
message tells you which of the two you have.

## `flip_test.sh` still cannot be run for this unit - a pre-existing defect in the tool

Re-measured here, and the control still holds: the same command on a committed, landed unit fails
identically on this branch.

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CSandwormRel.cpp) - ...
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp
kept: 0 / 1   failed: 1   skipped: 0
   FAIL MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp
```

`unit_info()` in that script picks the source root by counting `(` against `)` between the last
`MusyX(` and the `Object(...)` entry, and it counts **inside comments** - any later comment with an
unbalanced `(` makes every entry after it look like it lives under `extern/musyx/src/`. I cannot fix
`tools/`, and `goal_check.sh` does not run `flip_test.sh` for a `progress` item. What stands in for
it, and is the one rule for a REL unit anyway, is that **`Sandworm.rel` still hashes to
`config/G2ME01/config.yml` with our object in the link** - the build log shows
`[1/11] MWCC build/G2ME01/src/MetroidPrime/ScriptObjects/CSandwormRel.o` and
`[2/11] LINK build/G2ME01/Sandworm/Sandworm.plf`, the split claims 0x0..0xDC for a `Matching`
object, and `gate.sh`'s `hashes vs config.yml` (all 86) is `ok`.

## NEW: lines

None. The item is complete, not blocked. Everything new this run learned - the two `symbols.txt`
failures and which order they appear in - is a codegen lesson and is in the source file's header
and in the `configure.py` comment, not filed as work.
---

# progress-rel-head-sandworm - third run, 2026-09-30 (lane 2, worktree `../wt-mp2-goal-L2`)

**Landed, and the reason the two previous attempts were lost is now measured rather than guessed.**
`goal_check: PASS`, review-clean tree, nothing committed. The change is the same four functions the
two runs above produced; what is new is *why it vanished twice*, and the one placement choice that
fixes it.

## The previous two runs did not fail - the driver's carry onto a moved tip did

Neither run was rejected and neither failed the judge. `build/goal/run.log` shows both attempts:

```
goal_check: PASS progress-rel-head-sandworm
review PASS (worker) - transcript ...progress-rel-head-sandworm-L2-10-review1-...
goal/decomp moved (d2ee240 -> 0674096) during progress-rel-head-sandworm - carrying the judged change onto it
Applied patch to 'config/G2ME01/rels/Sandworm/splits.txt' cleanly.
Applied patch to 'config/G2ME01/symbols.txt' cleanly.
Applied patch to 'configure.py' with conflicts.
Falling back to direct application...
Falling back to direct application...
U configure.py
progress-rel-head-sandworm does not apply on 0674096 - releasing it for a fresh attempt
```

`tools/run_goal.sh`'s `rebase_onto_tip` (line 634) carries a judged change onto a moved tip with
`git apply --index --3way --binary`, and `tools/union_docs_conflicts.sh` union-merges **only**
`docs/*.md`. **A `configure.py` conflict releases the item for a fresh attempt, discarding a change
that already passed.** Both runs appended their `Rel("Sandworm", ...)` block immediately before the
list's closing `]`, so the block's patch context was the same lines `progress-rel-head-splinter`
(commit `0674096`) appended at - hence two conflicts on two different tips, in the identical place.

### The fix is the placement, and it is measured, not reasoned

`Rel(...)` order carries no requirement - each block is an independent module definition. This run
puts `Rel("Sandworm", ...)` **next to `Rel("SandBoss", ...)`, a committed 2026-09-29 block no lane
touches**, instead of at the end of the list. Both cases were reproduced in a throwaway clone under
`.tmp/opencode/`, replaying the exact `git apply --index --3way --binary` the driver runs, against
a simulated competing lane that appends its own `Rel(...)` at the end:

| placement of this block | competing lane appends at the end | result |
| --- | --- | --- |
| end of the `Rel` list (both lost runs) | yes | `Applied patch to 'configure.py' with conflicts` / `U configure.py` / rc=1 - **the logged failure, reproduced** |
| next to `Rel("SandBoss", ...)` (this run) | yes | `Applied patch to 'configure.py' cleanly` / rc=0 |

The control row is the same failure line-for-line as `run.log`, so the cause is identified rather
than assumed. `NEW:`-worthy? No - it is a codegen/packaging lesson, so it lives in the
`configure.py` comment on the block and here, not in the queue.

## Re-measured on this tree (not recalled from the two sections above)

Baseline `build/goal/judge/report.base.json`: `matched_functions` **10236**, `total_functions`
**28465**, `complete_units` **735**, `total_units` **2054**. `Sandworm/` summed **7 / 383** matched
(2 `global_destructor_chain` + 5 `REL_Setup`, nothing of ours) and had **no** `Rel(...)` block in
`configure.py`. HEAD is `0674096`, `git status --porcelain` empty, `CSandwormRel.cpp` absent,
`fn_8021887C` (unsuffixed) at `config/G2ME01/symbols.txt:9461` - so again **not `STALE:`**.

- Head bytes: `build/G2ME01/Sandworm/asm/auto_00_00000000_text.s` opens with `fn_56_0` (0x0, 0x2C),
  `RELExit` (0x2C, 0x24), `RELMain` (0x50, 0x20), `fn_56_70` (0x70, 0x6C), then `fn_56_DC`
  (0xDC, 0xF6C). **No accessor block above them**, so the claim is 0xDC over 4 functions.
- Head diff against SnakeWeedSwarm over 0x0..0xDC with every identifier masked: **55 instructions
  each, 53 identical in opcode and operands**, and the 2 that differ are the `bl` at 0x3C and the
  `bl` at 0xC8 - the module's own setter name and nothing else. (The two sections above say
  "11 of 55 lines differing only in the symbol they name"; that is the *unmasked* count, and both
  are the same measurement.)
- `auto_05_00000000_bss.s`: seven objects; `lbl_56_bss_38` is the **sixth**, `size:0x1C`.
- `auto_04_00000000_data.s`, parsed rather than eyeballed: `lbl_56_data_6E0` = 0 / 0xFFFFFFFF /
  `fn_56_5938` and `lbl_56_data_6EC` = 0 / 0xFFFFFFFF / `fn_56_5910`, both 0xC;
  `lbl_56_data_6F8` = 38 words (0x98 bytes, 36 virtuals) and `lbl_56_data_884` = 144 words
  (0x240 bytes, 142 virtuals), each with two leading zero words, and in **both** +0x38 is a
  `HealthInfo` slot (`HealthInfo__6CActorFv`, `HealthInfo__3CAiFv`) with `fn_56_0` at +0x3C.
- Setter 0x8021887C, 8 bytes, `stw r3, gLoader_Sandworm@sda21(r0); blr`, right after
  `LoadSandworm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218850 (0x2C bytes).
  Readers `fn_802187EC` (0x802187EC, +0x10) and `fn_80218818` (0x80218818, +0x4), called only from
  `fn_8012F3C0` at 0x8012FA70/0x8012FA98 next to `TCastToPtr<12CSandwormEye>__FP7CEntity`.
- `ldscript.lcf` FORCEACTIVE holds `fn_56_0` and `fn_56_DC`; nothing needs a `force_active:` entry.

### One measurement the two sections above do not have: which argument `__ptmf_scall4` adjusts

The two members are 12 bytes each and their reader thunks call **different** runtime helpers.
`build/G2ME01/asm/Runtime/ptmf.s`: `__ptmf_scall` (0x80345454) is
`lwz r0/r11/r12` then **`add r3, r3, r0`**, and `__ptmf_scall4` (0x8034547C) is the same three loads
then **`add r4, r4, r0`**. The "4" is the register number: the +0x10 member's `this` is in r3 and the
+0x4 member's `this` is in **r4**, i.e. it is passed as the second value, not the first. Neither
section above says this, and it is why the two members are typed identically in the source - the
argument lists below the struct are not both measurable, and the file says that rather than
inventing a signature. Codegen is unaffected: only the 12-byte sizes and the two offsets are
load-bearing in `fn_56_70`.

## Landed

- `src/MetroidPrime/ScriptObjects/CSandwormRel.cpp` (new, 4 functions, all **100.00%**), written
  from `CSnakeWeedSwarmRel.cpp`'s spelling.
- `config/G2ME01/rels/Sandworm/splits.txt:9-10` - the `CSandwormRel.cpp` entry for `.text
  0x00000000..0x000000DC`, ahead of `REL/global_destructor_chain.c`.
- `configure.py:1685-1745` - `Rel("Sandworm", [Object(Matching,
  "MetroidPrime/ScriptObjects/CSandwormRel.cpp")])`, placed **after** the `SandBoss` block, with the
  placement rationale and the measurements in the comment.
- `config/G2ME01/symbols.txt:9461` - **one token**, `fn_8021887C` ->
  `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` at 0x8021887C. The unit holding 0x8021887C is
  unclaimed, so no DOL byte moves.
- `files.cmake` untouched: the file calls `fn_56_DC` and `SetLoader_Sandworm`, which the port cannot
  link, for the reason the other heads measure. `docs/HANDOFF.md` shows modified in `git status`
  after `gate.sh`; that is `MP_GATE_DOCS_WRITE=1` rewriting the derived state block and module list
  (the diff is exactly 10236->10240, 5018->5022, 1512->1516 and `Sandworm` joining the list), not
  my edit.

### One spelling changed from the two sections above

They renamed the DOL token to `fn_8021887C__FP18SSandworm_FuncPtrs` and had to put the declaration
inside the `extern "C"` block to stop mwcceppc mangling the already-mangled name. This run instead
gives the DOL the **family's own name**, `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` - the 14
siblings at `config/G2ME01/symbols.txt:9529-9618` all carry `SetLoader_<Module>__FP<n><Struct>
_FuncPtrs` - and declares the function as ordinary C++ outside `extern "C"`, which is what
`CFishCloudRel.cpp` and `CSnakeWeedSwarmRel.cpp` do. Measured: the object compiles and the REL
links, so the mangled name matches with no `extern "C"` workaround at all. (`SSandworm_FuncPtrs` is
18 characters, hence `FP18`.) This matters because it is also the name
`src/MetroidPrime/ScriptLoaderRel.cpp` would use if the setter's body is ever added, so no second
rename is needed then.

## Measured

```
$ ./tools/decomp_build.sh
All:  31.22% fuzzy, 23.51% matched, 11.82% linked (10240 / 28465 functions)
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp
   .text      claimed    220   ours    220   retail    220   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/audit_rel_claim.py Sandworm
ok   MetroidPrime/ScriptObjects/CSandwormRel.cpp          0x00000000..0x000000DC  4/4 functions
ok   REL/global_destructor_chain.c                        0x00015890..0x00015904  2/2 functions
ok   REL/REL_Setup.cpp                                    0x00015904..0x00015AA8  5/5 functions
0 claim(s) with a problem
Sandworm: preplf 383 text symbols, plf 383, 0 dropped by -strip_partial
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSandwormRel.cpp
ok: 0 unit(s) checked, none emits its functions out of retail order
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
$ python3 tools/check_raw_offsets.py
ok: 156 raw-offset site(s) in 65 file(s), all documented in raw_offsets.md
$ python3 tools/check_module_wiring.py
94 unit(s) of our own code in 74 module(s): ... (Sandworm in the list)
$ MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/goal/judge/report.base.json
... report  ok ; per-function diff  SPLIT  Sandworm/auto_00_00000000_text: 375 function(s)
    accounted for across 2 new unit(s) in Sandworm (exact count match - a split, not a loss)
GATE PASS  0674096+5 changed
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-rel-head-sandworm
  ok counts: matched 10236 -> 10240   linked 5018 -> 5022
  ok target rose: module:Sandworm: 7 -> 11 / 383 functions
  ok no asm added
```

All 86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/` (measured at
`build/G2ME01/<Mod>/<Mod>.rel` - the `build/G2ME01/files/RelProd/` path the first section quotes
does not exist and makes all 86 look different): **86 checked, 0 diffs**;
`build/G2ME01/Sandworm/Sandworm.rel` sha1 `f818ae142461676bd2532a9efa6e7f7d0a15f7f3` =
`config/G2ME01/config.yml`. `total_functions` **28465 before and after**; `complete_units` 735 -> 736.

**No function anywhere got worse.** Per-function diff of the two `build/report.json` over all 28465
functions: **0 with a changed match verdict anywhere**, 376 gone / 376 new - 375 the
`auto_00_00000000_text` -> `auto_00_000000DC_text` split (371) plus the 4 of the new unit, and 1
the DOL rename `fn_8021887C` -> `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` (match `None` and 8
bytes before and after, unit 0/1 on both sides).

Built object: `powerpc-eabi-nm -n` gives exactly four defined symbols in retail order - `fn_56_0`
0x0, `RELExit` 0x2c, `RELMain` 0x50, `fn_56_70` 0x70 - and `powerpc-eabi-objdump -h` gives `.text`
0xDC and nothing but `.comment`, so the 36-virtual stand-in class emits no vtable and the split
claims `.text` only.

## `flip_test.sh` still cannot be run for this unit - a pre-existing defect in the tool

Re-measured here, and the control still holds: the same command on a committed, landed unit fails
identically on this branch.

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CSandwormRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CSandwormRel.cpp) - ...
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp
kept: 0 / 1   failed: 1   skipped: 0
   FAIL MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp
```

`unit_info()` in that script picks the source root by counting `(` against `)` between the last
`MusyX(` and the `Object(...)` entry, and it counts **inside comments** - any later comment with an
unbalanced `(` makes every entry after it look like it lives under `extern/musyx/src/`. I cannot fix
`tools/`, and `goal_check.sh` does not run `flip_test.sh` for a `progress` item. What stands in for
it, and is the one rule for a REL unit anyway, is that **`Sandworm.rel` still hashes to
`config/G2ME01/config.yml` with our object in the link** - the build log shows
`[1/11] MWCC build/G2ME01/src/MetroidPrime/ScriptObjects/CSandwormRel.o` and
`[2/11] LINK build/G2ME01/Sandworm/Sandworm.plf`, the split claims 0x0..0xDC for a `Matching`
object, and `gate.sh`'s `hashes vs config.yml` (all 86) is `ok`.

## NEW: lines

None. The item is complete, not blocked. What this run added - the carry-conflict cause and the
placement that avoids it, which argument register `__ptmf_scall4` adjusts, the family-name spelling
that removes the `extern "C"` workaround, and the wrong REL path the first section quotes - are
lessons, and they are in `configure.py`'s comment, the source file's header and the sections above,
not filed as work.
