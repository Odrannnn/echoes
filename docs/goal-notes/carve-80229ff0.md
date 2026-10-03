# carve-80229ff0 — StreamedMovie's loader setter, out as its own `Matching` unit

`kind: match`, `target: MetroidPrime/ScriptLoader/Carve80229FF0`. Landed: the unit is
`Object(Matching, ...)` in `configure.py` and **`tools/flip_test.sh` PASSes it** — "kept as
Matching". `tools/goal_check.sh build/goal/item.json` → **`PASS carve-80229ff0`**, with
`counts: matched 13586 -> 13587   linked 6634 -> 6635`.

## What it is

`.text 0x80229FF0..0x80229FF8`, 0x8 = 8 bytes, one function, from dtk's
`auto_03_80229FF0_text`:

```
fn_80229FF0    0x80229FF0  0x8    stw     r3, gLoader_StreamedMovie@sda21(r0)
                                     blr
```

The whole shape of the ScriptLoader setter family: store the argument into a loader pointer's
`.sbss` slot and return. The twin the seeder named, `fn_80200E3C`
(`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`, `stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`),
is already `Matching`; `fn_80229EE0` (`Carve80229EE0.c`, IngPuddle's) is the same shape one unit
away and was the closest model for the write-up.

**Who calls it.** Exactly one module imports the DOL function by its retail name: module 67,
`config/G2ME01/config.yml:437-441`, `files/RelProd/ScriptStreamedMovie.rel`,
sha1 `d9b45eae526aa3fb5290599843482512b257aba8`. `grep -rln fn_80229FF0 build/G2ME01/ --include=*.s`
returns two files: that module's listing and dtk's own auto unit. Both call sites are in
`build/G2ME01/ScriptStreamedMovie/asm/auto_00_00000000_text.s`:

* `RELExit` (`.text` 0xB4, 0x24 B) — `li r3, 0x0` (line 68), `bl fn_80229FF0` (line 70). The
  module tears its registration down on the way out, so the argument is `0`.
* `fn_67_F8` (`.text` 0xF8, 0x30 B) — `lis r4, fn_67_128@ha` (93) / `lis r3, lbl_67_bss_4@ha` (94) /
  `addi r0, r4, fn_67_128@l` (96) / `stwu r0, lbl_67_bss_4@l(r3)` (97), `bl fn_80229FF0` (98). r3
  still holds the slot's address, so the argument is `&lbl_67_bss_4`; that symbol is
  `config/G2ME01/rels/ScriptStreamedMovie/symbols.txt:37`,
  `.bss:0x00000004; // type:object size:0x4 data:4byte`.

**The reader fixes the sense of the store.** `LoadStreamedMovie` at 0x80229FC4 is
`lwz r6, gLoader_StreamedMovie@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`
(`build/G2ME01/asm/MetroidPrime/ScriptLoader/StreamedMovie.s:12-17`): it loads the *address* of a
loader slot and dispatches through its first word. So the argument is a loader slot, not a loader.
Same reading `Carve80229EE0.c:40-44` records for IngPuddle.

**The slot is already claimed by a unit of ours.** `gLoader_StreamedMovie` is
`.sbss 0x804195A8`, `size:0x8 data:4byte` (`symbols.txt:20771`), defined at
`StreamedMovie.cpp:16` and claimed by that unit's split (`splits.txt:1903-1905`). So this unit
claims `.text` only and takes the pointer `extern` — a second definition would be a duplicate the
moment both objects link. MWCC does not encode a variable's type in its name, so the store lands on
the same address whatever the type is spelled; `StreamedMovie.cpp`'s `SLoaderSlot` says what the
two words are.

**No host-only block is needed.** Unlike `Carve80229EAC.c`, whose slot `lbl_80419590` is claimed by
no unit of ours (so the flat host link loses it and `link_check.sh --strict` sees a new undefined
symbol), this unit's only reference is defined in `src/`. The port's undefined count cannot move.

## The four files, in address order

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve80229FF0.c` | new: header comment, `extern void* gLoader_StreamedMovie;`, `void fn_80229FF0(void* loader) { gLoader_StreamedMovie = loader; }` |
| `config/G2ME01/splits.txt` | `Carve80229FF0.c: .text start:0x80229FF0 end:0x80229FF8`, between `StreamedMovie.cpp` and `IngSpiderBallGuardian.cpp` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve80229FF0.c")` between the same two neighbours |
| `files.cmake` | `src/MetroidPrime/ScriptLoader/Carve80229FF0.c` after `Carve80229EE8.cpp` (block 1, alphabetical) |

**This carve is the whole-unit case.** `auto_03_80229FF0_text` was exactly
`# 0x80229FF0..0x80229FF8 | size: 0x8` — one function, this setter — so the auto unit disappears
from the build entirely. There is no renamed remainder and no shortened neighbour to check, and no
link-order cycle: the range sits in the 8-byte gap between two already-claimed units
(`StreamedMovie.cpp` ends 0x80229FF0, `IngSpiderBallGuardian.cpp` starts 0x80229FF8), which is the
shape `carve-80229ee0.md` (front case) and `carve-801e5230.md` (middle case) cover.

One-function file, so descending declaration order cannot be got wrong;
`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80229FF0.c` →
`ok: 0 unit(s) checked` (nothing to check).

## Measured

* `./tools/decomp_build.sh` → `All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13587 / 28465
  functions)`; `total_functions` still **28465** after the `splits.txt` edit.
* `sha1sum build/G2ME01/main.dol` → `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
* `build/report.json`, unit `main/MetroidPrime/ScriptLoader/Carve80229FF0`:
  `fuzzy_match_percent 100.0`, `total_functions 1`, `matched_functions 1`,
  `matched_functions_percent 100.0`, `complete_units 1`, and `fn_80229FF0` itself
  `fuzzy_match_percent 100.0`.
* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80229FF0.c` → `kept: 1 / 1  failed: 0`, PASS.
* `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80229FF0.c` → `.text claimed 8 ours 8 retail 8
  fits`, `no extra functions`.
* `python3 tools/check_symbol_names.py` → `checked 609 units; 0 declared names are missing`.
* `python3 tools/check_files_cmake.py` → `every configured DOL object is either in files.cmake or
  excluded with a reason`.
* `./tools/goal_check.sh build/goal/item.json` → **PASS carve-80229ff0**, and `ok  no judge-owned
  path touched`. The gate inside it covers DOL sha1, all 86 RELs, the report diff against
  `build/goal/judge/report.base.json`, module wiring, docs claims and the port probe (988 files,
  0 failures — up one from the 987 in `docs/`, because this file is a new host source).

## Note for the next run: `carve_diff.sh` is not a verdict here

`tools/carve_diff.sh 80229FF0 8 build/G2ME01/src/MetroidPrime/ScriptLoader/Carve80229FF0.o
fn_80229FF0` prints

```
  +0   retail: 80229ff0 stw  r3,-26584(r13)   ours: 00000000 stw  r3,0(0)
differing instructions: 1
NOT byte-exact
```

That is the tool reading an **unlinked** `.o`, where the `sda21` displacement is still zero and
carries a relocation. The already-flip-verified `Carve80229EE0.o` prints the identical
`1 differing / NOT byte-exact` at the same instruction (`-26600(r13)` vs `0(0)`). So for any carve
that stores into a global, `carve_diff.sh` reports this and it means nothing; `flip_test.sh` plus a
held `main.dol` is the verdict. The item's brief asks for the `carve_diff.sh` check and it is
reported here — with the measured reason it cannot answer.

## Nothing blocked

Nothing was left undone and no `NEW:` line is filed: the item's whole range matched and flipped.

Two things this run did *not* do, deliberately, rather than as blockers:

* `src/MetroidPrime/ScriptLoader/StreamedMovie.cpp:6-8` still says the 8-byte setter is
  "deliberately NOT claimed: REL modules import it by its retail name, so it cannot be renamed and
  must stay in dtk's auto unit". That sentence is now half-wrong (the name is preserved; only the
  supplier of the bytes changed), exactly as it was for `IngPuddle.cpp` before
  `Carve80229EE0.c` landed. The correction is recorded in the new file's header rather than in
  `StreamedMovie.cpp`, to keep the diff to what this item needs — an unrelated comment fix in a
  unit this item does not touch is the kind of thing the reviewer rejects the whole change for.
  Every other `*.cpp` in the family (`IngPuddle.cpp:7-10` is already updated; `FlyerSwarm.cpp`,
  `RsfAudio.cpp`, `FlyerSwarmLoaderSet.cpp`, `AtomicBeta.cpp`, `MysteryFlyer.cpp`) may still carry
  that sentence — worth one pass across the family some time.
* `docs/research/rel_loaders.md:162` (the "72 thunks" table) gives `StreamedMovie.cpp` /
  `LoadStreamedMovie` / 0x80229FC4 / 0x804195A8 / `fn_80229FC4`. That last column is the **thunk's
  own** `fn_` symbol, not the setter's — every row in the table pairs the thunk's address with the
  `fn_` name at that address (`fn_80200E10` for the thunk at 0x80200E10, and so on down to
  line 181) — so nothing there needs changing for this carve. Worth knowing if you go looking for
  the setter in that file: it is not listed.