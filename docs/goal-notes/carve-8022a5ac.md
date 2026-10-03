# carve-8022a5ac — `MetroidPrime/ScriptLoader/Carve8022A5AC` (match)

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0 with every check `ok`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13590 -> 13591   linked 6638 -> 6639
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13591 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022A5AC.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022a5ac
```

`tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022A5AC.c` prints `PASS -> kept as Matching`
(`kept: 1 / 1   failed: 0   skipped: 0`), so the DOL reproduced retail **with our object in the
link** — the one rule. `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
dtk reports `87 files OK`. `./tools/probe_sources.sh`: `992 files, 0 failed, 0 errors;
link: LINKED (286 undefined, 0 duplicates)`. `python3 tools/check_docs_claims.py`:
`docs claims agree with the tree`.

`build/report.json`: unit `main/MetroidPrime/ScriptLoader/Carve8022A5AC` is `complete: true`,
`.text` 8 bytes, `matched_functions` **1 / 1** at `fuzzy_match_percent` 100.0, function
`fn_8022A5AC` 8 bytes / 100.0%. `total_functions` still **28465**. `matched` 13590 -> **13591**,
`linked` 6638 -> **6639**.

## What was claimed

`.text` **0x8022A5AC..0x8022A5B4**, 0x8 = 8 bytes, 1 function — `config/G2ME01/symbols.txt:9856`:

```
fn_8022A5AC = .text:0x8022A5AC; // type:function size:0x8 align:4
```

out of dtk's `main/auto_03_8022A5AC_text` (`build/G2ME01/asm/auto_03_8022A5AC_text.s`, range
0x8022A5AC..0x8022AF0C, of which only the first 8 bytes are this function):

```
.fn fn_8022A5AC, global
/* 8022A5AC 002273AC  90 6D 98 50 */  stw r3, gLoader_BacteriaSwarm@sda21(r0)
/* 8022A5B0 002273B0  4E 80 00 20 */  blr
```

## What it is, and how that was measured

**BacteriaSwarm's loader setter** — the byte-shape twin of the matched `fn_80200E3C`
(`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`) and of `fn_8022A570`
(`src/MetroidPrime/ScriptLoader/Carve8022A570.c`), which between them are the whole shape of the
family: store the argument into a loader pointer's `.sbss` slot and return.

Both callers are in module 6's own listing,
`build/G2ME01/BacteriaSwarm/asm/MetroidPrime/ScriptObjects/CBacteriaSwarmRel.s`, and neither calls
it a setter by name, so the argument is read off them (measured there, not recalled):

* `RELExit` at `.text 0x2C` (0x24 B): `li r3, 0` / `bl fn_8022A5AC` — teardown.
* `fn_6_70` at `.text 0x70` (0x30 B): `lis r4, fn_6_A0@ha` / `lis r3, lbl_6_bss_10@ha` /
  `stwu r0, lbl_6_bss_10@l(r3)` / `bl fn_8022A5AC` — the store writes the module's own copy and
  leaves r3 holding its **address**, so the argument is a pointer *to* a loader pointer, and that
  copy is `lbl_6_bss_10` at `.bss:0x00000010`, `size:0x4 data:4byte`
  (`config/G2ME01/rels/BacteriaSwarm/symbols.txt:158`).

That is consistent with the reader: `src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` (a `Matching`
unit whose `.text` ends exactly at 0x8022A5AC) does `lwz r6, gLoader_BacteriaSwarm@sda21(r0)` /
`lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`. `BacteriaSwarm.cpp` claims **and defines**
`.sbss 0x804195D0..0x804195D8` (`gLoader_BacteriaSwarm`, `size:0x8`) and reads it, so this unit
takes it `extern` and does **not** claim it twice — the same arrangement `Carve8022A570.c` uses
with `gLoader_EmperorIngStage2Tentacle`. That file's header says the setter was left unclaimed
because REL modules import it by retail name; reproduced verbatim here in plain C, the unmangled
`fn_8022A5AC` lands in the DOL link, which is what `bl fn_8022A5AC` resolves against. Measured in
the module's own import table: `build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf` (binary) matches
the plain string `fn_8022A5AC`, not a mangled `SetLoader_...` form, so no `symbols.txt` rename is
needed and no module byte moves.

## The four files, all in one change

* `src/MetroidPrime/ScriptLoader/Carve8022A5AC.c` (new, 87 lines) — header in the style of
  `src/Dolphin/Carve8038A7DC.c` / `Carve8022A570.c`; body is
  `void fn_8022A5AC(struct SLoaderSlot* loader) { gLoader_BacteriaSwarm = loader; }`.
* `config/G2ME01/splits.txt:1939` — `.text start:0x8022A5AC end:0x8022A5B4`, placed after
  `BacteriaSwarm.cpp`'s range and before `MetroidPrime/Player/CPlayerVisor.cpp`. Claim is exactly
  the one range; `total_functions` still 28465.
* `configure.py:963` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A5AC.c")`, one line,
  in address order between `BacteriaSwarm.cpp` and `MetareeSwarm.cpp`.
* `files.cmake:1755` — `src/MetroidPrime/ScriptLoader/Carve8022A5AC.c`, same position in the port
  source list (the probe compiles it: `build/probe-logs/status.txt:645 ... 0`).

No `PortLinkStubs.cpp` entry existed for `fn_8022A5AC` (`grep -rn fn_8022A5AC src/` finds only
`CBacteriaSwarmRel.cpp`'s extern-C declaration, and `gate.sh`'s `port link dups` step is clean at
0 duplicates), so nothing had to be deleted there.

## Byte check — and a caveat worth recording

`./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022A5AC.c`:
`.text claimed 8 ours 8 retail 8 fits` / `no extra functions`.
`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve8022A5AC.c`: `0 unit(s)
checked, none emits its functions out of retail order` (a one-function file).

**`tools/carve_diff.sh` prints `NOT byte-exact` for this carve, and that verdict is wrong for any
`@sda21` store.** It disassembles `build/G2ME01/obj/.../Carve8022A5AC.o`, where the relocation has
not been applied:

```
retail: 2 instructions, 8 bytes
ours  : 2 instructions, 8 bytes
  +0   retail: 8022a5ac stw r3,-26544(r13)  ours: 00000000 stw r3,0(0)
differing instructions: 1     NOT byte-exact
```

`powerpc-eabi-objdump -r` on the same object shows the relocation is present
(`00000000 R_PPC_EMB_SDA21  gLoader_BacteriaSwarm`), and the already-`Matching` sibling
`Carve8022A570.o` produces the identical false verdict on both of its stores:

```
./tools/carve_diff.sh 0x8022A570 0x10 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8022A570.o fn_8022A570
  +0   retail: 8022a570 stw r3,-26560(r13)  ours: 00000000 stw r3,0(0)
  +2   retail: 8022a578 stw r3,-26552(r13)  ours: 00000008 stw r3,0(0)
  differing instructions: 2     NOT byte-exact
```

So for this family `flip_test.sh` (DOL hash) is the only byte evidence, exactly as the carve-vein
section says. A future lane should not read `carve_diff`'s `NOT byte-exact` on an `sda21` store as
a defect; compare against the object's relocations, or against a `Matching` sibling, first.

## Nothing else

No `NEW:`, `WALL:` or `STALE:` line. Nothing is blocked and nothing needed to be left out.

Two observations that are **not** part of this item and that I did not act on:

* `probe_sources.sh` reports `286 undefined` where `docs/HANDOFF.md`'s state block says `287`.
  This carve cannot be the cause: `build/probe-logs/files.txt` does not contain
  `src/MetroidPrime/ScriptObjects/CBacteriaSwarmRel.cpp` (it is deliberately excluded from the flat
  host link — that file's own header says so), so no caller in the sweep link was left undefined by
  `fn_8022A5AC` becoming defined. `check_docs_claims.py` agrees with the tree either way and the
  driver re-derives the state block, so I left both files alone.
* `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: those edits
  are `gate.sh`'s own (`sync_state_block.py` / docs-claims) rewrites of derived counts, not mine.