# cplayer-fn80012D10

`kind: progress`, target `MetroidPrime/Player/CPlayer`. One file changed:
`src/MetroidPrime/Player/CPlayer.cpp` (48 lines added, lines 598-645). No `tools/`, no
`config/`, no `build/goal/` edit beyond this notes file. `docs/HANDOFF.md` shows as modified
because `tools/gate.sh` runs with `MP_GATE_DOCS_WRITE=1` and rewrites the state block itself.

## Result, measured

`fn_80012D10` (260 B, `.text 0x80012D10`) went from **not in the source at all** to an exact
match. `build/report.json`, `main/MetroidPrime/Player/CPlayer`:

| | before | after |
| --- | --- | --- |
| matched functions | 61 / 228 | **62 / 228** |
| matched code | 2444 / 71652 (3.4109%) | **2704 / 71652 (3.7738%)** |

Whole build `All: 34.50% fuzzy, 27.83% matched, 12.89% linked (12220 / 28465 functions)`;
`matched 12219 -> 12220`, `linked 5860 -> 5860`.

`./tools/goal_check.sh build/goal/item.json` in the worktree: **PASS**

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12219 -> 12220   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.50% fuzzy, 27.83% matched, 12.89% linked (12220 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayer: 61 -> 62 / 228 functions
  ok    no asm added
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
`tools/unit_fit.sh MetroidPrime/Player/CPlayer.cpp` is unchanged by this edit (the 20 extra
weak/template functions it lists are pre-existing COMDAT copies; `fn_80012D10` is in both
objects). `python3 tools/check_decl_order.py --unit Player/CPlayer` does not report the unit,
so it is still emitted in retail order - the definition sits between `fn_80012e14`
(0x80012E14) and `AttachActorToPlayer` (0x80012CA0), which is where the descending-by-offset
rule puts it.

## What the function is

Prime 1's decomp has this body as `CPlayer::CalculateLeftStickEdgePosition`; Echoes' DOL
carries no symbol at 0x80012D10, so `config/G2ME01/symbols.txt` has dtk's `fn_<addr>`
placeholder and the definition is `extern "C"` - a C++ one would mangle and objdiff would pair
nothing (the same reason as `fn_8001935C` and `fn_80010F48` in this unit). It is a CPlayer
member in spirit: its only caller, `CPlayer::SidewaysDashAllowed` at 0x801897D8, sets
`addi r3,r1,8` (the caller's return slot) / `mr r4,r30` (`this`) / `fmr f1,f31` /
`fmr f2,f30`, so the first pointer argument is the player and is never read.

The body is Prime 1's, and it is the C++ that produces retail's bytes:

```cpp
float f31 = -1.f, f30 = -0.555f, f29 = 0.555f;
if (strafeInput >= 0.f) { f31 = -f31; f30 = -f30; }
if (forwardInput < 0.f) { f29 = -f29; }
const float f1 = static_cast<float>(atan(fabsf(forwardInput) / fabsf(strafeInput)));
const float f4 = CMath::Limit(f1 / (M_PIF / 4.f), 1.f);
```

* Retail calls the DOL's **double** `atan` (0x80352338) directly, so `CMath::ArcTangentR` must
  not be used: it is declared in `include/Kyoto/Math/CMath.hpp:93` and **not defined anywhere
  in this tree** (Prime 1 defines it in `src/Kyoto/Math/RMathUtils.cpp`), so a call to it would
  be an undefined symbol. `atan` is already used by other port units (`CEulerAngles.cpp`,
  `CMorphBall.cpp`) and the gate's link-gap check stayed clean.
* The two `frsp`s before the call are the float-to-double conversion of the ratio; the `frsp`
  after it is the double-to-float of the result. Nothing to write - it is what the cast
  produces.
* The SDA2 constants, read out of `.sdata2` (base 0x804223C0): 0.0f 0x8041A480, -1.0f
  0x8041A4A0, -0.555f 0x8041A55C, 0.555f 0x8041A560, pi/4 0x8041A564, 1.0f 0x8041A478.
  `0.555f`, `M_PIF / 4.f` and `CMath::Limit` all hit those, and the `fsel` at 0x80012DB0 is
  `Limit`'s `h * Sign(v)`.
* `cror eq,gt,eq` / `bne` at 0x80012D50 is the `>=` on `strafeInput` compiled inverted, which
  is why the test is written as `strafeInput >= 0.f` and not `strafeInput < 0.f`.

## The one instruction that took work, and the spellings measured

Everything else matched on the first try. The tail differed in exactly one instruction, and
the reason is that retail's compiler folds something mwcceppc does not:

```
retail 80012dc8  fmuls f0,f4,f2      f2 holds 0.0f, so the edge's z IS the constant
ours             fsubs f0,f2,f2      0.0f - 0.0f, emitted
```

Every spelling below was built with `tools/fast_try.sh MetroidPrime/Player/CPlayer` and scored
from `build/report.json`; the tail is the same 9 instructions in each case.

| spelling | score |
| --- | --- |
| `CVector3f(f30, f29, 0.f) - CVector3f(f31, 0.f, 0.f)` (Prime 1 verbatim) | 94.231% |
| `CVector3f(f30, f29, 0.f) - CVector3f(f31, 0.f, 0.f)` with both as named locals | 94.231% |
| the same with the base's z taken from `base.GetZ()` | 94.231% |
| `CVector3f(f30 - f31, f29 - 0.f, 0.f)` spelled component-wise | 94.308% (the y folds) |
| `CVector3f(f30 - f31, f29 - 0.f, 0.f)` through `SetX`/`SetY` | 94.308% |
| `ByElementMultiply` with the arguments swapped | 94.000% |
| `CVector3f::ByElementMultiply(...) + base` instead of `base + ...` | 94.077% |
| `CVector3f::ByElementMultiply(CVector3f(f4,f4,f4), scaled) - scaled base` | 91.231% |
| `base += ...; return base;` | 88.769% |
| `out.SetX/SetY/SetZ(...)` spelled out by hand | 89.692% |
| the subtract, then `edge.SetZ(0.f)` | **100.000%** |

The two behaviours are mutually exclusive in a single expression, which is why the fix is a
statement rather than an expression: inside `operator-`, mwcceppc leaves `0.f - 0.f` alone
(so the z stays a `fsubs`), and in a plain expression tree it folds `f29 - 0.f` to `f29`
(so the y loses its `fsubs`). Retail has neither fold. Writing the difference through the
vector subtraction and then pinning the z with `SetZ` gives the edge's z as the 0.0f constant
that `fmuls f0,f4,f2` multiplies, and keeps the y's subtraction inside the vector op where
mwcceppc leaves it. The code says so in a comment at the call site.

`edge.SetZ(0.f)` is redundant as arithmetic (the difference's z is already 0) and is only
there for codegen; it is the same trade the repo makes elsewhere (see
`docs/goal-notes/progress-unit-cplayer.md` on `GetDeathAlpha` and `SetHudDisable`).

## NEW:

None filed. The function landed, the unit is at 62/228 and cannot flip in one item, and the
two things still blocking other functions in it are already queued from
`docs/goal-notes/progress-unit-cplayer.md` (`port-files-cmake-first-person-camera` and
`progress-unit-cplayergun-bits`). Nothing new was measured about them in this run.
