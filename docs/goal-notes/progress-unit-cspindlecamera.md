# progress-unit-cspindlecamera

`MetroidPrime/Cameras/CSpindleCamera`: **11/16 -> 12/16 matched functions**. The unit stays
`NonMatching` (as the item requires); `Reset`, `CalculateTargetSplineDistance`,
`GetScanObjectIndicatorPosition` and `Think` are still unwritten. No `asm`, no header or config
change, no flip attempted, no commit.

Verified with `./tools/goal_check.sh build/goal/item.json` -> `PASS`, which re-ran the whole gate
itself. Measured, not recalled:

```
goal_check: item progress-unit-cspindlecamera (progress) target=MetroidPrime/Cameras/CSpindleCamera
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11879 -> 11880   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.63% fuzzy, 26.77% matched, 12.64% linked (11880 / 28465 functions)
  ok    target rose: main/MetroidPrime/Cameras/CSpindleCamera: 11 -> 12 / 16 functions
  ok    no asm added
goal_check: PASS progress-unit-cspindlecamera
```

`main.dol` sha1, all 86 RELs and `probe_sources.sh` are inside `gate.sh`; the per-function
`report diff` (no function anywhere got worse) is in there too. The whole diff is one line in
`src/MetroidPrime/Cameras/CSpindleCamera.cpp:46`.

## The change: `rstl::string` -> `rstl::string_l` for the camera's name

### `__ct__14CSpindleCameraF9TUniqueIdRC12CTransform4fbii` - 97.98% -> 100.00% (424 B)

Retail's prologue is `mr r4,r0; addi r3,r1,24; bl 802ff418 <string_l__4rstlFPCc>` - **two**
arguments, and the string temporary lands at `r1+24`. Ours emitted three extra instructions
(`addi r6,r1,8`, `li r5,-1`, and a temp pointer) before the call, because `rstl::string`'s
constructor from a `const char*` is the allocator-taking template: it forwards the string's
allocator and size to the out-of-line constructor. That pushed every stack slot in the function
up by 4, so all 20 later `addi rX,r1,N` / `stw rX,N(r1)` pairs were off by 4 and 98% was the
floor. Retail calls `string_l`, which takes the `char const*` alone.

One spelling, no variants tried:

| spelling | score |
| --- | --- |
| **`rstl::string_l("Spindle Camera")`** | **100.00%** |
| `rstl::string("Spindle Camera")` (original) | 97.98% |

**Generalisable codegen rule: for a `rstl::string` built from a literal and immediately passed to
a constructor, write `rstl::string_l`.** `string_l` is retail's actual callee
(`string_l__4rstlFPCc`, `0x802FF418`); `rstl::string` is a different, wider call. The three
sibling cameras already spell it this way - `CPathCamera.cpp:22` and
`CInterpolationCamera.cpp:23` use `string_l`, `CCinematicCamera.cpp:16` and `CBallCamera.cpp:24`
use `string` and are correspondingly not at 100% on their constructors. This is a real class of
2%-off constructors, not a one-off.

## The four functions that are left, and exactly what blocks each

Re-measured after the change (the item's `reason` figures are unchanged apart from the ctor):

```
main/MetroidPrime/Cameras/CSpindleCamera: 16.22% fuzzy, 15.78% matched code (12 / 16 functions)
     1.96%     204 B  Reset__14CSpindleCameraFRC12CTransform4fR13CStateManager
     1.94%     288 B  CalculateTargetSplineDistance__14CSpindleCameraCFR13CStateManager
     0.07%    5356 B  Think__14CSpindleCameraFfR13CStateManager
     1.43%    1836 B  GetScanObjectIndicatorPosition__14CSpindleCameraCFRC13CStateManager
```

### `Reset` (0x801B6CF0, 204 B) - blocked on a callee no unit defines

Retail's body is: `CameraManager(mgr)` -> `lwz r3,0x84(r3)` -> `bl 0x801B9480` ->
`TCastToPtr<17CScriptCameraHint>`, and if that hint is non-null *and* `CEntity`'s
`m_scriptingBlocked` bit is set (`lbz r0,0x20(r30); rlwinm. r0,r0,25,31,31` = bit 6 of the byte at
`0x20`, which is the CEntity bitfield word), then `mInResetThink = true`,
`CBallCamera::UpdateLookAtPosition(0.01f, mgr, false)`, a virtual `Think(0.01f, mgr)`, then
`mInResetThink = false; mFixedPositionInitialized = false`.

`0x801B9480` is `fn_801B9480` in `config/G2ME01/symbols.txt` and **no unit in the tree defines it**
(`grep -rn fn_801B9480 src/ include/` is empty). It is a `CHintManager` member: it reads the
`TUniqueId` at `+0xC` of its `this`, calls `GetObjectById`, and returns
`TCastToPtr<9CGameHint>`. Writing `Reset` needs a carve for it plus a `CCameraManager` accessor
for the hint manager at `+0x84` and for the ball camera at `+0x1C`; the last two are plain header
work but the first is not, and an inline spelling would not emit the `bl` the bytes need.

Float constants, read out of `.sdata2` via `_SDA2_BASE_` = `0x804223C0` (this is what
`tools/sda.py` exists for - the same displacements read against `r13` give nonsense):
`-21760(r2)` = `0x8041CEC0` = `0.01f`, `-21824(r2)` = `0x8041CE80` = `0.0f`.

### `CalculateTargetSplineDistance` (0x801B6BD0, 288 B) - blocked on a wrong `CMotionSpline` layout

`CPathCamera::CalculateLookAtDistance` (`0x801B45F8`, already 100% matched) is retail's twin of
this function and gives the exact template, so this one is *spelling* work, not archaeology. The
structure is `GetObjectById` + `TCastToPtr<CScriptSpindleCamera>`, two `GetControlPointCount() == 0`
guards, then `close_enough()` and `CMath::Clamp(0.f, mPlayerSplineDistance / len, 1.f)`.

The blocker is the two length floats it divides and multiplies by. Retail reads
`CMotionSpline + 0x34` (52) and `+0x38` (56); the closed-loop flag is bit 6 of the byte at `+0x3C`
(60). Measured out of `CMotionSpline`'s own retail code:

- `GetPositionByTime__13CMotionSplineCFf` (`0x80332B40`): `lfs f3,56(r4)` as the time->parameter
  divisor, `lfs f0,52(r4)` as the scale.
- `ValidateLength__13CMotionSplineCFf` (`0x80332BC0`): `lbz r0,60(r3); rlwinm. r0,r0,25,31,31` for
  the closed-loop flag, then clamps into `[52, 56]`.

`include/Kyoto/Math/CMotionSpline.hpp` has **`mLength` at 40 and `mDuration` at 44** - a uniform
**-12 offset delta** on the whole tail, exactly the signature `tools/probe_offsets.cpp`'s header
comment describes. So `GetLength()`/`GetDuration()` read the wrong words, and the two floats
`CalculateTargetSplineDistance` needs are not reachable by name at all.

**There is no `CMotionSpline.cpp` unit in the tree** (`find . -name 'CMotionSpline*'` returns only
the header; `CMotionSpline::FindClosestLengthOnSpline` is only *called*, from
`CPathCamera.cpp:174,186`, and resolves from an unclaimed `auto_*` unit). So no matched function
anywhere tests that layout, which is why the error survived. Fixing it is a change to a header
shared by `CBallCamera`, `CInterpolationCamera`, `CGameSpline`, `CGameSplineDesc` and
`CScriptSpindleCamera`, and it does not raise a count in *this* item's unit on its own - it is
someone else's item, and per the brief it belongs in a `NEW:` line rather than in this diff.

### `Think` (5356 B) and `GetScanObjectIndicatorPosition` (1836 B)

Not attempted: both are large, both call the eight `GetInterpolant`/`GetInVar` chains plus
`CScriptSpindleCamera` internals, and `Think` is 0.07% - an empty body. Out of reach for one item.

## Layout facts measured this run (so nobody re-derives them)

`CScriptSpindleCamera`'s layout in `include/MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp`
is **correct** - I checked it against the retail constructor (`0x801DFE28`) before assuming
otherwise, and it lines up exactly:

| member | ours | retail (`__ct__20CScriptSpindleCamera`, `addi r3,r15,N`) |
| --- | --- | --- |
| `mParameters` | `0x158` | 344 (`addi r3,r15,344`) |
| `mTargetSpline` | `0x5E0` | 1504 |
| `mTargetControlSpline` | `0x624` | 1572 |
| `mPlayerSpline` | `0x668` | 1640 |
| `mOrigXf` | `0x6AC` | 1708 |

`sizeof`: `CActor` `0x158`, `CSpindleCameraParameters` `0x488`, `CMotionSpline` `0x44`,
**`CMayaSpline` `0x44`**, `CTransform4f` `0x30`, `CScriptSpindleCamera` `0x6E0`. And on
`CSpindleCamera` itself: `mSpindleCameraId` `0x200`, `mInVars` `0x204`, `mMaxAzimuthInterpTimer`
`0x228`, `mLookDir` `0x22C`, `mTargetSplineDistance` `0x238`, `mPlayerSplineDistance` `0x23C`,
`mLookPosition` `0x240` - every one of them the offset the retail constructor stores to, which is
why the ctor reached 100% on the `string_l` fix alone.

Measured with a throwaway probe compiled by hand (`build/probe/probe_spline_layout.cpp`, under
`build/` so it is gitignored, deleted afterwards) because `tools/probe_cc.sh` lacks the
`-i extern/musyx/include` that `CActor.hpp` needs.

**Warning for other lanes: `tools/probe_offsets.cpp` is a TRACKED file.** Writing a scratch probe
over it and then deleting it shows up as ` D tools/probe_offsets.cpp` and fails `goal_check.sh`'s
"no judge-owned path touched" outright. `git checkout -- tools/probe_offsets.cpp` restores it.
Put scratch probes under `build/`, never in `tools/`.

---

# Second run (lane 6, 2026-10-02) — 12/16 -> **13/16**

`MetroidPrime/Cameras/CSpindleCamera`: **12 -> 13 matched functions** (unit still `NonMatching`, as the
item requires). `CalculateTargetSplineDistance` reached **100%**; `Reset` went 1.96% -> **97.84%**
(kept, does not count). `Think` (5356 B) and `GetScanObjectIndicatorPosition` (1836 B) untouched. No
`asm`, no config change, no flip attempted, no commit.

```
goal_check: item progress-unit-cspindlecamera (progress) target=MetroidPrime/Cameras/CSpindleCamera
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12353 -> 12354   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.88% fuzzy, 28.51% matched, 12.90% linked (12354 / 28465 functions)
  ok    target rose: main/MetroidPrime/Cameras/CSpindleCamera: 12 -> 13 / 16 functions
  ok    no asm added
goal_check: PASS progress-unit-cspindlecamera
```

`build/report.json` for the unit after this run: `21.46% fuzzy, 18.94% matched (13 / 16 functions)`,
`total_data 152 / 152 (100%)`.

## Three of the previous run's blockers were wrong. Re-measured, not inherited.

1. **`CMotionSpline`'s layout in this tree is CORRECT.** The note above claims `mLength` at 40 and
   `mDuration` at 44, "a uniform -12 offset delta". Measured this run with a throwaway probe
   compiled by `tools/probe_cc.sh` (`offsetof`, `sizeof(rstl::vector<T>) = 16`):
   `mControlPoints 4, mKnots 0x14, mKnotDistances 0x24, mLength 0x34 (52), mDuration 0x38 (56),
   sizeof 0x44`. The `+4` on the first member is the **vtable pointer** (`virtual ~CMotionSpline`),
   which is what the earlier run missed. 52/56 is exactly what retail reads
   (`lfs f31,52(r31)` in this very function, `ValidateLength`'s closed-loop bit at byte 60). There
   is nothing to fix in that header and no `NEW:` line is owed.
2. **`fn_801B9480` needs no carve.** `0x801B9480` sits in the *unclaimed* `.text` gap between
   `MetroidPrime/Carve801B9420` (ends `0x801B9424`) and `MetroidPrime/Carve801B94B4` (starts
   `0x801B94B4`) in `config/G2ME01/splits.txt`, so dtk fills it from retail and it is already a
   defined symbol in `build/G2ME01/main.elf` (`nm` -> `801b9480 T fn_801B9480`). Declaring it is all
   a caller needs — the repo already does this for `fn_800D042C`
   (`include/Collision/CCollisionInfo.hpp:57`). So: **grep for an undefined `fn_` before concluding a
   carve; check whether its address is claimed.**
3. **The accessors `Reset` needs already existed.** `CCameraManager::HintManager()` and
   `CCameraManager::BallCamera()` are public non-const overloads; measured offsets
   `mCameraHintManager = 0x84`, `mBallCamera = 0x1C`, which are retail's `lwz r3,132(r3)` and
   `lwz r3,28(r3)`. `CBallCamera::UpdateLookAtPosition(float, CStateManager&, bool)` existed too
   (private, so one `friend class CSpindleCamera;` was the only header change it needed).

## `CalculateTargetSplineDistance__14CSpindleCameraCFR13CStateManager` — 1.94% -> **100%** (288 B)

Retail (`tools/dis.sh 0x801B6BD0 0x120`) is `CPathCamera::CalculateLookAtDistance` (`0x801B45F8`,
already 100%) with a different spline: `GetObjectById(mSpindleCameraId)` +
`TCastToConstPtr<CScriptSpindleCamera>`, then `mTargetSpline.GetControlPointCount()`, then either
`mTargetSpline.FindClosestLengthOnSpline(mTargetSplineDistance, Player(mgr).GetBallPosition())`
when `mPlayerSpline` is empty, or `close_enough(mPlayerSpline.GetLength(), 0.f, 3.f)` bail plus
`mTargetControlSpline.EvaluateAt(CMath::Clamp(0.f, mPlayerSplineDistance / len, 1.f)) *
mTargetSpline.GetLength()`.

Spelling, and the two things that mattered:

| spelling | score |
| --- | --- |
| `const CVector3f ballPosition = Player(mgr).GetBallPosition();` then pass `ballPosition` | 91.31% |
| pass `Player(mgr).GetBallPosition()` inline (as written now) | **100.00%** |
| TODO stub (original) | 1.94% |

- **Naming the by-value temporary costs six instructions.** `const CVector3f ballPosition = ...`
  makes mwcc *copy* the `GetBallPosition` sret buffer at `r1+12` into a second slot at `r1+24`
  (`lfs f2,12(r1)` / `stfs f2,24(r1)` x3) before the call. Retail passes `addi r4,r1,12` — the sret
  slot itself. Generalisable: **do not name a by-value class temporary that is immediately passed
  by const reference.**
- **The `close_enough` epsilon is `3.f`, not the default.** `-21764(r2)` = `0x8041CEBC` =
  `0x40400000` = `3.0f` (read out of the retail DOL with `tools/dol_read.py 0x8041CE80 0x48`).
  `CPathCamera`'s twin uses `close_enough(len, 0.f)` with the default `1e-5`; this one does not.

Header change needed: `CScriptSpindleCamera`'s three splines are private with no accessors, so three
guessed getters were added (`GetTargetSpline`, `GetTargetControlSpline`, `GetPlayerSpline`), each a
`const` method returning a mutable reference, exactly as `CScriptPathCamera::GetSpline` does — which
required marking the three members `mutable`, because MWCC 2.7 rejects returning a non-const
reference to a member of a `const` method ("illegal implicit conversion from 'const CMotionSpline'")
and the tree's own convention for this is `mutable`. No codegen effect on any other unit.

## `Reset__14CSpindleCameraFRC12CTransform4fR13CStateManager` — 1.96% -> **97.84%** (204 B)

Body, now real rather than a TODO:

```cpp
CScriptCameraHint* hint =
    TCastToPtr<CScriptCameraHint>(fn_801B9480(CameraManager(mgr).HintManager(), mgr));
if (GetActive()) { if (hint) { mInResetThink = true;
  CameraManager(mgr).BallCamera()->UpdateLookAtPosition(0.01f, mgr, false);
  Think(0.01f, mgr); mInResetThink = false; mFixedPositionInitialized = false; } }
```

**The guard is `GetActive()`, not a "scripting blocked" flag.** Retail tests bit 6 of the byte at
`0x20` (`lbz r0,32(r30); rlwinm. r0,r0,25,31,31`). A probe that reads each `CEntity` bitfield
separately shows which bit that is in *this tree's* layout: `mActive` -> `rlwinm r3,r0,25,31,31`,
`mNotInArea` -> `26,31,31`, `mCastFlags` -> `30,28,31`, `mUpdateWhileOccluded` -> `31,31,31`,
`mUpdateDuringCinematicSkip` -> folded away (bit 7, always 0 after `lbz`). So bit 6 **is**
`mActive`, and `CEntity::GetActive()` already returns it. Semantics: only an *active* spindle camera
with an active `CScriptCameraHint` resets. Also measured: `-21760(r2)` = `0x8041CEC0` = `0.01f`,
`-21824(r2)` = `0x8041CE80` = `0.0f`.

Four spellings tried, all measured:

| spelling | score | what differs |
| --- | --- | --- |
| `if (GetActive() && TCastToPtr<CScriptCameraHint>(activeHint))` | 93.92% | `extrwi. r0,r0,1,24` where retail has `rlwinm.` |
| `if (!GetActive()) { return; } if (hint) {...}` | 97.84% | inner test is `beq join` + fallthrough; retail is `bne body` + `b join`, so ours is 4 bytes short |
| `if (hint && GetActive()) {...}` | 90.20% | mwcc merges the two tests, `cmplwi`/`bne` disappear entirely |
| **`if (GetActive()) { if (hint) {...} }`, cast hoisted** (as written now) | **97.84%** | 50 of 51 instructions match; only `extrwi.` vs `rlwinm.` |

**WALL (this run, measured): `Reset` sits at 97.84% across four different spellings, and the last
two are coupled — every spelling that gets retail's `bne body; b join` block layout makes mwcc
materialise the flag with `extrwi. r0,r0,1,24` instead of testing the bit in place with
`rlwinm. r0,r0,25,31,31`, and every spelling that gets `rlwinm.` gives `beq join` and falls through
into the body.** Standalone probes (`if (x->GetActive())`, `if (x->mActive)`, `if (x->GetA() && h)`,
with the then-block both small and four calls long) all produce `rlwinm.` + `beq`, so the coupling
is a block-layout/register-allocation decision inside this function, not something a simpler
expression reaches. Not fixed here.

## Other facts measured, so nobody re-derives them

- `GetObjectById` must be the **const** overload: retail's call is
  `GetObjectById__13CStateManagerCF9TUniqueId` (`0x80041998`), not `ObjectById` (`0x80041968`).
  `CGameCamera::CameraManager(CStateManager&)` (no `const` on the ref) is the one retail calls;
  the `GetCameraManager(const CStateManager&)` sibling is a different symbol.
- `rstl::vector<T>` is `{Alloc mAllocator; int mCount; int mCapacity; T* mItems;}`, so
  `size()` is at vector+4 — which is why retail's `GetControlPointCount()` guard is
  `lwz r0,8(r31)` with `r31 = &mTargetSpline`.
- `unit_fit.sh MetroidPrime/Cameras/CSpindleCamera.cpp` reports 7 functions / 992 bytes that ours
  emits and the retail unit object does not define (weak `CMayaSpline` / `rstl::vector` /
  `rstl::string` template copies and `CActor::GetHealthInfo`). **Measured to be pre-existing**: the
  four source/header files were reverted, the unit rebuilt, and the same seven appear. The change
  does make `.sdata2` grow 4 -> 12 bytes (the new `3.f`), and `.sdata2` is not claimed for this
  unit, so those bytes live in a neighbour either way.
- `tools/probe_cc.sh` lacks `-i extern/musyx/include`, which `CAudioSys.hpp` (and so
  `CCameraManager.hpp`) needs; add that flag by hand for probes of those headers.
- The scratch probes for this run were `build/probe/*.cpp`, under `build/` (gitignored) and deleted.
  The earlier warning still stands: `tools/probe_offsets.cpp` is tracked, and a scratch probe left
  there fails `goal_check.sh`'s "no judge-owned path touched".

## Files touched

- `src/MetroidPrime/Cameras/CSpindleCamera.cpp` — `CalculateTargetSplineDistance` (100%),
  `Reset` (97.84%), six includes.
- `include/MetroidPrime/ScriptObjects/CScriptSpindleCamera.hpp` — three guessed spline getters,
  three `mutable` members.
- `include/MetroidPrime/CHintManager.hpp` — `extern "C" CGameHint* fn_801B9480(...)`, with why.
- `include/MetroidPrime/Cameras/CBallCamera.hpp` — `friend class CSpindleCamera;`.
- `docs/HANDOFF.md` — **not edited by hand**; `gate.sh`'s `sync_state_block.py` rewrote the two
  derived counts (12353 -> 12354, DOL units 10805 -> 10806). The driver discards it either way.

No `NEW:` line is owed: the two functions this item owed are now written (one at 100%), the header
the previous run called wrong is right, and `Reset`'s remainder is one instruction of block layout,
not a separate unit.

---

# Third run (lane 7, 2026-10-02) — 13/16 -> **14/16**

`MetroidPrime/Cameras/CSpindleCamera`: **13 -> 14 matched functions** (unit still `NonMatching`, as the
item requires). `Reset` reached **100.00%** (was 97.84%). `Think` (5356 B) and
`GetScanObjectIndicatorPosition` (1836 B) still unwritten. No `asm`, no header or config change, no
flip attempted, no commit.

```
goal_check: item progress-unit-cspindlecamera (progress) target=MetroidPrime/Cameras/CSpindleCamera
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12376 -> 12377   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.96% fuzzy, 28.63% matched, 12.90% linked (12377 / 28465 functions)
  ok    target rose: main/MetroidPrime/Cameras/CSpindleCamera: 13 -> 14 / 16 functions
  ok    no asm added
goal_check: PASS progress-unit-cspindlecamera
```

`build/report.json` for the unit after this run: `21.51% fuzzy, 21.17% matched (14 / 16 functions)`,
`total_data 152 / 152 (100%)`.

## The second run's `Reset` WALL was the wrong spelling, not the wrong function

The previous run wrote `WALL: Reset ... every spelling that gets retail's block layout makes mwcc
materialise the flag with extrwi.`, having tried four `if` shapes. It was right that every `if` shape
fails, and wrong to conclude from that that the guard is unreachable: **no nested-`if` spelling of this
guard can produce retail's bytes, because retail's bytes are not a nested-`if`.**

The fix is one line, and it removes a whole `if`:

```cpp
// before - 97.84%, always
if (GetActive()) {
  if (hint) {
    mInResetThink = true;
    ...
  }
}
// after - 100.00%
if (!GetActive() || !hint) {
  return;
}
mInResetThink = true;
...
```

### How it was found: scan the DOL for retail's shape and read a matched function that has it

Retail's `Reset` body is

```
801b6d24:  lbz     r0,32(r30)
801b6d28:  rlwinm. r0,r0,25,31,31
801b6d2c:  beq     801b6da4        <- !GetActive() -> END
801b6d30:  cmplwi  r3,0
801b6d34:  bne     801b6d3c        <- hint      -> BODY
801b6d38:  b       801b6da4        <- !hint     -> END
801b6d3c:  lbz     r0,588(r30)     <- BODY, reached only by the `bne`
...
801b6da4:  lwz     r0,20(r1)      <- END, also the epilogue
```

i.e. the body is **out of line**, entered only by the branch, with a redundant `b` over it. Every
`if` (nested or `&&`) makes mwcc inline the body and drop the `b`; so the target shape is not an `if`
shape at all. Rather than keep guessing (the previous run's dead end), I dumped every instruction of
the linked DOL (`objdump -d build/G2ME01/main.elf`), scanned for that 4-instruction pattern, and
looked up the owning function's `fuzzy_match_percent` in `build/report.json`.

**That shape occurs in 18 functions in the DOL, and 7 of them are already at 100%** — so the answer
was sitting in the tree:

| retail site | function | score |
| --- | --- | --- |
| `0x8010a66c` | `CScriptActorRotate::Think` | 100.0% |
| `0x8015110c` | `CPatterned::RotateToPoint` | 100.0% |
| `0x80153434` | `CScriptColorModulate::Think` | 100.0% |
| `0x801dde34` | `CPlayerGunBase::HolsterGun` | 100.0% |
| `0x8027e490` | `CAuiEnergyBarT01::Draw` | 100.0% |
| `0x8027f7f4` | `CAuiImagePane::Draw` | 99.8% |
| `0x803252a0` | `CRumbleVoice::Deactivate` | 100.0% |

The minimal one is `CRumbleVoice::Deactivate` (`src/Kyoto/Input/CRumbleVoice.cpp:24`), and its source
is the template - an early-return guard written as a **short-circuit `||`** with the rest of the
function after it:

```cpp
if (id == -1 || !OwnsSustained(id)) {
  return;
}
if (mUsedChannels & (1 << GetChannelId(id))) {
  mDeltas[GetChannelId(id)].mPhase = SAdsrDelta::kP_Release;
}
```

which compiles to `cmpwi r0,-1 ; beq END ; bl OwnsSustained ; clrlwi. r0,r3,24 ; bne BODY ; b END ;
BODY:` - the same six instructions as `Reset`, with the same redundant `b`.

`CScriptColorModulate::Think` (`src/MetroidPrime/ScriptObjects/CScriptColorModulate.cpp:197`) is the
same rule with bitfields, and its comment records the same discovery from the other direction: an
`extrwi`-producing guard that only matched when the guard was written with a *redundant* re-test.

## Generalisable codegen rule (MWCC 2.7, PowerPC)

**A guard of two tests in front of the rest of a function is `if (!A || !B) { return; }`, not nested
`if`s.** The `||`-with-early-return is the only spelling measured that lays the body out of line
(`bne BODY ; b END ; BODY:`); every nested `if` and every `&&` measured emits `beq END ; beq END` and
folds the body into the fall-through. The two spellings have the same CFG, so this is a block-ordering
decision mwcc makes from the `return` edge, not from the condition.

Corollary, and the thing that actually generalises: **when a function is at 97-99% and the only
difference is block layout, find retail's byte shape elsewhere in the DOL and read a function that
already matches it.** Guessing C++ shapes is unbounded; the DOL is a finite index of the answers, and
`build/report.json` says which entries are already solved. 20 minutes of scanning beat the previous
run's four spellings.

## Every spelling measured this run (all on `Reset`, all against the same 51-instruction retail body)

All of these give the identical 97.84% - `beq END ; cmplwi ; beq END ; BODY` - with the body's first
instruction inline at `+0x44`:

| spelling | score |
| --- | --- |
| `if (GetActive()) { if (hint) { body } }` (the previous run's spelling) | 97.84% |
| `if (hint != nullptr)` inner test | 97.84% |
| `if (nullptr != hint)` inner test | 97.84% |
| inner `if (hint) { body } else {}` | 97.84% |
| `if (!hint) {} else { body }` | 97.84% |
| `if (GetActive() && hint)` | 97.84% |
| `if (GetActive() && (hint != nullptr))` | 97.84% |
| `if (hint) { body } else { return; }` | 97.84% |
| `if (!hint) { return; }` early-return, body unindented | 97.84% |
| two `goto end` guards, body between them | 97.84% |
| `if (GetActive() && (hint != nullptr))` with the cast inline in the `&&` | 74.02% (calls get sunk inside the guard) |
| `const bool bHasHint = hint != nullptr; if (GetActive() && bHasHint)` | 90.69% (adds `neg`/`or`/`srwi`) |
| **`if (!GetActive() \|\| !hint) { return; }`** | **100.00%** |

Standalone shape probes compiled with the unit's own cflags (76 distinct guard/loop shapes: `while`,
`do/while`, `for(;;)`, `switch`, `goto`-into-label, `&&`, ternary, `else`-with-empty-statement,
`break`-out-of-loop, nested scopes) produced the retail `bne BODY ; b END ; BODY:` shape **only** for
`goto` into a label placed after the guard. Every `if`-family shape collapsed to `beq ; beq`. The
`||`-with-`return` spelling is the one that is both natural and correct.

## Traps hit while measuring (so nobody re-hits them)

- **`build/report.json`'s `address` field is the object-relative offset, not the address.** For
  `Reset` it is the string `"7560"` (0x1D98 into the unit object). The absolute VA is in
  `metadata.virtual_address`. **Both are decimal strings, not hex** - `"2149281008"` is
  `0x801B6CF0`. Parsing either as hex silently produces a nonsense address and every lookup returns
  "no owner", which looks like "no matched function has this shape" and sends you back to guessing.
- The scratch probes for this run were `build/probe/*` (gitignored; `git status` shows only the one
  source file). `build/G2ME01/main.dis` (a ~1M-line dump of the DOL) is what the shape scan reads;
  regenerating it takes about a minute. The previous runs' warning still stands:
  `tools/probe_offsets.cpp` is tracked, and a scratch probe left there fails `goal_check.sh`'s
  "no judge-owned path touched".
- `tools/g2try.sh`'s range lookup from `splits.txt` fails for this unit (the unit's claim is
  `0x801B4F68..0x801B730C`, not `+320` from a function start), so `cmp`-by-eye is easier:
  retail via `tools/dis.sh 0x801b6cf0 0xcc`, ours via `nm` on
  `build/G2ME01/src/MetroidPrime/Cameras/CSpindleCamera.o` (ours is at object offset `0x1bc`).

## Facts carried forward, re-measured

- `unit_fit.sh MetroidPrime/Cameras/CSpindleCamera.cpp` still reports the same 7 pre-existing
  functions ours emits that the retail unit object does not (992 bytes: weak `CMayaSpline` /
  `rstl::vector` / `rstl::string` template copies and `CActor::GetHealthInfo`). The previous run
  measured these as pre-existing by reverting and rebuilding; this change adds no new ones.
- The hint is still resolved and cast **before** the flag is tested - that ordering is unchanged from
  the previous run and is not what this change fixes.

## Files touched

- `src/MetroidPrime/Cameras/CSpindleCamera.cpp:74-90` - `Reset`, one guard collapsed to
  `if (!GetActive() || !hint) { return; }`, comment updated to record why.
- `docs/HANDOFF.md` - **not edited by hand**; `gate.sh`'s `sync_state_block.py` rewrote the two
  derived counts (12376 -> 12377, DOL units 10828 -> 10829). The driver discards it either way.

No `NEW:` line is owed: the item's target moved 13 -> 14, and the one thing left to find was a codegen
rule, which is a note rather than an item.
