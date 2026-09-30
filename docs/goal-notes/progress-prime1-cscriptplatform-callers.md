# progress-prime1-cscriptplatform-callers

`MetroidPrime/ScriptObjects/CScriptPlatform` — progress item; the unit remains `NonMatching`.

## Result

The unit rose from **29/60 to 30/60 matched functions**; tree matched count rose **10313 -> 10314**, linked stayed **5048**. The judge reported `+1` target function and no regressions.

`BuildSlaveList` (0x800A2934, 468 B) is now **100.00%**. It reserves static-slave storage, resolves `PLAY/ACTV` connections into actors and their relative translations, and records `IBND/ACTV` trigger IDs. Retail's existing `fn_800A46F0`/`fn_800A14DC` helpers are used.

`AddRider(vector)` (0x800A38D0, 660 B) is implemented with duplicate lookup/timer update, rider transform and `XONP` notification, and reserve/push. It measures **97.73%**, so it does not yet count. Remaining differences are compiler output: iterator register allocation, one FP instruction scheduling/store order, and message/optional-timer register allocation. Variants measured this run: the `rideePos` nested transform scored 92.71%; direct transform temporary scored 96.43%; adding the explicit second ridee-null check scored 97.73% (kept). A hand-written search loop scored 92.94% and was discarded.

## Verification

- `./tools/goal_check.sh build/goal/item.json` -> PASS: matched **10313 -> 10314**, linked **5048 -> 5048**, target **29 -> 30/60**, no asm added; gate, symbol-name check, and All count all passed.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform` -> no functions out of retail order.
- `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptPlatform` measured `BuildSlaveList` 100.00% and `AddRider(vector)` 97.73%.
