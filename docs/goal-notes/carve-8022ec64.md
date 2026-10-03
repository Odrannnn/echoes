# carve-8022ec64 — `fn_8022EC64`, SwampBossStage1's loader setter — Matching

`kind: match`, target `MetroidPrime/ScriptLoader/Carve8022EC64`. Claimed exactly
`.text 0x8022EC64..0x8022EC6C` (0x8 bytes, 1 function) out of dtk's
`main/auto_03_8022EC64_text` (0x8022EC64..0x8022FF98, 0x1334 bytes, 19 functions).

## What it is

`build/G2ME01/asm/auto_03_8022EC64_text.s`, the unit's first run:

```
# .text:0x0 | 0x8022EC64 | size: 0x8
/* 8022EC64 0022BA64  90 6D 98 88 */  stw r3, gLoader_SwampBossStage1@sda21(r0)
/* 8022EC68 0022BA68  4E 80 00 20 */  blr
```

The loader-setter family: store the argument into the loader pointer's `.sbss` slot and
return. Byte-shape twin of the matched `fn_80200E3C`
(`src/MetroidPrime/ScriptLoader/Carve80200E3C.c`, `gLoader_SpacePirate`) and of `fn_8022A024`
(`Carve8022A024.c`, `gLoader_IngSpiderBallGuardian`) — identical apart from the `@sda21`
displacement, because the slots sit 8 bytes apart.

Displacement measured, not recalled: `build/binutils/powerpc-eabi-nm build/G2ME01/main.elf`
gives `_SDA_BASE_ = 0x8041FD80`; `symbols.txt:20783` gives
`gLoader_SwampBossStage1 = .sbss:0x80419608`. `0x80419608 - 0x8041FD80 = -0x6778`, encoded
`0x9888` → `90 6D 98 88`. `symbols.txt:9943` gives
`fn_8022EC64 = .text:0x8022EC64; // type:function size:0x8 align:4`.

## Argument type, read off module 78's own listing

`build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s`:

- `RELExit` at `.text 0xEC`, 0x24 bytes: `li r3, 0x0` then `bl fn_8022EC64` — teardown.
- `fn_78_130` at `.text 0x130`, 0x30 bytes (from `RELMain` at 0x110):
  `lis r4, fn_78_160@ha` / `addi r0, r4, fn_78_160@l` / `lis r3, lbl_78_bss_20@ha` /
  `stwu r0, lbl_78_bss_20@l(r3)` / `bl fn_8022EC64`, with r3 still pointing at the record.
  So the argument is that record's **address**, and
  `build/G2ME01/SwampBossStage1/asm/auto_05_00000000_bss.s` gives
  `lbl_78_bss_20` `.bss:0x20 size:0x4` → the record is one `FScriptLoader` (4 bytes).
  The same listing holds `lbl_78_bss_0` size:0x8 at 0x0 and `lbl_78_bss_8` size:0x18 at 0x8;
  only the last is the loader slot.

`src/MetroidPrime/ScriptObjects/CSwampBossStage1Rel.cpp:113-131` already reads it the same way
(`fn_8022EC64(&lbl_78_bss_20)`), and `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp`
(`Matching`, claims `.text 0x8022EC38..0x8022EC64` and `.sbss 0x80419608..0x80419610`)
defines the slot as `SLoaderSlot` and calls through at `+0`. So this unit claims `.text` only
and takes the slot as `extern`; the source declares a local
`struct SSwampBossStage1Loaders { void* swampBossStage1; }` so the store is one `stw` — the
same trick `Carve8022A024.c` uses, and the reason the struct is declared **above** the
prototype (a struct named in a parameter list is scoped to that list and the host build then
rejects the definition as a conflicting type).

`.c`, not `.cpp`: module 78 imports the plain retail name, so the definition must be
unmangled.

## The four carve files (all four, each in address order)

| file | edit |
| --- | --- |
| `config/G2ME01/splits.txt` | new block after `SwampBossStage1.cpp`, before `IngBoostBallGuardian.cpp`: `.text start:0x8022EC64 end:0x8022EC6C` |
| `configure.py` | one-line `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EC64.c"),` between `SwampBossStage1.cpp` and `IngBoostBallGuardian.cpp` |
| `files.cmake` | `src/MetroidPrime/ScriptLoader/Carve8022EC64.c` after `Carve8022EB54.c` in the carve group |
| `src/MetroidPrime/ScriptLoader/Carve8022EC64.c` | new, the source |

No `PortLinkStubs.cpp` duplicate: `grep -rn fn_8022EC64 src/` finds only the declaration and
the three calls in `CSwampBossStage1Rel.cpp` plus the new definition. No cycle: dtk's unit now
starts at 0x8022EC6C, so it no longer begins exactly where `SwampBossStage1.cpp`'s `.text`
ends (the `0x80302BAC` cycle in `RUNNING_THE_DECOMP.md`); it ends at 0x8022FF98 where
`IngBoostBallGuardian.cpp` starts, the same shape `Carve8022A024.c` already links with.
Descending source order is trivially satisfied (one function).

One extra edit, inside the item's blast radius: `SwampBossStage1.cpp`'s header said "The
8-byte setter at 0x8022EC64 is deliberately NOT claimed … must stay in dtk's auto unit".
That claim is false after this carve, so it is corrected in place (corrected, not deleted —
the reason it was left alone is recorded, and the file it now names is
`Carve8022EC64.c`). Note the same stale sentence is still in the other 60-odd
`*Loader*.cpp` / `*Swarm.cpp` neighbours from the earlier carve batches (e.g.
`IngSpiderBallGuardian.cpp:7-8`, which claims the same thing about `fn_8022A024`); those are
out of scope here.

## Measured

```
$ ./tools/decomp_build.sh                 # All: 37.69% fuzzy, 31.12% matched, 13.99% linked
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
$ python3 tools/check_symbol_names.py
checked 609 units; 0 declared names are missing from their object
```

`build/report.json`, before → after:

| measure | before | after |
| --- | --- | --- |
| `matched_functions` | 13595 | **13596** |
| `matched_code` | 2034036 | **2034044** |
| `total_units` | 2400 | **2401** |
| `complete_units` | 1021 | **1022** |
| `total_functions` | 28465 | **28465** (unchanged, as `splits.txt` edits must leave it) |

`main/MetroidPrime/ScriptLoader/Carve8022EC64`: `fn_8022EC64` 0x8 bytes, 100.00% matched,
`matched_functions 1 / 1`, `metadata.complete: true`.

```
$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022EC64.c
TEST MetroidPrime/ScriptLoader/Carve8022EC64.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022EC64.c
   .text      claimed      8   ours      8   retail      8   fits
   no extra functions: our object defines only what the retail unit object does
```

`tools/carve_diff.sh` run against the **pre-link** object
(`build/G2ME01/obj/.../Carve8022EC64.o`) prints `NOT byte-exact`, because that file still has
its `@sda21` relocation unresolved (`ours: 00000000 stw r3,0(0)`). Read the linked ELF
instead — which is what `flip_test.sh` and the DOL hash compare:

```
$ build/binutils/powerpc-eabi-objdump -d --start-address=0x8022EC64 --stop-address=0x8022EC6C build/G2ME01/main.elf
8022ec64 <fn_8022EC64>:
8022ec64:	90 6d 98 88 	stw     r3,-26488(r13)
8022ec68:	4e 80 00 20 	blr
```

8 of 8 bytes, retail's exactly. **`carve_diff.sh` wants the linked artifact, not the `.o`**
— worth a line in the tool's docstring, since it is otherwise a false alarm on every carve.

## Judge

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item carve-8022ec64 (match) target=MetroidPrime/ScriptLoader/Carve8022EC64
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13595 -> 13596   linked 6643 -> 6644
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13596 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EC64.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022ec64
```

## For the next run

This was the fourth carve of this loader-setter family in
`MetroidPrime/ScriptLoader/` (`Carve80200E3C.c`, `Carve8022A024.c`, `Carve80235DCC.c`, now
this one) and the shape is now fully pinned down: twin → copy the C, declare the module's
record as a one-pointer struct above the prototype, take the slot as `extern`, and the only
free variable is the `@sda21` displacement, which nobody has to predict because MWCC derives
it from the `extern` symbol. `docs/research/rel_loaders.md` is the index of the family; the
remaining setters still in dtk auto units are the ones listed there with no `*.c` next to
their `fn_<addr>`.
