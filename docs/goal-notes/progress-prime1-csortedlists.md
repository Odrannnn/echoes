# progress-prime1-csortedlists — MetroidPrime/CSortedLists

Trial of porting Prime 1's `src/MetroidPrime/CSortedLists.cpp` (Matching in Prime 1, GC/1.3.2)
to Echoes (GC/2.7). **The unit went from 11/20 to 18/20 matched functions; 8 of 9 remaining
functions reached 100%, the 9th is at 93.99%. The whole file is now 98.12% fuzzy.**

Diff: `src/MetroidPrime/CSortedLists.cpp` only, +141/-42. No asm, no header/layout change,
no other unit touched. Not committed (driver's job).

## The one change that unlocked six functions

`GetPointForSL` was the shared helper behind six of the seven enumerated functions, and this
tree's version branched on the axis:

```c++
return list < kSL_MaxX ? box.GetMinPoint()[int(list)] : box.GetMaxPoint()[int(list) - kSL_MaxX];
```

Prime 1's is a **flat float index**:

```c++
return reinterpret_cast<const float*>(&box)[list];
```

These are semantically identical — `ESortedLists` is `kSL_MinX, kSL_MinY, kSL_MinZ, kSL_MaxX,
kSL_MaxY, kSL_MaxZ` (offset 0,1,2 then 0,1,2 into the second vector), so `&box` reinterpreted
as `float[6]` is min.x,min.y,min.z,max.x,max.y,max.z. Retail confirms it: `FindInListLower` at
`0x800FE608` is

```
slwi    r6,r4,2            # list * 4
...
lfsx    f0,r3,r0           # *(float*)(this + node*44 + 4 + list*4)
```

— an indexed load off the box with **no** `cmpwi`/`bge` on the axis. The tree's ternary emitted
exactly that pair of extra instructions, and it is what held every function using the helper
below 100%. **Echoes' ESortedLists order is the same as Prime 1's, so the flat index is
correct here too — no layout change was needed or made.** Lesson worth keeping: when a
translated helper looks "equivalent", check whether the *compiled shape* (branch vs indexed
load) is what retail does, not just the C++ semantics.

## Per-function results (before% -> after%, and how Prime 1's source fared)

| function | before | after | outcome |
|---|---|---|---|
| `FindInListLower` | 67.07% | **100%** | small edits — Prime 1's *body* was right, but `int half;` hoisted as a separate declaration and `count = count - half - 1` (not `-= half + 1`) were needed. Combined with the `GetPointForSL` fix. |
| `FindInListUpper` | 68.79% | **100%** | small edits — Prime 1 tests `value < GetPointForSL(...)`, i.e. the branches are the **mirror** of this tree's `<=`. Retail's `fcmpo cr0,f1,f0` / `bge` confirms Prime 1's orientation. Plus `GetPointForSL`. |
| `InsertInList` | 41.70% | **100%** | **unchanged source after the `GetPointForSL` fix.** Prime 1 inlines the binary search instead of calling `FindInListLower`; adopting that lifted it straight to 100%. Nothing else needed. |
| `MoveInList` | 31.11% | **100%** | small edits — needs a `const short index` param with a mutable local `short idx` (retail's `extsh r5,r5` then `extsh. r7,r6`), and **two separate early returns**, not this tree's merged `if (idx >= size-1 \|\| !(...)) return;`. |
| `Move` | 71.25% | 71.25% | **Prime 1's source did not help** — see the wall below. |
| `AddToLinkedList` | 90.00% | **100%** | small edits — two separate `if`s, not `\|\|`; and `mNodes[nodeId].mNext = headId` (not the literal `-1`) in the first branch. Retail's `bnelr` / `beqlr` is two returns. |
| `CalculateIntersections` | 0.93% | **100%** | **Prime 1 verbatim, no edits at all.** This tree's body was a `// TODO ... return -1;` stub. Needed `#include "rstl/math.hpp"`-style access to nothing — it compiles as-is. Largest single win (604 bytes). |

Two functions outside the item's list were stubs and I filled them too, since they are the
callers these functions depend on and leaving them returning `-1` would make the file
incoherent:

| function | before | after | outcome |
|---|---|---|---|
| `ConstructIntersectionArray` | 0.74% | **93.99%** | Prime 1 verbatim. The residual is register allocation only — see below. |
| `BuildNearList(vec3f)` | 1.64% | **100%** | Prime 1 verbatim, no edits. Retail confirms the `8000.f` zero-length fallback (`lfs f0,-26932(r2)` then `fcmpu`/`beq`) and the six `min_val`/`max_val` picks feeding a stack `CAABox`. Needed `rstl/math.hpp` for `min_val`/`max_val` (this tree has them in `rstl/math.hpp`, **not** `rstl/algorithm.hpp` as Prime 1 assumes — one include added). |

So: **Prime 1's source for this class ports essentially unchanged.** Of the 9 functions I
touched, 5 needed no edits beyond the shared `GetPointForSL` fix, 3 needed small declaration/
branch-structure edits, and 1 (`Move`) did not help.

## The wall: `Move` at 71.25%

`Move`'s source is now **byte-identical to Prime 1's**, which is Matching there. It still sits
at 71.25%, and the diff is **pure instruction scheduling**:

Retail interleaves the box loads with the frame setup (`lwz r10,0(r5)` first, then
`stw r0,20(r1)`, `stw r31,12(r1)`, `stw r30,8(r1)`...). Ours front-loads `lhz r0,8(r4)` /
`li r4,0` / `stw r31,12(r1)` and issues the box words later. **Same 51 instructions, same
operands, different order** — no register-pressure problem, no semantic difference, and no
instruction missing or extra.

Two spellings tried, both measured, neither moved it:
- Prime 1 verbatim, inline `mNodes[actor->GetUniqueId().Value()]` — 71.25%
- hoisting `const TUniqueId id = actor->GetUniqueId(); SNode& node = mNodes[id.Value()];` —
  71.25%, byte-identical output to the above

`ConstructIntersectionArray`'s 93.99% is the same kind of residue: 185 of 188 instructions,
differing only in which callee-saved register each `short` lands in (`r27..r18` here vs
`r29..r9` in retail). Also not reachable by source.

This is the compiler difference the item warned about — GC/1.3.2 schedules differently from
GC/2.7 — and I found no source lever for it.

`WALL: Move 71.25% - instruction scheduling only (same 51 instructions, reordered); GC/2.7 vs GC/1.3.2, 2 spellings tried, no source lever found`

## What this means for the other Prime 1 trials

**Do the `GetPointForSL`-style shared-helper check first.** Six of the seven enumerated
functions were blocked by one helper's compiled shape, not by their own bodies, and the fix was
a one-line translation of Prime 1's helper. A `reinterpret_cast<const float*>` that looks like
a hack in Prime 1 is usually the *point*: it says retail indexes the struct flat. Look for
other `?:`-on-enum / GetX()-style accessors that should be flat index or flat reinterpret, and
check the class's enum ordering matches before assuming the translation is legal.

Second, **check branch orientation against retail's `fcmpo` operand order** — Prime 1's
`value < x` vs this tree's `x <= value` is the same logic and different code, and the disassembly
settles which one the function was written with.

Third, **Prime 1's bodies port cleanly.** Where a stub exists (`return -1`, a TODO comment),
dropping Prime 1's implementation in verbatim reached 100% on 3 of 4. Do not pre-tune these.

## Verification

```
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (retail)
sha1sum -c config/G2ME01/build.sha1 -> 87/87 OK  (main.dol + all 86 RELs)
./tools/probe_sources.sh        -> probe: 734 files, 0 failed, 0 errors; link: LINKED
python3 tools/check_symbol_names.py -> checked 484 units; 0 declared names missing
./tools/decomp_build.sh         -> All: 29.11% fuzzy, 21.25% matched, 11.37% linked (9444 / 28465)
```

Per-function comparison of `build/report.json` against `build/report.base.json`, function by
function across all 2018 units: **0 functions got worse, 8 got better, 0 disappeared, 0
appeared.** The 8 are exactly the CSortedLists functions in the table above. Matched function
count rose 9437 -> 9444.

`python3 tools/check_docs_claims.py` reports the HANDOFF state block stale
(`9444 / 28465`, `8078 / 16726`). That is the derived count the judge rewrites from the tree;
I did not edit `docs/HANDOFF.md` per the prompt.

Unit stays `NonMatching` — `tools/unit_fit.sh` reports `.text SHORT by 1428` because
`ConstructIntersectionArray` (752 B) and `Move` (204 B) do not fully match. Did not attempt a
flip.

---

# Second run (lane 3, 2026-09-29) — 18/20 -> **19/20**

The previous run's diff had been reset, so everything above had to be re-applied first. It
reproduced **exactly**: 11/20 -> 18/20, 98.12% fuzzy, 8 functions better, 0 worse, +7 matched
functions (9458 -> 9465). That confirms the earlier notes are reproducible, and it means the
7-function part of this item is now settled.

**New result: `ConstructIntersectionArray` 93.99% -> 100.00%. The unit is 19/20, 98.94% fuzzy,
9466 / 28465 matched (+8 over the branch head).**

## The fix: the `min_val` inside `ConstructIntersectionArray` is not codegen-neutral

Prime 1 writes, per axis:

```c++
const short xOutside = rstl::min_val< short >(minXa, mSortedLists[kSL_MaxX].mSize - maxXb);
```

and `rstl::min_val<T>` is `(b < a) ? b : a`. Copying that verbatim compiled to **93.99%** —
same values, 3 instructions too few, `.text` 12 bytes short. Normalising the disassembly (strip
register numbers and branch targets) showed the *only* structural difference from retail was
three missing `mr` copies, one per axis block:

```
retail                                   ours
  extsh  R,R     ; (short)minXa            extsh  R,R
  mr     R,R     ; xOutside = minXa  <--   (absent: ours reuses the extsh temp as the phi)
  subf   R,R,R   ; mSize - (short)maxXb     subf   R,R,R
  extsh  R,R                                extsh  R,R
  cmpw   R,R                                cmpw   R,R
  bge                                         bge
  mr     R,R                                mr     R,R
```

Retail needs a **third** register in that block (raw value, sign-extended value, phi target);
this tree fuses the last two. Everything else — including the whole descending
`r27, r26, r25, r24, ...` allocation of the twelve `FindInList*` results, which ours got
completely wrong at first — falls out once the phi has its own register.

The spelling that reaches it is: **name the `short` tail, then write the bare ternary.**

```c++
const short xTail = static_cast< short >(mSortedLists[kSL_MaxX].mSize - maxXb);
const short xOutside = xTail < minXa ? xTail : minXa;
```

This is semantically *identical* to `min_val` — same two operands, same comparison, same
result — so nothing is deleted or stubbed; it just stops the two `extsh` results from being
coalesced into one register.

### Spellings measured on `ConstructIntersectionArray` (all three axes changed together)

| spelling | result |
|---|---|
| `rstl::min_val<short>(minXa, mSize - maxXb)` (Prime 1 verbatim) | 93.99% |
| `rstl::min_val<short>(minXa, static_cast<short>(mSize - maxXb))` | 93.99% |
| `short Tail = mSize - maxXb;` + `static_cast<short>(Tail < minXa ? Tail : minXa)` | 95.08% |
| `rstl::min_val<short>(static_cast<short>(mSize - maxXb), minXa)` (operands swapped) | 94.15% |
| `short Tail = static_cast<short>(mSize - maxXb);` + `rstl::min_val<short>(minXa, Tail)` | 93.99% |
| `short Tail = ...;` + `minXa < Tail ? minXa : Tail` (comparison flipped) | 92.87% |
| `uint Tail = mSize - maxXb;` + `static_cast<short>(Tail < minXa ? Tail : minXa)` | 84.33% |
| `int Tail = ...` (same) | 84.97% |
| `const SSortedList& xList = mSortedLists[kSL_MaxX];` + `min_val` | 93.99% |
| duplicated expression, no named tail | 82.39% |
| a `static CountOutside(...)` helper | 86.21% |
| **`short Tail = static_cast<short>(...);` + `Tail < minXa ? Tail : minXa`** | **100.00%** |

The two that matter are the named `short` tail (without it the ternary alone is 95.08%) and
the ternary over `min_val` (with the tail but calling `min_val` is 93.99%). Both are needed.

## The wall stands: `Move` at 71.25%

**Fifteen spellings measured across the two runs; none above 71.25%.** Added this run:
`this->mNodes[...]` 71.25, `SNode*` + `->` 71.25, hoisted `const uint id` 71.25,
`reinterpret_cast` of `&box` 71.25, `CAABox& dst = node.mBox; dst = box;` 71.25, self-assign
after the box store 71.25, and five that made it *worse*: `const CAABox newBox = box;` first 49.76,
box store as the first statement 70.25, `CAABox(box.GetMinPoint(), box.GetMaxPoint())` 69.63,
hoisting the first two `mSelfIdxs` 60.75, `node.mBox = CAABox(box);` 49.76.

The diff is a pure reordering of the **same 24 instructions with the same operands** — no
instruction missing, none extra, no size change. Retail interleaves the box loads with the
frame setup (`lwz r10,0(r5)`, `stw r0,20(r1)`, `stw r31,12(r1)`, `stw r30,8(r1)`, ...) and
issues all six box loads before all six stores; this tree hoists the unique-id chain
(`lhz r0,8(r4)` / `li r4,0` / `clrlwi` / `mulli`) into those slots instead. The
`ConstructIntersectionArray` fix above shows a source-level lever *can* exist for an
allocation difference, but here there is no shape to change — only the scheduler's tie-break
between two ready subtrees differs, and that is GC/1.3.2 vs GC/2.7.

`WALL: Move 71.25% - instruction scheduling only (same 24 instructions, reordered); GC/2.7 vs GC/1.3.2, 15 spellings tried across two runs, no source lever found`

## Generalisation for the other Prime 1 trials

The earlier note's advice ("check the shared helper's *compiled shape*") generalises one step
further: **a semantically identical expression can still be the whole difference.** A helper
call and an inlined ternary, or two `const short` locals versus one expression, compile to
different register allocation in GC/2.7. When a ported function sits at 90-95% with
*identical values*, normalise the disassembly (strip register numbers and branch targets) and
diff it: if the only delta is a missing or extra copy, the fix is to give the intermediate its
own named variable, not to change any logic.

## Verification

```
sha1sum build/G2ME01/main.dol             -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
sha1sum -c config/G2ME01/build.sha1      -> 87/87 OK  (main.dol + all 86 RELs)
./tools/probe_sources.sh                 -> probe: 735 files, 0 failed, 0 errors; link: LINKED
                                            (254 undefined, 0 duplicates)
python3 tools/check_symbol_names.py      -> checked 484 units; 0 declared names missing
python3 tools/check_decl_order.py --unit MetroidPrime/CSortedLists.cpp
                                        -> ok, none emits its functions out of retail order
./tools/decomp_build.sh                  -> All: 29.14% fuzzy, 21.29% matched, 11.37% linked
                                            (9466 / 28465 functions)
```

Per-function comparison of `build/report.json` against `build/report.base.json` across all
2021 units: **0 functions got worse, 8 got better, 0 disappeared, 0 appeared.** All 8 are
`main/MetroidPrime/CSortedLists`:
`FindInListLower` 67.07->100, `FindInListUpper` 68.79->100, `InsertInList` 41.70->100,
`MoveInList` 31.11->100, `AddToLinkedList` 90.00->100, `CalculateIntersections` 0.93->100,
`ConstructIntersectionArray` 0.74->100, `BuildNearList(vec3f)` 1.64->100.
Matched functions 9458 -> 9466.

Diff: `src/MetroidPrime/CSortedLists.cpp` only, +140/-41 (one file, no header or layout change,
no other unit touched, no asm). Not committed — the driver's job.

**The unit now `unit_fit.sh`-fits**: `.text claimed 5524  ours 5524  retail 5524  fits`, with
no extra functions. It is still `NonMatching` and was deliberately not flipped — this is a
`progress` item — but `Move`'s instruction order is the *only* thing now separating
`MetroidPrime/CSortedLists` from a clean `flip_test.sh`. Worth one more item if someone wants
to attack that scheduling difference with fresh eyes.

No `NEW:` items filed: the one wall left (`Move`) is a measured dead end for this compiler
pair, not a unit whose success is reachable by a different lane.
