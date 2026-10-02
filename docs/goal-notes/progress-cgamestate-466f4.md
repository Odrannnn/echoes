# progress-cgamestate-466f4 — `fn_801466F4` (0x801466F4, 172B)

**Outcome: the item's judge will fail. `matched_functions` for the unit did not move: 88 → 88.**
The named function went from 66.63% to 90.56% and the whole gate is clean, but a `progress`
item needs a function that reaches 100%, and the residue is now three instruction slots of pure
mwcceppc scheduling. What is left is written up below with every spelling and score, so the
next run does not repeat it.

## What I changed

`src/MetroidPrime/Player/CGameState.cpp`, `fn_801466F4` only (the rest of the tree is
untouched; the unit stays `NonMatching` and was not flipped).

The old body kept the old begin and the old end in locals and passed those same locals to
`fn_801467A0`, so both had to stay live across the `fn_8014680C` call. mwcceppc then needed
five registers, emitted `stmw r27,28(r1)` / `lmw r27,28(r1)`, and the function came out
**144 bytes against retail's 172** - which is what 66.63% was.

Retail holds only `self` (r29), `capacity` (r30) and the new buffer (r31) across that call
and recomputes both ends afterwards (`lwz r0,4(r29)` / `lwz r3,12(r29)` / `mulli` /
`add r4,r3,r0`, 0x80146760-0x8014676C). Re-reading them from `self` in the source, and
building the end by advancing a cursor in place, reproduces retail's frame exactly:

```cpp
uchar* cursor = static_cast< uchar* >(self->x0c_data);
cursor += self->x04_count * 36;
void* range[4];
range[1] = cursor;
range[0] = cursor;
range[2] = self->x0c_data;
range[3] = self->x0c_data;
fn_8014680C(&range[3], &range[1], buffer);
fn_801467A0(static_cast< uchar* >(self->x0c_data),
            static_cast< uchar* >(self->x0c_data) + self->x04_count * 36);
```

This fixes **both** residues the item's `reason` named, which is the new part:

* `r27-r31` vs retail's `r29-r31` - gone. The prologue is now retail's three `stw`s and the
  epilogue its three `lwz`s; no `stmw`/`lmw` at all.
* store order 16/20/12/8 vs 12/8/16/20 - gone. The stores are now 12(r1), 8(r1), 16(r1),
  20(r1), in that order, and the object is 172 bytes like retail's.

## Measured

```
./tools/decomp_build.sh                 -> All: 29.08% fuzzy, 21.22% matched, 11.37% linked (9437 / 28465)
sha1sum build/G2ME01/main.dol           -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
86 x cmp build/G2ME01/<n>/<n> vs orig/G2ME01/files/RelProd/<n>.rel -> 0 differ
./tools/probe_sources.sh                -> 734 files, 0 failed, 0 errors; LINKED (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py     -> checked 484 units; 0 declared names are missing
```

`build/report.json`, unit `main/MetroidPrime/Player/CGameState`, before → after:

| | baseline | now |
|---|---|---|
| `matched_functions` | 88 / 116 | **88 / 116** (unchanged) |
| `fuzzy_match_percent` | 78.65321 | 78.87892 |
| `fn_801466F4` | 66.62791% (144B) | 90.558136% (172B) |

Whole-report `All:` 29.083054% → 29.083685% fuzzy, `matched_functions` 9437 → 9437. No
function anywhere got worse (checked by diffing every function in the unit's objdiff output
against the pre-change build: one `UP`, zero `DOWN`, zero `GONE`).

## The residue, exactly

Six instruction slots of 45 differ. The frame, the guard, the allocation, the loop-free
`fn_8014680C` call, the recompute, the `fn_801467A0` call, `CMemory::Free`, the two stores
and the epilogue are all byte-identical. What is left is where mwcceppc puts the two
argument-address computations:

```
retail  0x80146734  addi r3,r1,0x14      <- &range[3], emitted first
retail  0x80146738  mulli r0,r0,0x24
retail  0x8014673c  mr   r5,r31
retail  0x80146740  addi r4,r1,0xc       <- &range[1], emitted last
ours             addi r4,r1,0xc         <- &range[1] first
ours             mulli r0,r0,0x24
ours             addi r3,r1,0x14        <- &range[3] third
ours             mr   r5,r31
```

Both `addi`s compute the same addresses; only their order in the block differs. It is not
reachable by reordering the four `range[k] = ...` statements alone - see the sweep.

## Spellings tried, and their scores

`objdiff-cli diff -p . -u main/MetroidPrime/Player/CGameState fn_801466F4`, right side
(our object), percent / object size. 66.62791 / 144 is the tree's committed spelling.

**The change above (re-read + cursor) is 90.558136 / 172.** 54 further spellings, none at
100%:

* 24 permutations of the four `range[k] = ...` statements x 2 value styles (`cursor` local
  vs the expression repeated inline). Best 90.558136 (`range[1],range[0],range[2],range[3]`,
  the one kept) and, on the same 6 differing slots, **95.04651** for
  `range[0],range[3],range[2],range[1]`. Both are 6 slots off; the higher percent is the
  one that gives up retail's store order, so I kept the other. `95.023254` (`0,3,1,2`),
  `94.93` (`1,2,0,3` / `1,3,2,0` / `1,3,0,2` / `0,2,1,3`), `94.91`, `94.90`, `91.53`,
  `91.19`, `91.26`, `90.53`, `90.51`, `90.49` for the rest; the inline-expression style
  never got above 82.09 and only produced 168-byte objects.
* `uchar**`/`uchar*` range array, a struct wrapper for the array, an inner `struct Range`,
  argument addresses hoisted into `void* const* const` locals (declared first and declared
  last), the `dst` argument cast to `void*`, the allocation moved after the array is filled,
  the count hoisted into a `u32 bytes` local, two separate cursor expressions, a
  `begin`-named local feeding both the add and `range[2]/range[3]`: all 90.56, 91.26, 92.00
  or a build error. None moved the `addi`s into retail's order.

## Two things the next run should not re-derive

**`report generate` and `diff` disagree about `bl` targets, and `report` is the one that
counts.** About thirty functions in this unit read 99.0-99.9% under
`objdiff-cli diff` purely because the callee's *name* differs (`bl
GetHardModeDamageMultiplier__10CTweakGameCFv` where retail has `bl fn_80216D38`).
`objdiff-cli report generate` scores all of them **100** - relocation target names are not
compared - and the unit's `matched_functions` is 88, not the 58 a `diff`-based count would
suggest. Work from `build/report.json`, never from a `diff` percentage, when judging what a
`progress` item did.

**`objdiff.json` has `target_path` and `base_path` the other way round from this repo's own
convention.** `tools/compare_unit.sh` and `tools/flip_test.sh` both say `build/G2ME01/obj/`
is the retail-derived object dtk split out of the DOL and `build/G2ME01/src/` is our
compiled one, but `objdiff.json` names `obj/` as `target_path` and `src/` as `base_path`.
It does not corrupt the percentages (the comparison is symmetric) but it does mean `diff`'s
left/right and the `size`/`address` fields in `report.json` are the **retail** object's.
I confirmed it by disassembling both: `build/G2ME01/obj/.../CGameState.o` reproduces the
retail bytes at 0x801466F4 exactly, `build/G2ME01/src/.../CGameState.o` is ours.

## Not done, and why

* `fn_8014601C` (99.05%), `fn_801426E0` (97.50%) and `fn_8014680C` (92.69%) are the epilogue
  wall already recorded in `build/goal/notes/triaged-2026-09-29.md`; the first and last are
  **100% in `report.json`** and so are not worth anything. I re-confirmed `fn_8014680C`'s
  real difference (ours hoists `*end` out of the loop, retail reloads it at
  0x80146848 after the call) and could not move it: `while`, `do/while`, `void*`-typed
  cursors, an `endp` alias, `in != *endp` and `*end - in != 0` all score 92.69, 91.73 or
  90.77.
* The sixteen functions at 0.00% (2848 bytes) are a separate item. `docs/history` and the
  triaged notes already record that nine runs have failed to land them, several because a
  callee they need (`fn_80142760`, the 36-byte element's destructor) is itself one of them.
  I did not start one: nothing I measured here shows a path to 100% for any of them, and a
  `NEW:` costs a lane an hour.

WALL: fn_801466F4 90.56% - frame, registers, store order and size now match retail; the residue is three slots where mwcceppc orders the two argument-address `addi`s, and 54 spellings (24 store permutations x 2 value styles, plus array/struct/alias/local reshapes) never put them in retail's order.

---

# Run 2026-10-02 (lane L11): **done — `fn_801466F4` is 100.0%, unit 105 -> 106**

**Outcome: `goal_check.sh build/goal/item.json` -> PASS.** `main/MetroidPrime/Player/CGameState`
`matched_functions` **105 -> 106 / 116** (`matched_code_percent` 62.80 -> 63.74, unit fuzzy
89.08094 -> 89.39570); whole report `matched_functions` 12495 -> 12496, `All:` fuzzy 35.31639 ->
35.317272. No function in the unit or anywhere in `build/report.json` moved down (diffed every
unit's per-function fuzzy percent against the pre-change report: 1 UP, 0 DOWN, 0 GONE).

## The residue the last run left is real, and the fix is the *parameter type*, not the schedule

The last run's 90.56% spelling (`void* range[4]` + `fn_8014680C(&range[3], &range[1], buffer)`)
leaves 5 differing slots, re-measured on this tree:

```
retail  addi r3,r1,20 | mulli | mr r5,r31 | addi r4,r1,12 | add | stw12, lwz, stw8, stw16, stw20
ours    addi r4,r1,12 | mulli | addi r3,r1,20 | mr r5,r31 | add | stw12, stw8, lwz, stw16, stw20
```

`SGameStateBlock` is exactly `rstl::vector<T>`'s layout (allocator/count/capacity/items), so this
function is `rstl::vector<T>::reserve`, and the 4-word range is the **argument temporaries of
`uninitialized_copy(begin(), end(), newData)`** — two one-word *iterator objects passed by
value*. Measured corroboration, this run: this tree's own `include/rstl/vector.hpp` reserve
instantiated at 100% (`reserve__Q24rstl62vector<Q24rstl18pair<9TEditorId,b>,...>Fi` in
`main/MetroidPrime/CMapWorldInfo`, 172 B) builds the *same* pairs — `{8(r1),12(r1)} = end`,
`{16(r1),20(r1)} = begin` — for that call.

So the fix is to give `fn_8014680C` **class parameters** instead of `void* const*`/`void**`:

```cpp
struct SStateIter { void* mCur; SStateIter(void* cur) : mCur(cur) {} };
extern "C" void* fn_8014680C(SStateIter begin, SStateIter end, void* dst) { ... }
...
  fn_8014680C(SStateIter(self->x0c_data),
              SStateIter(static_cast< uchar* >(self->x0c_data) + self->x04_count * 36), buffer);
```

Class arguments are passed by invisible reference, mwcceppc evaluates them **right to left**
(`end` gets 8/12, `begin` 16/20 — retail's layout exactly), and the two `addi`s then land in
retail's order. Result: `fn_801466F4` **43/43 instruction slots identical, 100.0%**, and
`fn_8014680C` stays byte-identical at 104 B (its `lwz r0,0(r29)` loop-bound reload survives the
class spelling — so the "`end` must be a `void**`" reason in the old comment was wrong; it is the
*reload*, not the parameter form, that is load-bearing).

`fn_801466F4`'s body also drops the four `range[k] = ...` stores and the `cursor` local: the
temps are the arguments now, so the function is `allocate` -> the call -> `fn_801467A0` re-reading
both ends -> `Free` -> the two stores.

## Not to be re-derived

* The 24 store permutations the last run swept were permutations of a *shape that cannot match*.
  With the class spelling there are no store statements to permute — the store order (12,8,16,20)
  falls out of the argument evaluation.
* `void* range[4]`, `uchar* range[4]`, `uintptr_t range[4]`, two 2-word arrays (both declaration
  orders), a 4-member struct, `range + 3` / `&*(range + 3)`, hoisted `void** const` argument
  locals (before and after the stores) and `*(range + k)` stores: **all measured this run, all
  still 5 slots off**, all 172 B. (`uchar* range[4]` without casts does not compile.)
* `PutTo__11CWorldState` (97.30%) and `__ct__11CWorldState` (97.38%) are one instruction short for
  the *same* reason and are **not** reachable the same way: retail passes a second argument
  (`saveWorld`) to `CWorldLayerState::PutTo(CBitStreamWriter&)` and
  `CWorldLayerState::__ct(CBitStreamReader&)`, both of which are **1-parameter and `Matching`** in
  this tree (`config/G2ME01/symbols.txt` has only the 1-param names at 0x801721FC / 0x801722EC).
  Reproducing that would mean adding a second overload to a `Matching` unit, which changes the
  linked DOL. Filed as `NEW:` below for a lane that can decide where the overload may live.
* `report generate` is the count; `objdiff-cli diff` on this unit still shows 99.x% on ~30
  functions whose only difference is a callee *name* (relocation targets are not compared by
  `report`). The last run's warning holds.

## Gates (all on this tree, this run)

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
./tools/probe_sources.sh        761 files, 0 failed, 0 errors; LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   525 units checked, 0 missing names
python3 tools/check_decl_order.py --unit main/MetroidPrime/Player/CGameState   ok, none out of order
./tools/goal_check.sh build/goal/item.json    PASS (includes gate.sh: DOL, 86 RELs, report diff,
                                              wiring, docs claims, port probe; no asm added)
```

`docs/HANDOFF.md`'s state block was rewritten by `goal_check.sh` itself (12495 -> 12496); no other
file is touched.

## Diff

`src/MetroidPrime/Player/CGameState.cpp` only: the `SStateIter` struct, `fn_8014680C`'s two
parameters, `fn_801466F4`'s body, and the two comment blocks above them (the old comment's
"`end` must be a `void**`" reason is corrected in place and says it was superseded).

NEW: progress-cgamestate-worldstate-arg | progress | MetroidPrime/Player/CGameState | PutTo__11CWorldState (97.30%, 1 instruction) and __ct__11CWorldState (97.38%, 1 instruction) both miss retail's extra `saveWorld` argument to the 1-parameter, Matching CWorldLayerState::PutTo/ctor; needs a decision on where a 2-parameter overload may live without changing the linked DOL.
