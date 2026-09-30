# progress-prime1-canimtreeblend

Implemented Prime 1 logic in the Echoes `CAnimTreeBlend` methods, adapting only to local APIs and MWCC codegen. Baseline was re-measured after `./tools/gate.sh`: unit `5/10` matched; global `10061` matched, `4918` linked.

| Function | Before | After | Prime 1 donor result |
| --- | ---: | ---: | --- |
| `__dt__18CAnimTreeTimeScaleFv` | 0.00% | 0.00% | Not transplanted: Prime 1 defines this in `CAnimTreeContinuousPhaseBlend.cpp` with a non-inline declaration; Echoes has an inline destructor in `CAnimTreeTimeScale.hpp`. No destructor/header relocation attempted. |
| `VClone__14CAnimTreeBlendCFv` | 93.00971% | 100.00% | Prime 1 body with local `Clone()` wrappers matches unchanged. |
| `VGetTimeRemaining__14CAnimTreeBlendCFv` | 76.52778% | 100.00% | Prime 1 `max_val` body alone stayed at 76.52778%; replacing it with local `max_val_in_place` made it exact. |
| `VGetSteadyStateAnimInfo__14CAnimTreeBlendCFv` | 8.518292% | 100.00% | Prime 1 logic needed the same `max_val_in_place` codegen adaptation. |
| `VAdvanceView__14CAnimTreeBlendFRC13CCharAnimTime` | 9.681318% | 100.00% | Adapted `CAdvancement*` to local `SAdvancement*` and public `mDeltas`; `advance_reader` and `max_val_in_place` avoid extra copies. |

Final target: `9/10` matched (up from `5/10`); global matched `10061 -> 10065` (+4), linked stayed `4918`. `python3 tools/report_diff.py build/report.base.json build/report.json` reported exactly those four +100% functions and `no regression`. `./tools/decomp_build.sh Kyoto/Animation/CAnimTreeBlend.cpp` and `git diff --check` passed. `check_decl_order.py --unit Kyoto/Animation/CAnimTreeBlend.cpp` reported no units out of order.

`unit_fit.sh Kyoto/Animation/CAnimTreeBlend.cpp` reports `.text` 2360 vs 2008 claimed and six extra helper/weak functions (512 bytes); the unit remains `NonMatching`, as required for progress. No asm added.

`./tools/gate.sh` passed build/hash checks (DOL and all RELs), per-function diff, wiring, offsets, symbol/order/files/module checks, probe and port-link checks. Its docs-claims step alone reported the derived Handoff counts stale (`10065` matched, `8654` DOL vs the baseline figures). I did not edit the forbidden handoff; `tools/goal_check.sh` runs the gate with `MP_GATE_DOCS_WRITE=1` to derive those counts.
