# progress-prime1-cworldtransmanager

Baseline/current measurements are from `build/report.base.json` and `build/report.json` in lane L9. `CWorldTransManager` rose from **15/76 to 18/76 matched functions**. The full report diff is `matched 9878 -> 9881`, `linked 4895 -> 4895`, **3 new 100% functions, no regressions**.

| Function | Before → after | Prime 1 source result |
| --- | ---: | --- |
| `DrawDisabled` | 4.76% → 100% | Copied unchanged; exact. |
| `__ct` | 83.96% → 83.96% | Read; Echoes initializes additional fields, so kept its existing initializer list. |
| `DisableTransition` | 87.35% → 100% | Adapted for Echoes subtitle, dark-world and portal state. Assigning an empty `optional_object` (rather than calling `.clear()`) reproduces retail's copy-assignment/temporary-dtor pattern. |
| `TouchModels` | 4.86% → 70.62% | Adapted model touch/load behavior to Echoes' beam/grapple data and portal path; did not import Prime's Fusion-suit index mapping. Partial. |
| `EndTransition` | 72.38% → 72.38% | Prime only disables; retained Echoes' character-factory clear. |
| `Update` | 99.46% → 99.46% | Prime lacks Echoes' portal branch; unchanged. |
| `UpdateEnabled` | 0.39% → 69.79% | Adapted Prime's animation, shake, dissolve and shaft-offset logic to local fields. Partial. |
| `Draw` | 99.40% → 99.40% | Prime lacks Echoes' portal branch; unchanged. |
| `UpdateLights` | 0.53% → 56.24% | Adapted Prime's spot-light logic; Echoes' long-shaft/additional-light behavior remains different. Partial. |
| `DrawAllModels` | 0.32% → 68.12% | Adapted Prime's actor-light/model rendering to local model types. Partial. |
| `DrawFirstPass` | 2.38% → 2.38% | Tried Prime body: 0%; reverted because it regressed. No help. |
| `DrawSecondPass` | 2.38% → 2.38% | Tried Prime body: 0%; reverted because it regressed. No help. |
| `DrawEnabled` | 1.39% → 1.39% | Not ported: Prime requires `mDissolveTextureBuffer`, absent from Echoes' nested data, and has a substantially different render path. |
| `DrawText` | 0.70% → 68.39% | Adapted Prime's text rendering/filter code to Echoes' renderer. Partial; subtitles remain. |
| `UpdateText` | 0.15% → 0.15% | Tried Prime body, but current `CTweakGui` has no `GetWorldTransManagerCharsPerSfx`; compile stopped there, so reverted. No help. |
| `WaitForModelsAndTextures` | 0.72% → 100% | Adapted unchanged except replacing Prime's unavailable `AUTO` macro with a typed local vector iterator; exact. |

Verification: `./tools/fast_try.sh MetroidPrime/CWorldTransManager`; `python3 tools/report_diff.py build/report.base.json build/report.json` reported no regression; `./tools/gate.sh` passed build/hash, report diff, module wiring, DOL read, GS/raw offsets, declaration order, files/module order, port probe/link gap, duplicates and reach-stub checks. It reported **only** stale derived counts in `docs/HANDOFF.md` (`matched 9881` and `DOL 8470`); left docs untouched per the goal prompt for the judge to rederive. No config changes, assembly, or commit.
