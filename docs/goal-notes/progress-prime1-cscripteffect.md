# progress-prime1-cscripteffect

Target: `main/MetroidPrime/ScriptObjects/CScriptEffect` (35 functions).

Measured on the queued baseline after `./tools/gate.sh`: 8/35 matched; global matched 9939, linked 4896. Re-measured each listed function against `build/report.json`:

| Function | Before | After | Prime 1 adaptation / result |
| --- | ---: | ---: | --- |
| `SetActive__13CScriptEffectFb` | 93.333336% | 100.000000% | Same body; adding Prime 1's top-level `const` to the by-value parameter matches. |
| `Think__13CScriptEffectFfR13CStateManager` | 88.806170% | 97.519820% | Needed Echoes-specific particle-system handling; Prime 1-shaped nested timer checks, separate timeout/deletable branches and explicit white-color branch improved it, but did not reach 100%. |
| `Render__13CScriptEffectCFRC13CStateManager` | 89.833336% | 89.833336% | Tried Prime 1's `.get()` pointer-check idiom against Echoes' single polymorphic particle system; no measured change. Prime 1's separate electric-system body does not transfer unchanged. |
| `GetSortingBounds__13CScriptEffectCFRC13CStateManager` | 92.121216% | 92.121216% | Prime 1's direct `GetRenderBoundsCached()` fallback was tried and scored 58.52%; reverted. Kept Echoes' `CActor::GetSortingBounds(mgr)` fallback. |

The target rose 8 -> 9 matched functions; global matched rose 9939 -> 9940, linked stayed 4896. `./tools/goal_check.sh build/goal/item.json` passed: full gate (DOL and all 86 REL hashes, report diff, docs/wiring/probes), symbol check, no asm, and strict target increase. No flip was run; this is a `progress` item.
