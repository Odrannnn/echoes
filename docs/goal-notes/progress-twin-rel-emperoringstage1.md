# progress-twin-rel-emperoringstage1

`twin` item on `module:EmperorIngStage1` (module 16). **PASS.** The module's summed
`matched_functions` went **14 -> 22 of 241**; the project's went **13098 -> 13106 of 28465**
(`linked` 6196 -> 6204). Nothing was flipped to get there: both units added here were
`Matching` from the start, so every one of the 8 functions is in the binary with our object
in the link, which is the one rule.

## What I did

I did **not** use the twin list. Re-reading the module first showed the two claims that pay
without writing a single destructor: the recipe's own two shapes, both already landed on
sibling modules. The twin list (82 functions, mostly `rstl::` template destructors for
classes this tree does not model) is the wrong work order here - see "What is left" below.

1. **`MetroidPrime/ScriptObjects/CEmperorIngStage1Rel.cpp`, `.text 0x0000A22C..0x0000A2A0`,
   3 functions, 100%** - the module's entry-point block, `CIngPuddleRel.cpp` /
   `CScriptDarkSamusBattleStageRel.cpp`'s arrangement with this module's own names:

   | addr | retail | body |
   | --- | --- | --- |
   | 0xA22C | `RELExit` | `li r3,0 ; bl fn_8022756C` |
   | 0xA250 | `RELMain` | `bl fn_16_A270` |
   | 0xA270 | `fn_16_A270` | `lbl_16_bss_0 = fn_16_A2A0 ; fn_8022756C(&lbl_16_bss_0)` |

   **The block is in the middle of the module, not at its head**, and that is measured, not
   chosen: this module's head accessors are already claimed by the existing
   `EmperorIngStage1Accessors.cpp` at 0xB994..0xBA30, and its own entity loader `fn_16_A2A0`
   (0x31C) is the very next function above the range. One unit cannot claim two discontiguous
   ranges, so this needed its own file and its own `Object(...)`.

2. **`REL/REL_Setup.cpp`, `.text 0x0000C2A4..0x0000C448` + `.rodata 0x00000AC0..0x00000B44`,
   5 functions, 100%** - the "free 0 -> 5" every module gets once its `splits.txt` claims the
   tail (`_unresolved`, `_epilog`, `_prolog`, `ModuleDestructors`, `ModuleConstructors`).
   `configure.py` already carries `REL/REL_Setup.cpp` in the shared "REL" lib, so it must
   **not** be named in the `Rel(...)` block a second time ("Duplicate object name"); what the
   module actually needed was the two renames in its `symbols.txt`.

3. **`config/G2ME01/rels/EmperorIngStage1/symbols.txt`**, four renames, each replacing its
   `fn_` line (never inserting beside it, which is a dtk parse error):
   `fn_16_A22C -> RELExit`, `fn_16_A250 -> RELMain`, `fn_16_C3B0 -> ModuleDestructors`,
   `fn_16_C3FC -> ModuleConstructors`, all `scope:global`. `RELExit`/`RELMain` are what make
   `_epilog`/`_prolog` in the shared unit resolve against *this* file's definitions instead of
   dtk's retail ones; without `scope:global` the module grows 48 bytes of relocations and the
   hash breaks.

`fn_8022756C` (the DOL setter `stw r3, gLoader_EmperorIngStage1@sda21(r0); blr`,
`build/G2ME01/asm/auto_03_8022756C_text.s`) is left as the plain DOL symbol and the 8-byte
DOL unit is deliberately **not** claimed - REL modules import it by its retail name.
`lbl_16_bss_0` is `.bss:0x0 size:0x4`, unclaimed, so `extern` under `__MWERKS__` and a host
definition, for the reason `CMysteryFlyerRel.cpp` records. Definitions are declared in
descending retail text order; `python3 tools/check_decl_order.py --unit CEmperorIngStage1Rel`
says ok.

Not in `files.cmake`: the file defines a module entry point, which is `check_files_cmake.py`'s
counted-and-skipped case (it reports 47 such units). That also keeps the port's undefined
count at 287.

## What I measured

```
./tools/decomp_build.sh -r    # All: 37.09% fuzzy, 30.52% matched, 13.47% linked (13106 / 28465)
```
- module sum, `build/report.json`: `EmperorIngStage1/MetroidPrime/ScriptObjects/CEmperorIngStage1Rel`
  **3/3 at 100.00%**, `.../REL/REL_Setup` **5/5 at 100.00%**, `.../EmperorIngStage1Accessors`
  unchanged **14/14**. Sum **22**, was 14 (`build/goal/judge/report.base.json`).
- `total_functions` still **28465** - the two claims moved functions between units, none lost.
- Module sha1 vs `config/G2ME01/config.yml`: all **86 RELs OK**, `main.dol`
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/audit_rel_claim.py EmperorIngStage1`:
  `ok ...CEmperorIngStage1Rel.cpp 0x0000A22C..0x0000A2A0 3/3`,
  `ok ...EmperorIngStage1Accessors.cpp 0x0000B994..0x0000BA30 14/14`,
  `ok REL/REL_Setup.cpp 0x0000C2A4..0x0000C448 5/5`, **0 claims with a problem**,
  `241 text symbols, plf 241, 0 dropped by -strip_partial`. So the claims span no unclaimed
  gap and claim nothing dtk had to fill.
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS
  progress-twin-rel-emperoringstage1`**, gate.sh green, `no asm added`.
- `python3 tools/check_symbol_names.py` -> `checked 584 units; 0 declared names are missing`.

## `flip_test.sh` cannot judge REL units here - pre-existing, not caused by this change

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/EmperorIngStage1Rel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CEmperorIngStage1Rel.cpp)
  FAIL  -> reverted
```
It is **not** a missing file. `flip_test.sh`'s `unit_info` decides the source root by
"is there an unbalanced `(` between the last `MusyX(` and this entry", and on `configure.py`
as committed that test says `extern/musyx/src` for **every** REL unit. Verified by replaying
its own regex against `git show HEAD:configure.py`:
`HEAD .../CScriptDarkSamusBattleStageRel.cpp -> extern/musyx/src`, and the same FAIL for
`CMysteryFlyerRel` and for the pre-existing `EmperorIngStage1Accessors` - all three committed
`Matching` units with real sources under `src/`. The heuristic's parenthesis count is confused
by the parenthesis inside a comment or string before the entry; it is a latent tool defect,
not a property of my unit. **The REL equivalent of the flip is the module sha1, and it holds**,
so the item is judged on that. Not filed as `NEW:` - it is a tooling defect and would not
raise a count.

## What is left in the module (191 unmatched functions)

- `auto_00_0000A2A0_text` (0xA2A0..0xB994, 32 functions) is the module's own generated
  entity loader `fn_16_A2A0` plus its class code - `SLdrEditorProperties`,
  `SLdrPatternedAITypedef`, `SLdrActorParameters` and a 30-way typedef switch. It needs the
  `CEntity`/`CPatterned` hierarchy this tree does not model; `CMysteryFlyerRel.cpp`'s header
  records the same decision for the same reason. **Not a wall I measured - untouched.**
- `auto_00_0000BA30_text` (15) and `auto_fn_16_BF88_text` (1) and `auto_00_00000000_text`
  (171) are the rest of the class code.
- The twin list's 82 entries are real twins, but almost all of them are `rstl::` template
  destructors (`__dt__rstl::vector<...>Fv`, `__dt__CStaticInterferenceFv`,
  `__ct__CHealthInfoFRC...`) belonging to types that have no declaration in this tree.
  Writing one as `extern "C" void fn_16_x(void* p) { ... }` over raw offsets is what the
  existing `EmperorIngStage1Accessors.cpp` does for accessors, but a **destructor** body needs
  member offsets, member-destructor calls and a `Free(this)` path that only a real class
  declaration produces. A wrong guess does not score 0%, it scores wrong code at 99% - worse
  than not writing it. Whoever takes this module next should start from a named class, the
  way the `TypesMatch` recipe in `docs/RUNNING_THE_DECOMP.md` does, not from the twin list.
- `NEW:` filed: none. Nothing I measured this run is a unit that can reach 100% in one more
  item, so there is no honest one to queue.