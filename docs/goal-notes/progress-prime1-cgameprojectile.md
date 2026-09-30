# progress-prime1-cgameprojectile

`kind: progress`, target `MetroidPrime/Weapons/CGameProjectile`, stays `NonMatching`.

## Result

`main/MetroidPrime/Weapons/CGameProjectile` **6 -> 11** matched functions of 49.
Global `build/report.json` matched **9786 -> 9791**, linked unchanged at 4895.
Five functions reached an exact match; no function anywhere got worse
(`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`
prints `+100%` for exactly these five and `no regression`).

| function | before | after | how |
| --- | --- | --- | --- |
| `StopProjectile` | 76.4% | 100% | Prime 1's source shape adapted: the `AddWeaponId` call the repo's TODO comment described was simply missing |
| `GetBeamAttribType` | 92.9% | 100% | case order only, no value change |
| `ApplyDamageToActors` | 58.0% | 100% | Prime 1's `const CVector3f forward = ...` hoisted out of the `if` |
| `GetProjectileBounds` | 87.8% | 100% | `GetTranslation()` bound **by value** instead of by const reference |
| `Touch` | 33.1% | 100% | Prime 1's dock test, `actor.GetUniqueId()` not `dock->GetUniqueId()` |

Only `src/MetroidPrime/Weapons/CGameProjectile.cpp` changed (5 insertions, 4 deletions,
plus two `#include`s). No header, no `configure.py`, no `build/goal/` file.

## Per-function detail, and what did **not** work

### StopProjectile 76.4 -> 100 (Prime 1 unchanged in shape, one line added)

The repo's body was
`DeleteProjectileLight; mActive = false; MaterialList() = CMaterialList(); mgr.UpdateActorInSortedLists(this)`.
Retail (`0x80035C48`, 30 insns) is that plus **`mgr.AddWeaponId(GetOwnerId(), GetType())`**
between the light delete and the `mActive` clear - the TODO comment at that line was
literally describing the missing call, and `CEnergyProjectile::StopProjectile` in this
same tree already spells it that way. Everything else, including the odd
`MaterialList() = CMaterialList()` (which compiles to the two `stw r5,108/104(r30)`)
and the `rlwimi` bitfield clear, already matched.

### GetBeamAttribType 92.9 -> 100 (Prime 1's four-case shape, different order)

No source change to the logic at all: the values were already right
(`kWT_Dark->1<<18`, `kWT_Light->1<<19`, `kWT_Annihilator->1<<20`, `kWT_Phazon->1<<6`).
Only the **order of the four `case` labels** was wrong. Retail emits the case bodies
in the order Phazon, Dark, Light, Annihilator; the repo had Dark, Light, Annihilator,
Phazon, which put `lis r3,64` in the wrong slot. Reordering the cases to match is the
whole fix. (`kPA_*` values are the ActorCommon ones already in the tree; nothing in
the enum was touched.)

### ApplyDamageToActors 58.0 -> 100 (Prime 1 verbatim)

Repo had `GetTransform().GetForward()` **inside** the `if`; retail loads the three
floats into the outgoing argument slots *before* the `mPendingDamagee` test. Prime 1's
spelling - `const CVector3f forward = GetTransform().GetForward();` on its own line
above the `if` - reproduces retail exactly. The virtual dispatch on
`ApplyDamageToOneActor` (vtable slot 34) and the `kInvalidUniqueId` reset already
matched.

### GetProjectileBounds 87.8 -> 100 (one-word change, and it is worth recording)

Six spellings tried; **only the binding of `GetTranslation()` decides it**:

| spelling | score |
| --- | --- |
| `const CVector3f translation = GetTranslation();` (by value) | **100.00%** |
| `const CVector3f& translation = GetTranslation();` (current tree) | 87.83% |
| by-value + `rstl::min_val`/`max_val` with `pos`/`prev` locals | 82.87% |
| ternaries `a < b ? a : b` instead of `rstl::*_val` | 87.87% |
| argument order of `min_val`/`max_val` swapped | 87.83% |
| six named `float` locals then `CAABox(minX,...)` | 77.57% |
| build `CVector3f lo`/`hi` then `CAABox(lo, hi)` | 69.21% / 30.19% |

The 12% gap was purely register allocation: the by-reference binding lets GCC keep the
translation's three floats live in caller-saved registers across the whole min/max
chain, retail keeps them in a stack slot. `rstl::min_val(a,b)` is `(b<a)?b:a`, which is
what retail's `fcmpo` + `bge` pair encodes, so the helper is right; only the binding
was wrong. A `const CVector3f&` local of the *same* object is not equivalent to a copy
here.

### Touch 33.1 -> 100 (Prime 1's dock test)

Repo's body was `CActor::Touch(actor, mgr);` plus a TODO. Retail (`0x80035AD8`,
24 insns) is:

```cpp
if (CScriptDock* dock = TCastToPtr<CScriptDock>(actor)) {
  if (dock->GetCurrentAreaId() == GetCurrentAreaId()) {
    mTouchedDock = actor.GetUniqueId();
  }
}
```

Two details the diff forced:
- the comparison is on **`CEntity::m_areaId`** (`lwz 4(r3)` vs `lwz 4(r30)`), i.e.
  `GetCurrentAreaId()`, not the dock's own `GetAreaId()`;
- the `lhz` that fills `mTouchedDock` reads **`8(r31)`** - `r31` is the `CActor&`
  parameter, already live in a callee-saved register - not `8(r3)`, the cast result.
  So it is `actor.GetUniqueId()`, not `dock->GetUniqueId()`. Same object, different
  register, and objdiff counts the difference: with `dock->GetUniqueId()` this lands
  at **99.38%**, with `actor.GetUniqueId()` at 100%. `mTouchedDock` is at `0x400`,
  matching `mWpscId` + `TUniqueId` in the existing header layout.

## Measured, not attempted

`check_symbol_names.py`: 503 units, 0 missing. `check_decl_order.py`: clean (the file
already declares descending by retail offset). `unit_fit.sh`: `.data` fits, `.text`
SHORT by 9624 - expected, the unit is 17% matched and holds many still-stubbed bodies.
`probe_sources.sh`: 745 files, 0 failed. `link_check.sh`: 250 undefined, unchanged from
the branch head, 0 duplicates.

`./tools/gate.sh build/goal/judge/report.base.json` -> **GATE PASS** (all 15 checks ok,
DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged, all 86 RELs match
`config/G2ME01/config.yml`).

## Still unmatched, and why not now

Nine bodies are still Prime-1-shaped guesses or TODO stubs and stay at their previous
percentages: `CanCollideWithTrigger` 9.5%, `CanCollideWithGameObject` 6.8%,
`CanCollideWithComplexCollision` 3.7%, `CanCollideWithDoor` 13.1%, `CanCollideWith`
12.4%, `RayCollisionCheckWithWorld` 2.3%, `AcceptScriptMsg` 1.6%, `FluidFXThink` 3.4%,
`ApplyDamageToOneActor` 1.1%, `DoCollisionCheck` 8.4%, `UpdateProjectileMovement`
25.3%, `Chase` 0.2%, `CreateProjectileLight` 1.5%, `ResolveCollisionWithActor` 0.3%.

Spellings tried and their scores are in the table above for `GetProjectileBounds`; the
rest were not attempted this run, so no `WALL:` line is claimed for them.

Two blockers found while reading the disassembly, both about symbols the tree does not
have yet:

- **`CanCollideWith`** (`0x80034DB0`) dispatches on `CUnknown50` (id 50, which *is*
  declared in `src/MetroidPrime/TypesMatch.cpp:782` as `CAST_TO_IMPL(CUnknown50, 50)`
  and aliased to `CScriptDamageableTrigger` at line 348) for the door branch, and on
  `CPhysicsActor` + `CCollisionActor` plus a `0x5447` patterned-type test for the
  complex-collision branch. The `0x5447` test is `TypesMatch(21575)` reached through
  two vtable hops (`0x7c` then `0x14`) - Prime 1 spells it
  `PATTERNED_CAST_TO(CPuddleToadGamma, &act)`, but **this tree has no
  `CPuddleToadGamma` and no `PATTERNED_CAST_TO` macro at all** (Prime 1's
  `CPatterned.hpp:505-507` defines it; ours does not). `CGameProjectile.cpp` is not the
  place to add that machinery.
- **`CanCollideWithTrigger`** (`0x800358E8`) reads the weapon description through a
  helper at `0x80033B6C` that copies a `TLockedToken<CWeaponDescription>` out of
  `mProjectile` at `0x238` and locks it; the two flag bytes it then tests are `0x65`
  and `0x66` of `CWeaponDescription` (`mEWTR`/`mLWTR`). The tree's
  `CProjectileWeapon::GetWeaponDescription()` returns `TLockedToken` by value from a
  different offset, so the helper's shape has to be reconstructed before the flags mean
  anything. That helper is itself a 76-byte function in this same unit that is
  currently `fn_80033B6C` at 0%, so it is a unit-local carve, not a header change.

`NEW: progress-cgp-cancollidewith | progress | MetroidPrime/Weapons/CGameProjectile | CanCollideWith needs CPuddleToadGamma + PATTERNED_CAST_TO, absent from this tree; the 0x5447 test is unreachable until CPatterned.hpp grows Prime 1's cast macro`

`NEW: progress-cgp-ttoken-helper | progress | MetroidPrime/Weapons/CGameProjectile | fn_80033B6C (76 B, 0%) is the TLockedToken copy+Lock helper CanCollideWithTrigger calls; it must be carved as a unit-local function before that body can be spelled`
