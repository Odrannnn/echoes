# carve-80003efc

Goal item `carve-80003efc` (`kind: match`), target `MetroidPrime/Carve80003EFC`.
`tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L5` (`goal/lane-5` at `7d0233f7`): **PASS**, no PARTIAL.

## What I did

The four files of a carve, each entry placed in address order:

| file | change |
| --- | --- |
| `src/MetroidPrime/Carve80003EFC.c` | new, 1 function, plain C, header in the `Carve8038A7DC.c` / `Carve80004010.c` style |
| `config/G2ME01/splits.txt:28-29` | `.text start:0x80003EFC end:0x80003F08`, between `MetroidPrime/CMainResetGameState.cpp` (0x80003A48..0x80003BE8) and `MetroidPrime/Carve80004010.c` (0x80004010..0x8000408C) |
| `configure.py:712` | `Object(Matching, "MetroidPrime/Carve80003EFC.c"),` one line, same place |
| `files.cmake:653-656` | `src/MetroidPrime/Carve80003EFC.c`, between `Carve8000387C.cpp` and `Carve80004010.c` |

Claimed exactly `.text 0x80003EFC..0x80003F08` (0xC = 12 bytes) and nothing else. The body:

```c
void fn_80003EFC(int* vec) { vec[1] = 0; }
```

**No `PortLinkStubs.cpp` change was needed or wanted.** `fn_80003EFC` is a leaf with no caller in the
port link, so nothing referenced it and nothing stubs it: `grep -rn fn_80003EFC src/ include/` is
empty before and after this change, and `src/MetroidPrime/Carve80003EFC.c` becomes its definition.
The carve's own header says so rather than leaving a reader to wonder.

## The body, and where the spelling came from

`build/G2ME01/asm/auto_03_80003BE8_text.s:247-252` is three instructions:

```
/* 80003EFC */ li   r0,0
/* 80003F00 */ stw  r0,4(r3)
/* 80003F04 */ blr
```

That is the byte-shape twin the brief promised: `fn_80004010` (0x80004010..0x8000401C, 3/3 at
100.00% in `src/MetroidPrime/Carve80004010.c:98`) has the same three, and its own header traces the
shape back to `fn_80038D4C` (`src/MetroidPrime/CStateManager.cpp:105`, 100%) as `vec[1] = 0`. I
copied the twin's spelling rather than inventing one.

Evidence the reading is right, measured this run:

- `grep -rn "bl fn_80003EFC" build/G2ME01/asm/` returns exactly one hit, at **0x80003E6C**, inside
  retail's `rstl::vector<rstl::pair<unsigned int, unsigned int>, rmemory_allocator>::operator=`
  (`auto_03_80003BE8_text.s:192-245`). It calls with `r30` - the destination vector - then at
  0x80003E74 tests `+4` of the **source** and, on zero, frees `+0xC` of the destination through
  `Free__7CMemoryFPCv` (0x80003E80) before clearing `+4`/`+8`/`+0xC` with `li r0,0`. So the word
  written here is a count, and `vec[1] = 0` on an `int*` is the clear of it.
- That is the same capacity half of an `rstl::vector` assignment that `fn_80003F58` performs at
  0x80003F80 with `fn_80004010`, which is why the two are twins. (Both call sites measured with the
  same grep: `bl fn_80004010` -> one hit, 0x80003F80.)
- Only `+4` is written and nothing above it is touched, so the argument needs no element type -
  hence the plain `int*` and the file's lack of any `extern`.

## What I measured

    $ ./tools/flip_test.sh MetroidPrime/Carve80003EFC.c
      TEST MetroidPrime/Carve80003EFC.c
        PASS  -> kept as Matching
      kept: 1 / 1   failed: 0   skipped: 0
       PASS MetroidPrime/Carve80003EFC.c

    $ ./tools/unit_fit.sh MetroidPrime/Carve80003EFC.c
      == MetroidPrime/Carve80003EFC.c  (config/G2ME01/splits.txt)
         .text      claimed     12   ours     12   retail     12   fits
         no extra functions: our object defines only what the retail unit object does

    $ ./tools/carve_diff.sh 0x80003EFC 0xC build/G2ME01/obj/MetroidPrime/Carve80003EFC.o
      retail: 3 instructions, 12 bytes
      ours  : 3 instructions, 12 bytes
      differing instructions: 0
      BYTE-EXACT

    $ python3 tools/check_symbol_names.py
      checked 610 units; 0 declared names are missing from their object

    $ sha1sum build/G2ME01/main.dol
      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

    $ ./tools/goal_check.sh build/goal/item.json
      ok    no judge-owned path touched
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 13605 -> 13606   linked 6653 -> 6654
      ok    check_symbol_names.py
      ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13606 / 28465 functions)
      ok    flip_test MetroidPrime/Carve80003EFC.c: PASS, Object(Matching) in configure.py
      goal_check: PASS carve-80003efc

`build/report.json` after the build, unit `main/MetroidPrime/Carve80003EFC`:
`fuzzy_match_percent 100.0, total_code 12, matched_functions 1 / 1, complete_units 1, complete
True`, `fn_80003EFC` 12 B at 100.0%. `total_functions` is still **28465**, and
`main/auto_03_80003BE8_text` went 8 -> 5 functions with its range 0x428 -> 0x314, which is the
split and not a loss. The judge's own `build/goal/check-gate.log` says so:

    per-function diff  SPLIT  main/auto_03_80003BE8_text: 3 function(s) moved into
      main/MetroidPrime/Carve80003EFC, main/auto_03_80003F08_text (exact count match - a split, not a loss)
    port probe         ok    GATE PASS  7d0233f7+5 changed

Note there is **no link-order cycle**: the unit below the claim,
`MetroidPrime/CMainResetGameState.cpp` (a pre-existing `Matching` unit ending exactly where
`auto_03` begins), and the unit above, `Carve80004010.c`, both survived the split. `python3 tools/
check_decl_order.py --unit src/MetroidPrime/Carve80003EFC.c` reports `0 unit(s) checked` - the tool
only inspects multi-function units - which is expected for a single definition and the reason
`flip_test.sh`, not that tool, is the verdict above.

## Nothing blocked me, and nothing filed

The body matched on the first build; no spelling was retried, so there is no `WALL:` line.

**No `NEW:` item either, and here is why, so the next run does not re-derive it.** After this carve
`auto_03_80003BE8_text` still holds three unsourced `fn_` functions: `fn_80003BE8` (0x5C),
`fn_80003C44` (0xBC) and `fn_80003DA0` (0xA4). I checked for a matched twin for the biggest and
least obvious of them before deciding - `fn_80003C44`'s distinctive word-copy loop
(`addi r0,r6,0x7 / srwi r0,r0,3 / mtctr` / `addi r5,r5,8 / bdnz`) is present in 21 `auto_*` files
and in `MetroidPrime/Startup.s` and `MetroidPrime/CRainSplashGenerator.s`, but **zero** of the
functions in those files is at 100% in `build/report.json`, so there is no matched spelling to copy:

    $ python3 -c "...walk build/G2ME01/asm, cross .fn names against report.json functions at 100.0..."
    0

That is a twin hunt that has not been done, not a wall, so it belongs here and not in the queue: an
item built on it could not honestly be predicted to raise a count.

## One thing I noticed and did not fix

`docs/HANDOFF.md`'s state block came back dirty after `goal_check.sh` (matched 13605 -> 13606,
linked 6653 -> 6654, DOL units 11667 -> 11668). That is the judge tool rewriting its own derived
counts, and the prompt says the driver discards edits to that file, so I left it as it found it.