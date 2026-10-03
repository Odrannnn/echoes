# carve-80218d8c — `match` on `MetroidPrime/ScriptLoader/Carve80218D8C`

**Landed: one 8-byte loader setter, `fn_80218D8C` at 0x80218D8C..0x80218D94, as a new `Matching`
unit. `flip_test.sh` PASS and kept the unit; `tools/goal_check.sh` PASS.**

## What I did

Carved `fn_80218D8C` out of dtk's `main/auto_03_80218D8C_text` into
`src/MetroidPrime/ScriptLoader/Carve80218D8C.c`, the four-file carve, each entry in address
order:

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt:1725-1726` | new `MetroidPrime/ScriptLoader/Carve80218D8C.c` block, `.text 0x80218D8C..0x80218D94`, between the `WispTentacle.cpp` and `SpankWeed.cpp` blocks |
| `configure.py:886-891` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218D8C.c")`, **one line**, between `WispTentacle.cpp` and `SpankWeed.cpp`, with a 6-line comment naming the unit |
| `files.cmake:906-909` | the source, with a three-line comment, next to `Carve80218B68.c` |
| `src/MetroidPrime/ScriptLoader/Carve80218D8C.c` | the unit (87 lines) |

Plus one stale-claim fix, the same correction `Carve80218A6C.c` made to `MediumIng.cpp`:
`src/MetroidPrime/ScriptLoader/WispTentacle.cpp:6-9` said the setter was "deliberately NOT
claimed … must stay in dtk's auto unit". It now names the carve. The carve preserves the name,
which is the part that sentence was really about.

## What I measured

The claim is exactly the gap and nothing else. `symbols.txt:9504` is
`fn_80218D8C = .text:0x80218D8C; // type:function size:0x8 align:4`; the nearest claims below and
above are `WispTentacle.cpp` `.text 0x80218D60..0x80218D8C` (`splits.txt:1722`) and
`SpankWeed.cpp` `.text 0x80218D94..0x80218DC0` (`splits.txt:1729`), so 8 bytes is the whole gap
and the function fills it. **No unclaimed gap on either side, and no link-order cycle** — the
`RUNNING_THE_DECOMP.md` carve-vein hazard is proximity to an existing unit *boundary*, and this
range starts exactly where `WispTentacle.cpp` ends, which is the shape `Carve80218B68.c` already
took 0xC4 bytes earlier without a cycle.

The body, from `build/G2ME01/asm/auto_03_80218D8C_text.s`:

```
fn_80218D8C    0x80218D8C  0x8    stw r3, gLoader_WispTentacle@sda21(r0)
                               blr
```

The slot `gLoader_WispTentacle` is `.sbss 0x80419458`, `size:0x8 data:4byte`
(`symbols.txt:20724`), claimed and defined by `WispTentacle.cpp` (`:13-18` after my comment
edit), so this unit claims `.text` only and takes the pointer as `extern`. No
`PortLinkStubs.cpp` duplicate exists: `grep -n "80218D" src/MetroidPrime/PortLinkStubs.cpp`
returns nothing.

**The name is the module's import, which is why this is a `.c`.** The plain `fn_80218D8C` is in
module 86's `WispTentacle.preplf` symbol table (alongside the mangled retail names), and
`auto_00_000003C8_text.o` carries `R_PPC_REL24 fn_80218D8C` on both call sites. There is no
`CWispTentacleRel.cpp` in the tree, unlike `CGrenchlerRel.cpp` / `CSandBossRel.cpp`, so there is
no second declaration to reconcile.

**The argument, read off module 86** (`config/G2ME01/config.yml:548-551`, sha1
`6049b926db0c1f6a6072a234e886075714f9afd0`, matching
`sha1sum orig/G2ME01/files/RelProd/WispTentacle.rel`), from
`build/G2ME01/WispTentacle/asm/auto_00_000003C8_text.s`:

- `RELExit` (`.text:0x2C`, 0x24 bytes): `li r3, 0x0`, then `bl fn_80218D8C` — a null, the module
  tearing the loader down on the way out.
- `fn_86_438` (`.text:0x70`, 0x30 bytes, reached from `RELMain` at `.text:0x80`): `lis r4,
  fn_86_468@ha`, `lis r3, lbl_86_bss_0@ha`, `addi r0, r4, fn_86_468@l`, `stwu r0,
  lbl_86_bss_0@l(r3)`, then `bl fn_80218D8C` with `r3` still at `lbl_86_bss_0`.

So the argument is that record's address and **the record is 4 bytes**: one `FScriptLoader`.
`config/G2ME01/rels/WispTentacle/symbols.txt:167` is `lbl_86_bss_0 = .bss:0x00000000;
type:object size:0x4 data:4byte`, and `build/G2ME01/WispTentacle/asm/auto_05_00000000_bss.s` is
that module's only `.bss` object, the same 4 bytes. This is the same record shape as module 55's
behind `fn_802189D0` (`Carve802189D0.c`); it is a larger struct only in modules that also hand
over pointers-to-member-function — module 72's `lbl_72_bss_24` is 0x1C bytes behind
`fn_80200E3C`, and module 40's `lbl_40_bss_10` is 0x10 behind `fn_80218B68`. Hence
`struct SWispTentacleLoader { unsigned int loader; };`, not a four-word struct.

## Verification

Everything below is what the tools printed, not what I expected.

```
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80218D8C.c
ok: 0 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_files_cmake.py
files.cmake: 970 sources; configure.py declares 821 DOL objects; 282 documented exclusions
every configured DOL object is either in files.cmake or excluded with a reason
$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218D8C.c
   .text      claimed      8   ours      8   retail      8   fits
   no extra functions: our object defines only what the retail unit object does
$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218D8C.c
  PASS  -> kept as Matching        kept: 1 / 1   failed: 0   skipped: 0
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ python3 tools/check_symbol_names.py
checked 609 units; 0 declared names are missing from their object
```

`total_functions` is still **28465** after the `splits.txt` edit. The carve's object replaces
dtk's `auto_03_80218D8C_text`, it does not add a function.

`build/report.json` before and after, for the unit itself:

```
before  main/auto_03_80218D8C_text          total_functions 1, matched 0, auto_generated True
after   main/MetroidPrime/ScriptLoader/Carve80218D8C
                                            fuzzy 100.0, matched 1 / 1, complete_units 1,
                                            fn_80218D8C size 8 fuzzy_match_percent 100.0
```

The judge, run in this worktree against this lane's own baseline:

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item carve-80218d8c (match) target=MetroidPrime/ScriptLoader/Carve80218D8C
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13575 -> 13576   linked 6623 -> 6624
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13576 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80218D8C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80218d8c
```

**+1 matched function and +1 linked unit**, no function lost anywhere, DOL sha1 held, all 86 RELs
byte-equal with their config.yml sha1s.

## Note on `tools/carve_diff.sh` for this item

`carve_diff.sh` is useless for a `@sda21` store and I should have known that before running it.
It compares the *retail* side out of the **linked** `build/G2ME01/main.elf` and the *ours* side
out of the relocatable `.o`, so the retail instruction reads `stw r3,-26920(r13)` and ours reads
`stw r3,0(0)` with an `R_PPC_ADDR16 sda21` relocation — every `@sda21` carve in this family will
report `NOT byte-exact` for that reason alone. Pointing it at `build/G2ME01/obj/...` instead
does not help either: that side is a retail split object with no `.text` disassembled at the
addresses it expects. `flip_test.sh` is the real check and it PASSes; the instructions it moves
are confirmed by the DOL sha1 not moving and by the judge.

## Files touched

- `src/MetroidPrime/ScriptLoader/Carve80218D8C.c` — new, 87 lines
- `src/MetroidPrime/ScriptLoader/WispTentacle.cpp:6-9` — stale-claim correction
- `config/G2ME01/splits.txt:1725-1726` — the range
- `configure.py:886-891` — the `Object` plus its comment
- `files.cmake:906-909` — the source and its comment

Nothing outside `src/`, `configure.py`, `config/G2ME01/splits.txt` and `files.cmake` was edited;
no `tools/`, no `docs/`, no `build/goal/` other than this notes file. Not committed.
## Lane 8: passed, then failed on the moved tip (2026-10-03 06:13:36Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 0fea9616c183; re-do it against the current tip.

---

## Lane 8, second attempt, re-done on tip `0fea9616` — `match`, PASS (2026-10-03)

Same four-file carve as above, re-measured and re-landed on this worktree's tip. `flip_test.sh`
PASS, kept; `./tools/goal_check.sh build/goal/item.json` **PASS** (`matched 13585 -> 13586`,
`linked 6633 -> 6634`, `All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13586 / 28465)`,
`main.dol` sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `GATE PASS 0fea9616+7 changed`).
Not committed.

### The previous failure was not the change — read this before redoing anything

`build/goal/check-gate.log` from the 08:13 judged run (still on disk in `wt-mp2-goal-L8`) has
every step `ok` **except**:

```
gs offsets      error: probe sections are empty (.data 0, .sdata 0) - it did not emit g_probe
GATE FAIL: gs-offsets
```

`check-counts.log` in the same directory reads `matched 13585 -> 13586   linked 6633 -> 6634`
and `check-flip.log` reads `PASS -> kept as Matching`, so the carve itself was fine both times.
`python3 tools/probe_gs_offsets.py` on the **clean** tip 0fea9616 prints `ok: 41 offsets` in
0.6 s, and the gate step prints `gs offsets ok` on every later run. So the item was failed by a
**transient `probe_gs_offsets.py` failure** in the rebuild the driver did right after carrying
the patch, not by anything the diff did. That step compiles `tools/size_probe_gs.cpp` and
reads the object back out; a build tree caught mid-rebuild makes the probe object come out
empty and it reports this. **A `gs-offsets` failure on the first gate run after a carry is not
evidence the change broke anything — re-run the judge before rewriting the carve.**

### `build/report.json` in the worktree was a stale derived input

Left by the earlier attempt, it still described `main/MetroidPrime/ScriptLoader/Carve80218D8C`
as `matched 1 / 1, complete_units 1, auto_generated false` **while the source file was absent
from the tree**. Reading "before" from it says the item is done; it is not
(`build/` is gitignored and survives `git reset --hard`). The honest before is the judge's
baseline, `build/goal/judge/report.base.json`, recorded by `run_goal.sh` at HEAD `0fea9616`
(`cat build/goal/judge/HEAD` confirms it):

```
baseline totals   matched 13585 / total 28465
main/auto_03_80218D8C_text   total_functions 1  matched_functions 0  auto_generated true
```

That is also the gap check: `config/G2ME01/symbols.txt:9504` is
`fn_80218D8C = .text:0x80218D8C; // type:function size:0x8 align:4`, and the neighbours in
`splits.txt` were still `WispTentacle.cpp` `0x80218D60..0x80218D8C` and `SpankWeed.cpp`
`0x80218D94..0x80218DC0`, so the 8 bytes were unclaimed and the function filled them exactly.
**Not `STALE:`** — the baseline has it at 0 matched.

### Where each entry goes now, and why not where the first attempt put it

`0fea9616`'s own commit message says it landed *"Carried onto 41dba0f6efbd: conflicts with what
another lane landed were resolved by tools/resolve_carry_conflicts.py, after review:
files.cmake"*, so `files.cmake` is the contested file in this neighbourhood. The first attempt
put its `files.cmake` entry beside `Carve80218B68.c` (old line 909); on the current tip the
sibling loader-setter carves are one address-ordered block at `files.cmake:930-949`
(`Carve80218C30`, `Carve80218CF0`, `Carve80218D24`, `Carve80218D58`, `Carve80218DF4`), so this
one goes **between `Carve80218D58.c` and `Carve80218DF4.c`** (new `files.cmake:946-949`) — the
position that needs no conflict resolution against anything another lane is carving here.
`configure.py` and `splits.txt` positions are unchanged from the first attempt (the `Object`
between `WispTentacle.cpp` and `SpankWeed.cpp`; the block between the same two `splits.txt`
blocks) because neither neighbour moved.

### Measurements re-taken on this tree (nothing below is recalled)

- body, `build/G2ME01/asm/auto_03_80218D8C_text.s`: `stw r3, gLoader_WispTentacle@sda21(r0)`
  (`90 6D 96 D8`) / `blr` (`4E 80 00 20`).
- **The `@sda21` arithmetic in `Carve80218DF4.c:13` is written backwards.** It says
  "0x8041FD80 - 0x96E8 = 0x80419468", which is not true. The displacement is
  `slot - _SDA_BASE_` truncated to 16 bits: for `gLoader_WispTentacle`,
  `0x8041FD80 - 0x80419458 = 0x6928`, so the field is `0x10000 - 0x6928 = 0x96D8`. Both encodings
  the sibling files quote are right, only the sentence describing them is wrong; this file
  states it the right way round. Left `Carve80218DF4.c` alone — out of scope for this item.
- module 86 (`config/G2ME01/config.yml:548-551`, sha1
  `6049b926db0c1f6a6072a234e886075714f9afd0`, equal to
  `sha1sum orig/G2ME01/files/RelProd/WispTentacle.rel`):
  `RELExit` at `.text:0x2C` (0x3F4..0x418) does `li r3, 0x0` then `bl fn_80218D8C`; `fn_86_438`
  at `.text:0x70` (0x438..0x468, reached from `RELMain` at `.text:0x50`) does
  `lis r4, fn_86_468@ha` / `lis r3, lbl_86_bss_0@ha` / `addi r0, r4, fn_86_468@l` /
  `stwu r0, lbl_86_bss_0@l(r3)` then `bl fn_80218D8C` with `r3` still at `lbl_86_bss_0`.
  `config/G2ME01/rels/WispTentacle/symbols.txt:167` gives
  `lbl_86_bss_0 = .bss:0x00000000; size:0x4 data:4byte` and
  `build/G2ME01/WispTentacle/asm/auto_05_00000000_bss.s` is that module's only `.bss` object —
  4 bytes, one `FScriptLoader`. `fn_86_468`, the word stored, takes r3/r4/r5 and reads `0x8(r4)`,
  the `(CStateManager&, CInputStream&, CEntityInfo&)` signature.
- `fn_80218D8C` is in `build/G2ME01/WispTentacle/WispTentacle.preplf` and
  `auto_00_000003C8_text.o` carries `R_PPC_REL24 fn_80218D8C` on both call sites; there is no
  `src/MetroidPrime/ScriptObjects/CWispTentacleRel.cpp` and
  `grep -n "80218D" src/MetroidPrime/PortLinkStubs.cpp` returns nothing.
- `gLoader_WispTentacle` is `.sbss 0x80419458..0x80419460`
  (`config/G2ME01/symbols.txt:20724`, `size:0x8 data:4byte`), claimed and defined by
  `WispTentacle.cpp`, so this unit claims `.text` only and takes the pointer as `extern`.

### Verification (what the tools printed)

```
$ python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80218D8C.c
ok: 0 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_files_cmake.py
files.cmake: 980 sources; configure.py declares 821 DOL objects; 282 documented exclusions
every configured DOL object is either in files.cmake or excluded with a reason
$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218D8C.c
   .text      claimed      8   ours      8   retail      8   fits
   no extra functions: our object defines only what the retail unit object does
$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218D8C.c
  PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh ...   (per-function diff: SPLIT main/auto_03_80218D8C_text: 1 function(s)
        accounted for across 1 new unit(s) in main (exact count match - a split, not a loss))
  ok    counts: matched 13585 -> 13586   linked 6633 -> 6634
  ok    check_symbol_names.py        ok  609 units; 0 declared names are missing
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13586 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80218D8C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80218d8c
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/probe_sources.sh
probe: 987 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)
```

`build/report.json` after: `main/auto_03_80218D8C_text` is gone and
`main/MetroidPrime/ScriptLoader/Carve80218D8C` is `fuzzy 100.0, matched 1 / 1,
complete_units 1, fn_80218D8C size 8 fuzzy_match_percent 100.0`.
`main/MetroidPrime/ScriptLoader/WispTentacle` is untouched at 44 bytes / 1 / 1 — the shared
unit did not move. `total_functions` is still **28465**.

### Files touched this attempt

- `src/MetroidPrime/ScriptLoader/Carve80218D8C.c` — new, 96 lines
- `src/MetroidPrime/ScriptLoader/WispTentacle.cpp:6-10` — the stale "deliberately NOT claimed"
  comment, corrected the same way `0fea9616` corrected `DarkTrooper.cpp:4-10`
- `config/G2ME01/splits.txt:1752-1753` — the range
- `configure.py:906-913` — the one-line `Object` plus its 5-line comment
- `files.cmake:946-949` — the source and its 3-line comment, in the address-ordered carve block

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` also show as modified after running
`goal_check.sh`, but only the derived counts `986 -> 987` files, written by
`gate.sh` under `MP_GATE_DOCS_WRITE=1`; I did not edit them and the driver discards edits to
both before judging. No `tools/`, no `build/goal/` in this worktree other than the logs
`goal_check.sh` writes itself. Not committed.
