# progress-unit-cenergyprojectile

## Build fix round

The lane moved the whole `CEnergyProjectile::CCollisionCooldowns` block from the end of
`src/MetroidPrime/Weapons/CEnergyProjectile.cpp` to the top of the file (so the nested-class members
are emitted before the `CEnergyProjectile` methods) — but it pasted the block **twice** and dropped
only the tail copy. The result was two definitions each of `Contains(TUniqueId) const` and
`Add(TUniqueId, float)`, so mwcceppc aborted the unit with
`object 'CEnergyProjectile::CCollisionCooldowns::Add(TUniqueId, float)' redefined` and the whole
DOL failed to link (`main.dol` never produced, hence `decomp_build.sh` printed no `All:` line and
`gate.sh`'s `decl-order` step failed with no object to read).

Fix: deleted the second, duplicate copy only (the 20 lines that re-defined
`Add(TUniqueId, float)` and `Contains(TUniqueId) const`). Nothing else was touched: no declaration,
body, spelling or member of the matched functions changed, and the block's new position at the top
of the file — the part that produced the 9 extra matched functions — is kept as the lane left it.

Each of the five members is now defined once, in the order `__ct__`, `Contains`, `Update`,
`Add(TUniqueId, float)`, `Add(TUniqueId)`.

Verified after the fix: `./tools/decomp_build.sh` prints
`All: 33.31% fuzzy, 26.25% matched, 12.24% linked (11686 / 28465 functions)` with no linker error,
`sha1sum build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the unit still reports
`main/MetroidPrime/Weapons/CEnergyProjectile: 50.17% fuzzy, 21.18% matched (20 / 34 functions)`,
and `./tools/goal_check.sh build/goal/item.json` is `PASS` (gate.sh ok, counts 11677 -> 11686,
target rose 11 -> 20).
