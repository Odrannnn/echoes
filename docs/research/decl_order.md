# Units whose functions are declared in the wrong order

mwcceppc emits function definitions in **reverse source order** and mwldeppc places an input
object's `.text` in that object's own section order, so a unit's functions must be declared
**descending by retail offset**. A unit that gets this wrong is *permuted*: it compiles, it
links, every function scores 100% in objdiff (which pairs by name), `tools/unit_fit.sh` says it
fits (a permutation does not change any size), and the module's hash breaks on a handful of
bytes with no diagnostic anywhere. `tools/flip_test.sh` is the only check that sees it.

`AIMannedTurret` is the worked example: declared ascending, `fn_1_0`'s body landed at
`.text+0x10` instead of `0x0`, and three sessions read "3/3 at 100%" while the module's code was
in no link. Reordering the declarations - nothing else - made it flip.

**A permuted unit is not broken, but it cannot be flipped until it is reordered**, and the flip
is the only thing that makes its functions count. So this is a work list. `python3
tools/check_decl_order.py` measures it and checks this file: a newly permuted unit fails the
check, and so does an entry here that is no longer permuted, because that means somebody
reordered it.

Fixing one is mechanical - move the definitions - but it is only worth doing for a unit that is
otherwise ready to flip, or as part of the lane that is. **18 units, all `NonMatching`, none
`Matching`** - which is the point: a `Matching` unit cannot be permuted, because the hash would
already have broken. **The count here said 17 and was two behind**: `check_decl_order.py` now
prints `837 unit(s) checked, 18 permuted, all 18 accounted for`, which is 19 entries minus
`mainMid`, reordered 2026-09-26 (lane `midorder`) - see its removed entry below. Take the tool's
number, not this paragraph's.

## The list

One entry per unit, because the checker reads them one per line. Reordering is mechanical -
move the definitions - but it is only worth doing for a unit that is otherwise ready to flip,
or as part of the lane that is writing its remaining functions.

- `main/Kyoto/CPakFile` - the largest unmatched pool in the tree (22/33). Reorder in the lane
  that writes the remaining functions; the reorder alone buys nothing.
- `main/Kyoto/Audio/CStaticAudioPlayer` - **half reordered 2026-09-25**; now 23/24. The
  *source-defined* functions are now in retail order (the fix was to move the `MixToMono`
  definition to *after* `Decode`, with a forward declaration before it). What is still permuted
  is the trailing **pool of out-of-line template instantiations** - mwcceppc appends
  `__as__`/`reserve`/`clear`/`__dt__ vector`/`destroy`/`uninitialized_copy`/`erase` after the
  last source function, while retail interleaves them (`as, clear, destroy, dt_vector` sit
  between `StartMixOut` and `IsReady`). **That pool, not the two functions, is what now blocks
  the flip** - see "An emission-order wall: out-of-line template instantiations" in
  `docs/RUNNING_THE_DECOMP.md`.
- `main/MetroidPrime/ScriptObjects/CScriptStreamedMusic` - 21/23, two functions short.
- `main/Kyoto/Math/CTransform4f` - 27/33, mid-sized bodies.
- `SkyRipple/MetroidPrime/ScriptObjects/CScriptSkyRipple` - 7 of 15, kept `NonMatching` on
  purpose because promoting it would break the module. Reorder is cheap and unblocks a flip.
- `main/MetroidPrime/CStateManager` - 63/239 and known-hard; a lane on it must reorder first.
- `main/MetroidPrime/CActor` - known-hard, `CActor::CActor(...)` especially.
- `main/MetroidPrime/Player/CPlayerGun` - 61/135, the same shape as `CStateManager`.
- `main/MetroidPrime/TypesMatch` - 508/511, `NonMatching`; the three remaining functions are
  already characterised as hard, so the reorder is not the blocker.
- `main/Kyoto/Math/CMayaSpline` - not attempted; reorder when a lane takes it.
- `main/Kyoto/Graphics/CCubeMoviePlayer` - not attempted.
- `main/MetroidPrime/CEntity` - not attempted.
- `main/MetroidPrime/main` - 33 functions, mostly `CMain`'s; not a flip candidate.
- `main/MetroidPrime/CGameArea` - inherited from the second upstream sync (2026-09-28,
  `upstream/main` c3537e0); upstream's code, `NonMatching`. Reorder in the lane that takes it.
- `main/MetroidPrime/ScriptObjects/CScriptSpawnPoint` - inherited from the same sync;
  upstream's code, `NonMatching`. Reorder in the lane that takes it.
- **Reordered 2026-09-26 (lane `midorder`), and this is the removed entry** - `main/MetroidPrime/mainMid`. The
  entry that was here described it as *inherited* rather than introduced, which was right, and
  then predicted the fix as "two block moves with no out-of-line pool involved". **The first half
  was wrong and the second was right, and only measurement separates them.** The unit carried
  `main`'s permutation because it is a cut-and-paste out of `main.cpp`, but the *second* of the
  two inversions the old entry named - `CMain::SetFrameTimeMinimum` (0x80005C64) before
  `CMain::RsMain` (0x80005C6C) - **is in `main.cpp`, not here**: both addresses are inside
  `main`'s own claim (0x800053B8-0x80006B38), so `mainMid` had exactly one inversion,
  `CArchitectureQueue::Push` (0x80007A80) declared after `CGameArchitectureSupport::Update`
  (0x80007A14). The "15 of 21 functions are affected" this entry used to carry was wrong for a
  checkable reason: our object emits 20 `t`/`T` functions, **15** of which retail names in this
  range, and **2 of those 15 were out of place** - one adjacent transposition, `Push` and `Update`
  at positions 7 and 8 - against 0 of 15 after the move. The other five are not in this unit's
  claim at all (`CWorldState::Update` 0x8015B9B0, `CGameState::GetWorldState` 0x80142520, and
  `MakeMsg::CreateFrameEnd` 0x800489AC / `CreateFrameBegin` 0x80048A80 / `CreateTimerTick`
  0x80048DC8, the last three read out of `config/G2ME01/symbols.txt` and called from `Update` at
  0x80007A44), so the tool cannot compare them at all. **A two-position defect is not a
  two-function defect**: emission follows file order, so those 2 misplacements displaced all 12
  functions from position 9 up. **The
  out-of-line pool really is not a wall here, and that is the transferable finding**: after the
  move, the `rstl::list<CArchitectureMessage>` weak copies land in retail's exact slots -
  `push_back` 40 B, `do_insert_before` 112 B and `create_node` 136 B at 0x80007AA0, 0x80007AC8
  and 0x80007B38, the sizes and order of retail's own `fn_80007AA0`/`fn_80007AC8`/`fn_80007B38`,
  byte-identical apart from the two `bl` relocations - because mwcceppc emits each weak copy
  immediately after the source function that first needs it, and the source function they belong
  to is `Push`. The `mainTail` and `CStaticAudioPlayer` pools fail for the other reason: their
  instantiations are needed by *several* functions, so there is no single user to hang them on.
  **And the reorder bought nothing on its own, as predicted**: 9/21 functions and 54.52% fuzzy
  before and after, `matched` 3977 and `linked` 2555 unmoved, `flip_test` `FAIL`. What is left is
  named and measured in `docs/RUNNING_THE_DECOMP.md`'s Attempted modules table.

Inherited from upstream PrimeDecomp/echoes (f2dcbf4) with the merge that made it the base,
2026-09-28 - the order is upstream's, not something the merge changed; the counts are matched/total
from `build/report.json` at the merge:

- `main/Kyoto/Animation/CCharacterInfo` - 4/42, upstream's order.
- `main/Kyoto/Animation/CPASAnimState` - 14/16, upstream's order.
- `main/Kyoto/Animation/CPoseAsTransforms_Linear` - 10/16, upstream's order.
- `main/Kyoto/Audio/CSfxManager` - 60/159, upstream's order.
- `main/Kyoto/CSimplePool` - 10/21, upstream's order.
- `main/Kyoto/Graphics/CGX` - 53/54, upstream's order. 53/54; `CallDisplayList` is emitted after `GetFog` instead of before it.
- `main/Kyoto/Text/CFontRenderState` - 25/25, upstream's order. **Every function matches**, so the order is the whole blocker; reorder and `flip_test`.
- `main/MetaRender/CCubeRenderer` - 45/217, upstream's order.
- `main/MetroidPrime/CMapWorldInfo` - 20/23, upstream's order.
- `main/MetroidPrime/CMemoryCard` - 4/56, upstream's order.
- `main/MetroidPrime/Enemies/CStateMachine` - 2/30, upstream's order.
- `main/MetroidPrime/HUD/CSamusHud` - 13/88, upstream's order.
- `main/MetroidPrime/Player/CMorphBall` - 48/158, upstream's order.
- `main/MetroidPrime/Player/CPlayerGunBase` - 9/21, upstream's order.
- `main/MetroidPrime/Player/CPlayerState` - 66/72, upstream's order.

## What was checked, and what was not

The check compares the order our object emits (its symbols, in address order) against the order
retail has them (the addresses in `build/report.json`), for every function both know by name. It
covers every unit that has a compiled object - **334 of them** - so a unit with fewer than two
functions in common with retail is not examined, and neither is a function retail and we disagree
about the *name* of. Those are the two ways a permutation could hide from it, and neither is
silent in the same way: a name disagreement shows up as a lost pairing in the per-function diff.
