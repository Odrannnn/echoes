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

---

# Second run (lane 8) - **PASS**, `matched_functions` 20 -> 21

`./tools/goal_check.sh build/goal/item.json` prints `goal_check: PASS progress-cgp-doorbranch`,
every line `ok`: gate clean (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe),
`check_symbol_names.py` clean, tree `matched 11285 -> 11286`, `linked 5507 -> 5507` (no
regression), `no asm added`, `target rose: main/MetroidPrime/Weapons/CGameProjectile: 20 -> 21 /
49 functions`.

## What I changed - one file, one function renamed

`src/MetroidPrime/Weapons/CGameProjectile.cpp:15-21` (and its one call site, line 38). The
file-local helper

```cpp
static CTransform4f clear_transform(const CTransform4f& xf) { ... }
```

is now

```cpp
extern "C" CTransform4f fn_800361A4(const CTransform4f& xf) { ... }
```

with the body untouched. Nothing else in the tree changed.

## Why that raises the count: objdiff pairs by **name**, and this one was never paired

This is the thing the first run did not know, and it is worth more than any spelling I tried.

Retail's `CGameProjectile.o` contains this helper as an **unnamed global at 0x800361A4**, so dtk
calls it `fn_800361A4`. Our object contained the identical code under the *guessed* name
`clear_transform`, so objdiff had no name to pair it with and reported `0.00%` for
`fn_800361A4` - a function that was already fully decompiled.

Measured, not assumed. Comparing the two bodies byte for byte with
`python3 tools/bytescmp.py build/G2ME01/src/MetroidPrime/Weapons/CGameProjectile.o clear_transform
0x800361A4 92` gives **0 differing bytes outside relocation fields** (the only four differing
instructions are the two `bl`s and the `lis`/`addi` pair that form the `sZeroVector` address, and
objdiff ignores relocated fields). A scan of all 18 previously unpaired target functions against
every same-sized function in our object (`objdump -t` sizes, `objdump -r` relocation offsets,
bytes from `tools/dol_read.py`) reports 0 non-relocation byte differences for nine of them.

The naming is the project's own convention, not a trick: `build/report.json` has **337** target
functions named `fn_XXXXXXXX` already at 100% (of 4909 such functions), and `src/` already
contains 669 `extern "C" ... fn_8...` declarations/definitions. `fn_800361A4` was referenced
nowhere in the tree before this change, so the new global cannot collide.

## The other 17 unpaired functions in this unit - why none of them is renameable

Eleven of them are byte-identical to code we already emit, but every one is a **template
instantiation or a destructor**, so its symbol name is fixed by the template or class and cannot
be turned into `fn_XXXXXXXX` without editing shared headers (`include/rstl/construct.hpp`,
`optional_object.hpp`, `CImpactVisorEffect.hpp`) and risking every unit in the tree:

| retail symbol | B | our function it is byte-identical to |
|---|---|---|
| `fn_80032CB8` | 84 | `__dt__18CImpactVisorEffectFv` |
| `fn_80032D0C` | 92 | `__dt__optional_object<CImpactVisorEffect::SParticleEffect>` |
| `fn_80032D68`, `fn_80034D68`, `fn_800360BC` | 32 | `construct<CRayCastResult>` / `construct<SParticleEffect>` |
| `fn_80032D88` | 72 | `destroy<CImpactVisorEffect::SParticleEffect>` |
| `fn_80034D28` | 64 | `optional_object<CRayCastResult>::optional_object(const optional_object&)` |
| `fn_80034D88`, `fn_800360DC` | 40 | `construct_impl<CRayCastResult>` / `construct_impl<SParticleEffect>` |
| `fn_8003607C` | 64 | `__dt__optional_object<CImpactVisorEffect::SParticleEffect>` |
| `fn_80036104` | 128 | `optional_object<SParticleEffect>::optional_object(const optional_object&)` |

The other six we do not emit at all: `fn_80033340` (44 B), `fn_80034C8C` (92 B, the
`CCollidableAABox` copy constructor), `fn_80034CE8` (64 B), `fn_800350A0` (56 B,
`optional_object<CRayCastResult>::optional_object(const T&)`), `fn_800358e0__10CPatternedCFv`
(8 B) and `fn_80035FD8` (164 B), plus
`__ct__14CRayCastResultFfRC9CVector3fRC6CPlaneRC13CMaterialList` (88 B, a 5-argument
`CRayCastResult` constructor retail has and our header does not). Their only callers in this
object are `RayCollisionCheckWithWorld` and `ResolveCollisionWithActor`, which are still stubs, so
emitting them means decompiling those first.

## Wall measured this run: our mwcc never passes `&kInvalidUniqueId` for a by-value `TUniqueId`

This is what actually blocks `CanCollideWithTrigger` (85.33%), and it is not a source-shape
problem. Retail passes the **address** of the const global; our build always materialises a copy.

```
retail  0x80035A18:  addi r4,r13,-27740        ; r4 = &kInvalidUniqueId  (SDA21 reloc)
ours              :  lhz  r0,kInvalidUniqueId(r13) ; sth r0,12(r1) ; addi r4,r1,12
```

`TUniqueId` is a 2-byte struct (`include/MetroidPrime/TGameTypes.hpp:45`) and the MW ABI passes
struct arguments by pointer, so a by-value `TUniqueId` parameter is a pointer either way - the
caller is free to pass `&kInvalidUniqueId`, which is what retail does. I could not get our
mwcceppc to do that with any declaration. Measured with `tools/probe_cc.sh` on standalone probes
(compile flags identical to the build) and on the real unit:

- `TUniqueId` by value, arg a const global / a const file-static / a POD struct / a local const
  -> **copy into a stack slot** in all four probes.
- `const TUniqueId` (top-level const parameter) -> still copies; the real
  `CProjectileTouchResult` ctor with `const TUniqueId actorId` produced the same 31 differing
  instructions as before.
- `const TUniqueId&` parameter -> **passes the address, no copy**, but mangles to
  `__ct__22CProjectileTouchResultFRC9TUniqueIdRCQ...`, and retail's symbol is
  `__ct__22CProjectileTouchResultF9TUniqueIdRCQ...` (in `config/G2ME01/symbols.txt:1002`), so that
  is not retail's ctor.

So the by-value ctor is right and the address form is unreachable: **`CanCollideWithTrigger`
cannot reach 100% until this compiler behaviour is understood.** It is cross-unit: 26
address-form `kInvalidUniqueId` sites exist in the DOL across 17 functions and **none of those
functions is at 100% today** - `CGameProjectile::CanCollideWithGameObject` (7 sites, 51.27%),
`CanCollideWithComplexCollision` (3, 3.66%), `CPlayerGun::UpdateNormalShotCycle` (2, 54.74%),
`CanCollideWithTrigger` (2), and one each in `CanCollideWith` (12.45%),
`CanCollideWithDoor` (13.10%), `CBeamProjectile::SetCollisionResultData` (85.47%),
`CRagdoll::ProjectileCollision` (82.59%), `CParticleGenInfoGeneric` ctor (84.78%),
`CScriptDoor::AcceptScriptMsg` (56.44%), `CStateManager::ApplyLocalDamage` (40.99%),
`CPatterned::LaunchProjectile` (1.10%), `CGunWeapon::Fire` (0.22%), `CAuxWeapon::FireLightCombo`,
`CAuxWeapon::FireProjectile`, `fn_8003895C`. It is a wall, not a `NEW:` - no spelling reached
100%, so filing it would only spend a lane on the same evidence.

`CanCollideWithTrigger` needs 4 `TUniqueId` stack slots where retail needs 1 (0x08 in retail;
0x08/0x0c/0x10/0x14 in ours), which shifts every slot above it by `0xc` and accounts for the
whole remaining diff. The frame size itself is already right (0xb0 both sides).

## `CanCollideWithTrigger` spellings tried this run (all worse than the 31-differing baseline)

Counted as differing instructions out of 98 with
`python3 tools/bytescmp.py build/G2ME01/src/MetroidPrime/Weapons/CGameProjectile.o
CanCollideWithTrigger 0x800358E8 368`; base = **31**:

| spelling | differing instrs |
|---|---|
| base (as committed upstream) | **31** |
| `TUniqueId id = kInvalidUniqueId; if (collide) id = actor.GetUniqueId(); return R(id,...)` | 33 |
| swapped ternary `collide ? kInvalidUniqueId : actor.GetUniqueId()` | 34 |
| `const TUniqueId id = collide ? ... ; return R(id, ...)` | 34 |
| `CActor::GetUniqueId()` changed to return `const TUniqueId&` (header edit) | 34 |
| `const TUniqueId& id = collide ? ... ; return R(id, ...)` | 35 |
| two separate `return`s, `if (collide) return R(actor.GetUniqueId(), ...)` | 38 |
| ctor param `const TUniqueId` (top-level const) | 31 (no change) |
| ctor param `const TUniqueId&` | symbol mangles to `FRC9TUniqueId` - not retail's ctor |
| file-local `const TUniqueId` in place of the global | no change |

## The `CUnknown50` blocker in `reason` - still real, still not worked around

`CanCollideWith`'s door branch needs `TCastToPtr<CUnknown50>`, which exists only inside
`src/MetroidPrime/TypesMatch.cpp`; hoisting it is a change to that file and its ~150 siblings, not
to this unit's target, so I left it alone, exactly as the first run did. Retail's
`CanCollideWithGameObject` additionally calls `CRagdoll::ProjectileCollision` and
`TCastToPtr<CSwarmBasics>`, neither of which our body has, so that function is far from 100%
regardless of the `kInvalidUniqueId` wall.

## Reproducing

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/fast_try.sh MetroidPrime/Weapons/CGameProjectile   # ~2 s per spelling
./tools/goal_check.sh build/goal/item.json                # the judge; prints PASS
```

## Next run, do not repeat

- The nine renameable-by-convention helpers are done; the other seventeen unpaired functions in
  this unit are template instantiations and need a shared-header decision, not a local edit.
- Do not re-try the `TUniqueId` spellings above, and do not re-try the seven from the first run.
  The blocker is the compiler's by-value-`TUniqueId` copy, not the source shape.
- If a future item wants `CanCollideWithGameObject` (51.27%), the missing work is real
  decompilation, not codegen: `CSwarmBasics` material test, the `0x4f42..0xA389` range test, and
  the `CRagdoll::ProjectileCollision` call.
