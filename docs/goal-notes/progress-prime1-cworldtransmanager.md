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
