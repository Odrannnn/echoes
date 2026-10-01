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
