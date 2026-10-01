# progress-unit-cscriptwater

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptWater`. The unit stays `NonMatching`;
`flip_test.sh` was not run. **Measured: 5/36 -> 12/36 functions matched** in
`build/report.json`, total tree 11654 -> 11661. `tools/goal_check.sh build/goal/item.json`
prints `PASS`.

Diff is two files: `src/MetroidPrime/ScriptObjects/CScriptWater.cpp` and the one declaration
`include/MetroidPrime/ScriptObjects/CScriptWater.hpp` (`SetMorphing`'s parameter renamed to `m`,
to match the definition; `top-level const` on a parameter is not part of the type, so no other
call site changes).

## Per function: before -> after, and what produced it

| function | before | after | the change that did it |
|---|---|---|---|
| `GetSplashIndex__12CScriptWaterCFf` | 98.55% | **100%** | `scale *= 3.f` as a statement, then `if (idx >= 3)` instead of `if (index > 2)` |
| `SetMorphing__12CScriptWaterFb` | 99.12% | **100%** | parameter named `m`, `if (m != mMorphing)` (not reversed) |
| `GetSplashEffectScale__12CScriptWaterCFf` | 83.60% | **100%** | `scale - static_cast<float>(floor(scale))`, not `CMath::FloorF` |
| `CanRippleAtPoint__12CScriptWaterCFRC9CVector3f` | 79.40% | **100%** | call `GetTriggerBoundsWR()` twice, once per axis |
| `GetSortingBounds__12CScriptWaterCFRC13CStateManager` | 31.71% | **100%** | `const CAABox& bounds = mSurfaceBounds;` as a named reference |
| `ClearSplashInhabitants__12CScriptWaterFv` | 39.43% | **100%** | hand-written node walk, not `rstl::list::clear()` |
| `InhabitantIdle__12CScriptWaterFR6CActorR13CStateManager` | 3.45% | **100%** | base call, then `mgr.SendScriptMsg(&actor, GetUniqueId(), kSM_XINF, kInvalidUniqueId)` |

Nothing regressed; `linked` stayed at 5625 and the DOL sha1 and all 86 RELs are unchanged.

## The five findings worth keeping (each was measured, not guessed)

**1. `CMath::FloorF` has no definition anywhere in this tree, and retail does not call it.**
`include/Kyoto/Math/CMath.hpp:95` declares `static const float FloorF(float x);` and nothing
defines it - `grep -rn FloorF` over `src/` and `include/` finds only the declaration, plus callers
in `CScriptWater.cpp` and `CMapWorld.cpp`. Retail's `GetSplashEffectScale` calls the C library
`floor` (the **double** one, `build/G2ME01/asm/Runtime/s_floor.s`) and rounds with `frsp`
afterwards. Spelling it `scale - static_cast<float>(floor(scale))` reproduces that exactly;
`scale -= floor(scale)` alone is 97.21% (it drops the `frsp`), and `CMath::FloorF` is 83.60%
(it emits a call to a symbol nobody defines). **`CMath::FloorF` is a stub whose callers are all
broken; CMapWorld.cpp still uses it and probably should not.** Not fixed here - out of scope for
this item, and it is a shared unit.

**2. Naming a parameter changes CodeWarrior's register allocation.** `SetMorphing` was 99.12%
with a pure register-allocation difference: retail keeps the incoming bool in `r4` throughout
(`clrlwi r4,r4,24` ... `cmplw r4,r0` ... `cntlzw r0,r4`), ours copies it to `r6`. Measured:

    if (morphing != mMorphing)                        99.12%   (base)
    if (mMorphing != morphing)                        86.18%   (m-first)
    void SetMorphing(const bool m) { if (m != mMorphing) ... }   100.00%   (arg-first)
    const bool m = morphing; ...                      100.00%   (local-copy)

The winning factor is comparing the **argument** against the member, not the reverse.

**3. Retail does not CSE `GetTriggerBoundsWR()` across the two axes of `CanRippleAtPoint`.**
It emits two full calls, each returning into its own stack slot (`addi r3,r1,0x20` then
`addi r3,r1,0x8`). Writing one `const CAABox bounds = ...` and reading both axes out of it is
79.40%; calling it once per expression is 100%. A CSE-shaped `const CAABox bx/by` spelling is
also 100%, so what matters is that there are two calls, not the names.

**4. A named reference to a member changes how the compiler passes it.**
`GetSortingBounds` at 31.71% was a `TODO` returning `mSurfaceBounds`. Retail does
`addi r4,r4,0x210` once, reads `0xc/0x10/0x14(r4)`, and passes `r4` straight to
`__ct__6CAABox`. Lifting the member into `const CAABox& bounds` first is 100%; spelling the
minimum as `mSurfaceBounds.GetMinPoint()` inline is 89.81%, and the Prime 1 donor's whole body
(with its `mFogMagnitude`/`mFogBias` fog term) is only 62.71% - **Echoes dropped the fog term**,
this is not Prime 1's function.

**5. `rstl::list::clear()` is not what retail's `ClearSplashInhabitants` compiles to.**
`clear()` is `erase(begin(), end())`, and `erase(iterator, iterator)` is out-of-line, so it
becomes one `bl` to a range-erase helper. Retail instead walks the nodes inline, reading each
node's successor into `r31` *before* the erase and using that. Measured:

    mWaterInhabitants.clear();                              39.43%
    while (it != end) it = mWaterInhabitants.erase(it);      84.52%
    while (it != end) it = mWaterInhabitants.do_erase(it);   84.52%  (uses do_erase's return)
    node* next = it->mNext; do_erase(it); it = next;        100.00%  (reads the successor first)
    mWaterInhabitants.erase(it++);                           89.76%

The 84.52 vs 100.00 gap is exactly the successor read: using `do_erase`'s return value is *not*
what retail does.

## Left undone, with the spellings already tried

**`CalculateRenderBounds__12CScriptWaterFv` - 90.05%, and that is a wall.** 52 of 54 instructions
match; the two that do not are in the middle of the load/add block:

    retail 13  lfs  f4, 0x58(r31)      ours 13  fsubs f4,f1,f2
    retail 14  fsubs f1, f1, f0        ours 14  lfs  f3, 0x58(r31)

and correspondingly at 19/20/22/23 (`fadds f0,f6,f3` / `fadds f7,f5,f2` / `fadds f6,f1,f2` /
`stfs f8,0x18(r1)` in a different order). Only load scheduling, no register or size difference.
Twenty-five spellings tried, best is the pre-existing one at 90.05%:

    base90 (min/max + translation, unchanged)                 90.05%
    swap-args                                                    90.05%
    comp-adds (per-component +, no operator+)                   85.16%
    comp-adds-z-first                                            85.35%
    z-plusminus (named up/down floats)                          82.18%
    z-plusminus-args                                             81.82%
    z-plus-then-add                                              81.82%
    prime1-exact (the donor body verbatim)                      89.18%
    prime1-order (donor order, const everywhere)                89.18%
    decl-prim1-order-nonconst                                    89.18%
    slot-order (localMax, worldMax, localMin, worldMin)         82.22%
    slot-order-maxref                                            89.09%
    slot-order-trans-last                                        82.22%
    all-const                                                     77.82%
    trans-first-const                                            77.64%
    separate-add-fn (lo/hi then +=)                             77.93%
    add-op                                                       77.47%
    add-op-min-only                                               77.47%
    max-first-temp                                               85.38%
    max-then-min                                                 82.22%
    max-then-min-noref                                           88.95%
    bounds-ctor-max-first                                        85.38%
    ctor-max-then-min                                            22.91%
    no-locals                                                    82.22%
    declare-localmin-first                                       82.18%
    trans-last / swap-args variants                             <= 90.05%

**`GetSplashSound__12CScriptWaterCFf` - 85.71%, also a wall.** 12 of 14 instructions match. The
whole difference is where the LR reload lands:

    retail 0x800D8674  lhz r3, 0x2c8(r3)
    retail 0x800D8678  lwz r31, 0xc(r1)
    retail 0x800D867C  lwz r0, 0x14(r1)
    ours   0x23c       lwz r0, 20(r1)     <- ours hoists the LR reload above the lhz
    ours   0x240       lhz r3, 712(r3)

Both reload LR and both keep `r31`; the compiler just schedules the reload early. Twelve
spellings tried, all 85.71%: base, `const int idx` local, `const TSfxId` local, `data()[idx]`,
`+ 0`, `static_cast<TSfxId>`, `static_cast<ushort>` return type, `if (true)`, `if (idx < 0 ||
idx >= size())` guard (49.14%, worse), `it++`-style, `size_t` subscript. Note the *sibling*
`GetSplashEffect` is already 100% and differs only in the return type, so this is not a
`reserved_vector` access problem.

**`InhabitantAdded` / `InhabitantExited` - not attempted to completion.** The retail asm is fully
readable and the shape is clear, so a future run can go straight at it:

- both call the `CScriptTrigger` base first, then
  `actor.SetInFluid(mgr, <true|false>, GetUniqueId())`;
- both then test `actor.mFluidIds.empty()` (the `lwz 0x110(actor)` / `neg`-`or`-`srwi` idiom, i.e.
  `GetFluidCount() == 0` in `InhabitantAdded`, `== 0` in `InhabitantExited` - note `InhabitantExited`
  uses a plain `cmpwi r0,0`, `InhabitantAdded` uses the `neg/or/srwi` bool idiom);
- inside, a **virtual** call through vtable slot `0x8c` (`TypesMatch`, on `CActor*`, returns a
  `CEntity*` in `r3`, tested with `clrlwi. r0,r3,24`) against something that means
  `CGameCamera`;
- then `mgr.SendScriptMsg(&actor, GetUniqueId(), kSM_XENF /*0x58454e46*/ , kInvalidUniqueId)` -
  **`kSM_XENF`, not `kSM_XINF`**; and in `InhabitantExited` the message is `0x58455846` =
  `kSM_XEXF`;
- then `TCastToPtr<CGameCamera>(&actor)` (`bl "TCastToPtr<11CGameCamera>__FR7CEntity"`, result
  null-tested) and a **virtual** call through vtable slot `0x84` with `(TUniqueId, CStateManager&)`
  arguments - a camera "water effects entered" hook.

Blocked on this repo's own headers, not on the logic: there is no `CGameCamera.hpp` in
`include/MetroidPrime/Cameras/`, no `kEntityTypes::CGameCamera`, and no `InitializeWaterEffects`
on any camera class, so the two virtual hooks and the TCastToPtr type cannot be named. Guessing
their names would add a declaration nothing implements - an undefined symbol in the port - which
is exactly the kind of stub the review prompt rejects.

WALL: CalculateRenderBounds__12CScriptWaterFv 90.05% - 52/54 instructions match; the two that
  do not are a `lfs`/`fsubs` pair swapped in the load schedule and the three dependent `fadds`
  reordered with them. 25 spellings tried, all <= 90.05%; register allocation and instruction
  scheduling only, no size or operand difference.
WALL: GetSplashSound__12CScriptWaterCFf 85.71% - 12/14 instructions match; the only difference is
  that the compiler hoists the `lwz r0, 0x14(r1)` LR reload above the `lhz`. 12 spellings tried,
  all 85.71%.

## Method

`.tmp/opencode/score.py <fn>` rebuilds the unit and prints its per-function scores;
`.tmp/opencode/try.py <fn> <marker> <variants.py>` rewrites one function to each spelling in turn
and prints the score, restoring the file at the end. `.tmp/opencode/sbs.py <fn>` prints retail's
and our instructions side by side, which is what turned "83.60%" into "retail calls the C library
`floor` and rounds with `frsp`". Those are scratch, outside `tools/`, and not part of the diff.

---

# Run 3 (lane L6, 2026-10-01)

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptWater`. Re-measured first on this
tree: **12/36 matched** before, same as the item text - nothing had landed, so not STALE.

**Measured after: 12/36 -> 15/36.** Tree `12131 -> 12134` (judge's own numbers, printed by
`tools/goal_check.sh`): `PASS`, `linked 5860 -> 5860`, no asm, no judge-owned path touched.
Diff is one file, `src/MetroidPrime/ScriptObjects/CScriptWater.cpp`, plus the one added
`#include "MetroidPrime/TCastTo.hpp"`. `docs/HANDOFF.md` is the judge's rewrite of its own
derived counts, not mine.

| function | before | after | the change that did it |
|---|---|---|---|
| `GetNextConnectedWater__12CScriptWaterCFRC13CStateManager` | 2.33% | **100%** | Prime 1's body, and the cast result must be **null-tested in an `if`**, not returned |
| `AddToRenderer__12CScriptWaterCFRC13CStateManager` | 1.61% | **100%** | fluid-type-2 -> `Render(mgr)` branch; `CUnitVector3f(CVector3f(0,0,1), kN_Yes)` |
| `Touch__12CScriptWaterFR6CActorR13CStateManager` | 1.15% | **100%** | `!!actor.GetFluidCount()`, surface-crossing test, `FluidFXThink(kFS_EnteredFluid,...)` |
| `UpdateSplashInhabitants__12CScriptWaterFR13CStateManager` | 0.99% | 97.43% | node walk with the successor read into a local; `FluidFXThink(kFS_InFluid/kFS_LeftFluid,...)` |

Nothing regressed anywhere; the DOL sha1 and all 86 RELs are unchanged (gate.sh checks both).

## The findings worth keeping

**1. Retail tests a `TCastToPtr` result before returning it.** `GetNextConnectedWater` is 96.67%
with `return TCastToConstPtr<CScriptWater>(mgr.GetObjectById(...))` and 100% with that value
bound in an `if`. Retail's frame has the `cmplwi r3,0` / `beq` pair that continues the loop
instead. Prime 1's donor already has the nested `if`, so the donor was right and my first
reading of it was not - the *same* pattern as `ClearSplashInhabitants`' hand-written node walk
from run 1.

**2. `CUnitVector3f`'s `kN_Yes` ctor is what emits retail's out-of-line `Normalize`.** Retail's
`AddToRenderer` builds a `CVector3f(0,0,1)` in a stack slot, calls `Normalize__9CVector3fFv` on
*that slot*, then copies the three components into the plane's normal at +20. Spelling it

    CVector3f up(0,0,1); up.Normalize();          -> 95.02%  (two Normalize calls: the ctor
                                                       normalises the copy again)
    CUnitVector3f(up, kN_No)                      -> 95.02%
    CUnitVector3f(CVector3f(0,0,1), kN_Yes)       -> 100.00%

so the literal must be the thing constructed, not a separately normalised temporary. Related:
retail's branch is `GetPreRenderClipped()` (`lbz 0x150`, bit 31) then
`mFluidPlane->GetFluidType() == 2` -> a **virtual `Render(mgr)`** through slot `0x2c`; that
whole fluid-type-2 early-out does not exist in Prime 1's donor.

**3. `Touch` reads the actor's fluid count *before* the base call and re-tests it after the
push_back.** Retail's `neg`/`or`/`srwi r31,r0,31` idiom, computed before `CScriptTrigger::Touch`,
re-tested after the list insert. The spelling matters and `!!` is the one that reaches 100%:

    const bool wasInFluid = actor.GetFluidCount() != 0;   96.41%
    const int fluidCount = actor.GetFluidCount(); ... != 0  95.80%
    const bool wasInFluid = !actor.GetFluidCount();       97.36%
    const bool wasInFluid = !!actor.GetFluidCount();      100.00%

Also: the trigger-material test is `actor.GetMaterialList().HasMaterial(kMT_Trigger)` (retail
`and 0x68(actor),4`), the already-tracked case sets `(*it).second = true` and **returns**, and
the surface test is `touchMin.Z <= surfaceZ && touchMax.Z >= surfaceZ` where `surfaceZ` is
`GetTriggerBoundsWR().GetMaxPoint().GetZ()` - a `>=`/`<=` pair, not `>`/`<`.

**4. The frame is 80 bytes only if the touch bounds are a `const&`, not a value.**
`const rstl::optional_object<CAABox> touchBounds = ...` costs 112 bytes of frame and a 24-byte
copy (72.90%); `const rstl::optional_object<CAABox>& touchBounds = ...` gives retail's 80
(92.79% -> the rest came from finding 3). Same pattern as run 1's `GetSortingBounds`.

## Measured, not a wall, but not finished

**`UpdateSplashInhabitants` - 97.43%, 100 instructions vs retail's 101.** The only differences
are register assignment and one hoisted load: retail keeps the node pointer in `r31` and the
successor in `r30` and does `mr r31,r30` at the bottom; ours reloads `it->mNext` after the erase
(`lwz r31,4(r31)`), and the `r26`/`r28` roles are swapped. Twelve spellings tried:

    node walk + successor local (kept)              97.43%
    successor read as it->mNext in the increment    93.69%
    successor local declared after the `if`         92.87%
    end node hoisted into a local                  94.33%
    iterator-typed successor (get_node())           build fail (node* has no get_node)
    const node* successor / const iterator          build fail (mwcc 2.7 rejects both)
    int crossedSurface + `!= 0` tests               95.89%
    crossedSurface declared after `actor`           92.87%
    endNode local + no successor local              94.33%

The 97.43% body is the whole function; only scheduling differs, so a future run that wants it
should look at **the order the successor is declared in**, which is the one axis not exhausted
(it was moved twice; neither move helped).

**`InhabitantAdded` / `InhabitantExited` - run 1's blocker is wrong.** Run 1 recorded "there is no
`CGameCamera.hpp` in `include/MetroidPrime/Cameras/`, no `kEntityTypes::CGameCamera`, no
`InitializeWaterEffects`". Re-measured: **`CGameCamera.hpp` does exist**
(`include/MetroidPrime/Cameras/CGameCamera.hpp`) and the two virtual hooks the retail asm calls
are already declared there, guessed:

    include/MetroidPrime/Cameras/CGameCamera.hpp:32  virtual void UnkVtable84();
    include/MetroidPrime/Cameras/CGameCamera.hpp:33  virtual void UnkVtable88(TUniqueId fluidId);

and both are **100% matched** (`CGameCamera` 25/35, `CFirstPersonCamera` 14/17), so the slots at
`0x84` / `0x88` are real. What retail actually calls, confirmed against
`build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptWater.o`:

- `InhabitantAdded`: `CScriptTrigger::InhabitantAdded` base, `actor.SetInFluid(mgr, true, uid)`,
  then the `GetFluidCount()==0` guard, then a virtual through `0x8c` on `this` taking
  `(CActor&, CStateManager&)` and returning something bool-testable - **that is
  `CScriptTrigger::ShouldSendScriptMsgs`**, not a camera `TypesMatch`; then
  `SendScriptMsg(&actor, uid, kSM_XENF, kInvalidUniqueId)`, then
  `TCastToPtr<CGameCamera>(&actor)` and a virtual through **`0x84`** with `(TUniqueId, CStateManager&)`.
- `InhabitantExited`: identical but `SetInFluid(mgr, false, uid)`, `kSM_XEXF`, and virtual
  **`0x88`** with the same two arguments.

So the missing piece is only a *type name* for that `(CActor&, CStateManager&)` vtable-`0x8c`
call's argument list, and the two 100%-matched hooks' real signatures (`UnkVtable84`/`88`
currently take nothing / a `TUniqueId`, retail passes two args to each). Guessing a rename of a
100%-matched camera function would move a unit another lane owns; not attempted here.

## Method

`.tmp/opencode/w6_score.py [fn]` rebuilds just this unit and prints its per-function scores;
`.tmp/opencode/w6_dis.sh <fn>` prints retail's and our disassembly of one function;
`.tmp/opencode/w6_diff.py <fn>` prints only the differing instruction pairs (that is what turned
"97.43%" into "the only difference is `mr r31,r30` vs a reload").
`.tmp/opencode/w6_tryfn.py <fn>` applies each edit in turn and prints its score, restoring the
file at the end. All scratch, outside `tools/`, not part of the diff.

NEW: progress-unit-cscriptwater-inhabitants | progress | MetroidPrime/ScriptObjects/CScriptWater |
  UpdateSplashInhabitants is at 97.43% with the whole body recovered; the only remaining
  difference is that retail keeps the list node's successor in a register across the body
  (`mr r31,r30` at the loop bottom) while our `rstl::list::node*` spelling reloads it, and the
  declaration order of that successor local is the one axis not yet tried.
