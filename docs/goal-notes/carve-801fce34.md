# carve-801fce34

Goal item `carve-801fce34` (`kind: match`), target `MetroidPrime/ScriptObjects/Carve801FCE34`.
`./tools/goal_check.sh build/goal/item.json` in the lane worktree: **PASS** (exit 0), no PARTIAL.

## What I did

The four files of a carve, all three placed in address order:

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FCE34.c` | new, 60 lines of header + 1 function, plain C |
| `config/G2ME01/splits.txt` | `.text start:0x801FCE34 end:0x801FCE40` (:1437), between `ScriptObjects/Carve801FBC58.c` (0x801FBC58..0x801FBD68) and `ScriptObjects/Carve801FD4B0.cpp` (0x801FD4B0..0x801FD52C) |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FCE34.c")` (:806), one line, same place |
| `files.cmake` | `src/MetroidPrime/ScriptObjects/Carve801FCE34.c` (:831), same place |

Claimed exactly `.text 0x801FCE34..0x801FCE40` (0xC = 12 bytes) and nothing else. No
`PortLinkStubs.cpp` change was needed: `grep -rn fn_801FCE34 src/ include/ config/ files.cmake`
returned only `config/G2ME01/symbols.txt:8259` before this change, so the function had no
stand-in to remove and, having no callee, needs none added.

The body, and where it came from

Retail bytes, re-measured this run rather than read off dtk's listing:

```
$ build/binutils/powerpc-eabi-objdump -d --start-address=0x801FCE34 --stop-address=0x801FCE40 build/G2ME01/main.elf
801fce34 <fn_801FCE34>:
801fce34:	38 00 00 00 	li      r0,0
801fce38:	90 03 00 04 	stw     r0,4(r3)
801fce3c:	4e 80 00 20 	blr
```

It is the byte-shape twin the item names: `fn_80004010`
(`src/MetroidPrime/Carve80004010.c:98`, a `Matching` unit) is these three instructions and is
written there as `void fn_80004010(int* vec) { vec[1] = 0; }`, so this file uses the same
spelling and the same `int*` parameter type (line 61). No struct is spelled: the function's
argument is only ever read at `+4`, so there is nothing in its own bytes to name a field after.

Retail emitted this body three times in the neighbourhood and the other two are still dtk's:
`fn_80195228` (0x80195228) and `fn_80195234` (0x80195234), both 0xC bytes and both
`li r0,0 / stw r0,4(r3) / blr`, inside `MetroidPrime/Enemies/CStateMachine.cpp` — a
`NonMatching` unit (`configure.py:651`), so neither is matched here and neither is a `NEW:`
candidate for this run.

Evidence the reading is right: `grep -rn 'bl fn_801FCE34' build/G2ME01/asm/` returns exactly one
hit, at **0x801FCE14** inside `fn_801FCDAC` (0x801FCDAC, 0x88, unclaimed, and `.4byte
fn_801FCDAC` in a vtable at `build/G2ME01/asm/auto_07_803B7AE0_data.s:70`). That function's
teardown tail clears the object it was handed word by word and calls three container clears:

```
0x801FCDFC  stw   r0,0x48(r29)   // r0 = 0 from 0x801FCDF4
0x801FCE00  stw   r0,0x4(r29)
0x801FCE04  bl    fn_80195234    // r3 = r29+0x28
0x801FCE0C  bl    fn_80195228    // r3 = r29+0x18
0x801FCE14  bl    fn_801FCE34    // r3 = r29+0x18
```

which is the count-clear half of an `rstl::vector`-shaped member — exactly what the twin's own
caller `fn_80003F58` does (clear the word, test it, on zero release `+0xC` through
`Free__7CMemoryFPCv`; documented in that file's header). Two independent callers, one already
matched, both agreeing on one body.

## What was measured

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FCE34.c` -> `PASS -> kept as
  Matching`, `kept: 1 / 1   failed: 0   skipped: 0`.
- `./tools/carve_diff.sh 801FCE34 0xC build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FCE34.o`
  -> `retail: 3 instructions, 12 bytes / ours: 3 instructions, 12 bytes /
  differing instructions: 0 / BYTE-EXACT`.
- `./tools/unit_fit.sh ...` -> `claimed 12 ours 12 retail 12 fits`, and
  `no extra functions: our object defines only what the retail unit object does`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FCE34` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`. (It reports 0 units
  before the object is built, so this was run after `decomp_build.sh`, not before.)
- `./tools/decomp_build.sh` -> `87 files OK`, `All: 37.69% fuzzy, 31.13% matched, 13.99% linked
  (13611 / 28465 functions)`. `total_functions` is still 28465 after the `splits.txt` edit.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the gate
  re-checks that plus all 86 REL sha1s.
- `python3 tools/check_symbol_names.py` -> `checked 610 units; 0 declared names are missing`.
- `./tools/probe_sources.sh` -> `probe: 1011 files, 0 failed, 0 errors; link: LINKED (286
  undefined, 0 duplicates)`. The port's undefined count did not rise; this unit defines a DOL
  function no port source calls.
- `build/report.json`, from the judge's own report diff: `matched 13610 -> 13611`,
  `linked 6658 -> 6659`. `complete_units 1035 -> 1036`, `total_units 2410 -> 2412` (the new unit
  plus the `auto_03_801FBD68` dtk range, now split in two by the claim).
- `docs/HANDOFF.md` in `git diff` is the **judge's own** rewrite of the state block, done by
  `gate.sh` during `goal_check.sh`; I did not edit it.

## Boundaries, measured not assumed

Below the claim `0x801FBD68..0x801FCE34` is unclaimed and above it `0x801FCE40..0x801FD4B0` is
too — the next function is `SetupFSM__17CGenericFSM2State` (0x801FCE40, 0x198 = 408 bytes), so
the claim stops at 0x801FCE40 rather than running into it, and the claim does not span either gap.