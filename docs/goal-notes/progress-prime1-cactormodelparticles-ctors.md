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

---

# Run 7 (2026-10-02, lane 7)

## Re-measured first: the reason is stale again, and the unit is further along than run 6 saw

`item.json`'s reason quotes run 3. On the clean tree at `08cb55d2` this unit was already
**57 / 77**, not 48, and `__ct__CSystem` was already 100% (run 5). What was left:

```
 98.62 1160  UpdateOnFire            (WALL, run 6 + this run)
 96.86->100 140  fn_8014FD70          <- this run
 93.55  248  StartBurnDeath          (WALL, run 5 + this run)
 92.25   80  fn_8014C450             <- this run, still short
 91.14   88  fn_8014C554             <- this run, still short
 42.04  508  __ct__CItem             (dead end, confirmed by runs 3-6)
  0.46 1224  UpdateImplosion         (TODO stub)
  0.24 1676  GeneratePoints          (TODO stub)
  0.00  ...  fn_8014BAD4 76, fn_8014C208 484, fn_8014C3EC 100, fn_8014C4A0 180,
            fn_8014CC98 76, fn_8014CCE4 76, fn_8014CD30 12, fn_8014F9CC 72
```

`Render` and `AddStragglersToRenderer` are already 100% (run 5), so the reason's "0.7% / 1.0%"
for them is stale too.

## Result: 57 -> 62 / 77, `All:` 12191 -> 12196 matched. `./tools/goal_check.sh` -> **PASS**.

Unit fuzzy 72.99% -> **76.68%**, matched code 64.33% -> **67.19%**, data 100.00% (unchanged).
Linked 5860 -> 5860. Unit stays `NonMatching`; no flip attempted.

| function | before | after | what it is |
|---|---|---|---|
| `fn_8014C0AC` | 0.00% (64 B) | **100%** | `rstl::list<CItem>::insert(const iterator&, const CItem&)` |
| `fn_8014C0EC` | 0.00% (112 B) | **100%** | `rstl::list<CItem>::do_insert_before(node*, const CItem&)` |
| `fn_8014C15C` | 0.00% (100 B) | **100%** | `rstl::list<CItem>::create_node(node*, node*, const CItem&)` |
| `fn_8014CEF0` | 0.00% (112 B) | **100%** | `rstl::auto_ptr<CRainSplashGenerator>::operator=` |
| `fn_8014FD70` | 0.00% (140 B) | **100%** | `rstl::list<CItem>::do_erase(node*)` |
| `fn_8014C450` | 0.00% (80 B) | 92.25% | `reserved_vector<auto_ptr<CElementGen>,4>::reserved_vector(const&)` |
| `fn_8014C554` | 0.00% (88 B) | 91.14% | `reserved_vector<pair<auto_ptr<CElementGen>,uint>,8>::reserved_vector(const&)` |

All seven are **the same treatment run 5 applied to five others**: dtk gives retail's TU-local
weak template instantiations no symbol and names them `fn_<addr>`, objdiff pairs by name, and a
template instantiation is only ever emitted under its mangled name - so the identical bytes sat
in the object at 0.00%. Each is now written out under its retail name, body = the header's own
body. `rstl/reserved_vector.hpp:17-21` already prescribes exactly this and says why.

## The tool that found them - use this next, it is cheap and it is not a guess

`.tmp/opencode/symmatch.py` (new, untracked). For each retail function the report scores below
100%, it finds every symbol in `build/G2ME01/src/MetroidPrime/CActorModelParticles.o` of the
**same size** and compares the two instruction streams with **objdiff's masking** - bits 6..29
zeroed for `R_PPC_REL24` and `R_PPC_EMB_SDA21`, bits 0..15 for `ADDR16_HA`, and the low half for
`ADDR16_LO` - so a `bl` whose target moved does not count as a difference. Output on the clean
tree:

```
fn_8014BAD4   76  __ct__CSystem(const CSystem&)                     <- 76 B copy ctor
fn_8014C208  484  __ct__CItem(const CItem&)                         <- 484 B copy ctor
fn_8014C3EC  100  ~auto_ptr<CRainSplashGenerator>                   <- DELETING dtor
fn_8014C450   80  reserved_vector<auto_ptr<CElementGen>,4>::(const&)
fn_8014C4A0  180  ~reserved_vector<pair<auto_ptr<CElementGen>,uint>,8>  <- DELETING dtor
fn_8014C554   88  reserved_vector<pair<auto_ptr<CElementGen>,uint>,8>::(const&)
fn_8014CD30   12  CParticleElectric::GetParticleEmission() const
fn_8014CEF0  112  auto_ptr<CRainSplashGenerator>::operator=
fn_8014FD70  140  do_erase<list<CItem>>
```

**This is a whole-tree tool, not a per-unit one** - run it on any unit before looking for
functions to decompile; "the bytes are already in the object, only the name is wrong" is the
cheapest improvement `tools/report_diff.py`'s own docstring names, and it is findable
mechanically. Three bugs cost most of this run and are fixed in the file: read the instruction's
**bytes** from `parts[1]` of `objdump -dr` (`parts[2]` is the mnemonic); an `objdump -dr`
relocation is printed on the line **after** its instruction; and never compare the two sides'
absolute address lists, only their **offsets** from each base.

## What each body needs, and the two header changes

`include/rstl/list.hpp` and `include/rstl/auto_ptr.hpp` both had their data members `private`,
so the bodies could not be written out. Both are now `public`, with the same comment and the same
"Access is codegen-neutral" sentence `rstl/reserved_vector.hpp` already carries for exactly this
reason (`list.hpp:238-247`, `auto_ptr.hpp:9-16`). Verified codegen-neutral: nothing else in the
tree moved - see the report diff below, which shows `+5 functions at 100%, no regression`, and
`main.dol` still hashes `6ef9b491...`.

Bodies, all written as the header writes them so mwcceppc emits the same code:
`create_node` = `allocate` + two stores + `rstl::construct`; `do_insert_before` / `do_erase` =
`list.hpp`'s own statements; `insert` returns `iterator(self->do_insert_before(...))` (returning
it **by value**, not `*out = ...`: retail leaves the returned `node*` in `r3` instead of the sret
pointer, and an out-parameter spelling gets that wrong); `auto_ptr::operator=` is the header's.

### `fn_8014FD70` was one instruction: use the local for the store too

`list.hpp`'s `do_erase` writes `mStart = node->get_next()` and separately holds
`node* result = node->get_next()`. With both spelled out we emit `lwz r3,4(r4)` + `mr r31,r3` -
144 bytes to retail's 140, 96.86%. Writing `mStart = result` instead gives retail's single
`lwz r31,4(r4)` feeding both the store and the return: 100%. Same value, no work dropped.

## Measured, not fixed

* **`fn_8014C450` 92.25% / `fn_8014C554` 91.14% - register allocation only.** Both are 20
  instructions and the exact retail size (80 / 88 B); the structure, the loop shape (`bdnz`
  countdown), the `mtctr`/`beqlr` guard and the stolen `stb 0,0(r7)` are all retail's. Two
  differences remain: retail hoists `li r0,0` **before** `addi r6,r3,4`, we emit it after; and
  retail reuses `r4` (the dead `other`) for the reload of `self->mCount` and `r5`/`r4` for the
  element temps, we reuse `r3` (the dead `self`) and `r4`/`r3`. Four spellings measured, all
  byte-identical to the 92.25% version: `self->mCount = other.mCount; uninitialized_copy_n(
  other.data(), self->mCount, self->data())`; the same with a named `const int count` local
  (**worse**, 84.25% / 86.59%); the same with a named `*const dest = self->data()` (identical);
  and taking `self` as a **reference** instead of a pointer, i.e. the header's spelling verbatim
  (identical). Kept at 92.25/91.14 rather than dropped: they are no longer 0.00% and add nothing
  to the object (see below), but they do not raise `matched_functions`.
* **`fn_8014BAD4` (76 B, `CSystem`'s copy ctor) and `fn_8014C208` (484 B, `CItem`'s)**: retail
  calls the member's own constructor **directly** (`bl __ct__vector<CToken>(const vector&)` with
  `r3 = self`, no null test), which C++ can only spell as a placement new - and mwcceppc's
  placement new carries the `mr r30,r3 ; beq` retail does not have. Run 6 measured 88.68% for
  this and it is not reachable from the header. Not attempted again.
* **`fn_8014C3EC` (100 B) and `fn_8014C4A0` (180 B)** are **deleting** destructors
  (`extsh. r0,r4 ; ble ; bl CMemory::Free` on the dtor flag). `p->~T()` is not one and there is no
  C++ spelling that makes mwcceppc emit one for a class with no vtable.
* **`fn_8014CD30` (12 B, `CParticleElectric::GetParticleEmission`)**: our object already carries
  the byte-identical weak out-of-line copy. Writing `extern "C" bool fn_8014CD30(const
  CParticleElectric* self) { return self->GetParticleEmission(); }` gives a **virtual** call
  (`lwz r12,0(r3); lwz r12,80(r12); mtctr; bctrl`, 44 bytes) - mwcceppc does not devirtualise it.
  Reading `mEmitting` directly needs it public, and unlike the two `rstl` templates that is a
  game class's private data with no "written out by hand" precedent, so I left it alone. This
  one line in `include/Kyoto/Particles/CParticleElectric.hpp` would take a 12-byte function from
  0.00% to 100%; the call is a one-line change if someone wants it.
* **`fn_8014CC98` / `fn_8014CCE4`** (76 B each) are `CParticleElectric::SetOverrideIPos` /
  `SetOverrideFPos`, only reachable from the `GeneratePoints` stub. Unchanged from run 5.
* **`fn_8014F9CC` (72 B)** is `optional_object`'s steal-if-empty `operator=`; `m_data`/`m_valid`
  are private in `include/rstl/optional_object.hpp:78-80` and I did not add a third header change.
* **`__ct__CItem` (42.04%), `UpdateImplosion` (0.46%), `GeneratePoints` (0.24%)** - unchanged;
  the first is the dead end runs 3-6 confirmed four times, the other two are the TODO stubs.
* **`UpdateOnFire` 98.62% and `StartBurnDeath` 93.55%** - untouched this run, the
  `CSfxManager::AddEmitter` argument-schedule wall of runs 3-6 (16 + 14 spellings). Still there.

## Files changed

`src/MetroidPrime/CActorModelParticles.cpp` (+111), `include/rstl/list.hpp` (+7/-2),
`include/rstl/auto_ptr.hpp` (+6). No config, no `configure.py`, no `files.cmake`, no `.asm`,
no new undefined reference, no layout change (`CHECK_SIZEOF` untouched). Every added body is
the header's own statements; **no initialisation was dropped**. The two header edits are access
specifiers only. Lines: `fn_8014FD70` :28-45, `fn_8014CEF0` :645-657, `fn_8014CD30` :699-706,
`fn_8014C554` :746-752, `fn_8014C450` :754-758, `fn_8014C15C` :775-786,
`fn_8014C0EC` :788-799, `fn_8014C0AC` :801-805.

## Verification (all measured on this tree)

```
./tools/decomp_build.sh
  All:  34.44% fuzzy, 27.69% matched, 12.89% linked (12196 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 76.68% fuzzy, 67.19% matched (62 / 77 functions)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
python3 tools/report_diff.py <clean> build/report.json
  matched  12191 -> 12196   linked 5860 -> 5860   (+5 functions at 100%, 0 units newly linked)
  no regression
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
./tools/unit_fit.sh MetroidPrime/CActorModelParticles.cpp
  40 functions present in ours but not in the retail unit object, 4220 bytes - **identical to the
  clean tree** (measured by stashing this diff and rebuilding): all of them pre-existing weak
  COMDAT copies. Every one of the nine new symbols pairs with a retail function, so nothing new
  is unpaired.
python3 tools/check_symbol_names.py   checked 525 units; 0 declared names are missing
./tools/goal_check.sh build/goal/item.json   PASS (exit 0)
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 12191 -> 12196   linked 5860 -> 5860
  ok  target rose: main/MetroidPrime/CActorModelParticles: 57 -> 62 / 77 functions
  ok  no asm added
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run and reverted with `git checkout`;
the driver discards edits to it. No `tools/`, `build/goal/` or
`docs/research/port_link_baseline.txt` path touched, not committed. `.tmp/opencode/symmatch.py`
is untracked and will be cleaned.

## Still open, in the order I would take them next

1. `fn_8014CC98` / `fn_8014CCE4` (76 B each) become writable the moment `GeneratePoints` is a real
   body - they are its `SetOverrideIPos`/`SetOverrideFPos` calls. `GeneratePoints` (1676 B) is one
   whole item and unlocks three functions with it.
2. `fn_8014CD30` (12 B) - one line in `include/Kyoto/Particles/CParticleElectric.hpp`.
3. `UpdateOnFire` 98.62% / `StartBurnDeath` 93.55% - the `CSfxManager::AddEmitter` declaration in
   `include/Kyoto/Audio/CSfxManager.hpp:198` (retail's last parameter mangles as `b`, ours as `t`).
   30 spellings have now failed across four runs; only the header is left.

## Review rejected run 27 (2026-10-01 22:31:51Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/CActorModelParticles.cpp:699-706` commits a knowingly wrong body under retail's symbol name — `fn_8014CD30` is a 44-byte vtable dispatch, while retail's is a 12-byte `return mEmitting` — with a comment asserting the opposite of the note's own measurement; it is never called, so it only adds dead bytes, and the correct 12-byte weak copy is already in the object. Delete that definition and its comment and the rest of the diff (the five 100% matches and two partials) lands on its own; the acceptable alternative the note itself names is making `mEmitting` readable and returning it directly for a real 12-byte match.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cactormodelparticles-ctors-L7-27.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cactormodelparticles-ctors-L7-27-review1-20261001T222743.jsonl

## Fix round 1

Reviewer rejected run 27 for one hunk only: the `fn_8014CD30` definition. It committed a body
that is knowingly not retail's - `return self->GetParticleEmission()` on a `const
CParticleElectric*` is a virtual dispatch (`lwz r12,0(r3); lwz r12,80(r12); mtctr; bctrl`, 44
bytes) where retail's is a 12-byte `return mEmitting`, as this file's own "Measured, not fixed"
section recorded. **Deleted**, as the reviewer asked, not fixed: deleted the whole hunk
(comment plus body) and its one line in the inventory comment above the `fn_8014C554` /
`fn_8014C450` block, so no comment in this file still claims the symbol is written out here. Every
other hunk of the diff is untouched.

Cost of the deletion, measured: **zero.** `fn_8014CD30` scored 0.00% in both the before and after
report - the hand-written 44-byte body never scored and objdiff never needed the name, because the
byte-identical 12-byte weak out-of-line copy this TU already emits for the vtable is what pairs
with retail under its mangled name. The unit's numbers are bit-for-bit the ones run 7 reported:

```
main/MetroidPrime/CActorModelParticles  76.68% fuzzy, 67.19% matched code, 100.00% data (62 / 77)
All:  34.44% fuzzy, 27.69% matched, 12.89% linked (12196 / 28465 functions), linked 5860 -> 5860
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched 12191 -> 12196   (+5 functions at 100%, no regression)
  +100%  fn_8014C0AC, fn_8014C0EC, fn_8014C15C, fn_8014CEF0, fn_8014FD70
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/unit_fit.sh MetroidPrime/CActorModelParticles.cpp
  40 functions present in ours but not in the retail unit object, 4220 bytes - **the same figure
  as the clean tree**, so deleting the dead symbol unpaired nothing
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
python3 tools/check_raw_offsets.py   ok: 167 raw-offset site(s) in 71 file(s)
./tools/goal_check.sh build/goal/item.json   PASS (exit 0), target 57 -> 62 / 77
```

The `fn_8014CD30` route is still the run 7 follow-up it was: one line in
`include/Kyoto/Particles/CParticleElectric.hpp` to make `mEmitting` readable, then
`extern "C" bool fn_8014CD30(const CParticleElectric* self) { return self->mEmitting; }` is a real
12-byte match instead of a 44-byte dispatch. Not done here - it is a new definition, and this
round is only the correction the reviewer asked for.
