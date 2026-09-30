# progress-prime1-canimtreetweenbase — 2026-09-30

Baseline `main/Kyoto/Animation/CAnimTreeTweenBase` was 10/20. Per-function measurements (before → after):

| Function | Score | Prime 1 transfer |
| --- | --- | --- |
| `CopyNodeMinusStartTime__12CBoolPOINodeFRC12CBoolPOINodeRC13CCharAnimTime` | 0.00% → 100.00% | Small adaptation: Echoes stores a name hash (`GetNameHash`) rather than Prime 1's string (`GetString`). The body was moved from `CAnimSourceReaderBase.cpp` to its retail unit; host-only definition remains under `TARGET_PC`. |
| `VGetOffset__18CAnimTreeTweenBaseCFRC6CSegId` | 68.96% → 100.00% | Prime 1 behavior adapted to local `CVector3f::Lerp`; using its unsuffixed `1.0` matched the retail double threshold. |
| `VGetRotation__18CAnimTreeTweenBaseCFRC6CSegId` | 19.81% → 100.00% | Small API adaptation to local `CQuaternion::SlerpLocal`, plus unsuffixed `1.0`. |
| `VGetSegStatementSet__18CAnimTreeTweenBaseCFRC10CSegIdListR16CSegStatementSetRC13CCharAnimTime` | 32.53% → 32.53% | Not adapted: Prime 1 uses `CStackSegStatementSet`/`GetData`, absent from Echoes headers; Echoes factors this through its optional-time `BlendSegStatementSet` helper. Needs local support/API reconstruction. |
| `VSimplified__18CAnimTreeTweenBaseFv` | 0.69% → 99.46% | Prime 1 logic adapted to local APIs. Remaining `lanediff` differences: boolean tests (`clrlwi.` vs `cmplwi`) and local `@stringBase0` vs retail `lbl_803AEE08` relocations. |

Verified: `./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS progress-prime1-canimtreetweenbase`; matched 10069 → 10072, linked 4918 unchanged, target 10 → 13 / 20, no regression, no asm. `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` → `no regression`. Declaration-order check: 1 unit checked, none out of retail order; symbol-name check: 504 units, 0 missing.

The unit remains `NonMatching`; no flip was attempted, as this is a progress item. No new queue item or `WALL:` filed.
