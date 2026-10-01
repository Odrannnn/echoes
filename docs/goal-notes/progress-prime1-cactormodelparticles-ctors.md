# progress-prime1-cactormodelparticles-ctors

Run 6 (2026-10-01, lane 6). The item's `reason` was written from run 3's notes and is **stale on
both counts** - re-measuring first was the whole job:

- `__ct__CSystem` (296 B) was already **100%** on the clean tree at `ecde8b30` (run 5 landed it).
  Nothing left to do there.
- `__ct__CItem` (508 B, 42.04%) is a dead end, independently confirmed by runs 3, 4 **and** 5.
  Retail emits 127 instructions to our 166: it calls `fn_8014F9CC` (72 B, one of the unit's 0%
  `fn_` gaps) out of line to fill the 8-element `mOnFireGens` vector where MWCC inlines and fully
  unrolls, **and** it calls `__ct__13CUnitVector3fFRC9CVector3f` out of line for
  `mImplosionClipPlane` where our header inlines it. Both are inline/out-of-line decisions in
  shared headers (`rstl::construct`/`uninitialized_fill_n`, `CUnitVector3f`). Not a `progress`
  item. **Confirmed a third time here; do not file it again.**

So this run went after what was actually left. Two functions reached 100%.

## Result

`main/MetroidPrime/CActorModelParticles` **48 -> 50 / 77** matched functions.
Unit fuzzy 66.54% -> **66.58%**, matched code 52.33% -> **57.92%**, data 100.00% (unchanged).
`All:` 12108 -> **12110** matched (28465 total). Linked 5860 -> 5860. The unit stays
`NonMatching`; no flip attempted.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, exit 0).

| function | before | after | what did it |
|---|---|---|---|
| `UpdateAshGen__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager` | 99.97% | **100%** | the `mAshMaxParticles` clamp is **nested inside** the `actor != nullptr` test |
| `GetNextBestPt__FiRC13CSkinnedModelRC18SSkinningWorkspaceiR9CRandom16` | 97.14% | **100%** | `float maxDistance = 0.f;` declared **before** `startVec` |
| `UpdateOnFire__Q220CActorModelParticles5CItemFfP6CActorR13CStateManager` | 98.62% | 98.62% | not fixed - six spellings tried, all identical (see below) |

## Finding 1: `UpdateAshGen` - the clamp belongs *inside* the null test

Runs 3 and 4 both called this function's residue "the unreachable `.sdata2` float-literal pool
slot". It was not. With objdiff-faithful masking there is **exactly one** differing instruction, a
branch distance:

```
ours  insn 62  41 82 00 10   beq +0x10   (skip 4 instructions)
retail      62  41 82 00 40   beq +0x40   (skip 16 instructions)
```

Retail `0x8014E6D0..0x8014E714`:

```
8014e6d0: cmplwi r29,0            ; actor != nullptr ?
8014e6d4: beq   8014e714         ; null -> skip the transform call AND the whole clamp
8014e6d8: lwz   r3,124(r28) ; addi r4,r29,36 ; bl SetGlobalOrientAndTrans
8014e6e4: lwz   r3,132(r28) ; cmpwi r3,0 ; ble 8014e714
8014e6f0: cmpwi r3,16 ; li r0,16 ; bge +4 ; mr r0,r3        ; min_val(16, max)
8014e700: stw   r0,136(r28)                                ; mAshQueuedParticles
8014e704: lwz r3,136(r28) ; lwz r0,132(r28) ; subf r0,r3,r0 ; stw r0,132(r28)
8014e714: lwz r3,124(r28) ; ... mAshGen->Update(dt)
```

The `beq` skips **both** blocks, so retail's source nests the Echoes-only
`mAshQueuedParticles` clamp inside `if (actor != nullptr) { ... }`. Ours had the two `if`s side by
side, which produced the short branch. One brace pair, 99.97% -> 100%.

**Generalisable: when a function is off by a single branch *displacement*, diff the branch's jump
target against the block it is supposed to skip - not just the instruction.** Ours and retail had
identical instruction streams (188 for 188) and identical code around the branch; only the target
differed, and it differed by exactly the second block.

The second clamp, in the "just created the generator" arm, is **not** nested and was already right.

## Finding 2: `GetNextBestPt` - declaration order moves a literal load into the prologue

97.14% = 68/70, i.e. **two** instructions. Retail's 70-instruction stream and ours are identical
except that retail loads the `0.0f` into f28 in the prologue constant block at `0x8014CD6C`, just
after `stmw r25,52(r1)` and **ahead of** the six-`mr` argument block; we emitted the same `lfs`
12 instructions later, after the first `GetSkinnedPosition` call.

`float maxDistance = 0.f;` was declared *after* `const CVector3f startVec = model.GetSkinnedPosition(...)`.
Moving the declaration **above** the call moves the load into the prologue. Measured, both 100%:

| spelling | result |
|---|---|
| `int best; CVector3f startVec = ...; float maxDistance = 0.f;` (as landed by run 3) | 97.14% |
| `int best; float maxDistance = 0.f; CVector3f startVec = ...;` (kept) | **100.00%** |
| `float maxDistance = 0.f; int best = start; CVector3f startVec = ...;` | 100.00% - identical |

**Generalisable: MWCC places a local's initialiser at the point of its *declaration*, not at first
use. A float local initialised to a literal and used inside a loop belongs before any call, or the
literal load sinks to just before the loop and the prologue loses retail's constant block.**

## Finding 3 (this is the useful half): the "float pool slot" wall of runs 3, 4 and 5 does not exist

Runs 3, 4 and 5 each wrote off a family of residuals as *"retail loads the float from `.sdata2`,
MWCC folds the literal; we could not find a source construct that forces a pool load"* - and each
filed that as the blocker for `UpdateAshGen` (99.97%), `GetNextBestPt` (97.14%) and `UpdateOnFire`
(98.62%). **All three of those were wrong, and all three were unblocked by ordinary source edits
today.** The mistake was the *diff tool*, not the compiler.

`.tmp/opencode/sbs2.py` (run 3/4) normalises by **deleting** the immediate from every `lis/li/lha/
lwz/stw/stb/addi/lfs/stfs/addis/rlwinm`, so it cannot see a displacement difference at all - and
`bytescmp.py` aligns instruction-by-instruction from the top, so a single moved instruction
desynchronises the whole listing. Both report "the float differs". Neither one applies
**objdiff's relocation masking**, and objdiff *does* mask it.

`.tmp/opencode/mdiff.py` (new this run) does: it disassembles our symbol and the retail range,
reads our object's relocation table, and on both sides masks the field each relocation fills in -
bits 6..29 for `R_PPC_REL24` and `R_PPC_EMB_SDA21`, bits 0..15 for `R_PPC_ADDR16_HA`, bits 16..31
for `R_PPC_ADDR16_LO`. Then it reports differing *instructions*, not a desynchronised listing.
Recreated from this paragraph if gone; untracked, so the driver will clean it. **Use it instead of
`sbs2.py` and `bytescmp.py`.**

Calibration that makes it trustworthy - and the caveat:

- On all 48 functions objdiff scores 100% in this unit it reports **0 differing instructions** for
  36 of them, and exactly *one difference per unresolved relocation* for the other 12
  (`Make*Gen` -> `@stringBase0`, `__ct__CActorModelParticles` -> 9x `gpSimplePool`,
  `InitializeSystemTypes` -> 2x `skParticleNames`, `StartRainSplashes`, `SetupHook`). objdiff
  clearly *resolves* relocations to named/global symbols rather than masking them; my tool masks
  them, so it is conservative in the right direction. **After the two fixes above it reports
  0/70 for `GetNextBestPt` and 0/188 for `UpdateAshGen**, i.e. it agrees with objdiff's 100% on
  exactly the functions it was used to find.*
- Proof that SDA21 masking is right and that the whole "pool slot" story was a tool artefact:
  `LightDudeOnFire` scores **100.00%** in objdiff and its bytes are ours `lfs f0,0(0)` +
  `R_PPC_EMB_SDA21 @2050` against retail `lfs f0,-24920(r2)` - the identical pair that runs 3 and
  4 read as a wall. Same `@2050` label, same displacement -24920.

**Do not spend another run on a "`.sdata2` float literal" explanation without first running
`mdiff.py`.**

## What I measured and did not fix, so the next run does not repeat it

**`UpdateOnFire` 98.62%, 3 differing instructions of 290, all one swap.** Retail sets up the
`AddEmitter` arguments in the order `[lwz r6,4(r28)] [addi r3,r1,16] [lha r9,kMedPriority]`
(0x8014DEE0..0x8014DEE8); we emit the `lha` first and the `lwz` third, with `addi r3` in the
middle in both. `r6` is `actor->GetCurrentAreaId().Value()`, `r3` the implicit `CSfxHandle`
sret slot, `r9` `CSfxManager::kMedPriority`. Six spellings, **all measured at exactly 98.62%** with
identical 1160-byte / 290-instruction output: a named `const int area` local before the call; a
named `const short prio` local; both together; `static_cast<short>(kMedPriority)`; `(bool)true,
(bool)true`; and moving `prio` to just before the enclosing `if (!mSfx)` (that last one is a no-op -
the source was already identical). One that was **worse**: swapping the two trailing bools for
`(kMedPriority, true, true)`, 97.14%. This is MWCC's argument-evaluation order for a call with an
sret return and a default `short` last parameter; it is not reachable from the argument list. The
same shape is why run 5 could not fix `StartBurnDeath`'s `AddEmitter` argument order either, so the
next thing to try is the **header**: `CSfxManager.hpp:198` declares
`AddEmitter(ushort, const CVector3f&, int = kAllAreas, bool = false, bool = false, short =
kMedPriority)`, while retail's mangled name is `AddEmitter__11CSfxManagerFUsRC9CVector3Fibbs` -
last parameter `bool`, not `short`. Changing that default is a shared-header change with unbounded
blast radius and belongs in its own item.

Carried forward unchanged from earlier runs, all still true and not re-tried here:

- **`StartBurnDeath` 90.87%**: 244 vs 248 bytes, one instruction. Retail `cntlzw r0,r0; rlwinm
  r0,r0,27,31,31; neg r3,r0; addi r0,r3,9602` vs our `cntlzw r0,r0; srwi r3,r0,5; addi r0,r3,9601`
  - same value, and MWCC only picks the `rlwinm` when it has **not** proven the field's range. The
  range comes from the declared 4-value `EPlayerMorphBallState mMorphBallState`
  (`include/MetroidPrime/Player/CPlayer.hpp:548`). Runs 3, 4 and 5 measured 14 spellings in total,
  none better than 90.87%. Needs that member declared with an unknown range.
- **`__ct__CItem` 42.04%**: dead end, see the top.
- Not attempted, and why: `UpdateImplosion` (1224 B, 0.46%) and `GeneratePoints` (1676 B, 0.24%)
  are the largest untouched functions and have **no Prime 1 source at all** - Echoes-only bodies
  needing renderer/audio calls the port may have no definition for. `Render` (548 B, 0.73%) and
  `AddStragglersToRenderer` (420 B, 0.95%) are `TODO` stubs with the same problem. A `progress`
  item is only counted at 100%, so a 90% transcription of any of them gains nothing and risks the
  link gate. The 19 `fn_*` gaps (1916 B) are MWCC's out-of-line template COMDATs and can only be
  matched under `fn_` names, which C++ cannot produce.

## Files changed

`src/MetroidPrime/CActorModelParticles.cpp` only - two functions, **12 insertions, 7 deletions**.
No header change, no new symbol, no new undefined reference, no layout change, no `.asm`. Both
edits only move or nest statements that were already there; **no initialisation was dropped**. Lines:
`UpdateAshGen` clamp nesting :340-349, `GetNextBestPt` declaration order :635-640.

## Verification (all measured on this tree)

```
./tools/decomp_build.sh
  All:  34.23% fuzzy, 27.43% matched, 12.89% linked (12110 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 66.58% fuzzy, 57.92% matched, 100.00% data (50 / 77)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
python3 tools/report_diff.py build/report.base.json build/report.json
  matched  12108 -> 12110   linked 5860 -> 5860   (+2 functions at 100%, 0 units newly linked)
  no regression
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
python3 tools/check_symbol_names.py   checked 525 units; 0 declared names are missing
./tools/goal_check.sh build/goal/item.json   PASS (exit 0)
  ok  no judge-owned path touched
  ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 12108 -> 12110   linked 5860 -> 5860
  ok  target rose: main/MetroidPrime/CActorModelParticles: 48 -> 50 / 77 functions
  ok  no asm added
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run and reverted with `git checkout`; the
driver discards edits to it. No `tools/`, `build/goal/` or `docs/research/port_link_baseline.txt`
path touched, no `configure.py`/`config/`/`files.cmake` change, not committed. `.tmp/opencode/`
helpers (`mdiff.py`, `var.py` new; `probe.sh`, `try.py`, `sbs2.py` from runs 3-5) are untracked and
will be cleaned.

## Still open, in the order I would take them next

1. `UpdateOnFire` 98.62% - the `AddEmitter` argument order. Try the **declaration** in
   `CSfxManager.hpp:198` (retail's last parameter mangles as `b`, ours as `t`), not the call site.
   Same root cause as `StartBurnDeath` 90.87%'s `AddEmitter` schedule.
2. `StartBurnDeath` 90.87% - declare `CPlayer::mMorphBallState` with an unknown range. Shared
   header, whole-tree regression check required; its own item.
3. `UpdateImplosion` (1224 B) and `GeneratePoints` (1676 B) - Echoes-only, each a whole item.
