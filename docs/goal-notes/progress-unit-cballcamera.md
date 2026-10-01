# progress-unit-cballcamera — 6 functions matched (14 → 20 / 53)

## Result

`main/MetroidPrime/Cameras/CBallCamera`: **14 → 20 matched functions** of 53. Unit stays
`NonMatching` (the item is `kind: progress`; `flip_test.sh` was never run and no unit flip
was attempted). Unit fuzzy 23.78% → 24.72%, matched code 5.27% → 10.21%.

Whole tree: matched 12115 → 12121, linked 5860 → 5860, DOL sha1 held, all 86 RELs held,
`check_symbol_names.py` 0 missing, `goal_check.sh` **PASS**.

Files touched (all inside this worktree):

- `src/MetroidPrime/Cameras/CBallCamera.cpp` — six function bodies re-spelled.
- `include/MetroidPrime/Cameras/CBallCamera.hpp:115` — `ShouldResetSpline` return type
  `bool` → `int` (the only header change; nothing else in the tree depends on it and
  `check_symbol_names.py` is clean).

`docs/HANDOFF.md` was **not** edited by me; the judge rewrote the state block itself.

## Per function (all measured with `tools/fast_try.sh` + a retail/ours instruction diff)

| function | before | after | what changed |
|---|---|---|---|
| `ShouldResetSpline(CStateManager&) const` | 98.33% | **100.00%** | header return type `bool` → `int` |
| `ComputeVelocity(CVector3f, CVector3f, float)` | 81.56% | **100.00%** | named local for `positionDelta` |
| `FindDesiredTransform(CVector3f, CStateManager&)` | 78.62% | **100.00%** | named `dir`; named `xf` before `return` |
| `InterpolateCameraElevation(CVector3f, float)` | 63.29% | **100.00%** | early return + named `ret` |
| `TweenVelocity(const CVector3f&, const CVector3f&, float, float)` | 56.14% | **100.00%** | named `ret` + `ret +=` |
| `Reset(const CTransform4f&, CStateManager&)` | 95.49% | **100.00%** | `InvalidateSpline()` → `mSplineState = kBSS_Invalid;` |

Every one of the six is now **instruction-identical** to retail (branch displacements
excluded), not merely 100% by percentage.

### What each spelling was

**`ShouldResetSpline`** — the whole control flow already matched. The only difference was
the return: retail `mr r3,r31`, ours `clrlwi r3,r31,24`. MWCC's `clrlwi` is the int→bool
normalisation, so retail's return value is an `int`. Changing the **declaration in the
header** to `int` and keeping `bool ret = false` inside reproduces it exactly (measured:
`bool ret` + `int` return → still `clrlwi`; `int ret` + `int` return → `neg`/`or`/`srwi`,
worse; `bool ret` + `int` return → exact). Prime 1 has `const bool` here, so this is an
Echoes difference, not a donor-copy error.

**`ComputeVelocity`** — our body computed `positionDelta.Magnitude()` first, then tested
`mClampVelTimer > 0.f && positionDelta.IsMagnitudeSafe() && !mObtuseDirection`. Retail
copies the by-value parameter into a **named local first** and uses that everywhere:

```cpp
CVector3f velocity = positionDelta;
float magnitude = velocity.Magnitude();
if (mClampVelTimer > 0.f && velocity.IsMagnitudeSafe() && !mObtuseDirection) {
  velocity = velocity.AsNormalized() * CMath::Limit(magnitude, mClampVelRange);
}
return velocity;
```

The local is what makes retail's three-float frame copy (`addi r3,r1,20` / `stfs f2,28(r1)`)
appear; writing `positionDelta = positionDelta.AsNormalized() * ...` straight into the
parameter does not. Prime 1 has the same shape (`CVector3f ret = posDelta;`).

**`FindDesiredTransform`** — two named locals, both load-bearing:

```cpp
CVector3f dir = direction;            // not `direction = CVector3f(0,1,0)`
if (!direction.IsMagnitudeSafe()) {
  dir = CVector3f(0.f, 1.f, 0.f);
}
...
const CTransform4f xf = CTransform4f::LookAt(position, mLookPos);
return xf;
```

The test is on `direction` (the parameter) and the fix-up goes to `dir`; assigning to
`direction` makes the fallback write straight into the sret slot. Binding `LookAt`'s result
to `xf` is what makes retail's trailing `__ct__12CTransform4fFRC12CTransform4f` copy appear
(`addi r3,r1,112` / `bl <copy ctor>`), and it is also what takes retail's frame from 176 to
176 with the sret at `r1+112` rather than our `r1+64`. Measured: 78 retail insns / 69 ours
before, 78 / 78 exact after.

**`InterpolateCameraElevation`** — this is the biggest single win (63.29% → 100%) and it is
Prime 1's structure, which the tree's version had merged into one guarded block:

```cpp
if (mElevation < 2.f) {
  return position;
}
CVector3f ret = position;
if (!mClearLOS && mObscuringMaterial.HasMaterial(kMT_Floor)) {
  mElevInterpTimer = 1.f;
  ret.SetZ(GetTranslation().GetZ());
  mElevInterpStart = GetTranslation().GetZ();
} else if (mElevInterpTimer > 0.f) {
  mElevInterpTimer -= dt;
  float timer = CMath::Clamp(0.f, mElevInterpTimer, 1.f);
  float delta = ret.GetZ() - mElevInterpStart;
  ret.SetZ(delta * (1.f - timer) + mElevInterpStart);
}
return ret;
```

Three separate things matter, each measured: the **early return** (`if (mElevation < 2.f)`)
replaces `if (mElevation >= 2.f) { ... }` and is what turns retail's `bge` into our
`cror eq,gt,eq` + `bne`; the **named `ret`** makes `ret.SetZ` / `ret.GetZ` read the copy
instead of the by-value parameter; and the **`ret.SetZ(...)` before
`mElevInterpStart = ...`** order matters — retail reloads `92(r4)` (`GetTranslation().GetZ()`)
into `f6` and reuses it for both, so the two reads come from one load. Prime 1 has this exact
body.

**`TweenVelocity`** — same lesson as `ComputeVelocity`, and Prime 1 has it verbatim:

```cpp
CVector3f ret = currentVelocity;
CVector3f velDelta = newVelocity - currentVelocity;
if (velDelta.IsMagnitudeSafe()) {
  float t = CMath::Limit(velDelta.Magnitude() / (rate * dt), 1.f);
  ret += t * (dt * (rate * velDelta.AsNormalized()));
} else {
  ret = newVelocity;
}
return ret;
```

Our previous spelling was `if (!delta.IsMagnitudeSafe()) { return newVelocity; } … return
currentVelocity + …`. That is the same logic but it returns by value from two places, so
MWCC gives retail's 144-byte frame 89 instructions against our 89 and a different register
assignment; the `ret` accumulator plus `+=` is what makes retail's 92 instructions come out
(56.14% → 100.00%). Note Prime 1 spells the predicate `CanBeNormalized()`; this tree's
`CVector3f` has no such method and `IsMagnitudeSafe()` is the exact match.

**`Reset`** — a one-line change with a large effect. Our body called the out-of-line
`InvalidateSpline()`, which emitted an extra `bl InvalidateSpline__11CBallCameraFv` (23
call sites vs retail's 22) and shifted the whole tail by one instruction. Retail assigns the
member directly (`stw r0,1068(r30)`, and `mSplineState` is at 1068 = 0x430). Writing
`mSplineState = kBSS_Invalid;` inline removed the call and the function became
instruction-identical: 193 retail insns / 194 ours before, 193 / 193 exact after. Retail's
`Reset` does the same thing — the call to `InvalidateSpline` is nowhere in its 22-call list.

## Measured walls (do not re-try these)

- **`TeleportCamera(const CTransform4f&, CStateManager&)` — 97.00%, 16 of 35 insns differ.**
  Not a spelling problem: the control flow, all four `bl` targets and the argument registers
  are retail's. The difference is purely frame layout. Retail has **one** `TUniqueId`
  temporary (`sth r0,8(r1)` then `addi r4,r1,8`); ours materialises **two** (`sth r0,8(r1)`
  and `sth r0,12(r1)`, passes `r1+12`), which pushes the 12-byte `CVector3f` temporary from
  retail's `r1+12` to our `r1+16` and costs one extra instruction. Tried and measured, all
  produce the two-slot form: the original `CameraManager(mgr).UpdateCameraTriggers(GetUniqueId(), mgr)`;
  `GetCameraManager(mgr)` instead of `CameraManager(mgr)`; a named `const TUniqueId id`; the
  named local declared **before** the `CVector3f`; the named local declared **after** the
  `TeleportCamera` call; a named `const CVector3f position`. This is the same MWCC 2.7
  by-value `TUniqueId` materialisation already characterised in
  `src/MetroidPrime/Cameras/CCameraManager.cpp:357-364` and in
  `docs/goal-notes/progress-prime1-ccameramanager.md` ("MWCC 2.7 always materialises a fresh
  one"), where retail passes an sret buffer straight through and MWCC will not. Same root
  cause as `CCameraManager::SetCurrentCameraId`'s 93.24%.

- **`ResetToTweaks(CStateManager&)` — 92.94%, 68 of 144 insns differ.** One difference,
  repeated: retail copies the `CVector3f` returned by `GetBallCameraOffset()` into a
  12-byte stack temporary **word-wise** and then float-wise into `mLookAtOffset`:
  `lwz r0,0(r3)` / `lwz r4,4(r3)` / `stw r0,16(r1)` / `lwz r0,8(r3)` / `stw r4,20(r1)` /
  `lfs f0,16(r1)` / `stw r0,24(r1)` / `lfs f1,20(r1)` / `stfs f0,556(r31)` / … Ours copies
  straight from `r3` into the member with three `lfs`/`stfs`. That word-wise staging is why
  retail's frame is 96 and ours 80. Tried and measured, all give the direct copy: the plain
  assignment; a named `CVector3f offset`; a named `const CVector3f& offsetRef`;
  `mLookAtOffset = CVector3f(gpTweakBall->GetBallCameraOffset())`. The ctor
  (`__ct__…`) does the same assignment and *there* retail does the direct copy
  (`lfs f0,0(r3)` / `stfs f0,556(r29)` at 0x9d48/0x9d58), so the two sites genuinely differ
  and the difference is in how that one assignment is written, not in the member's type or
  the getter's signature — both are shared and both already match elsewhere.

- **`AcceptScriptMsg(CStateManager&, const CScriptMsg&)` — 94.63%.** Both objects make the
  identical **26** call relocations in the identical order (`AcceptScriptMsg__11CGameCamera…`
  through `DeleteObjectRequest__13CStateManagerF9TUniqueId`), so the source is right; only
  register assignment and frame size differ (retail 240 bytes with `stmw r25,212(r1)`, ours
  176 with `stmw r27,156(r1)`). Retail hoists `msg.GetMessage()` into `r25` **before** the
  base-class call; tried `const EScriptObjectMessage message = msg.GetMessage();` in front of
  it, which does hoist the load into a saved register but still lands 212 insns against
  retail's 216. Not chased further.

- **`__sinit_CBallCamera_cpp` — 70.18%, 30 of 56 insns differ.** Retail materialises the two
  `CMaterialList`s of `skLineOfSightFilter` through `R_PPC_EMB_SDA21` globals
  (`lbl_80419268`, `lbl_8041926C`) and only then stores into the filter object; ours builds
  them straight into the object and the `__shl2i` calls sit at different offsets. Only five
  `__shl2i` call sites on each side, same order, so this is again layout, not logic.

## Notes for the next run

- The three **cheap wins were all "add a named local"**, not "fix the logic": `ComputeVelocity`,
  `FindDesiredTransform`, `TweenVelocity`. When retail's frame is bigger than ours by exactly
  the size of a class temporary, the fix is a named local, and Prime 1's body is the donor.
  Start there on the next unit rather than reading the disassembly.
- Prime 1's `CBallCamera.cpp` was the donor for five of the six wins. Its
  `InterpolateCameraElevation` and `TweenVelocity` bodies transferred essentially verbatim
  (only `CanBeNormalized()` → `IsMagnitudeSafe()`). Its `ComputeVelocity` and
  `FindDesiredTransform` gave the named-local shape. Its `ShouldResetSpline` is
  `const bool` and its `Reset` uses `InvalidateSpline()` — **both wrong for Echoes**, so do
  not copy those two; Echoes' own bytes are what the notes above record.
- `docs/RUNNING_THE_DECOMP.md` says mwcceppc keeps a by-value struct parameter in the
  caller's buffer. The inverse also holds and cost me several tries here: when retail stages a
  class temporary on the stack before assigning it, **do not** bind it to a named local or a
  reference in the source — that is what removes the staging (see `ResetToTweaks`).
- `MoveCollisionActor` (82.85%, 189 of 213) and `UpdatePlayerMovement` (93.21%, 166 of 180)
  are next closest by differing-instruction count after the ones above; both differ in frame
  size and register allocation rather than in call structure, so they are the same kind of job.