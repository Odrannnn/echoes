# carve-80004b9c - `MetroidPrime/Player/Carve80004B9C` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-80004b9c`:

```
goal_check: item carve-80004b9c (match) target=MetroidPrime/Player/Carve80004B9C
goal_check: baseline .../wt-mp2-goal-L1/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12607 -> 12609   linked 5968 -> 5970
  ok    check_symbol_names.py
  ok    All:  35.47% fuzzy, 29.29% matched, 13.00% linked (12609 / 28465 functions)
  ok    flip_test MetroidPrime/Player/Carve80004B9C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80004b9c
```

`total_functions` is still **28465** after the `splits.txt` edit.  `build/report.json` has the unit at
`fuzzy_match_percent 100.0`, `matched_code 176 / 176`, `matched_functions 2 / 2`, and both functions
at `100.0`:

```
__dt__80004B9C  0x80004B9C  0x50  80 B  100.0
fn_80004BEC     0x80004BEC  0x60  96 B  100.0
```

The gate's own per-function diff is the clean-split form, not a loss and not a regression:

```
  SPLIT   main/auto_03_80004B9C_text: 2 function(s) accounted for across 1 new unit(s) in main
          (exact count match - a split, not a loss)
  LINKED   main/MetroidPrime/Player/Carve80004B9C
  +100%    main/MetroidPrime/Player/Carve80004B9C :: __dt__80004B9C
  +100%    main/MetroidPrime/Player/Carve80004B9C :: fn_80004BEC
no regression
```

## What the carve is

`.text 0x80004B9C..0x80004C4C`, `0xB0` = 176 bytes, **2 functions**, the whole of dtk's
`auto_03_80004B9C_text` (`config/G2ME01/symbols.txt:91-92`).  The item named both of them, and both
are claimed, so unlike `carve-80004c4c` there was nothing to extend the claim over:

| function | retail | what it is | its measured twin |
| --- | --- | --- | --- |
| `__dt__80004B9C` | 0x50, 20 insns | the deleting destructor of `SGameStateSlots`: `destroy_elements(this)`, then `CMemory::Free(this)` if `(short)flag > 0` | `fn_80004A4C` (0x80004A4C, `Player/CGameStateBlockDtor.cpp`), these 20 instructions with one `lwz r3,0xc(r30)` inserted at 0x80004A6C - and that file says so at lines 37-40 |
| `fn_80004BEC` | 0x60, 24 insns | `rstl::reserved_vector::destroy_elements` (`include/rstl/reserved_vector.hpp:97-105`) over the three inline 16-byte elements at `self + 4` | no twin in the tree; the loop shape is the one `fn_80004CD4` (`Player/Carve80004C4C.c`) is a stride-0x10 sibling of |

The object is `SGameStateSlots` in `include/MetroidPrime/Player/CGameStateBlocks.hpp:55` -
`{ int x00_count; SGameStateBlock x04_blk[3]; }`, 0x34 bytes - and all four neighbouring units are
`MetroidPrime/Player/`, so the directory is retail's own.  The pair slots in between
`Player/CGameStateBlockCopyCtor.cpp` (which ends at 0x80004B9C) and `Player/Carve80004C4C.c` (which
starts at 0x80004C4C), so the claim spans no unclaimed gap.

Both callees were already ours, which is what the item predicted and the reason the port link did
not grow: `fn_80004C4C` by `Player/Carve80004C4C.c` and `Free__7CMemoryFPCv` by
`Kyoto/Alloc/CMemory.cpp` (`src/Kyoto/Alloc/PortMwccNew.cpp:34` for the host).

## Two spellings that decide the bytes, both measured

`tools/carve_diff.sh 80004B9C B0 build/G2ME01/src/MetroidPrime/Player/Carve80004B9C.o`:

1. **The loop's cursor is declared before the index.**  `unsigned char* it; int i;` gives retail's
   `li r30,0` / `addi r31,r29,0x4`; declaring them the other way round is the same arithmetic with
   both registers swapped - **8 of 24** instructions differed.
2. **The increment order is `it += 16, ++i`.**  Retail emits `addi r31,r31,0x10` then
   `addi r30,r30,1`; `++i, it += 16` emits them the other way round - **2** instructions differed.
   This is the `sets versus orders` lesson in `docs/PROCESS_LESSONS.md` showing up in a for-loop's
   comma list, and it is the same shape as the `subi`/`addi` order `Carve80004C4C.c` had to keep.

3. **The return type of the destructor is `void*`, and that is measured, not assumed.**  Retail's
   epilogue is `mr r3,r30`.  Spelled `void`, mwcceppc drops it: the object came out **19
   instructions / 76 bytes** and everything from 0x80004BD4 on was shifted.  With `void*` it is 20
   and byte-exact.

**The count is re-read from the receiver on every iteration** (`lwz r0,0(r29)` at the bottom test,
`cmpw r30,r0`), so the test must be spelled on `self->mCount` and not on a saved copy.  That is the
one thing that differs from the copy loop next door: `fn_80004CD4` takes its count in `r4` and keeps
it in a register for the whole loop.

## A declaration in two other units is wrong, and was left alone

`src/MetroidPrime/main.cpp:1996` and `src/MetroidPrime/CMainResetGameState.cpp:294` both declare
`__dt__80004B9C` as returning **`void`**; retail returns its receiver in `r3` (see measurement 3).
Both are `Matching` units, both call sites discard the result, and the symbol has C linkage, so
neither mwcceppc nor the port's g++ ever sees a definition and a declaration in the same
translation unit - nothing breaks and nothing detects it.  Correcting them is a two-word change in
two units this item has no business rebuilding, so it is recorded in the carve's own header comment
instead and left as a `NEW:` line below.  (`fn_80004BEC`'s own declaration,
`src/MetroidPrime/Player/CGameState.cpp:35`, already matches and needed no change.)

## Files (the carve is four, and this one needed a fifth and sixth derived-doc edit)

| file | change |
| --- | --- |
| `src/MetroidPrime/Player/Carve80004B9C.c` | new, 164 lines; two definitions, **descending by address**, plain C so the `fn_`/`__dt__` names do not mangle |
| `configure.py:660` | `Object(Matching, "MetroidPrime/Player/Carve80004B9C.c"),` on one line, in address order between `Player/CGameStateBlockCopyCtor.cpp` (0x80004AA0) and `Player/Carve80004C4C.c` (0x80004C4C) |
| `config/G2ME01/splits.txt:40-41` | `.text start:0x80004B9C end:0x80004C4C`, between the same two units |
| `files.cmake:422` | `src/MetroidPrime/Player/Carve80004B9C.c`, in the carve block in address order |
| `docs/research/port_link_gap_list.md:246` | one line **deleted**: `- \`fn_80004BEC\`` |
| `docs/research/port_link_gap.md:111` | the `unmangled: fn_/lbl_/globals` row 56 -> 55, plus a dated entry |

The last two are not optional and not tidying.  `tools/gate.sh`'s `port link gap` step runs
`tools/link_gap.py --rebuild`, which checks the MISSING set against the list **both ways**: a
resolved name that is still listed fails with

```
stale: fn_80004BEC is listed but no longer missing - delete the entry
GATE FAIL: link-gap
```

and the `docs` step then re-derives the table in `port_link_gap.md` from the list, so it fails with

```
stale:   the gap table says unmangled: fn_/lbl_/globals is 56, the generated list has 55
stale:   the gap table's rows sum to 286, the generated list holds 285
GATE FAIL: docs
```

Both were measured here, in that order, one `goal_check.sh` run each.  Neither file is judge-owned
(`tools/`, `docs/research/port_link_baseline.txt` and `build/goal/` are; the step
`ok  no judge-owned path touched` says so).  `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show
as modified in `git status`: that is the judge's own `MP_GATE_DOCS_WRITE=1` step rewriting their
derived counts, not a lane edit.

No `symbols.txt` edit (the two placeholders already carry the right sizes and `scope:global`) and
no duplicate to delete - `PortLinkStubs.cpp` has no hit for either name.
`src/MetroidPrime/PortReachStubs.cpp:1666-1668` does alias `fn_80004BEC`, but that file is
**diagnostic only**, built with `-DMP_BOOT_STUBS=ON`, which nothing but `tools/boot_probe.sh`
passes, so it is not a duplicate.

## Verification (every number measured in this run)

```
tools/carve_diff.sh 80004B9C B0 build/G2ME01/src/MetroidPrime/Player/Carve80004B9C.o
    retail: 44 instructions, 176 bytes / ours: 44 instructions, 176 bytes
    differing instructions: 3   <- exactly the three `bl`s, which are R_PPC_REL24 relocations in
                                   our object (fn_80004BEC, Free__7CMemoryFPCv, fn_80004C4C) and
                                   all three resolve; the unchanged main.dol sha1 is the proof
tools/unit_fit.sh MetroidPrime/Player/Carve80004B9C.c
    .text claimed 176 ours 176 retail 176 fits; no extra functions
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/Carve80004B9C
    ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/check_symbol_names.py
    checked 529 units; 0 declared names are missing from their object
build/gate-probe.log        probe: 783 files, 0 failed, 0 errors; link: LINKED (290 undefined,
                            0 duplicates)      <- 291 -> 290: fn_80004BEC closed, nothing opened
build/gate-link.log         port link gap, measured over 777 object(s): 285 MISSING
                            ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md
build/gate-order.log        ok: 1010 unit(s) checked, 28 permuted, all 28 accounted for
sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs re-hashed against config/G2ME01/config.yml: 86 modules, 0 mismatched; 0 of 86 differ from
    orig/G2ME01/files/RelProd/ by cmp
./tools/flip_test.sh MetroidPrime/Player/Carve80004B9C.c
    PASS -> kept as Matching   (kept 1/1, failed 0, skipped 0)
```

## What the item predicted, and what actually happened

The item's `reason` says the unit is "net -2 on the gap and +2 matched".  The **+2 matched is
exact**.  The **-2 is -1**: `fn_80004BEC` was in
`docs/research/port_link_gap_list.md:246` and in the recorded undefined baseline
(`build/goal/judge/undef.base.txt:246`, `fn_80004BEC  CGameState.cpp.o`), but
`__dt__80004B9C` was **not** in either, despite `main.cpp:2050` and `CMainResetGameState.cpp:386`
both calling it.  Measured: the baseline's 291 undefined names contain `fn_80004BEC` and contain no
`__dt__80004B9C` line at all, and the port's undefined count is now 290.  The gap list is the list
of *missing* symbols, and dtk's own `auto_03_80004B9C_text.o` was never in the port's object
libraries, so nothing in `files.cmake` referenced `__dt__80004B9C` in a way the linker had to
report - the two call sites are the ones that were already there before this item and they did not
put it on the list.

## Caveats

- The `void`-returning declarations of `__dt__80004B9C` in two `Matching` units are wrong (see
  above) and are not fixed by this change.
- The `.s` file `build/G2ME01/asm/auto_03_80004B9C_text.s` is still on disk with retail's bytes; dtk
  no longer regenerates it now that the range is claimed, so it is the surviving record the carve
  header cites.  dtk now emits ours to
  `build/G2ME01/asm/MetroidPrime/Player/Carve80004B9C.s`.
- This lane's shell carries `MP_GOAL_TREE=.../wt-mp2-goal-L1` and `MP_GOAL_JUDGE`/`MP_GOAL_BASE`
  inside that same tree, so the counts line above is against **this** worktree's recorded baseline
  (matched 12607, linked 5968), not another lane's.
- Nothing is left unclaimed in this stretch any more: `config/G2ME01/splits.txt:35-47` is now the
  contiguous run `0x80004A4C..0x80004D84` with no gap in it, and the run dtk had named
  `auto_03_80004B9C_text` is gone from `build/report.json`.  Three `auto_03_80004*` units are still
  unclaimed and all three are outside that stretch - `auto_03_8000408C_text` (0x8000408C..0x800045A0),
  `auto_03_80004798_text` (0x80004798..0x80004A4C) and `auto_03_80004D84_text` (0x80004D84..0x800053B8,
  which `MetroidPrime/main.cpp` then claims).

NEW: fix-dt80004b9c-decl | match | MetroidPrime/main.cpp | `main.cpp:1996` and
`CMainResetGameState.cpp:294` declare `__dt__80004B9C` as returning `void`, but retail's epilogue is
`mr r3,r30` and a `void` spelling compiles to 19 instructions instead of 20 - one return type, two
`Matching` units, no behaviour change.
