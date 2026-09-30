# progress-prime1-cplayerenergydrain (progress, MetroidPrime/Player/CPlayerEnergyDrain)

## Result

**Complete, and the whole unit matched at once.** One line in one header. The unit went from
9/12 to **12/12** functions at 100% and 62.51% -> **100.00%** unit fuzzy. The unit stays
`NonMatching` (this is a `progress` item; I did not touch `configure.py` and did not run
`flip_test` to decide).

| | before | after |
|---|---|---|
| unit fuzzy | 62.51% | **100.00%** |
| unit matched code | 40.34% | **100.00%** |
| **matched functions** | **9 / 12** | **12 / 12** |
| project `All:` matched | 10354 | 10357 |
| project `All:` linked | 5048 | 5048 (unchanged - the unit is still `NonMatching`) |

The three functions the item named, all reached with Prime 1's source unchanged:

| function | before | after |
|---|---|---|
| `vector<CEnergyDrainSource>::reserve(int)` (172 B) | 72.05% | **100.00%** |
| `vector<CEnergyDrainSource>::insert_into<const_counting_iterator<CEnergyDrainSource>>(...)` (1548 B) | 30.47% | **100.00%** |
| `vector<CEnergyDrainSource>::erase(it, it)` (108 B) | 77.59% | **100.00%** |

## What I changed

**One line**, in `include/MetroidPrime/Player/CPlayerEnergyDrain.hpp`:

```cpp
 namespace rstl {
-RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(CEnergyDrainSource)
+RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CEnergyDrainSource)
 }
```

No edit to `src/`, to `rstl/vector.hpp`, to Prime 1's source, or to `configure.py`. The rebuild
touched 48 targets, all of them units that include this header.

## Why - the measured cause

`rstl::construct(void* dest, const T& src)` dispatches to `construct_impl`, and the generic
`construct_impl` in `include/rstl/construct.hpp:40` is a **placement new**, `new (dest) T(src)`.
For a type that is not declared trivially constructible, mwcceppc 2.7 emits the placement-new
**null check on the destination** around every store:

```
848: cmplwi r8,0        <- ours only
84c: beq    860
850: lhz  r0,0(r7)
854: sth  r0,0(r8)
```

Retail's `erase(it,it)` has no such pair anywhere. The same extra `cmplwi rX,0x0 / beq` appeared
around the three copy loops in `insert_into`, and in `reserve` it was worse than cosmetic: the
null check made the loop body large enough to cross MWCC's inline threshold, so
`uninitialized_copy<...>` was **outlined into its own weak symbol** (548 B object vs retail's
1548 B) instead of being inlined the way retail has it. One trait, three functions.

`CEnergyDrainSource` is genuinely trivially copyable - it declares no copy ctor, no copy
assignment and no destructor, and `CHECK_SIZEOF` puts it at 0x8, `lhz`/`lfs` wide. So the
declaration is the faithful modelling, not a trick: retail's rstl clearly took the
`*static_cast<T*>(dest) = src;` path, and that is exactly what
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` expands to. (It subsumes the old
`RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE`, so nothing was lost - the destructible trait is still
set.)

**This is the general lesson for the remaining rstl units, not a CEnergyDrainSource fact.** Any
type that `rstl` copies in a loop and that is not declared trivially constructible will carry
this null check, and any type where it pushes a loop over the inline threshold will also get its
`uninitialized_copy` outlined where retail inlined it. Grep
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` in `include/` before assuming a `rstl::vector<T>` unit is
stuck: declaring the trait for `T` is local to that header and moves only that unit.

## The three functions, per the item's request

- **`reserve`** - Prime 1's `vector.hpp` is a *different rstl release* (it has
  `rstl/RstlVersions.h`, `Alloc::allocate` statics and the `#if RSTL_VERSION >= RSTL_R3IJ`
  base-class split; ours is the hand-adapted variant with no version macros). Prime 1's text
  for `reserve` is character-for-character the body ours already had. It matched unchanged once
  the null check went away. Nothing to tune.
- **`insert_into`** - identical conclusion. I had expected the `long i` / `int i` induction
  variable question in our `vector.hpp:214` (`for (long i = 0; i < atIdx; ...)`) to matter, since
  Prime 1 has `int i` there; it did not. Do **not** "fix" that line on the strength of this run -
  the comment above it claims it costs nothing in both instantiations and this run is consistent
  with that.
- **`erase(it, it)`** - identical conclusion. The increment ordering
  (`addi r9` / `addi r7` interleaved into the body in retail vs all three at the loop tail in
  ours) was a *consequence* of the extra block the null check created, not a source-shape
  difference. No spelling change was needed.

So: Prime 1's source matched **unchanged** in all three; what was missing was a trait
declaration that Prime 1's newer rstl does not need because it never uses placement new for
trivial types.

## Measured, not guessed: how the diffs were read

`build/tools/objdiff-cli diff -p . -u main/MetroidPrime/Player/CPlayerEnergyDrain --format json`
(plus `build/binutils/powerpc-eabi-objdump -d` on `build/G2ME01/{obj,src}/...o` for the block
layout, since objdiff reports `b`/`beq` targets relative to each side's own function base).

## What the unit still cannot do: it does not flip

I did **not** flip it (item kind is `progress`, and the brief says the unit stays `NonMatching`),
but I measured what a flip would hit, so the next run does not have to:

- `tools/unit_fit.sh MetroidPrime/Player/CPlayerEnergyDrain.cpp`:
  `.sdata2 claimed 8, ours 4, retail 8 - **SHORT by 4**`, and
  `1 function(s) present in ours but not in the retail unit object, 84 bytes total`:
  `__dt__Q24rstl55vector<18CEnergyDrainSource,Q24rstl17rmemory_allocator>Fv` (weak).
- `tools/compare_unit.sh MetroidPrime/Player/CPlayerEnergyDrain`: `.sdata2 DIFFERS`
  (`0000 00000000 00000000` vs `0000 00000000`).

So a `match` item on this unit is a real unit of work (drop the `~vector()` instantiation and
recover the missing 4 bytes of `.sdata2`), not a one-line flip. I did not file it as `NEW:`
because I have not shown it can succeed.

## Gates

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (run in `wt-mp2-goal-L7`, the same script,
baselines and verdict the driver uses):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10354 -> 10357   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.49% fuzzy, 23.91% matched, 11.83% linked (10357 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayerEnergyDrain: 9 -> 12 / 12 functions
  ok    no asm added
goal_check: PASS progress-prime1-cplayerenergydrain
```

`gate.sh`'s report diff is the check that matters for a one-header change: **no function anywhere
got worse**, and all 86 RELs and the DOL still hash as pinned.

Also run directly: `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors; link: LINKED
(250 undefined, 0 duplicates)`; `sha1sum build/G2ME01/main.dol` ->
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3 tools/check_symbol_names.py` -> clean
(also checked by `goal_check`).

`git status` shows `include/MetroidPrime/Player/CPlayerEnergyDrain.hpp` plus
`docs/HANDOFF.md`. I did not touch `docs/HANDOFF.md`; `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh`
(inside `goal_check.sh`) rewrote its derived state block, and
`python3 tools/check_docs_claims.py` passes against the new numbers.

## Notes / follow-ups (not fixed here - out of scope for this item)

- `docs/RUNNING_THE_DECOMP.md` and the "329 files" figure in `AGENTS.md` are stale
  (`probe_sources.sh` now says **752 files**). Documentation, not this item.
- `rstl/vector.hpp`'s comment at line 209-213 claims the `long i` induction variable "matches
  retail in both instantiations". This run did not disprove it, but the run that wrote it should
  have said which two.
- Any `rstl` unit still showing a `cmplwi rX,0x0 / beq` pair around a copy loop, or an
  `uninitialized_copy<...>` weak symbol where retail has the loop inlined, has this same cause.
  Grep the objdiff output for both patterns before spending a run on codegen spellings.
