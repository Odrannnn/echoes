# progress-prime1-cbomb

**Result: `MetroidPrime/Weapons/CBomb` 5/12 -> 7/12 functions at 100%.** Unit `.text` fuzzy
97.1575% -> 97.9632%, `matched_code` 38.3057% -> 53.4070% (1664 -> 2320 of 4344 bytes).
Project `All:` matched functions **11317 -> 11319 / 28465**, `linked` 5507 -> 5507 (unchanged).
No function anywhere got worse, no `asm` added. The unit stays `NonMatching`; `flip_test.sh` was
not run, as the item says.

Diff is **one file**: `src/MetroidPrime/Weapons/CBomb.cpp` (+21/-14). No header touched, so no
class layout or `CHECK_SIZEOF` moved.

Verified with the judge itself:

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11317 -> 11319   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.57% fuzzy, 25.24% matched, 11.94% linked (11319 / 28465 functions)
  ok    target rose: main/MetroidPrime/Weapons/CBomb: 5 -> 7 / 12 functions
  ok    no asm added
goal_check: PASS progress-prime1-cbomb
```

## Per function, as the item asks for

| function | before | after | Prime 1's source |
|---|---|---|---|
| `CBomb::Touch` | 95.56 | **100.00** | **did not help.** Prime 1's `Touch` is a two-line stub (`if (!mIsNotDetonated) return;`) - Echoes added `mBeingDragged`, the id test, the material test and the sphere test. Fixed from the measured diff. |
| `CBomb::AddToRenderer` | 99.80 | **100.00** | **Prime 1's body shape was the answer, adapted.** Prime 1's signature is `AddToRenderer(const CFrustumPlanes&, const CStateManager&)` and it gets the radius via `mgr.GetPlayer()`, so that part had to stay as this repo's `TCastToPtr< CPlayer >(mgr.GetObjectById(GetOwnerId()))`. The two `CVector3f(r,r,r)` temporaries and the named `forward` local are Prime 1's verbatim. |
| `CBomb::Think` | 97.48 | **99.95** | **did not help.** Same skeleton, different game: Prime 1 branches on `mFuseTime > 0.5f`, calls `CActor::GlobalMove` (absent here) and passes a `CVector3f` position to `Explode` (here it is an `rstl::optional_object`). Fixed from the measured diff; see the wall below. |
| `CBomb::Explode` | 89.92 | 89.92 (untouched) | **different API.** Prime 1's `Explode(const CVector3f& pos, CStateManager&)` has no optional position, no player double-damage sfx and no `CGameLight`. Nothing to carry over. |
| `CBomb::AcceptScriptMsg` | 94.24 | 94.24 (untouched) | **different API.** Prime 1 is `ENTITY_ACCEPT_IMPL(CBomb)` plus `(EScriptObjectMessage, TUniqueId, CStateManager&)`; this repo is `(CStateManager&, const CScriptMsg&)`. Not attempted. |
| `fn_80083FD8`, `fn_80083FDC` | 0.00 | 0.00 (untouched) | not in Prime 1. Both are a bare `blr` (4 bytes) at the end of CBomb's `.text`. |

## The two new 100% functions

### `CBomb::Touch` - 95.56 -> 100.00

Two independent differences, both pure source shape:

1. **The material test's operand order.** `SharesMaterials(other)` is
   `other.value & value`, so the `and` operand order tells you which side is the receiver.
   Retail computes `actor.mMaterialList & mTriggerMaterials`; the tree had it the other way
   round. Writing `mTriggerMaterials.SharesMaterials(actor.GetMaterialList())` puts the loads
   in retail's order. **This alone was worth 95.56 -> 98.00.**
2. **The second bit-field test needs `beq cont; b end`, not `bne end`.** Retail emits
   `rlwinm. r3,25` / `beq end` / `rlwinm. r3,26` / `beq cont` / `b end`. Every flat spelling -
   `||` chain, four separate early returns, `if (A && !B) { ... }`, `if (!A || B) return; else` -
   except one peepoles to `beq end` / `bne end`, two instructions. **The explicit `else` is what
   forces retail's three**: only
   ```cpp
   if (!mIsNotDetonated || mBeingDragged) {
     return;
   } else {
     ...
   }
   ```
   reproduces it. Verified: removing just the `else` drops it straight back to 98.00.

### `CBomb::AddToRenderer` - 99.80 -> 100.00

Only stack-slot permutation, in a 436-byte function. Prime 1's shape is the fix: the tree had a
named `const CVector3f extent(radius, radius, radius)` and an unnamed forward-vector temporary,
which put four temporaries at 40/52/16/64 instead of retail's 28/40/64/16. Prime 1 writes the
`CVector3f` inline in the `CAABox` ctor arguments and names the forward vector, which lands all
four where retail has them. No header or class change; the radius source stays as this repo's
owner-id lookup.

## `CBomb::Think` - 97.48 -> 99.95, not 100

Two real fixes, then a wall.

- `SetTranslation(GetTranslation() + dt * mVelocity)` materialises **two** stack temporaries
  (996 bytes, 3 instructions longer than retail). Binding the product first -
  `const CVector3f v = dt * mVelocity; SetTranslation(GetTranslation() + v);` - keeps it in
  registers, which is what retail does: three `fmuls` then three `fadds` with no intermediate
  store. Writing the sum out component by component
  (`CVector3f(GetTranslation().GetX() + dt * mVelocity.GetX(), ...)`) also reaches retail's size
  but makes mwceppc **fuse** into `fmadds`, which retail does not use - 97.77%.
- `(1.f / distance) * delta` inline -> `const float inv = 1.f / distance; ... inv * delta`. This
  does not change the instruction count at all, but it stops mwceppc assigning the `delta`
  reload to `f2/f1/f0` when retail uses `f0/f1/f2`: 99.90 -> 99.95.

**WALL: CBomb::Think 99.95% - opcode sequence is now identical to retail; only four temporaries'
stack slots are permuted.**

What is left, measured with `objdiff-cli diff -u main/MetroidPrime/Weapons/CBomb`: 21
`DIFF_ARG_MISMATCH`, **no** `DIFF_OP_MISMATCH`, `DIFF_INSERT` or `DIFF_DELETE`. The four
temporaries occupy the same total span (16..72) in both objects, just in a different order:

| temporary | retail | ours |
|---|---|---|
| `SetTranslation` argument (`CVector3f`) | 16 | 60 |
| `Explode`'s `optional_object` (ray-cast branch) | 28 | 16 |
| `RayStaticIntersection`'s direction argument | 44 | 32 |
| `Explode`'s `optional_object` (`distance == 0` branch) | 56 | 44 |
| `Explode`'s `optional_object` (fuse branch) | 72 | 72 |
| `delta` (named local) | 88 | 88 |
| `filter`, `result`, its copy ctor | 120 / 128 / 176 | 120 / 128 / 176 |

Every named local is already where retail has it; only compiler temporaries move, and the two
that come out identical (the fuse-branch `optional_object` and `delta`) bracket the permutation.
Roughly fifteen source spellings were measured and none changed the order - see the table below.

| spelling of the `mVelocity.MagSquared() > 0.f` body | Think |
|---|---|
| `SetTranslation(GetTranslation() + dt * mVelocity)` (the tree's original) | 97.48 (996 bytes) |
| `CVector3f(GetTranslation().GetX() + dt * mVelocity.GetX(), ...)` inline | 97.77 |
| `const CVector3f v = dt * mVelocity; SetTranslation(GetTranslation() + v)` | **99.90** |
| `const CVector3f v = dt * mVelocity; SetTranslation(v + GetTranslation())` | 99.78 |
| same, `delta` from `GetTranslation()` instead of `GetTransform().GetTranslation()` | 99.89 |
| same, `const CVector3f v(dt * mVelocity)` ctor form | 99.90 |
| same, `CVector3f(v.GetX()*dt, v.GetY()*dt, v.GetZ()*dt)` explicit | 99.90 |
| same, `CVector3f v = mVelocity * dt` | 99.90 |
| same, non-`const` `CVector3f v` | 99.95 |
| same, `const CVector3f pos = GetTranslation() + v; SetTranslation(pos)` | 99.91 |
| same, `const float inv = 1.f / distance;` + `inv * delta` | **99.95** |
| same, `const float inv = 1.f / distance;` + `delta * inv` | 99.95 |
| same, `inv` and non-`const` `delta` | 99.95 |
| same, `v` declared *before* `mPrevLocation = ...` | 95.46 |
| same, `v + GetTranslation()` and `inv` together | 95.41 |

## Not committed, as instructed. Tree state

`src/MetroidPrime/Weapons/CBomb.cpp` (+21/-14) is the whole hand-made diff; the state-block
rewrites the gate makes are machine-made and live in `build/`. The throwaway helpers used to get
here (all under `/tmp`, nothing added to the tree): `cmp3.sh <retail_addr> <size> <symbol>` -
normalised side-by-side of one function, retail from `main.elf` by address and ours from
`build/G2ME01/src/.../CBomb.o` by symbol; `fndiff.py <unit> <symbol>` - byte-level diff of one
function between `build/G2ME01/obj/` and `build/G2ME01/src/`; `odiff.py <objdiff.json> <prefix>` -
turns `objdiff-cli diff -u <unit> --format json` into a readable list of `DIFF_*` entries (note
the keys are `diff_kind` / `arg_diff`, not `base` / `ours`, and a mismatched *argument* does not
change `fuzzy_match_percent` enough to show up in the report); `vt2.sh` / `va.sh` - restore a known
good `CBomb.cpp`, splice one function body in, rebuild, print the five scores.
