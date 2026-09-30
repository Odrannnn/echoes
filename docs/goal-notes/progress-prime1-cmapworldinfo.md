# progress-prime1-cmapworldinfo

`kind: progress`, `target: MetroidPrime/CMapWorldInfo`. **Unit stays `NonMatching`**; no
`flip_test.sh` was run (not the acceptance test for a `progress` item).

## Result, measured

`tools/goal_check.sh build/goal/item.json` -> **PASS**.

| | before | after |
|---|---|---|
| unit `matched_functions` | 21 / 23 | **22 / 23** |
| unit fuzzy | 98.71781% | 99.98824% |
| unit matched code | 70.311584% | 93.062904% |
| project `matched_functions` | 10318 / 28465 | 10320 / 28465 |

Per function, as the item asked:

| function | before | after | Prime 1's source |
|---|---|---|---|
| `rstl::vector<pair<TEditorId,bool>,rmemory_allocator>::insert_into<const_counting_iterator<pair<TEditorId,bool>>>` | 98.64341% | **100.00%** | needed one small edit (the `long` induction variable, below) |
| `rstl::sort<pointer_iterator<pair<TEditorId,bool>,...>,pair_sorter_finder<pair<TEditorId,bool>,less<TEditorId>>>` | 85.96610% | **99.830505%** | matched unchanged in body; needed the inline pragma below |
| the other 21 functions in the unit | 100% | 100% | Prime 1's source, unchanged |

No function anywhere got worse. A side effect, also an improvement and also
reported by the judge as part of the count: `main/MetroidPrime/Player/CPlayerState`'s
`insert_into<const_counting_iterator<TUniqueId>>` went 98.33861% -> **100.00%** from the
same `vector.hpp` change. The two `insert_into` instantiations live in different units,
so the judge counts them separately.

## What I changed (2 files, no asm)

### 1. `include/rstl/vector.hpp`, `insert_into`, line ~209: `int i` -> `long i`

```cpp
    for (long i = 0; i < atIdx; ++newIdx, ++i) {   // was: int i
      construct(newItems + newIdx, data()[i]);
    }
```

This is the whole `insert_into` diff. Prime 1's `vector.hpp` is byte-identical to ours
here (`int i`), and it matches there - so the difference is the compiler, not the
source. `atIdx` is a `difference_type` (a `long`); with an `int` induction variable
mwcceppc 2.7 materialises the zero it compares against and enters the first copy loop
through an explicit `cmpwi` + branch that retail does not have. With `long i` the
comparison folds into the record-setting `addze.` that divides the byte distance, and
the function is byte-exact. The diff was 10 instructions; this removed all 10.

**Spelling sweep** (each a full build, project-wide regression check on every one) -
`int i` 98.64 / **`long i` 100.00** / `int newIdx = 0;` before the `T* const newItems`
98.64 / `i != atIdx` 78.82 / `i += 1, ++newIdx` 78.83. So only the width of `i` matters;
loop-form changes are actively harmful.

### 2. `src/MetroidPrime/CMapWorldInfo.cpp`, line 10: `#pragma inline_max_size(160)`

`rstl::sort` swaps through `rstl::iter_swap`, and retail **expands** that swap into the
partition loop instead of calling it. With the project-wide 125-byte default the compiler
emits `bl iter_swap<...>` and keeps the pivot in a callee-saved register (`r31`); retail
inlines the swap, which frees `r9` (volatile) to hold the pivot. That single difference
cascaded into the frame size (retail `0x60`, ours `0x50`), a different register set
(`r28`/`r31` swapped) and a 20-byte size gap. Raising the threshold removes the call and
the whole cascade: the object grows the `iter_swap` body, and the unit's `.text` for
`sort` becomes exactly retail's 472 bytes.

**The pragma is TU-wide** - mwcceppc takes the *last* `#pragma inline_max_size` in the
file, so it cannot be scoped back down around `PutTo`. I tried
`#pragma inline_max_size(200)` at the top with a reset to 125 after the constructor and
again after `PutTo`; both reverted `sort` to 85.97%, because the reset at the end of the
file is the one that counts. This is a trap worth knowing: **`inline_max_size` scoping
does not work in this compiler.**

**Threshold sweep**, full builds, each checked for regressions project-wide:

| value | `sort` | `PutTo` |
|---|---|---|
| 125, 126 (project default) | 85.97% | 100% |
| **130, 135, 140, 150, 160, 180** | **99.83%** | **100%** |
| 200 | 99.83% | 98.24% |

At 200 the two `bit_vector` destructors in `PutTo` are inlined and mwcceppc rewrites
the epilogue to a `bl Free__7CMemoryFPv` that retail does not emit. **160 is the middle
of the 130..180 window**; 200 is the only value that breaks `PutTo`.

## What is left, and why

`sort` is at **99.830505%** - 4 instructions out of 118, in the pre-loop pivot setup,
and it is a pure register choice, not a structural difference:

```
retail:  lwz r5, 0x0(r28) ; subi r0, r3, 0x8 ; ... ; clrlwi r9, r5, 6 ; stw r0, 0x3c(r1)
ours:    lwz r0, 0x0(r28) ; subi r3, r3, 0x8 ; ... ; clrlwi r9, r0, 6 ; stw r3, 0x3c(r1)
```

Same instructions, same order, same size (472 = 472): the pivot copy lands in `r5`/`r0`
and the pre-decremented `end` in `r0`/`r3`. Everything after it - the whole partition
loop, including the now-inlined `iter_swap` - is instruction-for-instruction identical.
Getting the last 4 needs MWCC's register allocator to pick differently, which is what
declaration order is usually for in this codebase (see the existing comment on
`lower_bound`'s `halfDist`/`it` in the same header). I did **not** find the ordering.

**Spellings tried for `sort`, all measured, all 99.83%** (so the declaration order is
*not* the lever here, or not in any of these forms):

| spelling | score |
|---|---|
| `pivot` after `__sort3`, `it` after `pivot` (baseline) | 99.83% |
| `it` before `pivot` | 99.83% |
| `pivot` before `__sort3` | 99.83% |
| `--end` before `pivot` | 99.83% |
| `It it = first; ++it;` instead of `first + 1` | 99.83% |
| `end -= 1;` instead of `--end;` | 99.83% |
| `It end = last; --end;` before `__sort3` | 99.83% |
| `pivot` after `it` *and* after `--end` | 99.83% |
| `const value_type& pivot = *mid` (a reference, not a copy) | 99.83% **but regresses 8 other functions** in CGameArea / CTransitionDatabaseGame / CMapUniverse - do not use |
| `pivot` before `end` before `__sort3` | 95.72% |
| `pivot` between `end` and `__sort3` | 95.72% |
| `__sort3(*first, *mid, *end - 1, cmp)` with a bare `end` | 99.83% |

The reference form (`const value_type& pivot`) is the interesting negative result: it
leaves this function at 99.83% *and* costs 8 functions elsewhere, so `rstl::sort` is
close enough to its final shape that the header should not be touched again without a
full-project measurement.

`sort` also emits a `iter_swap` COMDAT in our object that retail does not define, and
`unit_fit.sh` should be run before anyone tries to flip this unit. I did not run it,
because a `progress` item is judged on the matched count, and the unit has one
sub-100% function left.

## Gates (all measured, all clean)

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh               probe: 752 files, 0 failed, 0 errors
python3 tools/check_symbol_names.py    checked 505 units; 0 missing names
./tools/decomp_build.sh                All: 31.32% fuzzy, 23.75% matched (10320 / 28465)
86 RELs                                86/86 sha1 match config.yml, 0/86 differ from
                                       orig/G2ME01/files/RelProd by cmp
total_functions                        28465 (unchanged; no splits.txt edit)
./tools/goal_check.sh build/goal/item.json   PASS
```

Reproduce the unit's numbers with `./tools/fast_try.sh MetroidPrime/CMapWorldInfo`
(0.3 s, no relink) - that is the loop to use, not `decomp_build.sh`.

## For the next run

* The open item is `rstl::sort<pointer_iterator<pair<TEditorId,bool>,...>,
  pair_sorter_finder<pair<TEditorId,bool>,less<TEditorId>>>` at 99.830505%, 4
  instructions of register allocation in the pre-loop pivot/`--end` setup. Everything
  structural about it is now correct (exact size, inlined `iter_swap`, volatile pivot).
  The table above lists 12 spellings that did not move it; do not repeat them.
* The lever that is *not* declaration order is still untested: the pivot's **type**.
  `value_type pivot` is `pair<TEditorId,bool>` (8 bytes, copied whole), but only
  `.first` is ever read by the comparator, and retail keeps just the masked `TEditorId`
  in `r9`. A `TEditorId`-typed pivot, or a `select1st`-style extraction, is a different
  shape from the twelve above and is the obvious next thing to try.
* `#pragma inline_max_size` is TU-wide in mwcceppc; the last one in the file wins. Do
  not spend a run trying to scope it.
* `rstl/vector.hpp` and `rstl/algorithm.hpp` are shared by 29 units. Any edit to
  `insert_into` or `sort` needs a full `./tools/decomp_build.sh` and a per-function diff
  against the previous `build/report.json`, not just `fast_try.sh` - the reference-form
  `pivot` above changed 9 functions across 4 units while leaving this unit unmoved.
