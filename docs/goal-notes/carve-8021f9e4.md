# carve-8021f9e4

`kind: match`, target `MetroidPrime/ScriptLoader/Carve8021F9E4`. **Done and flipped.**

## What it was

`fn_8021F9E4` at `.text 0x8021F9E4`, 8 bytes, sitting in dtk's
`build/G2ME01/asm/auto_03_8021F9E4_text.s`:

```
stw r3, gLoader_Shredder@sda21(r0)
blr
```

A byte-shape twin of the already-matched `fn_80200E3C`
(`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`) - the whole family shape: store the
argument into the loader pointer's `.sbss` slot and return. Only the `@sda21`
displacement differs from the nearest copy, `fn_8021F9B0` (DigitalGuardian's, the 8 bytes
immediately below): `90 6D 97 90` against `90 6D 97 98`, because the two slots are 8 bytes
apart (`0x80419510` / `0x80419518`). The `blr` is `4E 80 00 20` in both.

Both callers are in the Shredder module's listing
(`build/G2ME01/Shredder/asm/auto_00_000000C8_text.s`), so the argument is read off them and
not guessed:

* `RELExit` (0x24 B, `li r3, 0` at 0x104) - the module tears the loader down on the way out.
* `fn_68_138` (0x30 B, reached from `RELMain`) does `lis r4, fn_68_168@ha` , `addi r0, r4,
  fn_68_168@l` , `lis r3, lbl_68_bss_0@ha` , `stwu r0, lbl_68_bss_0@l(r3)` and then
  `bl fn_8021F9E4` at 0x154 with `r3` still on it. So the record is **4 bytes = one
  `FScriptLoader`** (`auto_05_00000000_bss.s` gives `lbl_68_bss_0 size:0x4`, and the module's
  whole `.bss` is 0x4). Shredder loads one entity, so the parameter is the bare function
  pointer - the same shape as Kralee's `fn_80200E70` in `Carve80200E70.c`, whose module
  record is also 4 bytes. There is no `src/MetroidPrime/ScriptObjects/CShredderRel.cpp` to
  read a name from (module 68 is unclaimed), which is why the record is declared locally.

## What I did

The four-file carve, all in address order, no claim spanning an unclaimed gap:

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8021F9E4.c` | new, 75 lines: header comment in the `Carve8021887C.c` / `Carve80200E3C.c` style, `struct SShredderLoader` at file scope, `extern struct SShredderLoader* gLoader_Shredder`, one 8-byte body |
| `configure.py:899` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8021F9E4.c"),` on one line, between `Shredder.cpp` and `FrontEndDataNetwork.cpp` |
| `config/G2ME01/splits.txt:1777-1778` | `.text start:0x8021F9E4 end:0x8021F9EC`, in the same place |
| `files.cmake:875-878` | the source path, with the comment the sibling carve entries carry |

Plus one comment correction, because the claim it made became false:
`src/MetroidPrime/ScriptLoader/Shredder.cpp:6-8` said the 8-byte setter at 0x8021F9E4 is
"deliberately NOT claimed ... must stay in dtk's auto unit". It now names the new unit and
says the name is not that file's to change. Same edit the `Carve80213CB8` and
`Carve80200E3C` lanes made to `SporbBase.cpp` and `SpacePirate.cpp`.

`.text` only - `gLoader_Shredder` is `.sbss 0x80419518 size:0x8` (`symbols.txt:20751`), the
`SLoaderSlot { value; padding; }` already claimed (`splits.txt:1773-1775`) and defined by
`Shredder.cpp`, so it is `extern` here and defined in exactly one unit. No `PortLinkStubs.cpp`
duplicate: `grep fn_8021F9E4 src/MetroidPrime/PortLinkStubs.cpp` is empty, and `gate.sh`'s
`port link dups` step passed. One function in the file, so the reverse-source-order rule
cannot be got wrong; `tools/check_decl_order.py` reports no permuted unit.

## Measured

* `build/report.json`, before -> after: `matched_functions` **13582 -> 13583**,
  `complete_units` **1008 -> 1009**, `total_functions` **28465 -> 28465** (the `splits.txt`
  edit moved no function between units), `fuzzy_match_percent` 37.686264 -> 37.686386.
* The new unit: `main/MetroidPrime/ScriptLoader/Carve8021F9E4`, `fuzzy_match_percent
  100.0`, `matched_functions 1 / 1`, `matched_code_percent 100.0`, `complete_units 1`.
* `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8021F9E4.c` -> `.text claimed 8 ours 8
  retail 8 fits`, `no extra functions`.
* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8021F9E4.c` -> `PASS -> kept as
  Matching`, `kept: 1 / 1 failed: 0 skipped: 0`. That is the acceptance test and it is the
  only thing that decides this item.
* `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, retail.
* `python3 tools/check_symbol_names.py` -> `checked 609 units; 0 declared names are missing`.
* `./tools/goal_check.sh build/goal/item.json` -> **PASS**, every line ok: no judge-owned
  path touched; `gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe);
  `counts: matched 13582 -> 13583   linked 6630 -> 6631`; symbol names clean; `All: 37.69%
  fuzzy, 31.12% matched, 13.99% linked (13583 / 28465 functions)`; flip test PASS with
  `Object(Matching)` in `configure.py`.

Not committed, as instructed. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as
modified in `git status` - those are `goal_check.sh`'s own derived-count rewrites (matched
13583, linked 6631, DOL units 11645, probe 984 files), not edits of mine.

## For the next lane

No `NEW:` lines. This address was one of the 30 same-shape setter carves already listed in
`docs/goal-notes/carve-80213cb8.md`, and its immediate neighbour `fn_8021F9B0`
(DigitalGuardian, 0x8021F9B0..0x8021F9B8, 8 bytes, `stw r3, gLoader_DigitalGuardian@sda21(r0)
; blr`) is the identical recipe against `DigitalGuardian.cpp`'s slot at `.sbss
0x80419510..0x80419518` - same gap, same extern, `+1` each. `docs/research/rel_loaders.md`
line 149 already carries the Shredder row.

One note for the carver, no cost: `Shredder.cpp` is the first `ScriptLoader/*.cpp` in this
family whose header comment had to be corrected, and the correction is only a comment - its
`.text` 0x8021F9B8..0x8021F9E4 is unchanged by it and still 44 bytes at 100%. `gate.sh`'s
report diff is what confirms that; do not assume a comment is free in a unit that owns a
`.sbss` slot.