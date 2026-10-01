# progress-unit-cautomapper

Lane 6, worktree `wt-mp2-goal-L6` (`goal/lane-6`). `kind: progress`,
target `MetroidPrime/CAutoMapper`.

## Re-measured baseline

`tools/fast_try.sh MetroidPrime/CAutoMapper` on the clean tree (my first action, before any
edit): **72/100 functions matched, 91.98% fuzzy** — the same figure the item's `reason` quotes,
so the item was not stale. Total `matched_functions` across all units: **11661**.

## Result

**75/100 matched, 92.02% fuzzy.** Total matched functions 11661 -> 11664. Nothing anywhere got
worse (verified by diffing every function's `fuzzy_match_percent` in `build/report.json` against
`build/goal/judge/report.base.json`: 3 better, 0 worse, 0 new, 0 lost).

Three functions taken to 100%, each by a source spelling change, no logic change:

| function | before | after |
|---|---|---|
| `SetCurAreaId` | 99.82% | **100%** |
| `BuildMiniMapWorldRenderState` | 97.65% | **100%** |
| `BuildMapScreenUniverseRenderState` | 97.44% | **100%** |

Only `src/MetroidPrime/CAutoMapper.cpp` changed (7 insertions, 6 deletions, three hunks).

### 1. `SetCurAreaId` — the `cmpw` operand order (99.82% -> 100%)

`lanediff` showed one differing instruction: retail has `cmpw r0,r31` where we emitted
`cmpw r31,r0`, i.e. the comparison operands were the wrong way round. Swapping the *written*
operands (`areaId != mCurAreaId.Value()`) changed nothing — mwcceppc normalises `!=` on two
ints to the same order regardless of which side is written. What does work is giving the
loaded value its own name so the compiler materialises it before the compare:

```cpp
const int curAreaId = mCurAreaId.Value();
if (curAreaId != areaId && ...
```

**Rule worth keeping: on an `x != y` int compare, a `cmpw` operand-order mismatch is not
fixable by swapping the source operands; introduce a named local for the loaded side.**

### 2 & 3. `BuildMiniMapWorldRenderState` / `BuildMapScreenUniverseRenderState` — do not
### hoist the `gpTweakAutoMapper` global into a local (97.6/97.4% -> 100%)

Both were at 97.4-97.7% with exactly the same five-word difference, in the function
prologue: retail loads the global **first**, before it spills the incoming argument registers
(`lwz r31,0(0)` then `mr r27,r4 / mr r28,r5 / mr r30,r7 / mr r26,r3`), we emitted the `mr`s
first and the `lwz` in the middle. The cause is the local:

```cpp
const CTweakAutoMapper* tweak = gpTweakAutoMapper.get();   // <- this line
... tweak->GetMiniCamDistance() ...
```

Hoisting the SDA2 load into a local gives the register allocator a value it must keep live
across the whole constructor call, so it schedules the reload after the spills. Writing
`gpTweakAutoMapper->...` at each use site gives the allocator a short-lived SDA load it can
hoist into the prologue, which is what retail does.

Note the sibling `BuildMapScreenWorldRenderState` was already at 100% *with* the same local —
it has one extra statement (`const float camDist = doingHint ? ...`) before the constructor, so
its allocator had already settled on retail's order. Do not "fix" the local there.

**Rule worth keeping: a prologue `lwz` of an SDA2 global before the argument spills is the
signature of not hoisting that global into a local. If retail's function reads a global
repeatedly inside one big constructor call, write it as `g->f()` at each use, not via a local.**

Spellings tried and rejected, all measured:

- `const CTweakAutoMapper& tweak = *gpTweakAutoMapper;` — identical, still 97.65%.
- `const float camDist = tweak->GetMiniCamDistance();` hoisted before the constructor, to try
  to force the load early — **91.71%**, much worse.
- `gpTweakAutoMapper->...` at the use sites — **100%**, the one that landed.

## Left unfinished, with the measurements

Ranked by number of differing 4-byte words (`tools/lanediff.sh` + a byte-run diff of
`obj/` vs `src/`), so the next run does not redo the ranking:

| words | % | function | what the diff is |
|---|---|---|---|
| 3 | 97.86 | `CheckLoadComplete` | two instructions swapped in the `mDummyWorlds = vector(n, auto_ptr())` setup: retail `stb r0,12(r1); addi r6,r1,8; stw r0,16(r1)` then the call, we do `stw`, `addi r6`, `stb`. Same instructions, different order — a scheduling difference only |
| 6 | 99.92 | `ProcessMapRotateInput` | `f4`/`f5` swapped: retail `lfs f5,100(r1)` feeds `fmuls f2,f5,f0` / `fnmsubs f31,f2,f1,f5` for the `angZ` term, we use `f4` there and `f5` for the `angX` term |
| 20 | 98.98 | `UpdateTempleKeys` | **not register allocation — the string literals.** Our `.rodata` puts `TempleKeyFoundIcon` at 0x25e, retail at 0x278 (10 bytes late): retail has `model_hex` early in `.rodata` and we emit it after `%s%d`. `GetString` is then called with `addi r4,r4,372` vs our `362`. Plus a `r27`/`r28` swap |
| 22 | 0.79 | `vector<auto_ptr<IWorld>>::clear` | ours is 164 B vs retail 96 B |
| 24 | 91.18 | `GetAreaPointOfInterest` | register selection: retail keeps `world` in `r30` and the area in `r31`, we keep the world in `r31` and use `r30` for the area (and need an extra `mr r0,r3`) |
| 28 | 99.23 | `FindClosestVisibleArea` | every allocation is shifted by one register (`r24`->`r25`, `r22`->`r23`, `r23`->`r24`, `r25`->`r22`). One extra GPR live somewhere |
| 31 | 27.21 | `vector<auto_ptr<IWorld>>::~vector` | ours 200 B vs retail 132 B — genuinely wrong body, not scheduling |
| 37 | 95.97 | `GetAreaHintDescriptionString` | ours 240 B vs retail 236 B |

Three things I tried on these that did **not** work, so nobody repeats them:

- `GetAreaPointOfInterest`: dropping the `const IWorld& world = *mWorld;` local and using
  `mWorld->...` directly **87.45%** (worse than 91.18%). The local is right; the difference is
  only how the world pointer is spilled.
- `GetAreaHintDescriptionString`: same shape as the `Build*RenderState` fix, but there is no
  `gpTweakAutoMapper` involved, so it does not apply. Not attempted further.
- `ProcessMapRotateInput`: swapping the order of the `angZ` statements
  (`angZ += ...right;` before `angZ -= ...left;`) **99.58%**, and hoisting both into named
  `CRelAngle` locals **98.17%** — both worse than the 99.92% we already had. Reverted both.
  The `f4`/`f5` choice is not reachable by reordering these two statements.

`.rodata` also differs in three string *contents*, unrelated to any function match and not
fixed here (fixing them changes `UpdateTempleKeys`' literals, which is the one place it would
pay off, but it touches shared string data and needs its own item):

- retail `Teleport Destination`, ours `Teleport_Destination` (src line 1670)
- retail `CMDL_CompassShellModel` (23 B), ours `CMDL_CompassShellMod` — the source says the
  full name at line 178, so this is a truncation, not a typo; worth a look on its own
- retail `.rodata` is 1760 B, ours 1736 B

## Verification

`./tools/goal_check.sh build/goal/item.json` in the worktree, unmodified:

```
goal_check: item progress-unit-cautomapper (progress) target=MetroidPrime/CAutoMapper
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11661 -> 11664   linked 5625 -> 5625
  ok    check_symbol_names.py
  ok    All:  33.28% fuzzy, 26.20% matched, 12.24% linked (11664 / 28465 functions)
  ok    target rose: main/MetroidPrime/CAutoMapper: 72 -> 75 / 100 functions
  ok    no asm added
goal_check: PASS progress-unit-cautomapper
```

No commit made. The unit stays `NonMatching` (correct for a `progress` item); no `configure.py`
or `config/` file was touched, and no function was added or removed, so the reverse declaration
order and `total_functions` are untouched.

## NEW

None filed. The unfinished work above is all one unit already in the queue, and the three
`rstl::vector<auto_ptr<IWorld>>` template bodies (0.79%, 27.21%, 69.52%) are the only place
with a genuinely wrong body rather than register allocation; they are worth a run of their own
but `MetroidPrime/CAutoMapper` is already the queued item for them, so re-filing it would just
duplicate the queue.