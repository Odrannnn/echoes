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
already have broken.

## The list

One entry per unit, because the checker reads them one per line. Reordering is mechanical -
move the definitions - but it is only worth doing for a unit that is otherwise ready to flip,
or as part of the lane that is writing its remaining functions.

- `main/Kyoto/CPakFile` - the largest unmatched pool in the tree (22/33). Reorder in the lane
  that writes the remaining functions; the reorder alone buys nothing.
- `main/Kyoto/Graphics/CGX` - 51/54, three functions from done. The best value per line moved.
- `main/Kyoto/Audio/CStaticAudioPlayer` - 22/24, and the two that remain are pure register
  allocation, so it is close.
- `main/MetroidPrime/ScriptObjects/CScriptStreamedMusic` - 21/23, two functions short.
- `main/Kyoto/Math/CTransform4f` - 27/33, mid-sized bodies.
- `ScriptCannonBall/MetroidPrime/ScriptObjects/CScriptCannonBall` - 12 of 26 written, unit
  `NonMatching`; blocked on 324 bytes of extra emitted functions as well. One file to reorder.
- `SkyRipple/MetroidPrime/ScriptObjects/CScriptSkyRipple` - 7 of 15, kept `NonMatching` on
  purpose because promoting it would break the module. Reorder is cheap and unblocks a flip.
- `main/MetroidPrime/CStateManager` - 63/239 and known-hard; a lane on it must reorder first.
- `main/MetroidPrime/CActor` - known-hard, `CActor::CActor(...)` especially.
- `main/MetroidPrime/Player/CPlayerGun` - 61/135, the same shape as `CStateManager`.
- `main/MetroidPrime/TypesMatch` - 508/511, `NonMatching`; the three remaining functions are
  already characterised as hard, so the reorder is not the blocker.
- `main/Kyoto/Input/CRumbleGenerator` - 0 of 8 matched; blocked on 452 bytes of extra emitted
  functions, so the reorder is not the blocker either.
- `main/Kyoto/Math/CMayaSpline` - not attempted; reorder when a lane takes it.
- `main/Kyoto/DolphinCDvdFile` - not attempted.
- `main/Kyoto/Graphics/CCubeMoviePlayer` - not attempted.
- `main/MetroidPrime/CEntity` - not attempted.
- `main/MetroidPrime/ScriptObjects/CScriptPickup` - not attempted.
- `main/MetroidPrime/main` - 35 functions, mostly `CMain`'s; not a flip candidate.

## What was checked, and what was not

The check compares the order our object emits (its symbols, in address order) against the order
retail has them (the addresses in `build/report.json`), for every function both know by name. It
covers every unit that has a compiled object - **334 of them** - so a unit with fewer than two
functions in common with retail is not examined, and neither is a function retail and we disagree
about the *name* of. Those are the two ways a permutation could hide from it, and neither is
silent in the same way: a name disagreement shows up as a lost pairing in the per-function diff.
