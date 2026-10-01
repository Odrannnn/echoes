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

---

# Run 2 (lane L6) — 3 functions matched (20 → 23 / 53)

## Result

`main/MetroidPrime/Cameras/CBallCamera`: **20 → 23 matched functions** of 53. Unit stays
`NonMatching` (`kind: progress`; `flip_test.sh` never run). Unit fuzzy 24.72% → **26.52%**,
matched code 10.21% → **17.82%**.

Whole tree: matched **12205 → 12208**, linked 5860 → 5860, DOL sha1 held, all 86 RELs held,
`check_symbol_names.py` 0 missing, `goal_check.sh` **PASS** ("target rose: 20 -> 23 / 53").
No function in the unit got worse (checked per-function against a clean-tree rebuild).

Files touched (both in this worktree, no header change):

- `src/MetroidPrime/Cameras/CBallCamera.cpp` (+49 / −8): three functions re-spelled, one
  rewritten from a stub, two `#include`s added.

## Re-measurement warning for the next run

`build/report.json` was **stale** when this run started. The figures quoted in `item.json` and in
the run-1 notes for `MoveCollisionActor` (82.85%) do **not** reproduce: a clean rebuild of the
untouched tree measures **82.60%**. The first `tools/fast_try.sh` after a checkout can reuse a
report built against a different tree. Rebuild before quoting any number.

## Per function

| function | before | after | what changed |
|---|---|---|---|
| `__ct__11CBallCamera(TUniqueId, TUniqueId, const CTransform4f&, f,f,f,f,int,int)` | 98.85% | **100.00%** | `rstl::string("Ball Camera")` → `rstl::string_l("Ball Camera")` |
| `UpdatePlayerMovement(float, CStateManager&)` | 93.21% | **100.00%** | `if (...) mObtuseDirection = true;`; a named dead `CVector2f`; a named `CVector2f velocityFlat` |
| `ClampElevationToWater(CVector3f, CStateManager&) const` | 6.33% | **100.00%** | the body, recovered from retail (was `// TODO: …; return position;`) |
| `GetScanObjectIndicatorPosition(const CStateManager&) const` | 6.26% | 92.08% | the body, recovered from retail (was `// TODO: …; return mLookPos;`) |

The first three are instruction-identical to retail (branch displacements excluded).

### `__ct__11CBallCamera` — one identifier, 98.85% → 100%

The ctor passed `rstl::string("Ball Camera")` to `CGameCamera`. Retail calls the out-of-line
helper `string_l__4rstlFPCc`; we called the out-of-line
`__ct__Q24rstl66basic_string…FPCciRCQ24rstl17rmemory_allocator`, which additionally takes
`li r5,-1` / `addi r6,r1,8` and so materialises an extra 4-byte temporary. That shifted **every**
local in the 2136-byte ctor by 4 and cost 2 instructions. Writing `rstl::string_l(...)` — the
spelling the rest of the tree already uses (`CPathCamera.cpp:22`, `CFirstPersonCamera.cpp:24`) —
removed both. 536 → 534 instructions, exact.

**Lesson:** when a large function is a frame-layout near-miss, compare the *relocation symbols*
first (`objdump -r`): a ctor-parameter helper mismatch displaces every local in the function.

### `UpdatePlayerMovement` — 93.21% → 100%, three independent causes

1. **`mObtuseDirection`.** We wrote `mObtuseDirection = CMath::AbsF(CMath::FastArcCosR(dot)) > 1.7453293f;`,
   which MWCC compiles to `mfcr r3` + `rlwimi r0,r3,5,28,28` — a read of **CR5.LT**, a stale
   bit, not the `fcmpo cr0` result. Retail has a real branch plus
   `lbz r0,517(r30)` / `li r3,1` / `rlwimi r0,r3,3,28,28` / `stb`. Splitting into
   `dot = CMath::Limit(dot, 1.f);` then `if (… > 1.7453293f) { mObtuseDirection = true; }`
   reproduces it. Prime 1 has this shape.

2. **A dead `CVector2f` temporary.** Retail calls `__ct__9CVector2fFff` **twice**; we called it
   once. The extra one takes `(mBallDelta.GetX(), mBallDelta.GetY())` (`lfs f1,976(r30)` /
   `lfs f2,980(r30)`), writes an 8-byte slot at `r1+16`, and the slot is never read — MWCC keeps
   the out-of-line constructor call and drops the value. Declaring a named, never-read
   `const CVector2f` immediately after `CVector3f ballPos = player.GetBallPosition();` makes it
   appear, and it is what makes retail's frame 128 rather than our 112: with it retail's slot map
   (`r1+24` ballPos, `r1+16` dead temp, `r1+8` velocity temp, `r1+36` camToBallFlat) reproduces
   exactly.

3. **The live `CVector2f` must also be a named local.**
   `mBallVelFlat = CVector2f(v.GetX(), v.GetY()).Magnitude();` reuses `r3` across the constructor
   and the `Magnitude()` call and emits one `addi` too few; `const CVector2f velocityFlat(...)`
   plus `velocityFlat.Magnitude()` emits the second `addi r3,r1,8`.

   Both must be **named together** — measured, all four combinations:

   | dead temp | live temp | result |
   |---|---|---|
   | unnamed | unnamed | slots right, second `addi` missing (97%) |
   | unnamed | named | slots **swapped**, `addi` present (97%) |
   | named | unnamed | slots **swapped**, `addi` missing |
   | named | named | **exact** |

   Order matters too: the dead temporary must be declared **before**
   `mBallDelta = ballPos - mPrevBallPos;` (declared after, its constructor call is scheduled after
   the `fsubs` chain and the whole prologue diverges).

   Naming it is not dead-statement padding: the retail bytes contain the `bl __ct__9CVector2fFff`
   and the frame slot, and the value is provably unread.

### `ClampElevationToWater` — 6.33% → 100%, recovered from the stub

Retail (0x80060F50, 85 insns) — every call and constant identified from the relocations and from
`.sdata2` (`lbl_8041CC0C` = 0.25f, `lbl_8041CC10` = −0.12f, `lbl_8041CC14` = +0.12f):

```cpp
CVector3f CBallCamera::ClampElevationToWater(CVector3f position, CStateManager& mgr) const {
  CScriptWater* water =
      TCastToPtr<CScriptWater>(const_cast<CEntity*>(mgr.GetObjectById(Player(mgr).InFluidId())));
  if (water == nullptr) {
    water = TCastToPtr<CScriptWater>(const_cast<CEntity*>(mgr.GetObjectById(InFluidId())));
  }
  CVector3f ret = position;
  if (water != nullptr) {
    const float waterZ = water->GetTriggerBoundsWR().GetMaxPoint().GetZ();
    const float z = position.GetZ();
    const float dz = z - waterZ;
    if (z >= waterZ && dz <= 0.25f) { ret.SetZ(waterZ + 0.25f); }
    else if (z < waterZ && dz >= -0.12f) { ret.SetZ(waterZ - 0.12f); }
  }
  return ret;
}
```

Semantics: push the camera *away* from the water surface — anything within 0.25 above the
surface is lifted to surface+0.25, anything within 0.12 below is dropped to surface−0.12; anything
already further out is untouched. So it is a dead-band, not a clamp into the water.

Four spellings had to be measured together; none alone is right:

- **`const_cast<CEntity*>(mgr.GetObjectById(...))`** is required. `ClampElevationToWater` is
  `const`, so `mgr.GetObjectById` picks the const overload returning `const CEntity*`, which
  `TCastToPtr` cannot take. Retail's callee *is* the const `GetObjectById__13CStateManagerCF9TUniqueId`,
  so `const_cast` (not `ObjectById`) is the spelling. `CScriptActorRotate.cpp:42` does the same.
- **`CVector3f ret = position;` as a named local** is what makes MWCC hold x/y/z in `f31`/`f30`/`f29`
  and store once at the end. Writing `position.SetZ(...)` on the by-value parameter instead makes
  MWCC write back into the caller's buffer and re-load x/y from it (73 insns, and the wrong
  stores).
- **`const float z = position.GetZ();` and `const float dz = z - waterZ;` as separate named
  locals.** With `dz = position.GetZ() - waterZ` alone, MWCC folds the reload and emits
  `fcmpo cr0,f0,f1` where retail has `fcmpo cr0,f2,f1` (97.24%). Naming `z` separately puts the
  reload back.
- **Two `if`s joined by `&&`, the second an `else if`.** Retail's control flow re-tests
  `position.GetZ() >= waterZ` inside the second arm (`fcmpo cr0,f2,f1; bge`), which is what
  `if (A && B) … else if (C && D) …` produces. `if (A) { if (B) … } else if (C)` gives the same
  score with different registers, and two plain `if`s with no `else` are **98.82%** — one
  instruction short: retail has `b 2c60` after the first `SetZ`, the two-`if` form falls through
  into the second test.

`CScriptWater.hpp` and `CFirstPersonCamera.hpp` had to be added to the includes; both are already
in the DOL build.

### `GetScanObjectIndicatorPosition` — 6.26% → 92.08%, one register short

Recovered the same way and left in at 92.08% (the residual is register assignment, below):

```cpp
CStateManager& m = const_cast<CStateManager&>(mgr);
CVector3f ret;
if (Player(m).GetCameraState() == CPlayer::kCS_Spawned) {
  const CVector3f indicator =
      const_cast<CCameraManager&>(CameraManager(m)).FirstPersonCamera()->GetScanObjectIndicatorPosition(m);
  CVector3f delta = indicator - mLookPos;
  const CPlayer& player = Player(m);
  const float morph = player.GetMorphDuration() == 0.f
                          ? 0.f : CMath::Clamp(0.f, player.GetMorphTime() / player.GetMorphDuration(), 1.f);
  float t = 1.f - morph;
  t = CMath::Clamp(0.f, t, 1.f);
  if (mState == kBCS_FromBall) { t = 1.f - t; }
  ret = mLookPos + delta * t;
} else {
  ret = mLookPos;
}
return ret;
```

Facts the notes record so they need not be re-derived:

- The guard is `GetCameraState() == kCS_Spawned` (**4**), not `kCS_Ball`.
- The second camera is reached **virtually**: `lwz r4,24(r3)` then `lwz r12,0(r4)` /
  `lwz r12,92(r12)` / `mtctr` / `bctrl`. Offset 24 of `CCameraManager` is `mFpCamera`
  (`include/MetroidPrime/CCameraManager.hpp:149`) and vtable slot 92/4 = 23 is
  `CActor::GetScanObjectIndicatorPosition` (`include/MetroidPrime/CActor.hpp:97`). So the call
  is `CameraManager(m).FirstPersonCamera()->GetScanObjectIndicatorPosition(m)` and it must be a
  **virtual** call — spelling it through the accessor that returns a concrete type loses it.
  `CCameraManager.hpp:35` `FirstPersonCamera()` is the accessor; `GetBallCamera()` is at offset 28
  and does *not* match. `CFirstPersonCamera.hpp` must be included (the class is forward-declared
  in `CCameraManager.hpp`, and calling the method on it needs the definition).
- The morph factor is `CMath::Clamp(0.f, GetMorphTime() / GetMorphDuration(), 1.f)` with a
  `!= 0.f` guard on the divisor — `CPlayer.hpp:217-218` documents both members at 0x1138/0x113C
  and retail divides them.
- Retail loads `lwz r0,1224(r30); cmpwi r0,5` for the `kBCS_FromBall` test (1224 = `mState`).
- **`if (A) { … } else { ret = mLookPos; } return ret;` is required**, not an early
  `return mLookPos;`. With the early return, MWCC lays the `mLookPos` copy out inline at the top
  and branches over it (92.08% either way but 8 extra instructions and a different tail). The
  `ret` form is what puts retail's tail-duplicated `b 21c8` + out-of-line copy.

## Measured walls (do not re-try these)

- **`GetScanObjectIndicatorPosition` — 92.08%, 8 of 106 insns differ.** All eight are register
  choice, not structure: retail keeps the morph factor in `f2` and ours in `f6`, which shifts the
  `fmr`/`fcmpo`/`fmuls` register names and reverses the order of the three `fadds`. Tried and
  measured, none better than 92.08%: writing the second clamp as explicit
  `if (lerp < 0.f) … else if (lerp > 1.f) …` (85.33%); `const float lerp` / `const float t` /
  `const float k = mState == kBCS_FromBall ? 1.f - t : t` (92.08%, no change);
  `delta * t + mLookPos` instead of `mLookPos + delta * t` (92.08%, different register set);
  `CVector3f ret; if (A) ret = …` with the whole factor inlined into a ternary on `mState`
  (58.29% — duplicates the clamp). The call structure, the 26 relocations and the control flow
  are exact.

- **`MoveCollisionActor` — measured 82.60% on a clean tree (not the 82.85% quoted elsewhere),**
  189 of 213 insns still differ, in two independent ways:
  1. Retail's early out is `mr. r31,r3 ; beq 2b1c`, where `2b1c` is a **duplicate of the sret
     copy** placed immediately before the epilogue; ours inlines the 6-instruction copy at the
     branch site and does `b <epilogue>`. Not chased.
  2. `delta / dt` is **not** retail's spelling. Retail emits one `fdivs f5,f0,f31`
     (`1.0f / dt`, `lbl_8041CBE0`) and three `fmuls`; `CVector3f operator/`
     (`include/Kyoto/Math/CVector3f.hpp:184`) emits three `fdivs`. Writing
     `ComputeVelocity(oldVelocity, delta * (1.f / dt), dt)` reproduces the reciprocal exactly and
     is strictly closer structurally, but on its own it does **not** raise the percentage
     (82.60% → 82.61%; frame is 176 in both). I reverted it to keep the diff to what counts and
     record it here as the known next step. With a named `const CVector3f scaledDelta` it
     measures 82.60%.
  3. Retail word-copies the **velocity** argument (`lwz`/`stw` ×3 through `r1+108..119`) and
     float-stores the scaled delta into `r1+84..92`; ours does the reverse. Same root cause as
     run 1's `ResetToTweaks` staging note.

- **`TeleportCamera(const CTransform4f&, CStateManager&)` — 97.00% is confirmed and is still a
  wall.** The *only* relocation difference is
  `GetCameraManager__11CGameCameraCFRC13CStateManager` (retail) vs
  `CameraManager__11CGameCameraCFRC13CStateManager` (ours); swapping the accessor in the source
  fixes the relocation and leaves the score at 97.00% (measured this run), because MWCC still
  materialises two `TUniqueId` slots. `mUniqueId` is `protected` in `CEntity`, so
  `UpdateCameraTriggers(mUniqueId, mgr)` does not compile ("illegal access to protected/private
  member") — there is no spelling that reaches `GetUniqueId()`'s single temporary.

- **`AcceptScriptMsg` — 94.63%, 216 retail insns vs 213 ours.** Everything run 1 recorded holds;
  the extra measurement is that retail's frame is **240** against our **176** — 64 bytes, roughly
  four class temporaries of staging around the two `CMaterialFilter::MakeIncludeExclude` /
  `CMaterialList` temporaries, not just the `msg.GetMessage()` hoist into `r25`. That is bigger
  than a register-allocation fix.

- **`ResetToTweaks` — 92.94% re-measured, unchanged;** the word-wise staging of
  `GetBallCameraOffset()` into `r1+16..27` is still the only real difference (frame 96 vs our 80
  follows from that 12-byte temporary). Run 1's four spellings all give the direct `lfs`/`stfs`
  copy.

- **`UpdateUsingFreeLook` — 68.92%, untouched.** Retail's frame is 352, ours 320 — the same
  "two class temporaries we do not have" signature that took `UpdatePlayerMovement` to 100%. Two
  dead copies are visible in the prologue: `ballPos` is staged into `r1+212..224` (never read)
  and the `ZRotation` result is copied `r1+184..196` → `r1+196..208` (never read), and
  `CQuaternion::ZRotation(CRelAngle::FromRadians(mFreeLookYawDelta))` is hoisted to the very top
  of the function, above the `mLookPos` update. First divergence is there; the rest of the 277
  insns is quaternion arithmetic I did not touch.

WALL: GetScanObjectIndicatorPosition(const CStateManager&) 92.08% - 8 of 106 insns differ, all
register choice (factor in f6 vs retail's f2); 5 spellings tried, call structure and control flow
already exact.
WALL: MoveCollisionActor(const CVector3f&, float, CStateManager&) 82.60% - needs both the tail-duplicated
early-out sret copy and retail's word-wise staging of the velocity argument; `delta * (1.f/dt)` is
confirmed correct but alone changes the score by 0.01.
WALL: ResetToTweaks(CStateManager&) 92.94% - retail's 12-byte word-wise staging of
GetBallCameraOffset() cannot be reproduced; the four assignments run 1 tried all give the direct
lfs/stfs copy.
WALL: TeleportCamera(const CTransform4f&, CStateManager&) 97.00% - MWCC always materialises a
second TUniqueId slot; mUniqueId is protected, so no spelling reaches retail's single temporary.
WALL: __sinit_CBallCamera_cpp 70.18% - re-measured unchanged; retail materialises both
skLineOfSightFilter CMaterialLists through R_PPC_EMB_SDA21 globals before storing them.

## Notes for the next run

- **Check the relocation symbols before the disassembly.** `rstl::string` → `rstl::string_l` was
  the whole 2136-byte ctor, and on `GetScanObjectIndicatorPosition` the relocation named the
  callee, the vtable slot and therefore both the accessor (`FirstPersonCamera`, not
  `BallCamera`) and the fact that the call had to stay virtual. A per-function
  `objdump -d -r | sort` diff of the two objects' relocations, filtered to functions under 100%,
  finds the "wrong helper / wrong overload / wrong target" cases in one pass; the symbol-delta
  list for this unit is short and worth re-running before touching any body.
- The two stub recoveries above show the cheap shape of a `progress` win on a unit full of stubs:
  identify the callees from the relocations, read the float constants out of
  `main.elf`'s `.sdata2` (`objdump -s -j .sdata2`), and reconstruct. Both are then ~90 instructions
  and reach 100% once the **named-local shape** is right.
- **The recurring lever is still "is it a named local".** Every function that moved here moved
  because of a name, not because of the arithmetic: the dead `CVector2f`, `velocityFlat`,
  `const float z`/`dz`, `const CVector2f ret = position`, `CVector3f ret` for the tail.
