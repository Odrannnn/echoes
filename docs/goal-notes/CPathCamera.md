# CPathCamera (match, MetroidPrime/Cameras/CPathCamera)

## Result

**Partial progress. The unit did not flip.** `tools/flip_test.sh MetroidPrime/Cameras/CPathCamera.cpp`
fails, as it must: six functions are still short of 100%. One function was matched outright.

| | before | after |
|---|---|---|
| unit fuzzy | 38.52% | 41.80% |
| unit matched code | 15.86% | 26.84% |
| **matched functions** | **9 / 16** | **10 / 16** |

Per-function (`build/report.json`, `main/MetroidPrime/Cameras/CPathCamera`):

```
  100.00%   836 B  MoveAlongSpline__11CPathCameraFfRC13CStateManager      (was 81.77%)  <-- matched
   95.71%   988 B  CalculatePositionDistance__11CPathCameraCFfRC13CStateManager  (was 85.81%)
   44.27%   120 B  __sinit_CPathCamera_cpp                              (unchanged)
    6.20%  1408 B  GetScanObjectIndicatorPosition__11CPathCameraCFRC13CStateManager
    2.88%  1580 B  Reset__11CPathCameraFRC12CTransform4fR13CStateManager
    0.70%   572 B  Think__11CPathCameraFfR13CStateManager
    0.44%   904 B  UpdateOrientation__11CPathCameraFfRC12CTransform4fRC13CStateManager
```

## What I changed

Only `src/MetroidPrime/Cameras/CPathCamera.cpp`. No header, no `configure.py`, no carve.

Every edit is a source-spelling change that makes mwcceppc emit retail's instruction shape.
No initialisation was deleted and no work was removed; each rewrite computes the same value.

- **`MoveAlongSpline` (81.77% -> 100.00%).** Three things, all needed:
  1. Inverted the `playerSpline.GetControlPointCount() == 0 || knotCount != 0` test to
     `!= 0 && == 0`, moving the body to the `else`. Retail lays the player-spline path out as
     the fallthrough and the `FindClosestLengthOnSpline`/`CalculatePositionDistance` path as the
     branch target; the polarity is what selects that layout. This alone was worth +14.5 points.
  2. Made `ret` a non-`const` named local assigned before `return ret;` instead of returning the
     call directly. Retail copies the result through a stack temporary into the sret pointer
     (three `stfs` to `0(r26)`); the direct return lets the compiler pass the sret pointer
     straight into `GetPositionByLength` and skips the copy.
  3. Spelled the look-at length as `camera->GetSpline().GetLookAtSpline().GetLength()` rather
     than reusing the cached `spline` reference. That one expression is the difference between
     retail's `addi r3,r31,36` and our `mr r3,r30` — the re-materialisation is what retail's
     register allocator did. Worth the last 3.4 points, and it is the one edit here that looks
     arbitrary: it is a codegen artefact, not a modelling statement.
  Also switched `GetPlayer(mgr)` to `Player(const_cast<CStateManager&>(mgr))`; retail calls the
  non-const `Player__11CGameCameraCFR13CStateManager`, not `GetPlayer`.
- **`CalculatePositionDistance` (85.81% -> 95.71%).** Retail uses six FPRs where we use five:
  - `float distance = 0.f;` declared at the *top* of the `GetFlags() & 4` block, then
    conditionally overwritten, instead of the `?:` conditional. Retail's `fmr f26,f2` hoists the
    zero to the block head; ours branched to a literal `lfs`.
  - Inverted `if (GetFlags() & 1) return newDistance;` to `if (!(GetFlags() & 1)) { ...damping...
    return ValidateLength(...); } return newDistance;`. Retail's `bne` makes the early return the
    branch target.
  - The non-closed-loop `newDistance` arm now branches on two `ValidateLength` calls instead of
    selecting an operand with `?:`.
  - The sign test re-derives the wrapped distance into a named `wrapped`/`remaining` pair shared
    by both arms (retail calls `GetLength` once for it, not once per arm) and negates with
    `step = step * -1.f` rather than `-step` (retail multiplies by a `-1.f` literal, it does not
    emit `fneg`).
  - Same `Player(const_cast<...>)` change.

## Verification

All measured, not recalled:

- `./tools/decomp_build.sh` — full build, **0 FAILED**, `main.dol` sha1
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (matches the pinned hash in `AGENTS.md`).
- All 86 RELs still `cmp`-equal and shasum-matching (`decomp_build.sh`'s `CHECK` step passed).
- `python3 tools/check_symbol_names.py` — `checked 503 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CPathCamera` —
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- **Function-level regression sweep across all 28465 functions** (diffed every entry of
  `build/report.json` against a baseline captured before I touched anything):
  `REGRESSIONS: 0, IMPROVED: 2`, no functions added or removed. This is the check that matters
  here, because the unit stays `NonMatching` so nothing else in the tree would have complained.
- `./tools/flip_test.sh MetroidPrime/Cameras/CPathCamera.cpp` — **FAIL**, reverted, tree rebuilt,
  `main.dol` hash restored. Expected; see below.
- `git status --short` shows only `M src/MetroidPrime/Cameras/CPathCamera.cpp`.

## Measured wall: the CMaterialList shared-header change (item.json's reason)

The item's `reason` says `__sinit_CPathCamera_cpp` "needs `CMaterialList::Add` to be a direct
store rather than a read-modify-write". **That change is worth taking, but not here: it is
net-negative across the tree.** Measured, with a full build and the same 28465-function sweep:

```
$ ./tools/decomp_build.sh
build/G2ME01/main.dol: FAILED        # 86 files OK, 1 checksum did NOT match
REGRESSIONS: 18   IMPROVED: 2
  100.00 ->  95.33  CGameProjectile::__ct__ (a Matching unit)
   98.85 ->  95.12  CBallCamera::__ct__
   98.77 ->  96.72  CBeamProjectile::UpdateFx
   94.17 ->  85.46  CGameHint::__ct__
   ... 14 more
  IMPROVED: __sinit_CPathCamera_cpp 44.27 -> 100.00
            CScriptEffect::__ct__    88.24 ->  89.22
```

Changing the one-arg constructor to `value(u64(1) << m1)` in
`include/Collision/CMaterialList.hpp` takes `__sinit` to 100% **and breaks the DOL hash**, because
the same constructor is used by 49 sites across 22 other files, several of them in `Matching`
units that currently reproduce retail. I reverted it; the tree is back to a correct build.

Local spellings of that same idea all scored *worse* than the shared `Add()` and were reverted:

| spelling | `__sinit` |
|---|---|
| `CMaterialList(u64(1) << kMT_Unknown59)` (folded literal) | 34.30% |
| same, via a file-scope `const EMaterialTypes` | 34.30% |
| via a non-inlined helper returning `CMaterialList` | 66.63% (sret call, wrong shape) |
| via an `inline` helper | 34.30% (folds again) |
| **shared-header change (reverted)** | **100.00%, 18 regressions** |

Retail's `__sinit` calls `__shl2i` with the shift count arriving in `r5` from a `.sdata2` load,
i.e. the count is a *runtime parameter*, and mwcceppc does not re-fold a shift after inlining.
A file-scope literal always folds, so the count can only stay a parameter by changing the shared
constructor. That is why the fix is a header change and why it cannot be local.

WALL: __sinit_CPathCamera_cpp 44.27% - needs a shared CMaterialList ctor change worth 18
function-wide regressions; not reachable from this unit alone.

## What still blocks the flip

Four large functions are unported, and they are the whole remaining gap:

- `UpdateOrientation` 904 B, 0.44%
- `Think` 572 B, 0.70%
- `GetScanObjectIndicatorPosition` 1408 B, 6.20%
- `Reset` 1580 B, 2.88%

All four are still the `// TODO:` stubs in the source. Together they are 4464 of the unit's
7616 code bytes; no amount of register-allocation work on the two near-miss functions will flip
the unit without them. `CalculatePositionDistance` also has ~4% left, which from the remaining
`lanediff` is **only** register assignment (which of `f26`/`f0` holds the addend, and the operand
order of one `fmuls`), not semantics — a plausible target for the next attempt, but it needs a
different spelling than any I tried.

## Codegen rules learned (do not file as items)

- MWCC lays out an `if` so the **first-listed body is the fallthrough**. Inverting a test can be
  worth 10+ points on its own with identical semantics.
- A ternary over a call that reads back its own result compiles to a branch + literal load; a
  local initialised to the same value and conditionally overwritten keeps it in a register.
- Retail calls the **non-const** `Player(CStateManager&)` from functions that take
  `const CStateManager&`; `const_cast` at the call site is what links the right symbol.
- Retail negates a float with `fmuls` by a `-1.f` literal, not `fneg`. `x = x * -1.f` matches;
  `x = -x` does not.
- Where retail re-materialises a pointer (`addi r3,r31,36`) where a cached copy exists (`mr r3,r30`),
  spelling the receiver through the original owner reproduces it. This fixed the last 3.4% of
  `MoveAlongSpline` and is the only edit here that is pure codegen archaeology.
- When retail computes an expression once and shares it across both arms of a branch, a C++ local
  is needed; writing the expression in each arm makes the compiler call `GetLength` twice.

## NEW items

None. The four unported functions are not new information — this item already named all four,
and they are the same unit. The `__sinit` header change is a *cross-cutting* edit whose blast
radius is 18 functions in 22 other files; it is a decision for the orchestrator about whether to
spend a lane re-matching those, not a one-unit item.

NEW: MetroidPrime/Cameras/CPathCamera | match | MetroidPrime/Cameras/CPathCamera | UpdateOrientation/Think/
GetScanObjectIndicatorPosition/Reset are unported stubs (4464 of 7616 code bytes, 0.4-6.2% each);
the unit cannot flip until they are written, and they are four separate decompilations.

## Gates run before finishing

`decomp_build.sh` (0 FAILED, `main.dol` = `6ef9b491...`), `check_symbol_names.py` (0 missing),
`check_decl_order.py` (clean), the 28465-function regression sweep (0 regressions), and
`flip_test.sh` (FAIL, reverted — recorded above rather than glossed). No `asm` was added, no
`tools/`, `docs/`, `build/goal/` (other than this notes file) or `configure.py` was touched, and
nothing was committed.
