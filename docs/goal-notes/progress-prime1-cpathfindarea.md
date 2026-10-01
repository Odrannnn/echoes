---

# Fifth run (lane 2, 2026-10-01) — 33 → 34 of 38

**Result: `goal_check.sh build/goal/item.json` → PASS.** The unit's `matched_functions` went
**33 → 34 of 38**; project-wide `matched` went 11407 → 11408, `linked` unchanged at 5523.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11407 -> 11408   linked 5523 -> 5523
  ok    check_symbol_names.py
  ok    All:  32.79% fuzzy, 25.53% matched, 11.98% linked (11408 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindArea: 33 -> 34 / 38 functions
  ok    no asm added
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
Diff: `src/MetroidPrime/PathFinding/CPathFindArea.cpp`, `config/G2ME01/symbols.txt` (one rename, no
other edit), plus the state block the gate rewrites in `docs/HANDOFF.md`. **No header change** — the
per-`T` mechanism that would have needed one is not required (see below). No `configure.py`, no
`files.cmake`, no `splits.txt`, no `tools/`. No assembly. `check_decl_order.py` prints `ok` after the
rename.

Unit `.text` 95.89% → **98.19%** fuzzy, `matched_code` 66.52% → **68.80%**, `.sdata2` still 100%.

Verified by diffing `report.json` against `build/goal/judge/report.base.json`: **no function anywhere
got worse**, one name changed (`fn_8013FE2C` → the mangled name, at 100.00%), nothing else moved.

## Per function: before → after (re-measured on this tree)

| function | before | after | what changed |
|---|---|---|---|
| `__ct<7CPFArea>__16CFactoryFnReturnFP7CPFArea` (was `fn_8013FE2C`) | 0.00% | **100.00%** | the rename + out-of-line `~auto_ptr<CPFArea>` |
| `__ct__7CPFAreaFRCQ24rstl12auto_ptr<Uc>i` | 98.07% | **98.08%** | `version`/`maxRegionNodes` declared before the stream |
| `FindClosestReachablePoint__…UiUi` | 97.86% | 97.86% | untouched (previous run's wall) |
| `PathExists__…Ui` | 95.20% | **95.68%** | mutate `destinationIndex` in place (partial) |
| `fn_80140120` | 0.00% | 0.00% | re-confirmed unfixable (below) |
| the 33 other functions | 100% | 100% | unchanged |

## The item's headline: `fn_8013FE2C` was never "unwritten", it was unnamed

Both earlier runs and the fourth all recorded `fn_8013FE2C` as 0.00% and the fourth characterised it
as 12 bytes from matching. Neither noticed the more basic thing: **it was also unpaired.** objdiff
pairs functions by name, `dtk` could not name it, and our object emits it under a C++ mangled name —
the same mechanism the third run got 15 functions from. The rename alone, before any code change,
takes it from 0.00% to **91.20%**, and the remaining 12 bytes are one teardown.

```sh
# one line in config/G2ME01/symbols.txt, name read off our own object with nm
__ct<7CPFArea>__16CFactoryFnReturnFP7CPFArea = .text:0x8013FE2C; // type:function size:0xA4
```

The fourth run's guess at the name was right and is confirmed by the call graph:
`FPathFindAreaFactory` (0x80140054) calls it once, and `include/Kyoto/CFactoryMgr.hpp:19` is
`CFactoryFnReturn(T* ptr) : obj(TToken< T >::GetIObjObjectFor(ptr).release()) {}` — so it is
`CFactoryFnReturn<CPFArea>::CFactoryFnReturn(CPFArea*)` and the mangled name `nm` prints is
`__ct<7CPFArea>__16CFactoryFnReturnFP7CPFArea`.

### The last 12 bytes: retail calls `~auto_ptr<CPFArea>`, we inlined it

```
retail 0x8013FEF8                     ours, same function
  addi    r3,r1,16                      lbz     r0,16(r1)
  li      r4,-1                         cmplwi  r0,0
  bl      __dt__auto_ptr<CPFArea>Fv     beq     0x10
                                          lwz     r3,20(r1)
                                          li      r4,1
                                          bl      ~CPFArea
```

The fourth run tried `RSTL_AUTO_PTR_OUT_OF_LINE` (a macro in `include/rstl/auto_ptr.hpp`) and measured
it at 132 bytes instead of retail's 164: "the compiler drops the work rather than calling the weak
copy". **That conclusion was right about the macro and wrong about the cause**, and this run's
measurement says why. The macro moves *every* `auto_ptr` destructor in the TU out of line, including
the `auto_ptr<TObjOwnerDerivedFromIObj<CPFArea>>` one immediately above it — which retail **inlines**
(its `bctrl` at 0x8013FEF0 is right there in the middle of the function). So the macro produced two
calls where retail has one call and one inline, and 132 < 164 is not "the work was dropped", it is
"the wrong teardown went out of line too".

The choice is therefore **per `T`**, and a whole-class macro can never express it. What does work, with
no header change at all, is a full explicit specialisation of `auto_ptr<CPFArea>` in the TU: same
members, destructor declared in-class and defined after it. mwcceppc is built with
`-inline deferred,noauto`, which inlines only what the `inline` keyword asks for, so a destructor
defined outside the class is not a candidate — that is the whole mechanism, and `RSTL_SINGLE_PTR_OUT_OF_LINE`
in `include/rstl/single_ptr.hpp:23` is the same trick one class up. 164 bytes, byte-identical.

| spelling of the temporary's teardown | bytes | % |
|---|---|---|
| rename only, no code change | 176 | 91.20 |
| `RSTL_AUTO_PTR_OUT_OF_LINE` (the fourth run's macro, moves **all** `auto_ptr` dtors out of line) | 132 | 68.78 |
| **full `auto_ptr<CPFArea>` specialisation with the destructor defined out of class** | **164** | **100.00** |

I also tried and rejected, all measured, all reverted: a member-only specialisation
(`template <> auto_ptr<CPFArea>::~auto_ptr()` — mwcceppc emits the *class* destructor anyway, 176 B);
a partial-specialisation `auto_ptr_dtor<T, bool>` base-class split in the header (does not compile:
`object 'rstl::auto_ptr<CPFArea>::~auto_ptr()' redefined`); `#pragma inline_max_size(N)` for N in
0..200 immediately before `FPathFindAreaFactory` (no effect at any value — the pragma does not reach
an already-instantiated template); a named `rstl::auto_ptr<CPFArea> tmp(ptr)` local instead of the
implicit temporary (176 B, unchanged); an explicit `auto_ptr<T>(ptr)` argument in
`CFactoryMgr.hpp:19` (unchanged); `#pragma noinline`-shaped probes. **A header-level
`auto_ptr_dtor_out_of_line<T>` trait is the tidier shape and would work, but it is not needed and not
worth the blast radius across every unit that uses `rstl::auto_ptr`** — put the full specialisation
here if it is ever needed for another `T`.

### Decl order: the specialisation has to sit *above* `~CPFArea`, not below

`gate.sh`'s `decl-order` check failed on the first pass of this item and only that. mwcceppc emits in
reverse source order, so the out-of-line `~auto_ptr<CPFArea>` has to be declared **before**
`CPFArea::~CPFArea()` to land at 0x801402C0, between `GetNewDerivedObject` (0x80140124) and
`PointPathExists` (0x80140324). Declared at the end of the file next to `FPathFindAreaFactory` it lands
immediately after `__ct<CPFArea>__16CFactoryFnReturnFP7CPFArea` and the check reports the swap.
`python3 tools/check_decl_order.py --unit MetroidPrime/PathFinding/CPathFindArea` prints `ok` now.
This is the third run's lesson again, and again it surfaced *because* of a rename.

## `PathExists` 95.20% → 95.68% (kept, not matched) — the fourth run's wall moved again

The fourth run's best was `low`/`high` seeded from the arguments, 95.20%, and it noted retail emits
**one** compare and **one** branch for the pair. Mutating the parameter in place is the shape that
gives one branch without a named `high`:

```cpp
int low = sourceIndex;
if (sourceIndex > destinationIndex) {
  low = destinationIndex;
  destinationIndex = sourceIndex;
}
```

which drops the `mr` that materialising the `high` phi needed. This is the same lesson the fourth run
recorded for `PointConnectionsTest` (`mr r7,r4` vs `mr r0,r4`), one function away. Measured this run,
all 164 bytes, all the same 41/42 instructions:

| spelling | % |
|---|---|
| `low`/`high` seeded, `if (sourceIndex > destinationIndex) { low = …; high = …; }` (fourth run's best) | 95.20 |
| **`low` seeded + `destinationIndex` mutated in place** | **95.68** |
| `const int high = destinationIndex;` after the branch | 95.68 |
| `connections` selected through a `const uint*`/`prereserved_vector*` | 95.68 |
| `const int numRegions` | 95.68 |
| `destination->GetIndex()` read before `source->GetIndex()` | 95.12 |
| connections reference first / between the `GetIndex()` calls | 81.34 / 66.41 |
| `numRegions` read last, or via `mRegions.size()` after the swap | 87.66 |
| swap the region *pointers* then index | 85.98 |
| `low`/`high` straight from `GetIndex()` with no named indices | 85.98 |
| explicit three-term `low`/`high` temporaries | 94.39 |
| branchless `?:` ternaries for `low` and `high` | 87.20 |
| `unsigned` for `low`/`high` | does not compile (`rstl` has no `min_val`/`max_val` in scope here) |

What is left is still register allocation and load order: retail loads `n` into r7, `src` into r0,
`dst` into r8 and gets `min` in r4 with no copy before the connections `beq`; the best spelling loads
`dst` into r5, needs `mr r8,r5` to give `destinationIndex` its own register, and keeps `min` in r6.

## `FindClosestReachablePoint`: one `GetCentroid()` call instead of two

Same 97.86%, but the source is now one `GetCentroid()` feeding both uses rather than two calls:

```cpp
const CVector3f& centroid = region.GetCentroid();
const CVector3f& delta = centroid - point;
```

This is the second run's `SetTransform` lesson applied here and it is **score-neutral** (97.86 either
way, 468 bytes, 117 instructions both). I kept it because it removes a redundant load and because it
is the shape retail's register allocation implies, not because it measured better. What is left is the
second run's finding unchanged: retail loads `c.y, c.x, c.z` into `f2, f1, f3` and **re-loads** the
centroid into `f31, f30, f29` for `result` (`lfs f31,40(r21)`); ours loads into `f5, f3, f4` and
`fmr`s them. Re-measured variants, all 468 bytes, none better:

| spelling | % |
|---|---|
| one `GetCentroid()` call, `result = centroid` (kept) | 97.86 |
| two `GetCentroid()` calls (the tree before this run) | 97.86 |
| `const CVector3f delta = region.GetCentroid() - point;` (by value) | 92.50 |
| `(region.GetCentroid() - point).MagSquared()` inline | 97.86 |
| `result = region.GetCentroid()` assigned before `closestDistanceSq` | 97.78 |
| `dx*dx + dy*dy + dz*dz` written out from the components | 92.50 / 93.33 |
| explicit `const CVector3f& centroid` + `delta` + `result = centroid` | 97.86 |

## `__ct__7CPFArea` 98.07% → 98.08%: the two stack slots were swapped

Retail's constructor puts `version` at **8(r1)** and `maxRegionNodes` at **12(r1)**, with the
`CPFMemoryStream` at 16(r1) (`stw r0,16(r1) ; stw r28,20(r1) ; stw r0,24(r1)` — its three members).
We had `version` at 12 and the loop variable at 8, so `fn_80141594` took `addi r3,r1,12` where retail
takes `addi r3,r1,8`. Declaring `maxRegionNodes` before `version` and both **before** the stream fixes
the slot order. Measured, all 1524 bytes: stream-then-locals 98.07, locals-then-stream 98.08,
`version` first 98.07. One hundredth of a percent — the rest of this function is still the
`r29`/`r30` allocation and the two `mr`s the `mPointSearchState` null test needs
(`mr. r4,r3 ; beq ; mr r4,r30 ; bl ; mr r4,r3` vs our `mr. r0,r3 ; beq ; … ; mr r4,r3`). Tried and
measured on the `mPoints[i].Fixup()` loop, which retail strength-reduces to a byte offset in a
register it starts at zero: `CPFPoint* point = &mPoints[0]; ++point` gives **96.61%** (it costs the
`Fixup` argument a register), `CPFPoint* const points = &mPoints[0]; points[i]` gives 96.69%, and both
`(&mPoints[0])[i]` and a named `CPFPoint& point = mPoints[i]` give 98.08, i.e. unchanged.

## `fn_80140120` (84 B): confirmed unfixable, and the reason is stronger than a duplicate

Fourth run's note: renaming it to `__dt__Q24rstl36vector<i,…>Fv` gives a linker duplicate, because
`main.o` already defines that name. Re-measured here, and the **fix the note did not try makes the
DOL stop reproducing retail**, which is worth recording so nobody tries it again:

| change | result |
|---|---|
| rename only | `mwldeppc: multiply-defined: 'rstl::vector<int>::~vector()' in CPathFindArea.o, previously defined in main.o` |
| rename + `scope:weak` on the new entry | DOL sha1 becomes `cb7368e2ce1f474023d739a4ced407bf6d5ffa40` (wrong) |
| rename + `scope:weak` on **both** copies (0x80009810 and 0x80140120) | DOL sha1 `6ff9a54501fb14c362b17ecc66f41794c9e9c007` (wrong), 87 REL checksums fail |

So the weak marking does resolve the duplicate, and it changes which copy mwldeppc keeps, and retail
keeps the other one. This is a hard stop, not a spelling.

## WALL:

WALL: PathExists__7CPFAreaCFPC9CPFRegionPC9CPFRegionUi 95.68% - 24 further spellings this run on top
of the previous three runs' 18 (superset); the instruction sequence is retail's, and the last bytes are
that ours loads `dst` into r5 and needs `mr r8,r5` where retail loads it into r8 directly and keeps
`min` in r4.

WALL: FindClosestReachablePoint__7CPFAreaFRQ24rstl30reserved_vector<P9CPFRegion,8>RC9CVector3fUiUi
97.86% - 8 spellings; retail re-loads the centroid into `f31/f30/f29` for `result` where ours `fmr`s
the values already in `f5/f3/f4`, and no source spelling found changes which FP registers hold them.

I did not write a `WALL:` for `__ct__7CPFArea`: 98.08% of 1524 bytes and the remaining difference is
the `r29`/`r30` allocation in a loop, which I measured three spellings of rather than a wall.

`fn_80140120` is characterised above rather than as a `WALL:` — the reason is measured (the weak
marking that fixes the duplicate breaks the DOL sha1), not a guess.

## NEW:

None filed. All of the remaining work is still inside `MetroidPrime/PathFinding/CPathFindArea`, this
item's own target, so requeue `progress-prime1-cpathfindarea` rather than open a new one.

## Lesson (not a NEW item)

* **A 0.00% function is unpaired before it is wrong.** Two runs characterised `fn_8013FE2C` by its
  bytes — "12 bytes from matching", "the teardown shape" — when the name alone was worth 91.20% and
  the remaining 12 bytes were a second, separate problem. Check `nm` on our object for a candidate
  name *before* analysing the disassembly, exactly as the third run's lesson says; the note it
  contradicts is that "0.00%" reads as a disassembly problem.
* **A whole-class opt-in cannot express a per-instantiation decision.** `RSTL_AUTO_PTR_OUT_OF_LINE`
  was measured, found to produce 132 bytes against retail's 164, and concluded to be dropping work.
  It was moving the *other* instantiation out of line as well. When retail's shape is "this one is a
  call, the identical one above it is inlined", the source of the difference is the type, and a macro
  on the class is the wrong instrument — a full explicit specialisation in the TU is both sufficient
  and header-free.