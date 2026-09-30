# progress-rel-head-ingboostballguardian

`kind: progress` · `target: module:IngBoostBallGuardian` (module 30) · worked 2026-09-30 in
`wt-mp2-goal-L4` on `goal/lane-4`. **Judge: PASS** (`./tools/goal_check.sh build/goal/item.json`,
every check `ok`).

## What I did

Decompiled the module's **head** - `.text 0x0..0x130`, the seventeen functions above the module's
own class code - as a new `Matching` unit, following the module recipe in
`docs/RUNNING_THE_DECOMP.md`. This is the `CIngSpaceJumpGuardianRel.cpp` / `CMetroidRel.cpp`
arrangement: one contiguous range, one file, the module-unique name, and everything else left
unclaimed so `dtk` fills it from retail.

The range came from `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt`:

```
0x000 fn_30_0   0x08  addi r3,r3,0xaec
0x008 fn_30_8   0x08  addi r3,r3,0xbd8
0x010 fn_30_10  0x08  li r3,1
0x018 fn_30_18  0x0C  lbl_30_rodata_64   (.rodata:0x64, .float 1 - this module's own)
0x024 fn_30_24  0x08  lbz r3, 0x44f(r3)
0x02C fn_30_2C  0x08  li r3,0
0x034 fn_30_34  0x08  li r3,0
0x03C fn_30_3C  0x10  *self = kInvalidUniqueId
0x04C fn_30_4C  0x0C  byte at +0x34c, bit 3
0x058 fn_30_58  0x0C  lbl_8041B758
0x064 fn_30_64  0x08  addi r3,r3,0x754
0x06C fn_30_6C  0x08  li r3,1
0x074 fn_30_74  0x1C  three floats from self+0x54 -> *out
0x090 fn_30_90  0x2C  virtual dispatch, vtable slot 0x38
0x0BC RELExit   0x24  li r3,0 / bl fn_8022FFC4
0x0E0 RELMain   0x20  bl fn_30_100
0x100 fn_30_100 0x30  lbl_30_bss_6C = fn_30_130 ; fn_8022FFC4(&lbl_30_bss_6C)
```

## Measured

- **The unit is 17/17 functions at 100.00%.**
  `IngBoostBallGuardian/MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel` in
  `build/report.json`: `fuzzy_match_percent 100.0`, `matched_functions 17`,
  `total_functions 17`.
- **Module `matched_functions` 7 -> 24 of 318**, exactly the 17 added (the 7 were
  `REL/global_destructor_chain` 2 + `REL/REL_Setup` 5). `goal_check` printed
  `target rose: module:IngBoostBallGuardian: 7 -> 24 / 318 functions`.
- **All 86 REL sha1s hold** against `config/G2ME01/config.yml` (checked with the doc's own
  snippet: `86 RELs checked, 0 differ`), and `build/G2ME01/IngBoostBallGuardian/IngBoostBallGuardian.rel`
  is `cmp`-equal to `orig/G2ME01/files/RelProd/`.
- `main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `probe_sources.sh` = `749 files, 0
  failed, 0 errors`; `check_symbol_names.py` = `checked 505 units; 0 declared names are missing`;
  `check_docs_claims.py` = `docs claims agree with the tree`; `unit_fit.sh` = `.text claimed 304
  ours 304 retail 304 fits` + `no extra functions`.
- Project counts: matched `10128 -> 10145`, linked `4917 -> 4934`,
  `All: 31.17% fuzzy, 23.46% matched, 11.78% linked (10145 / 28465 functions)`. `total_functions`
  is still **28465**; the splits edit moved no DOL function.

## Files changed (four, all of them needed)

- `src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel.cpp` - new, 17 functions in
  **descending** retail text order.
- `config/G2ME01/rels/IngBoostBallGuardian/splits.txt` - one new claim, `.text 0x0..0x130`, placed
  ahead of the two existing entries.
- `configure.py:1549-1573` - the `Rel("IngBoostBallGuardian", [Object(Matching, ...)])` block,
  with the measured note. **Not** in `files.cmake`, for the reason every other landed head records:
  listing it would make the port link `fn_30_130` and `fn_8022FFC4`, which it cannot.
- `docs/research/raw_offsets.md` - a `## ...CIngBoostBallGuardianRel.cpp (1 site)` section
  (required by `tools/gate.sh`'s raw-offsets check, which is the one that failed my first
  `goal_check` run), and the debt total, which I re-measured.

## Findings worth keeping

1. **`flip_test.sh` does not work on a REL unit** and its FAIL is vacuous: it prints
   `no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel.cpp) -
   configure.py would link the retail object and this would pass while proving nothing (it only
   prints 'Missing source file')`, then `FAIL -> reverted (tree rebuilt ...)`. It also **reverted
   the tree** - check `git status` after running it on a REL head. The module's sha1 against
   `config.yml` is the REL acceptance test (`AGENTS.md`), and it holds.
2. **No dead-strip hazard, measured.** `build/G2ME01/IngBoostBallGuardian/ldscript.lcf` puts all
   fourteen of `fn_30_0`..`fn_30_90` in FORCEACTIVE, so no `force_active:` entry in
   `config.yml` is needed. `.data:0x9C0` (0x148 bytes = 82 words: two leading + 80 virtuals)
   stores `fn_30_90` at word 15, i.e. vtable offset 0x3C, immediately above the
   `HealthInfo__3CAiFv` it dispatches to at 0x38 - the family's shape, measured, not assumed.
3. **The loader slot is `.bss:0x6C`, not `.bss:0x0`.** `lbl_30_bss_6C` is
   `size:0x4 data:4byte`, the module's own copy of the loader pointer. This module's `.bss:0x0` is
   a different 0x8-byte object read and written by `fn_30_130`, so the name is load-bearing.
4. **The setter is a plain DOL symbol, so no `symbols.txt` rename and no DOL change.**
   `fn_8022FFC4` is 0x8022FFC4, `stw r3, gLoader_IngBoostBallGuardian@sda21(r0); blr`
   (`build/G2ME01/asm/auto_03_8022FFC4_text.s`; `config/G2ME01/symbols.txt:9963`). It stores the
   *address* of a loader slot, and the record is four bytes because the loader is the only thing
   the DOL reads out of it - the "read the record's size off both readers" trap does not bite here.
5. **This head is the family in a different order**, which is why diffing bytes is the method and
   the `fn_<id>_<off>` names are not: it opens with **two address accessors a word apart**
   (`+0xAEC` then `+0xBD8`) where the family opens with one address accessor and a `li r3,1`; it
   has **no** `GetBoundingBox` wrapper and **no** `lbl_8041AAB8` store at +0x448 (so
   `lbl_8041B758` is the only DOL global its relocations name); and its module-local `.rodata`
   constant sits at 0x18 rather than 0x10. Every body still came from a sibling already at 100%
   - nothing had to be re-derived except the ordering.
6. **The three-float copy must stay spelled as subscript stores.** `fn_30_74` interleaves its loads
   and stores (`lfs 0x54 / stfs 0 / lfs 0x58 / stfs 4 / lfs 0x5c / stfs 8`); a `CVector3f` copy
   reverses the loads and the score falls (`CMetareeSwarmRel.cpp` records this).
7. **`check_decl_order.py --unit <name>` silently checks nothing** for these files: it reported
   `ok: 0 unit(s) checked`. The `--list`/bare run does check them - `ok: 959 unit(s) checked, 31
   permuted, all 31 accounted for` - and my unit is not among them.
8. `tools/check_raw_offsets.py` measures my file as **1 site**, not six: only `+0x54` goes through
   a `reinterpret_cast`. The other five (`+0x44F`, `+0x34C`, `+0x754`, `+0xAEC`, `+0xBD8`) are
   `static_cast< char* >` or a subscript, which `BYTE_CAST` does not key on. The doc's debt total
   was already stale before this change - it said `150 in 59` while the tool measured `152 in 61`;
   it now reads `153 sites in 62 files`, which is what `--list` prints.

## What is left

The module's other **301** functions stay retail and unclaimed, in
`IngBoostBallGuardian/auto_00_00000130_text` (247), `auto_00_00011CD8_text` (46) and
`auto_fn_30_110C4_text` (1). They are `CIngBoostBallGuardian`'s own methods, and moving them needs
the `CIngBoostBallGuardian`/`CActor`/`CPatterned`/`CAi` hierarchy this tree does not model - the
same blocker every other head in this family records, and it starts at the module's entity loader
`fn_30_130` (0x130, 0x4B4).
