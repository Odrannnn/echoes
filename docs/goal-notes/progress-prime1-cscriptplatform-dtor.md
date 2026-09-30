# progress-prime1-cscriptplatform-dtor

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item; unit remains `NonMatching`.

## Result

`__dt__15CScriptPlatformFv` improved from **77.78%** to **100.00%**. The unit rose from **28/60**
to **29/60** matched functions; the tree rose from **10283/28465** to **10284/28465**, with linked
unchanged at **5043**. `goal_check.sh` confirmed no regressions anywhere and no assembly added.

## What changed and what was measured

- Changed `mSplineController` at `0x42c` and `mWaypointTracker` at `0x440` from raw pointers to
  `rstl::single_ptr` owners. `CGameSpline` already has a virtual destructor; the still-opaque
  `CPlatformWaypointTracker` declaration now records the virtual destructor confirmed by retail's
  deleting call. Adjusted the two spline null checks to use `.null()`.
- Retail's destructor destroys the three Maya-spline owners at `0x444`, `0x448`, `0x44c`, then
  virtual-deletes the owners at `0x440` and `0x42c`, then destroys the `0x428` member. The new
  member types emit that exact sequence.
- The queued reason's proposed `mInitialTransform` offset `0x44c` is contradicted by the constructor:
  it zeroes pointer slots `0x444/0x448/0x44c`, initializes three IDs and `xrayAlpha` at
  `0x450..0x458`, and calls `CTransform4f`'s constructor at `0x45c`; bitfields are written at
  `0x48c/0x48d`. Thus `mInitialTransform` remains at `0x45c`; no layout reorder was needed.

## Verification

```text
./tools/decomp_build.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp
  before: 28/60; destructor 77.78%
  after: 29/60; destructor 100.00%; 3324/18000 matched code
./tools/goal_check.sh build/goal/item.json
  PASS; matched 10283 -> 10284; linked 5043 -> 5043; target 28 -> 29 / 60; no asm added
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
  ok: 1 unit(s) checked, none emits its functions out of retail order
git diff --check
  clean
```

The goal gate also passed DOL/REL hashes, report diff, module wiring, docs claims, port probe and
symbol-name checks. The gate's generated HANDOFF state-block refresh was reverted per the item
instructions; the driver regenerates those derived counts. No commit made.
