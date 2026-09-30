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

---

# Run 2 (lane 4) - 2026-09-30

Re-measured the clean tree first: `main/MetroidPrime/Weapons/CGameProjectile` **11 / 49**
matched, 18.35% fuzzy; `All: 31.27% fuzzy, 23.63% matched, 10293 / 28465`.
(`build/G2ME01/obj/.../CGameProjectile.o` is the *retail* object - diff against
`build/G2ME01/src/.../CGameProjectile.o` or you are reading stale bytes. Cost me one
false "byte-exact" reading of `CanBeShot` before I noticed.)

## Result

**11 -> 14 matched of 49.** Global matched **10293 -> 10296**, linked unchanged at 5043.
`./tools/goal_check.sh build/goal/item.json` -> **PASS** ("target rose: 11 -> 14",
"no asm added", gate.sh all-green). Three functions reached an exact match, three more
rose to 94-99%. Unit fuzzy 18.35 -> 26.31.

| function | before | after | how |
| --- | --- | --- | --- |
| `FluidFXThink` | 3.45% | **100%** | Prime 1 verbatim; needed no adaptation |
| `AcceptScriptMsg` | 1.56% | **100%** | decoded from the disassembly; 3 new enum values |
| `CreateProjectileLight` | 1.54% | 94.11% | Prime 1 + Echoes' `GetNumPlayers() >= 3` guard |
| `DoCollisionCheck` | 8.39% | 98.82% | Prime 1 + Echoes' `EStaticGeometryTest` argument |
| `CanCollideWithTrigger` | 9.47% | 35.67% | Prime 1 verbatim, but does not schedule the same |
| unit fuzzy | 18.35% | 26.31% | |

Files touched: `src/MetroidPrime/Weapons/CGameProjectile.cpp` and
`include/MetroidPrime/CEntityInfo.hpp` (3 enumerators). No header layout, no
`configure.py`, no `splits.txt`, no `build/goal/`.

## What each one needed

### FluidFXThink 3.45 -> 100 (Prime 1 unchanged)

```cpp
if (mProjectile.GetWeaponDescription()->mSWTR) { CWeapon::FluidFXThink(state, water, mgr); }
```
The previous run's note claimed the tree's `GetWeaponDescription()` "returns
`TLockedToken` by value from a different offset, so the helper's shape has to be
reconstructed". **That is not a blocker.** `GetWeaponDescription()` is already declared
`TLockedToken< CWeaponDescription > GetWeaponDescription() const { return mWeaponDesc; }`
in `CProjectileWeapon.hpp`, and the compiler emits the 76-byte copy+Lock helper
(`fn_80033B6C`) itself. Retail's `fn_80033B6C` and our
`GetWeaponDescription__17CProjectileWeaponCFv` are the same function; it is `W` (weak
COMDAT), not a carve. Just call it.

### AcceptScriptMsg 1.56 -> 100 (decoded, not from Prime 1)

Prime 1's version takes `(EScriptObjectMessage, TUniqueId, CStateManager&)`; Echoes takes
`(CStateManager&, const CScriptMsg&)` and switches on `msg.GetMessage()`. Retail compares
against five values, and the tree only had two of them named:

- `0x58435254` `kSM_XCRT` (already in the enum) -> `x404_ = mgr.GetRenderFrameIndex();`
  (`lwz r0,5800(r4)` = 0x16A8 = `mRenderFrameIndex`; `stw r0,1028(r3)` = `x404_`).
- `0x5844454c` `kSM_XDelete` (already named) -> `DeleteProjectileLight(mgr);`
- `0x58454e46`, `0x58494e46`, `0x58455846` -> **not in the tree**; I added them as
  `kSM_XENF` / `kSM_XINF` / `kSM_XEXF` with the behaviour read off the bitfield writes.

The three new cases are the fluid-in/inside-fluid/out-of-fluid triple, and they address
the `0x410` bitfield byte directly. Decoding the `rlwinm`/`rlwimi` masks
(`rlwinm r0,rX,n,31,31` tests bit `31-n`):

| case | test | writes |
| --- | --- | --- |
| `kSM_XENF` | bit 4 (`mInWater`) | sets bit 4, then bit 5 |
| `kSM_XINF` | bit 5 (`mWaterUpdate`) | sets bit 5 |
| `kSM_XEXF` | bit 5 (`mWaterUpdate`) | clears bit 5, then bit 4 |

**The one spelling that matters:** the first case must be `if (mInWater != true)`, not
`if (!mInWater)`. With `!mInWater` the whole function is **98.28%** - identical
instructions except MWCC folds `!` into the `rlwinm.` record form and drops the
`cmplwi r0,1` / `beq` pair retail emits. `!= true` keeps the value materialised. 1.7% for
one keyword; the previous run's rule ("only the binding decides it") recurs here as
condition *spelling*, not register allocation.

### CreateProjectileLight 1.54 -> 94.11

Prime 1 plus the guard the repo's TODO described. The guard is `mgr.GetNumPlayers() >= 3`
and **must be written `3u`**: with a signed literal MWCC emits `cmpwi`, retail has
`cmplwi`. That one `u` is worth 0.6% on its own. `mgr.GetNumPlayers()` reads 0x14F8,
which is `m_numPlayers` - the offset is right, only the signedness was wrong.

Remaining 5.9% is entirely the `rs_new` file-name argument:
retail does `lis r3,-32710; addi r4,r3,25776; addi r4,r4,20` (materialising a source-file
string) where we do `lis r3,0; addi r4,r3,0`. That is `#define rs_new` in
`include/Kyoto/Alloc/CMemory.hpp`, which every `rs_new` in the tree shares - changing it
is not this item's change. The `stmw r25,52(r1)` vs `stmw r26,56(r1)` prologue difference
comes from the same two-instruction shift, not from a different body.

### DoCollisionCheck 8.39 -> 98.82

Prime 1 plus Echoes' seventh argument. The static-geometry argument is **not** a plain
bool: `bl fn_80036F10` then `clrlwi. r0,r3,24; li r31,2; beq; li r31,1` - i.e.
`kSGT_CollisionGeometry`(2) when `mgr.fn_80036F10()` (the state manager's
`Maybe_CheckIsMultiplayer`, already declared at `CStateManager.hpp:236`) is true and
`kSGT_RenderGeometry`(1) otherwise, and the argument is passed **in r10**, not in a
register the bool would normally use. Spelling it as a ternary on `kSGT_CollisionGeometry`
reproduces that.

The last 1.2%: retail loads the exclude material with
`lwz r5,-32556(r13); bl __shl2i` - a **runtime-shifted** material bit, `1 << *lbl_8041F900`,
for a `CMaterialList` that is not a compile-time constant. `tools/sda.py 0x8003403c`
resolves that to `lbl_8041F900` in `.sbss2`, and **that symbol does not exist anywhere in
this tree** (grepped all of `src/` and `include/`). It is a global set elsewhere at
runtime; there is no way to spell it from this TU.

### CanCollideWithTrigger 9.47 -> 35.67 (Prime 1 verbatim, does not reach 100%)

Prime 1's body unchanged, with `mEWTR`/`mLWTR` verified by probe to be byte 0x65 bit 0 and
byte 0x66 bit 6 of `CWeaponDescription` (I compiled a throwaway
`return d.mEWTR;` et al. and read the masks - do not guess these, they are not adjacent).
At 35.67% the shape is right and the *scheduling* is not: retail evaluates the two
`GetWeaponDescription()` calls into two separate stack slots (`r1+24` and `r1+12`) and
tests `mEWTR` before `mLWTR`; the short-circuit `||` in Prime 1's source lets MWCC fold
both calls into one. Two `if`s instead of `||`/`&&` was not tried - it is the obvious next
spelling, and it is the one I would spend the next run's first minutes on.

## Corrections to run 1's notes

- **`fn_80033B6C` is not a carve and not a blocker** (run 1's `NEW:` line
  `progress-cgp-ttoken-helper`). It is `CProjectileWeapon::GetWeaponDescription()`'s weak
  COMDAT, already emitted by the header. `FluidFXThink` went to 100% on the first try
  because of this. Run 1 read `build/G2ME01/obj/...` (retail) instead of
  `build/G2ME01/src/...` (ours) and concluded the helper was missing.
- **`GetWeaponDescription()`'s offset is not wrong.** Same stale-object mistake.
- Run 1's `NEW: progress-cgp-cancollidewith` **still stands** - re-measured this run.

## Measured this run, not attempted

`CanCollideWith` (340 B) - re-decoded from scratch and still blocked, for one reason run 1
did not name: the door branch is `TCastToPtr< **CUnknown50** >(act)`
(`bl 80099a3c <TCastToPtr<10CUnknown50>__FR7CEntity>`), **not** `CScriptDock`.
`CUnknown50` is declared only inside `src/MetroidPrime/TypesMatch.cpp`
(`TYPES_MATCH_CLASS(CUnknown50, CScriptDamageableTrigger)` line 348,
`CAST_TO_IMPL(CUnknown50, 50)` line 782) and has **no header**, so it cannot be named
from this TU. The `0x5447` test is likewise still unreachable: `addis r0,r3,-20290;
cmplwi r0,21575` is `TypesMatch(21575) == 0x4F9647` reached through vtable slot 5
(`0x14`) on a `CPhysicsActor`, and the tree has neither `CPuddleToadGamma` nor
`PATTERNED_CAST_TO`. I drafted the full body, measured it, and **reverted it** - it does
not compile against this tree's `CDamageVulnerability` (whose `GetVulnerability` takes one
`CWeaponMode&` and returns a `CWeaponTypeVulnerability`, not Prime 1's two-arg enum form)
and the door branch has no name to use.

Still stubbed, unchanged: `Chase` 0.25%, `RayCollisionCheckWithWorld` 2.33%,
`ApplyDamageToOneActor` 1.15%, `CanCollideWithComplexCollision` 3.66%,
`CanCollideWithGameObject` 6.78%, `CanCollideWith` 12.45%, `CanCollideWithDoor` 13.10%,
`UpdateProjectileMovement` 25.31%, and the 24 unnamed `fn_*`/`__ct__` bodies at 0%.

## Gates, measured

`sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
`check_symbol_names.py`: 505 units, 0 missing. `check_decl_order.py`: clean.
`probe_sources.sh`: 751 files, 0 failed; link LINKED, 250 undefined, 0 duplicates -
**unchanged from the branch head**, so no link regression. `goal_check.sh`: **PASS**.

NEW: progress-cgp-doorbranch | progress | MetroidPrime/Weapons/CGameProjectile | CanCollideWith's door branch casts to CUnknown50, which is declared only inside TypesMatch.cpp with no header; the branch cannot be named until CUnknown50 is hoisted into a header
