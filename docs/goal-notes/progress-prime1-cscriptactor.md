# progress-prime1-cscriptactor

Target: `MetroidPrime/ScriptObjects/CScriptActor` (`progress`; remains `NonMatching`).

## Result

Measured before/after from `build/report.json` / `tools/fast_try.sh`:

| Function | Before | After | Prime 1 source and result |
|---|---:|---:|---|
| `GetSortingBounds__12CScriptActorCFRC13CStateManager` | 92.12% | 92.12% | The existing Echoes source already has Prime 1's body. An early-return spelling scored 85.45%, so it was reverted. Remaining difference is instruction scheduling/order. |
| `GetTouchBounds__12CScriptActorCFv` | 50.74% | 60.11% | Adapted Prime 1's positive active/material guard to Echoes' `kMT_Unknown59`, retaining Echoes' collision-primitive bounds inclusion. It remains non-exact; remaining diff is codegen ordering. |
| `Think__12CScriptActorFfR13CStateManager` | 88.37% | **100.00%** | Adapted Prime 1's explicit scaled-translation path, including separate scaled/unscaled `MoveToOR` calls. Kept Echoes-only emitter, random animation variation, `Stop`, script-message arguments, and base `Think`. Fusing `dt + variation` and using `GetHealthInfo()` reproduced retail; the latter matches the measured vtable call (slot 60 rather than `HealthInfo()`'s slot 56). |
| `__ct__Q24rstl29optional_object<10CModelData>FRCQ24rstl29optional_object<10CModelData>` | 0.00% | 0.00% | Prime 1's implementation is the `optional_object` copy-constructor template; Echoes' header already has equivalent code. The target object has no emitted copy-constructor symbol in this source unit (`nm`), so the unchanged template did not help. Explicit-instantiation syntax was rejected by MWCC; a temporary specialization did not emit the unreferenced function and was removed. |

Target unit: **9 -> 10 / 34** matched functions. Tree: `matched 9937 -> 9938`, `linked 4896 -> 4896`; `+1` exact match, no function worsened, no assembly added.

## Verification

- `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptActor` -> 10/34; `Think` omitted from the unmatched list (100%).
- `./tools/decomp_build.sh` -> `All: 30.61% fuzzy, 22.74% matched, 11.74% linked (9938 / 28465)`.
- `python3 tools/check_symbol_names.py` -> 503 units checked, 0 missing names.
- `./tools/gate.sh` verified configure, build/hash, REL hashes, per-function diff (`9937 -> 9938`, linked unchanged), module wiring, offsets, probe and link-gap checks. Its only failure was `docs claims`: the untouched HANDOFF state block lacks the judge-derived `9938` and `8527` counts. Per the item prompt, HANDOFF was not edited; judge write mode is expected to refresh those counts.
- No `flip_test` run (progress item); no commit.

## Retry on lane L9 (2026-09-30)

Re-measured this worktree with `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptActor`: **10/34** matched, unchanged from the current head; `GetTouchBounds` 60.11%, `GetSortingBounds` 92.12%, and the `optional_object<CModelData>` copy constructor 0.00%. `Think` remains 100.00%. No source edit from the earlier run was present at this head.

- `GetSortingBounds__12CScriptActorCFRC13CStateManager`: tried local/reference/volatile-id forms, signed and unsigned integer temporaries, a conditional expression, mutable-`this` aliases, and `register` hints. `try_batch.py` kept the best result at two differing instructions; `fast_try.sh` measured `register-self` and `register-id` at the same **92.12%**. `lanediff.sh` shows only the ID `lhz` scheduled before rather than after the prologue save/move instructions. **Measured wall this run:** `WALL: GetSortingBounds__12CScriptActorCFRC13CStateManager 92.12% - remaining difference is instruction scheduling; these new declaration/type/temporary shapes did not move it.`
- `GetTouchBounds__12CScriptActorCFv`: tried split/nested/reversed guards, non-const self and material-list locals, explicit base bounds, and pointer/reference temporaries for the bounds. `fast_try.sh` measured the `mutable-self` and `material-local` variants at **60.11%**; the semantics-equivalent variants retained eight differing instructions in `try_batch.py`. The remaining diff is scheduling of the active-bit read and collision-primitive load/compare. **Measured wall this run:** `WALL: GetTouchBounds__12CScriptActorCFv 60.11% - remaining differences are instruction ordering; these guard/local/pointer shapes did not move them.`
- `__ct__Q24rstl29optional_object<10CModelData>FRCQ24rstl29optional_object<10CModelData>`: tried a new out-of-line helper that returns a copy of an `optional_object<CModelData>`. It emitted only the helper (`nm`), not the target copy-constructor symbol; `fast_try.sh` stayed at 10/34 and the target at 0.00%. Removed the helper. No new conclusion beyond confirming this emission route does not make the target visible.


Representative exact diagnostics: `python3 tools/try_batch.py src/MetroidPrime/ScriptObjects/CScriptActor.cpp MetroidPrime/ScriptObjects/CScriptActor GetSortingBounds__12CScriptActorCFRC13CStateManager .tmp/opencode/sorting_variants4.py` -> `register-self 2 differing instrs`, `register-id 2`, `best: register-self (2 differing instrs)`; `python3 tools/try_batch.py src/MetroidPrime/ScriptObjects/CScriptActor.cpp MetroidPrime/ScriptObjects/CScriptActor GetTouchBounds__12CScriptActorCFv .tmp/opencode/touch_variants.py` -> `mutable-self 8 differing instrs`, `material-local 8`, `best: mutable-self (8 differing instrs)`. `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptActor` reported `10/34`, `GetSortingBounds 92.12%`, and `GetTouchBounds 60.11%` on the baseline and the listed variants. `build/binutils/powerpc-eabi-nm build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptActor.o | grep -E 'optional_object.*CModelData|CopyOptional'` after the helper trial printed only `CopyOptionalModelDataForEmission`.

`python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/CScriptActor` did not list `CScriptActor` as permuted (it did list the similarly prefixed `CScriptActorRotate`). `sha1sum build/G2ME01/main.dol` remained `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. No source change was retained, so the target count did not rise and `goal_check.sh` was not run; no NEW item found.

## Run 2026-10-02 (lane 9)

NEW: CScriptActor 13 -> 14 / 34 functions (matched 13050 -> 13051). goal_check PASS (gate.sh, decl-order, symbol names, no asm).

Per function (before% -> after%; Prime 1 source did not match for either, written from our own SLdr/LoadPickup pattern):
- `__ct__9SLdrActorFv` (weak, 0x8006F59C): 0% (unnamed `fn_8006F59C`) / 78.65% -> **100%**. SLdrActor.hpp ctor was missing three defaults: `editorProperties.unknown_0x5d298a43 = 3`, `actorInformation.lighting.ambientColor = CColor(1,1,1,1)`, `actorInformation.visor.visorFlags = 0xf`.
- `LoadActor` (0x8006EC28): 0% (retail symbol was `fn_8006EC28`, renamed in symbols.txt) -> 96.08%. Remaining diffs: bounds-branch areaId temp copy (stack 64/68), r27-r31 register order.
- `optional_object<CModelData>` copy ctor + `fn_8006F554`/`fn_8006F574` (construct/construct_impl): still 0%. Compiler inlines the chain into LoadActor; retail calls it out of line.

Declaration order: retail has CheckActorRenderOnly (0x8006EB98) below LoadActor, so LoadActor is declared before it (check_decl_order ok).

Measured (this run), copy-ctor chain not emitted out of line:
- WALL: `inline_depth(3)` file-wide: LoadActor 96.70%, ctor emitted at 87.5%, but other functions collapse (~10% matched) - unusable.
- WALL: `inline_max_total_size` thresholds change what is cut, but never retail's pattern (dtors inlined, copy ctor not).
- WALL: moving the optional_object copy ctor to an out-of-class `inline` definition had no effect (reverted).
- Untried: direct-init `modelData(LdrToModelData(...))`; forward-declared `construct`; fixing areaId temp / local order for r27-r31; naming fn_8006F554/574 in symbols.txt (only worthwhile once the ctor emits).
