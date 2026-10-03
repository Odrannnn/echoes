# carve-8021fa4c - `MetroidPrime/ScriptLoader/Carve8021FA4C`

`kind: match`. Carved `fn_8021FA4C` (retail `.text 0x8021FA4C..0x8021FA54`, 0x8 = 8 bytes,
1 function) out of dtk's unclaimed `main/auto_03_8021FA4C_text` unit into its own
`Matching` unit. **Done and flipped** - `flip_test.sh` PASS, `goal_check.sh` PASS.

## What the function is

```
fn_8021FA4C  0x8021FA4C  0x8   stw r3, gLoader_StoneToad@sda21(r0)
                                blr
```

StoneToad's loader setter. Byte-shape twin of the matched
`src/MetroidPrime/ScriptLoader/Carve80200EFC.c` (`stw r3, gLoader_Parasite@sda21(r0)` /
`blr`), so the body is that twin's body with this copy's slot - no spelling was tried,
there was nothing to try.

## Argument, read off the callers

Both callers are in module 77's own listing,
`build/G2ME01/StoneToad/asm/auto_00_00000428_text.s` (module 77 is
`files/RelProd/StoneToad.rel`, `config/G2ME01/config.yml:488`):

- `RELExit` at 0x454 (0x24) passes `li r3, 0` - the module tears its loader down on exit.
- `fn_77_498` at 0x498 (0x30) does `lis r4, fn_77_4C8@ha` / `lis r3, lbl_77_bss_0@ha` /
  `stwu r0, lbl_77_bss_0@l(r3)` and then `bl fn_8021FA4C` with `r3 = lbl_77_bss_0`. So the
  argument is that record's address and the record is **0x4 bytes, one `FScriptLoader`** -
  `build/G2ME01/StoneToad/asm/auto_05_00000000_bss.s` gives `lbl_77_bss_0 size:0x4`.

The slot is `gLoader_StoneToad` at `.sbss 0x80419528`, `size:0x8 data:4byte`
(`config/G2ME01/symbols.txt:20753`). `src/MetroidPrime/ScriptLoader/StoneToad.cpp` (a
`Matching` unit) already defines that slot as `struct SLoaderSlot { FScriptLoader* value;
unsigned int padding; }` and already calls member 0 through it, and its own header
reserves these eight bytes for a separate unit: *"The 8-byte setter at 0x8021FA4C is
deliberately NOT claimed"*. That reservation is what this file fills, so the carve claims
`.text` only and takes the pointer as `extern`.

## The four files, in one change

| file | what |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8021FA4C.c` | new, plain C so `fn_8021FA4C` stays unmangled |
| `config/G2ME01/splits.txt:1791` | `Carve8021FA4C.c:` `.text start:0x8021FA4C end:0x8021FA54`, between `StoneToad.cpp` and `Coin.cpp` |
| `configure.py:915` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8021FA4C.c"),` between `StoneToad.cpp` and `Coin.cpp` |
| `files.cmake:861` | `src/MetroidPrime/ScriptLoader/Carve8021FA4C.c`, after `Carve80201418.c` |

Address order in `splits.txt`, one `Object(...)` per line, and the range is exactly
0x8021FA4C..0x8021FA54 with no unclaimed gap on either side (`StoneToad.cpp` ends at
0x8021FA4C, `Coin.cpp` starts at 0x8021FA54). No `PortLinkStubs.cpp` duplicate existed -
`grep -rn 8021FA4C src/` found only the two comments in `StoneToad.cpp` that reserve the
range.

## Measured

```
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010          # the pinned retail hash
$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8021FA4C.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0
$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8021FA4C.c
   .text  claimed 8  ours 8  retail 8  fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/check_symbol_names.py
checked 609 units; 0 declared names are missing from their object
$ ./tools/decomp_build.sh
All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13585 / 28465 functions)
  DOL: 59.35% fuzzy, 48.21% matched, 22.09% linked (723 / 1597 files)
```

`build/report.json`, the new unit:

```
main/MetroidPrime/ScriptLoader/Carve8021FA4C  fuzzy 100.0  matched 1/1 functions  1/1 unit
  fn_8021FA4C  8 bytes  100.0
```

`total_functions` is still **28465** after the `splits.txt` edit, as required.

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13584 -> 13585   linked 6632 -> 6633
  ok    check_symbol_names.py
  ok    flip_test MetroidPrime/ScriptLoader/Carve8021FA4C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8021fa4c
```

## Two notes for the next run

**`carve_diff.sh` says `NOT byte-exact` here and that is not a result.** Run against the
relocatable `build/G2ME01/obj/.../Carve8021FA4C.o` it reads retail's
`stw r3,-26712(r13)` against ours' `stw r3,0(0)` - the `@sda21` displacement is a relocation
the linker fills, so an unlinked object can never show it. `flip_test.sh` and the matching
`main.dol` sha1 are the real test, and both are green. Do not chase the carve_diff line on a
unit whose body is a single `stw ...@sda21`.

**`check_decl_order.py --unit <this>` reports `0 unit(s) checked`.** It counts a unit only
when it has more than one function to order, so "cannot get it wrong" is a one-function
property here, not a pass it verified. Same conclusion, weaker evidence.

## Nothing left

Item complete - no `WALL:`, no `NEW:`. The rest of the `auto_03_8021FA4C_text` claim was
just this one function, and `build/G2ME01/asm/auto_03_8021FA4C_text.s` is now fully
sourced.