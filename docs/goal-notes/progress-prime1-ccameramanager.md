# progress-prime1-ccameramanager

`kind: progress`, target `MetroidPrime/Cameras/CCameraManager` (DOL unit, stays `NonMatching`).

**Result: the unit's `matched_functions` went 19 -> 30 of 66.** Project-wide
`matched` 9737 -> 9748, `linked` 4894 -> 4894 (unchanged, as a `NonMatching` unit must be).
`MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/report.base.json` -> **GATE PASS**;
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`probe_sources.sh` = 744 files, 0 failed, **link: LINKED (250 undefined, 0 duplicates)** - back at
the baseline; `check_symbol_names.py` = 0 missing; `check_decl_order.py` = ok.

## Per function: before -> after, and whether Prime 1's source carried over

All numbers are `fuzzy_match_percent` for that function, from `build/report.json` (base vs now).
"Prime 1" means `prime-ref/src/MetroidPrime/Cameras/CCameraManager.cpp`; "measured" means the body
was recovered from the Echoes disassembly instead, because Echoes' `CCameraManager` is a fork with a
different member set (Prime 1 has an inline `mCineCameras` vector and inline shake list; Echoes has
`mCinematicCameraId`, `mCameraHintManager`, `mCameraShakeManager`).

| function | before | after | source |
|---|---|---|---|
| `SetWaterFogScale` | 67.900 | **100.000** | Prime 1 nearly unchanged; the condition must read the **member** just written |
| `ProcessInput` | 98.421 | **100.000** | Prime 1; needs a *signed* compare against `ControllerNumber()` |
| `ResetCameras` | 92.250 | **100.000** | Prime 1; needs two `GetPlayer` calls and direct-init from the temp |
| `GetCurrentCameraId` | 89.857 | **100.000** | Prime 1; needs `if`/`return`, not a ternary |
| `Update` | 75.000 | **100.000** | Prime 1 + measured (the two separate managers) |
| `GetCurrentCameraTransform` | 54.031 | **100.000** | measured: `camXform * CTransform4f::Translate(shakeOffset)` |
| `GetGlobalCameraTranslation` | 13.103 | **100.000** | measured: `camXform.Rotate(shakeOffset)` |
| `GetWaterFarDistance` | 3.684 | **100.000** | **Prime 1 verbatim**, once the tweak fields moved onto `CScriptWater` |
| `UpdateAudioListener` | 2.273 | **100.000** | **Prime 1 verbatim** |
| `ClearPathCamera` | 5.000 | **100.000** | measured: `SetActive(false)` + clear the script-camera id |
| `ClearSpindleCamera` | 5.000 | **100.000** | measured: same shape as ClearPathCamera |
| `fn_801ABD68` | 7.778 | 96.667 | measured: `IsInCinematicCamera()` then bit 31 of the cinematic camera's flags |

So: **5 of the 11 needed only Prime 1's structure**, and Prime 1's `GetWaterFarDistance` /
`UpdateAudioListener` were byte-for-byte correct once Echoes' member offsets were identified.
The other 6 were Echoes-only and had to be read out of the disassembly.

## The rules that actually moved the number (reusable, not GameCube-specific)

1. **A condition that tests a value just assigned must re-read the member, not the parameter.**
   Retail `SetWaterFogScale` stores `fogDensityTarget` and then does `lfs f1,124(r3)` -
   it reloads the member - before `lfs f0,116(r3)` and `fcmpo`. A C++ ternary on the *parameter*
   compiles to 28 bytes against retail's 40 (67.900%); the same ternary on
   `mFogDensityFactorTarget` compiles to 40. The reload is the only difference.
2. **An `int == uint` comparison gets the wrong compare instruction.** `cmplw` (logical) where
   retail has `cmpw` (signed) costs 1.5%; `static_cast<int>` on the `uint` side fixes it.
3. **Direct-initialise from a returned temporary to get the extra copy-constructor call.**
   `ResetCameras` needs retail's `CTransform4f xf(mgr.GetPlayer(i)->CreateTransform...())` - the
   copy-ctor at 0x801ACE0C is what materialises the temporary - and two *separate* `GetPlayer`
   calls. Hoisting the player into `const CPlayer& player = *mgr.GetPlayer(...)` reads 92.250%.
4. **A `?:` that yields a class type does not become the two `return`s retail has.**
   `GetCurrentCameraId`: writing `return mCinematicCamera ? ... : kInvalidUniqueId;` gives 89.857%
   (a 16-byte frame); `if (...) { if (cam) return ...; return kInvalidUniqueId; }` gives 100%
   (retail's 32-byte frame).
5. **A static factory call reproduces retail's out-of-line `Translate`/`Rotate` shape.**
   `CTransform4f::Translate(v)` and `xform.Rotate(v)` are `static`/out-of-line in this repo, so
   `a * CTransform4f::Translate(off)` produces exactly retail's three calls. A local
   `CTransform4f shake; shake.Translate(off);` does not (55.906% vs 100%).

## What was measured and did **not** work (so the next run skips it)

- `fn_801ABD68`, 96.667%, one instruction: retail has `rlwinm r3,r0,31,31,31` where every
  spelling of "test bit 31" gives `srwi r3,r0,31`. Tried: `& 0x80000000u` (96.667), `GetFlags() < 0`
  (30.778 - the compiler drops it, `uint` is never negative), `static_cast<int>` + `< 0` (96.667),
  `static_cast<int>` + `& INT_MIN` (96.667), `& (1u<<31)` (96.667), the ternary form (96.667).
  `rlwinm` with a one-bit mask is MWCC's bitfield idiom, so the real member is a 1-bit field at bit
  31 of that word, not a `uint` flag. Fixing it means splitting `CCinematicCamera::mFlags` into
  bitfields - a layout change to a **shared** unit, deliberately not done here.
- `SetupInterpolation`, 97.843% and unchanged. The only difference is register choice: retail
  reloads `mInterpCamera` into r6 and has a dead `mr r5,r30` before `SetCurrentCameraId`; ours
  reloads into r5. That is allocator state, not source.
- `StopCinematics`, first four lines reach **71.562%** and were **reverted on purpose**.
  `mFpCamera->SkipCinematic()` asks the port for `CFirstPersonCamera::SkipCinematic`, and
  `CFirstPersonCamera.cpp` is deliberately *not* in `files.cmake` - `tools/check_files_cmake.py`
  records that listing it takes the port 318 -> 328 (opens 10, closes 0). So the four lines buy 0
  matched functions and cost a new port gap entry, which `tools/gate.sh` rejects. The body's shape
  and the retail addresses are in the comment left in the source.
- `AddCamera`, 47.0% and untouched. Retail's loop caches the count in r5 with `mtctr`/`bdnz` and
  grows with `reserve(cap ? cap*2 : 4)`; our `rstl::vector::push_back` already has that growth rule
  but the compiler spills the vector's fields to a 48-byte frame instead of a 32-byte one. Not
  diagnosed beyond that.
- `GetLastCameraTransform` (64.158%) / `UpdateCameraHistory` (79.337%) share retail's `Size()`,
  which is `mulhw` by the 0x2AABAAAB magic and `mulli r4,r0,48` - an index arithmetic over a
  fixed-capacity ring, not the pointer-subtraction the current `SCameraHistory::Size()` does. Both
  also route through `fn_801AAE20`, retail's real `SCameraHistory::Last()` returning an
  `optional_object`. Both are rewrites, not tweaks.
- `GetCameraBobMagnitude` (2.979%) needs three float constants read from a pool at 0x804174BC and
  a **double** cos argument at `_SDA2_BASE_-22136`; `tools/sda.py` says `_SDA2_BASE_` is 0x804223C0
  but resolving `-22172` against it lands at 0x8041CD24, which is not in the `.sdata2` dump. The
  r2 base for this unit is not the one `sda.py` documents. **Unresolved - someone needs to measure
  the unit's actual r2 before this function is retried.**
- `IsBallCameraTransitioning` (3.784%) reads `CPlayer+0x1174 -> +0xC80` compared against 4 and 5,
  then `CPlayer+0x38C == 2 && CPlayer+0x390 == 1`. `CPlayer::mCameraState` is documented at 0x388,
  so 0x38C/0x390 are its neighbours and 0xC80 is behind an unknown sub-object pointer. Needs
  `CPlayer`'s own layout work.
- `UpdateCameraTriggers` (2.041%) iterates a `CObjectList` reached as `*(CObjectList**)0x848` off
  `CStateManager` - a direct member with no accessor, and the offsets do not resolve to a clean
  `EGameObjectList` index against the current `CStateManager` layout.

## Files touched

- `src/MetroidPrime/Cameras/CCameraManager.cpp` - 11 function bodies + the includes they need
  (`CCameraShakeManager`, `CHintManager`, `CPathCamera`, `CSpindleCamera`, `CFluidPlaneCPU`,
  `CPlayerState`, `CScriptWater`, `CAudioSys`, `CSfxManager`).
- `include/MetroidPrime/CCameraShakeManager.hpp` (new) - declaration only, **no recovered layout**.
  Needed because retail's `CCameraManager` calls through `mCameraShakeManager` at +0x88.
  `void Update(float, CStateManager&)` and `CVector3f GetShakeOffset(const CStateManager&) const`,
  read off 0x801E78D4 and 0x801E782C.
- `include/MetroidPrime/CHintManager.hpp` - one declaration, `void Update(float)` (0x801B9798,
  takes only `this` + f1).
- `include/MetroidPrime/Cameras/CSpindleCamera.hpp` - `Get/SetSpindleCameraId` accessors for the
  existing `mSpindleCameraId` at 0x200.
- `include/MetroidPrime/ScriptObjects/CScriptWater.hpp` - four `x310_..x31c_` accessors named as
  the water/gravity fog range+base pair `GetWaterFarDistance` reads, plus `GetFluidPlane()`.
  Guessed names; the *offsets* are what the measured diff shows.
- `src/MetroidPrime/PortGlobals.cpp` - PC-side stand-ins for the three new undefined symbols the
  DOL build now references. They announce themselves once if reached. **Without these the gate
  fails**: the port's undefined count went 250 -> 253.

## NEW

(none filed - nothing found here is a separate unit that can reach a count on its own; the
`CCameraShakeManager` and `CHintManager` bodies belong to units that do not exist in this tree yet,
and their retail addresses are recorded in `CCameraShakeManager.hpp` and the `PortGlobals.cpp`
comment so a later item can pick them up.)
