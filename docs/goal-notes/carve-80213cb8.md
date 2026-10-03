# carve-80213cb8

`kind: match`, target `MetroidPrime/ScriptLoader/Carve80213CB8`. **Done and flipped.**

## What it was

`fn_80213CB8` at `.text 0x80213CB8`, 8 bytes, sitting in dtk's
`build/G2ME01/asm/auto_03_80213CB8_text.s`:

```
stw r3, gLoader_SporbBase@sda21(r0)
blr
```

A byte-shape twin of the already-matched `fn_80200E3C`
(`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`) and `fn_80200E70`
(`Carve80200E70.c`) - the whole family shape: store the argument into the loader
pointer's `.sbss` slot and return. Only the `@sda21` displacement differs, because the
three slots are 8 bytes apart (`0x80419358` / `0x80419360` / `0x804193A8`).

Both callers are in module 76's listing (`build/G2ME01/Sporb/asm/auto_00_000026A8_text.s`),
so the argument is read off them and not guessed:

* `RELExit` (0x24 B, `li r3, 0`) - the module tears the loader down on the way out.
* `fn_76_2804` (0x54 B, reached from `RELMain`) does `lis r3, lbl_76_bss_28@ha` then four
  stores into it (`stwu r7` +0, `stw r0` +4, `stw r6` +8, `stw r5` +0xC) before
  `bl fn_80213CB8` with `r3` still on it. So the record is **0x10 bytes = four
  `FScriptLoader`s** (`config/G2ME01/rels/Sporb/symbols.txt:517`, `lbl_76_bss_28 size:0x10
  data:4byte`), which is exactly `SSporbBaseLoaders` as `SporbBase.cpp:16-21` models it -
  which is why that unit carries four thunks.

## What I did

The four-file carve, all in address order, no claim spanning an unclaimed gap:

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve80213CB8.c` | new, 71 lines: header comment in the `Carve8038A7DC.c` style, `struct SSporbBaseLoaders` at file scope, `extern struct SSporbBaseLoaders* gLoader_SporbBase`, one 8-byte body |
| `configure.py:842` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve80213CB8.c"),` on one line, between `SporbBase.cpp` and `Sandworm.cpp` |
| `config/G2ME01/splits.txt:1566-1567` | `.text start:0x80213CB8 end:0x80213CC0`, between `SporbBase.cpp` and `CTweakTargeting.cpp` |
| `files.cmake:855-858` | the source path, with the comment the sibling carve entries carry |

Plus one comment correction, because the claim it made became false:
`src/MetroidPrime/ScriptLoader/SporbBase.cpp:11-15` said the 8-byte setter at 0x80213CB8 is
"deliberately NOT claimed ... must stay in dtk's auto unit". It now names the new unit and
says why the name is not this file's to change. That is the same edit the `Carve80200E3C`
lane made to `SpacePirate.cpp`.

`.text` only - `gLoader_SporbBase` is `.sbss 0x804193A8..0x804193B0`, already claimed and
defined by `SporbBase.cpp`, so it is `extern` here and defined in exactly one unit.
No `PortLinkStubs.cpp` duplicate: `grep fn_80213CB8 src/MetroidPrime/PortLinkStubs.cpp` is
empty, and `gate.sh`'s `port link dups` step passed. One function in the file, so the
reverse-source-order rule cannot be got wrong; `tools/check_decl_order.py --unit` confirms it.

## Measured

* `build/report.json` before: `matched_functions` **13556**, `complete_units` **985**,
  `total_functions` **28465**. After: **13557**, **986**, `total_functions` still **28465**
  (the `splits.txt` edit moved no function between units).
* The new unit: `main/MetroidPrime/ScriptLoader/Carve80213CB8`,
  `fuzzy_match_percent 100.0`, `matched_functions 1 / 1`, `complete_units 1`,
  `metadata.complete: true`.
* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80213CB8.c` -> `PASS -> kept as
  Matching`, `kept: 1 / 1 failed: 0`. That is the acceptance test and it is the only thing
  that decides this item.
* `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, retail.
* `./tools/goal_check.sh build/goal/item.json` -> **PASS**, every line ok:
  no judge-owned path touched; `gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs
  claims, port probe); `matched 13556 -> 13557  linked 6604 -> 6605`;
  `check_symbol_names.py` clean; `All: 37.66% fuzzy, 31.10% matched, 13.97% linked
  (13557 / 28465 functions)`; flip test PASS with `Object(Matching)` in `configure.py`.

Not committed, as instructed. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as
modified in `git status` - those are the judge's own derived-count rewrites from
`goal_check.sh`, not edits of mine.

## For the next lane: the setter family is a queue of 8-byte carves

There are **30** more `auto_*` units that are one 8-byte function of exactly this shape -
`stw r3, gLoader_<X>@sda21(r0) ; blr`, every one of them the setter for a `.sbss` slot a
`ScriptLoader/*.cpp` unit already claims and defines, every one of them sitting in an 8-byte
gap between two claimed units (so no claim spans an unclaimed gap, and no carve can hit the
`Cyclic dependency` link-order wall). I listed them by scanning `report.json` for
`auto_generated` single-function 8-byte units whose `.s` contains one `stw r3, gLoader_` and
one `blr`; `fn_80200EFC`(Parasite), `fn_802188B0`(CommandPirate), `fn_802189D0`(SandBoss),
`fn_80218A04`(FlyingPirate), `fn_80218A38`(Grenchler), `fn_80218A6C`(MediumIng),
`fn_80218AA0`(MinorIng), `fn_80218AD4`(ElitePirate), `fn_80218B68`(MetroidAlpha),
`fn_80218BC8`(GunTurretBase), `fn_80218BFC`(Lumite), `fn_80218C30`(Shrieker),
`fn_80218CF0`(SplitterMainChassis), `fn_80218D24`(ChozoGhost), `fn_80218D58`(Tryclops),
`fn_80218D8C`(WispTentacle), `fn_80218DC0`(SpankWeed), `fn_80218DF4`(DarkTrooper),
`fn_8021F9B0`(DigitalGuardian), `fn_8021F9E4`(Shredder),
`fn_8021FA18`(FrontEndDataNetwork), `fn_8021FA4C`(StoneToad), `fn_80227AF8`(Rezbit),
`fn_80229FF0`(StreamedMovie), `fn_8022A024`(IngSpiderBallGuardian),
`fn_8022EBC8`(EmperorIngStage3), `fn_8022EBFC`(DestructableBarrier),
`fn_8022EC30`(SwampBossStage2), `fn_8022FFC4`(IngBoostBallGuardian),
`fn_80235DCC`(DarkSamusBattleStage).

Each is the recipe in this file with one variable changed, so each is worth exactly **+1**
`matched_functions` and `linked` and takes minutes, not a lane-hour. **I filed no `NEW:`
lines for them, on purpose:** `wt-mp2-goal/build/goal/queue.json` already carries 14 queued
`MetroidPrime/ScriptLoader/Carve*` items, seven of them from this family
(`carve-802188b0`, `carve-802189d0`, `carve-80218a04`, `carve-80218a38`, `carve-80218a6c`,
`carve-80218aa0`, `carve-80218ad4`, plus `carve-802187e4`, `carve-80218918`,
`carve-8022d574`, `carve-80218b08`, `carve-80218b68`, `carve-80218bc8`), so the driver has
the queue and does not need it re-filed. This list is here so the next lane does not
re-derive it, and so the queue can be topped up from a measured list rather than a guess.

### One lesson, for whoever does the next one

The argument type does not need to be right, and getting it *more* right does not help.
`Carve80200E3C.c` and `Carve80200E70.c` each declare a real struct for the module's record
(`SSpacePirateFuncPtrs`, `SKraleeLoader`); mine declares `SSporbBaseLoaders` with four
`unsigned int` where `FScriptLoader` would be more honest, purely to avoid needing a
`FScriptLoader` typedef in a `.c` file that includes no header. All three emit
`stw r3, <sym>@sda21(r0) ; blr` byte-for-byte, because **MWCC does not encode a variable's
type in its name** and the body is one pointer store. MWCC's warning is not the constraint
here; the `stw` is.