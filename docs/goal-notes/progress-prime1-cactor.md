# progress-prime1-cactor

`kind: progress` · `target: MetroidPrime/CActor` · worktree `../wt-mp2-goal-L1` (lane 1) · 2026-09-30

**Result: `main/MetroidPrime/CActor` 56 -> 59 matched functions** (`All:` 9513 -> 9516, `linked`
unchanged at 4853). Three functions went to 100%; nothing else in the unit, or in any other
unit, changed by a single byte. The unit stays `NonMatching`; no `flip_test` was run and no
`configure.py`/`config/`/`asm` file was touched. Only `src/MetroidPrime/CActor.cpp` is modified.

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9513 -> 9516   linked 4853 -> 4853
  ok    check_symbol_names.py
  ok    All:  29.20% fuzzy, 21.35% matched, 11.64% linked (9516 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActor: 56 -> 59 / 98 functions
  ok    no asm added
goal_check: PASS progress-prime1-cactor
```

## The three that landed

All three are **Prime 1's `CActor.cpp` shape** adapted to this repo's accessors - the item's
suggestion was right, and each needed a *different* edit, not a transcription.

| function | before | after | edit |
|---|---|---|---|
| `CanDrawStatic__6CActorCFv` | 58.08% (152 B) | **100%** | Prime 1's three-statement `if` form, not one `return A && B && C && D;` |
| `InFluidId__6CActorCFv` | 44.50% (48 B) | **100%** | `if (empty) return kInvalidUniqueId; return back();` instead of a `?:` |
| `RemoveInvalidFluidIds__6CActorFR13CStateManager` | 89.49% (156 B) | **100%** | the `if`/`else` **arms swapped** (`!ObjectById` first) |

### `CanDrawStatic` - one boolean expression is not a chain of statements

Ours was a single expression. Retail's 38 instructions test `GetActive()`, then
`GetModelData()`, then `mDrawFlags`, and then test `IsNull()`/`HasAnimation()` **twice** -
which one `&&` chain cannot produce. Prime 1's shape does:

```cpp
if (!GetActive() || !HasModelData() || static_cast< char >(mDrawFlags.GetTrans()) > 4) {
  return false;
}
const CModelData* modelData = GetModelData();
if (modelData->IsNull() || modelData->HasAnimation()) {
  return false;
}
return true;
```

(Prime 1 writes `GetBlendMode()`; this repo's `CModelFlags` only has `GetTrans()`, and retail
Echoes reads the byte at `0x100` either way, so `GetTrans()` is the right accessor here.)

### `InFluidId` - a `?:` collapses where an early return does not

Ours emitted a **load from address 0**: the compiler merged both arms into one `lhz r0,0(r4)`
with `r4 = 0` on the empty path, because it saw `kInvalidUniqueId` and the vector's element
load as the same expression. Retail keeps two paths, one of them a relocated
`R_PPC_EMB_SDA21 kInvalidUniqueId` load. Writing the early return stops the merge. This is
worth knowing on its own: **the old code was a latent null dereference**, not just a byte
mismatch.

### `RemoveInvalidFluidIds` - branch polarity

Both bodies are the same two statements; only the order of the `if` arms differs, and that is
enough. MWCC makes the *first* arm the fall-through; retail falls through into
`mFluidIds.erase(it)` and branches over it. Prime 1's order is the opposite of Echoes' here.

```
ours   : if (obj) { ++it; } else { it = erase(it); }   -> branch TO the erase block
retail : if (!obj) { it = erase(it); } else { ++it; }  -> branch OVER the erase block
```

## What did not work (spellings tried, so the next run skips them)

**`GetOrbitPosition`, `GetAimPosition` (70.57%), `GetSortingBounds` (82.00%) - a measured
wall.** Retail copies the struct to the return slot **one float at a time, in order, with a
single FPR**:

```
lfs f0,0x54(r4) ; stfs f0,0(r3) ; lfs f0,0x58(r4) ; stfs f0,4(r3) ; ...
```

MWCC 2.7 always emits the **two-FPR interleaved** form instead
(`lfs f0; lfs f1; stfs f0; lfs f0; stfs f1; stfs f0`). I probed this directly with
`tools/probe_cc.sh` on a synthetic class - the two-FPR form comes out of *every* spelling:

- `return m;` / `return CVector3f(m.GetX(),m.GetY(),m.GetZ());` / `const CVector3f v = m; return v;`
  / `CVector3f v; v = m; return v;` / field-by-field `SetX/SetY/SetZ` / a user-declared copy ctor /
  a private `operator=` / a user-declared dtor / an in-class inline helper / a 6-float `CAABox`
  equivalent / the member at offset 84 / a base-class member / `const_cast` / `mutable` *is* the
  only lever - and it needs the **member** to be `mutable` or the **method** non-const.

The mechanism is aliasing: MWCC hoists the loads above the stores only when it can prove
`*sret` does not alias `this`, and a `mutable` member or a non-const `this` removes that proof.
Retail's symbol is `GetOrbitPosition__6CActorCFRC13CStateManager` - **const** - so the retail
source cannot be reaching it that way, and I could not find a const-method spelling that
reproduces it. Declaring `mPosition`/`mRenderBounds` `mutable` in `include/MetroidPrime/CActor.hpp`
*does* produce retail's bytes, but it is a shared header and would change codegen for every
unit that touches those members, so I did not do it. **The next thing to try is a member-level
`mutable`, measured against the whole report, not just CActor.**

**Prime 1's source verbatim, where Echoes forked (all measured worse or equal):**

| spelling tried | function | result |
|---|---|---|
| Prime 1's `GetYaw` (`sqrt`, `double ret = -atan2(...)`, no `CMath::SqrtF`) | `GetYaw` | **0.00%** (104 B) - completely different; keep `CMath::SqrtF` + `atan2f` |
| Prime 1's `IsModelOpaque` as an early-return chain instead of `else if` | `IsModelOpaque` | 95.24% -> 95.24%, no change |
| Prime 1's `SetModelData` (no `DeleteAllLights` call) | `SetModelData` | Echoes *does* have the call; keep ours |
| `if (!data.IsNull()) { new } else { free }` (polarity swap) | `SetModelData` | 90.93% -> **40.51%** |
| `if (!data.IsNull()) ... else if (...) ... else ...` | `SetModelData` | 90.93% -> **32.03%** |
| `const TUniqueId id = kInvalidUniqueId;` hoisted above the `switch` | `OnScanStateChange` | 99.79% -> **71.38%** (the load is hoisted out of all three arms) |
| `return GetTranslation();` / `return CVector3f(mPosition.GetX(),...)` | `GetOrbitPosition`, `GetAimPosition` | 70.57% -> 70.57% |
| `mValidTargetPlayers &= ~(1 << i) & 0xFu;` | `SetValidTarget` | 81.90% -> 76.19% |
| `mValidTargetPlayers = (mValidTargetPlayers & ~(1 << i)) & 0xFu;` | `SetValidTarget` | 81.90% -> 81.90% |
| `mTargetableVisorFlags &= ~flags & 0xFu;` | `SetVisorOrbitableFlags` | 83.75% -> 76.56% |
| `mTargetableVisorFlags = (mTargetableVisorFlags & ~flags) & 0xFu;` | `SetVisorOrbitableFlags` | 83.75% -> 83.75% |
| `... ? mEchoVolume : mNormalVolume` with a trailing `& 1` | `GetVisorSoundVolume` | 84.96% -> **81.21%** |

**A recurring, unexplained pattern: MWCC hoists the function's first load above the
callee-saved-register stores, retail does not.** This alone blocks `Render` (95.92% - 5 of 98
instructions differ, all of them the position of one `lwz r4,96(r3)`), `IsModelOpaque`
(95.24% - one `lbz`), `AddToRenderer` (98.97% - one `lwz` plus the order of the two
`GetShadow()` argument evaluations), `CanRenderUnsorted` (89.19% - one `lwz`/`li`), and
`GetLocatorTransform` / `GetScaledLocatorTransform` x2 (83.33% - one `lwz`). Every one of them
is instruction-for-instruction identical to retail apart from that hoist. I could not find a
source-level lever: the early-return reformulation, the Prime 1 spelling and the local-variable
spelling all leave the hoist in place.

**Two more, recorded so they are not re-derived:**

- `GetRenderAlphaBufferAlpha` (98.15%): retail materialises the `TUniqueId` argument **once** at
  `r1+8`; ours materialises it at `r1+8` *and* copies to `r1+12`, which is why ours is one
  instruction longer. `OnScanStateChange` (99.79%) is the same thing three times over - ours
  gives the three `SendScriptMsgs` calls three different outgoing slots (`r1+16/12/8`) and
  needs a 32-byte frame where retail needs 16. Both point at MWCC coalescing the by-value
  `TUniqueId` temp with the outgoing-argument slot in retail and not in this build. Changing
  `CPlayerTargeting::GetScanTargetIndex`'s second parameter to `const TUniqueId&` is the obvious
  experiment but it changes a shared header's symbol, so I did not try it blind.
- `SSound::SSound` (98.75%, 32 B) is a **register-allocation** difference only: ours
  `lwz r4,0(r4)` (destroys the incoming parameter register), retail `lwz r7,0(r4)`. Two of
  eight instructions. Same class of difference in `GetYaw` (11/26, identical instruction set) and
  `PlayCustomSound` (29/45, identical instruction set, only r27-r30 assignment differs).

`GetDistanceToCamera` (82.08%) is a genuine **register-pressure** difference, not scheduling:
retail's loop is 59 instructions in a 96-byte frame with no stack traffic, ours is 65 in 112
bytes and spills the three component deltas to `r1+8/12/16` and reloads `this->x`. The
instruction order of the three subtractions is already identical to retail's (y, x, z), so the
source would have to be restructured to hold one fewer value live, not respelled.

## Files touched

- `src/MetroidPrime/CActor.cpp` - `CanDrawStatic` (line ~848), `InFluidId` (line ~862),
  `RemoveInvalidFluidIds` (line ~868). 15 insertions, 6 deletions, one file.

## How it was verified

Per-function percentages come from `build/report.json` after `./tools/decomp_build.sh`, read
before and after each change and diffed, so "no other function moved" is a measurement rather
than an assumption. `tools/goal_check.sh build/goal/item.json` is the judge and it exits 0.
`docs/HANDOFF.md` is modified in the worktree only because `goal_check.sh` sets
`MP_GATE_DOCS_WRITE=1` and `check_docs_claims.py --write` re-derives the state block; I did not
edit it, and the driver discards it.

## Notes for the next run

- The `?:` -> early-return and if/else-arm-swap edits are cheap and general. Any function in
  this unit (or another) whose retail code loads a relocated `SDA21` constant on one arm and a
  struct element on the other is the `InFluidId` pattern; any loop whose retail body branches
  *over* the second arm is the `RemoveInvalidFluidIds` pattern.
- The cheap instrumentation is worth rebuilding: `objdiff-cli diff` only reports section-level
  percentages, so I wrote a small instruction-pair differ over
  `build/G2ME01/src/<unit>.o` vs `build/G2ME01/obj/<unit>.o` (dtk's retail object), plus a
  per-function percentage snapshot/diff. Both are throwaway; re-deriving them cost most of an
  hour. `tools/probe_cc.sh` + a synthetic class is the fast way to settle a codegen question
  without rebuilding the unit, and it is worth doing **before** touching the real source.
