# progress-unit-cinterpolationcamera

`kind: progress`, target `MetroidPrime/Cameras/CInterpolationCamera`.
Unit stays `NonMatching`. **6 -> 9 of 14 functions matched** (measured by `./tools/goal_check.sh
build/goal/item.json`: `target rose: main/MetroidPrime/Cameras/CInterpolationCamera: 6 -> 9 / 14
functions`; project `matched 11787 -> 11790`, `linked 5727 -> 5727`).

Re-measured first, as the brief asks: the `reason` figures were right. Baseline from
`./tools/decomp_build.sh MetroidPrime/Cameras/CInterpolationCamera` on the clean tree:

```
main/MetroidPrime/Cameras/CInterpolationCamera: 82.84% fuzzy, 3.45% matched (6 / 14 functions)
   __sinit_CInterpolationCamera_cpp                      79.04%  296 bytes
   __ct__20CInterpolationCameraF9TUniqueIdRC12CTransform4fii  98.17%  468 bytes
   CalculateOrientation__20CInterpolationCameraFfRC9CVector3fRbRC13CStateManager 73.27% 2424 bytes
   InterpolatePosition__20CInterpolationCameraFfR12CTransform4fRC9CVector3fRC13CStateManager 86.73% 488
   InterpolateSpline__20CInterpolationCameraFfR12CTransform4fRC9CVector3fRC13CStateManager 98.98% 236
   SetInterpolation__20CInterpolationCameraFRC12CTransform4f9TUniqueId9TUniqueIdb... 81.51% 696 bytes
   EndInterpolation__20CInterpolationCameraFQ220CInterpolationCamera10EEndReasonR13CStateManager 86.05% 392
   Think__20CInterpolationCameraFfR13CStateManager       92.01%  824 bytes
```

After: `85.53% fuzzy, 23.21% matched (9 / 14 functions)`; `All: 33.44% fuzzy, 26.50% matched
(11790 / 28465)`.

## What changed, per function

Per-function numbers are `objdiff report generate` (`fuzzy_match_percent`), which is what
`matched_functions` counts. The single-function `objdiff-cli diff` percentages below run ~1 point
lower because it compares relocation *targets*; a `lbl_8041CE18@sda21` vs `@1129@sda21` pool
reference is flagged `DIFF_ARG_MISMATCH` even though the bytes are identical. **The report rounds
those to 100**, so ignore `DIFF_ARG_MISMATCH` on pool constants and string literals.

### 1. `InterpolateSpline` 98.98% -> **100.00%** (236 bytes)

`GetControlPointCount()` returns `int`, so `== 0` compiled to `cmpwi`; retail has `cmplwi`.
Spelling: `if (mSpline.GetControlPointCount() == 0u)`. That is the whole function.

### 2. `__ct__20CInterpolationCameraF9TUniqueIdRC12CTransform4fii` 98.17% -> **100.00%** (468 bytes)

`rstl::string("Interpolation Camera")` calls `basic_string(const char*, int, allocator)` and costs
`li r5,-1` + an allocator local, which shifted the whole stack frame by 4 and made the object 8
bytes long. Retail calls `string_l__4rstlFPCc`, so it is `rstl::string_l("Interpolation Camera")` -
the same spelling `CFirstPersonCamera.cpp` and `CPathCamera.cpp` already use. Frame then matches
exactly (468/468).

### 3. `InterpolatePosition` 86.73% -> **100.00%** (488 bytes)

Two spellings, both measured:

* `if (limit < distance && ...)` -> `if (distance > limit && ...)`. Retail tests
  `fcmpo cr0, f1(distance), f29(limit); ble`; the other spelling gives `fcmpo cr0, f29, f1; bge` -
  the same predicate with the operands the other way round, which is a different instruction.
  86.73 -> 86.52. (`limit` is `mInitialDistance * remaining`, computed into `f29`.)
* Binding the argument: `const CVector3f position = target + delta;` and passing `position`. The
  inline `CalculateOrientation(dt, target + delta, ...)` allocates the argument temp at `0xc`, which
  collides with the slot retail gives `AsNormalized`'s sret; retail puts `position` at `0x18` and
  `AsNormalized`'s sret at `0xc`. Naming the local swaps the two allocations and the function goes
  to 100 with **no** remaining structural difference (488/488, only pool-constant arg mismatches).

### 4. `EndInterpolation` 86.05% -> 97.76% (392 bytes), and 5. `Think` 92.01% -> 96.93% (824 bytes)

Both were wrong about *which* accessor retail calls; see the shared finding below.

* `GetCameraManager(mgr)`, not `CameraManager(mgr)`. Retail's `EndInterpolation` calls
  `GetCameraManager__11CGameCameraCFRC13CStateManager`; `CameraManager(mgr)` compiles to
  `CameraManager__11CGameCameraCFR13CStateManager`, a different symbol. Since retail then calls the
  **non-const** `SetCurrentCameraId__14CCameraManagerF9TUniqueId` and
  `TransferCameraState__14CCameraManagerFR11CGameCameraR11CGameCameraR13CStateManager` on the
  result, `CGameCamera::GetCameraManager` must return a *mutable* reference. That is a header fix:
  `include/MetroidPrime/Cameras/CGameCamera.hpp` and its one-line body in
  `src/MetroidPrime/Cameras/CGameCamera.cpp` (a `const_cast` on the returned pointer, so the body is
  still the same 0x14 bytes). Checked: `CGameCamera` stays 25/35 and `CPathCamera` 10/16, and the
  gate's per-function diff reports nothing worse anywhere.
* `Player(mgr)`, not `GetPlayer(mgr)` - retail calls `Player__11CGameCameraCFR13CStateManager`.
* `if (state == kMS_Unmorphed || state == kMS_Unmorphing)` -> a `switch` over
  `Player(mgr).GetMorphballTransitionState()`. Retail's dispatch is
  `cmpwi r0,3; beq A; bge B; cmpwi r0,0; beq A; b B`, i.e. a two-case switch plus default, not an
  `||` chain.
* `cameraManager.FirstPersonCamera()` is called at **both** use sites in the unmorphed arm, not
  cached in a local - retail loads `lwz 24(r31)` twice, once inside the `reason == kER_Completed`
  branch and once after it.
* `Think`: `const CVector3f position = target->GetTransform().GetTranslation();` - retail reads
  `0x30/0x40/0x50(r28)`, which is `CTransform4f::m03/m13/m23` (the transform is at `+0x24`).
  `CActor::GetTranslation()` returns the *cached* `mPosition` at `0x54/0x58/0x5c`, a different
  offset. **Both spellings occur in retail**: `Think` uses the transform's, and its later
  `(target->GetTranslation() - xf.GetTranslation())` plus `InterpolatePosition`'s
  `GetTranslation()` use the cached `mPosition` (`0x54/0x58/0x5c`). 91.89 -> 93.86.
* `Think`: `bool done;` with an explicit `default: done = true; break;` instead of
  `bool done = true;` with no default. Retail materialises `li r29,1` inside the default arm at
  `0x1bd0`; the initialiser-before-the-switch spelling hoists it above the switch. 93.86 -> 96.81.
* `Think`: the `RayStaticIntersection` result goes into a named `const CRayCastResult` before
  `.IsValid()`. Retail calls `__ct__14CRayCastResultFRC14CRayCastResult`; the temporary form does
  not. 96.81.
* `SetInterpolation`: `CameraManager(mgr)` -> `GetCameraManager(mgr)`, same finding.

## What still blocks the remaining five

**A non-elided copy of a `CVector3f` temporary.** Retail's pattern, twice - `Think` at
`0x801b1c4c` and `SetInterpolation` at `0x801b20ac` - computes `a - b` into a 12-byte temp and then
copies it into the named local:

```
addi  r3, r1, 0x24          ; the named local's slot
...  fsubs f2,f0,f2 / fsubs f1,f1,f3 / fsubs f0,f0,f3
stfs  f2, 0x20(r1) ; stfs f1, 0x1c(r1) ; stfs f0, 0x18(r1)   <- temp
stfs  f0, 0x24(r1) ; stfs f1, 0x28(r1) ; stfs f2, 0x2c(r1)   <- copy into the local
bl Magnitude
```

Our MWCC 2.7 elides the copy in every spelling I tried (all measured, all the same 4 instructions
missing):

| spelling | `Think` |
| --- | --- |
| `(target->GetTranslation() - xf.GetTranslation()).Magnitude() > 3.f` | 93.86 (before the other two fixes) |
| `const CVector3f delta = ...; if (delta.Magnitude() > 3.f)` | 96.81 |
| `CVector3f delta; delta = ...; if (delta.Magnitude() > 3.f)` | 96.81 |
| `CVector3f delta = ...;` (non-const) | 96.81 |

Same for `SetInterpolation`'s `mInitialDistance` (81.33 either way). Note that MWCC *does* emit the
non-elided copy for `direction = direction.AsNormalized();` in the same function, so this is not a
general copy-elision setting - something about the `operator-` initialiser. **The next run should
try spellings that change how the temporary's address is taken**, e.g. binding a `const CVector3f&`
to the `operator-` result, or an explicit named temporary for the RHS (`xf.GetTranslation()`), which
is the one operand retail's `0x18` temp could be.

Also open, both register-allocation only:

* `Think` is 5 instructions from 100%: retail has one extra `fmr f1, f31` at the switch join
  (`0x801b1bd4`, dead - `ValidateCameraTransform` takes no float) and the 4 above.
* `EndInterpolation` is 97.76% and short 8 bytes: retail allocates `mgr` to `r30` and `reason` to
  `r29` (so it reloads `mr r5, r30` before each `SetCurrentCameraId`); ours allocates `mgr` to
  `r29`, `cameraManager` to `r30`, `reason` to `r31`. Same instructions, different registers.
* `SetInterpolation` (81.51%) additionally calls `ObjectById` where retail calls `GetObjectById`
  for `from`. Retail's `GetObjectById` therefore returns **non-const** `CEntity*` (it feeds
  `TCastToPtr<CGameCamera>(CEntity*)`), but our `CStateManager::GetObjectById` is declared
  `const CEntity*`, and `TCastToPtr` has no const overload. `TCastToConstPtr` is
  `TCastToPtr(const_cast<CEntity*>(p))`, so it emits the same call but yields a `const
  CGameCamera*`, which `TransferCameraState(CGameCamera&, ...)` rejects. I left `ObjectById` alone:
  changing `CStateManager::GetObjectById`'s return type is a ~40-call-site, whole-program change and
  the const-ness reaching MWCC's alias analysis could move unrelated matched functions.
  **A `progress` item on `MetroidPrime/CStateManager` to make `GetObjectById` return `CEntity*` is
  the honest fix**; it would then let `SetInterpolation` use it here.
* `__sinit_CInterpolationCamera_cpp` (79.04%, 296 bytes) is the anonymous-namespace initialiser:
  retail's `lwz r5, @62x@sda21` sequence has a different order and count of `.sdata2` loads than
  ours (`@624`..`@627` then `@620`, ours hoists two pairs into `r28`/`r29` earlier). That is a
  `CMaterialList`/`CMaterialFilter` static-initialiser ordering question, not a spelling of this
  file's code.

## Layout notes worth keeping

* `CActor` base fields: `mTransform` at `+0x24` (48 bytes), cached `mutable CVector3f mPosition` at
  `+0x54`, bitfield byte `+0x2a4`, `x2a0_` at `+0x2a0`. `CInterpolationCamera`: `mTargetId` `0x200`,
  `mTime` `0x204`, `mDuration` `0x208`, `mStartTransform` `0x20c`, `mLookPosition` `0x23c`,
  `mInitialDistance` `0x248`, `mInitialAngle` `0x24c`, `mAngularSpeed` `0x250`, `mPositionMode`
  `0x254`, `mRotationMode` `0x258`, `mSpline` `0x25c`.
* `CGameCamera::GetScanObjectIndicatorPosition` is vtable slot `+0x5c` and `SetActive` is `+0x1c`;
  retail really does dispatch them virtually through `lwz 0(r); lwz 0x5c(r12); mtctr; bctrl`.
* `rstl::string_l(...)` (not `rstl::string(...)`) is what retail's ctors call. `CBallCamera.cpp` and
  `CSpindleCamera.cpp` still use `rstl::string("...")` and are correspondingly short ~6 instructions
  in their constructors - free functions for a `progress` item on those units.

## Verification

`./tools/goal_check.sh build/goal/item.json` in `wt-mp2-goal-L8`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11787 -> 11790   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.44% fuzzy, 26.50% matched, 12.64% linked (11790 / 28465 functions)
  ok    target rose: main/MetroidPrime/Cameras/CInterpolationCamera: 6 -> 9 / 14 functions
  ok    no asm added
goal_check: PASS progress-unit-cinterpolationcamera
```

Touched: `src/MetroidPrime/Cameras/CInterpolationCamera.cpp`,
`src/MetroidPrime/Cameras/CGameCamera.cpp` (one line, the `GetCameraManager` body),
`include/MetroidPrime/Cameras/CGameCamera.hpp` (one declaration). No `configure.py`, `config/`,
`splits.txt` or `files.cmake` change; the unit is still `NonMatching`; `flip_test.sh` was not run
(it must not be, on a `progress` item).

`docs/HANDOFF.md` was rewritten by `gate.sh` when the judge ran (it writes the derived state block);
I reverted it with `git checkout --`, since the judge rewrites those counts from the tree itself.

NEW: unit-MetroidPrime-CStateManager | progress | MetroidPrime/CStateManager | `GetObjectById` must
return non-const `CEntity*` - retail feeds it straight to `TCastToPtr<CGameCamera>(CEntity*)`, our
header declares it `const CEntity*`, which forces every caller to pick between `ObjectById` (wrong
call symbol) and `TCastToConstPtr` (wrong pointer constness for `TransferCameraState`).
