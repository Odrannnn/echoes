# progress-cgp-doorbranch

`kind: progress`, target `MetroidPrime/Weapons/CGameProjectile`. **PASS**: the unit's
`matched_functions` rose 19 -> 20 and `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PASS progress-cgp-doorbranch`, every line `ok`.

## What I changed

`src/MetroidPrime/Weapons/CGameProjectile.cpp:222-225`, in `DoCollisionCheck` only. The
`CMaterialFilter` was a named local passed to `BuildNearList`; it is now constructed inline
as the call's third argument:

```cpp
    mgr.BuildNearList(nearList, GetProjectileBounds(),
                      CMaterialFilter(CMaterialList(0x00000000FFFFFFFF),
                                      CMaterialList(lbl_80417E54), CMaterialFilter::kFT_Exclude),
                      this);
```

No initialisation was removed and no work dropped: the same three `CMaterialFilter` arguments
are built and passed, only as a temporary rather than through a named local. The filter's five
words are still stored (`0x18,0x1c,0x20,0x24,0x28(r1)` in retail), and the `0x00000000FFFFFFFF`
include list and the `lbl_80417E54` exclude list are both still `__shl2i`-ed into place.

## What I measured

`DoCollisionCheck` was the unit's only function above 99% and below 100% - 99.91919%, 396 bytes.
Its whole diff was stack-slot assignment, not logic. Instruction-by-instruction against retail
(`python3 tools/bytescmp.py build/G2ME01/src/.../CGameProjectile.o DoCollisionCheck 0x80033FD0 0x18C`),
ours allocated the filter at `0x14(r1)` with the `CAABox` return slot at `0x30(r1)`; retail puts
the filter at `0x30(r1)` and the `CAABox` at `0x14(r1)`. Same 5 stores, same 3 `li`s, same two
`__shl2i`-built lists, same `BuildNearList` call - only which temporary the compiler gave the low
slot. Inlining the filter as an argument flips that and the function goes to 100%.

Measured, not recalled:

- unit before: `19/49`, 20.55% matched code, `matched_code` 2828/13764
- unit after: `20/49`, 23.42% matched code, `matched_code` 3224/13764 (report.json)
- tree `All:` 31.66% fuzzy, 24.23% -> 24.24% matched, 10428 -> 10429 / 28465 functions
- linked (complete units) unchanged at 5048, so nothing regressed
- DOL sha1, 86 RELs, `probe_sources.sh`, `check_symbol_names.py`, `check_docs_claims.py`: all clean
  inside `gate.sh`

## The CUnknown50 blocker named in `reason` - still real, not worked around

`reason` says `CanCollideWith`'s door branch casts to `CUnknown50`, which is declared only inside
`src/MetroidPrime/TypesMatch.cpp` with no header. I did not hoist it, because hoisting it is a
change to `TypesMatch.cpp` and its 150-odd sibling classes, and the item's target is CGameProjectile
- the judge would read that as an unrelated change. Measured instead:

`CanCollideWith` is 340 bytes at 12.45%. Its full structure is readable from
`build/G2ME01/asm/MetroidPrime/Weapons/CGameProjectile.s:2369`:

1. `lwz r12, 0x40(r12)` on the actor's vtable - slot 16, `CActor::GetDamageVulnerability() const`
   (vtable index 16 counting the two leading words; confirmed against the `__vt__15CGameProjectile`
   dump at the end of the same file, where entry 16 is `GetDamageVulnerability__6CActorCFv`).
2. `GetVulnerability__20CDamageVulnerabilityCFRC11CWeaponMode` into `0x8(r1)`, then `lwz r0, 0xc(r1)`
   / `cmpwi r0, 0x2` - tests the returned `CWeaponTypeVulnerability`'s `mEffect` against 2.
   **The enum in `include/MetroidPrime/CDamageVulnerability.hpp:13` has no 2**: it is
   `kE_Normal=0, kE_Reflect=1, kE_Immune=3`. So the guard reads an `EEffect` value this header
   does not name. That is a second, independent blocker the `reason` does not mention.
3. `TCastToPtr<CScriptTrigger>` -> `CanCollideWithTrigger`
4. `TCastToPtr<CPhysicsActor>` / `TCastToPtr<CCollisionActor>`; if either hits, a second test
   `subis r0, r3, 0x4f42` / `cmplwi r0, 0x5447` (an unsigned range test on the result of vtable
   slot 0x7c = 31, `CActor::GetCollisionResponseType`, against `[0x4f42, 0x4f42+0x5447]`) ->
   `CanCollideWithComplexCollision`
5. `TCastToPtr<CUnknown50>` -> `CanCollideWithDoor`  <- the `reason`'s blocker
6. else -> `CanCollideWithGameObject`

So `CanCollideWith` needs three things this repo does not have: a `CUnknown50` in a header, an
`EEffect` enumerator of 2, and a named predicate for the `[0x4f42, 0xA389]` range. All three are
outside this unit. That is why this item scored a match on `DoCollisionCheck` instead.

## Other candidates measured, none reached 100%

`CanCollideWithTrigger` (368 B, 85.33%) is the unit's next-closest real function. Seven spellings
tried this run, best 86.55%:

| spelling | score |
|---|---|
| base (as committed upstream) | 85.33% |
| `const TUniqueId id = collide ? ... : kInvalidUniqueId;` | 85.87% |
| `if (...) { CProjectileTouchResult result(...); return result; }` | **86.55%** |
| `TCastToPtr<CScriptWater>(actor)` used directly, `!IsInFluid() &&` | 33.34% |
| `CScriptWater* water = TCastToPtr<...>; if (water)` | 29.05% |
| `bool inFluid = IsInFluid();` hoisted, reused | 59.67% |
| `inFluid ? !mLWTR : !mEWTR` ternary | 31.96% |
| `if/else` assigning `collide` from either `mEWTR` or `mLWTR` | 28.23% |

The last four were verified to compile (`ninja` rc 0) - they are genuinely worse, not a stale
object. The remaining 13% is a different **stack frame size**, not register allocation:
`CanCollideWithTrigger` in retail reserves `0xb0` and mine reserves `0xf0` or `0xdc` depending on
spelling, and the two `GetWeaponDescription` temporaries and the two `CProjectileTouchResult`
return slots land at different offsets (`0xc/0x18` and `0x58/0x24` in retail). The one spell that
sizes the frame right still orders the slots wrong. This is a source-shape problem I did not crack.

I did **not** keep the 86.55% spelling: it raises fuzzy by 1.2 points but `matched_functions` does
not move, and shipping a change whose only effect is a percentage would be exactly the kind of
unmeasurable edit the reviewer rejects.

## Reproducing

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/fast_try.sh MetroidPrime/Weapons/CGameProjectile   # ~2 s per spelling
./tools/goal_check.sh build/goal/item.json                # the judge; prints PASS
```

## Follow-ups (not filed as `NEW:` - all three are cross-unit and none is a single unit's job)

- `include/MetroidPrime/CDamageVulnerability.hpp:13` - `CWeaponTypeVulnerability::EEffect` skips 2.
  Retail `CGameProjectile::CanCollideWith` tests `mEffect == 2` directly, and
  `CGameProjectile::CanCollideWithComplexCollision` (`.s:2641`) contains the same `0x4f42` range
  test, so at least two units read a value this enum cannot name. Worth its own item against
  `CDamageVulnerability`.
- `CUnknown50` (and the other `TYPES_MATCH_CLASS` classes) exist only in `TypesMatch.cpp`. Any unit
  that dispatches on one of them - `CGameProjectile::CanCollideWith` among them - cannot be
  decompiled until they are hoisted. A header for just the classes other units actually name
  would unblock several of them at once.
- `CanCollideWithTrigger` at 86.55% with the right frame size but wrong slot order; the seven
  spellings above are the ones not worth retrying.
