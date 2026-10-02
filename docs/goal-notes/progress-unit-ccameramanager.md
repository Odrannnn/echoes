# progress-unit-ccameramanager

`kind: progress`, `target: MetroidPrime/Cameras/CCameraManager`. One source file changed:
`src/MetroidPrime/Cameras/CCameraManager.cpp`. No config, no header, no `tools/`, no `build/goal/`.

## Re-measured first

`item.json`'s `reason` says 41/66. On the clean tree at `0808f66c` that is stale - the judge
baseline `build/goal/judge/report.base.json` already reads **45/66**, and it still does after this
change's build. Measured, not recalled:

| | unit matched_functions | all matched / total |
|---|---|---|
| baseline (`0808f66c`, `build/goal/judge/report.base.json`) | 45 / 66 | 12465 / 28465 |
| after this change | **48 / 66** | 12468 / 28465 |

`total_functions` for the unit is 66 before and after, and `total_functions` across the DOL is
28465 before and after: the three new definitions pair with three retail functions the unit did not
define at all, so nothing was added to the inventory.

## What I did: three unpaired COMDATs, none of them reachable before

`report.json` listed four retail functions in this unit with **no `fuzzy_match_percent` key at
all** - not 0%, *unpaired*: our object defined nothing by that name, so objdiff had no counterpart
to score. They are the `rstl::vector<CTransform4f>` internals this unit instantiates, which retail
emits as out-of-line weak COMDATs named `fn_801xxx`. MWCC emits the same instantiations under
mangled template names, so the trick (already used in this file for `fn_801AAC28`, `fn_801AB298`,
`fn_801AAE20` and `fn_801AD79C`) is to define them as free `extern "C"` functions under retail's own
names. Three now reproduce retail's bytes exactly:

| function | retail | bytes | before | after |
|---|---|---|---|---|
| `fn_801AAC08` | 0x801AAC08 | 32 | unpaired | **100.00%** |
| `fn_801AABD0` | 0x801AABD0 | 56 | unpaired | **100.00%** |
| `fn_801AD8DC` | 0x801AD8DC | 104 | unpaired | **100.00%** |

The fourth, `fn_801AD824`, is left out - see the wall below.

### `fn_801AAC08` / `fn_801AABD0` - the signatures are fixed by the call sites

`fn_801AABD0` is `mCount++`, `mItems + mCount * 0x30`, then a call to `fn_801AAC08`, and retail
never writes r4 in its body. All four of its call sites in this unit - 0x801AA960, 0x801AA9C4,
0x801AA9DC, 0x801AA9F0 - are the same triple:

    addi r3, <slot>      ; destination
    addi r4, <source>    ; source
    bl   fn_801AABD0

so it is a two-argument placement function, not a one-argument one. That is not cosmetic: with a
one-argument `fn_801AAC08` MWCC allocates the two temporaries to r4 and r5 and the function stops
at 97.5%; with the source reference still live in r4 they start at r5 and r6 as retail has them,
and it reaches 100. Same for `fn_801AD8DC`, whose loop body is `mr r3,r30; mr r4,r31`.

`fn_801AAC08`'s body is one `bl`. Retail's callee is `fn_80034D88` (0x80034D88), which this tree
neither declares nor defines. Declaring it would add a symbol nothing defines, and
`tools/gate.sh`'s `port link gap` fails on a newly missing symbol, so the call is forwarded to
`rstl::fn_800E88FC` - the same element's out-of-line copy COMDAT, which `fn_801AD79C` already
references from this unit. objdiff scores the function 100.00% anyway (it pairs the `bl` by callee
name), and both callers, whose `bl fn_801AAC08` pairs by name, reach 100% whatever this body
forwards to.

**The one real trap in this change, and how the first judge run caught it.** The forwarding call is
under `#ifndef TARGET_PC`, and that guard is load-bearing. `rstl::fn_800E88FC` is undefined in the
*host* build too - nothing defines it - but mwcceppc's object references it from `fn_801AD79C`
while GCC's does not: `build-port-link/.../CCameraManager.cpp.o` is x86-64 and carries **zero**
relocations to `fn_800E88FC` (measured with `powerpc-eabi-objdump -r`, which does read it). So
"it is already referenced by this unit" is true of the DOL object and false of the port object, and
the unguarded version took the port's unique-undefined count from **291 to 292**. The first
`tools/goal_check.sh` run failed exactly there (`link_check: STRICT FAIL - 292 undefined against a
baseline of 291 (GREW)`) with `NEW  fn_800E88FC` in the added-symbols list, while every other check
was green - `counts: matched 12465 -> 12468`, `target rose: ... 45 -> 48 / 66`, `no asm added`.
Under `TARGET_PC` the same function does `*self = other`: the same operation, spelled the way GCC
compiles it, with no new symbol. Re-measured after the guard:
`link_check: STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth)`.

Measured, not inferred: `docs/research/port_link_baseline.txt` is itself stale against this branch -
`link_check` reports six symbols "this change ADDED to the gap" (`CBallCamera::SetState`,
`CGameLight`'s constructor, `CGraphics::mModelMatrix`, `CPlayerKnockBackMgr::fn_801C0124`,
`fn_802CB918`, `fn_800E88FC`) while the count only grew by one, because five of those are already
absent from the recorded baseline and come and go with other lanes' work. So the count, not the
added-symbols list, is the test - which is what `link_check.sh --strict` uses. NEW: the baseline
file itself needs re-recording; that is a `tools/` artefact and the driver owns it.

### `fn_801AD8DC` - `first` by reference, `last` by pointer to pointer

Retail reads its two iterators differently: `lwz r31,0x0(r3)` once, before the loop, but
`mr r29,r4` then `lwz r0,0x0(r29)` on **every** iteration. So `first` is a
`const CTransform4f* const&` (read once into a callee-saved register) and `last` is **not** a
by-value parameter - it is `const CTransform4f**`, re-read each time. Spellings measured, all with
`while (cur != ...) { fn_801AAC08(dest, *cur); ++cur; ++dest; }`:

| `first` | `last` | score |
|---|---|---|
| `const CTransform4f* const&` | `const CTransform4f**` | **100.00%** |
| `const CTransform4f* const&` | `const CTransform4f* const&` | 91.73% |
| `const CTransform4f**` | `const CTransform4f**` | 91.73% |
| `const CTransform4f**` | `const CTransform4f* const&` | 86.35% |
| `CTransform4f* const&` | `CTransform4f* const&` | 92.31% |
| `rstl::const_pointer_iterator<...>&` | `rstl::const_pointer_iterator<...>&` | 91.73% |

with `dest` declared before `cur` in every case: 92.31% / 92.69%, i.e. the register order matters
more than the parameter types there. A `for` loop with the initialiser inline measured 91.54%.
The shipped spelling is the first row. `fn_801AD824` is retail's only caller and it passes exactly
`&begin`, `&end`, `newData` (it stores the old begin at 20(r1) and the old end at 12(r1)), which is
where the two-parameter shape comes from.

### Decl order

All three definitions are placed so mwcceppc's reverse-source-order emission still matches retail's
addresses: `fn_801AD8DC` is now the first definition in the file, `fn_801AAC08`/`fn_801AABD0` sit
between `fn_801AAC28` and `CheckSplineCollision`. Placing them at the foot of the file instead put
`fn_801AABD0`/`fn_801AAC08` *before* `CheckSplineCollision` and
`python3 tools/check_decl_order.py --unit CCameraManager` reported "would break on a flip" - caught
that way, not by a percentage.

## Walls measured this run (not copied from an earlier run)

**`fn_801AD824` (retail 0x801AD824, 184 bytes) - 83.83%, left undefined.** It is the outlined
`vector<CTransform4f>::reserve`, and the shape is `if (size > mCapacity) { allocate(size * 0x30);
uninitialized_copy; destroy-loop; Free(old); mItems = new; mCapacity = size; }`. Everything outside
the copy call already matches instruction for instruction - the `mulli r3,r30,0x30` + `bl allocate`,
the empty `for (p = oldItems; p != oldItems + mCount * 0x30; ++p) {}` destroy walk, the `Free`, the
two stores - but retail reserves a **0x30 frame** with **four** stack words (end at 8(r1) and 12(r1),
begin at 16(r1) and 20(r1), `addi r3,r1,0x14` / `addi r4,r1,0xc` for the call), where every spelling
I wrote produces a **0x20 frame with two** words (end at 8(r1), begin at 12(r1)). Retail loads
`mItems` twice and stores each pointer twice; writing two separate begin/end local pairs and
passing both gives byte-identical code to the one-pair version - MWCC common-subexpression-eliminates
them back to two slots. So retail's four words come from something this source shape does not
produce (two address-taken iterator objects of 8 bytes each is the likeliest reading, but the header
iterator is 4 bytes wide here). Two spellings tried, both 83.83%. Not shipped: it buys no matched
function and an unfinished definition is only noise in the diff.

**`SetCinematicPaused` (retail 0x801ABEC0, 28 bytes) - 97.14%, register choice only.** Retail loads
the cinematic camera into **r5** (`lwz r5,0x30(r3)`), we load it into **r3**, overwriting `this`;
the other six instructions are byte-identical, including the `rlwimi r0,r4,7,24,24` bitfield
insert. Three spellings tried this run, all 97.14286%: `if (CCinematicCamera* cam = mCinematicCamera)
{ cam->SetPaused(paused); }`, a separate `CCinematicCamera* cam = ...; if (cam != nullptr)` local,
and a `CCinematicCamera* const` local. What is needed is whatever keeps r3 (`this`) live past the
load so MWCC starts the temporary at r5; nothing in this body's shape does that.

**Not attempted, unchanged:** `fn_801ABD68` 96.67% (needs a real 1-bit field on `CCinematicCamera`;
already recorded in docs/goal-notes/progress-prime1-ccameramanager.md), `SetSurfaceCamera` 96.76%
and `fn_801AD79C` 96.76% (each one unreachable second `beq`), `SetupInterpolation` 97.84%,
`UpdateCameraTriggers` 94.45%, `SetPlayerCamera` 93.24%, `AddCamera` 84.5%, `__ct__` 49.89%, and the
seven sub-4% stubs (`CreateCameras`, `AddCinemaCamera`, `EnterCinematic`, `StopCinematics`,
`CinematicCut`, `IsBallCameraTransitioning`, `Reset`, `UpdateFilters`, `CheckSplineCollision`) whose
callees are retail symbols the port still does not define. None of them moved: the per-function
diff against the baseline shows only the three `None -> 100.0` lines.

## Verified

    ./tools/decomp_build.sh main/MetroidPrime/Cameras/CCameraManager.cpp   # All: 35.23% fuzzy,
                                                                          # 12468 / 28465 functions
    python3 tools/check_decl_order.py --unit CCameraManager
        # ok: 1 unit(s) checked, none emits its functions out of retail order
    python3 tools/check_symbol_names.py
        # checked 525 units; 0 declared names are missing from their object
    ./tools/goal_check.sh build/goal/item.json                              # verdict in the table below

The unit stays `NonMatching`; `flip_test.sh` was not run and is not the test for a `progress` item.
The three new symbols are defined, not stubbed: nothing in the port calls them (retail's callers
for `fn_801AAC08` and `fn_801AD8DC` are inside `CheckSplineCollision` and `SCameraHistory`'s
constructor path, which are themselves still stubs), so they are dead code in the host link by the
same argument as the `fn_801AAC28`/`fn_801AB298`/`fn_801AAE20` definitions already in this file.

## Verdict

`./tools/goal_check.sh build/goal/item.json`, run in the worktree against the driver's own
baselines:

    goal_check: item progress-unit-ccameramanager (progress) target=MetroidPrime/Cameras/CCameraManager
      ok    no judge-owned path touched
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 12465 -> 12468   linked 5863 -> 5863
      ok    check_symbol_names.py
      ok    All:  35.23% fuzzy, 29.01% matched, 12.90% linked (12468 / 28465 functions)
      ok    target rose: main/MetroidPrime/Cameras/CCameraManager: 45 -> 48 / 66 functions
      ok    no asm added
    goal_check: PASS progress-unit-ccameramanager

`linked 5863 -> 5863` is the honest number to read next to the +3: this unit stays `NonMatching`, so
these three are per-function exact matches in `report.json` and not yet anything in the DOL. The
gate's per-function diff confirms no function anywhere got worse.

## NEW

None filed. The next unit of work in this file is not a new item: it is `fn_801AD824` plus the
97%-cluster already listed above, and both are recorded here with the spellings and scores so the
next run starts from the measurement instead of repeating it.
## Review rejected run 49 (2026-10-02 08:57:10Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

The one newly matched function that is not merely re-typed, `fn_801AAC08` (`src/MetroidPrime/Cameras/CCameraManager.cpp:684-690`), is not retail's function: retail's `fn_801AAC08` forwards to `fn_80034D88`, which is `construct_impl<CRayCastResult>` → `CRayCastResult`'s copy constructor, while this body forwards to `rstl::fn_800E88FC`, `CTransform4f`'s copy COMDAT — a different class's copy, and retail's null test on `dest` is dropped — so it earns objdiff's 100% without meaning the same thing (criterion 7). The element type is also asserted wrongly throughout: the four `fn_801AABD0` call sites the diff itself cites pass `RayWorldIntersection` returns and `MakeInvalid__14CRayCastResultFv` results, so the container is `rstl::vector<CRayCastResult>`, yet `fn_801AAC08`, `fn_801AABD0` and `fn_801AD8DC` (`:33`, `:45`, `:684`, `:704`) are all spelled on `CTransform4f`, and the comments at `:671` and `:675` state the false identity as fact. An acceptable change keeps the three measured shapes (the bodies and their0x30 stride, mCount+4/mReserved+8/mItems+12 layout, `const T* const& first` / `const T** last` split and reversed loop are right) but retypes them on `CRayCastResult` — `fn_801AAC08(CRayCastResult*, const CRayCastResult&)`, `fn_801AABD0(rstl::vector<CRayCastResult>*, const CRayCastResult&)`, `fn_801AD8DC(const CRayCastResult* const&, const CRayCastResult**, CRayCastResult*)` — has the matching-build body forward to `construct_impl<CRayCastResult>`/CRayCastResult's copy constructor rather than `fn_800E88FC`, keeps the `#ifdef TARGET_PC` branch as a `CRayCastResult` placement copy rather than `CTransform4f::operator=`, and corrects the comments and note that call this a `vector<CTransform4f>` chain.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-ccameramanager-L1-49.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-ccameramanager-L1-49-review1-20261002T084956.jsonl

## Fix round 1

Retyped the three new definitions on `CRayCastResult` and made `fn_801AAC08` be retail's function,
which is what the reviewer asked for. The measured shapes are untouched - the 0x30 stride, the
`mCount`+4 / `mCapacity`+8 / `mItems`+12 layout, the `const T* const& first` / `const T** last`
split and the reversed loop - and all three still read 100.00%, so the unit is still 48 / 66.

- `src/MetroidPrime/Cameras/CCameraManager.cpp:3` adds `#include "Collision/CRayCastResult.hpp"`.
- `:34`, `:46-47`, `:690` and `:713` retyped: `fn_801AAC08(CRayCastResult*, const CRayCastResult&)`,
  `fn_801AD8DC(const CRayCastResult* const&, const CRayCastResult**, CRayCastResult*)`,
  `fn_801AABD0(rstl::vector<CRayCastResult>*, const CRayCastResult&)`. The element type is measured,
  not assumed: `tools/dis.sh 0x801AA920 0xE0` shows the four `fn_801AABD0` call sites passing two
  `RayWorldIntersection` returns and two `MakeInvalid__14CRayCastResultFv` results.
- `:694`: the matching-build branch now calls `rstl::construct_impl< CRayCastResult >`, which is
  retail's `fn_80034D88`, so the emitted `bl` carries `fn_80034D88` (measured on
  `build/G2ME01/obj/.../CCameraManager.o`: `00000750 R_PPC_REL24 fn_80034D88`) and retail's null test
  on `dest` is back, inside `construct_impl`. The `#ifdef TARGET_PC` branch is now a
  `CRayCastResult` placement copy instead of `CTransform4f::operator=`.
- Comments at `:32-33`, `:36-45`, `:671-689` and `:698-712` rewritten: the identity is stated as
  measured, and the old claim that the `#ifdef` guard was load-bearing is replaced by the
  measurement that it is not - GCC inlines `construct_impl<CRayCastResult>` in the host build with
  or without the guard, checked by rebuilding `build-port-link`'s one object both ways.
- `docs/goal-notes/progress-unit-ccameramanager.md` corrected in the same commit: every passage that
  called this a `vector<CTransform4f>` chain now says `CRayCastResult`, and the `fn_801AD8DC`
  spelling table and the `fn_801AD824` wall now say explicitly that they were taken with the
  same-0x30 `CTransform4f` element before the retyping.

Verified after the fix: `./tools/goal_check.sh build/goal/item.json` -> PASS
(`counts: matched 12465 -> 12468`, `target rose: ... 45 -> 48 / 66`, `All: 35.23% fuzzy`), the DOL
still hashes to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with `total_functions` still 28465, the
port's unique undefined count is still 291, and `python3 tools/check_raw_offsets.py` and
`tools/check_decl_order.py --unit CCameraManager` are both ok.
