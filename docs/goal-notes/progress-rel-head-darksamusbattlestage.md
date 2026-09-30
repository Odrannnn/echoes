# progress-rel-head-darksamusbattlestage

**Item:** `progress` on `module:DarkSamusBattleStage` (module 11). The module had no own-code
unit at all: its only unit was `DarkSamusBattleStage/auto_00_00000000_text`, 20 functions,
0 matched. Measured baseline: module sha1 `9fc5ff78c8fbe921198df31dfcd771373b4f63f0`,
`main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, global `matched 10127 / 28465`,
`linked 4917`.

## What landed

**8 of the module's 20 text functions now match, from 0.**

| unit | claim | functions | result |
|---|---|---|---|
| `MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp` (new, `Matching`) | `.text 0x0..0x74` | 3 | 3/3 at 100.00% |
| `REL/REL_Setup.cpp` (shared "REL" lib) | `.text 0xD94..0xF38`, `.rodata 0x30..0xB4` | 5 | 5/5 at 100.00% |
| `auto_00_00000074_text` (dtk) | `.text 0x74..0xD94` | 12 | retail, unclaimed |

`3 + 5 + 12 = 20`, which is exactly the module's complete text symbol count and exactly the
sum of its units' `total_functions` in `build/report.json`.

### The three head functions

```
0x00  RELExit  0x24  li r3,0 ; bl fn_80235DCC
0x24  RELMain  0x20  bl fn_11_44
0x44  fn_11_44  0x30  lbl_11_bss_0 = fn_11_74 ; fn_80235DCC(&lbl_11_bss_0)
```

**This module has no accessor block**, which is why the head is three functions and 0x74
bytes where `CIngPuddleRel.cpp`'s is five and 0xA8 and `CSwampBossStage1Rel.cpp`'s is
seventeen and 0x160. That is measured, not assumed: `build/G2ME01/DarkSamusBattleStage/asm/
auto_00_00000000_text.s` has `fn_11_0` calling `fn_80235DCC` and nothing else, where the
MysteryFlyer family's `fn_45_0` is `li r3,1`. The class is a `CScript` stage object over a
`CEntity` base: its vtable is the **0x20-byte** table at `.data:0x0` and holds
`fn_11_B78`, `TypesMatch__27CScriptDarkSamusBattleStageCFi`, `PreThink__7CEntityFfR13CStateManager`,
`Think__7CEntityFfR13CStateManager`, `AcceptScriptMsg__7CEntityFR13CStateManagerRC10CScriptMsg`
and `SetActive__7CEntityFb` after two leading words - six virtuals, not a `CActor`'s fourteen.
So there is no `GetBoundingBox` wrapper, no `+0x818` member accessor and no `lbl_8041AAB8`
store to reproduce, and nothing is missing from the block: `fn_11_74` (0x74, 0x150) is
already the module's entity loader. **A next run should not copy an accessor-block head
here** - diff against `CIngPuddleRel.cpp`, the three-function one, not `CSwampBossStage1Rel.cpp`.

### The record is four bytes, and this module's `.bss` settles it with no reading to do

`build/G2ME01/DarkSamusBattleStage/asm/auto_05_00000000_bss.s` holds **exactly one** object,
`lbl_11_bss_0` `size:0x4` at `.bss:0x0`, and it is the loader slot. Contrast `CSwampBossStage1Rel.cpp`,
where `.bss` holds three objects and the loader is the *last*, and `CSwampBossStage2Rel.cpp`,
four objects and the loader is the *second*: those two had to read the offset off the dump.
Here there is nothing to choose. The slot is referenced only by `fn_11_44` and read only by
`LoadDarkSamusBattleStage` in the `Matching` unit `src/MetroidPrime/ScriptLoader/
DarkSamusBattleStage.cpp`, so there is no second reader and no `__ptmf_scall` pmf.

The setter import is the **plain DOL symbol** `fn_80235DCC` (0x80235DCC, 8 bytes,
`stw r3, gLoader_DarkSamusBattleStage@sda21(r0); blr`, in
`build/G2ME01/asm/auto_03_80235DCC_text.s`, immediately after
`LoadDarkSamusBattleStage__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80235DA0 which
is 44 bytes and so ends exactly there), so **no `symbols.txt` rename and no DOL change**.
`strings build/G2ME01/DarkSamusBattleStage/DarkSamusBattleStage.plf | grep 80235D` prints
`fn_80235DCC`, which is where the module's own import table gets the name.

**No dead-strip hazard**: the module's `ldscript.lcf` FORCEACTIVE holds `_prolog`, `_epilog`,
`_unresolved`, `fn_11_74`, `fn_11_B70`, `fn_11_B78`, `_ctors`, `_dtors` and every `lbl_11_*`.
The three head functions are not in it but are reachable from it - `_epilog` calls RELExit,
`_prolog` calls RELMain, RELMain calls `fn_11_44` - and `audit_rel_claim.py` measures the
result directly: **preplf 20 text symbols, plf 20, 0 dropped by `-strip_partial`**.

## Config changes (intended list, not a copy from another tree)

- `configure.py`: a new `Rel("DarkSamusBattleStage", [Object(Matching,
  "MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp")])` block, placed directly
  after `DarkSamus` (module 10, the module immediately before this one).
- `config/G2ME01/rels/DarkSamusBattleStage/splits.txt`: two claims added - the head
  `.text 0x0..0x74`, and `REL/REL_Setup.cpp` at `.text 0xD94..0xF38` +
  `.rodata 0x30..0xB4`. Both ranges come from `tools/scaffold_rel_module.py DarkSamusBattleStage`;
  the module claimed nothing before.
- `config/G2ME01/rels/DarkSamusBattleStage/symbols.txt`: four renames, all `scope:global` -
  `fn_11_0` -> `RELExit` and `fn_11_24` -> `RELMain` (both read off the disassembly:
  `_epilog`'s second `bl` and `_prolog`'s, and each is the family's 0x24 / 0x20 shape), and
  `fn_11_EA0` -> `ModuleDestructors`, `fn_11_EEC` -> `ModuleConstructors` (the two 0x4C
  ctor/dtor-table walkers, the first calling `_dtors` and the second `_ctors`).
- Not added to `files.cmake`, for the reason the other heads measure: listing the file would
  make the port link `fn_11_74` and `fn_80235DCC`, which it cannot.
- `docs/HANDOFF.md` is **not** mine - `goal_check.sh` rewrote the derived state block.

## Measured

```
sha1sum build/G2ME01/main.dol                                    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs against config/G2ME01/config.yml                         0 differ
all 86 .rel cmp-equal to orig/G2ME01/files/RelProd/              0 differ
DarkSamusBattleStage.rel                                         9fc5ff78c8fbe921198df31dfcd771373b4f63f0  (unchanged)
total_functions                                                  28465  (unchanged)
All: 31.17% fuzzy, 23.46% matched, 11.79% linked (10135 / 28465)   from 31.16 / 23.45 / 11.78 (10127)
audit_rel_claim.py DarkSamusBattleStage   0 problems, 3/3 and 5/5, 0 of 20 dropped
unit_fit.sh CScriptDarkSamusBattleStageRel.cpp  .text claimed 116 ours 116 retail 116, fits;
                                              no extra functions
check_decl_order.py --unit DarkSamusBattleStage  ok, 3 units
check_symbol_names.py                          505 units, 0 missing
check_raw_offsets.py                           152 sites in 61 files
check_docs_claims.py                           docs claims agree with the tree
goal_check.sh build/goal/item.json             PASS, target rose 0 -> 8 / 20
```

Zero units lost matched functions and zero functions lost fuzzy percent, measured by diffing
`build/goal/judge/report.base.json` against `build/report.json` per unit and per function.

## Two things a next run should know

**`tools/flip_test.sh` cannot judge any REL unit - it is broken, and it was already broken.**
It reports `FAIL` for a source file that exists, in a split, and reproduces retail:

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CIngPuddleRel.cpp
TEST MetroidPrime/ScriptObjects/CIngPuddleRel.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CIngPuddleRel.cpp) - configure.py would link the retail object and this would
    pass while proving nothing (it only prints 'Missing source file')
  FAIL  -> reverted
```

The cause is `unit_info`'s MusyX test in `tools/flip_test.sh`: it takes `s.rfind('MusyX(', 0, m.start())`
and compares the **naive** `(` / `)` counts between there and the entry. `MusyX(` at configure.py:1292
closes at 1326, but the counts are still 87 open against 86 close at `CIngPuddleRel`'s entry, so
the test concludes the unit sits inside a MusyX call and prefixes `extern/musyx/src`. The surplus is
**not** from this change: the same imbalance (87/86) is present in `git show HEAD:configure.py`, and
every REL unit in the tree is affected. `flip_test.sh` is only run by `goal_check.sh` for `match`
items, so `progress` items never hit it. **I did not edit `tools/`** - it is the judge. Worth a
one-line fix (count parens with `ast` or `tokenize` instead of `str.count`), but not from a lane.
The REL equivalent of what flip_test would have checked is already measured above: the unit is
`Matching` in `configure.py`, its object is in the link (objdiff attributes 3 functions to
`DarkSamusBattleStage/MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel`), and with it in
the link the DOL and all 86 RELs still reproduce retail byte for byte.

**The next slice of this module is the 12 unclaimed functions, 0x74..0xD94, and none of it is a
one-liner.** `fn_11_74` (0x150) is the generated `SLdrDarkSamusBattleStage` loader and ends in
`__nw__FUlPCcPCc(0xAC, "CScriptDarkSamusBattleStage", <file>)`, so it needs the `CEntity`
constructor and the `LdrToEntityInfo` record. `fn_11_228` (0x68C) is a 30-way typedef switch on
hashes in the range 0x0800xxxx..0xF5Cxxxxx writing a struct that runs to +0x84, with the
`optional_object<CEntityInfo*>` at +0x3C read out of line (`fn_11_A2C`) and the `optional_object`
destructors at +0x24. `fn_11_90C` (0x120) is the same struct's constructor: 20 float initialisers
from `.rodata` plus 12 `stb` zero/one stores, and the twelve `.rodata` floats at
`.rodata:0x0..0x24` are already named, so it is mechanical. If a lane wants more of this module
the cheapest next win is `fn_11_90C` and `fn_11_B50` - but they are **inside** the 0x74..0xD94 run,
not separable from it, and one unit cannot claim two discontiguous ranges, so a `Matching` claim on
any of them needs a source that also reproduces a referenced neighbour.
