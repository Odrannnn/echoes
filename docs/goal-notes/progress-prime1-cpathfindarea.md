# progress-prime1-cpathfindarea — `MetroidPrime/PathFinding/CPathFindArea`

**Result: `goal_check.sh build/goal/item.json` → PASS.** The unit's `matched_functions` went
**10 → 11 of 38**; `CPFArea::FindClosestRegion` is now byte-identical to retail (100.00%).

```
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10355 -> 10356   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.47% fuzzy, 23.89% matched, 11.83% linked (10356 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindArea: 10 -> 11 / 38 functions
  ok    no asm added
```

Diff: `src/MetroidPrime/PathFinding/CPathFindArea.cpp` only (plus the state block the gate
rewrites in `docs/HANDOFF.md`). No header, `configure.py` or `tools/` change. No assembly.

## Per function: before → after (numbers from `build/report.json`, re-measured)

| function | before | after | what changed |
|---|---|---|---|
| `FindClosestRegion__7CPFAreaFRC9CVector3fUiUif` | 96.85% | **100.00%** | the real fix: three separate `GetObstructionCount` tests, nested |
| `FindClosestReachablePoint__…UiUi` | 88.14% | 91.87% | `FLT_MAX` as a local literal (`lfs f0,disp(r2)` instead of a `__float_max` symbol pair) |
| `.sdata2` section | 85.71% | **100.00%** | same: the pool is now retail's 4 words in retail's order |
| `PathExists__…Ui` | 90.07% | 90.07% | nothing found (wall, below) |
| `FindRegions__…CVector3fUiUib` | 88.80% | 88.80% | nothing attempted |
| `FindRegions__…CAABoxUiUib` | 94.73% | 94.73% | nothing attempted |
| `__ct__7CPFAreaFRCQ24rstl12auto_ptr<Uc>i` | 75.90% | 74.90% | **-1.00, see the caveat** |
| `SetTransform__…` | 77.63% | 77.63% | untouched |
| 24 × `fn_8014xxxx` | 0.00% | 0.00% | untouched |

Unit: `61.04%` fuzzy was `60.80%` at the start, `61.08%` now; `matched_code` 18.33% → 25.96%.

## The fix: the obstruction chain in `FindClosestRegion`

Prime 1's `FindClosestRegion` (read-only clone at
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/PathFinding/CPathFindArea.cpp:152`)
has no obstruction test at all — Echoes added one, and this repo's guess for it
(`CPFRegion::IsObstructed`, `include/MetroidPrime/PathFinding/CPathFindRegion.hpp:134`) does not
match what retail compiled. Retail at `0x80140888`:

```
80140888: mr    r3,r20 ; li r4,2 ; bl GetObstructionCount   ; kPFO_Unknown2
80140894: cmpwi r3,0 ; bgt 0x80140988                      ; obstructed -> reject
8014089c: cmplwi r28,0 ; beq 0x801408b8                    ; r28 = flags & 0x100
801408a4: mr r3,r20 ; li r4,0 ; bl GetObstructionCount      ; kPFO_Unknown0
801408b0: cmpwi r3,0 ; bgt 0x80140988
801408b8: cmplwi r27,0 ; beq 0x801408d4                    ; r27 = flags & 0x200
801408c0: mr r3,r20 ; li r4,1 ; bl GetObstructionCount      ; kPFO_Unknown1
801408cc: cmpwi r3,0 ; bgt 0x80140988
801408d4: ... IsPointInsidePaddedAABox
```

Two things had to be right, and each one alone leaves the function at 99.9%:

1. **The flag must be *set* for its count to be tested.** `beq` at `+0x18` jumps *over* the
   `kPFO_Unknown0` call to the next test, i.e. a **clear** `0x100` skips the call. This is the
   opposite of the `|| (flags & 0x100)` form, which compiles to `bne +0x18` (same target, inverted
   condition — 99.78%). The correct `IsObstructed` would be
   `c2 > 0 || (flags & 0x100) == 0 || c0 > 0 || (flags & 0x200) == 0 || c1 > 0`.
2. **The tests have to be nested `if`s, not one flat `&&` chain.** With a flat chain the
   clear-flag case is a *rejection*, so the compiler sends that branch to the shared reject label
   (`beq +0xe8`) instead of to the next test (`beq +0x18`) — 99.93%, 2 instructions from 100%.

Both spellings measured, all in this run:

| spelling | % |
|---|---|
| `!IsObstructed(flags)` as the header had it (baseline) | 96.85% |
| flat `&&` with `(flags & 0x100) \|\| count <= 0` (waive sense) | 99.78% |
| flat `&&` with `(flags & 0x100) != 0 && count <= 0` (require sense) | 99.93% |
| **nested `if`s, `(flags & 0x100) == 0 \|\| count <= 0`** | **100.00%** |

Note for the next run: mwcceppc (GC/2.7, this flag set) compiles `(x & (1 << n)) != 0` to
`rlwinm rD,rS,0,31-n,31-n` — **the bit index is `31 - n`, not `n`**. Verified with
`tools/probe_cc.sh` on a four-case probe: `& 0x100` → `MB=23`, `& 0x200` → `MB=22`,
`& 0x800000` → `MB=8`, `& 0x400000` → `MB=9`. So retail's `rlwinm r28,r18,0,23,23` is the source
mask `0x100`, not `0x800000`. Reading an `rlwinm` as a mask without this inversion costs an hour.

`CPFRegion::IsObstructed` in the header is therefore wrong for every caller, and the two
`FindRegions` overloads carry the same chain at `0x80140C4C` and (box) `0x80140Bxx` with the same
`beq`-over-the-call shape, so they need the same nested rewrite. I did not do it: it is not one
of the item's three named functions, the nested form cannot come from a shared `IsObstructed`
call, and it would touch `CPathFindSearch.cpp:252` (a different unit) for no measured gain.

## The `FLT_MAX` finding (unit-wide, worth keeping)

`libc/float.h:10` defines `FLT_MAX` as `(*(float*)__float_max)`, so every TU that writes `FLT_MAX`
emits an `R_PPC_ADDR16_HA/LO` pair against an external symbol and puts **nothing** in its
`.sdata2`. Retail's `CPathFindArea` unit object references no `__float_max` at all and its
`.sdata2` is `7f7fffff 40400000 00000000 38d1b717` — `FLT_MAX`, `3.f`, `0.f`, `1e-4f`, in that
order (`build/G2ME01/obj/MetroidPrime/PathFinding/CPathFindArea.o`, `lbl_8041C188..194`).
`extern/sdk/libc/float.h:9` has the literal retail used. Re-defining the macro to the literal for
this one TU makes our `.sdata2` byte-identical to retail's, and the `R_PPC_EMB_SDA21` addends then
line up one-for-one with retail's (checked: 7 relocs each side, same order, same +0/+4/+8/+12).
Without it our constants land four bytes late (`0x8041C18C` instead of `0x8041C188`) and every
`lfs f0,disp(r2)` in the unit is 4 off. `FindClosestRegion` and `FindClosestReachablePoint` both
load `FLT_MAX`/`3.f`/`0.f` this way; the constructor loads `FLT_MAX` at +8. The retail DOL
references `__float_max` from `Kyoto/Math/CAABox.o`, `WorldFormat/CAreaOctTree_Tests.o` and
`Runtime/float.o` only, so both spellings coexist upstream — a per-TU choice, not a global fix.

## Caveat: the constructor went 75.90% → 74.90%

`report_diff.py` prints it as `WORSE … __ct__7CPFArea… 75.90% -> 74.90%`. It is a **signal, not a
failure** (`tools/report_diff.py:298` only promotes `WORSE` to a hard failure when the function's
unit was `Matching`, and this one is not), and the gate passed. The trade is deliberate: the
`FLT_MAX` literal buys +3.73 on `FindClosestReachablePoint` and takes `.sdata2` from 85.71% to
100%, for −1.00 here. The cause is not the constant: retail's constructor prologue is
`stwu r1,-160(r1)` plus four separate `stw` of r31/r30/r29/r28, ours is `stwu r1,-144(r1)` plus
one `stmw r27,124(r1)` — we need one fewer callee-saved register, so the frame and the register
save both differ. That is a real source difference, not noise.

## What the constructor still needs (not attempted — out of budget)

`__ct__7CPFArea` is 74.90% of 1524 bytes and the diff is structural, not cosmetic:

* **The memory stream is a real object in retail.** Retail's cursor lives in a stack slot and is
  re-loaded after every read — `lwz r4,24(r1) … stw r4,24(r1)` appears ~20 times in the function
  (`0x80140EE4` onwards). Ours inlines `CPFMemoryStream` and keeps the cursor in `r29`. Retail also
  *calls* the stream methods: `fn_80141594` (24 B, called at `+0x174`), `fn_801418A4` (184 B,
  `vector<CPFRegionData>::reserve`), `fn_80141528` (108 B), `fn_801413C4` (176 B, `resize`), all of
  which are in this unit and all at 0.00%. So the fix is to make `CPFMemoryStream` a genuine
  local object whose methods are not fully inlined, and to stop routing the region-data
  reserve/resize through the external `reserve__…`/`resize__…` templates.
* **The point-search workspace is never constructed.** Retail ends with
  `__nw__FUlPCcPCc(0x24, "CPathFindArea", 0)`, a null check, a call at `0x801F8B48` and then
  `fn_8014137C` storing into `mPointSearchState` at +0x148 (`0x80141328`–`0x80141354`, 44 bytes).
  Our source has a `// TODO: identify the point-search workspace's native class` comment there and
  does nothing. `CPFPointSearchState` already exists in the header with `CHECK_SIZEOF(…, 0x24)`,
  so `mPointSearchState = new CPFPointSearchState(numPoints)` is the behaviour; it needs the ctor
  bound to whatever `0x801F8B48` is (check `config/G2ME01/symbols.txt` first — `check_symbol_names.py`
  will reject a new name).
* Our constructor is 1268 bytes against retail's 1524, i.e. ~250 bytes of real work missing, all of
  it in the two items above.

## Wall

Spellings tried for `PathExists__7CPFAreaCFPC9CPFRegionPC9CPFRegionUi`, all in this run, all
164 bytes like retail (41 instructions each, same instruction sequence, different allocation):

| # | spelling | % |
|---|---|---|
| 1 | baseline: `n, src, dst`, connections ref, `rstl::swap` | 90.07% |
| 2 | connections ref *after* the swap | 66.41% |
| 3 | connections ref between `n` and the two `GetIndex()` calls | 85.98% |
| 4 | connections ref first, then `n, src, dst` | 81.34% |
| 5 | #4 with a hand-written swap instead of `rstl::swap` | 81.34% |
| 6 | #4 with `low`/`high` names instead of `rstl::swap` | 81.34% |
| 7 | `src, dst, n` (connections ref first) | 80.73% |
| 8 | select the word at the end (`(flags & 2) ? mFlyers[bit/32] : mGround[bit/32]`) | 55.44% |
| 9 | `destinationIndex < sourceIndex` | 89.95% |
| 10 | `low`/`high` names + hand-written swap, otherwise the baseline order | 90.07% |
| 11 | `std::swap` | did not compile (no `<utility>`) |

The whole remaining difference is register allocation and load order: retail loads
`n, src, dst` into r7/r0/r8 and gets `min` in r4 / `max` in r8 with **no** copies before the
connections `beq` (`cmpw r0,r8; mr r4,r0; ble; mr r4,r8; mr r8,r0`), while every spelling above
loads `src, dst, n` and needs two `mr`s hoisted above the `beq` to materialise the swap's phi.

WALL: PathExists__7CPFAreaCFPC9CVector3fUiUif 90.07% - 11 spellings, all 164 bytes and the same 41 instructions; the last 8 bytes are a register-allocation + load-order difference in the min/max swap, no spelling moved it.

## NEW:

None filed. The remaining work in this unit is the constructor's missing `CPFPointSearchState`
construction and the stream object, plus the two `FindRegions` overloads' obstruction chain —
all of it is in `MetroidPrime/PathFinding/CPathFindArea`, which is this item's own target, so the
driver can simply requeue `progress-prime1-cpathfindarea` rather than open a new one.

## Lesson (not a NEW item)

`rlwinm rD,rS,0,MB,MB` from a single-bit `&` test masks bit `31-MB`, not bit `MB`. Get this
backwards and the compiler silently produces a plausible, wrong constant for hours.

---

# Second run (lane 4, 2026-10-01) — 11 → 14 of 38

**Result: `goal_check.sh build/goal/item.json` → PASS.** The unit's `matched_functions` went
**11 → 14 of 38**; project-wide `matched` went 11340 → 11343, `linked` unchanged at 5507.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11340 -> 11343   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.62% fuzzy, 25.30% matched, 11.94% linked (11343 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindArea: 11 -> 14 / 38 functions
  ok    no asm added
```

Diff: `src/MetroidPrime/PathFinding/CPathFindArea.cpp` only (plus the state block the gate
rewrites in `docs/HANDOFF.md`). No header, `configure.py` or `tools/` change. No assembly.
Unit `.text` 61.08% → 63.11% fuzzy, `matched_code` 25.96% → 38.66%, `.sdata2` still 100%.

## Per function: before → after (re-measured on this tree, `build/report.json`)

| function | before | after | what changed |
|---|---|---|---|
| `FindRegions__…RC6CAABoxUiUib` | 94.73% | **100.00%** | obstruction chain written out instead of `IsObstructed` |
| `FindRegions__…RC9CVector3fUiUib` | 88.80% | **100.00%** | same chain, **plus** `(flags & 2) \|\| (flags & 4)` → `(flags & 6)` |
| `SetTransform__…` | 77.63% | **100.00%** | bind the loop element to a `CPFPoint&` before the two uses |
| `FindClosestReachablePoint__…UiUi` | 91.87% | 97.86% | same obstruction chain (kept, not matched) |
| `PathExists__…Ui` | 90.07% | 95.20% | `rstl::swap` → explicit `low`/`high` (partial, see below) |
| `__ct__7CPFAreaFRCQ24rstl12auto_ptr<Uc>i` | 74.90% | 74.90% | untouched |
| `.sdata2`, `FindClosestRegion`, the 8 other 100% functions | 100% | 100% | unchanged |

Nothing got worse; nothing regressed anywhere (the gate's `report diff` is clean).

## The fix: `CPFRegion::IsObstructed` is wrong for every caller

The previous run's nested rewrite of `FindClosestRegion` was right but was applied to one
caller. Writing the chain out **at each of the three remaining call sites** is what took the two
`FindRegions` overloads and `FindClosestReachablePoint` the rest of the way. Measured shape,
ours before → retail, for the box overload at `0x80140af4`:

```
  li r19,0x0            <- ours only: the bool that IsObstructed returns
  li r4,0x2 ; bl GetObstructionCount ; cmpwi r3,0
  bgt reject            vs  ours: ble          (sense inverted by the negation)
  cmplwi r30,0 ; beq +0x18
  li r4,0x0 ; bl ; cmpwi r3,0
  bgt reject            vs  ours: bgt
  cmplwi r29,0 ; beq +0x18
  li r4,0x1 ; bl ; cmpwi r3,0
  bgt reject            vs  ours: ble
  li r19,0x1 ; clrlwi. r0,r19,24 ; bne skip    <- ours only
```

`!IsObstructed(flags)` has to materialise a bool in a callee-saved register, which costs four
extra instructions and one extra saved register (`stmw r19` vs retail's `stmw r20`).
The source that matches, and is a **flat chain, not nested `if`s** — the same chain that needed
nested `if`s in `FindClosestRegion` needs them here because the accept body is shared with the
`ignoreObstructions` path, so a nested rewrite would have to duplicate it:

```cpp
(ignoreObstructions ||
 (region->GetObstructionCount(kPFO_Unknown2) <= 0 &&
  ((flags & 0x100) == 0 || region->GetObstructionCount(kPFO_Unknown0) <= 0) &&
  ((flags & 0x200) == 0 || region->GetObstructionCount(kPFO_Unknown1) <= 0)))
```

The leading `ignoreObstructions ||` is what produces retail's `clrlwi. r0,r25,24 ; bne <accept>`
— a branch *into* the middle of the chain, which no `&&`-chain spelling can express.

**Both `FindRegions` overloads are now byte-identical to retail.** `CPFRegion::IsObstructed` in
`include/MetroidPrime/PathFinding/CPathFindRegion.hpp:134` has **no caller left** in this TU.
Leave it: `CPathFindSearch.cpp` still uses it (a different unit) and it is not this item's diff.

### The `(flags & 2) || (flags & 4)` → `(flags & 6)` step (only needed after the chain rewrite)

Retail compiles the vector overload's height test as **one** mask, `rlwinm r27,r22,0,29,30`
(= `flags & 6`), with one `cmplwi`. With `(flags & 2) || (flags & 4)` the same compiler emits
**two** masks (`rlwinm 0,30,30` and `rlwinm 0,29,29`) and two tests, costing one extra
callee-saved register: 94.18%, two instructions out. `(flags & 6)` is byte-identical to retail
and is semantically the same test. The box overload already said `flags & 6` and matches. Note
the merge is not stable under unrelated edits: before the chain rewrite the same
`(flags & 2) || (flags & 4)` text produced the merged mask. Measured, all in this run:

| spelling of the height test | % |
|---|---|
| `(flags & 2) \|\| (flags & 4)`, after the chain rewrite | 94.18% |
| **`(flags & 6)`** | **100.00%** |

## `SetTransform`: one reference, not two indexings

`77.63% → 100.00%` from binding the loop element once:

```cpp
for (int i = 0; i < mPoints.size(); ++i) {
  CPFPoint& point = mPoints[i];          // <- this line is the whole fix
  point.SetPosition(transform.GetTranslation() + delta.Rotate(point.GetPosition()));
}
```

Retail strength-reduces `&mPoints[i]` **once** into a byte-offset register (`addi r30,r30,0x1c`)
and uses that single pointer for both the `Rotate` argument and the three stores
(`stfs f0,0(r31) …`). With `mPoints[i]` written twice the compiler keeps the offset for the call
and re-loads `mItems` and recomputes the address for the stores (`+040/+044`), and it drops the
frame from `stmw r27,0xac` to four separate `stw`s. `CPFPoint& point = mPoints[i]` also fixed
the floating-point evaluation order for free (`fadds f1,f30,f1` before the three stores, as
retail has it, instead of ours interleaving the z-sum between the y and z stores).
A variant with an extra `const CVector3f position = …; point.SetPosition(position);` also
reached 100%; the shorter form is what is in the tree. Prime 1 has no `SetTransform` at all, so
this one is Echoes-only and came from the measured diff, not from prime-ref.

## `PathExists` 90.07% → 95.20% (partial; the previous run's wall moved)

The previous run's 11 spellings all kept `rstl::swap`. **Replacing the swap with two named
values is what moved it**, and it is a different shape from anything in that table: retail does
**one** compare and **one** branch for the pair (`cmpw r0,r8 ; mr r4,r0 ; ble ; mr r4,r8 ;
mr r8,r0`), so the source cannot be two independent `min`/`max` calls either. What matches is
the branchy form with the fall-through being the "already ordered" case:

```cpp
int low = sourceIndex;
int high = destinationIndex;
if (sourceIndex > destinationIndex) { low = destinationIndex; high = sourceIndex; }
```

All in this run, all 164 bytes and all the same 41/42 instructions:

| spelling | % |
|---|---|
| `rstl::swap` (the previous run's baseline, re-measured here) | 90.07% |
| `rstl::min_val` / `rstl::max_val` (`include/rstl/math.hpp`) | 94.39% |
| `if (sourceIndex <= destinationIndex) { … } else { … }` | 91.17% |
| **`low`/`high` seeded from the args, `if (sourceIndex > destinationIndex)`** | **95.20%** |
| `min_val(dst, src)` + `max_val(src, dst)` (swapped operands) | 94.39% |
| connections vector through a pointer, selected after the min/max | 95.20% |
| `destination->GetIndex()` before `source->GetIndex()` | 95.12% |

What is left is **register allocation and nothing else**: the instruction sequence is identical
to retail's, only the registers differ. Retail keeps `n→r7, src→r0, dst→r8, connections→r9,
min→r4`; the best spelling keeps `src→r0, n→r7, dst→r4, connections→r8, min→r5`, and it loads
`src` before `n` where retail loads `n` first. Same instruction order in the compare, different
assignment. I did not find a spelling that changes it.

## `FindClosestReachablePoint` 91.87% → 97.86% (kept, not matched)

Same obstruction-chain rewrite; what is left is again pure floating-point register allocation
in the `centroid - point` block. Retail loads `c.y, c.x, c.z` into `f2, f1, f3` and **re-loads**
the centroid into `f31, f30, f29` for `result` (`lfs f31,40(r21)`); ours loads them into
`f5, f3, f4`, none of which is clobbered, and `fmr`s them instead of re-loading. The
multiplication and spill order is the same in both (`stfs <dy>,12(r1)`, `stfs <dx>,8(r1)`,
`stfs <dz>,16(r1)`, then two `fadds`), so `MagSquared()` is not the problem. Measured:

| spelling | % |
|---|---|
| `const CVector3f& delta = region.GetCentroid() - point;` (kept) | 97.86% |
| `const CVector3f delta = …` (by value) | 92.50% |
| `const CVector3f centroid = …; const CVector3f& delta = centroid - point;` | 92.50% |
| `(region.GetCentroid() - point).MagSquared()` inline | 97.86% |
| `const float distanceSq = …` | 97.86% |
| `result` assigned before `closestDistanceSq` | 97.78% |
| `if (closestDistanceSq > distanceSq)` | 97.74% (and it cost `SetTransform` its 100%) |

## The constructor is still where the unit's remaining bytes are

`__ct__7CPFArea` is unchanged at 74.90% of 1524 bytes. The previous run's analysis still stands
and I did not re-open it: retail calls out to `fn_80141594`, `fn_801418A4`, `fn_80141528`,
`fn_801413C4` (all in this unit, all 0.00%) where we inline `CPFMemoryStream`, and it constructs
`CPFPointSearchState` through `__nw__FUlPCcPCc(0x24, …)` + a call at `0x801F8B48` + `fn_8014137C`,
which our source skips behind a `// TODO`. That is four new out-of-line functions to identify
before any of it can be written, not a tuning change.

## WALL:

WALL: FindClosestReachablePoint__7CPFAreaFRQ24rstl30reserved_vector<P9CPFRegion,8>RC9CVector3fUiUi
97.86% - 7 spellings; the instruction stream is retail's, the last 10 bytes are which FP
registers hold `centroid - point` and whether the centroid is re-loaded or `fmr`ed for `result`.

WALL: PathExists__7CPFAreaCFPC9CPFRegionPC9CPFRegionUi 95.20% - 7 spellings (superset of the
previous run's 11); identical instruction sequence to retail, differing only in register
assignment and in loading `src` before `numRegions`.

I did not write a `WALL:` for `__ct__7CPFArea`: I did not measure any spelling for it this run.

## NEW:

None filed. All of the remaining work is still inside `MetroidPrime/PathFinding/CPathFindArea`,
this item's own target, so requeue `progress-prime1-cpathfindarea` rather than open a new one.

## Lesson (not a NEW item)

A negated helper call is not free on this compiler: `!IsObstructed(flags)` needs a callee-saved
register for the bool it returns and flips the sense of every test inside it, four extra
instructions and one extra saved register for one call site. When retail's shape is a chain of
calls with `bgt`-style rejects, write the chain at the call site and delete the helper's users —
then check whether any caller is left.

---

# Third run (lane 2, 2026-10-01) — 14 → 30 of 38

**Result: `goal_check.sh build/goal/item.json` → PASS.** The unit's `matched_functions` went
**14 → 30 of 38**; project-wide `matched` went 11349 → 11365, `linked` unchanged at 5507.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11349 -> 11365   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.66% fuzzy, 25.40% matched, 11.94% linked (11365 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindArea: 14 -> 30 / 38 functions
  ok    no asm added
```

Diff: `src/MetroidPrime/PathFinding/CPathFindArea.cpp` (+18 lines: one out-of-line read helper and
its call site, plus comments) and `config/G2ME01/symbols.txt` (16 renames, no other edit). No
`configure.py`, no `files.cmake`, no `splits.txt`, no `tools/` change. No assembly.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
Unit `.text` 63.11% → **89.65%** fuzzy, `matched_code` 38.66% → **61.95%**, `.sdata2` still 100%.

## What the two previous runs missed: 16 of the 24 unmatched functions were never written at all

Both earlier runs worked function by function on the three functions `report.json` showed as
sub-100 and named, and both wrote off the rest as "0.00%, untouched". The 0.00% was not "hard":
`.tmp`-free reproduction of the check shows **our object already contained byte-identical code for
15 of them**, under C++ names. objdiff pairs by name, `dtk` could not name those TU-local weak
template instantiations so `symbols.txt` called them `fn_8013FED0` … `fn_8014195C`, and 15 of the
unit's 38 functions scored exactly 0 while sitting in `build/G2ME01/src/…CPathFindArea.o` at 100%.

How to reproduce the pairing (no new tooling needed, two commands per side):

```sh
OD=build/binutils/powerpc-eabi-objdump
$OD -s -j .text build/G2ME01/obj/MetroidPrime/PathFinding/CPathFindArea.o   # retail bytes
$OD -t      build/G2ME01/src/MetroidPrime/PathFinding/CPathFindArea.o        # our symbols+sizes
```

Match on (size, exact bytes). `fn_801418A4` (0xB8 bytes) and
`reserve__Q24rstl50vector<13CPFRegionData,Q24rstl17rmemory_allocator>Fi` are instruction-for-
instruction identical, including the internal `bl` targets' offsets in the function body.

**The fix is the rename, in `config/G2ME01/symbols.txt`, to the mangled name `nm` reads off our
own object** — the mechanism `docs/RUNNING_THE_DECOMP.md` documents twice ("Pairing a function the
retail symbol table has no name for" and "An unnamed function is often a template instantiation
you can identify by diffing it"; the CRumbleVoice measurement there is six functions 0% → 100%).
No code changes: every one of these was already at 100% byte-for-byte before the rename. Measured
on one of them first (`fn_801418A4` alone): 15 → 16 of 38, unit 63.45% → 66.70% fuzzy, nothing
else moved; then all sixteen at once, 14 → 30.

| retail name (was) | renamed to (read off our object with `nm`) | size |
|---|---|---|
| `fn_8013FED0` | `__dt__34TObjOwnerDerivedFromIObj<7CPFArea>Fv` | 0x90 |
| `fn_8013FFE0` | `__dt__Q24rstl33single_ptr<19CPFPointSearchState>Fv` | 0x58 |
| `fn_80140038` | `__dt__19CPFPointSearchStateFv` | 0x64 |
| `fn_8014009C` | `__dt__Q24rstl70vector<Q219CPFPointSearchState10SPointData,Q24rstl17rmemory_allocator>Fv` | 0x84 |
| `fn_80140174` | `__dt__Q24rstl50vector<13CPFRegionData,Q24rstl17rmemory_allocator>Fv` | 0x84 |
| `fn_801401F8` | `GetIObjObjectFor__16TToken<7CPFArea>FRCQ24rstl18auto_ptr<7CPFArea>` | 0x2C |
| `fn_80140224` | `GetNewDerivedObject__34TObjOwnerDerivedFromIObj<7CPFArea>FRCQ24rstl18auto_ptr<7CPFArea>` | 0x9C |
| `fn_801402C0` | `__dt__Q24rstl18auto_ptr<7CPFArea>Fv` | 0x64 |
| `fn_801413C4` | `resize__Q24rstl50vector<13CPFRegionData,Q24rstl17rmemory_allocator>FiRC13CPFRegionData` | 0xB0 |
| `fn_80141474` | `uninitialized_fill_n<P13CPFRegionData,13CPFRegionData>__4rstlFP13CPFRegionDataiRC13CPFRegionData` | 0x6C |
| `fn_801414E0` | `construct<13CPFRegionData>__4rstlFPvRC13CPFRegionData` | 0x20 |
| `fn_80141500` | `construct_impl<13CPFRegionData>__4rstlFPvRC13CPFRegionData` | 0x28 |
| `fn_80141528` | `__ct__13CPFRegionDataFRC13CPFRegionData` | 0x6C |
| `fn_801418A4` | `reserve__Q24rstl50vector<13CPFRegionData,Q24rstl17rmemory_allocator>Fi` | 0xB8 |
| `fn_8014195C` | `uninitialized_copy<…pointer_iterator<CPFRegionData>…,CPFRegionData>__4rstl…` | 0x68 |

Thirteen of the fifteen are confirmed by the call graph, not only by the bytes:

* the constructor calls `fn_801418A4` (its `mRegionData.reserve`) and `fn_801413C4`
  (its `mRegionData.resize`), and `fn_801413C4`'s grow path calls `fn_801418A4` then
  `fn_80141474` (uninitialized_fill_n) while `fn_801418A4` calls `fn_8014195C` (uninitialized_copy);
* the constructor calls `fn_80141528` with `r3` = a 0x34-byte stack slot and `r4` = the
  `__ct__13CPFRegionDataFv` result, and `fn_80141528`'s body is twelve `lfs`/`stfs`/`lwz`/`stw`
  pairs over `0(r4)…0x24(r4)` — a `CPFRegionData` copy constructor;
* `fn_801414E0` (32 B) → `fn_80141500` (40 B) → `fn_80141528`, which is exactly
  `construct` → `construct_impl` → copy-construct;
* `~CPFArea` (`0x8013FF60`) calls `fn_80140174` at `this+428`, `fn_8013FFE0` at `this+336` and
  `0x8002CDE0` at `this+16`; `fn_8013FFE0` calls `fn_80140038` with the dereferenced pointer and a
  deleting flag, and `fn_80140038` calls `fn_8014009C` at `+4` and `fn_80140120` at `+20` — i.e.
  `~single_ptr<CPFPointSearchState>` and `~CPFPointSearchState`, whose two members are
  `vector<CPFPointSearchState::SPointData>` and `vector<int>` at +4 and +20.

**Two names are inferred from the code shape alone, not from a caller**: `fn_801401F8` (0x2C, a
thin wrapper that forwards `r3` to `fn_80140224`) and `fn_80140224` (0x9C, `__nw__FUlPCcPCc(8, …)`
then three vtable stores and a 4-byte copy). Those are the two `TToken<CPFArea>` /
`TObjOwnerDerivedFromIObj<CPFArea>` object-factory helpers that `fn_8013FE2C` uses. The bytes are
identical either way; only the label is inferred, and retail's own name is unrecoverable.

### `fn_80140120` cannot be renamed (measured, do not retry)

Its two byte-identical twins in our object are `__dt__Q24rstl45vector<9CVector3f,…>Fv` and
`__dt__Q24rstl36vector<i,…>Fv`, and **both names are already in `symbols.txt`** at 0x8002CDE0 and
0x80009810 — retail emitted weak copies in two TUs. Renaming `fn_80140120` to either gives
`### mwldeppc.exe Linker Error: # in CPathFindArea.o` (duplicate symbol), not a matching.
Reverted; it stays `fn_80140120` at 0%.

### Decl order: the new function must go *after* `GetRegionListList`, not at the top of the file

`gate.sh`'s `decl order` check failed the first run of this item and only that. `fn_80141594` is
at 0x80141594, which is **lower** than `CPFAreaOctree::GetChildIndex` (0x80141AA4), so putting it
first in the file puts it last in the object — retail has it between `__ct__7CPFArea` (0x80140D88)
and `GetRegionListList` (0x801418E4). Its definition now sits there and
`python3 tools/check_decl_order.py --unit MetroidPrime/PathFinding/CPathFindArea` prints `ok`.
Note this check only started seeing the renamed functions at all *because* of this item: it
compares the two orders by name, so a rename adds nine more names to the comparison and can turn
a unit that was previously invisible to the check into a reported permutation. A renamed unit
should be run through `check_decl_order.py` before the judge.

## `fn_80141594`: retail's out-of-line read, and it is called exactly once (measured)

Retail's `fn_80141594` (24 B at 0x80141594) is the one function of the sixteen that our compiler
did *not* already emit under a C++ name, because `CPFMemoryStream` is a file-local class. Written
as an `extern "C"` free function over the stream — `(int& out, CPFMemoryStream& stream)`, i.e.
`this` in `r4`, so not a member — and called from the constructor for `mVersion`:

| spelling | % |
|---|---|
| `out = *reinterpret_cast<int*>(stream.Cursor()); stream.Cursor() += 4;` | 48.33 |
| **bind the cursor to a reference, assign `out` last** | **100.00** |

```cpp
extern "C" void fn_80141594(int& out, CPFMemoryStream& stream) {
  uchar*& current = stream.Cursor();
  const int value = *reinterpret_cast< const int* >(current);
  current += sizeof(int);
  out = value;
}
```

Written as two statements through `stream.Cursor()` the compiler reloads `mCurrent` **after** the
store through `out` (it cannot prove the two do not alias) and drops to 6 instructions that match
retail's 6 only where the mnemonics do. Binding the cursor to a reference keeps it in `r5`
across, which is retail's shape.

Wiring that one call also moved the constructor **74.90% → 90.22%** — the single largest
improvement in this run, and the same "the memory stream is a real object in retail" finding both
earlier runs recorded, confirmed by a build.

**But retail calls it exactly once.** Routing the constructor's other seven `ReadInt32()` calls
through the same out-of-line helper drops the constructor from 90.22% to **49.21%** (measured,
then reverted): retail inlines the rest and keeps the cursor in a stack slot, re-loading and
re-storing it around each read (`lwz r4,24(r1) … stw r4,24(r1)`, ~20 times from 0x80140EE4). Only
`mVersion` goes out of line.

## What is left in this unit (re-measured, `build/report.json`)

| function | before | after | note |
|---|---|---|---|
| `__ct__7CPFAreaFRCQ24rstl12auto_ptr<Uc>i` | 74.90% | **90.22%** | `fn_80141594` for `mVersion` |
| `fn_80141594` | 0.00% | **100.00%** | the out-of-line read above |
| 15 renamed template/weak instantiations | 0.00% | **100.00%** | `symbols.txt` rename only |
| `FindClosestReachablePoint__…UiUi` | 97.86% | 97.86% | untouched — previous run's wall, **not re-measured this run** |
| `PathExists__7CPFAreaCFPC9CPFRegionPC9CPFRegionUi` | 95.20% | 95.20% | untouched — previous run's wall, **not re-measured this run** |
| `fn_8013FE2C` | 0.00% | 0.00% | no twin in our object |
| `fn_80140324`, `fn_801403A8` | 0.00% | 0.00% | no twin in our object |
| `fn_8014137C` | 0.00% | 0.00% | no twin in our object |
| `fn_80140120` | 0.00% | 0.00% | twin exists, name taken — see above |

The constructor's remaining ~150 bytes are still the two items both earlier runs named and this run
did not re-open: `CPFMemoryStream` as a genuine local object whose `GetBlock`/cursor methods are
not fully inlined, and the point-search workspace construction behind the
`// TODO: identify the point-search workspace's native class` comment at line 165
(`__nw__FUlPCcPCc(0x24, "CPathFindArea", 0)` → a call at 0x801F8B48 → `fn_8014137C` storing into
`mPointSearchState` at +0x148). `fn_8014137C` (72 B) is `(void** slot, int flag)`: it calls
`fn_80140038` (now named `__dt__19CPFPointSearchStateFv`) on `*slot` with a deleting flag and
stores `flag` back into the slot, i.e. a `rstl::single_ptr<T>` destructor; the one we emit for
`mData` is `__dt__Q24rstl14single_ptr<Uc>Fv` at 84 bytes, so it is a different specialisation and
needs the member's real type, not a rename.

## WALL:

None written for `PathExists` or `FindClosestReachablePoint`: both sit at the previous run's
scores, this run measured no new spelling for either, and the brief says not to re-report an old
wall. `fn_80140120` is characterised above rather than as a `WALL:` — the reason is measured
(linker duplicate), not a guess.

## NEW:

None filed. All of the remaining work is still inside `MetroidPrime/PathFinding/CPathFindArea`,
this item's own target, so requeue `progress-prime1-cpathfindarea` rather than open a new one.

## Lesson (not a NEW item)

* **Before writing a function that reads 0.00%, check whether your object already has those
  bytes.** objdiff pairs by name; a byte-identical function under a name the retail symbol table
  never had scores exactly zero, and "0.00%" in this repo's report meant "unpaired", not "wrong"
  and certainly not "unwritten". `nm -n` the retail object and the source object, sort by
  (size, bytes), and the pool of free functions appears. On this unit that was 15 of 24 unmatched
  functions and two thirds of the item's whole rise.
* A `symbols.txt` rename **adds names to `check_decl_order.py`'s comparison**, so a rename can
  surface a permutation that was previously invisible. Run the tool after renaming.
* `rstl::single_ptr`/`vector`/`auto_ptr` destructors are the same 84/88/100/132-byte shape for
  every `T`, so a byte match alone does not name one; the *caller's* offsets into `this` do.

---

# Fourth run (lane 5, 2026-10-01) — 30 → 33 of 38

**Result: `goal_check.sh build/goal/item.json` → PASS.** The unit's `matched_functions` went
**30 → 33 of 38**; project-wide `matched` went 11392 → 11395, `linked` unchanged at 5507.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11392 -> 11395   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.73% fuzzy, 25.47% matched, 11.94% linked (11395 / 28465 functions)
  ok    target rose: main/MetroidPrime/PathFinding/CPathFindArea: 30 -> 33 / 38 functions
  ok    no asm added
```

Diff: `src/MetroidPrime/PathFinding/CPathFindArea.cpp`, `include/…/CPathFindArea.hpp`,
`include/rstl/single_ptr.hpp`, `config/G2ME01/symbols.txt` (3 renames, no other edit), plus the
state block the gate rewrites in `docs/HANDOFF.md`. No `configure.py`, no `files.cmake`, no
`splits.txt`, no `tools/` change. No assembly. `check_decl_order.py` prints `ok` after the renames.

Unit `.text` 89.65% → **95.89%** fuzzy, `matched_code` 61.95% → **66.52%**.

## Per function: before → after (re-measured on this tree)

| function | before | after | what changed |
|---|---|---|---|
| `__as__…single_ptr<19CPFPointSearchState>F…` (`fn_8014137C`) | 0.00% | **100.00%** | new out-of-line `operator=` + rename |
| `PointConnectionsTest__7CPFAreaFii` (`fn_801403A8`) | 0.00% | **100.00%** | written from scratch |
| `PointPathExists__7CPFAreaFPC8CPFPointPC8CPFPoint` (`fn_80140324`) | 0.00% | **100.00%** | written from scratch |
| `__ct__7CPFAreaFRCQ24rstl12auto_ptr<Uc>i` | 90.22% | **98.07%** | the point-search workspace |
| `FindClosestReachablePoint__…UiUi` | 97.86% | 97.86% | untouched (previous run's wall, **not re-measured**) |
| `PathExists__…Ui` | 95.20% | 95.20% | untouched (previous run's wall, **not re-measured**) |
| `fn_80140120`, `fn_8013FE2C` | 0.00% | 0.00% | characterised below, not attempted |
| the 26 other functions | 100% | 100% | unchanged; nothing regressed |

## The point-search workspace: it is a real object, and retail's `fn_801F8B48` is its constructor

Both earlier runs left the constructor's `// TODO: identify the point-search workspace's native
class` (line 165) untouched. Filling it in took the constructor **90.22% → 98.07%** and produced
the unit's last two new functions as a side effect.

`fn_801F8B48` is 0x9C bytes at 0x801F8B48 in the DOL. It is `CPFPointSearchState`'s constructor:
it stores the point count at +0 (`stw r31,0(r3)`, `r31 = r4` = the argument), then resizes the
`SPointData` vector at +4 through `fn_801F8BE4` with a stack-built 0x10-byte SPointData
(`-1, 0, f, f` — `lfs f0,-19840(r2)` twice), and clears the `int` vector at +0x14 through
`fn_801F8F00`. That is exactly the class already declared in the header, so retail's constructor
call at 0x80141590 is `workspace->CPFPointSearchState(numPoints)`, and the header's
`explicit CPFPointSearchState(int)` is its declaration - the header was right and only the call
site was missing. Declared as `extern "C"` alongside `__nw__FUlPCcPCc`, both of which retail
calls from this unit.

The null test and the `single_ptr` install match retail byte-for-byte:

```
retail 0x80141570  ours 0x801417a4
  lis r4,0 ; li r3,36 ; addi r4,r4,0 ; li r5,0 ; bl __nw__FUlPCcPCc
  mr. r4,r3 ; beq +0x10 ; mr r4,r30 ; bl fn_801F8B48 ; mr r4,r3
  addi r3,r31,328 ; bl fn_8014137C
```

Two details, each worth the other's time:

1. **The placement string is the shared `"\?\?(\?\?)"`, not this unit's name.** The notes for
   earlier runs said `__nw__FUlPCcPCc(0x24, "CPathFindArea", 0)`; that is a guess the previous run
   never checked. `symbols.txt:17166` has `lbl_803A91C0 = .rodata:0x803A91C0; // size:0x7
   data:string`, and the bytes at 0x803A91C0 in the linked DOL are `3f3f283f 3f29 0000` = `??(??)`.
   With `"CPathFindArea"` we emit a `.rodata` section and the `addi r4,r4,7` that walks to the
   string inside it (98.07% is with `"\?\?(\?\?)"`, which drops that `addi` and matches retail's
   `addi r4,r4,0`; the earlier score with the name was also 98.07% but with a 4-byte-longer body
   and 4 more diff bytes elsewhere). `rs_new`'s spelling is the repo's existing convention for
   this - `include/Kyoto/Alloc/CMemory.hpp:59` and the `CMEMORY_NEW_FILE` comment above it.
2. **`operator=(T* const)` is out of line in this TU; `~single_ptr` is not.** Retail's
   `fn_8014137C` is a real 72-byte function: `*r3 = *r3; li r4,1; bl ~CPFPointSearchState;
   *r3 = r4; return r3`. `RSTL_SINGLE_PTR_OUT_OF_LINE` (the existing opt-in, used only by
   `src/Kyoto/DolphinCDvdFile.cpp`) takes **both** members out of line, and doing that here costs
   `~CPFArea` its match: retail inlines `~single_ptr` in `~CPFArea` (0x801401EC calls
   `__dt__…single_ptr<19CPFPointSearchState>Fv`, i.e. the weak copy, so it *is* out of line there
   too — but the `operator=` needs to be out of line too, and the existing macro's `~single_ptr`
   out-of-line definition also changes `~CPFArea`'s frame). Measured: with the existing macro,
   `~CPFArea` 100% → 78.12% and the unit fell to 28/38. So `include/rstl/single_ptr.hpp` gained a
   narrower opt-in, `RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE`, which moves only `operator=(T* const)`.
   The emitted weak copy is `__as__Q24rstl33single_ptr<19CPFPointSearchState>FP19CPFPointSearchState`
   (read off our object with `nm`) and is **byte-identical** to retail's `fn_8014137C`, verified
   before the rename.

**Also measured, smaller:** the `fn_80141594` call passes its output through a local
(`int version; fn_80141594(version, stream); mVersion = version;`) rather than
`fn_80141594(mVersion, stream)`. Retail does `addi r3,r31,0x14c` (i.e. `&mVersion`) at 0x80140130,
so the direct spelling is right and the local is what the current source does - but binding the
out-parameter as a local `int` first and assigning after the call is worth **+0.72%**
(97.81% → 98.07% after the string fix, and 97.09% → 97.81% before it). Do not "simplify" it back.

## The two written functions: retail's point-connectivity test

Neither is in Prime 1. Both were identified from the measured bytes and the call graph
(`tools/who_calls.py`): `fn_801403A8` is called from `fn_80140324` in this unit **and** from
`fn_801F86F4` (0x801F876C) in `auto_03_801F7AD0_text`, so both are real out-of-line members that
another unit calls, and `dtk` could not name either.

`PointConnectionsTest` (0x801403A8, 124 B) is `PathExists` with the two members swapped:
`mPoints.size()` at +0x18C where `PathExists` reads `mRegions.size()`, and `mPointConnections`' data
at +0x1A0 where `PathExists` selects ground/flyers on `flags & 2`. There is no `flags` argument, so
there is no `& 0x14` early return either - only the `a == b` one. The body is otherwise the same 25
instructions, including the `total - remaining + high - (low + 1)` bit index.

`PointPathExists` (0x80140324, 132 B) is the pointer-taking wrapper: null-test both, `a == b`
returns true, then convert each `CPFPoint*` to an index and call the test. The `/28` is
`sizeof(CPFPoint)`, and the source spells it as a `CPFPoint*` pointer difference
(`source - &mPoints[0]`) which mwcceppc strength-reduces to a byte difference divided by the
element size - the same `0x92492493` / `mulhw` / `srawi 4` sequence `GetPointIndex` already emits at
100%. **Both matched on the first or second spelling**, so the note for the next run is the
*identification*, not a wall:

* the swap must be `if (a > b) { tmp = a; a = b; b = tmp; }` **mutating the parameters**, with the
  rest of the function using `a` and `b` directly. Introducing `int low`/`int high` first and
  using them in the arithmetic is the same computation and produced
  `mr r7,r4 ; ble ; mr r7,r5 ; mr r5,r4` where retail has `mr r0,r4 ; mr r4,r5 ; mr r5,r0`
  (first diff at +20, 124 B both ways). Two spellings, the second is 100.00%:

  | spelling of the min/max step in `PointConnectionsTest` | bytes |
  |---|---|
  | `low`/`high` seeded from the args, `if (a > b) { low = b; high = a; }` | differs at +20 |
  | `low`/`high` seeded, with a hand-written `tmp` swap | differs at +20 (identical codegen) |
  | **`if (a > b) { tmp = a; a = b; b = tmp; }`, then use `a`/`b`** | **IDENTICAL** |

  This is the same lesson as `SetTransform` in the second run: retail strength-reduces once and
  reuses the single value, so a spelling that keeps two live names costs a `mr`. It is *not* the
  same as `PathExists`, where `low`/`high` is what reaches 95.20% and the old wall stands.

* `PointPathExists`' two null tests must be two separate `== nullptr` comparisons, and its
  declaration order relative to `PointConnectionsTest` in the header does not matter (the call is
  by declaration, both are in the class).

## `fn_8013FE2C` (164 B) is `CFactoryFnReturn<CPFArea>`'s constructor, and it is 12 bytes from
## matching — characterised, not attempted

`fn_8013FE2C` is called once, from `FPathFindAreaFactory` at 0x80140054, and its body is
`CFactoryFnReturn<T>::CFactoryFnReturn(T*)` (`include/Kyoto/CFactoryMgr.hpp:19`): build the
`auto_ptr<TObjOwnerDerivedFromIObj<CPFArea>>` via `GetIObjObjectFor`, copy out `mHas`/`mItem` into
the return slot, then tear the temporary down. Our object already emits it, under the mangled name
`__ct<7CPFArea>__16CFactoryFnReturnFP7CPFArea` at 176 B.

The first 128 bytes are identical; the difference is the teardown, the same finding the third run
recorded for the constructor's stream:

| | retail 0x8013FEF8 | ours |
|---|---|---|
| teardown | `addi r3,r1,16 ; li r4,-1 ; bl __dt__…auto_ptr<7CPFArea>Fv` | `lbz r0,16(r1) ; cmplwi ; beq ; lwz r3,20(r1) ; li r4,1 ; bl ~CPFArea` |

`rstl::auto_ptr<T>::~auto_ptr` has to be out of line in this TU for the 3-instruction form, i.e. the
same opt-in as `single_ptr` but for `auto_ptr`. **Measured and it does not work**: adding
`RSTL_AUTO_PTR_OUT_OF_LINE` (a new opt-in in `include/rstl/auto_ptr.hpp`, since the existing
`RSTL_SINGLE_PTR_OUT_OF_LINE` is `single_ptr`-only) makes our `__ct<7CPFArea>__16CFactoryFnReturn`
**132 B** instead of 176 B - it removes the `mHas` test *and* the `delete`, i.e. the compiler drops
the work rather than calling the weak copy, and the bytes come out **shorter** than retail's 164.
Reverted. The out-of-line `~auto_ptr` is emitted for a `T` whose destructor the compiler can prove
is trivial, or the inlining decision has to be forced per call site rather than per class.

## WALL:

None written. `PathExists` and `FindClosestReachablePoint` sit at the second run's scores and this
run measured no new spelling for either, and the brief says not to re-report an old wall.
`fn_8013FE2C` is characterised above rather than as a `WALL:` — the reason is measured (the
out-of-line opt-in produces 132 B against retail's 164), not a guess.

## NEW:

None filed. All of the remaining work is still inside `MetroidPrime/PathFinding/CPathFindArea`,
this item's own target, so requeue `progress-prime1-cpathfindarea` rather than open a new one.

## Lesson (not a NEW item)

* **A placement-`new` string argument is a real byte difference.** `__nw__FUlPCcPCc(size, file,
  line)` puts `file` in the object's `.rodata`; naming the TU adds an `addi` to reach the string
  inside the section, and retail's is very often the shared `"\?\?(\?\?)"` at a *named* `.rodata`
  label. `symbols.txt` will have that label with `data:string`; read the bytes at the address
  before assuming the string is the unit's own name.
* **`register`-pressure differences track how many *live names* the source has, not how the
  computation is written.** The same min/max step is `mr r7,r4 / ble / mr r7,r5 / mr r5,r4` in one
  spelling and `mr r0,r4 / mr r4,r5 / mr r5,r0` in another, and the second matches retail exactly.
  When two spellings differ only in the `mr`s, try mutating the parameters in place.
