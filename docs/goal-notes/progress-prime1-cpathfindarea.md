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
