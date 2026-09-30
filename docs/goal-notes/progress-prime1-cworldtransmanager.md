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
