# progress-prime1-ccinematiccamera

Target `MetroidPrime/Cameras/CCinematicCamera`. **Unit `matched_functions` 4 -> 5** (project total
10380 -> 10381). The unit stays `NonMatching`; `flip_test.sh` was not run, as the item says.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** on this tree (gate.sh incl. DOL sha1 + 86
RELs + report diff + docs claims + port probe, no asm added).

## Prime 1 was not usable for this unit

`prime-ref/src/MetroidPrime/Cameras/CCinematicCamera.cpp` is a **different class**, not an older
version of this one:

* Prime 1's `CCinematicCamera` ctor is `(uid, name, info, xf, active, shotDuration, fovy, znear,
  zfar, aspect, flags)`; Echoes' is `(uid, xf, active, fov, nearZ, farZ, aspect, index,
  controllerIdx)`. Echoes' camera is driven by a separate `CScriptCamera` actor + a
  `CScriptCameraSpline`, Prime 1's by its own `mViewPoints` / `mTargets` / `mViewHFovs` vectors.
* Prime 1 has **no** `Reset` (it is `{}`), **no** `CanSkip`, and its `CalculateMoveOutofIntoEyePosition`
  also writes the waypoint vectors, so it is not the same function as Echoes' (which returns the eye
  position).
* Prime 1's `GetMoveOutofIntoAlpha` is the only function that is character-for-character the same as
  ours - and it was already at 100%.

So per-function results come from retail disassembly (`tools/dis.sh`), not from Prime 1.

| function | before | after | note |
|---|---|---|---|
| `Reset` | 74.03% | **100%** | Prime 1 gave nothing; two edits, below |
| `Think` | 68.37% | **72.58%** | one of the two `Reset` edits, below; still far off |
| `CanSkip` | 87.07% | 87.07% | Prime 1 has no equivalent; not attempted |
| `__ct__` | 93.09% | 93.09% | not attempted |
| `CalculateMoveOutofIntoEyePosition` | 7.12% | 7.12% | not attempted; needs the same unresolved `CScriptActor` player-actor flag as the block below |
| dtor / Render / ProcessInput / `GetMoveOutofIntoAlpha` | 100% | 100% | already there |

## What matched `Reset` (both edits are in `src/MetroidPrime/Cameras/CCinematicCamera.cpp`)

1. **`SetTargetFov` -> `SetFovAndTarget`.** Retail calls `SetFovAndTarget__11CGameCameraFf` at
   `0x801ae324` / `0x801ae340`; ours called `SetTargetFov`.
2. **The fov/aspect branch is written positive-first, with the `GetFovByTime` call duplicated into
   both arms.** Retail hoists the flag load and `spline` pointer above the branch
   (`0x801ae308`-`0x801ae314`) and then calls `GetFovByTime` once per arm. Written the other way
   round (`float fov = ...; if (...) fov /= ...;`) mwcc emits one call site and loads `mFlags`
   *after* the call. Getting this right is also what makes retail spill `f31` (the aspect ratio) and
   use a 48-byte frame instead of ours' 32.

   ```cpp
   CGameCameraSpline& spline = camera->GetSpline();
   if ((mFlags & CScriptCamera::kF_VerticalFov) != 0) {
     SetFovAndTarget(spline.GetFovByTime(mTime));
   } else {
     SetFovAndTarget(spline.GetFovByTime(mTime) / GetAspectRatio());
   }
   ```

The same block in `Think` (`+4.21` points) was the only change made there. Nothing else in the unit
was touched; `include/` is unchanged.

## Codegen rule worth keeping: `rlwinm`'s MB/ME print is MSB-indexed

`objdump` prints `rlwinm. r0,r0,0,21,21` for a test of `mFlags & 0x400`. The mask fields are numbered
from the **MSB**, so the bit actually tested is `31 - printed_MB`. Verified against the compiler in
this tree (mwcc GC/2.7, same flags as `build.ninja`), compiling `uint f(uint x){return x & K;}`:

| K | objdump prints | 31 - printed |
|---|---|---|
| `0x4` | `0,29,29` | bit 2 |
| `0x100` | `0,23,23` | bit 8 |
| `0x400` | `0,21,21` | bit 10 |
| `0x200000` | `0,10,10` | bit 21 |
| `0x800000` | `0,8,8` | bit 23 |

I initially "corrected" `CScriptCamera::EFlags` on the false reading (`kF_VerticalFov` bit 21,
`kF_SlowMotion` bit 22, `kF_CinematicPause` bit 23) and the numbers did move - to bit 10 / 9, i.e. the
compiler agreed with the table. The existing header values are right; retail tests `0x400` here,
`0x200` for slow motion (`Think` `0x801ae1e8`) and `0x100` for cinematic pause
(`CScriptCamera::AcceptScriptMsg` `0x801defc4`). Nothing to change.

## What still blocks `Think` (72.58%, 1280 bytes) - for the next run

`think` is not only register allocation: two whole blocks of retail code have no source here yet.

* `0x801ae104`-`0x801ae188`: the `CScriptActor` player-actor fade. `ObjectById(camera->mCameraActorId)`
  -> `TCastToPtr<CScriptActor>` -> `lbz r0,918(r3)` bit 2 (printed 29) -> `GetMoveOutofIntoAlpha()`
  -> `CColorF(1,1,1,alpha)` -> `CModelFlags(5, 0, 3, colour)` blitted to `actor+252..260`. This is the
  existing `TODO` in `Think`; it needs `CScriptActor`'s player-actor flag, which its shared interface
  does not expose.
* `0x801ae1b4`-`0x801ae1e0`: forward `mTime` to the linked `CScriptTimeKeyframe`
  (`ObjectById(camera+794)`, `TCastToPtr<CScriptTimeKeyframe>`, then `bl fn_801F93E0(this, mTime, mgr)`).
  The existing `TODO`; `fn_801F93E0` is still unnamed, so the call has no callee to write.
* Smaller, once the two above land: retail computes `|roll - 0.0f|` (`fsubs` against the `.sdata2`
  0.0f at `0x8041cd8c`) before `fabs`, ours computes `|roll|`; retail stores all three `up`
  components before the branch, ours two after; retail's frame is 528 bytes and it keeps `this` in
  `r29` / `mgr` in `r30`, ours 464 bytes with `r30`/`r31`.

`(r2` is `0x804223C0` in this build - it is set once in the small thunk at `0x80003464`, so `lfs
f0,-22068(r2)` in retail resolves to `0x8041cd8c`. Useful for reading the `.sdata2` constants:
`0x8041cd88` = 1.0f, `0x8041cd8c` = 0.0f, `0x8041cd84` = 0.25f.)

No `NEW:` filed: the rest of `Think` is this same item, and the two blockers above are the `TODO`s
already in the source.