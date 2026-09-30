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

---

# Run 2 (lane 2, 2026-09-30) - the queued reason is stale; raised the unit another function

## Result

`BuildNearListFromRiders__15CScriptPlatformFR13CStateManagerRCQ24rstl43vector<7SRiders,...>` went
from **98.0137%** to **100.00%**. The unit rose **33/60 -> 34/60** matched functions
(`matched_code` 4496/18000 -> 4788/18000, unit fuzzy 36.44889 -> 36.53000). Tree `matched`
11290 -> 11291 of 28465; `linked` unchanged at 5507. `goal_check.sh` **PASS**.

## The queued reason is already resolved - do not re-do the dtor work

`__dt__15CScriptPlatformFv` measures **100.00%** on this tree's clean HEAD, and the fix is
commit `20d2f0fe "progress: progress-prime1-cscriptplatform-dtor"`, which
`git merge-base --is-ancestor 20d2f0fe HEAD` confirms is **already an ancestor of `goal/lane-2`**.
The judge baseline `build/goal/judge/report.base.json` was also recorded at 33/60, i.e. with the
dtor already matched, so this item's blocker is gone and the remaining way to pass was to raise
some *other* function to 100%. Re-derived `mInitialTransform`/member order: not needed, the
previous run's conclusion (mInitialTransform at 0x45c, dtor at 100%) holds.

## What changed (all in `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`)

1. `BuildNearListFromRiders`: **deleted the hoisted `end` local** and wrote `it != riders.end()`
   in the loop condition. That single deletion is the whole match.
2. `AddSlave` / `AddRider`: `x->mDecayTimer = decayTimer` -> `(*x).mDecayTimer = decayTimer`.

## Measured spellings (this run) - skip these

`BuildNearListFromRiders` (all 73-73 instructions, register assignment only):

| spelling | score |
|---|---|
| `const_iterator end = riders.end();` hoisted, `it != end` | 98.0137% |
| **`it != riders.end()` inline, no `end` local** | **100.00%** |
| hoisted `end`, rewritten as `while (it != end)` | 98.0137% |
| `end` declared *before* `TNearList result` | 92.19178% |

Retail's assignment is r27=sret, r28=mgr, r29=result, r30=cursor, r31=end. Hoisting `end` adds a
live range and rotates the whole allocation by one; removing it lands on retail's.

`AddSlave` (107/107 instructions) and `AddRider` (165/165 instructions) - same trick, partial:

| spelling | AddSlave | AddRider |
|---|---|---|
| `it->mDecayTimer = decayTimer` | 98.83178% | 98.94546% |
| **`(*it).mDecayTimer = decayTimer`** | **99.85981%** | **99.61212%** |
| `rstl::optional_object<float>& t = slave->mDecayTimer; t = decayTimer;` | 98.83178% | - |
| `rstl::vector<SRiders>::iterator& r = it; (*r).mDecayTimer = ...` | - | 99.61212% |
| `slave->mDecayTimer.operator=(decayTimer)` | 98.83178% | - |
| `rstl::optional_object<float>* t = &(*slave).mDecayTimer; *t = decayTimer;` | 99.85981% | - |
| `static_cast<const rstl::optional_object<float>&>(decayTimer)` | 99.85981% | - |
| inverted `if (slave != end) {...} else {body}` | 61.49533% | - |
| explicit `rstl::find(mItems, mItems + mCount, ...)` | 90.92523% | - |

## What still blocks the two 99.x functions (not walls - no run has exhausted them)

Both are now **instruction-count identical** to retail; only register/displacement choices differ.

- **AddSlave 99.85981%** - 5 instructions. Retail keeps the `SRiders` base in `r3` and computes
  the member address into a scratch: `addi r0,r3,4 ; cmplw r0,r31`, then `lbz 8(r3)` /
  `stfs f0,4(r3)` / `stb 8(r3)`. We fold the `+4` into `r3` instead: `addi r3,r3,4 ;
  cmplw r3,r31`, then `lbz 4(r3)` / `stfs f0,0(r3)` / `stb 4(r3)`. The layout is the same
  (`mUid` 4 bytes at 0, `mDecayTimer` value at +4, flag at +8); only which register holds
  `&mDecayTimer` differs, i.e. whether `rstl::optional_object<float>::operator=`'s self-check
  address is materialised separately from the member base.
- **AddRider 99.61212%** - only the two `CScriptMsg` temporary blocks differ. Retail assigns
  r7 = `actor->GetUniqueId()`, r8 = the `kInvalidUniqueId` literal and stores 28 before 24
  (and 8 before 12); we swap r7/r8 and store 24 before 28. Same values, same order of `lhz`
  loads - purely which temp gets which register.

## New facts for whoever picks this unit up

- Ten retail functions score `None` because **our object does not define them at all**:
  `fn_800A4038`, `fn_800A4090`, `fn_800A4654`, `fn_800A469C`, `fn_800A1CA0`, `fn_800A1CE8`,
  `fn_800A1D4C`, `fn_800A31A0`, `fn_800A359C`, `fn_800A4840`. They are in the unit's retail
  range but carry no name in `config/G2ME01/symbols.txt`.
- **`__dt__Q24rstl25single_ptr<11CMayaSpline>Fv` in our object is byte-identical to retail's
  `fn_800A4090`** (all 88 bytes; same `__dt__11CMayaSplineFv` + `Free__7CMemoryFPCv` calls) -
  only the symbol name differs, which is why objdiff cannot pair them. Same story for
  `__dt__single_ptr<SPlatformMotionSpline>`. `fn_800A4038` is the same shape but calls
  `__dt__15CGameSplineDescFv`, so its member type would have to be `CGameSplineDesc`, not the
  `CGameSpline` the header currently uses. Writing these out as `extern "C"` (the trick already
  used in this file for `fn_800A47A8`, `fn_800A46F0`, `fn_800A14DC`) is the obvious route to
  +2 or +3; note mwcceppc will *not* inline `~single_ptr()` (`-inline deferred,noauto`, see the
  comment in `include/rstl/single_ptr.hpp`), so the body cannot simply call the template.
- `fn_800A4840` is `UserNames::IsUser(int)` - `src/MetroidPrime/CGameCollision.cpp` already
  declares it `extern "C"` and calls it 8 times, and its comment says the same 0x10-byte body is
  "the last function of `MetroidPrime/ScriptObjects/CScriptPlatform.cpp`'s retail range and no
  unit in this tree emits it". It cannot be scored from this unit without inventing a call site,
  so leave it to `CGameCollision`.

## Verification

```text
./tools/decomp_build.sh
  All: 32.46% fuzzy, 25.14% matched, 11.94% linked (11291 / 28465 functions)
  main/MetroidPrime/ScriptObjects/CScriptPlatform: 36.53% fuzzy, 26.60% matched (34 / 60 functions)
  DOL: 9743 / 16726 matched functions (baseline 9742)
./tools/goal_check.sh build/goal/item.json
  goal_check: PASS progress-prime1-cscriptplatform-dtor
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 11290 -> 11291   linked 5507 -> 5507
  ok  check_symbol_names.py
  ok  target rose: main/MetroidPrime/ScriptObjects/CScriptPlatform: 33 -> 34 / 60 functions
  ok  no asm added
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
  ok: 1 unit(s) checked, none emits its functions out of retail order
git diff --check
  clean
```

As in run 1, the gate's generated `docs/HANDOFF.md` state-block refresh was reverted per the item
instructions (the driver regenerates those derived counts, and `check_docs_claims.py` therefore
reports the stale `9743` claim until the driver rewrites the block). Only
`src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` is modified; no header change, so no other
unit's `.text` can have moved. No commit made.

## Codegen rule worth keeping (general, not GameCube-specific)

When a loop's *only* difference from retail is a wholesale rotation of callee-saved registers,
suspect a live range the source does not need. A hoisted `end = c.end()` local that the loop
condition could have read inline adds one and rotated all five registers; deleting it reproduced
retail exactly. Conversely `it->member = x` vs `(*it).member = x` changed register liveness in
`optional_object::operator=` and moved two more functions 1-1.5 points up. Try the *smaller*
spelling before touching types or layout.
