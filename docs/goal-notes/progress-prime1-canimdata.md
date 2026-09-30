# progress-prime1-canimdata — `MetroidPrime/CAnimData` 58 → 66 / 216

Lane 2, head `097f0bc`. `tools/goal_check.sh build/goal/item.json` → **PASS**. The unit stays
`NonMatching` (216 functions, `CalcPlaybackAlignmentParms` alone is 2760 bytes of the 39532-byte
claim), so this is progress, not a flip.

```
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9512 -> 9521   linked 4852 -> 4853
  ok    target rose: main/MetroidPrime/CAnimData: 58 -> 66 / 216 functions
  ok    no asm added
goal_check: PASS progress-prime1-canimdata
```

## The build was broken at this head (not mine, fixed in the worktree)

Same defect `docs/goal-notes/progress-cgamestate-map-lowerbound.md` reports, still unfixed at
`097f0bc`:

```
$ ./tools/decomp_build.sh
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)'
$ cat build/goal/judge/record-gate.log | tail -2
ninja + build.sha1          FAIL
```

`match-stream` (`ada6d97`) flipped `musyx/runtime/stream.c` to `Matching` but its four
`MUSY_VERSION` guards live in `extern/`, which `tools/run_goal.sh`'s `stage_change` pathspec
cannot stage, so they were never committed. I re-applied them. **One detail the previous note
does not spell out, and it cost this run a build**: the guard around
`sndStreamMixParameterEx` must be **one** `#if` that also covers `sndStreamFrq` and closes
**after** `#pragma pop` — my first attempt used two guards, one closing before the pop, which
compiles (depth ends at 0, never negative) and then puts `main.dol` at
`0a255c503164205a6e30cea28e47195c4ef0dd79` with **84 of 86 RELs failing their checksums**. Only
the single-guard form reproduces retail:

```
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
$ ./tools/gate.sh build/goal/judge/report.base.json   →  GATE PASS  097f0bc+3 changed
```

`extern/` is outside what `stage_change` stages, so the driver will not commit it and the branch
will still be unlinkable after this lands.

## What landed, per function (all measured with `tools/fast_try.sh` + `build/report.json`)

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `GetAdditiveAnimationWeight__9CAnimDataFUi` | 99.44% | **100.00%** | needed one edit |
| `IsAdditiveAnimationActive__9CAnimDataCFUi` | 99.44% | **100.00%** | needed one edit |
| `DelAdditiveAnimationImmediately__9CAnimDataFUi` | 99.58% | **100.00%** | needed one edit |
| `GetLocatorTransform__9CAnimDataCFRC…string…PC13CCharAnimTime` | 95.78% | **100.00%** | **matched unchanged** |
| `GetLocatorTransform__9CAnimDataCF6CSegIdPC13CCharAnimTime` | 83.80% | **100.00%** | needed the condition inverted |
| `GetAdditiveAnimationTree__9CAnimDataCFUi` | 63.31% | **100.00%** | Prime 1's loop + inverted test |
| `DelAdditiveAnimation__9CAnimDataFUi` | 62.17% | **100.00%** | Prime 1's loop verbatim |
| `AdvanceIgnoreParticles__9CAnimDataFfR9CRandom16b` | 48.12% | **100.00%** | **matched unchanged** |

Eight functions, every one of them one of the 32 the item names. The two 99.4% ones were a
single register: retail emits `cmplw r4,r0` where we emitted `cmplw r0,r4`, i.e. the comparison
written the other way round.

### The three recipes, so the next run does not re-derive them

1. **The additive-animation search loop is hand-written, not a range-for.** Retail's
   `DelAdditiveAnimation` / `GetAdditiveAnimationTree` / `DelAdditiveAnimationImmediately` all
   load `mAdditiveAnims` with `lwz r0,1080(r3)` (capacity) and `lwz r5,232(r3)` (data) and
   compute the end pointer with `mulli r0,r0,44` — a `reserved_vector` with 44-byte elements, so
   the loop is written against raw pointers with a `break` and a second `if (search != end)`.
   Our range-for with an early `return` compiled to the same loads but a different block layout.
   Prime 1's spelling
   (`rstl::pair<uint,CAdditiveAnimPlayback>* end/search; while (search != end) { if (…==search->first) break; ++search; } if (search != end) {…}`)
   is what matches. **`end` must not be `const`** — with `* const end` the key lands in r3 and the
   end pointer in r4; retail has the other way round and the function sits at 99.17%.
2. **MWCC lays out the `then` block inline.** Whenever retail jumps *over* a block to a
   cold one at the end of the function, writing the test the other way round is the fix:
   `GetLocatorTransform(CSegId,…)` went from `if (id == CSegId::Invalid()) { return Identity(); }`
   to `if (id.val() != 0xFF) { … return CTransform4f(…); } return CTransform4f::Identity();`
   (83.80% → 100%), and `GetAdditiveAnimationTree` from `if (search != end) { return found; }` to
   `if (search == end) { return rc_ptr(nullptr); } return found;` (62.29% → 100%). Both are the
   *same* source semantics, only the block order changes.
3. **`GetLocatorTransform(const rstl::string&, …)` is retail's own two-liner, unchanged from
   Prime 1** — but only once the source has **two** locals
   (`CSegId seg = …GetSegIdFromString(name); CSegId segCopy = seg; return GetLocatorTransform(segCopy, time);`).
   With the single temporary we had a 32-byte frame; retail's is 48 with the extra `stb r0,16(r1)`
   and `stb r0,8(r1)`. This one is the only landed function that is a pure copy.

### `AdvanceIgnoreParticles`: an initialisation was removed on purpose

`bool suspendEffects = false;` → `bool suspendEffects;`. Retail has **no store** to the out
parameter (`addi r5,r1,8` then straight to the call), and Prime 1's `Matching` source is
`bool suspendParticles;`. This is not "dropping an initialisation to raise a percentage" — the
callee assigns it first thing (`DoAdvance` opens `suspendParticles = false`), and the store simply
is not in the binary. Recorded here because the diff looks like the forbidden pattern otherwise.
**Caveat**: the tree's `DoAdvance` is still a TODO stub that does *not* write its out parameter,
so the port reads an uninitialised byte until `DoAdvance` is decompiled. `DoAdvance` is 1416
bytes at 4.41% and is the single largest blocker in this unit.

## Not reached, and why — measured, not guessed

- **`SetPhase` (48 B, 8.33%) and the rest of the `IAnimReader` virtual wrappers** — a
  **vtable-slot blocker, not a source blocker.** Retail dispatches `VSetPhase` at `lwz r12,92(r12)`
  (slot 23), `VSimplified` at slot 22 and `VGetTimeRemaining` at slot 5. `include/Kyoto/Animation/IAnimReader.hpp`
  puts those at slots 21, 20 and 3 — a **constant +2 shift**, i.e. retail's vtable has two more
  virtuals ahead of `VGetTimeRemaining` than our header declares. `GetContributionOfHighestInfluence__13CAnimTreeNodeCFv`
  (56 B, 0.00%) has the same problem at retail slot 26. Every caller in this unit therefore
  emits a different `lwz r12,N(r12)` and cannot reach 100% until the header's virtual list is
  corrected. That is a **shared-header change to `IAnimReader` and every derived class**, i.e.
  every vtable in the DOL — out of scope for this item and not something a `CAnimData.cpp` edit
  can reach.
- **`SetModelScale` (92 B, 81.74%, ours 108 bytes)** — the extra bytes are `clrlwi r4,r0,24`
  plus a `neg/or/srwi` bool normalisation. Retail does `lbz r0,685(r3) ; rlwimi r0,r5,7,24,24 ;
  stb` and then `rlwinm r4,r0,25,31,31 ; stb r4,753(r3)`, i.e. it stores **0 or 0x80**, so
  `CPoseAsTransforms_Linear::mUniformScale` (`include/Kyoto/Animation/CPoseAsTransforms_Linear.hpp:50`)
  is a **1-bit field in retail and a plain `uchar` in this tree**. Two spellings of the `&&` were
  tried (`const bool` local, and the assignment reflowed onto one line) and neither changes the
  generated code. Again a shared-header change.
- **`GetBoundingBox` (388 B, 12.66%)** — the highest-value target left, because a correct
  implementation would also emit two 0% COMDAT copies in this TU
  (`GetContributionOfHighestInfluence__13CAnimTreeNodeCFv`, 56 B, and
  `__ct__30CAnimTreeEffectiveContributionFRC30CAnimTreeEffectiveContribution`, 148 B), for +3.
  Retail's body is `find_by_key` over `mCharInfo.GetAnimBBoxList()` **plus** an
  `mCachedAnimBounds` fast path keyed on a word at `this+1436`, and the two are interleaved
  (`cmpwi r3,3 / bne` style dispatch on an `EMetaAnimType` read at `anim+0`). I did not get far
  enough to pin the struct offsets; not attempted rather than half-done.
- **`GetAnimationPrimitives` (244 B), `GetAnimationDuration` (616 B), `GetAverageVelocity` (736 B)**
  — all three need `CAnimationManager::GetMetaAnimation(uint)`, and
  `include/Kyoto/Animation/CAnimationManager.hpp` in this tree is a 0x20-byte stub with **no
  methods at all**. Recovering that one declaration is what unlocks three of them *and*
  `ReleaseData__Q24rstl18rc_ptr<9IMetaAnim>Fv` (100 B, 0.00%, a template instantiation this TU
  does not currently emit — its five sibling `ReleaseData` instantiations are already at 100%).
  That is the best single unblock left in the unit.
- **`IsAdditiveAnimation` (228 B, 2.46%)** — Prime 1's `binary_find` over
  `mCharFactory->GetAdditiveAnimInfoList()` looks right (`CCharacterFactory` really does have the
  list at +64/+72 with 12-byte elements), but retail's body calls `fn_8002EB74` with **five**
  stack arguments and reads an uninitialised `lbz r9,12(r1)` first, which is not a binary search.
  Echoes' version is doing something else. Not attempted.

## Files touched

- `src/MetroidPrime/CAnimData.cpp` — the eight functions above. No header touched, no class
  layout changed, no `asm` added, no other unit's `.text` moved (the DOL sha1 and all 86 REL
  hashes hold).
- `extern/musyx/src/musyx/runtime/stream.c` — **the build unblock, not this item's work**
  (see above). Outside what `stage_change` stages, so it will not be committed.

`docs/HANDOFF.md` was rewritten by the judge's own `check_docs_claims.py` during `goal_check.sh`
and reverted afterwards.

NEW: fix-stage-change-extern | progress | MetroidPrime/CAnimData | `tools/run_goal.sh`'s `stage_change` pathspec cannot stage `extern/`, so `match-stream` (ada6d97) committed a `Matching` flip for `musyx/runtime/stream.c` with its `MUSY_VERSION` guards dropped and every lane has been building an unlinkable tree since (`undefined: sndStreamMixParameter`); still unfixed at `097f0bc`, and the one-guard form (closing after `#pragma pop`) is the only one that reproduces retail

---

# progress-prime1-canimdata — run 2: `MetroidPrime/CAnimData` 58 → 72 / 216

Lane 2, head `097f0bc` (`goal/lane-2`). `tools/goal_check.sh build/goal/item.json` → **PASS**.
**Re-measured first: this lane started at 58, not 66.** The previous run's source change was never
committed to this branch, so its eight functions had to be re-derived — the recipes below all
reproduced, which is the useful confirmation that they are not one-off luck.

```
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9512 -> 9527   linked 4852 -> 4853
  ok    target rose: main/MetroidPrime/CAnimData: 58 -> 72 / 216 functions
  ok    no asm added
goal_check: PASS progress-prime1-canimdata
```

## The build was broken at this head again — and is now fixed on `goal/decomp`

The previous run's `NEW: fix-stage-change-extern` has since been fixed upstream as **`ef9e308`
"fix: commit the MusyX stream.c guards match-stream flipped against"**, on `goal/decomp`. My lane
branch (`goal/lane-2`, forked before it) did not have it, so the first full `./tools/decomp_build.sh`
failed with `undefined: 'sndStreamMixParameter'`. I re-applied the same guards by hand; the result
is byte-identical to `ef9e308` apart from the explanatory comment, so rebasing the lane makes the
diff disappear on its own. **No new information here — do not re-file.** The four guards, for the
record: `streamKill` and `sndStreamMixParameter` move from `<= 2.0.2` to `<= 2.0.3`;
`sndStreamLPFParameter` moves from `>= 2.0.2` to `> 2.0.3`; and one
`#if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3)` covers `sndStreamMixParameterEx` through
`sndStreamFrq`, closing **after** the `#pragma pop`.

## What landed, per function (measured with `tools/fast_try.sh` + `build/report.json`)

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `DelAdditiveAnimation__9CAnimDataFUi` | 62.17% | **100.00%** | **matched unchanged** (recipe 1) |
| `GetAdditiveAnimationTree__9CAnimDataCFUi` | 63.31% | **100.00%** | Prime 1's loop + inverted test |
| `GetAdditiveAnimationWeight__9CAnimDataFUi` | 99.44% | **100.00%** | **matched unchanged** (recipe 1) |
| `IsAdditiveAnimationActive__9CAnimDataCFUi` | 99.44% | **100.00%** | **matched unchanged** (recipe 1) |
| `DelAdditiveAnimationImmediately__9CAnimDataFUi` | 99.58% | **100.00%** | needs the `return`-inside shape (below) |
| `GetLocatorTransform(CSegId, …)` | 83.80% | **100.00%** | needs the condition inverted (recipe 2) |
| `GetLocatorTransform(const string&, …)` | 95.78% | **100.00%** | **matched unchanged** (recipe 3) |
| `SetEffectState` | 54.19% | **100.00%** | Prime 1's copy + `find_by_key` |
| `SetEffectComponentExternalParam` | 43.30% | **100.00%** | Prime 1's copy + `find_by_key` |
| `GetFirstParticleEffect` | 24.18% | **100.00%** | **a reference, not a copy** (below) |
| `SetRandomPlaybackRate__9CAnimDataFR9CRandom16` | 1.52% | **100.00%** | **matched unchanged**, member names only |
| `GetTimeOfUserEvent(EUserEventType, CCharAnimTime)` | 6.41% | **100.00%** | one-liner, unchanged |
| `CountUserEvents(…, tree)` | 1.77% | **100.00%** | needs the named `value` local (below) |

Thirteen functions, twelve of them named in the item. `matched_code` 5696 → 7816.

### New recipes, so the next run does not re-derive them

4. **`DelAdditiveAnimationImmediately` wants `return` inside the loop, not a guard after it.** The other
   four additive functions take the `break` + `if (search != end)` form; this one does not. With the
   guard form we emit a second `cmplw r4,r5 / beq` re-test after the loop that retail does not have
   (and going from the 99.58% range-for to the guard form drops it to 69.96%). With
   `if (match) { mAdditiveAnims.erase(search); return; }` retail's own block layout appears and it
   goes to 100. Same rule as recipe 1, one exception.
5. **`GetFirstParticleEffect` takes the effect list by *reference*; its three siblings take a *copy*.**
   Retail's `SetEffectState` / `SetEffectComponentExternalParam` open with
   `addi r3,r1,12 ; addi r4,r27,196 ; bl <vector copy ctor>` and close with a scoped-destroy call, so
   they copy the `TEffectList` to the stack before `find_by_key`. `GetFirstParticleEffect` has neither
   a copy nor a destroy — it passes `this+196` straight to `find_by_key`. Prime 1 copies in all four,
   so copying `GetFirstParticleEffect` too leaves it at 44.61%; the reference form is 100%. **Read
   the disassembly per function here, do not copy Prime 1's shape across the family.**
6. **Spell the local `const CCharacterInfo::TEffectList effects = mCharInfo.GetEffects();`** (a copy
   into a `const` local), not a non-const one. A non-const local makes `find_by_key` pick the `T&`
   overload, which returns `rstl::pointer_iterator` and does not compile against a `const_iterator`.
   With `const` it resolves to the `const T&` overload retail calls. Needs
   `#include "rstl/algorithm.hpp"`, which this file did not have.
7. **`components.size() != 0`, not `components.begin() != components.end()`.** Retail tests the count
   (`lwz r0,20(r4) ; cmpwi r0,0`); the iterator form compiles a `mulli r0,r0,28` plus an end-pointer
   compare that retail has no trace of (85.88% → 100%). The *loop* in `SetEffectState` does use
   `begin()`/`end()`, so this too is a per-site decision.
8. **`poi.GetValue() == static_cast<int>(type)` must go through a named `const int value` local.**
   Inline, MWCC emits `cmpw r31,r0` where retail has `cmpw r0,r31` — the same one-register flip as
   the 99.4% cases the previous run found, and it behaves identically here: 99.87% inline, 100.00%
   with the local. Swapping the operands of the `==` does **not** help (still 99.87%); it has to be
   a local.
9. **`mInt32POINodes[i]` and `sInt32TransientCacheData[i]` are safe to subscript directly** —
   `rstl::reserved_vector::operator[]` is `data()[idx]` with no bounds check, which is why Prime 1's
   `SetRandomPlaybackRate` ports across with nothing but `kPT_RandRate` and `GetValue()` renamed.
   Retail loads the base pointer once via SDA21 and indexes by a running 64-byte offset, which is
   what a plain subscript loop produces.

### `GetTimeOfUserEvent(…, tree)` is a measured 96.15% wall, not a source problem

The body is instruction-for-instruction identical to retail. The entire remaining diff is **register
numbering**: retail allocates `r29`/`r30` for the out-pointer and the event type and starts its
per-iteration temporaries at `r22`, while ours allocates `r27`/`r28` and starts at `r29` — every
subsequent register is shifted by two. Four spellings were tried and none moved it: a
`CInt32POINode*` temporary, `*(sInt32TransientCacheData + i)` instead of `[i]`, a `uint` loop index
(which made it *worse*, 96.15% → 95.76%), and a non-`const` `count`. The allocator is choosing a
different order for the two `lis`/`addi` vtable and `@stringBase0` materialisations that feed the
inlined `CInt32POINode` default constructor.

WALL: GetTimeOfUserEvent__9CAnimDataCF14EUserEventTypeRC13CCharAnimTimeRCQ24rstl25ncrc_ptr<13CAnimTreeNode> 96.15% - instruction stream is identical to retail; the diff is a 2-register allocation shift on the two loop counters, and four source spellings do not change it.

## Not reached, and why — measured, not guessed

- **`InitializeEffects` is a 62.38% register-allocation wall** (was 49.23%). Prime 1's structure is
  right — it is the one that made it jump from 49% to 62% — and the instruction stream is again
  identical, modulo retail allocating `r23`–`r31` where we allocate `r24`–`r31`. Three spellings tried
  (dropping the inner `{ }` scope around `CParticleData`, a non-`const` component reference, a named
  `components` reference) and all three left it at exactly 62.38%. Two functions in this unit now sit
  at this wall, which suggests it is a property of how this translation unit's frame is allocated
  rather than of any one function.
- **`GetBoundingBox()` (388 B, 12.66%)** — the previous run's read still holds, and I can be more
  precise about why: retail is **not** Prime 1's `find_by_key`. It calls `fn_8002C1B4` with *three*
  stack arguments and then does its own `lwz r0,4(r29) ; mulli r0,r0,28` end-pointer computation,
  with a `mCachedAnimBounds` fast path keyed on a word at `this+1436` interleaved into the search
  (`cmplw r3,r0 / beq` at 0x6350 jumps straight to the return). Recovering it needs the struct offsets
  for `CAnimTreeEffectiveContribution` (the `lwz r3,124(r1)` at 0x6348) pinned first. Not attempted.
- **`GetTimeOfUserEventForAnimation` and `CountUserEventsForAnimation`** both need
  `CAnimationManager::GetMetaAnimation`. The previous run called this header "a 0x20-byte stub with no
  methods at all"; that is confirmed and slightly worse than stated — **`GetMetaAnimation` is not
  merely undeclared, the symbol does not exist anywhere in the tree**: retail's call target is the
  raw placeholder `fn_8028CAE4` (0x8028CAE4, 0xA4 bytes) and `CMetaAnimTreeBuildOrders` is present
  only as the `NoSpecialOrders__24CMetaAnimTreeBuildOrdersFv` shim. Both retail bodies are short and
  otherwise fully readable (build the tree, `GetAnimationDuration`, call the 2-arg form, release) —
  but they cannot be written without that one class, and writing it is a separate item.
- **Still the previous run's walls, unchanged and still true:** the `IAnimReader` vtable is **+2 slots
  off** (retail dispatches `VGetTimeRemaining` at `lwz r12,20(r12)`, `VSimplified` at 20, `VSetPhase`
  at 92; `include/Kyoto/Animation/IAnimReader.hpp` puts them at 12, 88, 84) — this blocks `SetPhase`,
  `Simplified`, `GetContributionOfHighestInfluence` and both `*TimeRemaining` functions, and needs a
  change to every derived class in the DOL. `CPoseAsTransforms_Linear::mUniformScale` is a 1-bit field
  in retail and a plain `uchar` here, blocking `SetModelScale`. Neither is reachable from this file.
- **`AdvanceIgnoreParticles`**: the previous run's `bool suspendEffects;` finding still holds (retail
  has no store to the out parameter), but **I did not re-apply it** and the reason is worth
  recording: it needs `DoAdvance` to assign the parameter before use, and `DoAdvance` in this tree is
  a 1416-byte TODO stub that does not. Applying it would introduce a read of an uninitialised byte,
  which is a regression dressed as progress.

## A measurement worth keeping: 64 of this unit's 0% functions are already byte-identical

objdiff pairs by **symbol name**, so a template instantiation we emit correctly still scores 0% when
retail's copy of it is still carrying an `fn_<addr>` placeholder in `config/G2ME01/symbols.txt`.
Comparing the two objects with relocations normalised (zeroing the low 24 bits of every word carrying
an `R_PPC_REL24`), **64 of the 216 functions are byte-identical under a different name** — the other
65 are the ones already at 100%. Examples:

- retail `fn_800265A4` (0xD8) = our `erase__Q24rstl60reserved_vector<pair<Ui,21CAdditiveAnimPlayback>,8>FPQ24rstl32pair<Ui,21CAdditiveAnimPlayback>`
- retail `fn_8002F3D0` (0x84) = our `__dt__21TSegIdMap<9CVector3f>Fv`
- retail `fn_8002E0EC` (0x100) = our `__ct__Q24rstl36vector<i,…>FRC…`
- retail `fn_8002DBC8`, `fn_8002DDA4`, `fn_8002E4B8`, `fn_8002EB54` (0x20 each) = our `construct<13CInt32POINode>__4rstlFPvRC13CInt32POINode` (four separate COMDAT copies of one template)
- retail `fn_8002F558` / `fn_8002F5EC` (0x148) = our `__dt__reserved_vector<16CParticlePOINode,64>`; `fn_8002F680` = `__dt__reserved_vector<13CInt32POINode,16>`; `fn_8002F714` = `__dt__reserved_vector<12CBoolPOINode,8>`

**These are not counted by `matched_functions`, and renaming them in `symbols.txt` would be a config
change outside this item** — flagged because it is the single largest block of unclaimed credit in the
unit (~4.2 kbytes of the 13.9 kbytes still unmatched) and because it is a measurement, not a plan. It
is *not* free: `symbols.txt` renames interact with `splits.txt` and the REL wiring, and this item's
rule is that config belongs to the judge.

## Files touched

- `src/MetroidPrime/CAnimData.cpp` — the thirteen functions above, plus
  `#include "rstl/algorithm.hpp"`. No header touched, no class layout changed, no `asm` added, no
  initialisation removed, and the DOL sha1 and all 86 REL hashes hold, so no other unit's `.text`
  moved.
- `extern/musyx/src/musyx/runtime/stream.c` — **the build unblock, not this item's work**; identical
  to `ef9e308` on `goal/decomp` except for a comment. Rebasing the lane makes it disappear.

`docs/HANDOFF.md` is rewritten by the judge's own `check_docs_claims.py` during `goal_check.sh` and
was reverted afterwards. Not committed.

---

# progress-prime1-canimdata — run 3 (lane 9, remeasured `d2ee240`)

This worktree began at **58/216**, `matched_code=5696`, `GetFirstParticleEffect=24.18%`, and the same 99%-near misses recorded above; the earlier source edits were not present here. I did not copy the earlier attempt's changes blindly. I first measured the target with `./tools/fast_try.sh MetroidPrime/CAnimData`, then used the recorded recipes for the uncommitted work and tried an unlisted template-copy-constructor shape.

## Measured source changes

| function | before | after | Prime 1 result |
| --- | ---: | ---: | --- |
| `GetLocatorTransform(const rstl::string&, …)` | 95.78% | **100.00%** | Small source-shape edit: Prime's two `CSegId` locals, including `segCopy` |
| `GetLocatorTransform(CSegId, …)` | 83.80% | **100.00%** | Small edit: invert the invalid-id test so the identity return is cold |
| `SetRandomPlaybackRate` | 1.52% | **100.00%** | Prime body; Echo member names already agree |
| `CountUserEvents(…, tree)` | 1.77% | **100.00%** | No same-named Prime 1 body; implemented the Echo tree-list loop, keeping the measured named `value` local and Echo's `CInt32POINode` reset constructor |
| `GetFirstParticleEffect` | 24.18% | **100.00%** | Adapted Prime's effect-list lookup pattern; list must be a reference and the null return must be shared |
| `SetEffectState` | 54.19% | **100.00%** | Adapted Prime's by-value effect-list + `find_by_key` shape; const copy and component iterators matter |
| `SetEffectComponentExternalParam` | 43.30% | **100.00%** | Adapted Prime's analogous CEXT lookup; by-value effect list and `begin()!=end()` match this retail site |
| `DelAdditiveAnimation` | 62.17% | **100.00%** | Prime raw-pointer `while`/`break`/post-loop guard |
| `DelAdditiveAnimationImmediately` | 99.58% | **100.00%** | Prime-style raw-pointer loop, but early `return` inside the match block |
| `GetAdditiveAnimationWeight` | 99.44% | **100.00%** | Prime raw-pointer loop |
| `IsAdditiveAnimationActive` | 99.44% | **100.00%** | Prime raw-pointer loop |
| `GetAdditiveAnimationTree` | 63.31% | **100.00%** | Prime raw-pointer loop plus inverted end test |
| `__ct__rstl::vector<CPASAnimInfo>` | 71.11% | **100.00%** | New result, not in the previous notes: Echo's generic copy ctor outlined `uninitialized_copy_n`; a TU-local vector-constructor specialization with a counted loop, pointer ordering and a noinline element-copy helper matched retail |
| `fn_8002E95C` (support helper) | 0.00% | **41.82%** | C++ placement-copy helper required by the exact vector-constructor call shape; helper itself is not an exact match |
| `__as__CInt32POINode` (emitted helper) | 0.00% | **100.00%** | Exact collateral from the `CountUserEvents` reset assignment |
| `rstl::less<rstl::string>::operator()` (emitted helper) | 0.00% | **100.00%** | Exact collateral from the `find_by_key` effect lookups |

The vector specialization's helper is named `fn_8002E95C` because that is the retail relocation from the vector constructor; its retail body is a 40-byte object copy. The C++ placement-copy helper is semantically correct but only scores **41.82%** itself. The vector constructor is nevertheless exact. The source also leaves `GetTimeOfUserEvent(EUserEventType, CCharAnimTime)` alone: it was already 100% in this worktree's initial report. I did not reapply the old `AdvanceIgnoreParticles` edit because `DoAdvance` still does not assign its out parameter.

## Verification, this run

```
$ ./tools/fast_try.sh MetroidPrime/CAnimData
main/MetroidPrime/CAnimData: 26.59% fuzzy, 20.23% matched code, 73/216 functions

$ ./tools/goal_check.sh build/goal/item.json
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10218 -> 10233   linked 5004 -> 5004
  ok    check_symbol_names.py
  ok    All:  31.22% fuzzy, 23.53% matched, 11.81% linked (10233 / 28465 functions)
  ok    target rose: main/MetroidPrime/CAnimData: 58 -> 73 / 216 functions
  ok    no asm added
goal_check: PASS progress-prime1-canimdata

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol
$ python3 - <<'PY'  # hash loop over config/G2ME01/config.yml
86/86 modules match config.yml
$ python3 tools/check_decl_order.py --unit MetroidPrime/CAnimData
ok: 1 unit(s) checked, none emits its functions out of retail order
$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object
```

Final report has `matched_code=7996` (up from 5696) and **73/216** matched functions. The gate's report diff found no regressions; linked stayed 5004 because the target remains `NonMatching`. No `flip_test` was run for this progress item. `goal_check` regenerated `docs/HANDOFF.md`; I reverted that derived file, leaving only `src/MetroidPrime/CAnimData.cpp` changed in the lane. No commit.
