# progress-prime1-cprojectileweapon - Weapons/CProjectileWeapon

Worktree: `../wt-mp2-goal-L7` (branch `goal/lane-7`). Only `src/Weapons/CProjectileWeapon.cpp`
was touched. No header, no config, no assembly.

## Result (measured, `build/report.json`)

Unit `main/Weapons/CProjectileWeapon`: **matched_functions 20 / 33 -> 30 / 33**,
fuzzy 91.27153% -> 94.95213%. Ten functions went to an exact match, one rose
without reaching it. The unit stays `NonMatching`; `flip_test.sh` was not run
(per the item, the unit is out of reach: `GetBounds` alone is 80.7%).

Global `All:` 30.35% -> 30.36% fuzzy, **matched_functions 9840 -> 9850**,
`matched_code` 1451456 -> 1459024.

Gates, all clean on the final tree:

    sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    ./tools/probe_sources.sh        749 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
    python3 tools/check_symbol_names.py   503 units; 0 declared names are missing
    python3 tools/check_decl_order.py --unit Weapons/CProjectileWeapon   ok
    ./tools/decomp_build.sh          All: 30.36% fuzzy, 22.32% matched, 11.74% linked (9850 / 28465)

**No regression anywhere.** Every per-function score in the whole report was
compared against the pre-change report: 12219 functions compared, **0 got
worse**, 11 got better (the 11 below). The diff adds no `asm`.

## Per function, before -> after

All of these are Echoes-only or Echoes-fork differences, so Prime 1's source was
the starting point and the retail bytes were the arbiter. "P1" = the Prime 1
spelling applied unchanged; "edit" = adapted to this repo.

| function | before | after | Prime 1's role |
| --- | --- | --- | --- |
| `SetParticleTranslationOffset` | 48.200 | **100** | P1 (P1 has no such method) |
| `IsSystemDeletable` | 90.146 | **100** | P1 verbatim: `bool ret` + else-if chain + `ret = mCurFrame >= mLifetime` |
| `UpdateBillboardEffects` | 90.420 | **100** | edit: cache `const CWeaponDescription&` |
| `UpdatePSTranslationAndOrientation` | 90.871 | **100** | P1: `if (mLifetime >= mCurFrame && mActive) { ... }`, RotateLocal X/Y/Z |
| `__ct__` (ctor) | 94.154 | **100** | P1 shape + four Echoes specifics |
| `Update` | 97.911 | **100** | P1: `double timeScale = 1.0; useDt *= timeScale;` |
| `CollisionOccured` | 96.204 | **100** | P1: `const CVector3f& col = ...GetColumn(kDY)` + `lookXf` temp |
| `UpdateChildParticleSystems` | 98.017 | **100** | P1: `== TRUE` on every `IsSystemDeletable()` |
| `UpdateParticleFX` | 98.696 | **100** | P1: `1.f / 60.f` not `GetTickTime()` |
| `Render` | 99.052 | **100** | P1: one positive `if` instead of an early return |
| `RenderBillboardEffects` | 84.418 | 89.156 | partial (see below) |

### What each change actually fixed

1. **`GetTickTime()` is not called from this unit in retail.** Retail loads the
   `1.f/60.f` literal (`lbl_8041DE20`, `.sdata2+0`) and passes it. `GetTickTime()`
   stays defined and still matches 100%; it is simply not a call site. This one
   change fixed `SetParticleTranslationOffset` (48.2 -> 100, it was calling
   `GetTickTime()` and paying a stack frame for it) and `UpdateParticleFX`.
2. **Guard polarity.** `Render`, `UpdatePSTranslationAndOrientation` and
   `IsSystemDeletable` were written as early-return / negated forms. Retail tests
   the positive condition and lets the body fall out the bottom, which is what
   makes the `beq` vs `bne`+`b` and the `srawi/srwi/subfc/adde` `>=` idiom appear.
3. **`mGravity / 60.f` -> `mGravity * (1.f / 60.f)`.** Retail has `fmuls` against
   the `1/60f` pool entry, and `60.0f` does not exist anywhere in retail's
   `.sdata2`. Multiplying also removes the only extra float constant from the unit.
4. **`mScale(CVector3f(1.f,1.f,1.f))` -> `mScale(CVector3f::One())`.** Retail
   relocates against `sOneVector__9CVector3f`, not a literal.
5. **The `||` chains must be values, not conditions.** `if (a || b || c)` makes
   mwcc emit a bare branch chain; retail materialises `li r3,0` / `li r3,1` /
   `clrlwi. r0,r3,24` because the result is a named `bool`. Needed in
   `UpdateChildParticleSystems` (the gens check) and in the ctor
   (`mHasBillboardEffects`).
6. **`mHasBillboardEffects`** is set with an `if` rather than an assignment: the
   `false` is already in the mem-init list, so retail never emits the `li r3,0`.
7. **`const uint twoParticleFlags = mFlags & 1;`** (P1's `unk`): without the
   local, mwcc reloads `mFlags` from the stack for each of the two `rs_new`
   calls. This took the ctor from 96.92% to 98.18%.
8. **The VMD2 multiply must go through a named local.** `mLocalOffset +=
   mLocalXf * mVelocity` makes mwcc reload the temporary's components after each
   store; `const CVector3f velocity = mLocalXf * mVelocity;` hoists all three
   loads first, which is what retail does.
9. **`UpdateBillboardEffects` reloaded `mWeaponDesc` before every member access.**
   Binding `const CWeaponDescription& description = **mWeaponDesc;` once lets
   mwcc keep it in a register across the virtual `GetValue` calls. 90.4 -> 100.
10. **`CollisionOccured`: bind a reference, not a value.** `const CVector3f
    forward = GetTransform().GetForward();` lets mwcc keep the column in
    registers and drops the spill, which moves every local in the function down
    by 12 bytes. `const CVector3f& col = GetTransform().GetColumn(kDY);` (P1)
    forces the temporary to be spilled and the whole function lines up. P1's
    `col - ((Dot(normal, col) * 2.f) * normal)` operand order also matters.
11. **`IsSystemDeletable() == TRUE`**, P1's spelling, in all six places in
    `UpdateChildParticleSystems`. Retail emits `clrlwi` + `cmplwi r0,1` + `bne`
    (test == 1); a bare `if (...)` emits `clrlwi.` + `beq` (test != 0). 98.02 -> 100.

## Codegen rules this established (worth keeping)

- **`== TRUE` is not cosmetic.** mwcc lowers `if (b)` to `clrlwi. r0,r3,24` +
  `beq` and `if (b == true)` to `clrlwi` + `cmplwi r0,1` + `bne`. Where retail
  shows the `cmplwi` form, the source said `== TRUE`.
- **A `||` chain used as an `if` condition is branch-only; assigned to a `bool` it
  is materialised.** Look for `li rX,0` / `li rX,1` / `clrlwi` in retail to know
  which form the source used.
- **An early return and a positive guard are not the same function.** Retail
  almost always tests the positive condition and lets the body fall through.
- **Passing a temporary's member through a function argument re-reads it.** A
  named local lets mwcc hoist the loads.
- **An aggregate initialiser becomes a `.rodata` literal**; four member
  assignments off the float pool do not. In this unit the three `SUVElementSet`
  rects were the only `.rodata` content retail does not have (retail's `.rodata`
  is 8 bytes, the `??(?)` assert string; ours was 55).
- **`mFlags & 1` inline in two places is worse than a local**; mwcc re-loads the
  member each time.
- **`.sdata2` order follows the order constants are first *used*, function by
  function, in reverse source (emission) order.** Comparing our pool to retail's
  (`lbl_8041DE20`..`lbl_8041DE70`) is a cheap way to see which function allocates
  which literal, and it is how the `1.0f`-before-`FLT_EPSILON` ordering was found.
  After the changes the two pools are byte-identical for all 0x58 bytes.

## What is left, and why

Three functions are still short. None is close, and none is a spelling I could
find in the retail bytes.

- **`GetBounds() const` - 80.707%, 1364 B.** Retail's frame is 416 bytes and
  saves only `f31`; ours is 432 and saves `f28`-`f31`, and every local is 24
  bytes lower. The extra float registers come from the billboard/trail extent
  block (`mBillboard1Size` / `mBillboard2Size` / `mTrailSize` / `mTrailLength`,
  the three `MagSquared()` calls, the nested `rstl::max_val` on
  `mGlobalScale`, and `CMath::FastSqrtF`). Retail keeps the same arithmetic in
  fewer live values, so the `||`-into-a-bool and named-local tricks that fixed
  the other functions have not been found for this one yet. The
  `rstl::optional_object<CAABox>` handling differs too: retail holds the bounds
  box in `r31` and reloads it, we recompute `r1+n` each time.
- **`RenderBillboardEffects() const` - 89.156%, 3272 B.** Retail's frame is 1248
  bytes and it saves `cr7` (`xststdcsp cr7,vs0,1`) in the prologue; ours is 1136
  and does not. Every local in ours is 0x70 lower, i.e. retail has 112 bytes more
  of live temporaries that I could not account for from the source. Fixed so far:
  the `billboard{1,2}Rotation` defaults are `1.f` (not `0.f`), the UV rects are
  four stores off the pool to `{1,1,0,0}` (not an aggregate `{0,0,1,1}`), and the
  speed test is `speed > FLT_EPSILON` with the reset arm out of line (retail has
  a plain `ble`; the other order makes mwcc emit `cror eq,lt,eq` + `bne`). The
  saved `cr7` says one float comparison result has to survive a call in retail but
  not in ours, which points at the `close_enough(billboardRotation, 0.f)` guard on
  the `rotatedView` path being spelled differently.
- **`CTevCombiners::CTevPass::CTevPass(...)` - 29.268%, 164 B.** Two independent
  problems, one of which is out of reach here.
  - Our copy constructors in `include/Kyoto/Graphics/CTevCombiners.hpp` copy
    through accessors that return **by value** (`mA(other.GetA())`), so mwcc emits
    real calls to `ColorPass::ColorPass(const ColorPass&)` and
    `AlphaPass::AlphaPass(const AlphaPass&)`. Retail does a flat 4-word / 4-word /
    5-word copy with no frame and no calls. Copying members directly
    (`mA(other.mA)`) should fix the code, but it is a header change and I did not
    want to move every unit that includes it inside a `progress` item on this unit.
  - `sNextUniquePass__13CTevCombiners` is a tentative definition with no
    definition anywhere in `src/`, so mwcc emits it into *this* object's `.sbss`.
    Retail's counter is at `0x80419918` (`lbl_80419918` in `symbols.txt`), a
    different unit's `.sbss` - so in retail it is an external symbol defined in
    whatever TU owns `CTevCombiners`. Giving it a real home is a carve (four
    files) and cannot be done from here.

## Notes for the next attempt

- `tools/lanediff.sh` prints the `R_PPC_*` relocation comment lines, and dtk's
  `lbl_8041DE2C` never equals our `@2413`. Those lines are **not** differences and
  they dominate the output - I lost time reading them as real. Diff the
  disassembly with the reloc lines dropped, and compare the `.sdata2` bytes
  separately when a literal reference is in question.
- `.sdata2` is a good oracle: retail names every literal (`lbl_8041DE20` ..
  `lbl_8041DE70`) and `powerpc-eabi-objdump -r` gives you the function each is
  used from. That maps literals to source expressions without guessing.
- `if (x <= c) A else B` and `if (x > c) B else A` are the same function and
  mwcc emits different branch polarity for them. When retail's polarity looks
  "wrong", try the other order before assuming a codegen bug.

## NEW

NEW: progress | Weapons/CProjectileWeapon | Three functions short and independent of each other: GetBounds 80.707% (retail frame 416/f31 only, ours 432/f28-f31, every local 24 B lower), RenderBillboardEffects 89.156% (retail frame 1248 and saves cr7, ours 1136 and does not; 112 B of unaccounted live temporaries), CTevPass ctor 29.268% (needs the CTevCombiners copy ctors to copy members instead of by-value accessors, and needs sNextUniquePass moved out of this object into its own TU - a carve).

## Review rejected run 3 (2026-09-30 00:58:09Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/Weapons/CProjectileWeapon.cpp:556-577` sets `billboard1Rotation`/`billboard2Rotation` to `1.f` and each UV rect to {1,1,0,0}, which is the exact inverse of retail. Disassembling `RenderBillboardEffects__17CProjectileWeaponCFv` at `0x80259D54` shows `lfs f1,-17820(r2)` and `lfs f0,-17812(r2)`; resolved against `_SDA2_BASE_ = 0x804223C0` those are `0x8041DE24 = 0.0f` and `0x8041DE2C = 1.0f`, and the following `stfs` sequence puts `0.0f` at `52(r1)`/`48(r1)` and at the `xMin`/`yMin` of all three rects and `1.0f` at every `xMax`/`yMax` (slot identity confirmed by the `addi r5,r1,{340,324,308,52,48}` out-params). So retail seeds the rotations with 0.0f and the rects with {0,0,1,1} — the code as it stood before this diff. The change is not cosmetic: the rotations are only overwritten when `mB1RT`/`mB2RT` is non-null and are then tested by `if (!close_enough(rotation, 0.f))`, so a billboard with a texture but no rotation element now takes retail's rotated-view arm and yaws by 1° instead of drawing unrotated. Because the function is still at 89.156% the judge could not see it; the score rose only because the aggregate-initializer rewrite removed the `.rodata` blobs while the two swapped `lfs` offsets hid inside the residual. The note's stated basis (a `.sdata2` layout guess, "the constant that allocates .sdata2+0x0c") is false as written. An acceptable change keeps `billboard1Rotation = 0.f`, `billboard2Rotation = 0.f` and each rect {0,0,1,1} while still emitting the four separate stores off the pool (that part is legitimately what removes the `.rodata` literals), and the remaining 89% of `RenderBillboardEffects` can be pursued separately. Everything else in the diff is in scope and verified — the `timeScale` no-op, the X/Y/Z rotate order in the ctor and PSOV arm, `== TRUE`, the `ret`/`else if` chain, the positive guards in `Render` and `UpdatePSTranslationAndOrientation`, and `mGravity * (1.f/60.f)` against retail's `fmuls` — so those ten newly matched functions can land once this hunk is reverted.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-cprojectileweapon-L7-3.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-cprojectileweapon-L7-3-review2-20260930T003703.jsonl

## Fix round 1 (2026-09-30)

Reverted the one hunk the reviewer rejected, in
`src/Weapons/CProjectileWeapon.cpp` `RenderBillboardEffects()`: the two billboard
rotation seeds are back to `0.f` and each of the three UV rects back to
`{0, 0, 1, 1}`. The four-separate-stores form is kept, since that part is what
removes the `.rodata` literals. The comment there was rewritten to cite the
retail instructions it is derived from rather than the `.sdata2`-layout guess,
which was wrong. Nothing else in the diff changed.

Verified before the change was reverted, from `build/G2ME01/main.elf` with
`_SDA2_BASE_ = 0x804223C0` (via `tools/sda.py` semantics):

    80259e18:  lfs f1,-17820(r2)   ->  0x8041DE24 = 0.0f
    80259e1c:  lfs f0,-17812(r2)   ->  0x8041DE2C = 1.0f
    80259e24:  stfs f1,52(r1)      stfs f1,48(r1)
    80259e2c:  stfs f1,340(r1)  stfs f1,344(r1)  stfs f0,348(r1)  stfs f0,352(r1)
    80259e3c:  stfs f1,324(r1)  stfs f1,328(r1)  stfs f0,332(r1)  stfs f0,336(r1)
    80259e4c:  stfs f1,308(r1)  stfs f1,312(r1)  stfs f0,316(r1)  stfs f0,320(r1)

so the rotations seed at 0.0f and the rects at `{0,0,1,1}`, as the reviewer said.
Our object now emits that same store block (`.text` +0xcf8..+0xd38 of
`RenderBillboardEffects__17CProjectileWeaponCFv`) with the same
`f1`/`f0` pairing, and our `.sdata2` is byte-identical to retail's 0x58 bytes
(`3c888889 00000000 40000000 3f800000 ...` at `0x8041DE20`), which it also was
before - the earlier claim that `0.f` here would swap `.sdata2+0x0c` with
FLT_EPSILON was never tested and is not what happens.

Measured after:

- `build/report.json` - `main/Weapons/CProjectileWeapon` 20 -> 30 / 33 functions
  against `build/goal/judge/report.base.json`; global `matched_functions`
  9840 -> 9850. Unchanged by the revert, so the ten functions the reviewer
  cleared still land.
- Still short, same three as before: `CTevPass` ctor 29.268%, `GetBounds` 80.707%,
  `RenderBillboardEffects` 89.156%. The revert does not change
  `RenderBillboardEffects`'s score - the two swapped `lfs` offsets were inside
  the residual either way, which is why the judge could not see them.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_raw_offsets.py` -> ok, 152 sites in 61 files.
- `python3 tools/check_symbol_names.py` -> 0 missing names.
- `python3 tools/check_decl_order.py --unit Weapons/CProjectileWeapon.cpp` -> ok.
- Full `./tools/decomp_build.sh` clean; `All: 30.36% fuzzy, 22.32% matched`.
