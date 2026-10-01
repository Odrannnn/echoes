# progress-unit-cscriptpathcamera

`src/MetroidPrime/ScriptObjects/CScriptPathCamera.cpp` only. No header, `configure.py`,
splits or `tools/` change; the unit stays `NonMatching` and was not flipped.

## Result, measured

`main/MetroidPrime/ScriptObjects/CScriptPathCamera` **5 / 11 -> 8 / 11** matched functions.
Whole-tree `All:` 33.61% fuzzy, 26.74% -> 26.75% matched, 11869 -> 11872 functions.
`linked` held at 5727 (nothing was promoted, nothing regressed).
`./tools/goal_check.sh build/goal/item.json` -> **`PASS`**, all seven checks `ok`.

Unit fuzzy 94.33% -> 98.34%. Three functions reached 100%, three more moved a long way:

| function | before | after |
|---|---|---|
| `GetPositionByTime__19CScriptCameraSplineFfRC12CTransform4fRC13CStateManager` | 83.04% | **100%** |
| `GetPositionByLength__19CScriptCameraSplineFfRC12CTransform4fRC13CStateManager` | 83.04% | **100%** |
| `__ct__17CScriptPathCameraF...` (the long one) | 93.42% | **100%** |
| `AcceptScriptMsg__17CScriptPathCameraFR13CStateManagerRC10CScriptMsg` | 91.04% | 97.50% |
| `GetOrientationByTime__19CScriptCameraSplineFfRC12CTransform4fRC13CStateManager` | 97.02% | 97.02% |
| `GetOrientationByLength__19CScriptCameraSplineFffRC12CTransform4fRC13CStateManager` | 97.08% | 97.08% |

Still unmatched: the two `GetOrientationBy*` at 97% and `AcceptScriptMsg` at 97.5%.
`__dt__`, `TranslateSplines`, `RotateSplines` and `__ct__19CScriptCameraSpline` were already
100% and stayed there.

## What was tried, per function

### `GetPositionByTime` / `GetPositionByLength`: 83.04% -> 100% (one edit, both)

Retail computes the result in f0/f1/f2 on every branch and has **one** store block at the end
(`stfs f0/f1/f2 -> *r28` at `.L_801E1258`). We had three separate stores, one per `return`.
Rewriting as a single local with a single return at the bottom reproduced it exactly:

```cpp
CVector3f position;
if (GetPositionKnotCount() != 0) {
  position = CGameSpline::GetPositionByTime(time);
} else if (const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(mPositionId))) {
  position = actor->GetTranslation();
} else {
  position = xf.GetTranslation();
}
return position;
```

`GetPositionByLength` is the same function with `ByLength` and needed the same edit.
The `else if` matters: two sequential `if`s with an early `return` leave the control flow
shaped differently.

### The constructor: 93.42% -> 100%

Only one line. Retail calls `CreateFor__11CMayaSplineFffff` and passes the returned `CMayaSpline`
**straight into the mSpline argument slot**. We wrapped it, so a copy constructor ran between
`CreateFor` and the `CScriptCameraSpline` ctor that retail does not have:

```cpp
-          CMayaSpline(SLdrSpline::CreateFor(0.f, 0.f, 1.f, 1.f)), positionType, lookAtType)
+          SLdrSpline::CreateFor(0.f, 0.f, 1.f, 1.f), positionType, lookAtType)
```

`SLdrSpline` is `typedef CMayaSpline` (`include/Kyoto/Math/CMayaSpline.hpp:101`), so the cast was
purely redundant. **Lesson: a `typedef`'d class name used as a constructor is a copy, not a cast.**
Check for one before concluding a ctor is "different".

### `AcceptScriptMsg`: 91.04% -> 97.50%, two edits

1. **91.04% -> 92.80%.** Retail has `lhz r0, 0x24(r1)` followed by **three** `sth` stores of the
   same value (to `0x34(r1)`, `0x20(r1)` and the member). We had one. That is a named local being
   passed to an inlined `SetTargetId`: bind the `FindConnectedObject` result to a local first.
   Same for `SetPositionId`. Kept both.
2. **92.80% -> 97.50%.** The `mTimeKeyframeId = cond ? timeKeyframe : kInvalidUniqueId;` ternary
   became an `if`/`else`. Retail branches to `.L_801E08FC`, loads `kInvalidUniqueId@sda21` there,
   stores to the member, and rejoins; the ternary made mwcceppc build the value in r3 and fall
   through, which is 3 instructions in the wrong shape. Replacing the ternary with an explicit
   `if`/`else` reproduced the branch-and-rejoin exactly.

### `GetOrientationByTime` / `GetOrientationByLength`: stuck at 97% — a wall

Both are **3 instructions** from matching, and it is the *same* 3 instructions in both:
retail stores the `target - position` delta into **two** stack slots (0x30 and 0x3c) and calls
`IsMagnitudeSafe` on the second; we store it once. Everything else is instruction-for-instruction
identical modulo stack offsets.

Spellings tried, **all measured at 97.02% / 97.08%, all reverted to the last one that is not a
regression** (the file now has `const CVector3f delta(target - position);`, which was the
innocent-looking original shape):

- `(target - position).IsMagnitudeSafe()` (original, 97.02%)
- `const CVector3f delta = target - position;`
- `CVector3f delta = target - position;`
- `CVector3f delta; delta = target - position;`
- `const CVector3f& delta = target - position;`
- `const CVector3f delta(target - position);`

mwcceppc elides the temporary every time; retail keeps both, so retail's source must hold the
delta in something with an address of its own that survives the call - most likely the check is
not on `(target - position)` directly but on a value that went through a helper. **Do not re-try
these six.** The next attempt needs a different idea, not another spelling of the subtraction.

WALL: GetOrientationByTime 97.02% - retail materialises the (target-position) delta into two stack slots (0x30 and 0x3c) and calls IsMagnitudeSafe on the second; mwcceppc elides the temporary under all six spellings tried.

## For the next run

- The two `GetOrientationBy*` are 3 instructions from a flip and are the cheapest remaining
  work in this unit. The gap is one extra 12-byte store group, not a control-flow or
  register-allocation problem.
- `AcceptScriptMsg` at 97.50% has one residual difference left in the entry test: retail
  materialises the message constant (`lis r3, 0x5841 / addi r0, r3, 0x4c44 / cmpw r29, r0`)
  where we fold it into `addis r0,r29,-22593 / cmplwi r0,0x4c44`. Spelled as
  `EScriptObjectMessage(0x58414c44)` to check it is not the enumerator's value: **97.50%, no
  change**, so the constant is right and the difference is in how the comparison is written
  (`>=`/`<` vs `==`, or the operand order). Worth one attempt.
- `docs/HANDOFF.md`'s state block was rewritten by `gate.sh` during the judge run (derived
  counts); I reverted it so the diff is the one source file only.

## Verified

```
goal_check: PASS progress-unit-cscriptpathcamera
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11869 -> 11872   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.61% fuzzy, 26.75% matched, 12.64% linked (11872 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptPathCamera: 5 -> 8 / 11 functions
  ok    no asm added
```

Not committed, per the brief.