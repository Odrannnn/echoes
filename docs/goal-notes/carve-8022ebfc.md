# carve-8022ebfc — `MetroidPrime/ScriptLoader/Carve8022EBFC` is a `Matching` carve

## What I did

Carved the one unsourced function in dtk's unclaimed `main/auto_03_8022EBFC_text` range as its
own `Matching` unit, using the twin recipe. Four files, each entry in address order:

- `src/MetroidPrime/ScriptLoader/Carve8022EBFC.c` (new, 95 lines) —
  `void fn_8022EBFC(struct SLoaderSlot* loader) { gLoader_DestructableBarrier = loader; }`,
  plain C so the `fn_` name does not mangle.
- `config/G2ME01/splits.txt:1979-1980` — `MetroidPrime/ScriptLoader/Carve8022EBFC.c` /
  `.text start:0x8022EBFC end:0x8022EC04`, between `DestructableBarrier.cpp` (ends 0x8022EBFC)
  and `SwampBossStage2.cpp` (starts 0x8022EC04).
- `configure.py:973` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EBFC.c"),` on one
  line, between the same two units.
- `files.cmake:986-991` — the path plus a four-line comment in the style the sibling loader-setter
  carves use, between `Carve8022EB54.c` and `Carve80232834.c`.

The claim is exactly `.text 0x8022EBFC..0x8022EC04` and nothing else. The `.sbss` slot
`gLoader_DestructableBarrier` (0x804195F8) stays claimed by `DestructableBarrier.cpp` and is
taken as `extern` here, so it is defined in exactly one unit. No `PortLinkStubs.cpp` duplicate
(`grep -rn fn_8022EBFC src/MetroidPrime/PortLinkStubs.cpp` returns nothing), and the port's
undefined count did not move (nothing in the file is called).

## What I measured

- Retail bytes from the pristine disc, not our build:
  `python3 tools/dol_read.py 0x8022EBFC 0x8 orig/G2ME01/sys/main.dol` reads
  `90 6D 98 78 4E 80 00 20`. The listing is
  `build/G2ME01/asm/auto_03_8022EBFC_text.s:8-12` — `stw r3,
  gLoader_DestructableBarrier@sda21(r0)` / `blr`, 8 bytes.
  `config/G2ME01/symbols.txt:9939` gives `fn_8022EBFC = .text:0x8022EBFC; size:0x8 align:4`.
- Callers, both in module 13 (`config/G2ME01/config.yml:105`, `files/RelProd/
  DestructibleBarrier.rel`), read off `build/G2ME01/DestructibleBarrier/asm/
  auto_00_00000000_text.s`: `RELExit` (module `.text` 0x2C, 0x24 B) does `li r3, 0x0`
  (line 27) and calls on line 29; `fn_13_70` (module `.text` 0x70, 0x30 B, the loader
  registration; lines 52-57) does `lis r4, fn_13_A0@ha` / `lis r3, lbl_13_bss_0@ha` /
  `addi r0, r4, fn_13_A0@l` / `stwu r0, lbl_13_bss_0@l(r3)` first, so `r3` is `&lbl_13_bss_0`
  when it calls. `stwu` writes the cell and leaves its address in `r3`.
- **The record is 4 bytes**: `build/G2ME01/DestructibleBarrier/asm/auto_05_00000000_bss.s:8-11`
  gives `lbl_13_bss_0` `.bss:0x0 size:0x4` (one word, `&fn_13_A0` —
  `config/G2ME01/rels/DestructibleBarrier/symbols.txt:5`, `fn_13_A0 = .text:0x000000A0;
  size:0x8A0`), and it is the **only** object in the module's `.bss`, so this record holds no
  CodeWarrior pmf. That is the `.bss`-size check the module-head recipe prescribes before
  copying a sibling spelling, and it is why no second argument is written here.
- The type comes from the reader, `src/MetroidPrime/ScriptLoader/DestructableBarrier.cpp:19`,
  a `Matching` unit: `(*gLoader_DestructableBarrier.value)(mgr, input, info)` reads word 0 as a
  `FScriptLoader` (`include/MetroidPrime/ScriptLoader.hpp:23`), so the DOL slot holds a
  *pointer to* a loader cell and the argument is a record address, not a loader.
- The name is retail's, so the unit is `.c`: `strings build/G2ME01/DestructibleBarrier/
  DestructibleBarrier.plf | grep 8022EB` returns `fn_8022EBFC`, so module 13 imports that exact
  name and it cannot be renamed or aliased.
- `gLoader_DestructableBarrier` is `.sbss 0x804195F8`, `symbols.txt:20781`,
  `size:0x8 data:4byte`, claimed and defined by `DestructableBarrier.cpp` (its
  `splits.txt:1975-1977`, definition at its line 16).

## Results (all measured, not recalled)

```
./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve8022EBFC
  All: 37.69% fuzzy, 31.12% matched, 13.99% linked (1019 / 2400 files)
    Code: 2034020 / 6535816 bytes (13593 / 28465 functions)
  main/MetroidPrime/ScriptLoader/Carve8022EBFC: 100.00% fuzzy, 100.00% matched (1 / 1 functions)

./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022EBFC.c
  PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0

./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022EBFC.c
  .text claimed 8  ours 8  retail 8  fits
  no extra functions: our object defines only what the retail unit object does

python3 tools/check_decl_order.py --unit src/MetroidPrime/ScriptLoader/Carve8022EBFC.c
  ok: 0 unit(s) checked, none emits its functions out of retail order   (one function)

./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13592 -> 13593   linked 6640 -> 6641
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13593 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EBFC.c: PASS, Object(Matching) in configure.py
  goal_check: PASS carve-8022ebfc
```

`total_functions` is still **28465** after the `splits.txt` edit, and `sha1sum
build/G2ME01/main.dol` is still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. No `asm` added,
nothing deleted, no initialisation dropped.

## Notes for the next run

- `tools/carve_diff.sh 0x8022EBFC 0x8 build/G2ME01/obj/MetroidPrime/ScriptLoader/
  Carve8022EBFC.o fn_8022EBFC` says `differing instructions: 1 / NOT byte-exact`, and that is
  the unlinked relocation rendering only, as `carve-80227af8` already recorded: ours shows
  `stw r3,0(0)` against retail's `stw r3,-26504(r13)`. `objdump -d -r` on the object shows the
  identical byte words `90 60 00 00` / `4e 80 00 20` with `R_PPC_EMB_SDA21
  gLoader_DestructableBarrier` next to the store, and our own dtk dump
  (`build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve8022EBFC.s:10-11`) reproduces retail's
  words `90 6D 98 78` / `4E 80 00 20` with the same `gLoader_DestructableBarrier@sda21(r0)`
  symbol. `flip_test.sh`, which links, is the acceptance test. Ignore `carve_diff.sh` for a
  one-instruction store. Note the object path is `build/G2ME01/obj/...`, **not**
  `build/G2ME01/obj/main/...` — a wrong path reads as "0 instructions" and looks like a total
  miss.
- `DestructableBarrier.cpp:6-8` still says this setter is "deliberately NOT claimed: REL
  modules import it by its retail name, so it cannot be renamed and must stay in dtk's auto
  unit". That is half the requirement, as `carve-80227af8` recorded for `Rezbit.cpp`: a carve
  must preserve the **name**, and reproducing the `fn_<addr>` symbol verbatim in a `.c` file
  does. I left the neighbour's header alone rather than widening this diff; there are 40+ such
  stale sentences across `src/MetroidPrime/ScriptLoader/` and a single sweep is the right place.
- The cycle risk in "The carve vein" (a carve starting exactly where a claimed unit's `.text`
  ends) did **not** bite: `DestructableBarrier.cpp` ends at 0x8022EBFC and `SwampBossStage2.cpp`
  starts at 0x8022EC04, `dtk dol split` accepted it and the DOL sha1 held. That is the fourth
  carve here with this exact neighbour shape, so the risk is narrower than the note reads.
- The `.bss`-size check paid for itself again: this record is 4 bytes like `Rezbit`'s, so the
  family's four-byte registration spelling transferred with no second argument. Where the
  record *is* 0x1C (`fn_80200E3C`/SpacePirate, two pmfs) the parameter type must be spelled
  out, so read the `.bss` object before copying a sibling.