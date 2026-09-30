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
