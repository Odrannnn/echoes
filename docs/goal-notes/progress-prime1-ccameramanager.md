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

---

# Second run (lane 7, 2026-09-30)

Re-measured on a fresh tree first: the unit stood at **30 / 66 matched, 25.778% fuzzy**, project
**10278 / 28465 matched**. Prime 1 was **not** used this time - the functions still open are Echoes
additions Prime 1 does not have (`Set/SetFixed/SetSurface/ClearFixed/ClearSurface`, the trigger and
history helpers), and reading them out of the disassembly was faster than adapting Prime 1.

**Result: the unit's `matched_functions` went 30 -> 31 of 66**; project `matched` 10278 -> 10279,
`linked` 5043 -> 5043 (unchanged, as a `NonMatching` unit must be).
`MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/report.base.json` -> **GATE PASS**;
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**;
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`probe_sources.sh` = 750 files, 0 failed, **link: LINKED (250 undefined, 0 duplicates)**;
`check_symbol_names.py` = 0 missing; `check_decl_order.py` = ok. Per-function diff: **+1 at 100%, 0
functions worse anywhere in the tree.**

## Per function: before -> after

| function | before | after | note |
|---|---|---|---|
| `ClearFixedCamera` | 7.692 | **100.000** | the whole body is the virtual `CGameCamera::SetActive(vtable+0x1C)` on the camera at +0x38 |
| `CinematicCut` | 2.778 | 94.111 | written, measured, then **reverted** - see below |

Nothing else was carried. Everything measured and not carried is listed below with its score, so the
next run does not re-derive it.

## The one rule that decided this run's shape: a new callee is a port gap

**Writing a body that calls a retail symbol with no port definition costs a gate failure, whatever
percent it reaches.** This is the concrete form of the warning in the first run's notes, and it is
what stopped three separate attempts:

- `CinematicCut` at **94.111%** needs `CBallCamera::TeleportCamera(CTransform4f const&, CStateManager&)`,
  `CGameCamera::GetFov() const` and
  `CGameCamera::InterpolateFOV(float,float,float,TUniqueId,CStateManager&)`. All three *have* source
  bodies in this tree (`CBallCamera.cpp` / `CGameCamera.cpp`), but those TUs are not in the port
  build, so the calls opened 3 undefined symbols: the port went **250 -> 253** and `gate.sh` failed
  `port link gap` with all three named as `NEW`. A 94% function that is not 100% is worth **zero**
  matched functions, so paying three new gaps for it is strictly negative. Reverted; the body is
  recorded as a comment above the function instead, and the exact source is in this table.
- `SetSpindleCamera` at **96.68%** (see the shape below) needs
  `TCastToPtr<20CScriptSpindleCamera>__FP7CEntity` at 0x80098F44, which lives in `TypesMatch.cpp` -
  in `configure.py` as `NonMatching`, deliberately **not** in `files.cmake` (it cannot build for a
  host; see the comment block there). Same trade, same answer. Reverted.
- `SetPathCamera` needs `TCastToPtr<17CScriptPathCamera>__FP7CEntity` at 0x8009952C. Same blocker.

**The generalisable half:** *before writing a camera body, check whether each callee is already in
the port's link.* `tools/link_check.sh`'s baseline (`docs/research/port_link_baseline.txt`) lists the
250 by name; anything absent there is a gap you are about to open. A function whose score is already
below 100% and whose missing callees are gaps can never reach 100% here, whatever the spelling.

## Measured and reverted, so the next run skips the spellings

- **`CinematicCut`, 94.111%, 5 instructions out.** Retail 0x801AB9DC:
  ```
  if (IsInCinematicCamera()) {
    mBallCamera->TeleportCamera(mCinematicCamera->GetTransform(), mgr);   // mCinematicCamera+0x24
    mBallCamera->InterpolateFOV(mCinematicCamera->GetFov(), 2.f, 0.0011920929f,
                                mBallCamera->GetUniqueId(), mgr);
    StopCinematics(mgr);
  }
  ```
  Echoes drops Prime 1's trailing `SetCurrentCameraId(mBallCamera->GetUniqueId())`; the duration is
  `lfs f2,-22172(r2)` = 2.0f and the delay `lfs f3,-22180(r2)` = 0x3A9C4000 = 1250 * 2^-20 =
  0.0011920928955078125f, both read out of `.sdata2` with `_SDA2_BASE_ = 0x804223C0` (**the base
  `tools/sda.py` documents is correct** - the first run's note claiming the r2 base was wrong does
  not reproduce; resolving `-22172` against it lands in `.sdata2` exactly).
  The residual is `GetFov()`'s by-value TUniqueId temp: retail reloads `mBallCamera` into **r4** and
  stores the dead temp at `12(r1)`; ours keeps it in **r3** and dead-stores at `8(r1)`. That is
  allocator state, not source. Tried and measured: hoisting `GetUniqueId()` into a named local
  **before** the `TeleportCamera` call -> **82.78%** (worse - it moves the reload). Do not retry.
- **`SetSpindleCamera`, 96.68%**, and by inspection **`SetPathCamera`** (identical apart from the
  two callees). Retail 0x801AB794 / 0x801AB8DC:
  ```
  if (!mSpindleCamera->GetActive() || mSpindleCamera->GetSpindleCameraId() != uid)
    if (TCastToPtr<CScriptSpindleCamera>(mgr.ObjectById(uid))) {
      mSpindleCamera->SetActive(true); mSpindleCamera->SetSpindleCameraId(uid);
      mSpindleCamera->Reset(GetCurrentCameraTransform(mgr, false), mgr);
      UpdateCameraTriggers(mSpindleCamera->GetUniqueId(), mgr);
    }
  ```
  Three things worth keeping, because they are **not** in the header and are the whole difference
  from 1.6% to 96.7%:
  1. The flag tested is **`CEntity::GetActive()`**, i.e. `m_active` - `lbz r0,32(r3)` +
     `rlwinm. r0,r0,25,31,31`, bit 7 of the byte at +0x20. Not `m_entityUnknown`/`GetEditorFlag2()`
     as one might guess: reading that instead gives `clrlwi. r0,r0,31` (bit 0), which is a different
     instruction and dropped the score to **92.48%**.
  2. There is **no null check** on `mSpindleCamera` - `lwz r3,44(r3)` then straight to the load.
     Adding `mSpindleCamera &&` costs the two leading `cmplwi/beq` (that spelling measured 92.48%
     for the same reason as the flag).
  3. `GetScriptCameraId` is read from **+0x200** (`lhz r3,512(r3)`) - the same word `ClearSpindleCamera`
     writes - so `GetSpindleCameraId()` is right, not a separate field.
  The residual is one instruction: retail emits a **second, unreachable `beq` to the epilogue** on
  the same `rlwinm.` condition (`beq 0x...7dc` / `beq 0x...870` back to back), ours emits one. Tried:
  `!(A && B)` De Morgan form (96.68%, identical codegen), `!(B == uid)` (96.68%, identical), the
  `?:` form `A ? B != uid : true` (**79.74%** - it forces `neg/or/srwi`), and hoisting the predicate
  into `const bool drive` (**87.00%**). Only the last two changed the codegen, both for the worse.
  **Do not retry those four.** The `||`-with-two-branches shape is what the `Set*` trio share, and
  `SetFixedCamera` has the same skeleton with the id at **+0x20C** and no `GetCurrentCameraTransform`
  call (it takes the transform as a parameter).
- **`SetFixedCamera`, 0.82% -> untouched.** Retail 0x801AB674, 208 bytes; the measured skeleton is in
  the comment above it. Its id setter is **out of line at 0x80228910** (`sth r0,524(r3)`), an
  unclaimed range, so it is a fresh port symbol.
- **`ClearSurfaceCamera`, 4.76% -> untouched.** Retail 0x801AB4E8 is `SetActive(false)` then the
  **out-of-line** `SetScriptCameraId` at **0x801E95A8** (`sth r0,512(r3)`), unlike
  `ClearPathCamera`/`ClearSpindleCamera` whose stores are inline. No `CSurfaceCamera` unit in
  `splits.txt`, so it needs a header like `CFixedCamera`'s **and** a definition for that callee.
- **`SetSurfaceCamera`, 1.54% -> untouched.** Same family; not disassembled this run.

## Files touched

- `include/MetroidPrime/Cameras/CFixedCamera.hpp` (**new**) - `class CFixedCamera : public CGameCamera {}`,
  **no recovered layout**, mirroring the `CCameraShakeManager.hpp` pattern from the first run. Retail
  touches the fixed camera only through the virtual `SetActive` and the out-of-line id setter at
  0x80228910, so an empty subclass is the whole declaration `ClearFixedCamera` needs. Without it,
  `mFixedCamera->SetActive(false)` does not compile (MWCC: *illegal use of incomplete struct/union/
  class 'CFixedCamera'*) - `static_cast<CGameCamera*>(mFixedCamera)->SetActive(false)` compiles but
  is a cast the retail source does not have; the real declaration is better.
- `src/MetroidPrime/Cameras/CCameraManager.cpp` - `ClearFixedCamera`'s body (the one function that
  now matches), the include, and **measured comments** on `CinematicCut`, `SetPathCamera`,
  `SetSpindleCamera`, `SetFixedCamera` and `ClearSurfaceCamera` so the addresses and shapes above do
  not have to be re-derived.
- `docs/HANDOFF.md` - rewritten by `tools/gate.sh` (it owns that file); not hand-edited.

---

# Third run (lane 5, 2026-09-30)

Re-measured first: the unit stood at **31 / 66 matched, 26.135% fuzzy**, project **10485 / 28465**
(`build/goal/judge/report.base.json`). Run 2's claims that do **not** reproduce on this tree:
`SetSpindleCamera` is **1.61%** not 96.68%, `SetPathCamera` **1.56%**, `CinematicCut` **2.78%**
(both reverted spellings, so the tree is as it was) - but `ClearFixedCamera` **is** at 100% and
`AddCamera` 46.96%, `SetupInterpolation` 97.84%, `fn_801ABD68` 96.67%, `GetLastCameraTransform`
64.16%, `UpdateCameraHistory` 79.34% all reproduce.

**Result: the unit's `matched_functions` went 31 -> 32 of 66** (fuzzy 26.135% -> 27.474%);
project `matched` 10485 -> 10486, `linked` 5051 -> 5051 (unchanged, as a `NonMatching` unit must be).
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**.
`build/gate-diff.log`: `matched 10485 -> 10486 linked 5051 -> 5051 (+1 functions at 100%, 0 units
newly linked)` / ` +100%  main/MetroidPrime/Cameras/CCameraManager :: fn_801AB298` / **`no regression`**.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`probe_sources.sh` = 754 files, 0 failed, **link: LINKED (250 undefined, 0 duplicates)**;
`check_symbol_names.py` = 0 missing; `check_decl_order.py --unit MetroidPrime/Cameras/CCameraManager`
= ok. Only changed paths are the two below plus `docs/HANDOFF.md`, which `gate.sh` owns.

## The one function matched: `fn_801AB298` (0.00% -> 100.000%), 180 B

Retail's `SCameraHistory::Push`. Two separate problems, one naming and one codegen.

**1. The name, not the code, was scoring 0.00%.** Retail emits this body as `fn_801AB298`
(`symbols.txt:6976`), and our object emitted it as
`Push__Q214CCameraManager14SCameraHistoryFRC12CTransform4f`. **objdiff pairs functions by name, so a
correct member function scores 0.00% against a retail placeholder** - and 1257 `fn_`-named
functions in this tree already score 100% by being *named*, so this is the repo's own convention,
not a trick. The mechanism is `extern "C"` on a free function: `src/MetroidPrime/main.cpp:2172`
(`extern "C" void fn_80009864()`), `src/MetroidPrime/main.cpp:1918`. So:

```cpp
extern "C" void fn_801AB298(CCameraManager::SCameraHistory* self, const CTransform4f& xf) { ... }
```

and the two in-tree callers call `fn_801AB298(&mCameraHistory, xf)`. `SCameraHistory` had to move
from `private:` to public so the free function can name it; the nested type is *defined* there and
the data members keep their order, so no offset moves (`CHECK_SIZEOF(CCameraManager, 0xfa8)` still
holds and every other function in the unit is byte-identical).

**2. `const bool full = mBegin == mEnd; *mEnd++ = xf;` cannot produce retail's compare.** Measured:
that spelling gives `subf r5,r5,r3; cntlzw r0,r5; srwi r30,r0,5` (a branchless bool) plus the
`++mEnd` hoisted *before* the copy-constructor call, 168 bytes. Retail branches on the compare
*first*, `cmplw r0,r3; bne; li r30,1`, and assigns then increments. Giving MWCC a branch context
and splitting `*mEnd++ = xf` into `*mEnd = xf; ++mEnd;` gives exactly that:

```cpp
  bool full = false;
  if (self->mBegin == self->mEnd) { full = true; }
  *self->mEnd = xf;
  ++self->mEnd;
  if (self->mEnd == self->mTransforms.end()) { self->mEnd = self->mTransforms.begin(); }
  if (full) { ++self->mBegin; if (self->mBegin == self->mTransforms.end()) { self->mBegin = self->mTransforms.begin(); } }
```

**Byte-identical to retail's 180.** The reusable rule: *`const bool x = (a == b)` makes mwcceppc
materialise the bool arithmetically (`subf`/`cntlzw`/`srwi`); `if (a == b) x = true;` gives it a
branch to emit (`cmplw`/`bne`). Same for `p++` vs `p = p + 1` next to a call - splitting the two
statements is what stops the store being hoisted above the call.*

## Measured and NOT carried, so the next run skips it

- **`fn_801AAE20` (264 B) and its two callers - a wall, and it is four functions, not three.**
  Run 1 recorded that "`Size()` is `mulhw` by the 0x2AABAAAB magic and `mulli r4,r0,48`, an index
  arithmetic over a fixed-capacity ring, not the pointer-subtraction the current
  `SCameraHistory::Size()` does." **That does not reproduce and is wrong.** Retail's `Size()` is
  ordinary pointer subtraction - `lis r3,0x2AAB; addi r3,r3,-21845` is just MWCC's signed-divide-
  by-48 sequence for `T*` arithmetic on a 48-byte `CTransform4f`, and `if (mBegin == mEnd) return
  mCount` uses `mCount`, not a constant. Our `Size()` is already right and needs no rewrite.
  What is actually missing is in `Last()` itself. Retail's two paths call the copy constructor
  **with no null test**; mwcceppc 2.7 wraps every `new (dest) T(src)` in `rstl::construct_impl`
  in a `cmpwi r?,0; beq` on `operator new`'s result (documented at
  `include/Collision/CCollisionInfo.hpp:64`). Our split-into-two-returns rewrite gets the rest
  right - retail's `(mCount-1)*48` indexing, two straight-line paths, 280 bytes against 264 - and
  the 16 bytes left are exactly the two `cmpwi`/`beq` pairs. **There is no way to run a copy
  constructor at an arbitrary address in C++ without placement new, and the copy constructor has
  to be called, so this cannot be closed from our headers.** It also blocks `fn_801AD79C` (the
  history constructor's 80-iteration fill loop: retail's loop body is `mr r3; mr r4; bl`, ours is
  `cmplwi r28,0; beq; mr r3; mr r4; bl`) and `fn_801AD824`/`fn_801AD8DC`, which are the same
  construct helper. So: **four 0.00% functions in this unit are all blocked by one null test in
  `rstl/construct.hpp`.** That is a shared header; do not change it casually.
- **`SetupInterpolation`, 97.84% -> unchanged; hoisting the id is WORSE.** The residual is two
  instructions: retail reloads `mInterpCamera` into **r6** and has a dead `mr r5,r30` (r30 is
  `mgr`) before `SetCurrentCameraId`, ours reloads into **r5**. Tried this run: naming the id
  (`const TUniqueId uid = mInterpCamera->GetUniqueId(); SetCurrentCameraId(uid);`) -> **97.71%**,
  worse: it makes MWCC reuse `SetInterpolation`'s 28(r1) temp (`sth r0,28(r1)` appears) and pass
  16(r1) instead of 20(r1). The original spelling already emits retail's two stores
  `sth r0,16(r1); sth r0,20(r1)` and `addi r4,r1,20`. Do not retry the named-local spelling.
  Also measured here: `SetCurrentCameraId(TUniqueId uid)` compiles to `lhz r0,0(r4); sth r0,20(r3)`
  - MWCC passes this 16-bit typedef **by const reference**, which is why retail's caller can pass a
  stack address.
- **`SetCinematicPaused`, 97.14% - unchanged, and it is not the local's fault.** One register:
  retail loads `mCinematicCamera` into **r5**, ours into **r3** (`lwz r3,48(r3)` reuses `this`'s
  dead register). Tried: `CCinematicCamera* camera = mCinematicCamera; if (camera) camera->...` and
  `if (CCinematicCamera* camera = mCinematicCamera) camera->...` - **both give identical r3
  codegen, 97.14%**. A named local is not what moves it; do not retry those two.
- **`fn_801ABD68`, 96.67% - still blocked, but the note about what would fix it is wrong.**
  Retail `lwz r0,532(r3); rlwinm r3,r0,31,31,31`, ours `srwi r3,r0,31`. Run 1 said the fix is to
  split `CCinematicCamera::mFlags` into bitfields. Not so: **`AddCinemaCamera` (0x801AC1C4) copies
  the whole word** - `stw r0,532(r7)` where `r0 = *(this+0x314)` - so it is a **16-bit flag pair**,
  not a 31-bit mask: `0x8000` (bit 31 of the word) and `0x7FFF`. A bitfield
  `{ uint x : 15; bool y : 1; uint z : 16; }` cannot carry that, and no spelling of
  `& 0x80000000` reaches 100% (run 1 measured six). Flag names and meaning unresolved.
- Not re-tried (unchanged from run 2, still the same blockers): `StopCinematics` 2.08% (needs
  `CFirstPersonCamera::SkipCinematic`, not in the port build), `CinematicCut` 2.78%,
  `SetSpindleCamera` 1.61% / `SetPathCamera` 1.56% (need `TCastToPtr<...>` from `TypesMatch.cpp`),
  `SetFixedCamera` 1.92% / `ClearSurfaceCamera` 4.76% (need out-of-line id setters in unclaimed
  ranges), `AddCamera` 46.96%, `UpdateCameraTriggers` 2.04% and the other trigger helpers,
  `GetCameraBobMagnitude` 2.98%, `IsBallCameraTransitioning` 3.78%, `UpdateFilters` 0.39%,
  `CreateCameras` 0.26%, `Reset` 0.82%, `CheckSplineCollision` 0.31%. Run 2's "`AddCinemaCamera`"
  note is corrected by this list: it is **0.77%** and untouched, not measured again here.

## Files touched

- `src/MetroidPrime/Cameras/CCameraManager.cpp` - `SCameraHistory::Push` -> `fn_801AB298`
  (`extern "C"`, ~line 374), the two call sites in `UpdateCameraHistory`, the `Last()` split, and
  the measured comments.
- `include/MetroidPrime/CCameraManager.hpp` - `SCameraHistory` moved above `private:` with the
  reason; its `Push` declaration removed.
- `docs/HANDOFF.md` - rewritten by `tools/gate.sh` (it owns that file); not hand-edited.

## NEW

(none filed. The placement-new null test is a measured wall inside a shared header, not a unit
that can be claimed; the flag word at `CCinematicCamera+0x214` needs a name that no source in
this tree has yet.)

---

# Fourth run (lane 5, 2026-09-30)

Re-measured first on a fresh tree: the unit stood at **32 / 66 matched, 27.474% fuzzy**, project
**11222 / 28465** (`build/goal/judge/report.base.json`). Run 3's numbers all reproduce
(`ClearFixedCamera` 100%, `fn_801AB298` 100%, `fn_801ABD68` 96.67%, `AddCamera` 46.96%,
`SetupInterpolation` 97.84%, `GetLastCameraTransform` 64.16%, `UpdateCameraHistory` 79.34%).

**Result: the unit's `matched_functions` went 32 -> 33 of 66** (fuzzy 27.474% -> 28.831%);
project `matched` 11222 -> 11223, `linked` 5507 -> 5507 (unchanged, as a `NonMatching` unit must be).
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**.
`build/gate-diff.log`: `matched 11222 -> 11223 linked 5507 -> 5507 (+1 functions at 100%, 0 units
newly linked)` / ` +100%  main/MetroidPrime/Cameras/CCameraManager :: GetCameraBobMagnitude__14CCameraManagerCFv`
/ **`no regression`**. `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
Only hand-edited path is `src/MetroidPrime/Cameras/CCameraManager.cpp`; `docs/HANDOFF.md` is
rewritten by `tools/gate.sh` (it owns that file).

## The one function matched: `GetCameraBobMagnitude` (2.979% -> 100.000%), 188 B

Prime 1's source carried over **almost** unchanged - Echoes differs in exactly one cast, and that
cast is the whole function.

```cpp
  const float dot = CMath::AbsF(CMath::Limit(
      CVector3f::Dot(mFpCamera->GetTransform().GetForward(), CVector3f::Up()), 1.f));
  const float pitch = CMath::Limit(dot / static_cast< float >(cos(M_PIF / 6.f)), 1.f);
  return 1.f - pitch;
```

**The one thing Echoes changed from Prime 1 is `cosf` -> `cos`.** Prime 1 line 888 reads
`cosf(M_PIF / 6.f)`; retail 0x801AB498 calls `bl 80352578 <cos>`, the **double** function
(`cosf` is a different symbol, 0x80352F14). That alone measures **95.43%** - one instruction out:
ours emitted `fdiv f2,f2,f1` + `frsp f2,f2` (a double divide narrowed afterwards) where retail has
`frsp f2,f1; fdivs f2,f1,f2` (narrow the `cos` result, then divide in float). Adding the
`static_cast<float>` on the `cos` result is worth the remaining 4.57% and makes it byte-identical.

The constant confirms the promotion: the `lfd f1,-22136(r2)` argument is the 8 bytes at
`_SDA2_BASE_-22136` = **0x8041CD48** = `0x3FE0C15240000000` = **0.5235987901687622**, which is
`(float)(pi/6)` widened, *not* `pi/6` = 0.5235987755982988. So the argument really is
`M_PIF / 6.f` (a float division) widened for the double call, exactly as written above. **Run 1's
"the r2 base for this unit is not the one sda.py documents" is wrong and was the only reason this
function was skipped twice** - `tools/sda.py s2:-22136` resolves it, and `s2:` is the prefix that
picks `_SDA2_BASE_`; run 1 called the tool **without** it, so it resolved against `_SDA_BASE_` and
gave 0x8041A6E4. Always use `s2:` for an `r2` displacement.

Also resolved, and cheap to re-derive: the three pool reads are `CVector3f::sUpVector` (0x804174BC,
declared in `include/Kyoto/Math/CUnitVector3f.hpp:39` as an inline returning the static) and the
three transform reads at `fpCam+0x28/+0x38/+0x48` are `m01`/`m11`/`m21`, i.e.
`GetTransform().GetForward()`. `GetCameraBobMagnitude` therefore needs **no new callee**: `Up()` and
`Dot` are inline, `cos` is already in the port's link (it is `Runtime/s_cos.c`, claimed by
`config/G2ME01/splits.txt`), and the port's undefined count did not move. This is the shape of
function to look for first after two failed runs: self-contained, Prime 1 has it, and every callee
already resolves.

## Measured this run and NOT carried, so the next run skips them

- **`fn_801ABD68`, 96.67%, still one instruction, and the mechanism is now decoded.** Retail 0x801ABD90
  is `lwz r0,532(r3); rlwinm r3,r0,31,31,31`. **SH=31 is a rotate-right-by-1 with a 1-bit mask, so
  the flag is bit 0 of the word, not bit 31** - every run up to now read bit 31 and so could not
  match. `& 1u` is the right mask and gives `clrlwi r3,r0,31`, still not `rlwinm` (96.67%). The
  `rlwinm` is only emitted for a genuine 1-bit **bitfield**, so tried, both measured:
  - bitfield read through a member function over `CCinematicCamera::mFlags`, `{ bool flag0 : 1; uint rest : 31; }`
    -> `lbz r0,532(r3); rlwinm r3,r0,25,31,31`, **96.389%**. MWCC emits `lbz` and masks **bit 7 of
    the byte**, i.e. it is reading MSB-first out of the *low* byte.
  - the same with the order reversed, `{ uint rest : 31; bool flag0 : 1; }` -> `lbz r0,535(r3);
    clrlwi r3,r0,31`, **93.33%** - the compiler moved the field to the *next* byte (+3) instead.
  - a local copy of the word plus a local bitfield struct -> 32-byte frame, **65.50%**, and
    `GetFlags()` returns by value so `&GetFlags()` is not an lvalue (compile error, line 247).
  **Do not retry these three.** Getting the exact `rlwinm` needs the bitfield declared as a real
  member of `CCinematicCamera` (so MWCC allocates the storage), which is a layout change to a
  **shared** unit - deliberately not done. Note run 3's `AddCinemaCamera` observation that the word
  is copied whole (`stw r0,532(r7)`) is consistent with `mFlags` staying a plain `uint`; a bitfield
  would have to be `uint x : 15; bool y : 1; uint z : 16;` to survive that copy, and the flag's
  name and meaning are still unknown.
- **`SetPlayerCamera` (320 B, 1.25%) - disassembled, not written, and it has the same port-gap
  blocker as `SetSpindleCamera`.** Retail 0x801ABB38: `mInterpCamera->GetActive()` (byte +0x20
  bit 7, `rlwinm. r0,r0,25,31,31`); then `GetObjectById(uid)` (0x80041998) +
  **`TCastToPtr<11CGameCamera>__FP7CEntity` (0x8009A8DC)** + that camera's `GetActive()`; if both
  active, `SetCurrentCameraId(uid)`. Otherwise it reads `mgr + *(this)*4 + 5372` (i.e.
  `mgr.GetPlayer(mPlayerIndex)`), compares `player+0x38C` against **3 then 0**, and picks
  `this+0x18` (`mFpCamera`) when the state is 0 or 3 and `this+0x1C` (`mBallCamera`) otherwise,
  taking each one's `GetUniqueId()` at +0x8. Then unconditionally
  `UpdateCameraTriggers(GetCurrentCameraId(false), mgr)` and a `mInterpCamera->SetActive(false)`
  through vtable+0x1C. The `TCastToPtr<CGameCamera>` is in `TypesMatch.cpp`, which is **not in the
  port build** - so this is a fresh port gap, exactly like run 2's `SetSpindleCamera` verdict.
  The measured state offsets to reuse: `CPlayer+0x38C` (a *signed* compare against 0 and 3, which
  is why run 1's `IsBallCameraTransitioning` note reads `+0x38C == 2` - different field),
  `this+0x18` = `mFpCamera`, `this+0x1C` = `mBallCamera`, camera id at camera+0x8.

## Files touched

- `src/MetroidPrime/Cameras/CCameraManager.cpp` - `GetCameraBobMagnitude`'s body, the two includes
  it needs (`Kyoto/Math/CMath.hpp`, `Kyoto/Math/CUnitVector3f.hpp` - the latter is where `Up()` is
  actually *defined*, in the header, not in `CVector3f.cpp`), `fn_801ABD68`'s mask corrected to bit 0
  with the measured reason, and the `SetPlayerCamera` skeleton as a comment.
- `include/MetroidPrime/Cameras/CCinematicCamera.hpp` - **touched and reverted**; the bitfield
  accessor added during the experiment is gone and the file is byte-identical to HEAD.
- `docs/HANDOFF.md` - rewritten by `tools/gate.sh` (it owns that file); not hand-edited.

## Reusable rules this run added

1. **`tools/sda.py` needs the `s2:` prefix for an `r2` displacement** (`s2:-22136` -> `_SDA2_BASE_`).
   Without it the tool resolves against `_SDA_BASE_` and returns a plausible wrong address - which
   is what made run 1 record a false "wrong r2 base" blocker and cost two runs this function.
2. **A retail `lfd` of a constant that is `(float)x` widened, not `x`, tells you the argument was
   computed in float.** `0.3FE0C15240000000` = 0.5235987901687622 = `(float)(pi/6)`, so the source
   divides in float and lets the call widen it. Decoding the *exact* double is how the `cos` vs
   `cosf` choice and the required `static_cast<float>` on the result were both pinned down.
3. **When the float and double versions of a libm function are both retail symbols, `bl <name>` in
   the disassembly names the one that is called** - `cos` at 0x80352578 vs `cosf` at 0x80352F14.
   Prime 1's spelling is not evidence; the call target is.
4. **MWCC lays bitfields out MSB-first from the low byte.** A `bool x : 1` on a word gives
   `lbz` + `rlwinm 25,31,31` (bit 7 of byte 0), not `rlwinm 31,31,31`. This makes the bitfield route
   to retail's `rlwinm` in `fn_801ABD68` a dead end without a real storage member.
5. **Look for the next function with no new callees first.** After two runs of port-gap dead ends
   (`TCastToPtr`, `CFirstPersonCamera::SkipCinematic`), the whole remaining list was re-screened
   against `docs/research/port_link_baseline.txt`: of the 34 unmatched functions, `GetCameraBobMagnitude`
   was the only one that needed nothing new. `CMath::Limit`/`AbsF`/`Dot`/`Up` are all inline and
   `cos` is already in the port link.

## NEW

(none filed. The `rlwinm` bit in `fn_801ABD68` is a measured wall needing a layout change to a
shared unit, not a claimable target; `SetPlayerCamera` is blocked by `TCastToPtr<CGameCamera>` in
`TypesMatch.cpp`, which is deliberately out of the port build and so cannot raise a count here.)

---

# Fifth run (lane 5, 2026-09-30)

Re-measured first on a fresh tree: the unit stood at **33 / 66 matched, 28.831% fuzzy**, project
**11254 / 28465** (`build/goal/judge/report.base.json`). Run 4's numbers all reproduce
(`GetCameraBobMagnitude` 100%, `fn_801AB298` 100%, `fn_801ABD68` 96.67%, `AddCamera` 46.96%,
`SetupInterpolation` 97.84%, `SetCinematicPaused` 97.14%).

**Result: the unit's `matched_functions` went 33 -> 34 of 66** (fuzzy 28.831% -> 31.189%);
project `matched` 11254 -> 11255, `linked` 5507 -> 5507 (unchanged, as a `NonMatching` unit must
be). `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`probe_sources.sh` = 751 files, 0 failed, **link: LINKED (244 undefined, 0 duplicates)** - down
from 250; `check_symbol_names.py` = 0 missing; `check_decl_order.py --unit
MetroidPrime/Cameras/CCameraManager` = ok.

## The finding this run is built on: 7 retail functions had NO counterpart in our object at all

`build/report.json` lists a unit's functions from **both** sides. A function with **no
`fuzzy_match_percent` key** is *retail-only*: our object never emitted anything objdiff could pair
with it. All four earlier runs treated those seven as "0.00%, blocked" and moved on. Two of them
(`fn_801AD79C`, `fn_801AAC28`) were simply **never written**; they are not walls.

| function | bytes | what it is | before | after |
|---|---|---|---|---|
| `fn_801AAC28` | 132 | `rstl::vector<CTransform4f>::~vector` COMDAT | **unpaired** | **100.000%** |
| `fn_801AD79C` | 136 | `SCameraHistory`'s out-of-line copy-constructor COMDAT | **unpaired** | 96.765% |
| `fn_801AAC08` | 32 | `rstl::construct_impl<CRayCastResult>` | unpaired | not attempted |
| `fn_801AABD0` | 56 | `rstl::vector<CTransform4f>::emplace_back` | unpaired | not attempted |
| `fn_801AAE20` | 264 | `SCameraHistory::Last()` | unpaired | not attempted |
| `fn_801AD824` | 184 | `rstl::vector<CTransform4f>::reserve` | unpaired | not attempted |
| `fn_801AD8DC` | 104 | `rstl::vector<CTransform4f>::uninitialized_copy_n` | unpaired | not attempted |

**Reusable rule: a target-only function is not a wall, it is an unwritten function.** The earlier
note "four 0.00% functions in this unit are all blocked by one null test in `rstl/construct.hpp`"
was about the *bytes*; it did not establish that we could not emit the symbols at all. Sorting a
unit's functions by "has a `fuzzy_match_percent` key" is a work list no report summary shows.

## What made them pair (the run-3 `fn_801AB298` mechanism, applied twice)

Both are ordinary member functions of a class template or of a nested struct. Written as members
they are emitted under a mangled name that pairs with nothing, which is exactly why they showed up
unpaired. Each is defined as a free `extern "C"` function named as retail names it:

```cpp
extern "C" rstl::vector< CTransform4f >* fn_801AAC28(rstl::vector< CTransform4f >* self, int flag);
```

`fn_801AAC28`'s body is retail's, read off 0x801AAC28: a `self != nullptr` guard, `destroy(begin,
end())`, `deallocate(mItems)`, then `if (sign-extended 16-bit flag > 0) CMemory::Free(self)`, and
it **returns `self`** (retail's epilogue ends `mr r3,r30`). Three details each measured:

- **`return self;` is worth 4.55%** (96.97 -> 100.000). It looks like dead code; it is the value
  retail leaves in r3 on *both* paths, and without it the early-return path is 3 instructions
  short. Try it before concluding a guard is the whole diff.
- **`static_cast< short >(flag) > 0`**, not `flag > 0`: retail has `extsh. r0,r31`, so the
  parameter is a 16-bit value that MWCC sign-extends into the compare. `int` alone gives `cmpwi`
  and costs 3.03%.
- **the `if (self != nullptr)` wrapper must not be hoisted**; it is retail's `mr. r30,r3; beq` at
  the top and skips the whole body.

`fn_801AD79C` reaches 96.765% and not 100%. The body is `mCount = 80`, 80 copy-constructor calls,
`mBegin = data()`, `mEnd = mBegin + 1`, and getting the fill to compile as retail's needs the same
`construct_impl` trick (below). The last **one instruction** is allocator state - retail reloads
`mBegin` into **r4**, ours into **r3** - and no spelling moved it (see the wall below).

## The reusable rule this run added: an out-of-line `construct_impl` kills the placement-new null test

mwcceppc 2.7 expands `new (dest) T(src)` into "call `operator new`, **test the result against
null**, then construct", and that `cmpwi r?,0; beq` survives inlining. Retail's inlined copies have
no such test. `include/Collision/CCollisionInfo.hpp` already documents the fix for one type -
specialise `rstl::construct_impl` to call a function the compiler has no body for. It had never
been applied to `CTransform4f`, which is what `SCameraHistory`'s fill loop copies.

Adding the same specialisation in `include/Kyoto/Math/CTransform4f.hpp`, calling a declared-but-
undefined `extern "C" void fn_800E88FC(CTransform4f*, const CTransform4f&)` - retail's own name for
that copy constructor's out-of-line COMDAT copy (`config/G2ME01/symbols.txt:4055`) - took
`fn_801AD79C` from **90.74% to 96.76%** in one edit. Two other spellings measured *before* it,
neither of which removes the test: a bare `new (cur) CTransform4f(initial)` and
`rstl::construct_impl` instead of `rstl::construct`. The test is a property of the placement new
itself, so only routing through a bodyless callee removes it.

Two smaller facts from the same function, both measured:

- **`rstl::uninitialized_fill_n(data(), 80, value)` is worth 4% over a hand-written index loop**
  (86.18% -> 90.74%). It is the shape retail actually has: retail's loop counter `r31` counts to 80
  while the destination pointer `r30` walks `+48`, and the hand-written `mData[i * 48]` form makes
  MWCC recompute `self + i*48 + 4` and emit a null guard on it instead.
- **A byte-offset index into `reserved_vector`'s `mData` keeps the null guard**; a `CTransform4f*`
  walking loop does not - but that form scores **73.21%** because MWCC then hoists `mEnd` out of
  the frame. `uninitialized_fill_n` is the shape that satisfies both.

## `AddCamera` 46.964% -> 84.500% (measured, carried; contributes 0 matched functions)

Never attempted by any earlier run - run 1 called it "not diagnosed beyond that". It has **no
port-gap blocker**: it already called `GetObjectById` and `TCastToPtr<CGameCamera>` before this
run, so editing the body opens no new undefined symbol. Two changes, each measured:

- **The search is an iterator walk, not an index loop.** `while (it != end && *it != uid) ++it;`
  with `it` and `end` named locals gives 63.95%. Retail has no `lhzx`; it reloads a pointer from
  24(r1) and does `addi r3,r3,2`. `for (it = begin(); it != mCameras.end(); ++it)` with the test
  inside and an early `return` gives only **80.04%**, and `for(...; ++it) { if (...) break; }`
  followed by `if (it != end) return;` gives **82.62%**. The `while` with the test *after* the loop
  is the shape retail has (84.50%). **Do not retry the other three.**
- **Growth is `size() == capacity()` then `reserve(size() + 1)` then `push_back_unsafe`**, not
  `push_back`. `rstl::vector::push_back` reserves `capacity ? capacity * 2 : 4`, and retail's
  inline `reserve` argument is literally `mCount + 1` (`addi r4,r6,1` at 0x801AB3EC). This is worth
  20.55 points (63.95 -> 84.50) and is the whole difference between our growth and retail's.

Still 84.50%, not 100%: retail's frame is 48 bytes with four spilled iterator slots (12/16/20/24)
and ours is 32 with both iterators in registers. That is allocator state.

## Measured this run and NOT carried, so the next run skips the spellings

- **`SetCinematicPaused`, 97.143%, one register, and it is not the local's fault** (run 3's finding
  reproduces exactly). Retail `lwz r5,48(r3)`; ours `lwz r3,48(r3)`. Measured here, **all six
  give identical `r3` codegen and 97.143%**: `if (m) m->SetPaused(p)`, a named non-const pointer
  with `!= nullptr`, `if (m != nullptr)`, a named **const** pointer, a `const` pointer to a const
  camera, `if (!m) return; m->SetPaused(p);` (early-return form), a `CCinematicCamera&` deref
  binding, and a named pointer with `== nullptr` + early return. Retail's `beqlr` shows the null
  test *is* the return path, which is already what we emit. **WALL: do not retry any of these.**
- **`fn_801AD79C`, 96.765%, one instruction** - retail `lwz r4,3844(r28)` / `addi r0,r4,48`,
  ours `lwz r3,...` / `addi r0,r3,48`. Measured, none moved it: a named `first` local, dropping it,
  `self->mEnd = first + 1` (93.68%), `++self->mEnd` (85.00%), `self->mEnd = data() + 1` (86.18%),
  `mEnd = begin().get_pointer() + 1` (90.88%), `mBegin = mTransforms.begin()` (90.88%), a dead
  `self->mBegin = self->mBegin` (96.76%, identical codegen), `&data()[1]` (87.62%), the
  `(mBegin = data()) + 1` comma form (87.79%), and two `self->mEnd =` spellings (87.91%).
  **WALL: allocator state, not source.**
- **`__ct__14CCameraManagerF9TUniqueIdi` fell 57.59% -> 49.89% and is the one thing this diff
  makes worse.** Calling `fn_801AD79C` from the body is what retail does (0x801AD734, `r4` =
  `CTransform4f::sIdentity` at 0x804173D4 - verified, not assumed), and the call site's
  instruction alignment through the fog fields is now *correct* where it was not before. The drop
  is a length-alignment artefact of the scaffold: retail's constructor allocates and constructs a
  `CHintManager` (68 bytes, `fn_801BC8C4` behind an `rstl::string`) and a `CCameraShakeManager`
  (2116 bytes, `fn_801E82F0`) at 0x801AD6A8-0x801AD71C, and this tree has neither, so the two
  bodies still diverge there - we simply get further before they do. Measured alternatives, all
  **worse or equal**: keeping `mCameraHistory(CTransform4f::Identity())` in the mem-init list
  (49.89%), `mFluidSoundHandle()` in the list (49.89%), and moving `mFluidSoundHandle`'s zeroing
  into the body after the call (**50.74%**, the best of the three, and what is carried). The
  original spelling, with the fill inlined into the mem-init list, is what scored 57.59%; it is
  replaced because the out-of-line symbol is what makes `fn_801AD79C` exist to be measured at all.
  **A future item that implements `CreateCameras` should recover both managers and re-take this
  function** - it should then be worth more than the 7.7 points it currently costs.
- **`fn_801AAC08` (32 B) and `fn_801AABD0` (56 B)** are the cheapest remaining pair and were not
  attempted. `fn_801AAC08` is `rstl::construct_impl<CRayCastResult>` - a 32-byte forwarding stub to
  `fn_80034D88` - and `fn_801AABD0` is `vector<CTransform4f>::emplace_back`, which calls it. Both
  are `+4` = mCount, `+12` = mItems. The `construct_impl`-as-bodyless-callee trick above is exactly
  what `fn_801AAC08` *is*, so the mechanism is already proven on this unit. They need `CRayCastResult`
  to be instantiable in this TU.
- **`fn_801AAE20` (264 B), `fn_801AD824` (184 B), `fn_801AD8DC` (104 B)** still unpaired. Run 3
  called `fn_801AAE20` a wall over the placement-new null test; **that reasoning does not
  reproduce as a wall on the *symbol*** - see the table above. What is true is narrower: run 3's
  `Last()` rewrite is 280 bytes against retail's 264 because mwcceppc wraps each copy in
  `rstl::construct_impl`. With the `construct_impl` specialisation now in place for
  `CTransform4f`, **`fn_801AAE20` is worth re-measuring from scratch** - it is the largest of the
  three and the same trick that took `fn_801AD79C` from 90.74% to 96.76% may close it.

## Files touched

- `src/MetroidPrime/Cameras/CCameraManager.cpp` - `fn_801AAC28` (**the newly matched function**),
  `fn_801AD79C`, `AddCamera`'s body, the constructor's history fill, and the measured comments.
  Declaration order matters here: `fn_801AD79C` (retail 0x801AD79C, the highest offset in the unit)
  is the **first** definition in the file and `fn_801AAC28` (0x801AAC28, second lowest) sits
  immediately before `CheckSplineCollision`. Both positions are forced by
  `tools/check_decl_order.py`, which is the only thing that catches a permutation.
- `include/Kyoto/Math/CTransform4f.hpp` - `rstl::construct_impl` specialisation for
  `CTransform4f`, plus the declared-not-defined `fn_800E88FC`. Shared header; the note above says
  why, and `probe_sources.sh` measured the port's undefined count **falling** 250 -> 244.
- `include/MetroidPrime/CCameraManager.hpp` - `SCameraHistory`'s constructor is now default-only,
  so that no `__ct__` of its own is emitted into the unit's `.text` (it broke decl order at index
  8 when it was a forwarding member). No layout change, no offset moved.
- `docs/HANDOFF.md` - rewritten by `tools/gate.sh` (it owns that file); not hand-edited.

## Reusable rules this run added

1. **A report entry with no `fuzzy_match_percent` key is retail-only, not "0.00%".** It is an
   unwritten function, and writing it under retail's `fn_` name is what pairs it. This was worth
   +1 matched function and is the cheapest work in this unit.
2. **`return self;` at the end of a destructor is not dead code.** Retail's epilogue leaves the
   pointer in r3 on both the guard-taken and guard-not-taken paths, and omitting it costs 4.55%.
3. **A flag parameter retail compares with `extsh.` is a 16-bit value.** `static_cast<short>(x) > 0`
   rather than `x > 0`, worth 3.03% here.
4. **A placement-new null test is removable by making `construct_impl` call a bodyless function**,
   per type. Already done for `CCollisionInfo` and now `CTransform4f`; any other type whose retail
   inlined copies lack the test is a candidate.
5. **`rstl::uninitialized_fill_n` beats a hand-written fill loop** by 4 points here, because it
   keeps retail's two-register loop shape (a counter and a walking pointer) instead of folding it
   into one recomputed address.
6. **Check decl order after adding any function to a unit.** Both new functions landed in the wrong
   place on the first attempt and only `check_decl_order.py` said so; `objdiff` stayed at 100% for
   the paired function throughout.

## NEW

(none filed. The three unpaired `rstl` functions remaining above are inside this same unit, so a
separate item could not raise a count for them independently - they belong to the next run of this
item, not to a new one. `__ct__14CCameraManager` needs `CreateCameras`, the `CHintManager` and the
`CCameraShakeManager` bodies, which are not units in this tree; that is recorded above as the
follow-up, not as a claimable target.)

---

# Sixth run (lane 3, 2026-10-01)

Re-measured first on a fresh tree: the unit stood at **34 / 66 matched, 31.189% fuzzy**, project
**11321 / 28465** (`build/goal/judge/report.base.json`). Every earlier run's number reproduces
(`GetCameraBobMagnitude` 100%, `fn_801AB298` 100%, `fn_801AAC28` 100%, `fn_801AD79C` 96.76%,
`SetupInterpolation` 97.84%, `SetCinematicPaused` 97.14%, `AddCamera` 84.50%).

**Result: the unit's `matched_functions` went 34 -> 37 of 66** (fuzzy 31.189% -> 34.346%, matched
code 21.19% -> 27.68%); project `matched` 11321 -> 11324, `linked` 5507 -> 5507 (unchanged, as a
`NonMatching` unit must be). `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**.
`build/goal/check-gate.log`: `per-function diff matched 11321 -> 11324 linked 5507 -> 5507
(+3 functions at 100%, 0 units newly linked)`, then `port probe ok`, `port link gap ok`,
`GATE PASS`. `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`check_symbol_names.py` = 0 missing; `check_decl_order.py --unit` = ok. Per-function diff against
the judge baseline: **+3 at 100%, 0 functions worse anywhere in the tree.** Only two paths are
hand-edited; `docs/HANDOFF.md` is rewritten by `tools/gate.sh` (it owns that file).

## Per function: before -> after

| function | before | after | what it was |
|---|---|---|---|
| `fn_801AAE20` | **unpaired** | **100.000%** | retail's `SCameraHistory::Last`, already written and already byte-identical - as a *member* |
| `GetLastCameraTransform` | 64.158 | **100.000%** | the old body was an admission of ignorance, not a port |
| `UpdateCameraHistory` | 79.337 | **100.000%** | went through `GetCurrentCamera`, a function retail does not call here, and had the branches the other way round |

**`fn_801AAE20` was never a wall, and never even a wrong body.** Run 3 wrote `Last()` as a member,
measured 96.765%/280 bytes, and concluded the placement-new null test made it unreachable; run 5
measured that the body *is* 264 bytes and that `rstl::construct_impl` for `CTransform4f` had since
become a call to a bodyless function. Both were right and the conclusion was stale: the body
compiled **byte-identical to retail's 264** (`bl fn_800E88FC` at +0xd8 and +0xf0, the two
`rstl::construct_impl` calls, no `cmpwi`). The only thing wrong was the *name*: objdiff pairs by
name, so a member (`Last__Q214CCameraManager14SCameraHistoryCFv`) pairs with nothing and retail's
264 bytes show as 0.00% forever. This is run 3's own `fn_801AB298` mechanism, applied to the member
that run 3 declined to rename. Re-measuring a stale "unreachable" note is the whole run.

```cpp
extern "C" rstl::optional_object< CTransform4f >
fn_801AAE20(const CCameraManager::SCameraHistory* self);
```

The parameter is `const SCameraHistory*`, not `SCameraHistory*`: retail's `fn_801AAE20` is only
ever called from `const` methods (`GetLastCameraTransform`) and writes nothing but the return
slot, and a non-const parameter does not compile at those call sites
(`does not match 'fn_801AAE20(CCameraManager::SCameraHistory *)'`). A free function returning a
52-byte class puts the hidden return pointer in r3 and `this` in r4, which is retail's register
usage exactly.

**`GetLastCameraTransform`, 64.158% -> 100.000%, 228 B.** Retail 0x801AAD3C has an inlined
`Size()` whose `cmpwi r0,0` and the *valid-flag test after the first `Last()` call* both branch to
the **same** `sIdentity` return, and it calls `fn_801AAE20` **twice**, into two different 52-byte
stack slots (60(r1) and 8(r1)), returning `8(r1)`. One call is the condition, the other is the
value, so:

```cpp
  if (mCameraHistory.Size() != 0 && fn_801AAE20(&mCameraHistory)) {
    return fn_801AAE20(&mCameraHistory).data();
  }
  return CTransform4f::Identity();
```

This returns a reference into a temporary that dies at the closing brace. That is the undefined
behaviour retail has, it is what the 264/228 bytes require, and nothing in this tree calls the
function - the previous body said "the target appears to return a destroyed optional's payload" and
then returned a *different* pointer, which is why it scored 64%. The comment in the source now
says this outright, so the next reader does not "fix" it back.

**`UpdateCameraHistory`, 79.337% -> 100.000%, 380 B.** Two independent changes, each measured:

- *Do not go through `GetCurrentCamera`.* Retail calls `GetCurrentCameraId(false)` (a literal
  `false`, hence `li r5,0` at the top of the frame), `CStateManager::GetObjectById` (0x80041998),
  `TCastToPtr<11CGameCamera>` (0x8009A8DC) and copy-constructs from the camera's transform at
  `+0x24`. `GetCurrentCamera` is a real out-of-line function in both builds, so calling it emits a
  `bl` retail does not have. Writing the chain out is worth **+12.01 points** (79.337 -> 91.35) on
  its own. Note the cast has to be `TCastToConstPtr<CGameCamera>`: `CStateManager::GetObjectById`
  returns `const CEntity*` and `TCastToPtr` has no `const` overload. **This opens no port gap** -
  `TCastToPtr<11CGameCamera>__FP7CEntity` was already referenced from this unit by `AddCamera`, so
  the undefined count is unmoved. That contradicts runs 2 and 4, which treated
  `TCastToPtr<CGameCamera>` as a fresh port gap on the strength of its living in `TypesMatch.cpp`;
  check whether the object *already* references the symbol before repeating that reasoning.
- *The `Size() == 0` push has to be the out-of-line branch.* Both `if (Size()==0) { push; return; }
  body;` and `if (Size()==0) { push; } else { body; }` compile to `cmpwi r0,0; **bne** <body>` with
  the push in the fall-through, which is 91.35% and identical instruction-for-instruction
  otherwise. Retail is `cmpwi r0,0; **beq** <push at the very end>`. Writing it
  `if (mCameraHistory.Size() != 0) { body } else { push }` makes the body the fall-through and
  sinks the push to the tail, and that is the last **8.65 points**. Both spellings put a *second*
  copy of the `fn_801AB298` call at the tail (retail has one at +0xa8 and one at +0x15c), so the
  duplicated push is not the tell - the *side* the branch sits on is.

## Measured this run and NOT carried, so the next run skips it

- **The four remaining unpaired `rstl` COMDATs are a dead chain, not four functions.**
  `fn_801AAC08` (32 B), `fn_801AABD0` (56 B), `fn_801AD824` (184 B), `fn_801AD8DC` (104 B) form a
  closed group: `fn_801AD824` is a heap `rstl::vector<CTransform4f>::reserve`
  (`mCount`+4 / `mReserved`+8 / `mItems`+12) that calls `fn_801AD8DC`, and both it and
  `fn_801AABD0` (`emplace_back`) call `fn_801AAC08`. But **nothing in this tree has a heap
  `rstl::vector<CTransform4f>`**: `SCameraHistory::mTransforms` is a `reserved_vector<CTransform4f,
  80>` whose inline buffer is written directly at `self+4` by `fn_801AD79C`, and
  `CHECK_SIZEOF(CCameraManager, 0xfa8)` depends on that. Retail's `CCameraManager.cpp` must
  instantiate a heap `vector<CTransform4f>` for some class this layout does not model, so all four
  would have to be emitted as code nothing calls - a symbol written only so objdiff pairs it, which
  is exactly what the reviewer rejects. **Do not write them as free functions.** The identities, for
  whoever wants them later: `fn_801AAC08` is a bare 32-byte forwarder with no null test to
  `fn_80034D88`; `fn_80034D88` (0x28) is `construct_impl<CRayCastResult>` (`cmplwi r3,0; beq; bl
  __ct__14CRayCastResultFRC14CRayCastResult`) - and every *named* `construct_impl<T>` in
  `config/G2ME01/symbols.txt` is 0x28, so `fn_801AAC08` is **not** a `construct_impl`; retail folded
  it with a copy-constructor COMDAT, which is why it calls a `construct_impl`. `fn_801AD8DC` is
  `uninitialized_copy_n` with retail's signature `f(dest, const S* const& src, const S* const& end)`
  - both bounds **by reference**, read as `lwz r31,0(r3)` and `lwz r0,0(r29)` - where ours is
  `uninitialized_copy_n(S src, int n, D dest)`, and its loop is rotated (entry `b`s to the
  condition at the bottom). `fn_801AD824`'s own epilogue has **no stack restore** - it does
  `lwz r0,52(r1); lwz r31,44(r1); lwz r30,40(r1); lwz r29,36(r1); mtlr r0; addi r1,r1,48; blr`
  against a 48-byte frame, i.e. it reads four bytes above the saved LR.
- **`AddCamera`, 84.500% -> unchanged.** Retail 0x801AB34C (224 B) and ours are both 48
  instructions / 192 bytes; the whole difference is allocator state, as run 5 said. Retail's frame
  is 48 bytes and it makes **four** iterator stores (`stw r5,12(r1); stw r5,16(r1); stw r4,20(r1);
  stw r4,24(r1)`) of which only 24(r1) is ever read - two dead copies of `end` and one dead copy of
  `begin`. Ours is a 32-byte frame with both iterators in registers. Retail's loop reloads the
  walking pointer every iteration (`lwz r3,24(r1); addi r3,r3,2; stw r3,24(r1); lwz r3,24(r1);
  cmplw r3,r5; beq; lhz r3,0(r3); cmplw r3,r0; bne`). Run 5's three iterator spellings (84.50 /
  80.04 / 82.62) plus this run's are the space searched; the missing ingredient is whatever makes
  MWCC allocate two iterators' worth of dead spills, and I did not find it.
- Not re-tried, same blockers as before: `fn_801AD79C` 96.76% (one instruction, `r4` vs `r3`;
  run 5 measured nine spellings), `fn_801ABD68` 96.67% (needs a real 1-bit bitfield member on
  `CCinematicCamera`, a shared-unit layout change), `SetupInterpolation` 97.84% and
  `SetCinematicPaused` 97.14% (register choice; runs 3 and 5 measured 2 and 8 spellings),
  `StopCinematics` 2.08% / `CinematicCut` 2.78% / `SetPathCamera` 1.56% / `SetSpindleCamera` 1.61%
  (port gaps or out-of-line setters in unclaimed ranges), `CreateCameras` 0.26%,
  `UpdateFilters` 0.39%, `Reset` 0.82%, `AddCinemaCamera` 0.77%, `EnterCinematic` 1.10%,
  `SetPlayerCamera` 1.25%, `SetFixedCamera` 1.92%, `SetSurfaceCamera` 1.54%,
  `ClearSurfaceCamera` 4.76%, `IsBallCameraTransitioning` 3.78%, the three trigger helpers
  (2.04-2.33%), `CheckSplineCollision` 0.31%, `__ct__14CCameraManager` 49.89% (needs
  `CHintManager`/`CCameraShakeManager` bodies, as run 5 recorded).

## Files touched

- `src/MetroidPrime/Cameras/CCameraManager.cpp` - `Last()` -> `fn_801AAE20` (forward declaration
  after `fn_801AD79C`, definition between `StartScreenFlash` and `GetLastCameraTransform`),
  `GetLastCameraTransform`'s body, `UpdateCameraHistory`'s body and comments.
- `include/MetroidPrime/CCameraManager.hpp` - the `Last()` declaration removed. No layout change,
  no member moved, `CHECK_SIZEOF(CCameraManager, 0xfa8)` unmoved.
- `docs/HANDOFF.md` - rewritten by `tools/gate.sh` (it owns that file); not hand-edited.

## Reusable rules this run added

1. **An unpaired retail function is often a correctly-written member that was never renamed.**
   The tell is that the report has no `fuzzy_match_percent` key for it *and* a mangled name for the
   same body appears in the object. Run 5 found this (`fn_801AAC28`); run 3 wrote the body and
   stopped; this run closed it. **Before writing a `NEW:` or a `WALL:` for one, run
   `powerpc-eabi-nm -S <unit>.o | grep -i <name>`** - if a symbol of the right size is sitting in
   the object under a mangled name, the work is a rename, not a wall.
2. **A stale "unreachable" note is worth one re-measurement, because the blocker it names is
   usually a shared header someone has since changed.** Run 3's "this cannot reach 264 from our
   headers" was about the placement-new null test, and run 5 removed that test for `CTransform4f`.
3. **Two calls to a function that returns a class by value, into two different stack slots, mean a
   ternary/short-circuit over two temporaries, not a duplicated statement.** `cond ? f().x() :
   other` is the shape; `lbz <slot+48>` is the `optional_object::m_valid` test of the *first*
   temporary and `addi r3,r1,<slot2>` in the epilogue is the second one's payload.
4. **Which side of a compare the branch sits on is source, not allocator.** `if (A) { X; } else {
   Y }` and `if (A) { Y; return; } X` compile to the *same* `cmpwi` + inverted branch with the
   bodies in the opposite order; only the `!=` form makes the big body the fall-through and sinks
   the small branch to the tail. Worth 8.65 points on `UpdateCameraHistory` and it is invisible in
   the instruction *count* - both spellings are 95 instructions and 380 bytes.
5. **`tools/check_decl_order.py --unit` must be re-run after *moving* a function, not just after
   adding one.** Moving `fn_801AAE20` to the top of the file (because 0x801AAE20 looked like the
   lowest offset in the unit) made it permuted; the right slot is *between* `StartScreenFlash`
   (0x801AAF28) and `GetLastCameraTransform` (0x801AAD3C). `objdiff` stayed at 100% for
   `fn_801AAE20` throughout - only the tool catches it.
6. **A symbol's living in `TypesMatch.cpp` does not make it a port gap.** `TCastToPtr<CGameCamera>`
   was rejected for `SetPlayerCamera`/`SetPathCamera`/`SetSpindleCamera` in runs 2 and 4 on that
   basis, but `AddCamera` has referenced it since run 5, so the port resolves it. **Check
   `powerpc-eabi-objdump -r <unit>.o` for the symbol before refusing a body over it.**

## NEW

(none filed. The four unpaired `rstl` COMDATs are one dead chain inside this same unit and would
have to be written as uncalled code, so a separate item could not raise a count for them
independently. `AddCamera`'s 48-byte frame and `fn_801AD79C`'s one-instruction `r4`/`r3` difference
are allocator state with no measured source spelling. `__ct__14CCameraManager` still needs
`CreateCameras` plus the `CHintManager` and `CCameraShakeManager` bodies, which are not units in
this tree - unchanged from run 5, and still recorded as the follow-up rather than a claim.)
