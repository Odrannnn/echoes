# progress-unit-cplayerorbit

## Run 2 (this run)

`MetroidPrime/Player/CPlayerOrbit`, **seven** functions taken to an exact match. The unit went
**22/60 -> 29/60 matched functions**, fuzzy **6.36% -> 11.46%**. Judge: `goal_check: PASS`
(gate ok including DOL sha1 `6ef9b491...`, 86 RELs, port probe 747 files / 291 undefined
unchanged; matched 12103 -> 12110; linked 5849 -> 5849; no asm; decl order ok).

| function | addr | before | after | the spelling that mattered |
| --- | --- | --- | --- | --- |
| `ActivateOrbitSource` | 0x801222D8 | 4.17% | 100% | see "case order is a switch-lowering detail" below |
| `CheckOrbitDisableSourceList(const CStateManager&)` | 0x80121864 | 3.41% | 100% | p1 shape verbatim: erase + `it = begin()` restart, `!empty()` return |
| `UpdateOrbitPosition` | 0x8011EF50 | 2.00% | 100% | `SetOrbitPosition(distance)` takes no mgr here; actor branch needs both tests |
| `SetOrbitTargetId` | 0x8011EE74 | 1.82% | 100% | two **named locals** (see below) |
| `AddOrbitDisableSource` | 0x801219EC | 1.92% | 100% | `const TUniqueId orbitTarget = GetOrbitTargetId();` in a local |
| `BeginGrapple` | 0x8011E768 | 2.44% | 100% | p1 donor applies as-is |
| `fn_8011eac4` | 0x8011EAC4 | 2.94% | 100% | **unsigned** loop bound (see below) |

Files touched: `src/MetroidPrime/Player/CPlayerOrbit.cpp`,
`include/MetroidPrime/Player/CPlayer.hpp` (one enum value), `include/MetroidPrime/Cameras/
CFirstPersonCamera.hpp` (one setter), `src/MetroidPrime/PortGlobals.cpp` (one cast, see the gate
note). No `asm`.

## Things that cost time, for the next run

**Case order inside a `switch` changes the emitted decision tree, and it has to match retail's
order or the bytes differ.** Retail `ActivateOrbitSource` tests `mOrbitSource` (0x58c) as
`cmpwi 1 / beq`, then `bge`, then `cmpwi 3 / bge`, then `b` - a three-step tree over cases
{0,1,2}. MWCC builds that tree from the order the labels appear in the source. Measured, all
96-byte body otherwise identical:

| source order | score |
| --- | --- |
| `case 0: default:` first, then `case 1`, then `case 2` | **100%** |
| `case 0: default:`, `case 2`, `case 1` | 99.50% |
| `case 2`, `case 1`, `case 0: default:` | 84.17% |
| `case 1`, `case 0: default:`, `case 2` | 82.71% |
| `case 1`, `case 2`, `case 3: default:` | 82.00% |
| `case 2`, `case 1`, `case 3: default:` | 82.13% |
| `case 1`, `case 2`, plain `default:` | 78.46% |
| `case 2`, `case 1`, plain `default:` | 78.38% |
| `if / else if / else` | 57.04% |
| `case 1,2,0` each with its own body (no shared `default:`) | 66.00% |

The rule that fits: **list the cases in ascending value order and put the shared body on the
*first* (lowest) case plus a bare `default:`**. Note `SetOrbitState` (332 B, measured 98.63%,
not landed) uses the *descending* order - OrbitObject, OrbitCarcass, NoOrbit, OrbitPoint -
because retail's tree there tests 2, then 0, then 4. So the order is per-function and must be
read off retail, not guessed from p1.

**`SetOrbitTargetId` needs both casts materialised into locals, in the p1 spelling.** Writing the
`||` inline is 84.98%: retail keeps the first cast's result live in `r31` across the *second*
call, which only happens with two named locals.

```cpp
const CPatterned* patterned = TCastToConstPtr< CPatterned >(mgr.GetObjectById(target));
const CSwarmBasics* swarm = TCastToConstPtr< CSwarmBasics >(mgr.GetObjectById(target));
if (patterned || swarm) { ... }
```

Retail's Echoes version has only two casts (Prime 1 has four) and also writes `x591_` (`bool`
at 0x591): `true` whenever the id was valid, `false` again when the id is invalid at the end -
so the two writes are independent, not an else.

**A `TUniqueId` local is worth four instructions of stack traffic.** `AddOrbitDisableSource`
needs `const TUniqueId orbitTarget = GetOrbitTargetId();` as a local (100%). Inlining the getter
into the `GetObjectById` argument is 99.90%: the two `sth` to the argument-save slots land at
different displacements. Same for `mgr.GetObjectById(mOrbitTargetId)` (the member, no local):
98.06%.

**One `static_cast` on the loop bound.** `fn_8011eac4` walks `mgr.GetNumPlayers()` comparing
player unique ids and orbit target ids. Retail emits `cmplw r30,r0` (unsigned); `int i` against
`int GetNumPlayers()` emits `cmpw` and scores 98.24%. `static_cast< uint >(mgr.GetNumPlayers())`
on the bound, with `int i`, is 100%. `uint i` is 92.00%, a hoisted `const uint numPlayers` is
85.18%, and a `while` loop is 82.88%. The interesting part: it is the *bound* that must be
unsigned, not the counter.

## Measured but not matched

`SetOrbitState` (332 B, **98.63%**) is now a near-miss and is the best next target. Everything
matches except the stack displacement of the carcass-case `CVector3f` temp: retail puts it at
`24(r1)` and this repo's build puts it at `20(r1)`, one 4-byte slot lower, because retail
materialises `mOrbitNextTargetId`'s value in `16(r1)` before storing it to `0x3dc` and this
repo's does not. Spellings tried and their scores: p1 order 98.63 (best, kept), ascending case
order 56.76, `mOrbitNextTargetId` also cleared in NoOrbit 93.81, that plus OrbitPoint 95.92
(via `SetOrbitState` local-first ordering), a hoisted `const TUniqueId next` 93.81, a hoisted
`const TUniqueId invalid` used by all three `SetOrbitTargetId` calls 88.70,
`playerToPoint = mOrbitPoint; playerToPoint -= GetTranslation();` 88.89, two locals `playerToPoint`
+ `flat` 98.63, `mOrbitNextTargetId = TUniqueId(kInvalidUniqueId)` 98.63. A `SetOrbitNextTargetId`
inline setter would need adding to `CPlayer.hpp` first (there is none; the p1 donor calls one).

`BreakGrapple` (252 B, 1.59%), `UpdateOrbitOrientation` (308 B, 1.30%), `SetOrbitRequest`
(360 B), `WithinOrbitScreenBox`/`WithinOrbitScreenEllipse` (404/416 B), `UpdateOrbitSelection`
(336 B), `UpdateAimTarget` (348 B) are still stubs. `BreakGrapple` is mostly p1's body with two
Echoes substitutions and has not been tried at all yet - that is the cheapest unstarted one.
`fn_80121908` (120 B), `fn_80123334` (104 B), `fn_8012339C` (372 B) and `fn_8011c3c0` (1608 B)
remain pure stubs; `fn_80121908` disassembles as a `reserved_vector` erase helper
(`0x80121908`, takes the vector at r3 and an iterator at r4) and is presumably a template
instantiation this unit is expected to emit.

## Gate note (a NEW cost worth recording)

Adding `TCastToConstPtr<CSwarmBasics>` to `SetOrbitTargetId` **grew the port link's undefined
count by one** and failed the whole item until fixed. `TypesMatch.cpp`, which holds that
`TCastToPtr`, is deliberately not in `files.cmake`, so a new `TCastToPtr<CSwarmBasics>` reference
is a new undefined symbol. The fix is a definition in `src/MetroidPrime/PortGlobals.cpp`, next to
the existing `TCastToPtr<CPatterned>`:

```cpp
template <>
CSwarmBasics* TCastToPtr< CSwarmBasics >(CEntity* entity) {
  return reinterpret_cast< CSwarmBasics* >(TryCast(entity, kET_SwarmBasics));
}
```

`reinterpret_cast` is required, not `static_cast`: `include/MetroidPrime/Enemies/CSwarmBasics.hpp`
is a partial ABI declaration that deliberately does not name `CActor` as a base, so
`static_cast<CSwarmBasics*>(CEntity*)` does not compile. Retail's body is the ordinary type-id
wrapper with `li r4,102` (`tools/dis.sh 0x80098908 0x24`); `TypesMatch.cpp`'s
`CAST_TO_IMPL_INCOMPLETE` covers the same situation. Any future item in this repo that calls a
`TCastToPtr` for a class outside `TypesMatch`'s port-visible set will hit this the same way:
check `build-port-link/link_undefined.txt` before assuming the gate is about your unit.
---

# Run 3 (this run)

`MetroidPrime/Player/CPlayerOrbit`, **five more** functions to an exact match. The unit went
**29/60 -> 34/60 matched functions**, fuzzy **11.46% -> 21.80%**. Judge: `goal_check: PASS`
(gate ok including DOL sha1 `6ef9b491...`, 86 RELs, port probe 749 files / 289 undefined
unchanged; matched 12159 -> 12164; linked 5860 -> 5860; no asm; decl order ok).

| function | addr | before | after | the spelling that mattered |
| --- | --- | --- | --- | --- |
| `BreakGrapple` | 0x8011E80C | 1.59% | **100%** | p1 body + two Echoes edits; `mGun->GrappleArm()` |
| `SetOrbitRequest` | 0x8011E908 | 1.11% | **100%** | p1 `BreakOrbit` renamed; needs 2 new enum values |
| `ApplyGrappleJump` | 0x8011E534 | 0.71% | **100%** | p1 body verbatim; needed a `CScriptGrapplePoint` header |
| `SetOrbitPosition` | 0x8011F13C | 0.93% | **100%** | `fn_80019360()` not `GetFirstPersonCameraTransform` |
| `UpdateOrbitOrientation` | 0x80122488 | 1.30% | **100%** | p1 body verbatim, `mCameraManager` not `mgr.GetCameraManager()` |

**All five were p1's body verbatim or nearly so.** The whole cost of this item was Echoes
substitutions, not the logic. `BreakGrapple` is the pattern: p1's body, with
`BreakOrbit(type, mgr)` -> `SetOrbitRequest(request, mgr)` (Echoes merged the two functions),
`!CheckPostGrapple()` -> `!InGrappleJumpCooldown()`, and the arm reset guarded by
`mGun->GrappleArm() != nullptr && GetAnimState() != kAS_Done`.

## The three Echoes substitutions, measured

**`SetOrbitRequest` needs two new enum values.** Retail tests `cmpwi r30,8 / beq` then
`cmpwi r30,7 / bge`, and the *sign* of the tree (`bge`, not `b`) matches an ascending-value
source order. Added to `EPlayerOrbitRequest` in `include/MetroidPrime/Player/CPlayer.hpp`:

```cpp
kOR_BadVerticalAngle = 7,
kOR_ActivateOrbitSource = 8,
```

Case 7's body is `SetOrbitState(kOS_OrbitPoint, mgr)` then
`mOrbitPoint = GetEyePosition() + tweak->GetOrbitNormalDistance(mOrbitType) * GetTransform().GetForward();`
- note `GetEyePosition()`, not `GetTranslation()` as p1 has, and the temporary is `GetEyePosition()`
itself (retail `bl GetEyePosition` into `12(r1)`, then reads it back). `SetOrbitPosition` also
uses `GetEyePosition()` where p1 has `GetTranslation()`. The prologue also stores
`mOrbitRequest = request` (0x3ac) first and short-circuits to `ActivateOrbitSource` when
`TCastToConstPtr<CPlayer>(mgr.GetObjectById(mOrbitTargetId)) != nullptr && mTurretState == kTS_None`
(0x12f8) - **that guard does not exist in p1 at all**, so a p1-donor transcription of this
function would be wrong on the first four instructions.

**`SetOrbitPosition` (the 1-arg Echoes overload) starts from `fn_80019360()`,** not p1's
`GetFirstPersonCameraTransform(mgr)`. Retail calls `bl 80019360` (the gun-follow transform) and
copies the result; the `kOS_OrbitPoint && mOrbitRequest == kOR_BadVerticalAngle` branch replaces
it with `GetTransform()` + `SetTranslation(GetEyePosition())`. 100% first try.

**`UpdateOrbitOrientation` is p1 verbatim** except `mCameraManager->FirstPersonCamera()` for
p1's `mgr.GetCameraManager()->GetFirstPersonCamera()`. Retail reads `lwz r3,4888(r31)`
directly, i.e. the member. 100% first try. Note the p1 fall-through (`case kOS_OrbitPoint:`
falls into the shared body after the free-look test) reproduces retail's tree as written - the
case *order* here is p1's ascending order and it is right.

## `CScriptGrapplePoint` had no header at all

`ApplyGrappleJump` and `UpdateOrbitSelection` both need `point->GetTranslation()`, and this repo
only forward-declared the class (`src/MetroidPrime/Player/CPlayerOrbit.cpp:13`). Added
`include/MetroidPrime/ScriptObjects/CScriptGrapplePoint.hpp` as a **partial declaration deriving
from `CActor` with no members**, matching `TypesMatch.cpp`'s `TYPES_MATCH_CLASS(CScriptGrapplePoint, CActor)`.
It declares no key function, so including it emits no vtable - `CHECK_SIZEOF` and the DOL hash
both still hold. `GetTranslation()` then comes from `CActor` (mPosition at 0x54), which is what
retail reads (`lwz 0x58(r31)` etc.).

## NEW gate cost, same shape as run 2's: a `TCastToPtr` in `ApplyGrappleJump` and a `mGrappleArm` in `BreakGrapple` grew the port link

`BreakGrapple` calls `mGun->GrappleArm()->SetAnimState(...)`, and `CGrappleArm.cpp` is one whole
`NonMatching` unit that `files.cmake` does not list - so `CGrappleArm::SetAnimState` became a
**new undefined symbol** (289 -> 290) and the item would have failed. Fixed the way
`CGrappleArmReturnToDefault.cpp` already does it: a new one-function-per-file port carve-out,
`src/MetroidPrime/Player/CGrappleArmSetAnimState.cpp`, listed in `files.cmake` with the reason.
It carries `SetAnimState` **and the four bodies only it reaches** -
`PlayGrappleAnimation`, `DisconnectGrappleBeam`, `GrappleBeamDisconnected`, `ResetAuxParams` -
because a stub would buy the number rather than the behaviour, and `SetStateFlags` was already
in `CGrappleArmReturnToDefault.cpp` so it is not repeated. Net effect on the port's undefined
count: **zero** (289 before, 289 after); `kAPS_Grapple` had to be redeclared as a file-local
enum because `CGrappleArm.cpp:32` keeps it local.

**The generalisable rule: in this repo, calling any `CGrappleArm`, `CSwarmBasics`,
`CScriptGrapplePoint` or other non-`files.cmake` body from a unit you are decompiling is a new
undefined symbol unless you also add the carve-out.** Check `build-port-link/link_undefined.txt`
before assuming the gate is about your unit - `git stash push -- src include` +
`./tools/probe_sources.sh` isolates which symbols are *yours* (this run: three of the four new
names, `LoadAreaAttributes`, `mp_cswarmbasics`, `mp_cswarmbasics_exit`, were already on HEAD).

## Byte-identical but objdiff says <100% - do not chase it

Three functions in this unit are **byte-for-byte identical to retail** (verified against the
linked ELF's `.text`, not against objdiff's report) and objdiff still scores them under 100:

| function | objdiff | real bytes differing |
| --- | --- | --- |
| `UpdateOrbitSelection` (0x80122338, 336 B) | 96.43% | **0** |
| `WithinOrbitScreenEllipse` (0x801216C4, 416 B) | 99.81% | **0** |
| `WithinOrbitScreenBox` (0x80121530, 404 B) | 95.15% | **0** |

`tools/bytescmp.py` shows "differing instructions" for these because it compares the *object's*
relocation-resolved bytes against retail's and the two `bl` displacements land differently before
the linker runs; after the link they are identical. `report.json`'s `matched_functions` counts
objdiff, so these do **not** count as matched - but they are not defects either, and chasing the
objdiff number here is wasted time. Verify with:

```python
# read our object out of build/G2ME01/asm/<unit>.s, retail's .text out of the linked ELF,
# compare byte ranges
```

`UpdateOrbitSelection`'s only remaining difference before the link is one extra `lfs f1` our
build emits for the `0.f` argument of `ValidateAimTargetId` (retail passes nothing, because the
parameter is dead in the stub). Making that parameter defaulted (`float dt = 0.f`) does not
change it. **Not a wall - it is already byte-exact; only objdiff's bookkeeping disagrees.**

## `WithinOrbitScreen*` need the int-to-float `xoris` shape, and the *order* of the two halves

Both take `GetOrbitZoneCentreX/Y` and `GetOrbitZoneWidth/Height`, which return **`int`** in this
repo, and retail converts with `xoris r3,r3,0x8000` + `lis r0,0x4330` + `lfd`. That is
`CCast::LtoF`, and writing it that way is what gets the size to match (404/404 and 416/416).

For the ellipse, `const int heX = ...; CCast::LtoF(heX * heX)` **CSEs to one call and gives the
wrong 372-byte body**; retail calls `GetOrbitZoneWidth` twice and does `mullw r30,r3,r31`, so the
square must be written as **two separate call expressions**:

```cpp
const int heX = GetTweakPlayer()->GetOrbitZoneWidth(zone);
const float heXSq = CCast::LtoF(heX * GetTweakPlayer()->GetOrbitZoneWidth(zone));
```

and the same for `heY`. Measured: one call each (CSE) 372 B, two calls each 416 B = retail.
For the box, named locals for `screenPosition.GetX()`/`GetY()` make it *worse* (420 B); inline
`GetX()` in the comparison is 404 B = retail.

## Files touched

- `src/MetroidPrime/Player/CPlayerOrbit.cpp` - six function bodies
- `include/MetroidPrime/Player/CPlayer.hpp` - two `EPlayerOrbitRequest` values
- `include/MetroidPrime/Player/CPlayerGun.hpp` - `GrappleArm()` accessor (2 overloads)
- `include/MetroidPrime/Player/CGrappleArm.hpp` - `GetAnimState()` accessor
- `include/MetroidPrime/ScriptObjects/CScriptGrapplePoint.hpp` - **new**, partial declaration
- `src/MetroidPrime/Player/CGrappleArmSetAnimState.cpp` - **new**, port carve-out for the arm
  anim-state reset and the four bodies only it reaches
- `files.cmake` - the one line listing it

No `asm`. `tools/check_symbol_names.py`: 0 missing. `tools/probe_sources.sh`: 749 files,
0 failures, 289 undefined (unchanged). DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## Next: still unstarted, cheapest first

`SetOrbitState` is still **98.63%** and unchanged by this run - run 2's wall on it stands
(the carcass-case `CVector3f` temp lands at `20(r1)` instead of `24(r1)`). The eleven
`Before*`-shaped stubs are untouched and, on this run's evidence, mostly Echoes-forked enough
that p1 needs adapting rather than copying: `UpdateAimTarget` reads two **unnamed** `.sdata`
bools at 0x80417DF8/0x80417DF9 (p1's `gkAutoAim` / `gkAutoAimAtOrbitedObject`) and a
`CTweakPlayerControls` accessor `fn_8021583C` at 0x8021583C that does not exist here;
`UpdateAimCandidates` needs `fn_80123334` (104 B, a `CAABox::GetTransformedAABox` wrapper, not
declared at all) plus the unnamed frame counters at 0xeac/0xeb0; `ValidateFPPosition` (560 B) is
`BuildColliderList` + `CCollidableAABox` + `DetectCollisionBoolean` and is nearly all straight-line
code, so it is probably the cheapest of the three once its types are in hand.

---

# Run 4 (this run)

`MetroidPrime/Player/CPlayerOrbit`, **one** more function to an exact match: `UpdateAimTarget`
(0x8011F2E8, 348 B) 1.15% -> **100%**. The unit went **34/60 -> 35/60 matched functions**,
fuzzy **21.80% -> 22.98%**, matched code 16.44% -> 17.64%. Judge: `goal_check: PASS`
(gate ok including DOL sha1 `6ef9b491...`, 86 RELs, port probe 751 files / **288** undefined
against a baseline of 291; matched 12205 -> 12206; linked 5860 -> 5860; no asm; decl order ok).

Files touched: `src/MetroidPrime/Player/CPlayerOrbit.cpp` (the body plus three declarations),
`include/MetroidPrime/Player/CPlayer.hpp` (one inline accessor, no layout change),
`src/MetroidPrime/PortCTweakPlayerControls.cpp` (`fn_8021583C`),
`src/MetroidPrime/PortGlobals.cpp` (two `.sdata2` flag definitions). No `asm`, no config.

## Re-measured first, and run 3's head counts still held

`build/report.json` on the clean tree of this worktree already showed 34/60 and fuzzy 21.80%,
exactly what run 3 reported, and the same 26 functions short. So the item was not stale and
run 3's work was already on this branch. `fn_80121908` / `fn_80123334` / `fn_8012339C` are still
reported unmatched: our object emits `erase__Q24rstl29reserved_vector<9TUniqueId,5>FP9TUniqueId`
(weak, 120 B, byte-identical to `fn_80121908`) under the *mangled* name, and `nm` shows no
`fn_80121908` symbol at all - the name in `build/G2ME01/asm/**.s` is the listing's address label,
not the symbol. objdiff pairs by symbol, so those three stay at 0% no matter what the bytes are.
**Do not spend a run on them.**

## `ValidateFPPosition` (560 B) reaches 100% **as a source translation** and is still blocked

Prime 1's body ported to this repo's own headers compiles to retail's 560 bytes **on the first
try** (fuzzy 21.80% -> 23.71%, 34 -> 35/60; `goal_check` got as far as the counts before
failing). The p1 shape is right and every header it needs is already here (`CAABox`,
`CCollidableAABox`, `CMaterialFilter`, `CStateManager::BuildColliderList`,
`CGameCollision::DetectCollisionBoolean`, `CPhysicsActor::GetBaseBoundingBox`). The
`margin(1.f, 1.f, 1.f)` argument order in particular has to stay p1's (`min - margin + position`
first, then `max + margin + position`), because MWCC evaluates the `CAABox` constructor's
arguments right to left - and `GetBaseBoundingBox()` must be bound to a `const CAABox&`, not a
value, or the 24-byte box is staged as floats instead of copied as words.

**It cannot be committed, and the blocker is the port link, not the unit.** Retail's last call is
`CGameCollision::DetectCollisionBoolean`, and `src/MetroidPrime/CGameCollision.cpp` is a whole
`NonMatching` unit that `files.cmake` does not list, so the call is a new MISSING symbol:

```
GATE FAIL: link-gap
  gap grew: _ZN14CGameCollision22DetectCollisionBooleanERK13CStateManagerRK19CCollisionPrimitive
            RK12CTransform4fRK15CMaterialFilterRKN4rstl15reserved_vectorI9TUniqueIdLi1024EEE
            is not in port_link_gap_list.md
```

`docs/research/port_link_gap_list.md` is judge-owned, so the only way through is to define the
symbol for the host. I measured the cascade and it does not close: `DetectCollisionBoolean`
needs `DetectStaticCollisionBoolean` and `DetectDynamicCollisionBoolean`, which need
`CMetroidAreaCollider::AABoxCollisionCheckBoolean` / `SphereCollisionCheckBoolean`,
`fn_802896F0`, `skStaticGeometryMaterials`, `CCollisionPrimitive::CollideBoolean`,
`CPhysicsActor::GetPrimitiveTransform` / `GetCollisionPrimitive`, `CWorld::GetChainHead`,
`CGameArea::GetPostConstructed` and `CMaterialFilter::GetPassEverything`. **None of those
fourteen names is in `port_link_gap_list.md` or `port_link_baseline.txt`**, so every one of them
would have to be defined too - that is the static-collision path, i.e. a separate item. Same
trap as run 2's and run 3's, one level deeper. Reverted; the source shape above is the whole
answer for whoever picks it up with the collision path in hand.

NEW: progress-unit-cgamecollision-boolean | progress | MetroidPrime/CGameCollision | the port's
MISSING set has no entry for any of the 14 names `CGameCollision::DetectCollisionBoolean` needs,
so it cannot be defined without the static-collision path; that path is the unit's whole body.

## `UpdateAimTarget`: what mattered was two `TUniqueId`/`switch` shapes, not the logic

Prime 1's body is much bigger than Echoes'; only its first two blocks survive. Retail (348 B):

```cpp
UpdateAimCandidates(mgr);
if (!GetCombatMode()) { SetAimTarget(kInvalidUniqueId); mAimTargetTimer = 0.f; return; }
if (!lbl_8041A438 && lbl_8041A439) {                 // two one-byte .sdata2 globals, 0 and 1
  if (mOrbitState == kOS_OrbitObject || mOrbitState == kOS_ForcedOrbitObject) {
    if (ValidateOrbitTargetId(GetOrbitTargetId(), mgr) == 0) { SetAimTarget(GetOrbitTargetId()); }
  }
  return;
}
TCastToPtr<CActor>(mgr.GetObjectById(<aim target>));   // dead, result never read
if (!fn_8021583C(GetTweakPlayerControls())) { return; }
switch (mOrbitState) {
case kOS_ForcedOrbitObject:
case kOS_OrbitObject:
  if (ValidateOrbitTargetId(GetOrbitTargetId(), mgr) == 0) { SetAimTarget(GetOrbitTargetId()); }
  break;
default: break;
}
```

**The two orbit-state tests are spelled differently on purpose, and both spellings are forced:**

| block | retail tree | source that produces it | score with the other spelling |
| --- | --- | --- | --- |
| globals branch (0x8011F350) | `cmpwi 1 / beq / cmpwi 4 / bne` (5 insns) | `A \|\| B` | - |
| tweak branch (0x8011F3D4) | `cmpwi 4 / beq / **bge** / cmpwi 1 / beq / b` (6 insns) | two-case `switch` | `A \|\| B` = 96.22% |

The `bge` is the range check a switch lowering emits after its first case; `||` never produces
it. This is run 2's `ActivateOrbitSource` lesson in the other direction: the `||` form is the
*naive* one and the `switch` form is what retail compiled, and which one a given function used
has to be read off its bytes, not guessed from p1.

**The dead `TCastToPtr` needs a getter, not the member.** Retail is
`lhz r0,1548(r30) / mr r3,r31 / addi r4,r1,28 / sth r0,24(r1) / **sth r0,28(r1)** / bl
GetObjectById` - the value is staged in a temp and then copied to the argument slot, two `sth`.
Passing `mAimTarget` directly gives one `sth`, and the missing temp shifts **every later
argument four bytes down the frame** (`addi r4,r1,44` where retail has `addi r4,r1,48`, and so
on down the function). Adding `TUniqueId GetAimTarget() const { return mAimTarget; }` to
`CPlayer.hpp` and calling that fixes it: the inlined call's result is a temp, so the compiler
stages it. This is run 2's "a `TUniqueId` local is worth four instructions of stack traffic",
one level up - an inlined *accessor* is the local. No layout change, and the judge's
per-function diff says nothing else in the tree moved.

`mgr.GetObjectById` is declared `const CEntity* ... const` in this repo but retail passes its
result to `TCastToPtr<CActor>(CEntity*)`, so the call needs `const_cast< CEntity* >` - a no-op at
the ABI level, and it emits no instruction.

**How to see the difference without objdiff's percentage.** `build/G2ME01/asm/**/*.s` is *stale*
in this worktree (no ninja target regenerates it) and there are two object paths,
`build/G2ME01/src/<unit>.o` (the one `fast_try.sh` and `decomp_build.sh` build) and
`build/G2ME01/obj/<unit>.o` (older). Reading the wrong one gives a wrong answer. Use
`build/binutils/powerpc-eabi-objdump -d build/G2ME01/src/MetroidPrime/Player/CPlayerOrbit.o`,
find the function by its symbol line, and compare bytes against `tools/dis.sh 0x8011F2E8 0x15C`.
The 15 zero-displacement instructions are the unresolved `bl` / `lhz@kInvalidUniqueId` /
`lbz@lbl_8041A438` relocations and are expected; everything else must be byte-identical, and
*the instruction count* is the giveaway (87 expected, 84 is what the `||`+`mAimTarget` spelling
emits).

## The two `.sdata2` flags are two globals, and they must not be `const`

`python3 tools/sda.py s2:-32648 s2:-32647` -> 0x8041A438 / 0x8041A439. `objdump -s` on `.sdata2`
gives `00 01 00 00` at 0x8041A438, and `nm build/G2ME01/main.elf` calls both `D` (writable), so
they are two separate one-byte objects, not halves of one - p1's `gkAutoAim` /
`gkAutoAimAtOrbitedObject` are the same two flags. Declared **without** `const` (the
`progress-prime1-cbeamprojectile` rule: a `const` extern for a `D` symbol is a licence for MWCC
to hoist and re-order loads), defined in `src/MetroidPrime/PortGlobals.cpp` with values `false`
/ `true`, exactly where `lbl_804183DD` and friends already live.

## `fn_8021583C` had a home already

`lwz r3,0(r3)` / `lbz r3,325(r3)` / `blr` - the same shape as `fn_80215860`, which
`CMorphBall::IsMovementAllowed` already calls and which
`src/MetroidPrime/PortCTweakPlayerControls.cpp` already defines for the host. Added there, next
to it. **325 = 0x145 and `SLdrTweakPlayerControls::booleans` is 21 `bool`s at 0x130, so 0x145 is
the byte *past* the last one**; the generated loader header has no name for it, so the host body
reads it at the measured offset rather than through a field that does not exist. The value is
whatever the tweak script put there, which is what retail reads.

**Net effect on the port's undefined count: 289 -> 288.** `fn_8021583C` is a new reference but
the port now defines it, so it is a net -1 and the link-gap gate stays clean.

## Next: still unstarted, cheapest first

Ordered by measured size; **all are still 0-1% stubs**, so nothing here is a near-miss:
`fn_80123334` (104 B) / `fn_8012339C` (372 B) / `fn_80121908` (120 B) are *unmatchable* (see
above); `fn_8011c3c0` (1608 B), `UpdateAimCandidates` (432 B), `UpdateOrbitTarget` (1080 B),
`ValidateObjectForMode` (696 B), `UpdateOrbitableObjects` (728 B), `CheckEnemyAgainstOrbitZone`
(1164 B), `UpdateGrappleArmTransform` (824 B), `FindOrbitTargetId` (896 B),
`ValidateAimTargetId` (912 B), `FindAimTargetId` (1016 B), `ValidateOrbitTargetId` (1008 B),
`ValidateCurrentOrbitTargetId` (1124 B), `FindOrbitableObjects` (752 B),
`FindBestOrbitableObject` (1788 B), `UpdateOrbitInput` (1928 B), `UpdateGrappleState` (2008 B),
`ApplyGrappleForces` (3304 B).

**`UpdateAimCandidates` (432 B) is the cheapest real one, and its pieces are mostly *named*
already - run 3's note called 0xeac/0xeb0 "unnamed frame counters", which is wrong:**
`CPlayer+0xEAC` is `mAimCandidateIndex` and `0xEB0` is `mAimCandidateRefreshFrames` (the
constructor initialises the latter to 20, which is retail's `li r0,20`), because
`mAimCandidates` is a `rstl::reserved_vector<TUniqueId, 1024>` = 4 + 2048 bytes at 0x6A8. The
`lbz r0,0x1269(r31) / rlwinm. r0,r0,29,31,31` is bit 0 of the byte at 0x1269, which is the
existing `bool x1269_24_ : 1`, used as `if (x1269_24_) distance *= 0.5f`. Measured from retail:

- it calls `fn_80123334(r3 = &ret, r4 = 1 /*cropBottom*/, r5 = cameraXf, f1 = GetAimBoxWidth(),
  f2 = GetAimBoxHeight(), f3 = distance)`, which is p1's `static CAABox BuildNearListBox(bool,
  const CTransform4f&, float x, float z, float y)` **out of line** (`CAABox(-x, cropBottom ? 0 :
  -y, -z, x, y, z).GetTransformedAABox(xf)`), so p1's line 893-894 gives the argument order
  verbatim. Define it in this file as `extern "C"` so the symbol is `fn_80123334`.
- the material filter's include list is read as a **global**, `lwz r5,-31376(r13)` = 0x804182F0
  = 59 (`objdump -s` on `.sdata`), i.e. `CMaterialList(lbl_804182F0)` with
  `extern "C" EMaterialTypes lbl_804182F0;` - a real symbol in
  `build/G2ME01/obj/auto_09_804182C0_sdata.o`, so the decomp link binds it, and `PortGlobals.cpp`
  needs the definition. **Not `kMT_Target` (40)** as p1 has.
- `FindAimTargetId` is entered with `r3 = &slot, r4 = this, r5 = mgr`, i.e. it returns through a
  hidden pointer, and there are unexplained `stw 1,40(r1)` / `stw 0,48(r1)` before the
  `BuildNearList` call. Both still unexplained; that 12-instruction block may be one struct.
- **the call site copies the returned `CAABox` from the return slot at 48(r1) to 96(r1)**, so try
  the bare expression *and* a named `const CAABox` local - p1's named local would not copy.
- the `CMaterialFilter` is at 72(r1) (20 bytes: include 72, exclude 80, type 88 = 1) and the
  `CAABox` at 96(r1), the return slot of `fn_80123334` at 48(r1), and the candidate list is
  `CPlayer+0x6A8` itself with its `mCount` cleared to 0 in the same window.

## Review rejected run 26 (2026-10-01 23:21:13Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/PortGlobals.cpp:558` sets `lbl_8041A439 = true` where retail holds 0 — `.sdata2` at 0x8041A438 is `00 00 01 00` and the only two references to it in `main.elf` are loads with no stores, so both flags are constants zero (Prime 1's donor likewise sets `gkAutoAim`/`gkAutoAimAtOrbitedObject` to `false`); the note's "`00 01 00 00` at 0x8041A438" is a one-byte mis-read. That makes `!lbl_8041A438 && lbl_8041A439` true on the host and false on retail, turning a branch retail never executes into the live path, so the port permanently skips the `fn_8021583C` gate and re-derives the aim target from the orbit target each combat-mode frame — a wrong port value, and a violation of PortGlobals.cpp's stated contract of holding the retail value. The `UpdateAimTarget` translation itself is correct and100% byte-matched and can stay; an acceptable change keeps this diff and makes it `extern "C" bool lbl_8041A439 = false;` (measured from `build/G2ME01/main.dol`, correcting the note's figure).

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-cplayerorbit-L4-26.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-cplayerorbit-L4-26-review1-20261001T231816.jsonl

## Fix round 1

One value was wrong, in `src/MetroidPrime/PortGlobals.cpp`: `lbl_8041A439` was initialised to
`true` where retail holds 0. Measured from `build/G2ME01/main.dol` (the `.sdata2` payload located
by matching a 64-byte run from the middle of the section out of `main.elf`'s copy): the bytes at
0x8041A430 are `00 00 00 00 3f 80 00 00 00 00 01 00 00 01 00 00`, so 0x8041A438 = `00` and
0x8041A439 = `00`. Run 4's note said `00 01 00 00` - a one-byte mis-read. Cross-checked: a full
`objdump -d build/G2ME01/main.elf` has exactly two references to those addresses, `lbz
r0,-32648(r2)` at 0x8011f338 and `lbz r0,-32647(r2)` at 0x8011f344, both loads, and **no**
instruction stores to either, so both flags are constant zero at runtime (p1's donor likewise
sets `gkAutoAim` / `gkAutoAimAtOrbitedObject` to `false`).

Changed:

- `src/MetroidPrime/PortGlobals.cpp` - `extern "C" bool lbl_8041A439 = true;` -> `= false;`, and
  the comment's byte figure corrected to `00 00 01 00` with the no-store/no-other-reference
  evidence, which is what makes the block above 0x8011F3A4 dead on retail.
- `src/MetroidPrime/Player/CPlayerOrbit.cpp` - the extern block's comment said the two globals
  hold "0 and 1"; now "both holding 0". The `extern "C"` declarations and the
  `!lbl_8041A438 && lbl_8041A439` test in `UpdateAimTarget` are unchanged and still 100%
  byte-matched - they describe retail's tree, and retail's tree is what the DOL contains.
- `docs/goal-notes/progress-unit-cplayerorbit.md` - the same figure corrected in the "two
  `.sdata2` flags" section, flagging the mis-read.

`UpdateAimTarget`'s translation, `fn_8021583C`, `GetAimTarget()`, the `switch` vs `||` spellings
and the `CheckEnemyAgainstOrbitZone`/`ValidateFPPosition` findings are all untouched - the
reviewer judged them and they do not depend on this value. On the host the port now takes the
`fn_8021583C` path in combat mode, as retail does.
