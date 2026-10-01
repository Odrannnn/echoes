# progress-unit-ctweakplayergun

Unit `MetroidPrime/Tweaks/CTweakPlayerGun`, DOL unit, stays `NonMatching` (3 of 33 functions left).
`build/goal/item.json` said 20/33; re-measured on the clean tree at **20/33** (the reason's
per-function list was accurate, and it omitted the three 75.29% `beam_Misc` getters it did not
list: `GetBlackHoleDamage`, `GetSunBurstRaysDamage`, `GetImploderDamage` - they were also at
75.29%, so there were 13 functions left, not 10).

**Result: 20 -> 30 / 33 matched functions.** Unit fuzzy 88.40% -> 98.90%, matched code
30.43% -> 90.39%. Global matched 11712 -> 11722; linked unchanged at 5727 (the unit does not flip).
`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

## What I changed (one file)

`src/MetroidPrime/Tweaks/CTweakPlayerGun.cpp` only. Added one file-local helper:

```cpp
static inline CDamageInfo LdrDamage(const SLdrTDamageInfo& data, bool charged = false,
                                   bool comboed = false, bool noImmunity = false,
                                   bool flag = false) {
  return CDamageInfo(data, charged, comboed, noImmunity, flag);
}
```

and routed every `CDamageInfo(...)` / `SWeaponInfo(...)` construction through it.

| function | before | after |
|---|---|---|
| `GetDarkBeamBlobDamage() const` | 75.29 | **100.00** |
| `GetMissileDamage() const` | 75.29 | **100.00** |
| `GetBombInfo() const` | 75.29 | **100.00** |
| `GetPowerBombInfo() const` | 75.29 | **100.00** |
| `GetBlackHoleDamage() const` | 75.29 | **100.00** |
| `GetSunBurstRaysDamage() const` | 75.29 | **100.00** |
| `GetImploderDamage() const` | 75.29 | **100.00** |
| `GetComboDamage(CPlayerState::EBeamId) const` | 89.88 | **100.00** |
| `GetPhazonBeamInfo() const` | 83.54 | **100.00** |
| `BuildCache()` | 85.82 | **100.00** |

The Prime 1 counterpart at `prime-ref/src/MetroidPrime/Tweaks/CTweakPlayerGun.cpp` turned out to
be no help: Echoes' engine forked, and the Prime 1 file reads its damage values off a
`CInputStream` in the constructor rather than out of an `SLdrTweakPlayerGun`. The headers and
member layout were left alone.

## The codegen rule (this is the whole result, and it generalises)

**Retail keeps the hidden return pointer live across every CDamageInfo constructor call in this
unit: `stw r31,12(r1)` / `mr r31,r3` before the `bl`, `lwz r31,12(r1)` after it. Writing
`return CDamageInfo(mData->...)` at the call site makes mwcceppc drop all three, leaving the
function 16 bytes short at 75.29%.** Going through an inlined helper that *returns* a
`CDamageInfo` is what puts the pointer back in r31.

Confirmed the mechanism is specifically "an inlined callee that returns a class by value":
`CTweakPlayer::GetDarkWorldDamageInfo()` (already 100%) calls `LdrToDamageInfo(const SLdrDamageInfo&)`,
a non-inline function returning `CDamageInfo`, and it emits the same `mr r31,r3` / spill / reload.
The matched `__ct__17CCameraShakerData...` functions in the same file do **not** preserve it -
constructing a `CCameraShakerData` directly leaves r31 unused. So it is not "class returned by
value", it is "a call that returns a class by value, or an inlined one".

Two knock-on effects worth recording, both from the same change:

- **Temporaries stop being tracked in registers.** `GetPhazonBeamInfo` and `BuildCache` build
  `CDamageInfo` temporaries for `SWeaponInfo`'s `const CDamageInfo&` parameters. Without the
  helper MWCC keeps each temporary's address in a callee-saved register (`mr r30,r3`, and then
  `mr r4,r3` / `mr r5,r30` for the second), burning a third callee-saved register (r29 in
  `GetPhazonBeamInfo`, r30 in `BuildCache`) and re-loading `this->mData` from the wrong register.
  Retail instead recomputes the fixed stack slot (`addi r4,r1,36`, `addi r5,r1,8`) and uses only
  r30/r31. Same root cause, 3 and 17 instructions respectively.

**`mwcc` will not inline an 8-argument constructor call.** `static inline CCameraShakerData
LdrShaker(const SLdrCameraShakerData&, int)` with the same 8-argument `CCameraShakerData` ctor was
*not* inlined: it emitted a real `bl LdrShaker__FRC20SLdrCameraShakerDatai` and the three
functions fell 88.56% -> 58.56% (and added an undefined symbol). `LdrDamage`'s 5-argument body
inlines. Do not reach for this helper shape on a wide constructor.

## Not done: the three CCameraShakerData getters (all 88.56%, 2 instructions)

`GetRecoilCameraShakerData`, `GetProjectileRecoilCameraShakerData`,
`GetProjectileImpactCameraShakerData`. Both sides are 72 bytes / 18 instructions with identical
relocations (`sZeroVector__9CVector3f` for the position argument - retail passes the address of
the real global, not an inlined zero, so `CVector3f::Zero()` is right). The whole remaining diff
is that retail loads the flags argument **before** the two float arguments and we load it after:

```
retail:  addi r7,r8,700 ; lwz r4,624(r8) ; lfs f1,628(r8) ; lfs f2,836(r8) ; lwz r9,840(r8)
ours:    addi r7,r8,700 ; lfs f1,628(r8) ; lfs f2,836(r8) ; lwz r4,624(r8) ; lwz r9,840(r8)
```

A pure two-instruction scheduling swap; every operand, immediate and relocation agrees.

Nine spellings tried, all **88.56%**, all producing the identical instruction sequence:

1. the tree as it stood (local `const SLdrCameraShakerData& shaker`)
2. direct `mData->recoil.<field>` for every argument, no local
3. `const uint flags = ...;` hoisted into its own local, passed as `flags`
4. all three scalars hoisted (`attenuationDistance`, `duration`, `flags`), declared in that order
5. scalars hoisted and declared flags-first
6. `static_cast<int>(shaker.flagsCameraShaker)`
7. `const SLdrCameraShakerData* shaker = &mData->recoil;` with `->`
8. `const CVector3f& position = CVector3f::Zero();` hoisted
9. a `static inline uint LdrFlags(const SLdrCameraShakerData&)` accessor for the flags argument

Also measured and rejected: the `LdrShaker` inline helper (above, 58.56%). The
`LdrDamage` trick does **not** transfer here - there is no `CDamageInfo` in these functions.

`python3 tools/check_decl_order.py --unit MetroidPrime/Tweaks/CTweakPlayerGun` -> ok;
`./tools/unit_fit.sh MetroidPrime/Tweaks/CTweakPlayerGun.cpp` -> `.text` claimed/ours/retail
2248/2248/2248 `fits`, no extra functions; `python3 tools/check_symbol_names.py` -> 0 missing.

WALL: CTweakPlayerGun::GetRecoilCameraShakerData 88.56% - retail schedules the flags load ahead of
the two float loads; nine source spellings all emit the same order, and mwcc will not inline the
8-argument CCameraShakerData ctor call to reshape the DAG.