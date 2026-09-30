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
