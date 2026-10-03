# carve-80235dcc - `MetroidPrime/ScriptLoader/Carve80235DCC.c`

**Result: the unit is `Matching` and verified by `tools/flip_test.sh`.**
`./tools/goal_check.sh build/goal/item.json` exits 0 (`goal_check: PASS carve-80235dcc`), with
`matched 13580 -> 13581` and `linked 6628 -> 6629`. All six judge lines `ok`.

## What I did

The item's four files, all in this one change, as a carve is required to be:

1. `src/MetroidPrime/ScriptLoader/Carve80235DCC.c` - new, 92 lines: the 8-byte body
   `stw r3, gLoader_DarkSamusBattleStage; blr` (`void fn_80235DCC(struct
   SDarkSamusBattleStage_FuncPtrs* loader)` at line 91) plus the header comment in the style of
   `src/Dolphin/Carve8038A7DC.c` and the sibling carves `src/MetroidPrime/ScriptLoader/
   Carve802188E4.c` (DarkSamus, the same module one address along in the same loader block) and
   `Carve80200E3C.c`.
2. `configure.py:958` - `Object(Matching, "MetroidPrime/ScriptLoader/Carve80235DCC.c"),` on one
   line, in address order between `DarkSamusBattleStage.cpp` (line 957) and `DarkCommando.cpp`
   (line 959).
3. `config/G2ME01/splits.txt:2007-2008` - `.text start:0x80235DCC end:0x80235DD4`, in address
   order between `DarkSamusBattleStage.cpp` (ends 0x80235DCC) and `DarkCommando.cpp` (starts
   0x80235DD4). **`.text` only**: the `.sbss` slot belongs to `DarkSamusBattleStage.cpp`.
4. `files.cmake:954-958` - the entry with its comment, after `Carve8023492C.c` and before
   `Carve802392A4.cpp`.

Claimed exactly `0x80235DCC..0x80235DD4` and nothing else. One function, so declaration order
cannot be wrong, but `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptLoader/Carve80235DCC.c` was run anyway: `ok: 0 unit(s) checked, none emits its
functions out of retail order` (a one-function unit is not what that tool looks at).
`total_functions` is still **28465** after the `splits.txt` edit, and `All:` did not fall.
No `PortLinkStubs.cpp` duplicate exists (grepped `src/MetroidPrime/PortLinkStubs.cpp` for
`80235DCC`: no match). No `asm` was added, and no `docs/` file was edited by me - the
`docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` edits in `git status` are the judge's own
derived-count rewrites from `goal_check.sh` (matched 13580->13581, linked 6628->6629, probe
981->982 files).

## What I measured

- Retail bytes: `build/G2ME01/asm/auto_03_80235DCC_text.s` - `stw r3,
  gLoader_DarkSamusBattleStage@sda21(r0)` / `blr`, 8 bytes, `fn_80235DCC`.
- `config/G2ME01/symbols.txt:10045` - `fn_80235DCC = .text:0x80235DCC; // type:function size:0x8
  align:4`. `symbols.txt:10046` is the next thing in the DOL, `LoadDarkCommando` at 0x80235DD4.
- `strings build/G2ME01/DarkSamusBattleStage/DarkSamusBattleStage.plf | grep 80235D` ->
  `fn_80235DCC`, so the name is retail's and the unit must be `.c` (`-lang=c`). It is the same
  import `src/MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp:80` already declares
  `extern "C"`, which names the argument `FScriptLoader*`.
- Module call sites, `build/G2ME01/DarkSamusBattleStage/asm/auto_00_00000000_text.s`: `fn_11_0`
  (`RELExit`) materialises `li r3, 0x0` at line 12 and calls it at line 14; `fn_11_44` at
  .text:0x44 materialises `lis r4, fn_11_74@ha` (line 37), `addi r0, r4, fn_11_74@l` (line 40),
  stores it at `lbl_11_bss_0` with `stwu r0, lbl_11_bss_0@l(r3)` (line 41) and hands **the cell's
  address** over (line 42). So the argument is the address of a one-word loader cell.
- `build/G2ME01/DarkSamusBattleStage/asm/auto_05_00000000_bss.s`: `lbl_11_bss_0` is `.bss:0x0
  size:0x4 data:4byte` and is the module's only `.bss` object.
- The slot is `.sbss:0x80419658; // type:object size:0x8 data:4byte` (`symbols.txt:20795`), owned
  and defined by `MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp:16`
  (`splits.txt:2003-2005`), whose reader calls `(*gLoader_DarkSamusBattleStage.value)(...)`
  (line 19). So this unit takes it `extern` and claims `.text` only.
- Our object is the twin's shape exactly (control run against `Carve802188E4.o`):
  `T fn_80235DCC` / `U gLoader_DarkSamusBattleStage` and one `R_PPC_EMB_SDA21
  gLoader_DarkSamusBattleStage` at offset 0.
- Report entry after the build: `main/MetroidPrime/ScriptLoader/Carve80235DCC`,
  `fuzzy_match_percent 100.0`, `matched_functions 1 / 1`, `complete_units 1`,
  `metadata.complete True`, one `.text` section of 8 bytes at 2149801420 (0x80235DCC).
  `main/auto_03_80235DCC_text` is gone from the report.
- Gates: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `check_symbol_names.py` 0 missing
  names over 609 units; `unit_fit.sh` ".text claimed 8 ours 8 retail 8 fits / no extra functions";
  `All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13581 / 28465 functions)`; `gate.sh` green
  inside the judge (all 86 RELs, `hashes vs config.yml ok`, `port probe ok`, report diff, wiring,
  docs claims). Probe count rose 981 -> **982** files, 0 failed, 0 errors, `LINKED (286
  undefined, 0 duplicates)` - the port compiles this file too and it adds no undefined symbol.

## Two notes for the next lane

**`tools/carve_diff.sh` takes an address, not a unit name**
(`carve_diff.sh:15` does `int(sys.argv[1], 16)`), so `carve_diff.sh
MetroidPrime/ScriptLoader/Carve80235DCC.c` dies with `ValueError` before it prints anything.
Passed the *name* it cannot run at all; and per `carve-80218bfc`'s note it could not decide this
family anyway (it disassembles the unlinked object where the `R_PPC_EMB_SDA21` is still `0(0)`).
What decides a carve is `flip_test.sh` plus objdiff's 100%. Recorded because the briefing file's
build section suggests running it.

**`flip_test.sh` needs the unit's real filename, not the briefing's `.cpp` spelling.**
`goal-unit-prompt.md` says "the trailing .cpp is required"; that is only true for a unit whose
source is really a `.cpp`. `unit_info()` greps `configure.py` for the argument verbatim, so
`flip_test.sh MetroidPrime/ScriptLoader/Carve80235DCC.cpp` prints `SKIP ... not listed in
configure.py` and **exits 0** on this unit (an already-`Matching` entry is matched by the
literal string, so nothing is skipped as a failure). `tools/goal_check.sh:177` gets this right -
it tries `.cpp`, `.cp`, then `.c` - which is why the judge's own flip line is `PASS`. Not a tool
bug worth fixing here (a change under `tools/` fails the item outright); flagging it so nobody
reads the SKIP as a pass.

## Still unclaimed, same shape - all ten re-measured here, not recalled

`carve-80218bfc` listed twelve `stw r3, gLoader_X@sda21(r0) ; blr` 8-byte setters still sitting
alone in an `auto_03_*` unit. I re-measured that list against this tree rather than trusting it:
`80218B68` and `80235DCC` are now carved, and the **remaining ten are all still unclaimed**
(`grep "start:0x<addr> " config/G2ME01/splits.txt` -> 0 hits for each), each still has its own
8-byte `build/G2ME01/asm/auto_03_<addr>_text.s` with that one `stw` and a `blr`, and each name is
imported by exactly one module's `.plf` (measured by scanning `build/G2ME01/*/*.plf` for the
literal `fn_<addr>`). Slot name, address and importing module, all measured:

| address | slot | importing module |
| --- | --- | --- |
| 0x8021F9E4 | `gLoader_Shredder` | `Shredder.plf` |
| 0x8021FA18 | `gLoader_FrontEndDataNetwork` | `ScriptFrontEndDataNetwork.plf` |
| 0x8021FA4C | `gLoader_StoneToad` | `StoneToad.plf` |
| 0x80227AF8 | `gLoader_Rezbit` | `Rezbit.plf` |
| 0x80229FF0 | `gLoader_StreamedMovie` | `ScriptStreamedMovie.plf` |
| 0x8022A024 | `gLoader_IngSpiderBallGuardian` | `IngSpiderballGuardian.plf` |
| 0x8022EBC8 | `gLoader_EmperorIngStage3` | `EmperorIngStage3.plf` |
| 0x8022EBFC | `gLoader_DestructableBarrier` | `DestructibleBarrier.plf` |
| 0x8022EC30 | `gLoader_SwampBossStage2` | `SwampBossStage2.plf` |
| 0x8022FFC4 | `gLoader_IngBoostBallGuardian` | `IngBoostBallGuardian.plf` |

Each is this exact carve: claim `.text <addr>..<addr+8>`, `.c`, `extern struct S<X>*
gLoader_X`, four files. Ten `NEW:` lines would put ten near-identical hours in the queue for a
family that has now flipped 2 of 2, so I am filing them all and letting the driver decide; a
seeding pass can take the table above instead if it would rather not.

NEW: carve-8021f9e4 | match | MetroidPrime/ScriptLoader/Carve8021F9E4 | Shredder's 8-byte `gLoader_Shredder` setter at 0x8021F9E4..0x8021F9EC, own unclaimed 8-byte `auto_03_8021F9E4_text`, import confirmed; same four-file carve as this one.
NEW: carve-8021fa18 | match | MetroidPrime/ScriptLoader/Carve8021FA18 | ScriptFrontEndDataNetwork's 8-byte `gLoader_FrontEndDataNetwork` setter at 0x8021FA18..0x8021FA20, own unclaimed 8-byte `auto_03_8021FA18_text`, import confirmed.
NEW: carve-8021fa4c | match | MetroidPrime/ScriptLoader/Carve8021FA4C | StoneToad's 8-byte `gLoader_StoneToad` setter at 0x8021FA4C..0x8021FA54, own unclaimed 8-byte `auto_03_8021FA4C_text`, import confirmed.
NEW: carve-80227af8 | match | MetroidPrime/ScriptLoader/Carve80227AF8 | Rezbit's 8-byte `gLoader_Rezbit` setter at 0x80227AF8..0x80227B00, own unclaimed 8-byte `auto_03_80227AF8_text`, import confirmed.
NEW: carve-80229ff0 | match | MetroidPrime/ScriptLoader/Carve80229FF0 | ScriptStreamedMovie's 8-byte `gLoader_StreamedMovie` setter at 0x80229FF0..0x80229FF8, own unclaimed 8-byte `auto_03_80229FF0_text`, import confirmed.
NEW: carve-8022a024 | match | MetroidPrime/ScriptLoader/Carve8022A024 | IngSpiderballGuardian's 8-byte `gLoader_IngSpiderBallGuardian` setter at 0x8022A024..0x8022A02C, own unclaimed 8-byte `auto_03_8022A024_text`, import confirmed.
NEW: carve-8022ebc8 | match | MetroidPrime/ScriptLoader/Carve8022EBC8 | EmperorIngStage3's 8-byte `gLoader_EmperorIngStage3` setter at 0x8022EBC8..0x8022EBD0, own unclaimed 8-byte `auto_03_8022EBC8_text`, import confirmed.
NEW: carve-8022ebfc | match | MetroidPrime/ScriptLoader/Carve8022EBFC | DestructibleBarrier's 8-byte `gLoader_DestructableBarrier` setter at 0x8022EBFC..0x8022EC04, own unclaimed 8-byte `auto_03_8022EBFC_text`, import confirmed.
NEW: carve-8022ec30 | match | MetroidPrime/ScriptLoader/Carve8022EC30 | SwampBossStage2's 8-byte `gLoader_SwampBossStage2` setter at 0x8022EC30..0x8022EC38, own unclaimed 8-byte `auto_03_8022EC30_text`, import confirmed.
NEW: carve-8022ffc4 | match | MetroidPrime/ScriptLoader/Carve8022FFC4 | IngBoostBallGuardian's 8-byte `gLoader_IngBoostBallGuardian` setter at 0x8022FFC4..0x8022FFCC, own unclaimed 8-byte `auto_03_8022FFC4_text`, import confirmed.