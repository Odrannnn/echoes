# progress-unit-cplayer

`kind: progress`, target `MetroidPrime/Player/CPlayer`. One file changed:
`src/MetroidPrime/Player/CPlayer.cpp`. No `tools/`, no `config/`, no `build/goal/` edit
beyond this notes file.

## Result, measured

`build/report.json` on the clean tree before this run: **52 / 228** functions matched.
After: **60 / 228**. Eight functions went from sub-100% to an exact match:

| function | bytes | before | after |
| --- | --- | --- | --- |
| `SetAimTarget__7CPlayerF9TUniqueId` | 48 | 99.17% | 100% |
| `SetSteam__Q27CPlayer11CVisorSteamFfffUi` | 48 | 91.25% | 100% |
| `IsMorphBallTransitioning__7CPlayerCFv` | 40 | 58.70% | 100% |
| `IsEnergyLow__7CPlayerCFv` | 120 | 62.30% | 100% |
| `fn_80019360__7CPlayerCFv` | 40 | 18.90% | reverted, see below |
| `SetMorphBallState__Q27CPlayer21EPlayerMorphBallState...` | 88 | 85.59% | 100% |
| `GetOrbitPosition__7CPlayerCFRC13CStateManager` | 64 | 35.88% | 100% |
| `SetHudDisable__7CPlayerFfff` | 60 | 25.87% | 100% |
| `GetDamageVulnerability__7CPlayerCFRC9CVector3fRC9CVector3fRC11CDamageInfo` | 300 | 97.20% | 100% |

Net **+8** (the `fn_80019360` row is the one that did not land; the other eight did).

`./tools/goal_check.sh build/goal/item.json` in the worktree: **PASS**, with

```
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 11526 -> 11534   linked 5625 -> 5625
ok    check_symbol_names.py
ok    All:  33.03% fuzzy, 25.92% matched, 12.24% linked (11534 / 28465 functions)
ok    target rose: main/MetroidPrime/Player/CPlayer: 52 -> 60 / 228 functions
ok    no asm added
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged.
`docs/HANDOFF.md`'s state block was rewritten by `tools/gate.sh` itself (it runs with
`MP_GATE_DOCS_WRITE=1`); that is the judge's own doing, not a hand edit.

## What each change was, and the retail bytes behind it

Method per function: read the retail range out of the linked ELF with `tools/dis.sh`,
compare against our object's same function with a side-by-side `diff -y`, change one
spelling, rebuild with `tools/fast_try.sh MetroidPrime/Player/CPlayer` (a couple of
seconds), re-diff. The helper lived in `/tmp/opencode/cmpfn.sh`; the diffs quoted below
are the instruction sequences.

### `SetAimTarget` (99.17% -> 100%)

Only the *order of the operands* of the second compare differed:

```
retail: lhz r0,1548(r3) ; cmplw r0,r5 ; beq
ours : lhz r0,1548(r3) ; cmplw r5,r0 ; beq
```

so retail compares `mAimTarget` against the argument, not the argument against
`mAimTarget`. `mAimTarget != target` instead of `target != mAimTarget` is the whole
fix. (`cmplw` is not commutative in its encoding even though it is in its result, so this
is one of the few places operand order alone decides a match.)

### `IsMorphBallTransitioning` (58.70% -> 100%)

Retail does a range test, not two equality tests:

```
retail: lwz r0,908(r3) ; cmpwi r0,4 ; bge end
                 cmpwi r0,2 ; bge -> 1
                 b end
```

i.e. the *unsigned* two-sided form, which is what a `switch` over the whole enum lowers
to. Prime 1's decomp already spells it as a `switch` (`prime-ref/src/MetroidPrime/Player/CPlayer.cpp:469`);
copied verbatim and it matches:

```cpp
switch (mMorphBallState) {
case kMS_Morphing:
case kMS_Unmorphing:
  return true;
default:
  return false;
}
```

### `SetSteam` (91.25% -> 100%)

The body was right but written as an early return, which gave
`cror eq,lt,eq ; beqlr`. Retail has a bare `blelr`, so the condition is the positive one:

```cpp
if (mNextTexture == kInvalidAssetId || targetAlpha > mNextTargetAlpha) { ...assign... }
```

Same logic as Prime 1 (`prime-ref/.../CPlayer.cpp:1117`), same three stores.

### `IsEnergyLow` (62.30% -> 100%)

Two things: the threshold comparison and the call order.

* the vtable call at 0x3C must happen **before** `GetItemCapacity`, and its result must
  be kept in `f31` across the second call - so the HP is loaded into a local first;
* the capacity test is `cmpwi r3,4 ; blt` -> `>= 4 ? 100.f : 30.f`. The old `> 3`
  compiles to `cmpwi r3,3 ; ble`, one byte off and a whole branch shape out.

```cpp
const float hp = GetHealthInfo()->GetHP();
const int tanks = mPlayerState->GetItemCapacity(CPlayerState::kIT_EnergyTanks);
return hp < (tanks >= 4 ? 100.f : 30.f);
```

The three SDA2 constants are already in the link (0x8041A484 = 30.f, 0x8041A488 = 100.f);
only the shape was wrong.

### `SetMorphBallState` (85.59% -> 100%)

Retail's tail is a `switch` with **all four** enumerators listed and no `default`:

```
retail: cmpwi r4,3 ; beq end ; bge end ; cmpwi r4,0 ; beq end ; bge -> call ; b end
```

which is `state == kMS_Morphing` reached through a switch that has a case for every
value. Three shapes were tried and measured:

| spelling | result |
| --- | --- |
| `if (state == kMS_Morphed \|\| state == kMS_Morphing)` (the old one) | 85.59% |
| `if (state == kMS_Morphing)` | 85.59%, `cmpwi r4,2 ; bne` |
| `switch` with a `default:` and the load under `kMS_Morphed` | 85.59% |
| `switch` listing all four cases, load under `kMS_Morphing` | **100%** |

The winning shape:

```cpp
switch (state) {
case kMS_Unmorphed:   break;
case kMS_Morphed:
case kMS_Morphing:    mMorphBall->LoadMorphBallModel(); break;
case kMS_Unmorphing:  break;
}
```

Omitting `default` is what makes the compiler emit the range check; with a `default` it
knows the value is unconstrained and drops it.

### `GetOrbitPosition` (35.88% -> 100%)

The ternary `cond ? GetBallPosition() : GetEyePosition()` forces a temporary: the
compiler materialises each result in a stack slot and copies it to the return slot
afterwards, which is 8 extra instructions and a 48-byte frame. Retail's is 16 bytes and
each arm tail-calls. Two separate `return` statements, and nothing else:

```cpp
if (mMorphBallState == kMS_Morphed) { return GetBallPosition(); }
return GetEyePosition();
```

**Generalisable:** a `?:` over two class-returning calls is not the same function as the
two `return`s, in codegen terms. Worth trying before a return-value class is rewritten.

### `SetHudDisable` (25.87% -> 100%)

The three stores were all we had; the tail was missing. Retail 0x800100AC:

```
stfs f1,4424 ; stfs f2,4428 ; stfs f3,4432
lfs f1,0.0f ; fcmpu f1,f0[4428] ; bnelr          <- mStaticOutSpeed != 0 -> return
lfs f0,4424 ; fcmpu f1,f0 ; bne -> store f1 (0.0f)
                             lfs f0,1.0f ; stfs f0,4436
```

Same shape as Prime 1's (`prime-ref/.../CPlayer.cpp:2534`). Note the ternary
`mStaticTimer == 0.f ? 1.f : 0.f` compiles to a *reload of 0.0f into f1* and a store of
f1 - the wrong register. Writing the two `if`/`else` arms out is what matches.

### `GetDamageVulnerability(const CVector3f&, const CVector3f&, const CDamageInfo&)` (97.20% -> 100%)

The `||` of two early returns was the only difference, and it showed up as an inverted
first branch:

```
retail: fcmpo cr0,f1,f0 ; ble  -> skip the immune return
ours : fcmpo cr0,f1,f0 ; bgt  -> jump to the immune return
```

Same test, opposite polarity, so every branch in the function landed 0xC bytes off.
Splitting into two `if` blocks gives the early-return form retail has:

```cpp
if (mInvulnerabilityTimer > 0.f) { return &mImmuneVulnerability; }
if (mPlayerState->GetItemAmount(CPlayerState::kIT_Invincibility, true) != 0) {
  return &mImmuneVulnerability;
}
```

## Reverted: `fn_80019360` - a port-gap failure, not a decompilation failure

It matched 100% (18.90% -> 100%, +1) with this body:

```cpp
return mCameraManager->FirstPersonCamera()->GetGunFollowTransform();
```

which is exactly retail's three instructions (`lwz r3,4888(r3)` = mCameraManager at
0x1318, `lwz r3,24(r3)` = `CCameraManager::mFpCamera` at 0x18, tail call to
`CFirstPersonCamera::GetGunFollowTransform`).

**It fails the gate anyway**, and the reason is worth recording because it will bite any
lane that writes a call into a unit in the port build:

```
port link gap   246  MISSING
link gap not accounted for:
  gap grew: _ZNK18CFirstPersonCamera21GetGunFollowTransformEv is not in port_link_gap_list.md
GATE FAIL: link-gap
```

`src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` is **not in `files.cmake`**, so it is
not in `MP_GAME_SOURCES` and not compiled into `mp_game`. Its symbols are only reachable
today through reach stubs, which the real link does not use. Any cross-unit call to it
from a unit that *is* in the port build therefore adds a genuinely missing symbol, and
`tools/link_gap.py` fails the gate on a grown gap - correctly, since the gap did grow.

So the body is reverted to `return GetTransform();` with a comment recording the real
spelling and why it is not used yet. **The match is still available**: it needs
`CFirstPersonCamera.cpp` in `files.cmake` (or the function defined in a unit that is),
after which the one-line body above lands immediately. `CFirstPersonCamera.cpp` is
`Object(NonMatching, ...)` in `configure.py:476` and compiles clean.

## Tried and left at sub-100% (spellings, so the next run does not repeat them)

### `GetDeathAlpha` 95.758% (132 B) - a register allocation wall

The logic is right; only the register assignment of the two `max_val`s differs. Retail
keeps `elapsed` in **f1** and `duration` in **f2**:

```
retail: lfs f2,4772 ; lfs f1,4724 ; lfs f0,0.0f ; fsubs f1,f2,f1 ; fcmpo f0,f1 ; ...
                                               fcmpo f0,f2 ; ... fdivs f1,f1,f2
```

Five spellings, all measured, none reaches 100%:

| spelling | generated |
| --- | --- |
| `rstl::max_val(0.f, mDeathTime - mDeathFadeDelay)` (original) | `fsubs f2,f2,f1`; result in f2, duration in f1 |
| `max_val` with the subtraction as a separate `const float t` | `fsubs f1,f2,f1` (right!) but the divide becomes `fdivs f1,f2,f1` |
| the two `const float`s declared in the other order (duration first) | `f3` used for duration; `fdivs f1,f1,f3` |
| `CMath::Max(a, b)` / `CMath::Min` (which take `const T&` and compare `a > b`) | **outlines the accessors** - `bl` to a local copy, 4 extra calls, a `-48(r1)` frame and an f31 save. Much worse, 95.758% but a different wrong answer |
| one nested expression, no temporaries | `f3` again |

**Lesson:** `CMath::Max`/`CMath::Min` are *not* a drop-in for `rstl::max_val`/`min_val`
here. They return `const T&`, and mwcceppc then materialises the arguments. For a
scalar clamp, `rstl::max_val` is the one that inlines.

### `__ct__CDamageVulnerability(const CDamageVulnerability&)` 82.609% (92 B)

The copy constructor is **not in any source file** - it is emitted implicitly by
`CCollisionActor.cpp`, `CTargetReticles.cpp` and `CPlayer.cpp`, all three carrying the
same weak symbol. Retail's version (0x8001C634) is `__copy(dest, src, 21)` followed by
`__copy(dest+21, src+21, 3)` and then a call to the vector's copy at `+32`; ours copies
21 bytes and then jumps straight to the tail, because our layout puts the two 4-byte
words at +24/+28 and the vector at +32 with nothing in between. The 3-byte copy at +21
is the `uchar x15[3]` filler in `CDamageVulnerability`. This needs a header/ABI
decision, not a body change - and it would have to be made in all three TUs at once or
the three weak copies stop agreeing. **Not attempted; it is not a one-item job.**

### `DetachActorFromPlayer` 54.545% (44 B)

Retail's tail is `mGun->SetActorAttached(false)`, which is
`lbz r0,942(r3) ; rlwimi r0,r4,6,25,25 ; stb r0,942(r3)` - clearing **bit 25** of the
byte at `CPlayerGun+942`. Our `CPlayerGun` has no such member: the closest is Prime 1's
`bool mActorAttached : 1;` (`prime-ref/include/MetroidPrime/Player/CPlayerGun.hpp:453`),
and Echoes' bitfield block sits at a different offset, so adding it is a **layout change
to a shared class** (the recurring blocker in `docs/research/raw_offsets.md`, rule 2).
Left alone.

### `GetDamageVulnerability()` (no-arg) 29.717% (184 B)

The no-arg overload forwards to the three-arg one with
`CVector3f::Zero(), CVector3f(0,0,1), CDamageInfo()`. Retail 0x8000ED34 is 184 bytes
where ours is a short forward, so retail's is *not* a forward - it constructs a
`CDamageInfo` and does the same chain itself, or calls something else. Not investigated;
the three-arg one is done and this one is a separate read.

### `GetCombatMode` / `GetExplorationMode` 13.333% (60 B each)

Both are the same shape over `CPlayerGun`: `lwz r3,3772(r3)` (= `CPlayer::mGun` at
0xEBC) then `lwz r0,936(r3)` and the same four-way range test as
`IsMorphBallTransitioning`. They differ only in which values map to 1. Both are one
`switch` away, but the member at `CPlayerGun+936` is not identified in our header, so
there is nothing to switch on yet. Cheap once someone names that member.

### `fn_80019360` - see above, reverted, one line away from 100%.

## NEW:

None filed. The two things worth another lane are not new units and not new work
discovered here:

* `fn_80019360` needs `src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` in `files.cmake`.
  That is a build-list change to an existing unit, and `files.cmake` also has
  `tools/check_files_cmake.py` and the decomp `files.cmake` (a different file - the port
  one is the only one CMake reads) to keep in step. **NEW: port-files-cmake-first-person-camera
  | progress | MetroidPrime/Player/CPlayer | fn_80019360 matches 100% with
  `mCameraManager->FirstPersonCamera()->GetGunFollowTransform()` but the call adds a
  port-undefined symbol because CFirstPersonCamera.cpp is not in files.cmake; adding it
  makes the function match immediately.**
* `CPlayerGun`'s bitfield block: `DetachActorFromPlayer` (54.5%), `GetCombatMode` and
  `GetExplorationMode` (13.3% each) all read bits our `CPlayerGun.hpp` does not name -
  byte 942 bit 25, and the 4-byte word at 936. **NEW: progress-unit-cplayergun-bits |
  progress | MetroidPrime/Player/CPlayerGun | three CPlayer functions are blocked on
  naming CPlayerGun's 0x3A8/0x3AE members; the class is 0x814 bytes and its bitfield
  block needs an offset audit before any member is added.**
