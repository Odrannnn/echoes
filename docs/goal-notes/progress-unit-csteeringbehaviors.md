# progress-unit-csteeringbehaviors

Lane 7, `goal/lane-7`, worktree `../wt-mp2-goal-L7`. Item: raise `MetroidPrime/CSteeringBehaviors`'s
`matched_functions` in `build/report.json`; the unit stays `NonMatching`.

## Result: 9/13 -> 12/13 functions matched (three functions taken to 100%)

`./tools/goal_check.sh build/goal/item.json` -> **PASS**, `matched 12284 -> 12287`, `linked 5863 ->
5863` (unchanged), target `9 -> 12 / 13`, no asm added.

## What I changed

Only `src/MetroidPrime/CSteeringBehaviors.cpp`. Two changes, both measured one at a time.

### 1. `DropZ()` -> `ToVec2f()` at every `radial` computation (5 sites, 3 functions)

`(position - orbitPoint).DropZ()` -> `(position - orbitPoint).ToVec2f()` wrapped in the
`CVector3f(const CVector2f&, float)` constructor. This is the Prime 1 donor's own spelling
(`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/CSteeringBehaviors.cpp:343`,
`:365`, `:397`, `:424`, `:450`, `:462`), adapted only in that this repo's `CVector3f::DropZ()` returns
a `CVector3f` (`include/Kyoto/Math/CVector3f.hpp:73`) while Prime 1's returns a `CVector2f`.

Why it matters, from the retail bytes: retail calls `__ct__9CVector2fFff` (a `CVector2f(float,float)`
constructor) between the subtraction and the `CanBeNormalized` test, i.e. the source really did build a
`CVector2f` and then re-widened it with `z = 0`. `DropZ()` here inlines straight to
`CVector3f(mX, mY, 0.f)` and emits no call, so our object was structurally short two instructions
and used a different stack frame size. Measured per function:

| function | before | after |
|---|---|---|
| `ProjectOrbitalPosition` (832 B) | 89.81% | **100%** |
| `ProjectOrbitalIntersection` 7-arg (1128 B) | 92.41% | **100%** |
| `ProjectOrbitalIntersection` 8-arg (1180 B) | 93.62% | **100%** |

### 2. `FLT_MAX` spelled as a literal (file top)

The repo's established workaround, already used at
`src/MetroidPrime/ScriptObjects/CScriptTeamAiMgr.cpp:17` and
`src/MetroidPrime/PathFinding/CPathFindArea.cpp:21`: libc's `FLT_MAX` is `(*(float*)__float_max)`, so
mwcceppc emits `lis r4, __float_max@ha` / `addi r5, ...` / `lfs f17, 0(r5)` (three instructions)
where retail does one in-place `lfs f17, lbl_8041B984@sda21` (`0x7f7fffff`, the DOL's shared
`.sdata2` at `0x8041B984`). Retail's `lbl_8041B984` value and the literal agree; the relocation
differs but objdiff scores the loaded value, and the extra two instructions plus a 0x10 stack-frame
shift were the whole remaining gap in both `ProjectOrbitalIntersection` overloads. Measured:
92.41% -> 99.04% and 93.62% -> 99.08% after change 1 alone, then both -> **100%** after change 2.
Verified against the retail relocations: `objdump -r build/G2ME01/obj/MetroidPrime/CSteeringBehaviors.o`
lists `lbl_8041B984` at offsets `0x4cc`, `0x960` and `lbl_8041B988` (`1e-5f`) at `0x548`, `0x9e4` -
these are the two constants this file loads.

## What is still open: the 6-arg `ProjectLinearIntersection` (464 B, 1.21%)

Unchanged by this run and still the unit's only unmatched function. Its body is *not* the loop it
looks like in the current source: retail at `0x800F9D04` builds five coefficients on the stack
(offsets `0x18`..`0x28`), calls `fn_802CB918` with `r3 = sp+0x18` (the coefficients) and
`r4 = sp+0x8` (a 4-float roots buffer), then iterates `roots` with `mtctr`/`bdnz` reading through
`0(r4)`, comparing each root against `lbl_8041B980` (`0.0f`), and writing
`intersection = position + velocity*t + 0.5f*t*t*acceleration`. The coefficient values it builds
are the quartic of a constant-acceleration intercept, matching Prime 1's
`ProjectLinearIntersection` 6-arg body at prime-ref lines 305-334. So the current
`return false` stub is wrong in shape and the donor gives the logic - but the solver it needs is a
**different unit's** function:

    0x802CB918 is inside `Kyoto/Math/RMathUtils.cpp`'s claimed `.text`
    (config/G2ME01/splits.txt:2204, 0x802CB330..0x802CDF8C), which is `NonMatching` at 29/39 and
    currently defines no quartic/cubic solver (src/Kyoto/Math/RMathUtils.cpp has SolveQuadratic at
    :408 and nothing else of the kind).

I could not fill this in without inventing that solver here, which would put a symbol in our object
that the retail object does not define - `tools/unit_fit.sh` reports on exactly that, and the
reviewer rejects the shape. It stays a stub, deliberately: it is honest (it does not do work it
cannot), and the surrounding three functions are the real gain.

`./tools/unit_fit.sh MetroidPrime/CSteeringBehaviors.cpp`:

    .text      claimed 5780   ours 5324   retail 5780   SHORT by 456
    .sdata     claimed   -    ours    40   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
    .sdata2    claimed   -    ours    48   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
    no extra functions: our object defines only what the retail unit object does

`python3 tools/check_decl_order.py --unit MetroidPrime/CSteeringBehaviors` -> `none emits its
functions out of retail order`.

## Verified

- `./tools/decomp_build.sh MetroidPrime/CSteeringBehaviors` -> unit `12/13`, `92.07%` fuzzy,
  `91.97%` matched code; `All: 34.69% fuzzy, 28.14% matched, 12.90% linked (12287 / 28465)`.
- `./tools/goal_check.sh build/goal/item.json` -> PASS (gate.sh incl. DOL sha1, 86 RELs, report diff,
  module wiring, docs claims, port probe; plus counts, symbol names, no asm).

I did not run `flip_test.sh`: this is a `progress` item and the unit is staying `NonMatching`.

## NEW

NEW: progress-quartic-solver-rmathutils | progress | Kyoto/Math/RMathUtils | fn_802CB918 (the shared quartic/cubic solver at 0x802CB918, inside RMathUtils.cpp's claim) is unwritten; CSteeringBehaviors' 6-arg ProjectLinearIntersection cannot reach 100% until it exists