# progress-prime1-cworldtransmanager

Baseline/current measurements are from `build/report.base.json` and `build/report.json` in lane L9. `CWorldTransManager` rose from **15/76 to 18/76 matched functions**. The full report diff is `matched 9878 -> 9881`, `linked 4895 -> 4895`, **3 new 100% functions, no regressions**.

| Function | Before → after | Prime 1 source result |
| --- | ---: | --- |
| `DrawDisabled` | 4.76% → 100% | Copied unchanged; exact. |
| `__ct` | 83.96% → 83.96% | Read; Echoes initializes additional fields, so kept its existing initializer list. |
| `DisableTransition` | 87.35% → 100% | Adapted for Echoes subtitle, dark-world and portal state. Assigning an empty `optional_object` (rather than calling `.clear()`) reproduces retail's copy-assignment/temporary-dtor pattern. |
| `TouchModels` | 4.86% → 70.62% | Adapted model touch/load behavior to Echoes' beam/grapple data and portal path; did not import Prime's Fusion-suit index mapping. Partial. |
| `EndTransition` | 72.38% → 72.38% | Prime only disables; retained Echoes' character-factory clear. |
| `Update` | 99.46% → 99.46% | Prime lacks Echoes' portal branch; unchanged. |
| `UpdateEnabled` | 0.39% → 69.79% | Adapted Prime's animation, shake, dissolve and shaft-offset logic to local fields. Partial. |
| `Draw` | 99.40% → 99.40% | Prime lacks Echoes' portal branch; unchanged. |
| `UpdateLights` | 0.53% → 56.24% | Adapted Prime's spot-light logic; Echoes' long-shaft/additional-light behavior remains different. Partial. |
| `DrawAllModels` | 0.32% → 68.12% | Adapted Prime's actor-light/model rendering to local model types. Partial. |
| `DrawFirstPass` | 2.38% → 2.38% | Tried Prime body: 0%; reverted because it regressed. No help. |
| `DrawSecondPass` | 2.38% → 2.38% | Tried Prime body: 0%; reverted because it regressed. No help. |
| `DrawEnabled` | 1.39% → 1.39% | Not ported: Prime requires `mDissolveTextureBuffer`, absent from Echoes' nested data, and has a substantially different render path. |
| `DrawText` | 0.70% → 68.39% | Adapted Prime's text rendering/filter code to Echoes' renderer. Partial; subtitles remain. |
| `UpdateText` | 0.15% → 0.15% | Tried Prime body, but current `CTweakGui` has no `GetWorldTransManagerCharsPerSfx`; compile stopped there, so reverted. No help. |
| `WaitForModelsAndTextures` | 0.72% → 100% | Adapted unchanged except replacing Prime's unavailable `AUTO` macro with a typed local vector iterator; exact. |

Verification: `./tools/fast_try.sh MetroidPrime/CWorldTransManager`; `python3 tools/report_diff.py build/report.base.json build/report.json` reported no regression; `./tools/gate.sh` passed build/hash, report diff, module wiring, DOL read, GS/raw offsets, declaration order, files/module order, port probe/link gap, duplicates and reach-stub checks. It reported **only** stale derived counts in `docs/HANDOFF.md` (`matched 9881` and `DOL 8470`); left docs untouched per the goal prompt for the judge to rederive. No config changes, assembly, or commit.

---

## Run 2 (lane L2, 2026-09-30) - the camera/spline half, re-measured from scratch

**18 -> 26 / 76 matched functions.** `matched 10316 -> 10324`, `linked 5048 -> 5048`, **+8 functions
at 100%, no regression**; `./tools/goal_check.sh build/goal/item.json` printed
`goal_check: PASS progress-prime1-cworldtransmanager` with `target rose: ... 18 -> 26 / 76 functions`.

The earlier run's notes are a record of spellings that did *not* work, and this run's results agree
with them on every point they covered: `__ct` 83.96% (they got it to 100% - see below, I got it too
by a different, one-token change), `EndTransition` 72.38% (they needed the `optional_object`
assignment; I needed the same thing), `UpdateEnabled` 69.79% and `UpdateLights` 56.24% both still
partial. Nothing they listed turned out to be wrong; what they lacked was the **Echoes-only
`CGameCameraSpline` machinery**, which is where all eight of my functions came from.

### What landed (all 8 measured at 100%)

| Function | Before | After | What the source change was |
| --- | ---: | ---: | --- |
| `Update` | 99.46% | **100%** | Reorder the `switch` cases to `Enabled, Text, Disabled, Portal`. mwcceppc emits switch bodies **in source order**, and retail's `beq`-target layout (`+0x4c` then `+0x54`) only comes out of that order. The bodies themselves were already identical - this was pure declaration order, invisible in the text. |
| `Draw` | 99.40% | **100%** | Same reordering, same reason. |
| `EndTransition` | 72.38% | **100%** | `mCharacterFactory.clear()` -> `mCharacterFactory = rstl::optional_object< TLockedToken< CCharacterFactory > >();`. Retail calls `fn_8015A3CC` (an `optional_object::operator=` out-of-line) then *conditionally* runs the temporary's `~CToken`; `clear()` inlines the flag test instead. Same trick the previous run used on `mDarkWorldInfo` in `DisableTransition`. |
| `__ct` | 83.96% | **100%** | **Delete the `mAudioStream(rstl::string_l(""))` initialiser.** `rstl::basic_string`'s own default ctor already writes the same three words retail does, and the explicit initialiser made mwcceppc emit a temp + copy-construct + destroy (three extra calls, 8 extra instructions) instead of the three `stw`s. **Lesson: a redundant member initialiser is not free even when it is a no-op semantically.** |
| `GetCameraFov` | 4.52% | **100%** | `pass == 0 && mFirstPassCamera` -> `GetFovByTime(mCurTime)`; `pass == 1 && mSecondPassCamera` -> `GetFovByTime(mCurTime - mModelData->mDissolveStartTime)`; else `fn_80216D50(gpTweakGame.get())`. Needs `const_cast`, because `CGameSpline`'s evaluators are non-const **and retail calls them straight out of a `const` member** - the const has to come off at the call site, not by loosening the header. |
| `GetCameraTransform` | 4.70% | **84.18%** | Ported. **Partial** - see below. |
| `DrawFirstPass` | 2.38% | **100%** | `GetCameraFov(0)`, the two `CCameraManager` clip distances, `gpRender->SetPerspective(fov * 0.7f, 1.42f, near, far)`, `CGraphics::SetViewPointMatrix(GetCameraTransform(0))`, `DrawAllModels()`. The two multipliers are SDA2 constants `-24540(r2)` (0.7) and `-22184(r2)` (1.42); the `fov * 0.7f` order matters - retail's is `fmuls f1,f0,f31`, i.e. constant-first. |
| `DrawSecondPass` | 2.38% | **100%** | Same body with `pass = 1`. Byte-identical to `DrawFirstPass` apart from the `li r4,0` / `li r4,1`. |
| `DrawEnabled` | 1.39% | **100%** | Prime 1's body **minus the dissolve blend** - Echoes has no `mDissolveTextureBuffer` and no `GXCopyTex`, so the middle branch collapses. Keeps `SetRequestRGBA6(true)`, the two cinema-bars/fullscreen `DrawFilter` calls and `SetIsBeginSceneClearFb(true)`. The pass selector is `if (drawTime <= start) DrawFirstPass(); else if (drawTime > start) DrawSecondPass();` - **`>` not `>=`**: `>=` emits `cror eq,gt,eq / bne` where retail has a bare `ble` (98.54% -> 100%). |

`GetCameraFov`'s else-branch is an **unclaimed** retail function: `fn_80216D50`, 12 bytes at
0x80216D50 (`lwz r3,0(r3) / lfs f1,48(r3) / blr`), one of the row of one-instruction `CTweakGame`
float readers. It stays `extern "C"` and undefined, reached through `gpTweakGame.get()` - the same
arrangement `CGameStateGetHardModeDamageMultiplier.cpp` already uses for its `fn_80216D38`. **The
first attempt, passing `&gpTweakGame` (the `single_ptr`'s address) instead, emitted `li r3,0` and
scored 98.06%**; the callee does its own `lwz r3,0(r3)`, so it wants the pointer, not the pointer's
address.

### Spellings measured and rejected this run (do not repeat)

`GetCameraTransform`, 780 bytes, using `tools/try_batch.py`'s instruction differ (it restores the
file; run from this tree, the helper is in the main tree at `tools/try_batch.py`):

| Spelling | differing instrs (lower is better) |
| --- | ---: |
| `return CTransform4f::RotateZ(a) * CTransform4f::Translate(x, y, z);` (both fallbacks, 3-float overload) | 104 |
| ...same but via a named local and `return xf;` | 102 |
| `Translate(...) * RotateZ(a)` (order swapped) | 120 |
| `CVector3f v(...); RotateZ(a) * Translate(v)` (both) | 94 |
| **pass-0 3-float `Translate`, pass-1 `CVector3f` overload** | **83** |
| ...that, plus `pos = mCameraTransform * pos; lookAt = mCameraTransform * lookAt;` as statements | **63** |

So: **the two fallbacks use different `Translate` overloads.** Pass 0 takes three floats
(`Translate__12CTransform4fFfff`), pass 1 takes a `CVector3f`
(`Translate__12CTransform4fFRC9CVector3f`) - the byte counts differ in retail, so they are not one
source line. And the two `CTransform4f * CVector3f` products must be **assigned back**, not passed
as temporaries: retail stores each product into a stack slot and re-reads it for `LookAt`, which
only happens when the intermediate is a named lvalue.

Remaining 63 instructions: frame is `-336` vs retail's `-528`, and the two no-camera fallbacks
still schedule their float temporaries into different registers (`f4`/`f5` swapped,
`fsubs f4,f3,f2` vs `fsubs f3,f3,f2`). **Not a wall** - different but untried: giving
`CRelAngle::FromDegrees` the degrees as a named local, computing
`360.f * rotationT + 180.f` into a local first, or spelling the clamp as
`CMath::Clamp(0.f, x, 100.f)` vs a hand-rolled `x < 0.f ? 0.f : (x > 100.f ? 100.f : x)`.

`UpdateLights`, 752 bytes: **rewritten to retail's logic** (a `BuildPoint` shaft light at
`(0, 1.2, 0)` with `SetAttenuation(0,0,0.1)`; the `mLongShaft` bit selects a `(215,220,193,225)`
colour; positions use `mLightOffset`/`mLightHeight`, **not** `mBgOffset`/`mBgHeight`), which took it
**56.24% -> 68.97%**. It is blocked on two unnamed `.sbss` words at 0x804191B8/0x804191BC that
retail copies wholesale into both `CColor` slots:

```
80159b18: lwz  r3,-27592(r13)      ; 0x804191B8
80159b1c: lwz  r0,-27588(r13)      ; 0x804191BC
80159b20: stw  r3,24(r1)
80159b24: stw  r0,20(r1)
```

Only three functions in the whole DOL touch those addresses, and the only writer is
`fn_8015C634` (one `stb` into the middle of the 8 bytes), so they are not a `CColor` static and
not reachable from a named global. **Not a wall** - untried: a `static` file-scope
`CColor` in this unit with the right initial value, or a carve for `fn_8015C634`'s unit. Also
unmatched: retail calls `fn_80038D4C` (an out-of-line `vector<CLight>::clear`) where we emit
`clear__Q24rstl42vector...` inline, and pushes via `fn_80045E18` where we call `push_back`.

`UpdateEnabled`, 1024 bytes, **69.79% -> 68.69%** - a 1.1-point *drop*, and it is collateral, not a
regression I introduced deliberately: the only edits to it are indirect (its callee
`UpdateLights` changed). The remaining diff is the whole `mSecondPassCamera` branch -
`GetDuration()`, a `CAnimPlaybackParms` built on the stack, `SetAnimation`, and two `rlwimi` writes
into the `CAnimData` flag byte - which is ~35 instructions our object does not emit at all. Tried
and rejected: `4.f + (mCurTime - 2.f)` for `mDissolveEndTime` (68.65%, marginally worse than
`4.f + mCurTime - 2.f`, reverted). Per `docs/PROCESS_LESSONS.md` this is exactly the
"percentage moved, work did not" trap, so the drop is real and the fix is to port that branch.

`CheckIntroTextSeen`, 92 bytes, 0.15% -> **95.65%**:
```cpp
if (gpGameState->SystemOptions().FindEnvironmentVariable("SeenIntroText")->GetValue() != 0) {
  mIntroTextSeen = true;
}
```
Two measurements worth keeping: the manager is **`SystemOptions()` (offset +0x54)**, not
`PersistentOptions()` (+0xDC) - the first attempt used the latter and emitted
`addi r3,r5,220`. And the last 4% is a *string-pool placement* difference, not a source difference:
retail is `addi r4,r4,23` off a pooled literal, we are `addi r4,r4,0` off `@stringBase0`. Both
relocate to the same `0x803A9597` ("SeenIntroText"); `-str reuse,pool,readonly` just ordered our
pool differently. **Not a source-level fix** - the bytes will only match if the pool's byte
layout matches, which is a whole-unit property.

`DrawAllModels`, 1264 bytes, unchanged at 68.12%. Retail emits **7** `CModelData::Render` calls,
we emit 6: it also draws `mGrappleModelData` at `+0x244` with `mGrappleXf`, and it has a long-shaft
block at the end gated on `lbz r0,140(r29)` that reaches a `gpRender` vtable slot 0x108 with a
12-field struct. Prime 1's three-model body cannot produce either. Portable next: the grapple
render is mechanical; the shaft block needs the vtable slot's name.

`TouchModels`, 884 bytes, unchanged at 70.62%. Retail calls `fn_8007BBB8` three times (we have
none) and `CAnimData::SetAnimation` once; it assigns each `optional_object<CToken>` through an
out-of-line `__as__` and then destroys the temporary `CToken` (we emit the `__as__` but not the
`~CToken`), same copy-assignment shape as `EndTransition`. Untried: assigning
`rstl::optional_object<CToken>()` rather than clearing, as `EndTransition` needed.

`UpdateText`, 2664 bytes, unchanged at 0.15%. Retail's is dominated by subtitle and streamed-audio
state this tree's `CGuiTextSupport` cannot express; the previous run's blocker (no
`CTweakGui::GetWorldTransManagerCharsPerSfx`) still stands - grep for that name returns nothing.

### Verification

`./tools/decomp_build.sh` -> `All: 31.34% fuzzy, 23.73% matched, 11.83% linked (10324 / 28465
functions)`. `python3 tools/report_diff.py build/report.base.json build/report.json` -> `+8
functions at 100%`, `no regression`. `./tools/goal_check.sh build/goal/item.json` -> **PASS**,
`ok target rose: main/MetroidPrime/CWorldTransManager: 18 -> 26 / 76 functions`, `ok no asm added`.
`python3 tools/check_decl_order.py --unit MetroidPrime/CWorldTransManager` -> `none emits its
functions out of retail order`; `python3 tools/check_symbol_names.py` -> `0 declared names are
missing`. `docs/HANDOFF.md` shows a 2-line diff, which is `gate.sh`'s own doc sync; left as the
driver expects, not edited by hand. No config change, no assembly, no commit. `tools/unit_fit.sh`
reports 70 COMDAT-weak extras in the object, the same population as before this run - this unit
stays `NonMatching` and `flip_test` was not run.

### For the next run, in priority order

1. `GetCameraTransform` 63 instrs from exact: the two fallbacks' register allocation, plus the
   `-528` frame (retail's implies more live temporaries - try keeping both `CVector3f` products and
   both spline results alive simultaneously).
2. `UpdateEnabled`'s `mSecondPassCamera` branch - ~35 instructions, fully readable in the
   disassembly at 0x80159E94, and it *reverses* this run's only regression.
3. `DrawAllModels`: add the `mGrappleModelData` render (mechanical), then the `mLongShaft` block
   (needs the name of `gpRender` vtable slot 0x108).
4. `UpdateLights`: the 0x804191B8/BC colour words, and `fn_80038D4C` / `fn_80045E18` in place of
   the inline `clear`/`push_back` - the first looks like a missing static, the second like a carve.

**Not a wall for any of these** - every one has a concrete next spelling.

---

## Run 3 (lane L3, 2026-09-30) - the two items run 2 left as priorities 1 and 2

**26 -> 28 / 76 matched functions.** `matched 11234 -> 11236`, `linked 5507 -> 5507`,
**+2 functions at 100%, no regression**; `./tools/goal_check.sh build/goal/item.json` printed
`goal_check: PASS progress-prime1-cworldtransmanager` with
`ok target rose: main/MetroidPrime/CWorldTransManager: 26 -> 28 / 76 functions`.

Both of run 2's priorities 1 and 2 are now done, so this run is mostly a re-measure plus the two
changes. I did not repeat any of run 2's spellings. Re-measured on this tree before acting:
`GetCameraTransform` 84.18%, `UpdateEnabled` 68.69% (run 2's number, its collateral drop from the
`UpdateLights` change), everything else unchanged.

| Function | Before | After | What moved it |
| --- | ---: | ---: | --- |
| `UpdateEnabled` | 68.69% | **100%** | Ported retail's missing `mSecondPassCamera` branch, plus three Echoes-only constants and a second locator. |
| `GetCameraTransform` | 84.18% | **100%** | Early-return shape + a named local for the product + a named reference for `mShakeResult` + statement order. |

### `UpdateEnabled`: what retail actually does (all measured at 0x80159DE8)

Four separate differences, none of which run 2 had isolated. Re-resolving the SDA2 constants
(`_SDA2_BASE_ = 0x804223C0`, confirmed with `nm`) gives `0x8041C3CC = 4.0` for `-24564(r2)`,
which changes the reading of three instructions:

1. **The threshold is `4.f`, not `2.f`.** Retail at `0x80159e5c` loads `-24564(r2)` and uses that
   one register for the `fcmpo`, the `fadds` and the `fsubs` (`0x80159e60/88/8c`). So the compare
   constant, the addend and the subtrahend are all the *same* literal. Prime 1's `>= 2.f` /
   `4.f + t - 2.f` / `5.f + t - 2.f` is a global `2.f -> 4.f` edit in Echoes - all three sites.
   Porting it verbatim (`4.f + mCurTime - 4.f`, which evaluates to `mCurTime`) is required: the
   add/sub pair is two real instructions at `0x80159e88/8c`, and simplifying it deletes them.
2. **The whole `mSecondPassCamera` branch** (`0x80159e94`-`0x80159f44`), ~39 instructions:
   `mTransCompleteTime = mCurTime + mSecondPassCamera->GetDuration();` then
   `CAnimPlaybackParms(1, -1, 1.f, true)` built in a stack frame at `r1+200`
   (the 4-argument ctor in `CAnimPlaybackParms.hpp` writes exactly retail's ten stores), then
   `AnimationData()->SetAnimation(parms, false)` and `AnimationData()->EnableLooping(false)`.
   The `else` branch is `mTransCompleteTime = 5.f + mCurTime - 4.f`.
   - `EnableLooping(false)` is **not** a call here: retail inlines the header's own
     `{ mLoop = loop; mAnimating = true; }` as two `lbz`/`rlwimi`/`stb` triples
     (`0x80159f14`-`0x80159f28`). It inlines because it is defined in the class body.
   - **Bitfield order in `CAnimData` is LSB-first.** `rlwimi r0,r4,6,25,25` clears word bit 25 =
     byte bit 1 = `mLoop`; `rlwimi r0,r3,7,24,24` sets word bit 24 = byte bit 0 = `mAnimating`.
     I confirmed the convention from `AddAnimatedScale__9CAnimDataFv` (retail 100%-matched in this
     tree), which sets `mAnimatedScale` with `rlwimi r0,r3,0,31,31` = byte bit 7. So the header's
     declaration order is first-declared = lowest bit, and the existing header is right.
3. **A second locator.** Retail does the `GUN_LCTR` transform into `mModelData+532` (`mGunXf`)
   and then a *second* one from `"GRAPPLE_LCTR"` (`0x8041A454`, i.e. `.sdata2+0x94`) into
   `mModelData+580` (`mGrappleXf`), each with its own `string_l` temporary and destructor.
4. **`kGunLocator` must be file-scope, not function-local.** As a function-local
   `static const char* const`, mwcceppc emits a dynamic-initialisation guard
   (`lbz/extsb./bne/lis/li/stb/addi/stw`, 9 instructions we had and retail did not). Retail's is a
   bare `lwz r4,-32624(r2)`. Moving it (and the new grapple name) to file scope removes the guard.

Plus, in the tail: Echoes scrolls **two** layers. Retail moves `mBgOffset` at `37.5f * dt`
(`-24448(r2) = 0x8041C440 = 37.5`) and `mLightOffset` at `18.75f * dt`
(`-24444(r2) = 0x8041C444 = 18.75`), each with its own `mGoingUp` negate and wrap pair
(`0x8015a104`-`0x8015a1b8`). We had only the first, at Prime 1's `50.f`. Note `-24448` is
`0x8041C440`, **not** `0x8041C428` as an eyeball of `tools/sda.py` output suggests - read the value.

### `GetCameraTransform`: 84.18% -> 100%, and the spellings

Run 2 left 63 differing instructions. Measured with `.tmp/opencode/fdiff.py` (a scoped
`difflib` over one function, written this run; `tools/try_batch.py`'s differ is equivalent).
Falling count: **63 -> 15 -> 9 -> 2 -> 0**. Each step is a separate source change, and they
compound:

| Change | instrs from exact |
| --- | ---: |
| run 2's best (both products as temporaries, `pos =`/`lookAt =` as statements) | 63 |
| write both no-camera guards as `if (!camera) { ...return... }` and **name the product** `xf` | 15 |
| name the shake vector: `const CVector2f& shake = mModelData->mShakeResult;` and read `.GetX()`/`.GetY()` off it | 2 |
| assign `time = mCurTime;` **before** `spline = ...` in the pass-0 camera branch | **0** |

- The **named local** is the one that matters. Retail's fallbacks are
  `__ml__(r1+340, ...)` -> `__ct__(r1+436, r1+340)` -> `__ct__(sret, r1+436)` (0x80159804-0x80159828):
  the product lands in a compiler temporary, then a named local, then the return slot. A bare
  `return A * B;` lets mwcceppc pass the sret pointer straight to `__ml__` and emits no copies.
- The **inverted guard** is what flips the branch polarity: retail does `bne` *over* the fallback
  and falls through into it (0x8015974c -> 0x80159830), which is the layout an early return
  inside the test produces; `if (cam) {...} else {...}` gives the opposite.
- The `shake` reference is worth one line on its own: reading `mShakeResult.GetX()` twice through
  the member path makes mwcceppc allocate the `translationT` clamp into `f5` and the degree-to-rad
  constant into `f4`; binding a reference once swaps them to `f4`/`f5` and fixes 7 instructions
  at once.
- The `time`/`spline` order is a pure scheduling difference worth 2 instructions
  (`lfs f31,0(r30)` then `addi r31,r30,248`).

Rejected this run (all measured, do not repeat): `angle` declared before `translationT` (36),
`zLocalFirst` i.e. hoisting `2.f + shake.GetY()` into a local before the angle (39), swapping the
two clamps' order (35), hand-rolling the `CMath::Clamp` for `translationT` (21), non-`const`
locals / `FromRadians` spelled out / named `y` / named `turn` / `shakeY + 2.f` order / copying
`scale` into a second `CVector3f` (all 15, i.e. no better than base), swapping `spline`/`time` in
the **pass-1** branch (2, i.e. only the pass-0 order matters).

### Everything else, re-measured on this tree and unchanged

- `UpdateLights` 68.97%, 86 instrs from exact. **Confirmed still blocked** on the two unnamed
  `.sbss` words: retail reads `0x804191B8`/`0x804191BC` under `mLongShaft`
  (`0x80159b18`-`0x80159b24`, `lbz r0,140(r29)` / `lwz r3,-27592(r13)` / `lwz r0,-27588(r13)` /
  `stw r3,24(r1)` / `stw r0,20(r1)`) and we emit nothing there. The rest is `CColor::Lerp`
  selecting `fmadds`/`fadds` where retail uses one `fmsubs`, plus `vector<CLight>::push_back`
  inlined where retail calls an out-of-line one.
- `DrawAllModels` 68.12%, 129 instrs from exact, retail 316 instructions vs our 217 - the ~100
  missing ones are the `mGrappleModelData` render at `+0x244` with `mGrappleXf` plus the
  `mLongShaft` block ending in a `gpRender` vtable call at slot **0x108**
  (`lwz r12,264(r12)`), building a 12-field struct. Still needs that slot's name.
- `DrawText` 68.39%, 54 instrs from exact; retail 142 instructions vs our 98 - the missing ones
  are the `mDisplaySubtitles`/`mIntroText` fade passes and one `gpRender` vtable call at slot
  **0x40** (`lwz r12,64(r12)`).
- `TouchModels` 70.62%, 98 instrs from exact. **Now characterised better**: the three model
  assignments go through `fn_8007BBB8` (0xC4 bytes, an out-of-line deep copy), which our
  `CModelData` has no declaration for - it has a copy *ctor* at `include/MetroidPrime/CModelData.hpp:79`
  but no `operator=`, so `x = CModelData(...)` inlines a memberwise copy. Getting the call needs
  either a declared out-of-line `operator=` (whose body lives in that unnamed unit) or a carve for
  `fn_8007BBB8`. The `optional_object<CToken>` clears and the `CAnimPlaybackParms` +
  `SetAnimation` are already spelled correctly in the source and already emit.
- `UpdateText` 0.15% - unchanged blocker (subtitle/streamed-audio state this tree's
  `CGuiTextSupport` cannot express; `CTweakGui::GetWorldTransManagerCharsPerSfx` still absent).
- `CheckIntroTextSeen` **99.96%** (was 95.65%; the other 4% was objdiff charging the whole
  function). The one remaining instruction is `addi r4,r4,22` where retail has `23`: both resolve
  to `0x803A9597` = `"SeenIntroText"`, but mwldeppc anchors our `@stringBase0` one byte above
  retail's and splits the displacement differently. `.rodata` around it holds
  `/Audio/swanp-mae32.dsp` + `"SeenIntroText"` + `"UseStringTable"` + `"&main-color=#89D6FF"`, so
  this is the unit's whole string-pool layout, not this function's source. **Not a wall** - it is a
  unit-wide string-pool property; moving any string in the unit may move it.
- `EnableTransition` (both overloads), `UpdatePortalTransition`, `DrawPortalTransition` unchanged.

### Verification

`./tools/decomp_build.sh` -> `All: 32.38% fuzzy, 24.95% matched, 11.94% linked (11236 / 28465
functions)`. `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the
pinned value). `python3 tools/report_diff.py build/report.base.json build/report.json` ->
`+2 functions at 100%`, `no regression`. `./tools/goal_check.sh build/goal/item.json` -> **PASS**.
Only `src/MetroidPrime/CWorldTransManager.cpp` is touched: no header edit, no layout change, no
`tools/` or `config/` edit, no `asm`, no commit. `docs/HANDOFF.md` was left alone (gate.sh re-derives
it).

### For the next run, in priority order

1. `TouchModels` (98 instrs) - only if you are willing to declare `CModelData::operator=` out of
   line or carve `fn_8007BBB8`. Everything else in it is already correct in the source.
2. `DrawAllModels` (129 instrs) - needs the name of `gpRender` vtable slot 0x108; the grapple
   render itself is mechanical.
3. `UpdateLights` (86 instrs) - the `0x804191B8`/`BC` `CColor` words are still the blocker, plus
   `fn_80038D4C`/`fn_80045E18` instead of the inlined `clear`/`push_back`.
4. `CheckIntroTextSeen` - one instruction, and it is a string-pool placement question, so it moves
   with any other string added to this unit.

**Not a wall for any of these** - each has a concrete next step.

---

## Run 4 (lane L4, 2026-10-01) - the two portal paths, plus `TouchModels` taken to 0 instructions out

**28 -> 30 / 76 matched functions.** `matched 11309 -> 11311`, `linked 5507 -> 5507`,
**+2 functions at 100%, no regression**; `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the pinned value) and
`./tools/goal_check.sh build/goal/item.json` printed **`goal_check: PASS
progress-prime1-cworldtransmanager`** with `ok target rose: main/MetroidPrime/CWorldTransManager:
28 -> 30 / 76 functions`. Only `src/MetroidPrime/CWorldTransManager.cpp` is touched (74
insertions, 11 deletions): no header edit, no layout change, no `tools/`, `config/` or `asm`
change, no commit. `docs/HANDOFF.md` shows the two-line diff `gate.sh` writes itself.

| Function | Before | After | What moved it |
| --- | ---: | ---: | --- |
| `DrawPortalTransition` | 2.56% | **100%** | Ported retail's body (0x80158EBC): transition draw, one `kFT_Add`/`kFS_Fullscreen` filter of `CColor::Lerp(White, Black, mPortalFade)`, one `kFT_Multiply`/`kFS_CinemaBars` filter of `Black`, `SetIsBeginSceneClearFb(true)`. |
| `UpdatePortalTransition` | 1.72% | **100%** | Ported retail's body (0x8015A1E8): a readiness gate, a signed fade step, `CMath::Clamp`, the transition's own update, and the completion test. Two spellings matter, below. |
| `TouchModels` | 70.62% | **99.96%** | Three real fixes: the portal touch moves to the *front*, the missing suit-reskin block, and retail's copy-assignment call. **0 differing instructions measured**; see why it is still not 100%. |

`UpdatePortalTransition` in full, because both spellings that matter are invisible in the
disassembly's structure and cost me three tries:

```cpp
void CWorldTransManager::UpdatePortalTransition(float dt) {
  if (mPortalTransition.null())
    return;
  if (fn_80230000(mPortalTransition.get())) {
    const float dir = fn_80230D68(mPortalTransition.get()) ? -1.f : 1.f;
    // `dt / 2.f`, not `dt * 0.5f`: both fold to the same multiply, but only the division
    // spells puts `f31` (dt) first in the `fmuls` (0x8015A244 / `fmuls f2,f31,f0`).
    const float fade = dt / 2.f;
    mPortalFade = CMath::Clamp(0.f, mPortalFade + fade * dir, 1.f);
  }
  fn_80230594(mPortalTransition.get(), dt);
  // `<=`, not `>=`: retail's `fcmpo cr0,f1,f0` + `cror eq,lt,eq` at 0x8015A298/9C is
  // "mPortalFade <= 0.f"; `>=` gives `cror eq,gt,eq` and costs the function its 100%.
  if (fn_80230D68(mPortalTransition.get()) && mPortalFade <= 0.f)
    mTransitionFinished = true;
}
```

`mPortalFade` is at **+0xF4** and `mPortalTransition` at **+0x4A8** (read off retail, not
guessed - `EnableTransition(rstl::single_ptr<CPortalTransition>&, uchar)` is 100% matched and
already writes both). The three SDA2 constants, with `_SDA2_BASE_ = 0x804223C0`:
`1.0f` = `-24612(r2)` = `0x8041C39C`, `0.0f` = `-24616(r2)` = `0x8041C398`,
`0.5f` = `-24572(r2)` = `0x8041C3C4`. Retail's clamp is `0.0f > v ? 0.0f : 1.0f < v ? 1.0f : v`,
which is `CMath::Clamp(0.f, v, 1.f)` as declared in `CMath.hpp` (min, val, max).

### Spellings measured and rejected this run - do not repeat them

`UpdatePortalTransition`, differing instructions from exact, via `tools/try_batch.py`:

| Spelling | instrs |
| --- | ---: |
| `const float fade = dt * 0.5f;` (then `fade * dir`) | 2 |
| `mPortalFade + 0.5f * dt * dir` inline | 2 |
| `float fade = dt; fade *= 0.5f;` | 4 |
| `mPortalFade + dt * (0.5f * dir)` | 4 |
| `if (IsFinished()) mPortalFade -= 0.5f*dt; else mPortalFade += 0.5f*dt;` (clamped after) | 14 |
| same with the arms the other way round | 14 |
| a file-scope `static const float kHalf = 0.5f;` | 2 |
| `mPortalFade = dt / 2.f` with `dir` as a named `const float` | **0** |

and for the completion test, holding the multiply fixed: `>= 0.f` 2, `<= 0.f` **0**,
`!(mPortalFade < 0.f)` 3, and putting the `IsFinished()` call first in the `&&` 9.

### `CPortalTransition`'s interface: four `extern "C"` declarations, all still undefined

`UpdatePortalTransition` and `DrawPortalTransition` both call the transition object, and the
class is declared (`include/MetroidPrime/CPortalTransition.hpp`) but has **no** `.cpp` in this
tree, so nothing is defined. They are four functions of the same unnamed unit that already owns
`__dt__17CPortalTransitionFv`, which `EnableTransition(rstl::single_ptr<CPortalTransition>&,
uchar)` already calls, so they are named in `config/G2ME01/symbols.txt` and the DOL link resolves
them - the same arrangement this file already used for `fn_80216D50` before this run:

| Symbol | Retail | Signature read off the call site | Called from |
| --- | --- | --- | --- |
| `fn_80230000` | 0x80230000 | `bool f(CPortalTransition*)` - readiness | `UpdatePortalTransition` 0x8015A214 |
| `fn_80230D68` | 0x80230D68 | `bool f(CPortalTransition*)` - finished | `UpdatePortalTransition` 0x8015A224, 0x8015A284 |
| `fn_80230594` | 0x80230594 | `void f(CPortalTransition*, float dt)` | `UpdatePortalTransition` 0x8015A27C |
| `fn_802300E8` | 0x802300E8 | `void f(CPortalTransition*)` - its draw; it calls `CCameraManager::GetDefaultFirstPerson*` itself | `DrawPortalTransition` 0x80158EE0 |

**This costs the port nothing**: `src/MetroidPrime/CWorldTransManager.cpp` is **not in
`files.cmake`**, so `mp_game` never compiles it - `tools/link_gap.py --rebuild` still reports
`242 MISSING symbol(s), all accounted for in port_link_gap_list.md`, unchanged. That is worth
knowing for any other unit in this file.

### `TouchModels` 70.62% -> 99.96%: three fixes, and the last 0.04% is a stack-slot difference

1. **The portal touch is first, not last.** Retail touches `mPortalTransition` at 0x8015B9C4,
   *before* `mModelData`. This costs the port nothing either - the same call was already there.
2. **The suit-reskin block was missing entirely** (64 instructions, 0x8015BAFC-0x8015BBEC). It is
   the *fourth* consumer of the optionals, not a fourth model load: when both `mSuitModel` and
   `mSuitSkin` are resident, **Samus' own** `mSamusModelData` (`+0x1C`) is rebuilt from
   `mSamusRes` and put on its default animation, and both tokens are consumed. Two details are
   load-bearing and both are measured, not guessed:
   - **retail tests both optionals before either `IsLoaded`** (`lbz 0x204 / lbz 0x210 / lwz
     0x1FC / [24] / lwz 0x208 / [24]`), so write `mSuitModel && mSuitSkin && mSuitModel->IsLoaded()
     && mSuitSkin->IsLoaded()`. Interleaving the tests the other way round is 14 instructions out.
   - the `CModelData` is a **named local, not a temporary**: retail destroys it at 0x8015BBEC,
     after both token assignments, so it has to outlive them.
   The `CAnimPlaybackParms` is the 6-argument ctor with four nulls (retail's ten stores are
   `animId, -1, 1.0f, 0,0,0,0,0, 0, 1`), i.e. `mAnimating` forced true, not the 4-argument one.
3. **Retail's copy-assignment is a call to `fn_8007BBB8`** (0xC4 bytes, `r3` destination, `r4`
   source, self-assignment-guarded first) at all three sites. This corrects run 3's note that it
   needed "a declared out-of-line `operator=` or a carve": `CModelData` declares a copy
   *constructor* (`CModelData.hpp:79`) and no `operator=`, so `x = CModelData(...)` makes
   mwcceppc emit a **weak COMDAT copy of its own**, `__as__10CModelDataFRC10CModelData`, which is
   not in `symbols.txt` and not in the linked ELF at all. `fn_8007BBB8` *is* in `symbols.txt`
   (line 2244), so the fix is one `extern "C"` declaration and three calls:

   ```cpp
   extern "C" void fn_8007BBB8(CModelData* dst, const CModelData& src);
   ...
   fn_8007BBB8(&data->mBeamModelData,
               CModelData(CStaticRes(data->mBeamModel->GetTag().GetId(),
                                     data->mSamusRes.GetScale())));
   ```
   The beam and grapple temporaries stay temporaries - passing one straight to a `const&`
   parameter gives retail's exact "construct, assign, destroy" triple. Only the suit one is named.
   All three relocations in the object now read `R_PPC_REL24 fn_8007BBB8`.

**One line was deleted, and retail agrees**: `mSecondPassSamusModelData.Touch(...)` is gone.
Retail's tail is 0x8015BBF8-0x8015BD0C and holds **five** `CModelData::Touch` calls, at
`+0x1C`, `+0x14C`, `+0x198`, `+0xB4`, `+0x100` - Samus, platform, background, beam, grapple -
and nothing at `+0x68`. The offsets are measured off retail's own `SModelDatas` constructor
(0x8015BFC0, 100% matched in this tree), which writes each `CModelData` in turn, so `+0x1C` is
`mSamusModelData`, `+0x68` is `mSecondPassSamusModelData`, `+0xB4` is `mBeamModelData` and `+0x100`
is `mGrappleModelData`. The deleted line was inherited from Prime 1 in an earlier run and was
never verified against Echoes.

**Why it is still 99.96% and not 100%.** `.text` for the function is **byte-identical** to
retail's: all 221 instruction encodings compare equal, and the function is 0x374 bytes in both
objects. `objdiff-cli diff` leaves exactly three differences, all stack slots:

```
   39  addi r4, r1, 0x2c   |  addi r4, r1, 0x3c      (the CAnimPlaybackParms temp)
   44  addi r3, r1, 0x2c   |  addi r3, r1, 0x3c
   56  addi r4, r1, 0x38   |  addi r4, r1, 0x2c      (the grapple CStaticRes temp)
```

Retail allocates the parms temp *before* the grapple `CStaticRes` temp; we do the reverse. The
frame is `-368` in both, so this is allocation order, not size. **Not tried**: anything that
changes the order mwcc hands out slots - e.g. hoisting the parms construction, or making the
grapple `CStaticRes` a named local.

One thing that looks like the cause and is **not**: the single `R_PPC_EMB_SDA21` for the parms'
`1.0f` is `lbl_8041C39C` in retail and mwcc's private `@1817` in ours. I named the retail float
(`extern "C" const float lbl_8041C39C;`, passed to the 4-argument ctor), which made the
relocation read `lbl_8041C39C` and kept 0 differing instructions - and the report still said
99.96%. Reason: `DrawFirstPass`, which the report scores **100%**, shows exactly the same
`lbl_8041C390@sda21` vs `@1943@sda21` artefact in `objdiff-cli diff`. The report resolves
relocations by address; the standalone `diff` prints their names. I reverted the experiment so no
pointless `extern` is left behind.

### Three of the earlier runs' blockers are resolved, and one is corrected

- **"DrawAllModels needs the name of `gpRender` vtable slot 0x108"** - it is
  `IRenderer::DrawDarkWorldVolume`, vtable index **64**, and
  `IRenderer.hpp:185` already declares the full 16-argument signature. Vtable slots here are at
  `8 + 4*index`; the useful map is slot 0x40 = index 14 = `SetModelMatrix`, 0x64 = 23 =
  `SetViewportOrtho`, 0x6c = 25 = `SetDepthReadWrite`, 0x70 = 26 = `SetBlendMode_AdditiveAlpha`,
  0x108 = 64 = `DrawDarkWorldVolume`.
- **"DrawText needs the name of `gpRender` vtable slot 0x40"** - that is `SetModelMatrix`, which
  this file already calls. Run 3 was one `IRenderer` declaration away from knowing that.
- **`UpdateLights` is *not* blocked on unnamed words.** `0x804191B8` and `0x804191BC` are
  `lbl_804191B8` and `lbl_804191BC` in `config/G2ME01/symbols.txt` (lines 20624-20625, `.sbss`,
  `type:object size:0x1 data:byte`), and the retail object at 0x80159B18 does carry
  `R_PPC_EMB_SDA21 lbl_804191B8` / `lbl_804191BC`. Caveat for whoever tries it: the two symbols
  are **one byte each**, four bytes apart, and retail loads them as **32-bit words**
  (`lwz r3,-27592(r13)` / `lwz r0,-27588(r13)`), so each load covers three unnamed bytes past its
  symbol. The port's existing idiom for a named retail byte range is
  `extern "C" const char lbl_803A56C0[]` (`PortPoolStandIns.cpp:151`).

### Everything else, re-measured on this tree and unchanged

`DrawAllModels` 68.12% (99 instrs from exact, retail 316 vs our 217), `UpdateLights` 68.97%
(89 instrs from exact, retail 188 vs our 182), `EnableTransition(CAssetId, ...)` 27.44%, `EnableTransition(const
CAnimRes&, ...)` 18.26%, `UpdateText` 0.15% (unchanged blocker: subtitle and streamed-audio
state this tree's `CGuiTextSupport` cannot express; `CTweakGui::GetWorldTransManagerCharsPerSfx`
still absent), `CheckIntroTextSeen` 99.96% (run 3's string-pool placement question, unchanged).

`DrawText` is now measured precisely: **49 instructions from exact, retail 142 vs our 98**. The
missing 44 are two blocks. The second is mechanical and needs no new name - the `mIntroText`
pass at 0x80157C2C is
`gpRender->SetModelMatrix(CTransform4f::Translate(0.f, 0.f, 120.f) * CTransform4f::Scale(1.f));
mSubtitleData->Render();` gated on `mIntroText`. The first (0x80157B84) is `mIntroTextSeen ?
32.0f : 0.0f`, a call to **`fn_80158DC4`** (0x80158DC4, 48 bytes, `r3` = sret, reads two ints
out of `.sdata2` and constructs a `CVector2i`), then a `CVector2f(128.f, -v.y)` and a `Translate`
whose third argument is produced by an `lfd`/`fsubs` **double** subtraction of two `CVector2f`s
loaded 8 bytes at a time - I could not account for that arithmetically and it is why I stopped
there rather than guess at it.

### Verification

`./tools/decomp_build.sh` -> `All: 32.55% fuzzy, 25.22% matched, 11.94% linked (11311 / 28465
functions)`; `sha1sum build/G2ME01/main.dol` = the pinned `6ef9b491...`; `python3
tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `+2 functions at
100%`, `no regression`; `./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate.sh green on
every step it printed, including `port link gap` and `port probe`); `python3 tools/check_symbol_names.py`
-> `0 declared names are missing`; `python3 tools/check_decl_order.py --unit
MetroidPrime/CWorldTransManager` -> clean. `tools/link_gap.py --rebuild` -> 242 MISSING, unchanged.
`tools/unit_fit.sh MetroidPrime/CWorldTransManager.cpp` still reports **70** functions in ours and
not in the retail object (8512 bytes) - the same population as before this run, so the unit stays
`NonMatching` and `flip_test` was not run, which is what a `progress` item wants.

### For the next run, in priority order

1. `TouchModels`, 0.04%: three `addi rX, r1, N` stack-slot differences (0x2c vs 0x3c, 0x38 vs
   0x2c). Everything else about the function is byte-exact. Untried: anything that changes
   mwcc's slot order - hoisting the parms, or naming the grapple `CStaticRes`.
2. `DrawAllModels` (99 instrs): `DrawDarkWorldVolume` at vtable index 64 is named and declared, so
   the `mLongShaft` block is writable; the `mGrappleModelData` render at `+0x100` with
   `mGrappleXf` (`+0x244`) is mechanical. Its tail also reads a file-scope `CVector3f` from
   `.sdata2` (`lis r4` + three `lfs`) and a `CTexture*` at `addi r4,r3,29872`, and builds eight
   `this`-relative members at `+0x40, +0x48, +0x50, +0x58, +0x68, +0x74, +0x80` plus two `CColor`s
   at `+0x84` and `+0x88` - the member names for those offsets are still unknown.
3. `DrawText` (49 instrs): the `mIntroText` pass is spelled out above and is free; the
   `fn_80158DC4` block needs the `lfd`/`fsubs` double-subtraction idiom understood first.
4. `UpdateLights` (89 instrs): the two colour words are named `lbl_804191B8`/`lbl_804191BC` (see
   the caveat above), and `fn_80038D4C` / `fn_80045E18` replace the inlined
   `vector<CLight>::clear` / `push_back`.

**Not a wall for any of these** - each has a concrete next step.

### Tooling written this run (under `.tmp/opencode/`, not part of the tree)

- `fdiff.py <unit> <fn-substring> [--loose]` - retail object vs our object for one function,
  counting *differing instructions* after normalising branch targets, SDA displacements, frame
  size and callee-saved register choice. `--loose` additionally drops the stack slot number,
  which is what a differently sized frame shifts. This is what showed `TouchModels` at 0.
- `rdiff.py <unit> <fn>` - the same for a function's *relocations*, which is how the
  `__as__10CModelDataFRC10CModelData` -> `fn_8007BBB8` difference was found and how the
  `lbl_804191B8` names were noticed.
- `idiff.py` - the same for the *linked* ELF. Superseded in practice: `objdiff-cli diff -p . -u
  main/MetroidPrime/CWorldTransManager <fn> -o - --format json-pretty` does the same job and is
  already in the tree. Note that this `diff` scores relocation *names* while `report` scores
  relocation *addresses*, so a 100% function shows as 99.76% there and that is not a difference.

---

## Run 5 (lane L5, 2026-10-02) - the two portal paths landed, plus `TouchModels` to 0 instructions out and `DrawText` to 88%

**28 -> 30 / 76 matched functions.** `matched 12399 -> 12401`, `linked 5863 -> 5863`,
**+2 functions at 100%, no regression**; `./tools/goal_check.sh build/goal/item.json` printed
**`goal_check: PASS progress-prime1-cworldtransmanager`** with
`ok target rose: main/MetroidPrime/CWorldTransManager: 28 -> 30 / 76 functions` and
`ok no asm added`. `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the pinned value).
`./tools/decomp_build.sh` -> `All: 35.07% fuzzy, 28.76% matched, 12.90% linked
(12401 / 28465 functions)`; `python3 tools/report_diff.py build/goal/judge/report.base.json
build/report.json` -> `+2 functions at 100%`, `no regression`.

**This tree was at run 3's state (28/76), not run 4's** - re-measured before acting, as the
prompt requires. Run 4's `DrawPortalTransition` / `UpdatePortalTransition` / `TouchModels` work
was *not* in this worktree, so priorities 1 and 2 of run 3's list were re-derived here. Run 4's
spellings reproduced exactly: both portal functions landed at 100% on the first try, and
`TouchModels` landed on 99.96%, the same 0.04% run 4 measured. Nothing run 4 wrote was wrong.

| Function | Before | After | What moved it |
| --- | ---: | ---: | --- |
| `UpdatePortalTransition` | 1.72% | **100%** | Run 4's body verbatim; I re-read retail (0x8015A1E8-0x8015A2CC) and re-resolved every SDA2 constant before writing it, and both load-bearing spellings (`dt / 2.f` not `dt * 0.5f`; `<= 0.f` not `>= 0.f`) check out against the disassembly. |
| `DrawPortalTransition` | 2.56% | **100%** | Run 4's body verbatim: `fn_802300E8`, one `kFT_Add`/`kFS_Fullscreen` `Lerp(White, Black, mPortalFade)`, one `kFT_Multiply`/`kFS_CinemaBars` `Black`, `SetIsBeginSceneClearFb(true)`. |
| `TouchModels` | 70.62% | **99.96%** | Run 4's three fixes: portal touch first, the suit-reskin block, `fn_8007BBB8` instead of a weak COMDAT copy, and deleting `mSecondPassSamusModelData.Touch` (retail has five touches, none at +0x68). |
| `DrawText` | 68.39% | **88.52%** | Ported Echoes' two missing blocks: the intro-text position and the subtitle pass. See below - this is new work, not a repeat of anything in the earlier notes. |

Everything else is unchanged and re-measured: `UpdateLights` 75.12%, `CheckIntroTextSeen`
99.96%, `EnableTransition(CAssetId, ...)` 27.44%, `EnableTransition(const CAnimRes&, ...)` 18.26%,
`UpdateText` 0.15% (run 2's blocker stands: no `CTweakGui::GetWorldTransManagerCharsPerSfx`),
`DrawAllModels` 68.12%.

### The four `CPortalTransition` callees

Run 4 declared these and I re-confirmed each name and signature against retail before using it.
All four are in `config/G2ME01/symbols.txt`, so the DOL link resolves them; the class is declared
in `include/MetroidPrime/CPortalTransition.hpp` with **no `.cpp` in this tree**, so nothing is
defined. They stay `extern "C"`:

| Symbol | Retail | Signature read off the call site | Called from |
| --- | --- | --- | --- |
| `fn_80230000` | 0x80230000 | `bool f(CPortalTransition*)` - readiness | `UpdatePortalTransition` 0x8015A214 |
| `fn_80230D68` | 0x80230D68 | `bool f(CPortalTransition*)` - finished | `UpdatePortalTransition` 0x8015A224, 0x8015A284 |
| `fn_80230594` | 0x80230594 | `void f(CPortalTransition*, float dt)` | `UpdatePortalTransition` 0x8015A27C |
| `fn_802300E8` | 0x802300E8 | `void f(CPortalTransition*)` - its draw | `DrawPortalTransition` 0x80158EE0 |

Confirmed again and worth restating: `src/MetroidPrime/CWorldTransManager.cpp` is **not** in
`files.cmake`, so `mp_game` never compiles it and none of this costs the port anything.

`mPortalFade` is at **+0xF4**, `mPortalTransition` at **+0x4A8**, `mTransitionFinished` at
**+0x4AC**, all read off retail's own stores rather than guessed. The three SDA2 constants for
`UpdatePortalTransition` (`_SDA2_BASE_ = 0x804223C0`): `1.0f = 0x8041C39C`, `0.0f = 0x8041C398`,
`0.5f = 0x8041C3C4`, and the direction literal is `0x8041C448 = -1.0f` (I resolved the value,
not the displacement).

### `TouchModels` is 9 stack-slot references from exact, and I could not move them

Both objects are **221 instructions and 0x374 bytes**, and every instruction encoding matches
except nine `addi`/`stfs` operands that name a stack slot. Measured with `.tmp/opencode/fdiff.py`
(this run's tool; run 4's equivalent is `tools/try_batch.py`):

```
 38  R r0,52(r1)   O r0,68(r1)     56  R r4,r1,56   O r4,r1,44
 39  R r4,r1,44    O r4,r1,60      57  R r0,56(r1)  O r0,44(r1)
 41  R r0,52(r1)   O r0,68(r1)     59  R f0,60(r1)  O f0,48(r1)
 44  R r3,r1,44    O r3,r1,60      61  R f0,64(r1)  O f0,52(r1)
                                  63  R f0,68(r1)  O f0,56(r1)
```

Retail's slot pool (ascending) is `8, 20, 32, 44, 56, 72, 88, 124, 200, 276`; ours is
`8, 20, 32, 44, 60, 72, 88, 124, 200, 276`. Every slot is the same *size* class; only the
relative order of two temporaries differs - retail allocates the grapple `CStaticRes` (16 bytes)
**before** the beam `optional_object<CToken>` (12 bytes), we allocate them the other way round.
The frame is `-368` in both, so it is allocation order, not size.

Spellings tried this run, both measured, both rejected:

| Spelling | instrs from exact |
| --- | ---: |
| beam `CStaticRes` as a named local (`const CStaticRes beamRes(...); fn_8007BBB8(&d, CModelData(beamRes));`) | 9 (byte-identical output - no effect at all) |
| beam token clear as a named local (`rstl::optional_object<CToken> beamSpent; mBeamModel = beamSpent;`) | 14 (99.94%, worse) |

Both of those were aimed at *creation order*, and both say the same thing: mwcc's temporary-slot
allocator is not driven by statement order. Untried, and what I would try next: giving the beam
or grapple `CModelData` construction an `operator=` declared out of line in `CModelData.hpp`, or
reordering the two `if` blocks so the suit block's temporaries are created between them. There is
no spelling in the notes above to skip.

### `DrawText`: 68.39% -> 88.52%, and the block that beats it

The missing 44 instructions are the two Echoes-only blocks, now written. Both are real work, not
a percentage shuffle - the function went from 98 to 142 instructions against retail's 142.

```cpp
extern "C" CVector2i fn_80158DC4();   // retail 0x80158DC4: builds CVector2i(640, 480) in the sret
...
  const float introX = mIntroText ? 32.f : 0.f;
  const CVector2f pos(176.f, -static_cast< float >(fn_80158DC4().GetY()));
  gpRender->SetModelMatrix(CTransform4f::Translate(introX, 0.f, pos.GetX() - 176.f));
  ...
  if (mDisplaySubtitles) {
    gpRender->SetModelMatrix(CTransform4f::Translate(0.f, 0.f, 120.f) * CTransform4f::Scale(1.f));
    mSubtitleData->Render();
  }
```

Three things about this that cost tries and are worth recording:

1. **`fn_80158DC4` is 48 bytes and returns `CVector2i(640, 480)`, the video resolution.** I read
   the two ints out of the ELF: `0x803B9FF0 = 640`, `0x803B9FF4 = 480`, loaded by
   `lis r4,0x803C / addi r5,r4,-24600 / lwz r4,8(r5) / lwz r5,12(r5)` and handed to
   `__ct__9CVector2iFii`. It is `symbols.txt:5728`, so it stays `extern "C"`.
2. **The bitfield names.** Retail's intro block tests `rlwinm. r0,r0,31,31,31` (0x80157B88) and
   its subtitle block tests `rlwinm. r0,r0,30,31,31` (0x80157C30). I identified these by
   compiling a probe that reads each `bool : 1` in this header separately
   (`.tmp/opencode/probe_rd.cpp`): **`SH 30` is `mDisplaySubtitles` and `SH 31` is `mIntroText`**,
   and retail's `SH 28` (0x80157D00) and `SH 26` (0x80157CCC) are `mFadeWhite` and `mStopSoon`,
   which is what our source already used and already matched. The subtitle pass therefore hangs
   off `mDisplaySubtitles`, not `mIntroText` - which is why run 4 called the block "the
   `mIntroText` pass" and could not make it line up.
   **Do not trust a bit number you read off `rlwinm`**: mwcc's read and write encodings for this
   group disagree with a naive "SH 31 - n = bit n" reading (the *write* masks are 24..31 in
   declaration order while the *read* shifts are 25..31+). Compile the probe; it takes 5 seconds.
3. **`introX` has to be a named local.** With the ternary inline in the `Translate` call, mwcc
   evaluates `fn_80158DC4()` *first* and the bitfield test second; retail's order is the test
   first (0x80157B84), then the accessor (0x80157BA0). Naming the float restores retail's order.

**Why it is 88.52% and not 100%, measured:** retail builds the (176, -y) pair with two inline
`stw`s (`lis r0,17200 / stw r0,280(r1)` and `xoris r0,r3,32768 / stw r0,284(r1)`) and reads it
back with `lfd f0,280(r1) / fsubs f3,f0,f3`. We emit the same two `stw`s **and then a call to
`__ct__9CVector2fFff`** plus an `fneg` and a second `fsubs`, so we are one instruction longer in
two places and the frame is 288 instead of 336 - which shifts every other slot in the function
and is why `fdiff` still reports 116 differing instructions. The cause is a header property,
not a spelling: **`CVector2f(float, float)` is declared but not defined in
`include/Kyoto/Math/CVector2f.hpp:12`**, so mwcc cannot inline it and emits a call to
`__ct__9CVector2fFff` (`symbols.txt:12905`, 0x802CA564, 0xC bytes). Retail's DrawText inlines
the two stores. Making that constructor inline in the header would fix this function but it
changes codegen for every `CVector2f` user in the tree, so it is **not** an item-local change
and must not be done here.

Rejected this run, all measured, do not repeat: `const CVector2f pos` non-const (9 instrs, no
change); `(pos - CVector2f(176.f, 0.f)).GetX()` (worse, 84.25%); `pos.GetX() - lbl_8041C3A8` read
through an `extern "C"` retail float so the subtraction cannot be folded (9 instrs, byte-identical
- mwcc folds it anyway); the `CVector2f(...)` spelled inline in the argument list rather than as a
named local (same 88.52%, but it makes the code harder to read, so the named local is what is
committed).

### Verification

`./tools/decomp_build.sh` -> `All: 35.07% fuzzy, 28.76% matched, 12.90% linked (12401 / 28465
functions)`, no build or link errors. `sha1sum build/G2ME01/main.dol` = the pinned
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `./tools/goal_check.sh build/goal/item.json` ->
**PASS** (gate.sh green on every step it printed). `python3 tools/check_decl_order.py --unit
MetroidPrime/CWorldTransManager` -> `ok: none emits its functions out of retail order`.
`python3 tools/check_symbol_names.py` -> `checked 525 units; 0 declared names are missing`.
Only `src/MetroidPrime/CWorldTransManager.cpp` is touched (85 insertions, 12 deletions): no header
edit, no class-layout change, no `tools/`, `config/` or `asm` change, no commit.
`docs/HANDOFF.md` carries the two-line diff `gate.sh` writes itself (`matched 12399 -> 12401`,
`DOL units 10851 -> 10853`); left for the judge to rederive, not edited by hand. `flip_test` was
not run - this is a `progress` item and the unit stays `NonMatching`.

### For the next run, in priority order

1. `DrawAllModels` (99 instrs from exact, retail 316 vs our 217). The two missing pieces are both
   still unwritten: the `mGrappleModelData` render at `+0x100` with `mGrappleXf` (`+0x244`), and
   the `mLongShaft` block. **Correction to run 4 on the latter**: the flag is not at `+0x8C` of
   `CWorldTransManager`; that offset is inside `mDarkWorldInfo` (`+0x1C`, 0x70 bytes, so the byte
   at `+0x8C` is one past its end). The struct the block reads is `this->mDarkWorldInfo`, and the
   `DrawDarkWorldVolume` call (vtable slot `0x108` = index 64) does have its full signature in
   `IRenderer.hpp:188` (run 4 said 185, which is the comment above it). `CDarkWorldInfo`'s own member names for `+0x20, +0x28, +0x30, +0x38,
   +0x48, +0x54, +0x60, +0x64, +0x68` and the byte at `+0x6C` are still unknown - probing them
   with the `offsetof` trick in `.tmp/opencode/probe_wtm.cpp` is the cheap first step.
   Also still wrong in that function: `CActorLights`'s constructor is called with 4 arguments in
   our source and retail passes a float and five more bytes, and `BuildFakeLightList` gets
   `CColor(0.1f, 0.1f, 0.1f, 1.f)` from us and `CColor(0.f, 0.f, 0.f, 1.f)` from retail
   (`f1/f2/f3 = 0.0`, `f4 = 1.0` at 0x80159288-0x8015929C). Both are mechanical.
2. `TouchModels`, 0.04%: nine stack-slot operands, all one ordering question - see the slot table
   above. Not a wall; no spelling tried so far moves it.
3. `DrawText`, 11.5%: blocked on `CVector2f(float, float)` being out-of-line in this tree's
   header, which is a tree-wide codegen change and out of scope for this item.
4. `UpdateLights` (75.12%): run 4's `lbl_804191B8`/`lbl_804191BC` and
   `fn_80038D4C`/`fn_80045E18` findings are unchanged and untested.

**Not a wall for any of these** - each has a concrete next step, except `DrawText`, which is
blocked on a header property rather than on a spelling.

### Tooling written this run (under `.tmp/opencode/`, not part of the tree)

- `fdiff.py <fn-substring> [-l]` - retail object vs our object for one function, counting
  *differing instructions* after normalising branch targets, SDA displacements, frame size and
  callee-saved register choice; `-l` additionally drops stack-slot numbers. Same job as run 4's
  tool, rewritten because run 4's was in a lane worktree this tree does not have.
- `probe_rd.cpp` - compiles one `return t->mFlag;` per `bool : 1` in `CWorldTransManager` and
  reads the `rlwinm` shift out of the object. This is how the `mDisplaySubtitles` / `mIntroText`
  identification above was established.
- `probe_wtm.cpp` - the same `offsetof` trick for every member of `CWorldTransManager`, which is
  what showed `mBgOffset` at `0x90` and `mDarkWorldInfo` at `+0x1C`. **The header's member
  offsets are not what the declaration order suggests** (`mStrTable` is 0xC bytes, and
  `optional_object<T>` stores the object inline, not a pointer) - probe, do not compute.
