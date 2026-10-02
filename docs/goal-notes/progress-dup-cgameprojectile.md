# progress-dup-cgameprojectile

**Judge PASS.** `main/MetroidPrime/Weapons/CGameProjectile: 21 -> 31 / 49 functions` (10 matched,
none worse); tree `matched 13133 -> 13143`, `linked 6225 -> 6225`; DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs, probe, decl order, raw offsets, wiring
all green (`./tools/goal_check.sh build/goal/item.json`).

## What the four functions are, and what actually matched them

The item's hint was right: they are **compiler-emitted members and `rstl::` template
instantiations, not hand-written bodies**, and the tree already emits every one of them under
the mangled name mwcceppc gives - so the fix was to give the *retail* symbol that name
(`tools/apply_rename.py`), which is what makes objdiff pair the two. **Before/after, all
0.00% -> 100.00%, and the construct that owns the bytes:**

| retail | size | new name in `config/G2ME01/symbols.txt` (the construct) |
|---|---|---|
| `fn_80036104` | 128 | `__ct__Q218CImpactVisorEffect15SParticleEffectFRCQ218CImpactVisorEffect15SParticleEffect` - the implicit copy ctor of `CImpactVisorEffect::SParticleEffect` |
| `fn_80032D0C` | 92 | `__dt__Q24rstl56optional_object<Q218CImpactVisorEffect15SParticleEffect>Fv` - `optional_object<SParticleEffect>`'s destructor (flag at +0x14, then `destroy`) |
| `fn_80032D88` | 72 | `destroy<Q218CImpactVisorEffect15SParticleEffect>__4rstlFPQ218CImpactVisorEffect15SParticleEffect` - `rstl::destroy<T>` with `destroy_impl` inlined |
| `fn_8003607C` | 64 | `__ct__Q24rstl56optional_object<Q218CImpactVisorEffect15SParticleEffect>FRCQ24rstl56optional_object<Q218CImpactVisorEffect15SParticleEffect>` - `optional_object<SParticleEffect>`'s copy ctor (flag at +0x14, then `construct`) |
| `fn_800360BC` | 32 | `construct<Q218CImpactVisorEffect15SParticleEffect>__4rstlFPvRCQ218CImpactVisorEffect15SParticleEffect` |
| `fn_800360DC` | 40 | `construct_impl<Q218CImpactVisorEffect15SParticleEffect>__4rstlFPvRCQ218CImpactVisorEffect15SParticleEffect` - `new (dest) T(src)`: one null test, then the copy ctor |
| `fn_80032CB8` | 84 | `__dt__18CImpactVisorEffectFv` - `CImpactVisorEffect`'s destructor |
| `fn_80034D28` | 64 | `__ct__Q24rstl33optional_object<14CRayCastResult>FRCQ24rstl33optional_object<14CRayCastResult>` |
| `fn_80034D68` | 32 | `construct<14CRayCastResult>__4rstlFPvRC14CRayCastResult` |
| `fn_80034D88` | 40 | `construct_impl<14CRayCastResult>__4rstlFPvRC14CRayCastResult` |

The four *named* ones were done first, in the item's order; the other six are the same rename in
the same unit and came out of the same pass (retail's ladder is
`fn_80032CB8 -> fn_80032D0C -> fn_80032D68 -> fn_80032D88` and
`fn_8003607C -> fn_800360BC -> fn_800360DC -> fn_80036104`, all in this unit's claim).

**One source fix was required** and it is the reason the 128-byte copy ctor is now byte-identical
rather than 8 bytes long: `CImpactVisorEffect::SParticleEffect::mSendCollideMessage` was declared
`bool ... : 1`, and a bit-field is copied with `rlwinm`/`rlwimi`; retail copies the whole byte
(`lbz r0,18(r31); stb r0,18(r29)`). It is a plain `bool`
(`include/MetroidPrime/Weapons/CImpactVisorEffect.hpp:18`). Offsets and size are unchanged
(`CHECK_SIZEOF(CImpactVisorEffect, 0x3c)` still holds), and no function in the tree moved.

Verified before renaming, with a byte comparison of the two objects' `.text` (not objdiff):
`build/G2ME01/src/MetroidPrime/Weapons/CGameProjectile.o`'s ten symbols are byte-identical to the
retail object's ten (128/64/32/40/72/92/84/64/32/40 bytes, 0 differing bytes each). Then
`./tools/gate.sh` (per-function diff: `+10 functions at 100%, 0 units newly linked`, nothing
worse) and `./tools/goal_check.sh`.

## Not matched, and what stops each (measured this run, all in this unit)

- `fn_80032D68` (32 B) is `destroy_impl<SParticleEffect>` (`bl fn_80032D88`). Our `destroy<T>`
  inlines `destroy_impl`, so the object has no such symbol; a body would need a use that keeps it
  out of line.
- `fn_80035FD8` (164 B) is `__ct__18CImpactVisorEffectFRC18CImpactVisorEffect`, which our tree
  **does** emit - but 156 B, so it does not pair. The 8 bytes: retail's `optional_object<SBlurEffect>`
  copies with `addic. r3,this+0x18; beq` + `lwz/lfs/stfs` (typed moves) and its
  `optional_object<pair<int,float>>` copies through a checked `r3` too, while ours copies the
  SBlurEffect with `lwz/stw` and drops the pair's null test. Fixing the copy path (not the
  members) is what this needs.
- `fn_80034CE8` (64 B) is the copy constructor of the class with a `u16` at 0 plus
  `optional_object<CRayCastResult>` at +4 (`CProjectileTouchResult`, 0x38); nothing in the source
  copies one, so no symbol is emitted.
- `fn_800350A0` (56 B) is `optional_object<CRayCastResult>::optional_object(const CRayCastResult&)`
  (`m_valid = true; construct<T>(m_data, item);`) - also not emitted.
- `fn_80034C8C` (92 B) installs two vtables and copies +8..+0x24; not emitted.
- `fn_80034C34` (88 B) is `__ct__14CRayCastResultFfRC9CVector3fRC6CPlaneRC13CMaterialList`, which
  the header defines but this TU never calls.
- The 27 REL copies (`fn_1_111C` in AIMannedTurret, `fn_2_135C` in AtomicAlpha, ... the same ten
  shapes in 27 more modules) are unchanged work: each module needs the same rename in its own
  `config/G2ME01/rels/<Module>/symbols.txt` **and** that module to be linked from our own code,
  which is the module recipe, not a rename alone.
- Still unwritten bodies in this unit (`ResolveCollisionWithActor`, `Chase`,
  `RayCollisionCheckWithWorld`, `ApplyDamageToOneActor`, the four `CanCollideWith*`, `fn_80035FD8`'s
  neighbours) are the real remaining function pool: 18 unmatched, 8 of them `fn_`/template
  instantiations above.

## Note for the next run (a NEW: is *not* filed for this)

`tools/check_decl_order.py` now reports this unit **permuted**, which it could not see before the
rename (both sides spelled these functions `fn_`). One inversion: our object emits
`construct<14CRayCastResult>` at `.text+0xb3c`, after `CanCollideWith` at `.text+0xa84`; retail has
it at 0x80034D68, before `CanCollideWith` at 0x80034DB0. The weak copy hangs on the source
function that first needs it (`CanCollideWith`'s
`return CProjectileTouchResult(kInvalidUniqueId, rstl::optional_object_null());` TODO), so the
reorder follows that function's real body. The unit was added to
`docs/research/decl_order.md` with that measurement, which is what the checker wants for a
permuted unit; the unit is `NonMatching`, so no flip is in reach regardless.

`NEW:` **not** filed: the remaining candidates (`fn_80035FD8`, `fn_80034CE8`, `fn_800350A0`) are
the same unit as this item, and `goal_seed.py --only dup` still has this unit's shape in the
queue, so a second item here would be a duplicate rather than new work.
