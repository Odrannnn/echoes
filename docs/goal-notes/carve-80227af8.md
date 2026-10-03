# carve-80227af8 — `MetroidPrime/ScriptLoader/Carve80227AF8` is a `Matching` carve

## What I did

Carved the one unsourced function in dtk's unclaimed `main/auto_03_80227AF8_text` range as its
own `Matching` unit, using the proven twin recipe. Four files, each entry in address order:

- `src/MetroidPrime/ScriptLoader/Carve80227AF8.c` (new) — `void fn_80227AF8(struct SLoaderSlot*
  loader) { gLoader_Rezbit = loader; }`, plain C so the `fn_` name does not mangle.
- `config/G2ME01/splits.txt:1864-1865` — `MetroidPrime/ScriptLoader/Carve80227AF8.c` /
  `.text start:0x80227AF8 end:0x80227B00`, between `Rezbit.cpp` (ends 0x80227AF8) and
  `RsfAudio.cpp` (starts 0x80227B00).
- `configure.py:935` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve80227AF8.c"),` on one
  line, between the same two units.
- `files.cmake:958` — `src/MetroidPrime/ScriptLoader/Carve80227AF8.c`.

Claim is exactly `.text 0x80227AF8..0x80227B00` and nothing else. The `.sbss` slot
`gLoader_Rezbit` (0x80419580) stays claimed by `Rezbit.cpp` and is taken as `extern` here.

## What I measured

- Retail bytes: `build/G2ME01/asm/auto_03_80227AF8_text.s:8-12` —
  `stw r3, gLoader_Rezbit@sda21(r0)` / `blr`, 8 bytes, `fn_80227AF8`
  (`symbols.txt:9771`, `size:0x8`); `symbols.txt:9772` gives `LoadRsfAudio...` at 0x80227B00.
- Callers, both in module 53 (`config/G2ME01/config.yml:356`, `files/RelProd/Rezbit.rel`), read
  off `build/G2ME01/Rezbit/asm/auto_00_00000000_text.s`: `RELExit` (module .text 0xF4, 0x24 B)
  passes `li r3, 0` (call on line 131); `fn_53_138` (module .text 0x138, 0x30 B, the loader
  registration, call on line 159) does `lis r4, fn_53_168@ha` / `lis r3, lbl_53_bss_0@ha` /
  `addi r0,r4,fn_53_168@l` / `stwu r0, lbl_53_bss_0@l(r3)` first, so `r3` is `&lbl_53_bss_0`.
- **The record is 4 bytes, not 8**: `build/G2ME01/Rezbit/asm/auto_05_00000000_bss.s:7-10` gives
  `lbl_53_bss_0` `.bss:0x0 size:0x4` (one word, `&fn_53_168`) and `:12-16` gives `lbl_53_bss_4`
  `size:0xC` — so this record has no CodeWarrior pmf in it and the family's four-byte
  registration spelling transfers unchanged. That is the `.bss`-size check
  `docs/RUNNING_THE_DECOMP.md` prescribes before copying a sibling module head's spelling, and it
  is why no second argument is written here.
- `gLoader_Rezbit` is `.sbss 0x80419580`, `symbols.txt:20766`, `size:0x8 data:4byte`.

## Results (all measured, not recalled)

```
./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve80227AF8
  All: 37.69% fuzzy, 31.12% matched, 13.99% linked (13586 / 28465 functions)
  main/MetroidPrime/ScriptLoader/Carve80227AF8: 100.00% fuzzy, 100.00% matched (1 / 1 functions)

./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80227AF8.c
  PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0

./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80227AF8.c
  .text claimed 8  ours 8  retail 8  fits
  no extra functions: our object defines only what the retail unit object does

python3 tools/check_symbol_names.py    -> checked 609 units; 0 declared names are missing
./tools/probe_sources.sh              -> probe: 987 files, 0 failed, 0 errors; link: LINKED
                                         (286 undefined, 0 duplicates)

./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13585 -> 13586   linked 6633 -> 6634
  ok    flip_test MetroidPrime/ScriptLoader/Carve80227AF8.c: PASS
  goal_check: PASS carve-80227af8
```

`total_functions` is still **28465** after the `splits.txt` edit, and `Code:` in the progress
block reads `13586 / 28465`.

Note on `tools/carve_diff.sh`: it reports `differing instructions: 1` for the `stw`, which is the
unlinked relocation rendering only — ours shows `stw r3,0(0)` where retail shows
`stw r3,-26624(r13)`. Our object's own dtk dump
(`build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve80227AF8.s`) carries the identical byte words
`002248F8` / `4E 80 00 20` and the same `gLoader_Rezbit@sda21(r0)` symbol, and `flip_test.sh`
(which links) is the acceptance test. So that tool's verdict is not a byte result on an unlinked
object; ignore it for one-instruction stores.

## Notes for the next run

- `Rezbit.cpp:6-8` still says this setter is "deliberately NOT claimed ... must stay in dtk's auto
  unit". That is half the requirement, as `Carve8022756C.c` and `Carve8022A570.c` already record
  for their neighbours: a carve must preserve the **name**, and reproducing the `fn_<addr>` symbol
  verbatim in a `.c` file does. I followed the established precedent and left the neighbour's
  header alone rather than widening this diff; there are 40+ such stale sentences left across
  `src/MetroidPrime/ScriptLoader/` and they all read the same way, so a single sweep correcting
  them is the right place, not a carve at a time.
- The cycle risk in "The carve vein" (a carve starting exactly where a claimed unit's `.text`
  ends) did **not** bite here: `Rezbit.cpp` ends at 0x80227AF8 and `RsfAudio.cpp` starts at
  0x80227B00, `dtk dol split` accepted it and the DOL sha1 held. That is the third carve here
  with this exact neighbour shape, so the risk is narrower than the note reads — it bit on
  `auto_03_803029D8_text`, where the preceding claim ended *inside* a dtk auto range.
- No `asm` added, nothing deleted, no initialisation dropped.