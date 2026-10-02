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

---

# Third attempt (lane 5, 2026-10-02)

`fn_80012D10` was already matched by run 1, and this run found **eight** more functions in the
same unit, all at 100%. Two files changed: `src/MetroidPrime/Player/CPlayer.cpp` (+130/-13) and
`include/MetroidPrime/Player/CPlayerGunBase.hpp` (+25, two inline accessors and their comments).
No `tools/`, no `config/`, no `files.cmake`, no `build/goal/` edit beyond this notes file.

## Result, measured

| | before | after |
| --- | --- | --- |
| matched functions | 69 / 228 | **77 / 228** |
| matched code | 3468 / 71652 (4.8401%) | **4372 / 71652 (6.1017%)** |

Whole build `All: 34.71% fuzzy, 28.25% matched, 12.90% linked (12300 / 28465 functions)`;
`matched 12292 -> 12300`, `linked 5863 -> 5863`.

`./tools/goal_check.sh build/goal/item.json` in the worktree: **PASS**

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12292 -> 12300   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.71% fuzzy, 28.25% matched, 12.90% linked (12300 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CPlayer: 69 -> 77 / 228 functions
  ok    no asm added
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
`python3 tools/check_symbol_names.py` -> `0 declared names are missing`.
`python3 tools/check_decl_order.py --unit Player/CPlayer` does not name `Player/CPlayer`, so it
is still emitted in retail order (every edit below is an in-place body replacement; no
declaration moved). `tools/gate.sh`'s `per-function diff` reports `+8 functions at 100%, 0 units
newly linked` and no function got worse anywhere.

| function | bytes | before | after |
| --- | --- | --- | --- |
| `DetachActorFromPlayer__7CPlayerFv` | 44 | 54.55% | 100% |
| `GetCombatMode__7CPlayerCFv` | 60 | 13.33% | 100% |
| `GetExplorationMode__7CPlayerCFv` | 60 | 13.33% | 100% |
| `AttachActorToPlayer__7CPlayerF9TUniqueIdb` | 112 | 37.11% | 100% |
| `AddToRenderer__7CPlayerCFRC13CStateManager` | 132 | 3.03% | 100% |
| `RenderReflectedPlayer__7CPlayerFR13CStateManager` | 132 | 3.03% | 100% |
| `fn_80012e14__7CPlayerCFv` | 164 | 4.88% | 100% |
| `fn_80017358__7CPlayerFf` | 200 | 2.00% | 100% |

## The method that worked

Exactly run 2's, and it is now measured twice: **sort `build/report.json`'s unmatched functions
by size and read the smallest ones.** All eight here are 44-200 B and every one matched on the
first or second spelling. Nothing in this unit needs a codegen wall; the blockers the earlier
runs recorded were all *layout* blockers, and the layout is now all probed.

Three things make the loop fast and they are worth repeating:

1. `build/report.json` per unit -> `fuzzy_match_percent < 100` -> sort by `size`.
2. `./tools/dis.sh <vaddr> <size>` for retail's bytes. The addresses come straight out of
   `functions[].metadata.virtual_address`, so no `symbols.txt` lookup is needed.
3. **`tools/probe_offsets.cpp`'s method, scripted.** `tools/probe_cc.sh` as shipped lacks
   `-i extern/musyx-port/include`, so any TU that pulls in `musyx.h` (every one that includes
   `CPlayer.hpp`) fails to compile the probe; add that `-i` by hand. Then `#define private
   public` **and `#define protected public`** around the include - `protected` is needed for
   `CPlayerGunBase`'s members, and without it the compiler says "illegal access to
   protected/private member" on `offsetof`. `offsetof` on a **bitfield** is still "illegal
   operand", so leave those out. Compile the same flags `build.ninja` uses (take them off the
   `ninja -t commands` line) or the offsets are not the offsets.

To decide whether a candidate is *reachable at all* before writing it, this run also added a
scan that disassembles each unmatched function, collects its `bl` targets, and checks them
against the set of symbols the port's own objects already define. Every candidate below came
back with 0 missing callees. Two did not and were skipped without a build: `StopSounds`
(112 B, 3.57%) calls `fn_80060FD4`, and `__ct__CDamageVulnerability(const CDamageVulnerability&)`
(92 B, 82.61%) calls `fn_8001C6E4`; both are unmangled retail helpers with no port definition,
and adding either call would raise the port's undefined count.

## `fn_80012e14` - Prime 1's `GetMaximumPlayerPositiveVelocity`, plus a Phazon branch

Prime 1's body (`prime-ref/src/MetroidPrime/Player/CPlayer.cpp:1994`) is
`GetItemAmount(kIT_SpaceJumpBoots) ? 14.f : 11.66666f`. Echoes prepends a `GetSurfaceRestraint()`
test and a second `GetItemAmount(kIT_GravityBoost)`, and takes **four** returns, all named
constants:

| retail | constant | value |
| --- | --- | --- |
| `lfs -32344(r2)` = 0x8041A568 | `5.25f` | phazon, no gravity boost, has boots |
| `lfs -32340(r2)` = 0x8041A56C | `4.75f` | phazon, no gravity boost, no boots |
| `lfs -32336(r2)` = 0x8041A570 | `14.f` | other surface, has boots |
| `lfs -32332(r2)` = 0x8041A574 | `11.66666f` | other surface, no boots |

`11.66666f` is the literal, not `35.f/3.f`: the SDA word is `0x413AAAA4`, and `35.0f/3.0f` and
`11.666666f` both round to `0x413AAAAA` while `11.66666f` rounds to `0x413AAAA4`. That one bit of
mantissa is the whole difference between 100% and one instruction out.

Two spellings were needed:

| spelling | score |
| --- | --- |
| `mPlayerState->GetItemAmount(...)` written out at both sites | 95.61% |
| the same, with `CPlayerState* const playerState = mPlayerState;` hoisted | **100%** |

The second matters because retail holds `mPlayerState` in `r31` across **both** calls and the
`GetSurfaceRestraint` call in between; written out twice, the compiler reloads `lwz r3,4884(r30)`
the second time. Same lesson as `UpdateVisorTransition` in run 2, one level down.

`GetItemAmount`'s second parameter is spelled `true` explicitly even though the header defaults
it: retail materialises `li r5,1`, and MWCC will not elide it.

## `fn_80017358` - two countdown timers, and `= CSfxHandle()` not `= 0`

Retail 0x80017358 accumulates `x1194_` (0x1194) then runs two identical countdown blocks:
`x1190_` against `mLandingSfx` (0x117C), and `mDamageSfxTimer` (0x11A0) against
`mSamusVoiceSfx` (0x119C). Each is `if (timer > 0.f) { timer -= dt; if (timer <= 0.f) { stop; } }`.

| spelling | score |
| --- | --- |
| `mLandingSfx = 0;` after the `SfxStop` | 85.52% |
| `mLandingSfx = CSfxHandle();` (both handles) | **100%** |

`= 0` picks the `CSfxHandle(uint)` converting constructor and emits `addi r3,r1,16 / li r4,0 /
bl <uint ctor> / lwz r0,16(r1) / stw r0,4476(r31)` - five instructions where retail has `li r0,0
/ stw r0,4476(r31)`. The rest of the repo already writes `= CSfxHandle()` (see
`src/MetroidPrime/CPauseScreen.cpp:210`); `= 0` is the trap.

## `RenderReflectedPlayer` - a `float` local, not a `CVector3f` local

Retail reads `mgr.mCurrentRenderPlayer` (0x15F8), compares it with `this`, and switches on
`mMorphBallState` (0x38C) to pick 1.8f or 1.68f; it then materialises a `CVector3f` in the frame
at 8(r1) and calls `CModelData::SetScale` twice with its address (`ModelData()` at 0x60,
`mBallTransitionBeamModel` at 0x1210).

| spelling | score |
| --- | --- |
| `CVector3f scale(1.8f,1.8f,1.8f);` and `SetX/SetY/SetZ(1.68f)` in the cases | 84.24% |
| the same, all three components assigned in the cases | 84.24% |
| `float scale = 1.8f; ... scale = 1.68f;` then `const CVector3f v(scale, scale, scale);` | **100%** |

The first form stores the 1.8f triple **before** the branch (the compiler has to, the object is
live across it) and then re-stores 1.68f inside; retail loads `lfs f0,1.8` once at function entry
and only stores once, after the branch. One `float` local and one `CVector3f` built afterwards
is what produces that.

The `switch` needs all four `case` labels spelled, in ascending order, with `default: break;`
**after** them - that is what makes MWCC emit `cmpwi 3 / beq / bge / cmpwi 0 / beq / bge / b`
rather than the shorter `cmpwi 3 / bge / cmpwi 0 / bge / b`.

## `AddToRenderer` - the `if/else` is load-bearing, not the `||`

```cpp
if (x126b_28_) return;                                     // rlwinm. mb=29 on 0x126B
if (mCameraState != kCS_FirstPerson && mMorphBallState == kMS_Morphed) {
  CActor::AddToRenderer(mgr);
} else {
  mGun->AddToRenderer(mgr);
  CActor::AddToRenderer(mgr);
}
```

| spelling | score |
| --- | --- |
| `if (mCameraState != kCS_FirstPerson \|\| mMorphBallState != kMS_Morphed) { mGun->...; }` then one `CActor::AddToRenderer` | 93.48% |
| the `&&` with an explicit `else` holding both calls | **100%** |

Retail's branch is `cmpwi 0 / bne +0x40` off `mCameraState` and `cmpwi 1 / beq +0x58` off
`mMorphBallState`, with the gun call at the fall-through - the *negative* of the `||` form.
The gun call is `mtctr`/`bctrl` off vtable slot 10 of the `rstl::single_ptr<CPlayerGun>` at 0xEBC;
no spelling needed beyond `mGun->AddToRenderer(mgr)`, which finds it.

`x126b_28_` is the third-from-last `bool ... : 1` of the `0x126b` byte. Same trap as run 2's
`x1268_*` group: the flag names are offset-from-MSB labels over an unordered declaration order,
so do not compute the name from the shift.

## `GetCombatMode` / `GetExplorationMode` - and MWCC's read/write shift asymmetry

Both read `mGun`'s holster word at 936 and switch on it: combat is the two "out" states
(1 drawing, 2 drawn), exploration is the two "away" states (0 holstered, 3 holstering). The
word is `CPlayerGunBase::mGunHolsterState` (0x3A8, `EGunHolsterState`), which was `protected`, so
this run added a one-line public accessor `GetGunHolsterState()` to `CPlayerGunBase.hpp`
(header change, no layout effect).

The order of the `case` labels is what decides the emitted code, and **it differs between the
two functions**:

| spelling | GetCombatMode | GetExplorationMode |
| --- | --- | --- |
| cases listed true-first, then the false pair, then `default: break;` | **100%** | 98.87% |
| cases listed false-first, then the true pair, then `default: break;` | 100% | **100%** |
| only the two cases that return `true`, plus `default: return false;` | 100% | 78.53% |

Retail's `GetExplorationMode` puts the `li r3,0 / blr` for cases 1-2 **above** the `li r3,1 /
blr` for cases 0 and 3, and its `GetCombatMode` puts the `li r3,1 / blr` first. Neither is
obvious from the source; both are matched by listing the case labels in the order the `li`s come
out.

## `AttachActorToPlayer` / `DetachActorFromPlayer` - the MWCC read/write bitfield asymmetry

This is the finding most worth keeping. **MWCC 2.7 numbers a bitfield's read and write shift
differently by one.** For one member `k` (0-based declaration index in its byte):

* a read emits `rlwinm r0,r0,24+k,31,31` (**mb = 24+k**)
* a write emits `rlwimi r0,rX,31-mb,mb,mb` with the **same** mb - but because retail's `bl` is
  what we are matching, retail's `rlwimi r0,r6,6,25,25` (shift 6 = 31-25) means **mb = 25**, and
  the member that emits *that* is the one whose *read* is `mb = 26`.

Measured on `CPlayerGunBase`'s flag byte at 942 (0x3AE), both ways:

| member | read | write |
| --- | --- | --- |
| `mUnderwater` | `mb=25` | `mb=24` |
| `x3ae_25_` | `mb=26` | **`mb=25`** |
| `mInBigStrike` | `mb=27` | `mb=26` |
| `mMissileMode` | `mb=28` | `mb=27` |
| `mInPhazonPool` | `mb=29` | `mb=28` |

Retail `AttachActorToPlayer` (0x80012CCC) and `DetachActorFromPlayer` (0x80012C90)
read-modify-write `mb=25`, and `CPlayerGun::AcceptScriptMsg` (0x801CBE98) *reads* `mb=25` at
0x801CBE9C. Prime 1 calls that bit `mActorAttached`
(`prime-ref/include/MetroidPrime/Player/CPlayerGun.hpp:228,453`). Two attempts:

| spelling | Detach | Attach |
| --- | --- | --- |
| write `mUnderwater` | 98.64% | 48.00% |
| write `x3ae_25_` | **100%** | 48.54% |
| same, with the guard written `if (attached == kInvalidUniqueId) { ... return true; } return false;` | **100%** | **100%** |

So the member is the second one declared, and the header's `x3ae_25_` is the placeholder for
Prime 1's `mActorAttached`. **Both this run and run 2 picked the wrong member for the same
reason: they read the shift off a read instruction and then wrote the member.** If you are
writing a bitfield, write it and read the *write's* shift.

`AttachActorToPlayer`'s second half is Prime 1's body verbatim (`prime-ref:2018`), and the guard
has to be the nested `if (...) { ...; return true; } return false;` rather than
`if (...) return false;` - the early-return form puts the three `stfs`/`stw` before the branch
and MWCC reorders them differently. Both flags (`= 0x1f48`, `= 0x2E4`) and
`kInvalidUniqueId` are already correct in the tree.

## Not attempted, and why (measured, so the next run skips them)

* `fn_80011fc0` (128 B, 3.13%): reads `mModelData` (0x60) then its anim data (0x10), tests
  byte 0x128 of that, and calls `CAnimData::Render` with a `CColorF(1,1,1,1)`. Its callees are
  all defined, so it is reachable; it needs two unnamed members of `CAnimData` at 0x124/0x128
  that this header does not have (`CAnimData`'s private list stops at `mPose`/`mPlaybackParms`
  well below 0x124 - see the offsets in `tools/probe`-style probes: `mPassedParticleCount`
  0x29C, `x2a8_` 0x2A8, `mPose` 0x2B0, `mPlaybackParms` 0x414, `sizeof` 0x5B8, against
  retail's 0x124/0x128). A header change to a shared class that also decompiles elsewhere; not
  this item's business.
* `fn_8000d3ac` (96 B, 4.17%), `fn_8000d540` (156 B, 2.56%), `fn_8000ba60` (152 B, 2.63%),
  `fn_8000bbb4` (144 B, 2.78%), `PreThink` (184 B, 2.17%), `fn_8000d0ac` (104 B, 5.38%): all
  0 missing callees and all reachable in principle; each needs either a
  `CScriptPlayerTurret` class (this tree has only the loader struct
  `include/MetroidPrime/ScriptLoader/SLdrPlayerTurret.hpp`, no class), a `CScriptWater` /
  `CScriptTrigger` member, or a `CStateManager` member at 0x14F8/0x848/0x24DC that this header
  does not name. All **layout** blockers, not codegen blockers. `mgr+0x24DC` is
  `mCurrentRenderPlayerIndex` (probed) and `mgr+0x848` is `mObjectLists` (probed), so those two
  are the cheapest of the six if the callers' semantics fall out.
* `fn_80019360` (40 B, 18.90%): unchanged blocker from run 1 - the spelling reproduces the bytes
  but `src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` is still not in `files.cmake`, so the
  call grows the port's link gap.
* `GetDeathAlpha` (132 B, 95.76%) and `StartSamusVoiceSfx` (220 B, 98.91%): run 2's walls,
  unchanged and not retried.
* `GetCurrentBeam` (32 B, 99.88%): one instruction. Ours is `lwz r3,12(r3)` where retail has
  `lwz r3,1424(r3)`, i.e. `mPlayerState->GetCurrentBeam()` reading a different member of
  `CPlayerState`. A header member-order question in a shared class, not this item.

## NEW:

None filed. The two things still blocking other functions in this unit are already queued from
`docs/goal-notes/progress-unit-cplayer.md` (`port-files-cmake-first-person-camera`,
`progress-unit-cplayergun-bits`). The three new classes of blocker this run mapped -
`CScriptPlayerTurret` absent from the port, `CAnimData`'s members above 0x2B0 unnamed, and
`CStateManager`'s 0x14F8 unnamed - are all *layout* gaps in shared headers that other lanes
decompile, so filing them would duplicate work already in flight rather than open a lane.
