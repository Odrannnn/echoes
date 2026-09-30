# progress-prime1-cbodystateinfo — `MetroidPrime/BodyState/CBodyStateInfo`

## Verdict

`tools/goal_check.sh build/goal/item.json` → **PASS** (run in `wt-mp2-goal-L1`, `goal/lane-1`):

```
ok    no judge-owned path touched
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 10368 -> 10370   linked 5048 -> 5048
ok    check_symbol_names.py
ok    All:  31.50% fuzzy, 23.99% matched, 11.83% linked (10370 / 28465 functions)
ok    target rose: main/MetroidPrime/BodyState/CBodyStateInfo: 32 -> 34 / 52 functions
ok    no asm added
goal_check: PASS progress-prime1-cbodystateinfo
```

## Measured, unit `main/MetroidPrime/BodyState/CBodyStateInfo`

| | before | after |
|---|---|---|
| `matched_functions` | **32** / 52 | **34** / 52 |
| `matched_code` | 2232 / 6532 (34.17024%) | **4816** / 6532 (73.72933%) |
| `fuzzy_match_percent` | 65.3962 | **75.008575** |
| whole DOL `All:` | 31.49% fuzzy / 23.95% matched / 10368 functions | 31.50% / 23.99% / **10370** |

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged, unit stays `NonMatching`).

## What landed — two functions, +2

### 1. `SetupLocomotionStates__14CBodyStateInfoFR6CActor9EBodyType`: 99.99392% → 100%

Prime 1 was no use here: its `CBodyStateInfo` is built on `rstl::map<int, CBodyState*>` and
seven per-body-type `Setup*BodyStates` helpers, while Echoes uses a fixed 29-entry
`rstl::vector` plus `SetupBodyStates`/`SetupLocomotionStates`. Only two names are shared, and
this is not one of them.

The measured diff (`tools/bytescmp.py`, retail `0x800EF908`, 1316 bytes) was **one instruction**:
at body offset `+0x214` we emitted `li r3,76` where retail emits `li r3,1040`, and at `+0x344`
the reverse. Everything else matched once relocations were discounted. The jump table itself is
byte-identical (`jumptable_803B3CC0`, 9 words, verified against the retail DOL with
`tools/dol_read.py`), so the enum is right and the *case bodies* are laid out in the wrong order.

mwcceppc emits `switch` case bodies in **source** order; the jump table then indexes them by enum
value. Retail's source order for the three middle cases is `WallWalker, NewFlyer,
RestrictedFlyer`, not `RestrictedFlyer, WallWalker, NewFlyer`. Reordering the three `case` blocks
in `src/MetroidPrime/BodyState/CBodyStateInfo.cpp` puts each class in its retail slot and the
function matches byte-for-byte. (Checked with `tools/check_decl_order.py --unit
MetroidPrime/BodyState/CBodyStateInfo` — the unit's own function order is still retail-descending.)

### 2. `__dt__14CBodyStateInfoFv`: 50.48896% → 100%

Retail *inlines* all 24 embedded-state destructors into `~CBodyStateInfo` — the 0x3C-byte
"write vtable, write base vtable, write `CBodyState` vtable" block per member, walked backwards
from offset 484 down to 32, then the two `single_ptr` deleting-dtor virtual calls at offsets
28/24, then `__dt__Q24rstl48vector<...>Fv` with `li r4,-1`, then the `Free__7CMemoryFPCv`
deleting tail.

Our object instead emitted 17 out-of-line `bl __dt__<X>Fv` calls, because those seventeen classes
declared their destructor in the header as `~X() override;` and never defined it. A
declared-but-undefined destructor cannot be inlined. The fix is the pattern this repo already
uses for the sibling classes that *do* match at 100% (`CABSIdle`, `CABSAim`, `CABSFlinch`,
`CABSReaction`, `CBSJump`, `CBSHurled`, `CBSWallHang`, `CBSLocomotion`, `CBSTurn`): define it
inline, `~X() override {}`.

Changed, one line each, `~X() override;` → `~X() override {}`:

`CBSAttack.hpp`, `CBSCover.hpp`, `CBSDie.hpp`, `CBSFall.hpp`, `CBSGenerate.hpp`, `CBSGetup.hpp`,
`CBSGroundHit.hpp`, `CBSKnockBack.hpp`, `CBSLieOnGround.hpp`, `CBSLoopAttack.hpp`,
`CBSLoopReaction.hpp`, `CBSProjectileAttack.hpp`, `CBSScripted.hpp`, `CBSSlide.hpp`,
`CBSStep.hpp`, `CBSTaunt.hpp`, `CABSLoopReaction.hpp`.

These seventeen headers are included by nothing but `CBodyStateInfo.hpp`, which is included by
`CBodyStateInfo.cpp` and `CBodyController.hpp` only — the blast radius is the BodyState units and
`tools/report_diff.py` reports **no regression** anywhere in the tree.

Dropping the declaration entirely (leaving it implicit) behaves the same for the object and adds
an extra undefined `__vt__7CBSFall`; `~X() override {}` emits the destructor as a weak COMDAT
like the classes that already match, which is the convention already in these headers.

## Not reached

### `GetLocomotionSpeed__14CBodyStateInfoCFQ23pas15ELocomotionAnim` — 90.82609% before and after

Byte-identical to retail except for **one instruction pair**, in the prologue:

```
retail  800f0048 stwu r1,-16(r1) | 004c mflr r0 | 0050 lwz r5,12(r3) | 0054 stw r0,20(r1) | 0058 lwz r0,20(r5)
ours             stwu r1,-16(r1) | mflr r0 |      stw r0,20(r1) |      lwz r5,12(r3) |      lwz r0,20(r5)
```

i.e. retail hoists the `mStates.mItems` load one slot above the LR spill and we do not. Same
registers (`r5` = vector data pointer, `r0` = the loaded state), same frame, same 21 remaining
instructions. Sixteen source spellings tried across two `tools/try_batch.py` batches — all landed
on exactly 2 differing instructions:

`const CBSLocomotion*` / `const CBodyState*` / non-const pointer temporaries; static_cast at the
declaration vs at the call; reference-then-deref; single vs nested `if`; `&&` with the operands
swapped (13 differing — changes the branch shape); ternary; `nullptr !=` comparisons; `0.f` vs
`0.0f`; `mStates[pas::kAS_Locomotion]` vs a local `idx` vs `&mStates.mItems[0]`; `mBodyController`
hoisted into a local (7 differing). Nothing moves the spill. This is a scheduling decision, not a
source-shape one.

WALL: GetLocomotionSpeed__14CBodyStateInfoCFQ23pas15ELocomotionAnim 90.83% - mwcceppc hoists the `mStates.mItems` load above the LR spill in retail and not in any of 16 source spellings tried here; the rest of the function is already byte-identical.

### The 17 unnamed retail destructors (`fn_800F08BC`, `fn_800F0B90`…`fn_800F11D8`)

These are retail's `T` (dtk-local) copies of the same seventeen deleting destructers, and the
change above now makes us emit byte-identical code for them — e.g. our `__dt__7CBSFallFv` is the
same 92 bytes and the same 23 instructions as retail's `fn_800F11D8`, modulo the `__vt__` and
`Free__7CMemoryFPCv` relocations. They still score 0% in `build/report.json`, and this run's
+2 comes entirely from the two functions above.

The reason is pairing, not bytes: dtk names those retail symbols `fn_<address>` because they are
local in retail's object, so objdiff has no name in common with our `__dt__7CBSFallFv` and cannot
align them while the unit is `NonMatching`. Matching them would need our symbols to land on
retail's exact offsets (`fn_800F11D8` is at `+0x18D0` in the unit; ours is at `+0x1918`), which in
turn needs this unit's remaining functions to match first — i.e. it is the unit flipping, not a
spelling. Worth knowing before anyone spends a run on it.

## For the next run

- The unit is at 34/52. The 17 `fn_*` destructors and `GetLocomotionSpeed` are the whole
  remainder; `fn_800F08BC` (108 B, three vtable stores → `CAdditiveBodyState` level, so it is one
  of the `CABS*` classes) and `fn_800F0F84` (136 B, the odd-sized one) are the only two whose
  class is not determined from the 92-byte shape.
- `__dt__Q24rstl20single_ptr<7CBSTurn>Fv`, `__dt__Q24rstl27single_ptr<13CBSLocomotion>Fv`,
  `__dt__10CBodyStateFv`, `__dt__13CBodyStateCmdFv` and `__dt__11CBCSlideCmdFv` are weak symbols
  our object emits that retail's object does not define. Retail calls the two `single_ptr`
  destructors through the vtable at offsets 28/24 instead, so `RSTL_SINGLE_PTR_OUT_OF_LINE` is
  probably wrong for this TU — but fixing it is a separate change and was left alone here.