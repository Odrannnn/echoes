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
