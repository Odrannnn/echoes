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

---

# Second attempt (lane 7, 2026-10-02)

`fn_80012D10` was **already matched** on this tree - the previous run's commit
`9250f5fb progress: cplayer-fn80012D10` is an ancestor of `HEAD`, so the item's named function
is done. But `kind: progress` is judged on the **unit's** matched count rising, and the unit was
at 62/228 with 166 functions unmatched. So this run took the item as "raise
`main/MetroidPrime/Player/CPlayer` past 62" and landed **seven more** functions, all at 100%.

One file changed: `src/MetroidPrime/Player/CPlayer.cpp` (+49 / -9). No `tools/`, no `config/`,
no header edits, no `build/goal/` edit beyond this notes file. `docs/HANDOFF.md` shows as
modified because `tools/gate.sh` runs with `MP_GATE_DOCS_WRITE=1` and rewrote the state block
itself (12248 -> 12255).

## Result, measured

| | before | after |
| --- | --- | --- |
| matched functions | 62 / 228 | **69 / 228** |
| matched code | 2704 / 71652 (3.7738%) | **3468 / 71652 (4.8401%)** |

Whole build `All: 34.62% fuzzy, 27.98% matched, 12.89% linked (12255 / 28465 functions)`;
`matched 12248 -> 12255`, `linked 5860 -> 5860`.

`./tools/goal_check.sh build/goal/item.json` in the worktree: **PASS** (output quoted above).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
`python3 tools/check_symbol_names.py` -> `0 declared names are missing`.
`tools/check_decl_order.py --unit Player/CPlayer` does not name the unit, so it is still emitted
in retail order (every function below was an in-place body replacement, so no declaration moved).

Per-function diff against `build/goal/judge/report.base.json`, measured from `build/report.json`:

| function | bytes | before | after |
| --- | --- | --- | --- |
| `CanEnterMorphBallState__7CPlayerCFv` | 140 | 4.00% | 100% |
| `fn_8000bf1c__7CPlayerCFv` | 88 | 9.09% | 100% |
| `fn_8000e85c__7CPlayerFf` | 80 | 5.00% | 100% |
| `fn_8000eba0__7CPlayerFv` | 48 | 8.33% | 100% |
| `UpdateCrosshairsState__7CPlayerFRC11CFinalInput` | 72 | 5.56% | 100% |
| `UpdateVisorTransition__7CPlayerFfR13CStateManager` | 140 | 2.86% | 100% |
| `ObjectInScanningRange__7CPlayerF9TUniqueIdRC13CStateManager` | 196 | 2.86% | 100% |

## The method that worked, and the one that did not

Six of the seven are **retail-behaviour recovery from the disassembly**, not Prime 1 verbatim
transcription. The recipe, which is the reusable part:

1. `build/report.json` per unit, filter `fuzzy_match_percent < 100`, sort by `size`. The small
   ones are where the wins are: all seven here are 48-196 B, and **every one matched on the
   first or second spelling**. Nothing in this unit needs a wall; the previous runs' blockers
   were all *layout* blockers (a member not in the header at all), not codegen blockers.
2. `tools/dis.sh <vaddr> <size>` for retail's bytes. `report.json`'s
   `functions[].metadata.virtual_address` gives the address per function, so the loop is
   mechanical - no `symbols.txt` lookup needed.
3. Map each `lwz`/`lhz`/`lbz` displacement to a member name. **`tools/probe_offsets.cpp` is the
   tool for this**: compile a file with `#define private public` *before* the include and an
   `offsetof` table in `.data`, then read it back with objdump. This took about two minutes and
   resolved every offset in this run (0x12DC, 0x12C4/0x12C8, 0x2D4, 0x3B0/0x3DC/0x468/0x59C,
   0x5CC, 0x1268, 0x135C, 0x13AC/0x13B4, 0x1314, `mgr+0x24DC`). Do this before guessing.
   Note: `offsetof` on a **bitfield** is an "illegal operand" under mwcceppc, so bitfields must
   be found by exclusion from the neighbouring non-bitfield members - see `x1268_27_` below.
4. `tools/fast_try.sh MetroidPrime/Player/CPlayer` - two seconds, rebuilds one object and
   reprints every function's score. Iterate on spelling only.

### The bitfield at 0x1268, and why `x1268_27_` and not `x1268_28_`

`CanEnterMorphBallState` tests one bit of the byte at 0x1268 with
`lbz r0,4712(r31) ; rlwinm. r0,r0,28,31,31`, i.e. bit 28 counting from the MSB, which is
**bit 3 counting from the LSB** - and the header's eight `bool x1268_2X_ : 1` flags are laid out
in an order that is not the one their names suggest. Measured: naming the flag `x1268_28_`
emits `rlwinm. r0,r0,29,31,31` (83.0%), and `x1268_27_` emits `28` and matches (100%). So the
existing `x1268_24_`..`x1268_31_` names are **offset-from-MSB labels over an unordered
declaration order**; do not compute the flag name arithmetically from the shift. The four
spellings tried: `x1268_28_` 83.0%, `x1268_27_` 100%. (mwcceppc 2.7 also rejects
`EGrappleState::kGS_None` with a "declaration syntax error" - a nested enum's enumerator has to
be named bare, `kGS_None`, as the rest of the file already does.)

### `fn_8000e85c` - the whole tail is `min_val` + a timer, no calls

```cpp
x12dc_ = rstl::min_val(x12dc_ + 1, 2);
if (mSustainedDamageCount == 0) { return; }
mSustainedDamageTime += dt;
if (mSustainedDamageTime > 0.3f) { mSustainedDamageTime = 0.f; }
```

`0.3f` is `.sdata2:0x8041A474`, read as `lfs f0,-32548(r2)`; `rstl::min_val(a, b)` is
`(b < a) ? b : a` in `include/rstl/math.hpp:8` and that is the order the compare wants (it is
the *second* argument that is the cap). Offsets confirmed by probe: `x12dc_` 0x12DC,
`mSustainedDamageCount` 0x12C4, `mSustainedDamageTime` 0x12C8.

### `UpdateVisorTransition` - three spellings needed, and one of them is load-bearing

Prime 1's body (`prime-ref/src/MetroidPrime/Player/CPlayer.cpp:929`) reads
`CPlayerState* playerState = mgr.PlayerState();`. That is **wrong for Echoes**: retail does
`lwz r31,4884(r3)` = `this->mPlayerState`, so the parameter `mgr` is unused and the spelling
that matches is `CPlayerState* playerState = mPlayerState;`. **Lesson: on a port, check whether
the retail body reads the member before copying the reference's accessor.**

Second: the two `TUniqueId` stores are `lhz r0,lbl_8041B764@sda(r13)` twice - a **named**
constant, not `TUniqueId(0)`, and not `kInvalidUniqueId` either (that is 0xFC00|0x3FF in
`.sbss`). `lbl_8041B764` is `.sdata2:0x8041B764`, two bytes of value 0, owned by
`MetroidPrime/CPhysicsActor.cpp .sdata2`. Measured: `TUniqueId(0)` inline gives **93.43%**
(the `sth` becomes `sth r0,988` with `li r0,0` where retail has the `lhz`), and
`kInvalidUniqueId` gives **100%**. So **`kInvalidUniqueId` is the right name for a zero
`TUniqueId` here** even though its value reads as 0xFFFF in `PortGlobals.cpp` - the *load* is
what has to match, not the value. Worth a separate look later: either the constant is a second
zero-valued `TUniqueId` global, or `kInvalidUniqueId` is misplaced in `.sdata2`; this run did
not chase it, it just measured which spelling reproduces the bytes.

Third: the three orbit vectors are cleared with `clear()`, which is `mCount = 0` after a
destroy-elements loop that retail's compiler folded away, so each is one `stw`.

### `ObjectInScanningRange` - `const` on the locals is load-bearing

The body is Prime 1's shape. But `const CActor* actor` gives 94.39% and
`const CActor* const actor` **plus `const CVector3f dist`** gives 100%: without the `const` on
the *value*, MWCC 2.7 keeps the vector's address rather than passing `&dist` in the return slot
the way retail does (`addi r3,r1,12` before the `bl`, reused for `Magnitude` afterwards).
Same shape of problem as `GetDeathAlpha`'s register wall in the previous run's notes, one level
up: it is the *local's* constness, not the arithmetic. Also `TCastToConstPtr< CActor >` is needed,
not `TCastToPtr` - `mgr` is `const CStateManager&` and `GetObjectById` is the const overload, so
the template's non-const `CEntity*` parameter does not accept the result.

### `UpdateCrosshairsState` and `fn_8000bf1c` - both one-liners

```cpp
mDrawCrosshairs = mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown63, input);
```
`kC_Unknown63` is the name for command 63 in this header; retail's is Prime 1's
`kC_ShowCrosshairs`. The call is on `this->mControlMapper` (`addi r3,r31,5072` = 0x13D0), not a
`ControlMapper::` static, and the third argument is the `kFT_Filtered` default (retail materialises
`li r6,0`).

```cpp
if (mPlayerState->HasPowerUp(CPlayerState::kIT_LightSuit)) { return 0.f; }
return mSafeZoneHealSfxTimer / GetTweakPlayer()->GetDarkWorldDamageGracePeriod();
```
`kIT_LightSuit` is `0xE` = 14, which retail passes as `li r4,0x0e`; `mSafeZoneHealSfxTimer` is
0x135C = 4956, confirmed by probe. **The `clrlwi. r0,r3,24` after `HasPowerUp` is MWCC 2.7's own
code for an `if` on a `bool`, not a retail oddity** - it appears identically in
`UpdateVisorTransition` and `StartSamusVoiceSfx` too, and every function above that uses it
matches. Someone reading a diff will assume it is wrong; it is not.

### `fn_8000eba0` - one call, and the object is not missing

`ModelData()->AnimationData()->CollectAnimationTokens(mBallTransitionsRes, true)` matches
outright. Both accessors already exist (`CActor::ModelData()` at
`include/MetroidPrime/CActor.hpp:165`, `CModelData::AnimationData()` at
`include/MetroidPrime/CModelData.hpp:119`) and `CAnimData.cpp` is already in `files.cmake`
(line 1195), so no new symbol is opened. It needed
`#include "MetroidPrime/CAnimData.hpp"` - `CModelData.hpp` only forward-declares `CAnimData`,
so the call will not compile without the full definition.

## Tried and left at sub-100% in this run (so the next run skips them)

`StartSamusVoiceSfx__7CPlayerFUssi` (220 B, 2.55% -> **98.909%**, not kept) - the logic is
right and every instruction matches except the very last one, the return. Retail's tail is
`clrlwi r3,r31,24`; ours is `mr r3,r31`, `neg`/`or`/`srwi`, or `mr r3,r31` with the `if (result)`
hoisted. So retail's `result` is a value of a type that `clrlwi rX,rY,24` normalises to bool,
and the seven spellings below all move the *other* three instructions instead. Also `SfxStart`'s
priority argument is `lha r10,lbl_8041E2E4@sda` - a named constant (0x7FFF) that
`CSfxManager.hpp` does not have, which needed a **new static in a shared class** to spell; that
is a wider change than this item and the function was not going to flip anyway, so it was
reverted.

| spelling | score |
| --- | --- |
| full body, `bool result`, `kVoicePriority` as a new `CSfxManager` static, `bool` return | 98.909% |
| same but `short result` | 93.182% |
| same but `ushort` / `uint` / `uchar` result, `uchar` return | 93.182% - 98.909% |
| same but early-return form, no `result` variable | 66.091% |
| `return result != 0` from an `int result` | 95.000% |
| `const bool result` | 93.182% |
| `result = false;` moved outside the priority branch | 97.000% |

**WALL: StartSamusVoiceSfx 98.909% - the return's `clrlwi r3,r31,24` versus `mr`/`neg-or-srwi`
survives every spelling of the flag variable's type and every placement of the two early
returns; the rest of the 220-byte body matches.**

## `NEW:`

None filed. The two things still blocking other functions in this unit are the ones already
queued from `docs/goal-notes/progress-unit-cplayer.md` - `port-files-cmake-first-person-camera`
and `progress-unit-cplayergun-bits` (`DetachActorFromPlayer` 54.5%, `GetCombatMode` and
`GetExplorationMode` 13.3% each, all blocked on `CPlayerGun`'s unnamed bitfield members) - and
`docs/goal-notes/progress-unit-cplayer.md`'s `GetDeathAlpha` register wall. Nothing new was
measured about any of them here, so filing them again would be noise.
